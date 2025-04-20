#pragma once
#include <Arduino.h>
#include "HX710B.h"
#include <DFRobot_PH.h>
#include <GravityTDS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "PHMeter.h"

//TDS leaks current and needs to be powered on and off and influences the PH reading.
//todo: Need to greeze PH wile enabling TDS for reading....

class SensorReader
{
private:
    HX710B &hx710b;
    PHMeter &ph;
    GravityTDS &tds;
    DallasTemperature &temp;

    // Board pin definitions
    const uint8_t LIQUID_LEVEL_OUT_PIN = GPIO_NUM_26; // HX710B data pin
    const uint8_t LIQUID_LEVEL_SCK_PIN = GPIO_NUM_27; // HX710B clock pin

    const uint8_t PH_VALUE_PIN = GPIO_NUM_32; // Analog pin for pH sensor

    const uint8_t TDS_VCC_PIN = GPIO_NUM_13; // VCC pin for TDS sensor
    const uint8_t TDS_PIN = GPIO_NUM_39;  // TDS sensor pin
    // const uint8_t TEMP_PIN = GPIO_NUM_22; // Temperature pin sensor

    // Last readings
    float lastLiquidValue = NAN;    // Raw sensor value
    float lastLiquidLevel = NAN;    // Calculated level (percentage)
    float lastPH = NAN;
    float lastTDS = NAN;
    float lastTemperature = NAN;
    unsigned long lastReadTime = 0;
    
    // Calibration values for liquid level
    long calibrationMin = 0;  // Sensor value when empty
    long calibrationMax = 0;  // Sensor value when full
    long calibrationCritical = 0; // Critical level value
    
    // pH calibration values
    float ph4ADC = 0;   // ADC value at pH 4
    float ph7ADC = 0;   // ADC value at pH 7
    float ph10ADC = 0;  // ADC value at pH 10

public:
    SensorReader(HX710B &hx, PHMeter &phSensor, GravityTDS &tdsSensor, DallasTemperature &tempSensor)
        : hx710b(hx), ph(phSensor), tds(tdsSensor), temp(tempSensor) {}

    void begin()
    {
        temp.begin();

        hx710b.begin();

        ph.begin();

        tds.setPin(TDS_PIN);
        tds.setAref(3.3);
        tds.setAdcRange(4096);
        tds.begin();
    }

    void updateReadings()
    {
        if (millis() - lastReadTime < 1000)
            return; // Only read once per second

        // Read temperature
        temp.requestTemperatures();
        lastTemperature = temp.getTempCByIndex(0);

        // Read liquid level
        if (hx710b.is_ready())
        {
            lastLiquidValue = hx710b.read();
            
            // Calculate the liquid level percentage if calibration values are set
            if (calibrationMax != calibrationMin && !isnan(lastLiquidValue)) {
                lastLiquidLevel = map(lastLiquidValue, calibrationMin, calibrationMax, 0, 100);
                lastLiquidLevel = constrain(lastLiquidLevel, 0, 100);
            } else {
                lastLiquidLevel = NAN;
            }
        }
        else
        {
            lastLiquidValue = NAN;
            lastLiquidLevel = NAN;
        }

        // Read pH value using calibrated values if available
        float adcValue = analogRead(PH_VALUE_PIN);
        
        // If we have calibration data, use it for more accurate calculation
        if (ph4ADC > 0 && ph7ADC > 0) {
            // Calculate slope and intercept for pH curve
            float slope = (7.0 - 4.0) / (ph7ADC - ph4ADC);
            float intercept = 7.0 - slope * ph7ADC;
            lastPH = slope * adcValue + intercept;
            
            // If we have the pH 10 calibration point, use it to refine the calculation
            // for high pH values
            if (ph10ADC > 0 && adcValue > ph7ADC) {
                slope = (10.0 - 7.0) / (ph10ADC - ph7ADC);
                intercept = 7.0 - slope * ph7ADC;
                lastPH = slope * adcValue + intercept;
            }
        } else {
            // Fall back to PHMeter if no calibration is available
            lastPH = ph.readPH();
        }

        //analogWrite(TDS_VCC_PIN, HIGH); // Turn on TDS sensor
        //lastTDS = analogRead(TDS_PIN);// Read TDS value from analog pin
        analogWrite(TDS_VCC_PIN, LOW); // Turn off TDS sensor

        lastReadTime = millis();
    }

    float getLiquidValue() { return lastLiquidValue; } // Get raw sensor value
    float getLiquidLevel() { return lastLiquidLevel; } // Get calculated percentage
    float getPH() { return lastPH; }
    float getTDS() { return lastTDS; }
    float getTemperature() { return lastTemperature; }
    
    // Liquid calibration methods
    void setLiquidCalibration(long minValue, long maxValue, long criticalValue) {
        calibrationMin = minValue;
        calibrationMax = maxValue;
        calibrationCritical = criticalValue;
    }
    
    long getLiquidCalibrationMin() { return calibrationMin; }
    long getLiquidCalibrationMax() { return calibrationMax; }
    long getLiquidCalibrationCritical() { return calibrationCritical; }
    
    // pH calibration methods
    void setPHCalibration(float ph4_adc, float ph7_adc, float ph10_adc = 0) {
        ph4ADC = ph4_adc;
        ph7ADC = ph7_adc;
        ph10ADC = ph10_adc;
    }
    
    float getPH4ADC() { return ph4ADC; }
    float getPH7ADC() { return ph7ADC; }
    float getPH10ADC() { return ph10ADC; }
    
    // Return current ADC reading from the pH pin
    uint16_t getCurrentPHADC() { return analogRead(PH_VALUE_PIN); }
};
