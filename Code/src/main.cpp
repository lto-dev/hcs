#include <Arduino.h>
#include <WiFiManager.h>
#include <SPIFFS.h>
#include <time.h>
#include <Preferences.h>

#include "RelayController.h"
#include "SensorReader.h"
#include "HX710B.h"
#include "HydroAuth.h"
#include "Config.h"
#include "GrowthManager.h"
#include "MQTTManager.h"
#include "WebServerManager.h"
// todo: remove light switch, now controlled by timer and growth profile
// todo: add ph/up, down pump control and logic
// todo: add food pump control and logic
// todo: better integration with home assistant
// todo: add pins in configuration so i can switch boards
// todo: add time zone support
 
// Hardware initialization
HX710B hx710b(GPIO_NUM_26, GPIO_NUM_27); // HX710B pins
PHMeter ph(GPIO_NUM_32);
GravityTDS tds;
OneWire oneWire(GPIO_NUM_22); // Temperature sensor pin
DallasTemperature temp(&oneWire);
RelayController relayController;

// Create our sensor reader
SensorReader sensorReader(hx710b, ph, tds, temp);

// Shared resources
Preferences preferences;
WiFiManager wifiManager;
WiFiClient espClient;

// Forward declaration of globals that will be initialized in setup()
SystemConfig systemConfig;
ConfigManager* configManager = nullptr;
GrowthManager* growthManager = nullptr;
MQTTManager* mqttManager = nullptr;
WebServerManager* webServerManager = nullptr;

// Alert Thresholds
const float PH_MIN = 5.5;
const float PH_MAX = 6.5;
const int LIQUID_ALERT_PERCENT = 20; // Alert when below 20%

// Global variables for tracking timers (exposed for WebServerManager)
time_t lastWateringTime = 0;
time_t pumpOnTime = 0;

// Function prototypes
void initMQTT();
void setupTimeSync();
void checkAlerts(int levelPercent, float phValue);
void updateRelaysBasedOnCycle();

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Hydroponics System");

  pinMode(GPIO_NUM_25, OUTPUT);
  digitalWrite(GPIO_NUM_25, LOW); // Set GPIO 25 to LOW (off)

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  // Debug SPIFFS contents
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }

  // Initialize sensors and relays
  relayController.begin();
  sensorReader.begin();

  // Initialize configuration
  configManager = new ConfigManager(preferences, sensorReader);
  configManager->begin();
  systemConfig = configManager->getConfig();

  // Initialize WiFi
  wifiManager.setConfigPortalTimeout(180);
  if (!wifiManager.autoConnect("HydroponicsAP")) {
    Serial.println("Failed to connect, restarting");
    ESP.restart();
  }
  
  // Setup time synchronization
  setupTimeSync();

  // Initialize growth profile manager
  growthManager = new GrowthManager(preferences);
  growthManager->begin();

  // Initialize MQTT manager
  mqttManager = new MQTTManager(espClient, systemConfig);
  mqttManager->begin();
  mqttManager->setCallback([](const String& topic, const String& payload) {
    Serial.printf("MQTT Message: Topic: %s, Payload: %s\n", topic.c_str(), payload.c_str());
    
    if (topic == mqttManager->getTopicPump()) {
      bool newState = payload.equalsIgnoreCase("ON");
      relayController.setState(RELAY_PUMP, newState);
      Serial.printf("Setting pump state to: %s\n", newState ? "ON" : "OFF");
    } 
    else if (topic == mqttManager->getTopicLights()) {
      bool newState = payload.equalsIgnoreCase("ON");
      relayController.setState(RELAY_LIGHTS, newState);
      Serial.printf("Setting lights state to: %s\n", newState ? "ON" : "OFF");
    }
  });

  // Initialize web server
  webServerManager = new WebServerManager(80, systemConfig, *growthManager, 
                                         sensorReader, relayController, preferences, configManager, mqttManager);
  webServerManager->begin();

  Serial.println("Hydroponics System Initialized");
}

void loop() {
  wifiManager.process();

  // Update sensor readings
  sensorReader.updateReadings();

  // Get current values
  float liquidValue = sensorReader.getLiquidValue();
  float liquidLevel = sensorReader.getLiquidLevel();
  float phValue = sensorReader.getPH();
  float tdsValue = sensorReader.getTDS();
  float tempValue = sensorReader.getTemperature();

  // Get liquid level percentage
  int levelPercent = 0;
  if (!isnan(liquidLevel)) {
    levelPercent = (int)liquidLevel;
    Serial.printf("Liquid Level: %.2f (%d%%), Raw Value: %.2f\n", liquidLevel, levelPercent, liquidValue);
  }

  // Check alerts
  checkAlerts(levelPercent, phValue);

  // Update relay status based on active growth cycle (if any)
  updateRelaysBasedOnCycle();
  
  // MQTT handling - only if enabled
  if (systemConfig.mqtt_enabled) {
    if (!mqttManager->connected()) {
      if (mqttManager->connect()) {
        Serial.println("MQTT Connected");
      } else {
        Serial.println("MQTT Connection failed");
        delay(5000);
        return;
      }
    }
    
    mqttManager->loop();

        // Log values
        if (!isnan(liquidLevel)) {
          Serial.printf("Liquid Level: %.2f (%d%%), Raw Value: %.2f\n", liquidLevel, levelPercent, liquidValue);
        }
        if (!isnan(phValue)) {
          Serial.printf("pH Value: %.2f\n", phValue);
        }
        if (!isnan(tdsValue)) {
          Serial.printf("TDS Value: %.2f ppm\n", tdsValue);
        }
        if (!isnan(tempValue)) {
          Serial.printf("Temp Value: %.2f C\n", tempValue);
        }

    // Publish sensor data if connected to MQTT
    if (mqttManager->connected()) {
      if (!isnan(liquidLevel)) {
        mqttManager->publishLiquidLevel(levelPercent);
      }

      if (!isnan(phValue)) {
        mqttManager->publishPH(phValue);
      }

      if (!isnan(tdsValue)) {
        mqttManager->publishTDS(tdsValue);
      }

      if (!isnan(tempValue)) {
        mqttManager->publishTemperature(tempValue);
      }
    }
  }

  // Add small delay between readings
  delay(1000);
}

// Setup time synchronization with NTP server
void setupTimeSync() {
  Serial.println("Setting up time synchronization...");
  
  // Configure time servers and timezone
  configTime(0, 0, systemConfig.ntp_server); // UTC time, no daylight saving offset
  
  // Wait for time to be set (timeout after 10 seconds)
  time_t now = time(nullptr);
  int timeout = 10;
  while (now < 1000000000 && timeout > 0) {
    Serial.println("Waiting for NTP time sync...");
    delay(1000);
    now = time(nullptr);
    timeout--;
  }
  
  if (now < 1000000000) {
    Serial.println("Failed to get time from NTP server!");
  } else {
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.print(timeinfo.tm_year + 1900);
    Serial.print("-");
    Serial.print(timeinfo.tm_mon + 1);
    Serial.print("-");
    Serial.print(timeinfo.tm_mday);
    Serial.print(" ");
    Serial.print(timeinfo.tm_hour);
    Serial.print(":");
    Serial.print(timeinfo.tm_min);
    Serial.print(":");
    Serial.println(timeinfo.tm_sec);
  }
}

// Check sensor values against thresholds
void checkAlerts(int levelPercent, float phValue) {
  String alertMsg = "";

  if (levelPercent < LIQUID_ALERT_PERCENT) {
    alertMsg += "Low water level! ";
  }
  if (phValue < PH_MIN) {
    alertMsg += "pH too low! ";
  }
  if (phValue > PH_MAX) {
    alertMsg += "pH too high! ";
  }

  if (alertMsg.length() > 0 && mqttManager->connected()) {
    mqttManager->publishAlert(alertMsg);
  }
}

// Update relay status based on active growth cycle
void updateRelaysBasedOnCycle() {
  // Debug log for tracking function calls
  static unsigned long lastExecutionTime = 0;
  static bool firstRun = true;
  
  unsigned long currentMillis = millis();
  Serial.printf("[DEBUG] updateRelaysBasedOnCycle called. Time since last call: %lu ms\n", 
                lastExecutionTime == 0 ? 0 : currentMillis - lastExecutionTime);
  lastExecutionTime = currentMillis;
  
  const GrowthCycle& activeCycle = growthManager->getActiveCycle();
  if (!activeCycle.active) {
    Serial.println("[DEBUG] No active growth cycle");
    return;
  }
  
  // Get current time
  time_t now = time(nullptr);
  if (now < 1000000000) { // Basic sanity check for valid time (year ~2001+)
    Serial.println("[ERROR] System time not yet synchronized");
    return;
  }
  
  // Get current stage settings
  GrowthStage* currentStage = growthManager->getCurrentStageSettings();
  if (!currentStage) {
    Serial.println("[ERROR] Current stage settings unavailable");
    return;
  }
  
  // Log current stage and settings
  String currentStageName = growthManager->getCurrentGrowthStage(now);
  Serial.printf("[INFO] Current stage: %s, Water interval: %d min, Water duration: %d min, Light hours: %d\n", 
                currentStageName.c_str(), currentStage->waterInterval, 
                currentStage->waterDuration, currentStage->lightHours);
  
  // Control lights based on time of day and light hours
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  
  // Light control: use the light start hour from the profile
  int lightStartHour = currentStage->lightStartHour;
  int lightEndHour = (lightStartHour + currentStage->lightHours) % 24;
  
  // Determine if lights should be on based on the fixed start time
  bool shouldLightsBeOn;
  if (lightStartHour < lightEndHour) {
    // Normal case (e.g., 6AM to 6PM)
    shouldLightsBeOn = (currentHour >= lightStartHour && currentHour < lightEndHour);
  } else {
    // Wrap-around case (e.g., 6PM to 6AM)
    shouldLightsBeOn = (currentHour >= lightStartHour || currentHour < lightEndHour);
  }
  
  bool currentLightState = relayController.getState(RELAY_LIGHTS);
  
  // Calculate time until next light transition
  int minutesToLightTransition;
  if (shouldLightsBeOn) {
    // Lights should be on, calculate time until they turn off
    if (currentHour < lightEndHour) {
      minutesToLightTransition = (lightEndHour - currentHour) * 60 - currentMinute;
    } else {
      minutesToLightTransition = ((lightEndHour + 24) - currentHour) * 60 - currentMinute;
    }
  } else {
    // Lights should be off, calculate time until they turn on
    if (currentHour < lightStartHour) {
      minutesToLightTransition = (lightStartHour - currentHour) * 60 - currentMinute;
    } else {
      minutesToLightTransition = ((lightStartHour + 24) - currentHour) * 60 - currentMinute;
    }
  }
  
  Serial.printf("[INFO] Light schedule: Current time: %d:%02d, Lights: %s (should be %s), Hours: %d-%d, Minutes until transition: %d\n", 
                currentHour, currentMinute,
                currentLightState ? "ON" : "OFF", 
                shouldLightsBeOn ? "ON" : "OFF",
                lightStartHour, lightEndHour,
                minutesToLightTransition);
  
  if (currentLightState != shouldLightsBeOn) {
    Serial.printf("[ACTION] Setting lights to: %s\n", shouldLightsBeOn ? "ON" : "OFF");
    relayController.setState(RELAY_LIGHTS, shouldLightsBeOn);
    if (mqttManager->connected()) {
      mqttManager->publishLightsState(shouldLightsBeOn);
    }
  }
  
  // Water pump control based on interval
  unsigned long wateringIntervalSeconds = currentStage->waterInterval * 60; // Convert minutes to seconds
  
  // On first run, start a watering cycle immediately
  if (firstRun) {
    Serial.println("[INFO] First run detected - starting initial watering cycle");
    relayController.setState(RELAY_PUMP, true);
    lastWateringTime = now;
    firstRun = false;
    
    if (mqttManager->connected()) {
      mqttManager->publishPumpState(true);
    }
  } else {
    // Log watering information
    time_t secondsSinceLastWatering = (lastWateringTime > 0) ? now - lastWateringTime : 0;
    time_t secondsUntilNextWatering = (lastWateringTime > 0) ? 
                                     wateringIntervalSeconds - secondsSinceLastWatering : 0;
    
    if (secondsUntilNextWatering < 0) secondsUntilNextWatering = 0;
    
    Serial.printf("[INFO] Watering schedule: Interval: %lu s, Last watering: %ld s ago, Next watering in: %ld s\n", 
                  wateringIntervalSeconds, 
                  secondsSinceLastWatering,
                  secondsUntilNextWatering);
    
    // Check if it's time for another watering cycle
    if (lastWateringTime > 0 && now - lastWateringTime >= wateringIntervalSeconds) {
      Serial.printf("[ACTION] Starting watering cycle. Current time: %ld, Last watering time: %ld, Difference: %ld s\n", 
                   now, lastWateringTime, now - lastWateringTime);
      
      relayController.setState(RELAY_PUMP, true);
      lastWateringTime = now;
      
      if (mqttManager->connected()) {
        mqttManager->publishPumpState(true);
      }
    }
  }
  
  // Turn off pump after watering duration
  bool pumpCurrentState = relayController.getState(RELAY_PUMP);
  
  if (pumpCurrentState) {
    if (pumpOnTime == 0) {
      Serial.println("[INFO] Pump turned on, starting duration timer");
      pumpOnTime = now;
    } else {
      unsigned long wateringDurationSeconds = currentStage->waterDuration * 60; // Convert minutes to seconds
      time_t pumpRunTime = now - pumpOnTime;
      time_t timeRemaining = wateringDurationSeconds - pumpRunTime;
      
      if (timeRemaining < 0) timeRemaining = 0;
      
      Serial.printf("[INFO] Pump running for %ld s, will turn off in %ld s\n", 
                    pumpRunTime, timeRemaining);
      
      if (pumpRunTime >= wateringDurationSeconds) {
        Serial.println("[ACTION] Stopping watering cycle - duration completed");
        relayController.setState(RELAY_PUMP, false);
        pumpOnTime = 0;
        
        if (mqttManager->connected()) {
          mqttManager->publishPumpState(false);
        }
      }
    }
  } else {
    if (pumpOnTime != 0) {
      Serial.println("[INFO] Pump turned off, resetting duration timer");
      pumpOnTime = 0;
    }
  }
  
  // pH alerts based on current stage's optimal range
  float phValue = sensorReader.getPH();
  if (!isnan(phValue)) {
    Serial.printf("[INFO] Current pH: %.2f, Target range: %.1f-%.1f\n", 
                  phValue, currentStage->phMin, currentStage->phMax);
                  
    bool phOutOfRange = (phValue < currentStage->phMin || phValue > currentStage->phMax);
    if (phOutOfRange && mqttManager->connected()) {
      String alertMsg = "pH ";
      alertMsg += (phValue < currentStage->phMin) ? "too low" : "too high";
      alertMsg += " for " + currentStageName + " stage!";
      mqttManager->publishAlert(alertMsg);
    }
  }
}
