#ifndef MOCK_MULTICHANNEL_GAS_GMXXX_H
#define MOCK_MULTICHANNEL_GAS_GMXXX_H

#include "Wire.h"
#include "../modules/seeed_multigas.h"

template<typename WireType = TwoWire>
class GAS_GMXXX {
private:
    uint8_t _addr = 0x08;

public:
    void begin(WireType &wire, uint8_t addr = 0x08) {
        (void)wire;
        _addr = addr;
    }

    float measure_CO() {
        auto dev = std::dynamic_pointer_cast<SeeedMultigasSensor>(VirtualI2CBus::getInstance().getDevice(_addr));
        if (dev && dev->isEnabled()) {
            float co, no2, voc;
            dev->getReadings(co, no2, voc);
            return co;
        }
        return 9999.0f;
    }

    float measure_NO2() {
        auto dev = std::dynamic_pointer_cast<SeeedMultigasSensor>(VirtualI2CBus::getInstance().getDevice(_addr));
        if (dev && dev->isEnabled()) {
            float co, no2, voc;
            dev->getReadings(co, no2, voc);
            return no2;
        }
        return 9999.0f;
    }

    float measure_VOC() {
        auto dev = std::dynamic_pointer_cast<SeeedMultigasSensor>(VirtualI2CBus::getInstance().getDevice(_addr));
        if (dev && dev->isEnabled()) {
            float co, no2, voc;
            dev->getReadings(co, no2, voc);
            return voc;
        }
        return 9999.0f;
    }
};

#endif // MOCK_MULTICHANNEL_GAS_GMXXX_H
