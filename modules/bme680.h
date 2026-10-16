#ifndef MODULE_BME680_H
#define MODULE_BME680_H

#include "i2c_bus.h"

class BME680Sensor : public VirtualI2CDevice {
private:
    float _temperature = 26.2f; // °C
    float _humidity = 55.4f;    // %
    float _pressure = 1013.25f; // hPa
    float _gasResistance = 24500.0f; // Ohms
    uint8_t _activeRegister = 0x00;

public:
    BME680Sensor(uint8_t address = 0x77)
        : VirtualI2CDevice(address, "Bosch_BME680_Temp_Hum_Pres_Gas") {}

    void setReadings(float temp, float hum, float pres, float gasRes) {
        _temperature = temp;
        _humidity = hum;
        _pressure = pres;
        _gasResistance = gasRes;
    }

    void getReadings(float &temp, float &hum, float &pres, float &gasRes) const {
        temp = _temperature;
        hum = _humidity;
        pres = _pressure;
        gasRes = _gasResistance;
    }

    void onWrite(const std::vector<uint8_t> &data) override {
        if (!data.empty()) {
            _activeRegister = data[0];
        }
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> res;
        if (!isPowered()) return res;

        for (size_t i = 0; i < quantity; ++i) {
            uint8_t reg = _activeRegister + i;
            if (reg == 0xD0) {
                // Chip ID for BME680
                res.push_back(0x61);
            } else if (reg == 0x73) {
                // Status register (bit 7: new_data, bit 5: gas_valid)
                res.push_back(0xA0);
            } else {
                res.push_back(0x00);
            }
        }
        return res;
    }
};

#endif // MODULE_BME680_H
