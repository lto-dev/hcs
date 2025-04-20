#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include "HydroAuth.h"
#include "Config.h"
#include "GrowthManager.h"
#include "SensorReader.h"
#include "RelayController.h"
#include "MQTTManager.h"

// User structure for authentication
struct User {
    char username[32] = "admin";
    char password[32] = "admin";
};

class WebServerManager {
private:
    AsyncWebServer _server;
    HydroAuth _auth;
    SystemConfig& _config;
    GrowthManager& _growthManager;
    SensorReader& _sensorReader;
    RelayController& _relayController;
    Preferences& _preferences;
    MQTTManager* _mqttManager;
    ConfigManager* _configManager;
    
    User _webUser;
    
public:
    WebServerManager(uint16_t port, SystemConfig& config, GrowthManager& growthManager, 
                    SensorReader& sensorReader, RelayController& relayController,
                    Preferences& preferences, ConfigManager* configManager, 
                    MQTTManager* mqttManager = nullptr)
        : _server(port),
          _config(config),
          _growthManager(growthManager),
          _sensorReader(sensorReader),
          _relayController(relayController),
          _preferences(preferences),
          _configManager(configManager),
          _mqttManager(mqttManager) {
        
        // Default credentials
        strlcpy(_webUser.username, "admin", sizeof(_webUser.username));
        strlcpy(_webUser.password, "admin", sizeof(_webUser.password));
        
        setupAuth();
    }
    
    void begin() {
        setupEndpoints();
        _server.begin();
    }
    
private:
    void setupAuth() {
        _auth.setUsername(_webUser.username);
        _auth.setPassword(_webUser.password);
        _auth.setRealm("Hydroponics Control");
        _auth.setAuthFailureMessage("Authentication failed");
    }
    
    void setupEndpoints() {
        // Serve HTML interface
        _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
            if (!_auth.authenticate(request)) {
                return;
            }
            request->send(SPIFFS, "/index.html", "text/html");
        });

        // Serve static files
        _server.serveStatic("/", SPIFFS, "/").setFilter([this](AsyncWebServerRequest *request) {
            return _auth.authenticate(request);
        });

        // Configuration endpoints
        _server.on("/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("GET /config - Entering");
            String json;
            StaticJsonDocument<512> doc;
            doc["device_id"] = _config.device_id;
            doc["mqtt_enabled"] = _config.mqtt_enabled;
            doc["mqtt_server"] = _config.mqtt_server;
            doc["mqtt_port"] = _config.mqtt_port;
            doc["mqtt_user"] = _config.mqtt_user;
            doc["mqtt_password"] = _config.mqtt_password;
            doc["ntp_server"] = _config.ntp_server;
            serializeJson(doc, json);
            request->send(200, "application/json", json);
        });

        AsyncCallbackJsonWebHandler *configHandler = new AsyncCallbackJsonWebHandler("/config", [this](AsyncWebServerRequest *request, JsonVariant &json) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("POST /config - Entering");
            JsonObject jsonObj = json.as<JsonObject>();
            Serial.println("Received JSON:");
            serializeJson(jsonObj, Serial);
            Serial.println();

            bool prevMqttEnabled = _config.mqtt_enabled;
            
            if (jsonObj.containsKey("device_id")) strlcpy(_config.device_id, jsonObj["device_id"], sizeof(_config.device_id));
            if (jsonObj.containsKey("mqtt_enabled")) _config.mqtt_enabled = jsonObj["mqtt_enabled"].as<bool>();
            if (jsonObj.containsKey("mqtt_server")) strlcpy(_config.mqtt_server, jsonObj["mqtt_server"], sizeof(_config.mqtt_server));
            if (jsonObj.containsKey("mqtt_port")) _config.mqtt_port = jsonObj["mqtt_port"];
            if (jsonObj.containsKey("mqtt_user")) strlcpy(_config.mqtt_user, jsonObj["mqtt_user"], sizeof(_config.mqtt_user));
            if (jsonObj.containsKey("mqtt_password")) strlcpy(_config.mqtt_password, jsonObj["mqtt_password"], sizeof(_config.mqtt_password));
            if (jsonObj.containsKey("ntp_server")) strlcpy(_config.ntp_server, jsonObj["ntp_server"], sizeof(_config.ntp_server));
            
            // Update the ConfigManager's internal config with our modified values
            _configManager->getConfig() = _config;
            
            // Now save using the global ConfigManager instance
            _configManager->saveConfig();
            
            // Handle MQTT connection state based on enabled setting
            if (_mqttManager) {
                if (prevMqttEnabled && !_config.mqtt_enabled) {
                    // MQTT was enabled but now disabled - disconnect
                    Serial.println("MQTT disabled, disconnecting...");
                    _mqttManager->disconnect();
                }
            }

            AsyncJsonResponse *response = new AsyncJsonResponse();
            JsonObject root = response->getRoot();
            root["status"] = "ok";
            response->setLength();
            request->send(response);
        });
        _server.addHandler(configHandler);

        // Calibration endpoints
        _server.on("/calibration", HTTP_GET, [this](AsyncWebServerRequest *request) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("GET /calibration - Entering");
            String json;
            StaticJsonDocument<512> doc;
            
            // float liquidValue = _sensorReader.getLiquidValue();
            // float liquidLevel = _sensorReader.getLiquidLevel();
            // float phValue = _sensorReader.getPH();
            
            // Add liquid level data
            // if (!isnan(liquidValue)) {
            //     doc["liquid_value"] = liquidValue;
            //     doc["liquid_level"] = liquidLevel;
            // } else {
            //     doc["liquid_value"] = "N/A";
            //     doc["liquid_level"] = "N/A";
            //     doc["liquid_error"] = "Sensor read failed";
            // }
            
            doc["cal_dry"] = _config.cal_dry;
            doc["cal_critical"] = _config.cal_critical;
            doc["cal_half"] = _config.cal_half;
            doc["cal_full"] = _config.cal_full;
            
            // Add pH calibration data
            // if (!isnan(phValue)) {
            //     doc["ph_value"] = phValue;
            //     doc["ph_adc"] = _sensorReader.getCurrentPHADC(); // Get current ADC reading from pH pin
            // } else {
            //     doc["ph_value"] = "N/A";
            //     doc["ph_adc"] = "N/A";
            //     doc["ph_error"] = "Sensor read failed";
            // }
            
            doc["ph4_adc"] = _config.ph4_adc;
            doc["ph7_adc"] = _config.ph7_adc;
            doc["ph10_adc"] = _config.ph10_adc;
            
            serializeJson(doc, json);
            request->send(200, "application/json", json);
        });

        AsyncCallbackJsonWebHandler *calibrationHandler = new AsyncCallbackJsonWebHandler("/calibration", [this](AsyncWebServerRequest *request, JsonVariant &json) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("POST /calibration - Entering");
            JsonObject jsonObj = json.as<JsonObject>();
            Serial.println("Received JSON:");
            serializeJson(jsonObj, Serial);
            Serial.println();

            // Handle liquid level calibration
            if (jsonObj.containsKey("cal_dry")) _config.cal_dry = jsonObj["cal_dry"];
            if (jsonObj.containsKey("cal_critical")) _config.cal_critical = jsonObj["cal_critical"];
            if (jsonObj.containsKey("cal_half")) _config.cal_half = jsonObj["cal_half"];
            if (jsonObj.containsKey("cal_full")) _config.cal_full = jsonObj["cal_full"];
            
            // Handle pH calibration
            if (jsonObj.containsKey("ph4_adc")) _config.ph4_adc = jsonObj["ph4_adc"];
            if (jsonObj.containsKey("ph7_adc")) _config.ph7_adc = jsonObj["ph7_adc"];
            if (jsonObj.containsKey("ph10_adc")) _config.ph10_adc = jsonObj["ph10_adc"];
            
            // Update the ConfigManager's internal config with our modified values
            _configManager->getConfig() = _config;
            
            // Now save using the global ConfigManager instance
            _configManager->saveConfig();

            AsyncJsonResponse *response = new AsyncJsonResponse();
            JsonObject root = response->getRoot();
            root["status"] = "ok";
            response->setLength();
            request->send(response);
        });
        _server.addHandler(calibrationHandler);

        // User management endpoint
        _server.on("/user", HTTP_POST, [this](AsyncWebServerRequest *request) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("POST /user - Entering");
            if (request->hasParam("username", true) && request->hasParam("password", true)) {
                strlcpy(_webUser.username, request->getParam("username", true)->value().c_str(), sizeof(_webUser.username));
                strlcpy(_webUser.password, request->getParam("password", true)->value().c_str(), sizeof(_webUser.password));
                
                // Update auth middleware with new credentials
                setupAuth();
                
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400);
            }
        });

        // Status data endpoint (replaces sensors endpoint)
        _server.on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("GET /status - Entering");
            String json;
            StaticJsonDocument<512> doc; // Increased size for additional timing data
            
            // Get current values
            float liquidValue = _sensorReader.getLiquidValue();
            float liquidLevel = _sensorReader.getLiquidLevel();
            float phValue = _sensorReader.getPH();
            float tdsValue = _sensorReader.getTDS();
            float tempValue = _sensorReader.getTemperature();
            uint16_t phADC = _sensorReader.getCurrentPHADC();

            // Get liquid level percentage
            int levelPercent = 0;
            if (!isnan(liquidLevel)) {
                levelPercent = (int)liquidLevel;
            }

            doc["liquid_level"] = isnan(liquidLevel) ? "N/A" : String(levelPercent);
            doc["liquid_value"] = isnan(liquidValue) ? "N/A" : String(liquidValue);
            doc["ph_value"] = isnan(phValue) ? "N/A" : String(phValue);
            doc["ph_adc"] = String(phADC);
            doc["tds_value"] = isnan(tdsValue) ? "N/A" : String(tdsValue);
            doc["temperature_value"] = isnan(tempValue) ? "N/A" : String(tempValue);
            doc["pump_state"] = _relayController.getState(RELAY_PUMP);
            doc["lights_state"] = _relayController.getState(RELAY_LIGHTS);
            
            // Add WiFi status
            doc["wifi_status"] = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
            doc["wifi_rssi"] = WiFi.RSSI();
            doc["wifi_ip"] = WiFi.localIP().toString();
            
            // Add MQTT status if MQTT manager is available
            if (_mqttManager) {
                doc["mqtt_status"] = _mqttManager->connected() ? "connected" : "disconnected";
            } else {
                doc["mqtt_status"] = "disabled";
            }
            
            // Add watering and light schedule information from GrowthManager
            const GrowthCycle& activeCycle = _growthManager.getActiveCycle();
            if (activeCycle.active) {
                GrowthStage* currentStage = _growthManager.getCurrentStageSettings();
                if (currentStage) {
                    // Watering information
                    JsonObject wateringInfo = doc.createNestedObject("watering_info");
                    bool pumpState = _relayController.getState(RELAY_PUMP);
                    
                    // Get watering timer info from main.cpp global variables
                    extern time_t lastWateringTime;
                    extern time_t pumpOnTime;
                    
                    time_t now = time(nullptr);
                    time_t secondsUntilNextChange = 0;
                    
                    if (pumpState) {
                        // Pump is running - calculate time until it turns off
                        if (pumpOnTime > 0) {
                            unsigned long wateringDurationSeconds = currentStage->waterDuration * 60; // Convert minutes to seconds
                            time_t pumpRunTime = now - pumpOnTime;
                            secondsUntilNextChange = wateringDurationSeconds - pumpRunTime;
                            if (secondsUntilNextChange < 0) secondsUntilNextChange = 0;
                        }
                    } else {
                        // Pump is off - calculate time until next watering
                        if (lastWateringTime > 0) {
                            unsigned long wateringIntervalSeconds = currentStage->waterInterval * 60; // Convert minutes to seconds
                            time_t secondsSinceLastWatering = now - lastWateringTime;
                            secondsUntilNextChange = wateringIntervalSeconds - secondsSinceLastWatering;
                            if (secondsUntilNextChange < 0) secondsUntilNextChange = 0;
                        }
                    }
                    
                    wateringInfo["seconds_until_next_change"] = secondsUntilNextChange;
                    wateringInfo["interval_minutes"] = currentStage->waterInterval;
                    wateringInfo["duration_minutes"] = currentStage->waterDuration;
                    
                    // Light schedule information
                    JsonObject lightInfo = doc.createNestedObject("light_info");
                    bool lightsState = _relayController.getState(RELAY_LIGHTS);
                    
                    // Calculate time until next light transition
                    struct tm timeinfo;
                    localtime_r(&now, &timeinfo);
                    int currentHour = timeinfo.tm_hour;
                    int currentMinute = timeinfo.tm_min;
                    int currentSecond = timeinfo.tm_sec;
                    
                    int lightStartHour = currentStage->lightStartHour;
                    int lightEndHour = (lightStartHour + currentStage->lightHours) % 24;
                    
                    int secondsUntilLightTransition = 0;
                    if (lightsState) {
                        // Lights are on, calculate time until they turn off
                        if (currentHour < lightEndHour) {
                            secondsUntilLightTransition = ((lightEndHour - currentHour) * 3600) - (currentMinute * 60) - currentSecond;
                        } else {
                            secondsUntilLightTransition = (((lightEndHour + 24) - currentHour) * 3600) - (currentMinute * 60) - currentSecond;
                        }
                    } else {
                        // Lights are off, calculate time until they turn on
                        if (currentHour < lightStartHour) {
                            secondsUntilLightTransition = ((lightStartHour - currentHour) * 3600) - (currentMinute * 60) - currentSecond;
                        } else {
                            secondsUntilLightTransition = (((lightStartHour + 24) - currentHour) * 3600) - (currentMinute * 60) - currentSecond;
                        }
                    }
                    
                    lightInfo["seconds_until_next_change"] = secondsUntilLightTransition;
                    lightInfo["light_hours"] = currentStage->lightHours;
                    lightInfo["start_hour"] = currentStage->lightStartHour;
                    lightInfo["end_hour"] = lightEndHour;
                }
            }
            
            serializeJson(doc, json);
            request->send(200, "application/json", json);
        });
        
        // Legacy sensor endpoint - redirects to status for backward compatibility
        _server.on("/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
            request->redirect("/status");
        });

        // Relay control endpoints
        AsyncCallbackJsonWebHandler *pumpHandler = new AsyncCallbackJsonWebHandler("/relay/pump", [this](AsyncWebServerRequest *request, JsonVariant &json) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("POST /relay/pump - Entering");
            JsonObject jsonObj = json.as<JsonObject>();
            
            if (jsonObj.containsKey("action")) {
                String action = jsonObj["action"];
                if (action == "toggle") {
                    bool newState = !_relayController.getState(RELAY_PUMP);
                    _relayController.setState(RELAY_PUMP, newState);
                    
                    AsyncJsonResponse *response = new AsyncJsonResponse();
                    JsonObject root = response->getRoot();
                    root["status"] = "ok";
                    response->setLength();
                    request->send(response);
                    return;
                }
            }
            request->send(400);
        });
        _server.addHandler(pumpHandler);

        AsyncCallbackJsonWebHandler *lightsHandler = new AsyncCallbackJsonWebHandler("/relay/lights", [this](AsyncWebServerRequest *request, JsonVariant &json) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("POST /relay/lights - Entering");
            JsonObject jsonObj = json.as<JsonObject>();
            
            if (jsonObj.containsKey("action")) {
                String action = jsonObj["action"];
                if (action == "toggle") {
                    bool newState = !_relayController.getState(RELAY_LIGHTS);
                    _relayController.setState(RELAY_LIGHTS, newState);
                    
                    AsyncJsonResponse *response = new AsyncJsonResponse();
                    JsonObject root = response->getRoot();
                    root["status"] = "ok";
                    response->setLength();
                    request->send(response);
                    return;
                }
            }
            request->send(400);
        });
        _server.addHandler(lightsHandler);

        // Growth Profile endpoints
        _server.on("/growth-profile", HTTP_GET, [this](AsyncWebServerRequest *request) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("GET /growth-profile - Entering");
            String json;
            StaticJsonDocument<2048> doc;
            
            // Add profiles to response
            JsonObject profilesObj = doc.createNestedObject("profiles");
            const GrowthProfile* profiles = _growthManager.getProfiles();
            int profileCount = _growthManager.getProfileCount();
            
            for (int i = 0; i < profileCount; i++) {
                String profileId = profiles[i].id;
                JsonObject profile = profilesObj.createNestedObject(profileId);
                
                profile["name"] = profiles[i].name;
                
                // Seedling stage
                JsonObject seedling = profile.createNestedObject("seedling");
                seedling["duration"] = profiles[i].seedling.duration;
                seedling["waterDuration"] = profiles[i].seedling.waterDuration;
                seedling["waterInterval"] = profiles[i].seedling.waterInterval;
                seedling["lightHours"] = profiles[i].seedling.lightHours;
                seedling["lightStartHour"] = profiles[i].seedling.lightStartHour;
                seedling["phMin"] = profiles[i].seedling.phMin;
                seedling["phMax"] = profiles[i].seedling.phMax;
                
                // Growing stage
                JsonObject growing = profile.createNestedObject("growing");
                growing["duration"] = profiles[i].growing.duration;
                growing["waterDuration"] = profiles[i].growing.waterDuration;
                growing["waterInterval"] = profiles[i].growing.waterInterval;
                growing["lightHours"] = profiles[i].growing.lightHours;
                growing["lightStartHour"] = profiles[i].growing.lightStartHour;
                growing["phMin"] = profiles[i].growing.phMin;
                growing["phMax"] = profiles[i].growing.phMax;
                
                // Harvesting stage
                JsonObject harvesting = profile.createNestedObject("harvesting");
                harvesting["duration"] = profiles[i].harvesting.duration;
                harvesting["waterDuration"] = profiles[i].harvesting.waterDuration;
                harvesting["waterInterval"] = profiles[i].harvesting.waterInterval;
                harvesting["lightHours"] = profiles[i].harvesting.lightHours;
                harvesting["lightStartHour"] = profiles[i].harvesting.lightStartHour;
                harvesting["phMin"] = profiles[i].harvesting.phMin;
                harvesting["phMax"] = profiles[i].harvesting.phMax;
            }
            
            // Add active cycle information if one exists
            const GrowthCycle& activeCycle = _growthManager.getActiveCycle();
            if (activeCycle.active) {
                JsonObject cycleObj = doc.createNestedObject("activeCycle");
                cycleObj["profileId"] = activeCycle.profileId;
                cycleObj["startTime"] = activeCycle.startTime;
                cycleObj["active"] = activeCycle.active;
                
                // Calculate current stage and time elapsed
                time_t now = time(nullptr);
                String currentStage = _growthManager.getCurrentGrowthStage(now);
                cycleObj["currentStage"] = currentStage;
                
                // Add elapsed and remaining days
                GrowthProfile* profile = _growthManager.findProfileById(activeCycle.profileId);
                if (profile) {
                    long elapsedSeconds = now - activeCycle.startTime;
                    int elapsedDays = elapsedSeconds / (24 * 60 * 60);
                    cycleObj["elapsedDays"] = elapsedDays;
                    
                    // Calculate total duration and remaining days
                    int totalDuration = profile->seedling.duration + profile->growing.duration + profile->harvesting.duration;
                    int remainingDays = totalDuration - elapsedDays;
                    if (remainingDays < 0) remainingDays = 0;
                    cycleObj["remainingDays"] = remainingDays;
                    cycleObj["totalDuration"] = totalDuration;
                    
                    // Add progress percentages for each stage
                    JsonObject progress = cycleObj.createNestedObject("progress");
                    int seedlingDuration = profile->seedling.duration;
                    int growingDuration = profile->growing.duration;
                    int harvestingDuration = profile->harvesting.duration;
                    
                    if (elapsedDays < seedlingDuration) {
                        // In seedling stage
                        progress["seedling"] = (elapsedDays * 100) / seedlingDuration;
                        progress["growing"] = 0;
                        progress["harvesting"] = 0;
                    } else if (elapsedDays < (seedlingDuration + growingDuration)) {
                        // In growing stage
                        progress["seedling"] = 100;
                        progress["growing"] = ((elapsedDays - seedlingDuration) * 100) / growingDuration;
                        progress["harvesting"] = 0;
                    } else if (elapsedDays < totalDuration) {
                        // In harvesting stage
                        progress["seedling"] = 100;
                        progress["growing"] = 100;
                        progress["harvesting"] = ((elapsedDays - seedlingDuration - growingDuration) * 100) / harvestingDuration;
                    } else {
                        // Completed
                        progress["seedling"] = 100;
                        progress["growing"] = 100;
                        progress["harvesting"] = 100;
                    }
                }
            }
            
            serializeJson(doc, json);
            request->send(200, "application/json", json);
        });

        AsyncCallbackJsonWebHandler *growthProfileHandler = new AsyncCallbackJsonWebHandler("/growth-profile", [this](AsyncWebServerRequest *request, JsonVariant &json) {
            if (!_auth.authenticate(request)) {
                return;
            }
            
            Serial.println("POST /growth-profile - Entering");
            JsonObject jsonObj = json.as<JsonObject>();
            Serial.println("Received JSON:");
            serializeJson(jsonObj, Serial);
            Serial.println();

            // Check the action field to determine what operation to perform
            if (jsonObj.containsKey("action")) {
                String action = jsonObj["action"];
                
                if (action == "save_profile" && jsonObj.containsKey("profileId") && jsonObj.containsKey("profile")) {
                    // Save or update a profile
                    const char* profileId = jsonObj["profileId"];
                    JsonObject profileObj = jsonObj["profile"];
                    
                    GrowthProfile newProfile;
                    // Copy ID and name
                    strlcpy(newProfile.id, profileId, sizeof(newProfile.id));
                    if (profileObj.containsKey("name")) {
                        strlcpy(newProfile.name, profileObj["name"], sizeof(newProfile.name));
                    } else {
                        strlcpy(newProfile.name, "Unnamed Profile", sizeof(newProfile.name));
                    }
                    
                    // Copy seedling stage settings
                    if (profileObj.containsKey("seedling")) {
                        JsonObject seedling = profileObj["seedling"];
                        newProfile.seedling.duration = seedling.containsKey("duration") ? seedling["duration"] : 14;
                        newProfile.seedling.waterDuration = seedling.containsKey("waterDuration") ? seedling["waterDuration"] : 5;
                        newProfile.seedling.waterInterval = seedling.containsKey("waterInterval") ? seedling["waterInterval"] : 60;
                        newProfile.seedling.lightHours = seedling.containsKey("lightHours") ? seedling["lightHours"] : 8;
                        newProfile.seedling.lightStartHour = seedling.containsKey("lightStartHour") ? seedling["lightStartHour"] : 6;
                        newProfile.seedling.phMin = seedling.containsKey("phMin") ? seedling["phMin"] : 5.5;
                        newProfile.seedling.phMax = seedling.containsKey("phMax") ? seedling["phMax"] : 6.5;
                    } else {
                        // Default seedling values
                        newProfile.seedling = {14, 5, 60, 8, 6, 5.5, 6.5};
                    }
                    
                    // Copy growing stage settings
                    if (profileObj.containsKey("growing")) {
                        JsonObject growing = profileObj["growing"];
                        newProfile.growing.duration = growing.containsKey("duration") ? growing["duration"] : 30;
                        newProfile.growing.waterDuration = growing.containsKey("waterDuration") ? growing["waterDuration"] : 5;
                        newProfile.growing.waterInterval = growing.containsKey("waterInterval") ? growing["waterInterval"] : 30;
                        newProfile.growing.lightHours = growing.containsKey("lightHours") ? growing["lightHours"] : 12;
                        newProfile.growing.lightStartHour = growing.containsKey("lightStartHour") ? growing["lightStartHour"] : 6;
                        newProfile.growing.phMin = growing.containsKey("phMin") ? growing["phMin"] : 5.8;
                        newProfile.growing.phMax = growing.containsKey("phMax") ? growing["phMax"] : 6.2;
                    } else {
                        // Default growing values
                        newProfile.growing = {30, 5, 30, 12, 6, 5.8, 6.2};
                    }
                    
                    // Copy harvesting stage settings
                    if (profileObj.containsKey("harvesting")) {
                        JsonObject harvesting = profileObj["harvesting"];
                        newProfile.harvesting.duration = harvesting.containsKey("duration") ? harvesting["duration"] : 14;
                        newProfile.harvesting.waterDuration = harvesting.containsKey("waterDuration") ? harvesting["waterDuration"] : 5;
                        newProfile.harvesting.waterInterval = harvesting.containsKey("waterInterval") ? harvesting["waterInterval"] : 45;
                        newProfile.harvesting.lightHours = harvesting.containsKey("lightHours") ? harvesting["lightHours"] : 10;
                        newProfile.harvesting.lightStartHour = harvesting.containsKey("lightStartHour") ? harvesting["lightStartHour"] : 6;
                        newProfile.harvesting.phMin = harvesting.containsKey("phMin") ? harvesting["phMin"] : 6.0;
                        newProfile.harvesting.phMax = harvesting.containsKey("phMax") ? harvesting["phMax"] : 6.5;
                    } else {
                        // Default harvesting values
                        newProfile.harvesting = {14, 5, 45, 10, 6, 6.0, 6.5};
                    }
                    
                    // Add or update the profile
                    bool success = _growthManager.addProfile(&newProfile);
                    
                    AsyncJsonResponse *response = new AsyncJsonResponse();
                    JsonObject root = response->getRoot();
                    root["status"] = success ? "ok" : "error";
                    if (!success) {
                        root["message"] = "Failed to save profile, maximum number of profiles reached";
                    }
                    response->setLength();
                    request->send(response);
                }
                else if (action == "start_cycle" && jsonObj.containsKey("cycle")) {
                    // Start a new growth cycle
                    JsonObject cycleObj = jsonObj["cycle"];
                    
                    bool success = false;
                    if (cycleObj.containsKey("profileId")) {
                        const char* profileId = cycleObj["profileId"];
                        
                        // Get start time, default to current time if not provided
                        unsigned long startTime;
                        if (cycleObj.containsKey("startTime")) {
                            startTime = cycleObj["startTime"];
                        } else {
                            startTime = time(nullptr);
                        }
                        
                        success = _growthManager.startGrowthCycle(profileId, startTime);
                    }
                    
                    AsyncJsonResponse *response = new AsyncJsonResponse();
                    JsonObject root = response->getRoot();
                    root["status"] = success ? "ok" : "error";
                    if (!success) {
                        root["message"] = "Failed to start cycle, profile not found";
                    }
                    response->setLength();
                    request->send(response);
                }
                else if (action == "stop_cycle") {
                    // Stop the active growth cycle
                    _growthManager.stopGrowthCycle();
                    
                    AsyncJsonResponse *response = new AsyncJsonResponse();
                    JsonObject root = response->getRoot();
                    root["status"] = "ok";
                    response->setLength();
                    request->send(response);
                }
                else {
                    // Unknown action
                    AsyncJsonResponse *response = new AsyncJsonResponse();
                    JsonObject root = response->getRoot();
                    root["status"] = "error";
                    root["message"] = "Unknown action";
                    response->setLength();
                    request->send(response);
                }
            } else {
                // No action specified
                AsyncJsonResponse *response = new AsyncJsonResponse();
                JsonObject root = response->getRoot();
                root["status"] = "error";
                root["message"] = "No action specified";
                response->setLength();
                request->send(response);
            }
        });
        _server.addHandler(growthProfileHandler);
    }
};
