#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "Config.h"

// Callback function type
typedef std::function<void(const String& topic, const String& payload)> MqttCallback;

class MQTTManager {
private:
    WiFiClient& _wifiClient;
    PubSubClient _mqttClient;
    SystemConfig& _config;
    MqttCallback _callback;

    // MQTT Topics
    char _topic_liquid[50];
    char _topic_ph[50];
    char _topic_tds[50];
    char _topic_temperature[50];
    char _topic_pump[50];
    char _topic_lights[50];
    char _topic_ph_up[50];
    char _topic_ph_down[50];
    char _topic_alerts[50];

public:
    MQTTManager(WiFiClient& wifiClient, SystemConfig& config) 
        : _wifiClient(wifiClient), 
          _mqttClient(wifiClient),
          _config(config) {
        
        setupTopics();
    }

    void setCallback(MqttCallback callback) {
        _callback = callback;
        
        // Set the PubSubClient callback
        _mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            String message = String((char*)payload).substring(0, length);
            Serial.printf("MQTT Message Received - Topic: %s, Payload: %s\n", topic, message.c_str());
            
            // Call the user's callback
            if (this->_callback) {
                this->_callback(String(topic), message);
            }
        });
    }

    void begin() {
        setupTopics();
        _mqttClient.setServer(_config.mqtt_server, _config.mqtt_port);
    }

    void setupTopics() {
        // Set up MQTT topics
        snprintf(_topic_liquid, sizeof(_topic_liquid), "hydroponics/%s/liquid_level", _config.device_id);
        snprintf(_topic_ph, sizeof(_topic_ph), "hydroponics/%s/ph_value", _config.device_id);
        snprintf(_topic_tds, sizeof(_topic_tds), "hydroponics/%s/tds_value", _config.device_id);
        snprintf(_topic_temperature, sizeof(_topic_temperature), "hydroponics/%s/temperature_value", _config.device_id);
        snprintf(_topic_pump, sizeof(_topic_pump), "hydroponics/%s/pump_state", _config.device_id);
        snprintf(_topic_lights, sizeof(_topic_lights), "hydroponics/%s/lights_state", _config.device_id);
        snprintf(_topic_ph_up, sizeof(_topic_ph_up), "hydroponics/%s/ph_up_state", _config.device_id);
        snprintf(_topic_ph_down, sizeof(_topic_ph_down), "hydroponics/%s/ph_down_state", _config.device_id);
        snprintf(_topic_alerts, sizeof(_topic_alerts), "hydroponics/%s/alerts", _config.device_id);
    }

    bool connect() {
        if (_mqttClient.connected()) {
            return true;
        }
        
        Serial.println("Attempting MQTT connection...");
        String clientId = "HydroponicsController-";
        clientId += _config.device_id;

        Serial.printf("Connecting to broker: %s:%d\n", _config.mqtt_server, _config.mqtt_port);
        Serial.printf("Client ID: %s\n", clientId.c_str());
        Serial.printf("Username: %s\n", _config.mqtt_user);

        bool connected = _mqttClient.connect(clientId.c_str(), _config.mqtt_user, _config.mqtt_password);

        if (!connected) {
            int state = _mqttClient.state();
            Serial.print("MQTT connection failed, state=");
            Serial.print(state);
            Serial.print(" (");
            switch (state) {
            case -4:
                Serial.print("Connection timeout");
                break;
            case -3:
                Serial.print("Connection lost");
                break;
            case -2:
                Serial.print("Connect failed");
                break;
            case -1:
                Serial.print("Disconnected");
                break;
            case 1:
                Serial.print("Bad protocol");
                break;
            case 2:
                Serial.print("Bad client ID");
                break;
            case 3:
                Serial.print("Unavailable");
                break;
            case 4:
                Serial.print("Bad credentials");
                break;
            case 5:
                Serial.print("Unauthorized");
                break;
            default:
                Serial.print("Unknown error");
            }
            Serial.println(")");
            return false;
        }
        
        Serial.println("Successfully connected to MQTT broker");
        
        // Subscribe to control topics
        Serial.println("Subscribing to topics:");
        Serial.printf("- %s\n", _topic_pump);
        Serial.printf("- %s\n", _topic_lights);

        bool pumpSub = _mqttClient.subscribe(_topic_pump, 1);
        bool lightsSub = _mqttClient.subscribe(_topic_lights, 1);

        Serial.printf("Subscription results - Pump: %s, Lights: %s\n",
                    pumpSub ? "success" : "failed",
                    lightsSub ? "success" : "failed");

        // Publish discovery messages for Home Assistant
        if (pumpSub && lightsSub) {
            Serial.println("Successfully subscribed to all topics");
            publishDiscoveryMessages();
        }
        
        return connected;
    }

    void loop() {
        _mqttClient.loop();
    }

    bool connected() {
        return _mqttClient.connected();
    }
    
    void disconnect() {
        if (_mqttClient.connected()) {
            Serial.println("Disconnecting from MQTT broker");
            _mqttClient.disconnect();
        }
    }

    bool publish(const char* topic, const char* payload, bool retain = false) {
        return _mqttClient.publish(topic, payload, retain);
    }

    bool publish(const char* topic, const String& payload, bool retain = false) {
        return _mqttClient.publish(topic, payload.c_str(), retain);
    }

    // Convenience methods for publishing to specific topics
    bool publishLiquidLevel(float level) {
        if (!_mqttClient.connected() || isnan(level)) {
            return false;
        }
        return _mqttClient.publish(_topic_liquid, String((int)level).c_str());
    }

    bool publishPH(float ph) {
        if (!_mqttClient.connected() || isnan(ph)) {
            return false;
        }
        return _mqttClient.publish(_topic_ph, String(ph).c_str());
    }

    bool publishTDS(float tds) {
        if (!_mqttClient.connected() || isnan(tds)) {
            return false;
        }
        return _mqttClient.publish(_topic_tds, String(tds).c_str());
    }

    bool publishTemperature(float temp) {
        if (!_mqttClient.connected() || isnan(temp)) {
            return false;
        }
        return _mqttClient.publish(_topic_temperature, String(temp).c_str());
    }

    bool publishAlert(const String& message) {
        if (!_mqttClient.connected() || message.isEmpty()) {
            return false;
        }
        return _mqttClient.publish(_topic_alerts, message.c_str());
    }

    bool publishPumpState(bool state) {
        if (!_mqttClient.connected()) {
            return false;
        }
        return _mqttClient.publish(_topic_pump, state ? "ON" : "OFF");
    }

    bool publishLightsState(bool state) {
        if (!_mqttClient.connected()) {
            return false;
        }
        return _mqttClient.publish(_topic_lights, state ? "ON" : "OFF");
    }

    const char* getTopicPump() const { return _topic_pump; }
    const char* getTopicLights() const { return _topic_lights; }
    const char* getTopicAlerts() const { return _topic_alerts; }

private:
    void publishDiscoveryMessages() {
        char discoveryTopic[100];
        StaticJsonDocument<512> doc;

        // Liquid Level Sensor
        snprintf(discoveryTopic, sizeof(discoveryTopic), "homeassistant/sensor/%s/liquid_level/config", _config.device_id);
        doc["name"] = "Liquid Level";
        doc["uniq_id"] = String(_config.device_id) + "_liquid_level";
        doc["stat_t"] = _topic_liquid;
        doc["unit_of_meas"] = "%";
        doc["dev_cla"] = "water";
        doc["ic"] = "mdi:water-percent";
        _mqttClient.publish(discoveryTopic, doc.as<String>().c_str(), true);
        doc.clear();

        // pH Sensor
        snprintf(discoveryTopic, sizeof(discoveryTopic), "homeassistant/sensor/%s/ph_value/config", _config.device_id);
        doc["name"] = "pH Value";
        doc["uniq_id"] = String(_config.device_id) + "_ph_value";
        doc["stat_t"] = _topic_ph;
        doc["unit_of_meas"] = "pH";
        doc["ic"] = "mdi:ph";
        _mqttClient.publish(discoveryTopic, doc.as<String>().c_str(), true);
        doc.clear();

        // TDS Sensor
        snprintf(discoveryTopic, sizeof(discoveryTopic), "homeassistant/sensor/%s/tds_value/config", _config.device_id);
        doc["name"] = "TDS Value";
        doc["uniq_id"] = String(_config.device_id) + "_tds_value";
        doc["stat_t"] = _topic_tds;
        doc["unit_of_meas"] = "ppm";
        doc["ic"] = "mdi:water";
        _mqttClient.publish(discoveryTopic, doc.as<String>().c_str(), true);
        doc.clear();

        // Pump Switch
        snprintf(discoveryTopic, sizeof(discoveryTopic), "homeassistant/switch/%s/pump/config", _config.device_id);
        doc["name"] = "Pump";
        doc["uniq_id"] = String(_config.device_id) + "_pump";
        doc["stat_t"] = _topic_pump;
        doc["cmd_t"] = _topic_pump;
        doc["ic"] = "mdi:pump";
        _mqttClient.publish(discoveryTopic, doc.as<String>().c_str(), true);
        doc.clear();

        // Lights Switch
        snprintf(discoveryTopic, sizeof(discoveryTopic), "homeassistant/switch/%s/lights/config", _config.device_id);
        doc["name"] = "Grow Lights";
        doc["uniq_id"] = String(_config.device_id) + "_lights";
        doc["stat_t"] = _topic_lights;
        doc["cmd_t"] = _topic_lights;
        doc["ic"] = "mdi:lightbulb";
        _mqttClient.publish(discoveryTopic, doc.as<String>().c_str(), true);
        doc.clear();
    }
};
