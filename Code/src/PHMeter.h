#ifndef PH_METER_H
#define PH_METER_H

#include <Arduino.h>
#include <Preferences.h>

class PHMeter {
public:
    PHMeter(uint8_t adcPin) : pin(adcPin) {}

    void begin() {
        preferences.begin("phmeter", false);

        int a4 = preferences.getInt("adc4", -1);
        int a7 = preferences.getInt("adc7", -1);
        int a10 = preferences.getInt("adc10", -1);

        // Load defaults if not set
        //if (a4 < 0 || a7 < 0 || a10 < 0) {
            // Default: assume 2481 = pH 7, +/- 420 for 4 & 10
            a4 = 2900;
            a7 = 2500;
            a10 = 2100;
            setCalibration(a4, a7, a10);
            saveCalibration(); // Save defaults
        // } else {
        //     setCalibration(a4, a7, a10);
        // }

        preferences.end();
    }

    // Save calibration to Preferences (flash)
    void saveCalibration() {
        preferences.begin("phmeter", false);
        preferences.putInt("adc4", adcValues[0]);
        preferences.putInt("adc7", adcValues[1]);
        preferences.putInt("adc10", adcValues[2]);
        preferences.end();
    }

    // Set ADC values corresponding to pH 4, 7, 10
    void setCalibration(int adc_ph4, int adc_ph7, int adc_ph10) {
        adcValues[0] = adc_ph4; phValues[0] = 4.0f;
        adcValues[1] = adc_ph7; phValues[1] = 7.0f;
        adcValues[2] = adc_ph10; phValues[2] = 10.0f;
        computeSlopeIntercept();
    }

    // Read raw ADC and convert to pH
    float readPH() {
        int adc = analogRead(pin);
        Serial.printf("ADC Value: %d\n", adc);
        return adcToPH(adc);
    }

    // Optional: convert ADC manually
    float adcToPH(int adcValue) const {
        return slope * adcValue + intercept;
    }

private:
    uint8_t pin;
    int adcValues[3];      // Raw ADC readings for pH 4, 7, 10
    float phValues[3];     // [4.0, 7.0, 10.0]
    float slope = 0.0f;
    float intercept = 0.0f;
    Preferences preferences;

    void computeSlopeIntercept() {
        float sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
        const int n = 3;

        for (int i = 0; i < n; ++i) {
            float x = static_cast<float>(adcValues[i]);
            float y = phValues[i];
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumXX += x * x;
        }

        float denom = n * sumXX - sumX * sumX;
        if (denom != 0.0f) {
            slope = (n * sumXY - sumX * sumY) / denom;
            intercept = (sumY - slope * sumX) / n;
        } else {
            slope = 0;
            intercept = 7.0f; // Neutral fallback
        }
    }
};

#endif // PH_METER_H
