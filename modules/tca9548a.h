#ifndef MODULE_TCA9548A_H
#define MODULE_TCA9548A_H

#include "i2c_bus.h"

class TCA9548AMux : public VirtualI2CDevice {
private:
    uint8_t _channelMask = 0x00;

public:
    TCA9548AMux(uint8_t address = 0x70)
        : VirtualI2CDevice(address, "TCA9548A_I2C_Multiplexer") {}

    void onWrite(const std::vector<uint8_t> &data) override {
        if (!data.empty()) {
            _channelMask = data[0];
        }
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> res;
        if (quantity > 0) res.push_back(_channelMask);
        return res;
    }

    uint8_t getChannelMask() const { return _channelMask; }
    bool isChannelActive(uint8_t channel) const {
        if (channel > 7) return false;
        return (_channelMask & (1 << channel)) != 0;
    }
};

#endif // MODULE_TCA9548A_H
