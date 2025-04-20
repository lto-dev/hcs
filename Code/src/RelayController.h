#pragma once
#include <Arduino.h>

// Relay pin definitions
#define RELAY_PUMP_PIN 21
#define RELAY_LIGHTS_PIN 19
#define RELAY_PH_UP_PIN 18
#define RELAY_PH_DOWN_PIN 5

// Relay names
#define RELAY_PUMP 0
#define RELAY_LIGHTS 1
#define RELAY_PH_UP 2
#define RELAY_PH_DOWN 3
#define RELAY_COUNT 4

class RelayController {
private:
    struct Relay {
        uint8_t pin;
        bool state;
    };

    Relay relays[RELAY_COUNT] = {
        {RELAY_PUMP_PIN, false},    // Water Pump
        {RELAY_LIGHTS_PIN, false},  // Grow Lights
        {RELAY_PH_UP_PIN, false},   // pH Up
        {RELAY_PH_DOWN_PIN, false}  // pH Down
    };
    
    const char* relayNames[RELAY_COUNT] = {
        "WaterPump",
        "GrowLights", 
        "PH_Up",
        "PH_Down"
    };

public:
    RelayController() = default;

    void begin() {
        for (int i = 0; i < RELAY_COUNT; i++) {
            pinMode(relays[i].pin, OUTPUT);
            digitalWrite(relays[i].pin, LOW);
        }
    }

    void setState(uint8_t relayNum, bool state) {
        if (relayNum < RELAY_COUNT) {
            relays[relayNum].state = state;
            digitalWrite(relays[relayNum].pin, state ? HIGH : LOW);
        }
    }

    bool getState(uint8_t relayNum) {
        if (relayNum < RELAY_COUNT) {
            return relays[relayNum].state;
        }
        return false;
    }

    const char* getName(uint8_t relayNum) {
        if (relayNum < RELAY_COUNT) {
            return relayNames[relayNum];
        }
        return "Unknown";
    }
};
