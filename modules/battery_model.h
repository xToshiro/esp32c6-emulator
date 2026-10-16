#ifndef MODULE_BATTERY_MODEL_H
#define MODULE_BATTERY_MODEL_H

#include "../hal/Arduino.h"
#include <algorithm>

class BatteryModel {
private:
    uint8_t _pin;
    float _r1;
    float _r2;
    float _vRef;
    float _calFactor;
    float _currentVoltage = 3.85f; // Volts (Full)

public:
    BatteryModel(uint8_t pin = 5, float r1 = 47000.0f, float r2 = 100000.0f,
                 float vRef = 3.3f, float calFactor = 1.258f)
        : _pin(pin), _r1(r1), _r2(r2), _vRef(vRef), _calFactor(calFactor) {
        updateAdc();
    }

    void setVoltage(float volts) {
        _currentVoltage = std::clamp(volts, 0.0f, 4.35f);
        updateAdc();
    }

    float getVoltage() const {
        return _currentVoltage;
    }

    void updateAdc() {
        float dividerRatio = (_r1 + _r2) / _r2;
        float factor = _vRef * dividerRatio * _calFactor;
        int rawAdc = (int)std::clamp(std::round((_currentVoltage / factor) * 4095.0f), 0.0f, 4095.0f);
        setSimulatedAnalogRead(_pin, rawAdc);
    }
};

#endif // MODULE_BATTERY_MODEL_H
