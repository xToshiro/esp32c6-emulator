#ifndef MOCK_SEEED_HM330X_H
#define MOCK_SEEED_HM330X_H

#include "Wire.h"
#include "../modules/hm3301.h"

#define NO_ERROR 0

class HM330X {
private:
    uint8_t _addr = 0x40;

public:
    int init() {
        auto dev = VirtualI2CBus::getInstance().getDevice(_addr);
        return (dev && dev->isEnabled()) ? NO_ERROR : -1;
    }

    int read_sensor_value(uint8_t *data, uint32_t len) {
        if (!data || len < 29) return -1;
        auto dev = VirtualI2CBus::getInstance().getDevice(_addr);
        if (!dev || !dev->isEnabled()) return -1;
        std::vector<uint8_t> res = dev->onRead(len);
        if (res.size() < 29) return -1;
        std::memcpy(data, res.data(), 29);
        return NO_ERROR;
    }
};

#endif // MOCK_SEEED_HM330X_H
