#ifndef MODULE_DFROBOT_PM_H
#define MODULE_DFROBOT_PM_H

#include "i2c_bus.h"

class DFRobotPMSensor : public VirtualI2CDevice {
private:
    uint16_t _pm1_0 = 8;   // ug/m3
    uint16_t _pm2_5 = 14;  // ug/m3
    uint16_t _pm10 = 22;   // ug/m3
    uint8_t _reg = 0;

public:
    DFRobotPMSensor(uint8_t address = 0x19)
        : VirtualI2CDevice(address, "DFRobot_AirQualitySensor_PM") {}

    void setReadings(uint16_t pm1, uint16_t pm25, uint16_t pm10) {
        _pm1_0 = pm1;
        _pm2_5 = pm25;
        _pm10 = pm10;
    }

    void getReadings(uint16_t &pm1, uint16_t &pm25, uint16_t &pm10) const {
        pm1 = _pm1_0;
        pm25 = _pm2_5;
        pm10 = _pm10;
    }

    void onWrite(const std::vector<uint8_t> &data) override {
        if (!data.empty()) _reg = data[0];
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> res;
        if (!isPowered()) return res;
        for (size_t i = 0; i < quantity; ++i) {
            uint8_t r = _reg + i;
            if (r == 0x0D) res.push_back((_pm1_0 >> 8) & 0xFF);
            else if (r == 0x0E) res.push_back(_pm1_0 & 0xFF);
            else if (r == 0x0F) res.push_back((_pm2_5 >> 8) & 0xFF);
            else if (r == 0x10) res.push_back(_pm2_5 & 0xFF);
            else if (r == 0x11) res.push_back((_pm10 >> 8) & 0xFF);
            else if (r == 0x12) res.push_back(_pm10 & 0xFF);
            else res.push_back(0);
        }
        return res;
    }
};

#endif // MODULE_DFROBOT_PM_H
