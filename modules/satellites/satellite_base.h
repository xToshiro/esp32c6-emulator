#ifndef SATELLITE_BASE_H
#define SATELLITE_BASE_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <sstream>

enum class SatelliteType {
    SENSOR_I2C,
    STORAGE_SPI,
    UART_GPS,
    MODEM_NET,
    ANALOG_ADC,
    EXPANDER_I2C
};

// Abstract Satellite Module Interface
// Represents an independent physical peripheral that can be plugged, unplugged,
// power-gated, or fault-injected at any moment during execution.
class ISatelliteModule {
protected:
    std::string _name;
    std::string _subsystem;
    SatelliteType _type;
    bool _plugged = true;       // Physical connection (cable/socket)
    bool _powered = true;       // Power rail status (e.g. via MCP23008)
    std::string _activeFault = "NONE";
    mutable std::recursive_mutex _mutex;

public:
    ISatelliteModule(const std::string &name, const std::string &subsystem, SatelliteType type)
        : _name(name), _subsystem(subsystem), _type(type) {}
    virtual ~ISatelliteModule() = default;

    std::string getName() const { return _name; }
    std::string getSubsystem() const { return _subsystem; }
    SatelliteType getType() const { return _type; }

    // Physical Plug / Unplug
    virtual void setPlugged(bool plugged) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _plugged = plugged;
    }
    virtual bool isPlugged() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _plugged;
    }

    // Power Rail (Gated via MCP23008 or Battery Cutoff)
    virtual void setPowered(bool powered) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _powered = powered;
    }
    virtual bool isPowered() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _powered;
    }

    // Module is active only when physically plugged AND powered
    virtual bool isOperational() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _plugged && _powered && (_activeFault != "DISCONNECTED") && (_activeFault != "DEAD");
    }

    // Fault Injection
    virtual void setFault(const std::string &fault) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _activeFault = fault;
    }
    virtual std::string getFault() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _activeFault;
    }
    virtual void clearFault() {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _activeFault = "NONE";
    }

    // Periodic tick
    virtual void step(uint32_t dtMs) = 0;

    // Diagnostic status summary
    virtual std::string getStatusSummary() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        std::ostringstream ss;
        ss << "Module: " << _name << " [" << _subsystem << "] "
           << "Plugged: " << (_plugged ? "YES" : "NO (Desconectado)") << ", "
           << "Power: " << (_powered ? "ON" : "OFF (Corte)") << ", "
           << "Fault: " << _activeFault;
        return ss.str();
    }
};

#endif // SATELLITE_BASE_H
