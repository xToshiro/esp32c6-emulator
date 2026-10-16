#ifndef MOCK_WIFI_H
#define MOCK_WIFI_H

#include "Arduino.h"
#include <vector>
#include <string>
#include <mutex>

#define WIFI_OFF 0
#define WIFI_STA 1
#define WIFI_AP  2
#define WIFI_AP_STA 3

#define WL_NO_SHIELD        255
#define WL_IDLE_STATUS      0
#define WL_NO_SSID_AVAIL    1
#define WL_SCAN_COMPLETED   2
#define WL_CONNECTED        3
#define WL_CONNECT_FAILED   4
#define WL_CONNECTION_LOST  5
#define WL_DISCONNECTED     6

struct MockScannedAP {
    String ssid;
    int rssi;
    String bssidStr;
    uint8_t bssid[6];
};

class WiFiClass {
private:
    int _mode = WIFI_STA;
    int _status = WL_DISCONNECTED;
    String _ssid;
    IPAddress _ip = IPAddress(192, 168, 1, 100);
    IPAddress _gateway = IPAddress(192, 168, 1, 1);
    IPAddress _subnet = IPAddress(255, 255, 255, 0);
    std::vector<MockScannedAP> _scannedAPs;
    std::mutex _mtx;

public:
    WiFiClass() {
        // Pre-populate simulated APs matching the firmware
        _scannedAPs.push_back({"MoQa_Wifi_Infra", -58, "AA:BB:CC:11:22:33", {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33}});
        _scannedAPs.push_back({"MoQa_Wifi_Infra", -72, "AA:BB:CC:11:22:44", {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x44}});
        _scannedAPs.push_back({"Backup_Lab_AP", -85, "11:22:33:44:55:66", {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}});
    }

    void mode(int m) {
        std::lock_guard<std::mutex> lock(_mtx);
        _mode = m;
        if (m == WIFI_AP) {
            _ip = IPAddress(192, 168, 4, 1);
            _status = WL_CONNECTED;
        }
    }

    void disconnect(bool wifioff = false) {
        std::lock_guard<std::mutex> lock(_mtx);
        (void)wifioff;
        _status = WL_DISCONNECTED;
    }

    void begin(const char *ssid, const char *pass = nullptr, int channel = 0, const uint8_t *bssid = nullptr) {
        std::lock_guard<std::mutex> lock(_mtx);
        (void)pass; (void)channel; (void)bssid;
        _ssid = ssid ? ssid : "";
        _status = WL_CONNECTED;
        _ip = IPAddress(192, 168, 1, 105);
    }

    int status() {
        std::lock_guard<std::mutex> lock(_mtx);
        return _status;
    }

    void setSleep(bool enable) { (void)enable; }

    bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = IPAddress(), IPAddress dns2 = IPAddress()) {
        std::lock_guard<std::mutex> lock(_mtx);
        _ip = local_ip;
        _gateway = gateway;
        _subnet = subnet;
        (void)dns1; (void)dns2;
        return true;
    }

    IPAddress localIP() { std::lock_guard<std::mutex> lock(_mtx); return _ip; }
    IPAddress gatewayIP() { std::lock_guard<std::mutex> lock(_mtx); return _gateway; }
    IPAddress subnetMask() { std::lock_guard<std::mutex> lock(_mtx); return _subnet; }
    int RSSI() { return -62; }

    int scanNetworks() {
        std::lock_guard<std::mutex> lock(_mtx);
        return (int)_scannedAPs.size();
    }

    String SSID(int i) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (i >= 0 && i < (int)_scannedAPs.size()) return _scannedAPs[i].ssid;
        return String();
    }

    int RSSI(int i) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (i >= 0 && i < (int)_scannedAPs.size()) return _scannedAPs[i].rssi;
        return -100;
    }

    String BSSIDstr(int i) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (i >= 0 && i < (int)_scannedAPs.size()) return _scannedAPs[i].bssidStr;
        return String();
    }

    const uint8_t* BSSID(int i) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (i >= 0 && i < (int)_scannedAPs.size()) return _scannedAPs[i].bssid;
        return nullptr;
    }

    bool softAP(const char *ssid, const char *pass = nullptr) {
        std::lock_guard<std::mutex> lock(_mtx);
        (void)pass;
        _ssid = ssid ? ssid : "";
        _mode = WIFI_AP;
        _status = WL_CONNECTED;
        _ip = IPAddress(192, 168, 4, 1);
        return true;
    }

    void macAddress(uint8_t *mac) {
        if (!mac) return;
        mac[0] = 0x68; mac[1] = 0xB6; mac[2] = 0xB3;
        mac[3] = 0x2A; mac[4] = 0x4C; mac[5] = 0x8E;
    }

    // Emulator injection hooks
    void setSimulatedConnected(bool conn) {
        std::lock_guard<std::mutex> lock(_mtx);
        _status = conn ? WL_CONNECTED : WL_DISCONNECTED;
    }
};

extern WiFiClass WiFi;

#endif // MOCK_WIFI_H
