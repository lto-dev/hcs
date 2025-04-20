#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include "SensorReader.h"

struct SystemConfig {
  char device_id[32] = "tower1";
  bool mqtt_enabled = false;  // Flag to enable/disable MQTT
  char mqtt_server[64] = "mqtt.local";
  int mqtt_port = 1883;
  char mqtt_user[32] = "user";
  char mqtt_password[32] = "password";
  char ntp_server[64] = "pool.ntp.org";
  
  // Liquid level calibration
  long cal_dry = 0;      // Sensor not submerged
  long cal_critical = 0; // Pump safe level
  long cal_half = 0;     // 50% full
  long cal_full = 0;     // 100% full
  
  // pH calibration
  float ph4_adc = 0; // ADC reading at pH 4
  float ph7_adc = 0; // ADC reading at pH 7
  float ph10_adc = 0; // ADC reading at pH 10
};

class ConfigManager {
private:
  Preferences& _preferences;
  SystemConfig _config;
  SensorReader& _sensorReader;

public:
  ConfigManager(Preferences& preferences, SensorReader& sensorReader) 
    : _preferences(preferences), _sensorReader(sensorReader) {}

  void begin() {
    loadConfig();
  }

  const SystemConfig& getConfig() const {
    return _config;
  }

  SystemConfig& getConfig() {
    return _config;
  }

  void saveConfig() {
    _preferences.begin("hydroponics", false);
    _preferences.putBytes("config", &_config, sizeof(SystemConfig));
    _preferences.end();
    
    // Update sensor calibration values
    updateSensorCalibration();
  }

  void loadConfig() {
    _preferences.begin("hydroponics", false);

    // Check if config exists and is the correct size
    if (_preferences.getBytesLength("config") != sizeof(SystemConfig)) {
      // No config exists - initialize with defaults
      Serial.println("No saved config found - initializing with defaults");

      // Set default MQTT values
      strlcpy(_config.mqtt_server, "mqtt.local", sizeof(_config.mqtt_server));
      _config.mqtt_port = 1883;
      strlcpy(_config.mqtt_user, "", sizeof(_config.mqtt_user));
      strlcpy(_config.mqtt_password, "", sizeof(_config.mqtt_password));

      // Set default device ID based on MAC address
      uint8_t mac[6];
      WiFi.macAddress(mac);
      snprintf(_config.device_id, sizeof(_config.device_id),
              "tower-%02x%02x%02x", mac[3], mac[4], mac[5]);

      // Save the defaults
      saveConfig();
    } else {
      // Load existing config
      _preferences.getBytes("config", &_config, sizeof(SystemConfig));
    }

    _preferences.end();
    
    // Apply calibration values to sensors
    updateSensorCalibration();
  }

private:
  void updateSensorCalibration() {
    // Set calibration values in SensorReader
    _sensorReader.setLiquidCalibration(_config.cal_dry, _config.cal_full, _config.cal_critical);
    _sensorReader.setPHCalibration(_config.ph4_adc, _config.ph7_adc, _config.ph10_adc);
  }
};
