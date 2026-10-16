#ifndef MODULE_SCD41_H
#define MODULE_SCD41_H

#include "i2c_bus.h"
#include <cmath>

class SCD41Sensor : public VirtualI2CDevice {
private:
    uint16_t _co2 = 420;        // ppm
    float _temperature = 25.5f; // °C
    float _humidity = 58.0f;    // %
    bool _periodicActive = false;
    uint16_t _lastCommand = 0;

    static uint8_t calculateCRC8(uint8_t msb, uint8_t lsb) {
        uint8_t crc = 0xFF;
        uint8_t data[2] = {msb, lsb};
        for (int i = 0; i < 2; i++) {
            crc ^= data[i];
            for (uint8_t bit = 8; bit > 0; --bit) {
                if (crc & 0x80) crc = (crc << 1) ^ 0x31;
                else crc = (crc << 1);
            }
        }
        return crc;
    }

public:
    SCD41Sensor(uint8_t address = 0x62)
        : VirtualI2CDevice(address, "Sensirion_SCD41_CO2") {}

    void setReadings(uint16_t co2, float temperature, float humidity) {
        _co2 = co2;
        _temperature = temperature;
        _humidity = humidity;
    }

    void getReadings(uint16_t &co2, float &temperature, float &humidity) const {
        co2 = _co2;
        temperature = _temperature;
        humidity = _humidity;
    }

    void onWrite(const std::vector<uint8_t> &data) override {
        if (data.size() >= 2) {
            _lastCommand = ((uint16_t)data[0] << 8) | data[1];
            if (_lastCommand == 0x21b1) {
                _periodicActive = true;
            } else if (_lastCommand == 0x3f86) {
                _periodicActive = false;
            }
        }
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> res;
        if (!isPowered()) return res;

        // Command 0xec05 (read_measurement)
        // 9 bytes: [CO2_MSB, CO2_LSB, CRC, T_MSB, T_LSB, CRC, RH_MSB, RH_LSB, CRC]
        uint16_t raw_co2 = _co2;
        uint8_t co2_msb = (raw_co2 >> 8) & 0xFF;
        uint8_t co2_lsb = raw_co2 & 0xFF;
        uint8_t co2_crc = calculateCRC8(co2_msb, co2_lsb);

        uint16_t raw_t = (uint16_t)std::clamp(std::round((_temperature + 45.0f) * 65535.0f / 175.0f), 0.0f, 65535.0f);
        uint8_t t_msb = (raw_t >> 8) & 0xFF;
        uint8_t t_lsb = raw_t & 0xFF;
        uint8_t t_crc = calculateCRC8(t_msb, t_lsb);

        uint16_t raw_h = (uint16_t)std::clamp(std::round(_humidity * 65535.0f / 100.0f), 0.0f, 65535.0f);
        uint8_t h_msb = (raw_h >> 8) & 0xFF;
        uint8_t h_lsb = raw_h & 0xFF;
        uint8_t h_crc = calculateCRC8(h_msb, h_lsb);

        uint8_t packet[9] = {co2_msb, co2_lsb, co2_crc, t_msb, t_lsb, t_crc, h_msb, h_lsb, h_crc};
        for (size_t i = 0; i < quantity && i < 9; ++i) {
            res.push_back(packet[i]);
        }
        return res;
    }
};

#endif // MODULE_SCD41_H
