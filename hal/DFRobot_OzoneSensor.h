#ifndef MOCK_DFROBOT_OZONE_SENSOR_H
#define MOCK_DFROBOT_OZONE_SENSOR_H

#include "Wire.h"
#include "../modules/dfrobot_ozone.h"

class DFRobot_OzoneSensor {
private:
    uint8_t _addr = OZONE_ADDRESS_3;

public:
    bool begin(uint8_t addr = OZONE_ADDRESS_3) {
        _addr = addr;
        auto dev = VirtualI2CBus::getInstance().getDevice(_addr);
        return (dev && dev->isEnabled());
    }

    int16_t readOzoneData(uint8_t count = 10) {
        (void)count;
        auto dev = std::dynamic_pointer_cast<DFRobotOzoneSensor>(VirtualI2CBus::getInstance().getDevice(_addr));
        if (dev && dev->isEnabled()) {
            return dev->getReadings();
        }
        return 0;
    }
};

#endif // MOCK_DFROBOT_OZONE_SENSOR_H
