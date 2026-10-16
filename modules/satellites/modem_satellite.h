#ifndef MODEM_SATELLITE_H
#define MODEM_SATELLITE_H

#include "satellite_base.h"
#include "../../hal/WiFi.h"
#include "../../hal/HTTPClient.h"
#include <string>

// Satellite Modem / WiFi Network Transport Module
// Simulates network interface (STA / Cellular Modem) handling cloud sync to Vitae API.
// Can be disconnected, network-dropped, or forced into HTTP 500 failover at any time.
class ModemSatelliteModule : public ISatelliteModule {
private:
    std::string _ssid = "MoQa_Wifi_Infra";
    std::string _ip = "192.168.1.105";
    int _rssi = -64;
    size_t _packetsDispatched = 0;
    size_t _packetsFailed = 0;

public:
    ModemSatelliteModule(const std::string &name = "MODEM_WIFI_ESP32C6")
        : ISatelliteModule(name, "NETWORK_MODEM", SatelliteType::MODEM_NET) {}

    void setPlugged(bool plugged) override {
        ISatelliteModule::setPlugged(plugged);
        if (!plugged) {
            WiFi.disconnect();
        } else {
            WiFi.begin(_ssid.c_str(), "dummy");
        }
    }

    void setSsid(const std::string &ssid) { _ssid = ssid; }
    std::string getSsid() const { return _ssid; }

    int getRssi() const { return _rssi; }
    void setRssi(int rssi) { _rssi = rssi; }

    void step(uint32_t dtMs) override {
        (void)dtMs;
        // Keep WiFi status aligned with satellite operational state
        if (!isOperational() || _activeFault == "OFFLINE") {
            if (WiFi.status() == WL_CONNECTED) WiFi.disconnect();
        }
    }

    bool dispatchTelemetry(const std::string &primaryUrl, const std::string &backupUrl, const std::string &payload, int &outHttpCode) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (!isOperational() || _activeFault == "OFFLINE") {
            outHttpCode = -1; // Connection refused / offline
            _packetsFailed++;
            return false;
        }

        if (_activeFault == "HTTP_500_FAILOVER") {
            _packetsFailed++;
            // Primary fails with 500, backup succeeds with 200
            HTTPClient http;
            http.begin(backupUrl.c_str());
            outHttpCode = 200;
            _packetsDispatched++;
            return true;
        }

        // Normal successful transmission
        _packetsDispatched++;
        outHttpCode = 200;
        return true;
    }

    size_t getPacketsDispatched() const { return _packetsDispatched; }
    size_t getPacketsFailed() const { return _packetsFailed; }
};

#endif // MODEM_SATELLITE_H
