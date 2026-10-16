#ifndef MODULE_DFROBOT_OZONE_H
#define MODULE_DFROBOT_OZONE_H

#include "i2c_bus.h"

#define OZONE_ADDRESS_3 0x73

class DFRobotOzoneSensor : public VirtualI2CDevice {
private:
    int16_t _ozonePpb = 35; // Ozone concentration in ppb
    uint8_t _reg = 0;

public:
    DFRobotOzoneSensor(uint8_t address = OZONE_ADDRESS_3)
        : VirtualI2CDevice(address, "DFRobot_Ozone_Sensor_O3") {}

    void setReadings(int16_t ozonePpb) {
        _ozonePpb = ozonePpb;
    }

    int16_t getReadings() const { return _ozonePpb; }

    void onWrite(const std::vector<uint8_t> &data) override {
        if (!data.empty()) _reg = data[0];
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> res;
        if (!isPowered()) return res;
        // Ozone data high and low byte
        res.push_back((_ozonePpb >> 8) & 0xFF);
        res.push_back(_ozonePpb & 0xFF);
        if (quantity > 2) {
            res.resize(quantity, 0);
        }
        return res;
    }
};

#endif // MODULE_DFROBOT_OZONE_H
