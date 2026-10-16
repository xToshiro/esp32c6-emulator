#ifndef MOCK_SENSIRION_SCD4X_H
#define MOCK_SENSIRION_SCD4X_H

#include "Wire.h"
#include "../modules/scd41.h"

#define SCD41_I2C_ADDR_62 0x62

class SensirionI2cScd4x {
private:
    uint8_t _addr = SCD41_I2C_ADDR_62;

public:
    void begin(TwoWire &wire, uint8_t addr = SCD41_I2C_ADDR_62) {
        (void)wire;
        _addr = addr;
    }

    int16_t wakeUp() { return 0; }
    int16_t stopPeriodicMeasurement() { return 0; }
    int16_t reinit() { return 0; }
    int16_t startPeriodicMeasurement() {
        auto dev = VirtualI2CBus::getInstance().getDevice(_addr);
        return (dev && dev->isEnabled()) ? 0 : -1;
    }

    int16_t readMeasurement(uint16_t &co2, float &temp, float &hum) {
        auto dev = std::dynamic_pointer_cast<SCD41Sensor>(VirtualI2CBus::getInstance().getDevice(_addr));
        if (dev && dev->isEnabled()) {
            dev->getReadings(co2, temp, hum);
            return 0;
        }
        return -1;
    }
};

#endif // MOCK_SENSIRION_SCD4X_H
