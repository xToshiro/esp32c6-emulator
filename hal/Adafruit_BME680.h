#ifndef MOCK_ADAFRUIT_BME680_H
#define MOCK_ADAFRUIT_BME680_H

#include "Wire.h"
#include "../modules/bme680.h"

class Adafruit_BME680 {
public:
    float temperature = 25.0f;
    float humidity = 50.0f;
    float pressure = 101325.0f;
    float gas_resistance = 20000.0f;

private:
    uint8_t _addr = 0x77;

public:
    bool begin(uint8_t addr = 0x77) {
        _addr = addr;
        auto dev = VirtualI2CBus::getInstance().getDevice(_addr);
        return (dev && dev->isEnabled());
    }

    void setGasHeater(uint16_t temp, uint16_t time) {
        (void)temp; (void)time;
    }

    bool performReading() {
        auto dev = std::dynamic_pointer_cast<BME680Sensor>(VirtualI2CBus::getInstance().getDevice(_addr));
        if (dev && dev->isEnabled()) {
            float t, h, p, g;
            dev->getReadings(t, h, p, g);
            temperature = t;
            humidity = h;
            pressure = p * 100.0f; // Pa
            gas_resistance = g;
            return true;
        }
        return false;
    }
};

#endif // MOCK_ADAFRUIT_BME680_H
