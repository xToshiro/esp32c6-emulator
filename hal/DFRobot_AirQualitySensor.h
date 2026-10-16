#ifndef MOCK_DFROBOT_AIR_QUALITY_SENSOR_H
#define MOCK_DFROBOT_AIR_QUALITY_SENSOR_H

#include "Wire.h"
#include "../modules/dfrobot_pm.h"

#define PARTICLE_PM1_0_ATMOSPHERE 1
#define PARTICLE_PM2_5_ATMOSPHERE 2
#define PARTICLE_PM10_ATMOSPHERE  3

class DFRobot_AirQualitySensor {
private:
    TwoWire *_wire;
    uint8_t _addr;

public:
    DFRobot_AirQualitySensor(TwoWire *wire, uint8_t addr = 0x19)
        : _wire(wire), _addr(addr) {}

    bool begin() {
        auto dev = VirtualI2CBus::getInstance().getDevice(_addr);
        return (dev && dev->isEnabled());
    }

    uint16_t gainParticleConcentration_ugm3(uint8_t type) {
        auto dev = std::dynamic_pointer_cast<DFRobotPMSensor>(VirtualI2CBus::getInstance().getDevice(_addr));
        if (dev && dev->isEnabled()) {
            uint16_t p1, p25, p10;
            dev->getReadings(p1, p25, p10);
            if (type == PARTICLE_PM1_0_ATMOSPHERE) return p1;
            if (type == PARTICLE_PM2_5_ATMOSPHERE) return p25;
            if (type == PARTICLE_PM10_ATMOSPHERE) return p10;
        }
        return 0;
    }
};

#endif // MOCK_DFROBOT_AIR_QUALITY_SENSOR_H
