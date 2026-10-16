#ifndef MOCK_WIRE_H
#define MOCK_WIRE_H

#include "Arduino.h"
#include "../modules/i2c_bus.h"
#include <vector>

class TwoWire {
private:
    uint8_t _txAddress = 0;
    std::vector<uint8_t> _txBuffer;
    std::vector<uint8_t> _rxBuffer;
    int _sdaPin = -1;
    int _sclPin = -1;
    uint16_t _timeoutMs = 150;
    bool _writeError = false;

public:
    TwoWire() = default;

    void begin(int sda = -1, int scl = -1) {
        _sdaPin = sda;
        _sclPin = scl;
    }

    void setPins(int sda, int scl) {
        _sdaPin = sda;
        _sclPin = scl;
    }

    void setTimeOut(uint16_t timeout_ms) {
        _timeoutMs = timeout_ms;
    }

    void clearWriteError() {
        _writeError = false;
    }

    void beginTransmission(uint8_t address) {
        _txAddress = address;
        _txBuffer.clear();
        _writeError = false;
    }

    size_t write(uint8_t data) {
        _txBuffer.push_back(data);
        return 1;
    }

    size_t write(const uint8_t *data, size_t quantity) {
        if (!data || quantity == 0) return 0;
        _txBuffer.insert(_txBuffer.end(), data, data + quantity);
        return quantity;
    }

    uint8_t endTransmission(bool sendStop = true) {
        (void)sendStop;
        uint8_t status = VirtualI2CBus::getInstance().writeToSlave(_txAddress, _txBuffer, millis());
        _txBuffer.clear();
        if (status != 0) _writeError = true;
        return status;
    }

    uint8_t requestFrom(uint8_t address, uint8_t count, bool sendStop = true) {
        (void)sendStop;
        _rxBuffer = VirtualI2CBus::getInstance().readFromSlave(address, count, millis());
        return (uint8_t)_rxBuffer.size();
    }

    uint8_t requestFrom(uint8_t address, uint8_t count, uint8_t sendStop) {
        return requestFrom(address, count, (bool)sendStop);
    }

    int available() {
        return (int)_rxBuffer.size();
    }

    int read() {
        if (_rxBuffer.empty()) return -1;
        uint8_t b = _rxBuffer.front();
        _rxBuffer.erase(_rxBuffer.begin());
        return (int)b;
    }

    int peek() {
        if (_rxBuffer.empty()) return -1;
        return (int)_rxBuffer.front();
    }
};

extern TwoWire Wire;

#endif // MOCK_WIRE_H
