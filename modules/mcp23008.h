#ifndef MODULE_MCP23008_H
#define MODULE_MCP23008_H

#include "i2c_bus.h"
#include <map>
#include <functional>

class MCP23008Expander : public VirtualI2CDevice {
private:
    uint8_t _selectedRegister = 0x00;
    uint8_t _registers[11] = {0}; // 0x00 IODIR, 0x09 GPIO, 0x0A OLAT, etc.
    std::function<void(uint8_t pin, bool state)> _onPinStateChange;

public:
    MCP23008Expander(uint8_t address = 0x20)
        : VirtualI2CDevice(address, "MCP23008_GPIO_Expander") {
        _registers[0x00] = 0x00; // IODIR default output
        _registers[0x09] = 0xFF; // GPIO all high (all sensors powered)
    }

    void onWrite(const std::vector<uint8_t> &data) override {
        if (data.empty()) return;
        _selectedRegister = data[0];
        if (data.size() > 1) {
            uint8_t val = data[1];
            if (_selectedRegister < sizeof(_registers)) {
                uint8_t oldGpio = _registers[0x09];
                _registers[_selectedRegister] = val;
                
                // If writing to GPIO (0x09) or OLAT (0x0A), notify pin changes
                if (_selectedRegister == 0x09 || _selectedRegister == 0x0A) {
                    _registers[0x09] = val;
                    for (uint8_t pin = 0; pin < 8; ++pin) {
                        bool oldState = (oldGpio & (1 << pin)) != 0;
                        bool newState = (val & (1 << pin)) != 0;
                        if (oldState != newState && _onPinStateChange) {
                            _onPinStateChange(pin, newState);
                        }
                    }
                }
            }
        }
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> res;
        for (size_t i = 0; i < quantity; ++i) {
            uint8_t reg = (_selectedRegister + i) % sizeof(_registers);
            res.push_back(_registers[reg]);
        }
        return res;
    }

    uint8_t getGpioState() const { return _registers[0x09]; }
    bool getPinState(uint8_t pin) const {
        if (pin > 7) return false;
        return (_registers[0x09] & (1 << pin)) != 0;
    }

    void setOnPinStateChange(std::function<void(uint8_t, bool)> cb) {
        _onPinStateChange = cb;
    }
};

#endif // MODULE_MCP23008_H
