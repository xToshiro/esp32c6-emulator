#ifndef SATELLITE_MANAGER_H
#define SATELLITE_MANAGER_H

#include "satellite_base.h"
#include "gps_satellite.h"
#include "sdcard_satellite.h"
#include "modem_satellite.h"
#include "sensor_satellites.h"
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <iostream>

// Central Satellite Peripheral Hub
// Coordinates all satellite modules (Sensors, SD, GPS, Modem) connected to the ESP32-C6 core.
// Allows disconnecting, reconnecting, power-gating, or fault-injecting any satellite module at runtime.
class SatelliteManager {
private:
    std::map<std::string, std::shared_ptr<ISatelliteModule>> _satellites;
    mutable std::recursive_mutex _mutex;

    SatelliteManager() = default;

public:
    static SatelliteManager& getInstance() {
        static SatelliteManager instance;
        return instance;
    }

    void registerSatellite(std::shared_ptr<ISatelliteModule> sat) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _satellites[sat->getName()] = sat;
    }

    std::shared_ptr<ISatelliteModule> getSatellite(const std::string &name) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        auto it = _satellites.find(name);
        if (it != _satellites.end()) return it->second;
        return nullptr;
    }

    // Toggle physical connection (cable plug/unplug)
    bool setSatelliteConnection(const std::string &name, bool connected) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        auto it = _satellites.find(name);
        if (it != _satellites.end()) {
            it->second->setPlugged(connected);
            return true;
        }
        return false;
    }

    // Toggle power rail (MCP23008 gating or battery cutoff)
    bool setSatellitePower(const std::string &name, bool powered) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        auto it = _satellites.find(name);
        if (it != _satellites.end()) {
            it->second->setPowered(powered);
            return true;
        }
        return false;
    }

    // Inject specific fault (e.g. NACK, TIMEOUT, NO_FIX, GARBAGE, CRC_ERROR)
    bool injectFault(const std::string &name, const std::string &fault) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        auto it = _satellites.find(name);
        if (it != _satellites.end()) {
            it->second->setFault(fault);
            return true;
        }
        return false;
    }

    void clearAllFaults() {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        for (auto &pair : _satellites) {
            pair.second->clearFault();
            pair.second->setPlugged(true);
            pair.second->setPowered(true);
        }
    }

    void stepAll(uint32_t dtMs) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        for (auto &pair : _satellites) {
            pair.second->step(dtMs);
        }
    }

    std::vector<std::string> listSatelliteNames() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        std::vector<std::string> names;
        for (const auto &pair : _satellites) {
            names.push_back(pair.first);
        }
        return names;
    }

    // Get diagnostic status of all satellite modules
    std::string getFullStatusReport() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        std::ostringstream ss;
        ss << "=== SATELLITE PERIPHERAL HUB STATUS ===\n";
        for (const auto &pair : _satellites) {
            ss << "  * " << pair.second->getStatusSummary() << "\n";
        }
        return ss.str();
    }
};

#endif // SATELLITE_MANAGER_H
