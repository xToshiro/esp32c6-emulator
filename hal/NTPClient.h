#ifndef MOCK_NTP_CLIENT_H
#define MOCK_NTP_CLIENT_H

#include "Arduino.h"
#include "WiFiUdp.h"
#include <ctime>

inline void configTime(long gmtOffset_sec, int daylightOffset_sec, const char* s1, const char* s2 = nullptr, const char* s3 = nullptr) {
    (void)gmtOffset_sec; (void)daylightOffset_sec; (void)s1; (void)s2; (void)s3;
}

class NTPClient {
private:
    WiFiUDP _udp;
    String _poolServer;
    long _offset;

public:
    NTPClient(WiFiUDP &udp, const char *poolServer, long timeOffset = 0)
        : _udp(udp), _poolServer(poolServer ? poolServer : ""), _offset(timeOffset) {}

    void setPoolServerName(const char *poolServer) {
        _poolServer = poolServer ? poolServer : "";
    }

    void begin() {}
    void end() {}

    bool forceUpdate() {
        return true;
    }

    unsigned long getEpochTime() {
        time_t now = time(nullptr);
        // If system time is before 2025, provide a realistic simulated epoch (e.g. 2026-10-15)
        if (now < 1735689600) {
            now = 1792070400; // 2026 epoch
        }
        return (unsigned long)now;
    }
};

#endif // MOCK_NTP_CLIENT_H
