#ifndef MODULE_SEEED_MULTIGAS_H
#define MODULE_SEEED_MULTIGAS_H

#include "i2c_bus.h"

class SeeedMultigasSensor : public VirtualI2CDevice {
private:
    float _co = 1.2f;    // ppm CO
    float _no2 = 0.05f;  // ppm NO2
    float _voc = 0.85f;  // ppm VOC
    uint8_t _cmd = 0;

public:
    SeeedMultigasSensor(uint8_t address = 0x08)
        : VirtualI2CDevice(address, "Seeed_GMXXX_Multichannel_Gas") {}

    void setReadings(float co, float no2, float voc) {
        _co = co;
        _no2 = no2;
        _voc = voc;
    }

    void getReadings(float &co, float &no2, float &voc) const {
        co = _co;
        no2 = _no2;
        voc = _voc;
    }

    void onWrite(const std::vector<uint8_t> &data) override {
        if (!data.empty()) _cmd = data[0];
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> res;
        if (!isPowered()) return res;
        // 4 bytes float or raw value
        res.resize(quantity, 0);
        return res;
    }
};

#endif // MODULE_SEEED_MULTIGAS_H
