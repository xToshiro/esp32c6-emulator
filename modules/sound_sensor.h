#ifndef MODULE_SOUND_SENSOR_H
#define MODULE_SOUND_SENSOR_H

#include "../hal/Arduino.h"
#include <cmath>

class SoundSensorModel {
private:
    uint8_t _pin;
    float _targetDb = 45.0f; // dB SPL

public:
    SoundSensorModel(uint8_t pin = 4) : _pin(pin) {
        updateAdc();
    }

    void setDecibels(float db) {
        _targetDb = std::clamp(db, 30.0f, 120.0f);
        updateAdc();
    }

    float getDecibels() const {
        return _targetDb;
    }

    void updateAdc() {
        // v = 0.001 * 10^((db - 15) / 20)
        // deltaAdc = (v * 4095.0) / 3.3
        float exponent = (_targetDb - 15.0f) / 20.0f;
        float v = 0.001f * std::pow(10.0f, exponent);
        int deltaAdc = (int)std::clamp(std::round((v * 4095.0f) / 3.3f), 1.0f, 4090.0f);
        // Set mid-point reading so readSoundDB sees the peak
        setSimulatedAnalogRead(_pin, 2048 + deltaAdc / 2);
    }
};

#endif // MODULE_SOUND_SENSOR_H
