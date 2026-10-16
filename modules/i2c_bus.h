#ifndef VIRTUAL_I2C_BUS_H
#define VIRTUAL_I2C_BUS_H

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <iostream>
#include <functional>

// Abstract Virtual I2C Peripheral Device
class VirtualI2CDevice {
protected:
    uint8_t _address;
    std::string _name;
    bool _powered = true;
    bool _enabled = true;

public:
    VirtualI2CDevice(uint8_t address, const std::string &name)
        : _address(address), _name(name) {}
    virtual ~VirtualI2CDevice() = default;

    uint8_t getAddress() const { return _address; }
    std::string getName() const { return _name; }

    virtual void setPower(bool on) { _powered = on; }
    virtual bool isPowered() const { return _powered; }

    virtual void setEnabled(bool enabled) { _enabled = enabled; }
    virtual bool isEnabled() const { return _enabled && _powered; }

    // Handlers called by TwoWire / Master
    virtual void onWrite(const std::vector<uint8_t> &data) = 0;
    virtual std::vector<uint8_t> onRead(size_t quantity) = 0;
    virtual void reset() {}
};

// I2C Bus Transaction Event Log
struct I2CTransactionLog {
    uint8_t address;
    bool isWrite;
    std::vector<uint8_t> data;
    bool ack;
    uint32_t timestampMs;
};

// Virtual I2C Master Bus Controller
class VirtualI2CBus {
private:
    std::map<uint8_t, std::shared_ptr<VirtualI2CDevice>> _devices;
    std::vector<I2CTransactionLog> _history;
    std::mutex _mutex;
    std::function<void(const I2CTransactionLog&)> _onTransactionCallback;

    VirtualI2CBus() = default;

public:
    static VirtualI2CBus& getInstance() {
        static VirtualI2CBus instance;
        return instance;
    }

    void registerDevice(std::shared_ptr<VirtualI2CDevice> dev) {
        std::lock_guard<std::mutex> lock(_mutex);
        _devices[dev->getAddress()] = dev;
    }

    void unregisterDevice(uint8_t address) {
        std::lock_guard<std::mutex> lock(_mutex);
        _devices.erase(address);
    }

    std::shared_ptr<VirtualI2CDevice> getDevice(uint8_t address) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _devices.find(address);
        if (it != _devices.end()) return it->second;
        return nullptr;
    }

    // Bus scan: returns addresses of devices that respond with ACK
    std::vector<uint8_t> scanBus(uint8_t startAddr = 8, uint8_t endAddr = 120) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<uint8_t> found;
        for (uint8_t addr = startAddr; addr <= endAddr; ++addr) {
            auto it = _devices.find(addr);
            if (it != _devices.end() && it->second->isEnabled()) {
                found.push_back(addr);
            }
        }
        return found;
    }

    // Master writes to slave
    // Returns 0 on ACK, 2 on NACK (address not found or disabled), 3 on data error
    uint8_t writeToSlave(uint8_t address, const std::vector<uint8_t> &data, uint32_t timestampMs = 0) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _devices.find(address);
        bool ack = (it != _devices.end() && it->second->isEnabled());

        I2CTransactionLog log{address, true, data, ack, timestampMs};
        _history.push_back(log);
        if (_onTransactionCallback) _onTransactionCallback(log);

        if (!ack) return 2; // NACK

        it->second->onWrite(data);
        return 0; // Success ACK
    }

    // Master reads from slave
    std::vector<uint8_t> readFromSlave(uint8_t address, size_t quantity, uint32_t timestampMs = 0) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _devices.find(address);
        bool ack = (it != _devices.end() && it->second->isEnabled());

        std::vector<uint8_t> result;
        if (ack) {
            result = it->second->onRead(quantity);
        }

        I2CTransactionLog log{address, false, result, ack, timestampMs};
        _history.push_back(log);
        if (_onTransactionCallback) _onTransactionCallback(log);

        return result;
    }

    void setOnTransactionCallback(std::function<void(const I2CTransactionLog&)> cb) {
        std::lock_guard<std::mutex> lock(_mutex);
        _onTransactionCallback = cb;
    }

    void clearHistory() {
        std::lock_guard<std::mutex> lock(_mutex);
        _history.clear();
    }
};

#endif // VIRTUAL_I2C_BUS_H
