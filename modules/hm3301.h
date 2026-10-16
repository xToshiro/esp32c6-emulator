#ifndef MODULE_HM3301_H
#define MODULE_HM3301_H

#include "i2c_bus.h"

class HM3301Sensor : public VirtualI2CDevice {
private:
    uint16_t _pm1_0 = 9;
    uint16_t _pm2_5 = 15;
    uint16_t _pm10 = 25;

public:
    HM3301Sensor(uint8_t address = 0x40)
        : VirtualI2CDevice(address, "Seeed_HM3301_Grove_PM") {}

    void setReadings(uint16_t pm1, uint16_t pm25, uint16_t pm10) {
        _pm1_0 = pm1;
        _pm2_5 = pm25;
        _pm10 = pm10;
    }

    void onWrite(const std::vector<uint8_t> &data) override {
        (void)data;
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> buf(29, 0);
        if (!isPowered()) return buf;

        // Populate 29-byte packet matching HM3301 spec
        buf[10] = (_pm1_0 >> 8) & 0xFF;
        buf[11] = _pm1_0 & 0xFF;
        buf[12] = (_pm2_5 >> 8) & 0xFF;
        buf[13] = _pm2_5 & 0xFF;
        buf[14] = (_pm10 >> 8) & 0xFF;
        buf[15] = _pm10 & 0xFF;

        uint8_t sum = 0;
        for (int i = 0; i < 28; i++) sum += buf[i];
        buf[28] = sum;

        if (quantity < 29) {
            buf.resize(quantity);
        }
        return buf;
    }
};

#endif // MODULE_HM3301_H
