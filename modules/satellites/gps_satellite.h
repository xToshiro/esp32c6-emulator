#ifndef GPS_SATELLITE_H
#define GPS_SATELLITE_H

#include "satellite_base.h"
#include "../../hal/Arduino.h"
#include <iomanip>
#include <sstream>
#include <chrono>

// Satellite GPS Module
// Simulates hardware GPS streaming NMEA sentences ($GPRMC, $GPGGA) to SerialGPS.
// Can be unplugged, powered down via PIN_GPS_POWER (Pino 6), or fault-injected at any time.
class GPSSatelliteModule : public ISatelliteModule {
private:
    double _lat = -3.7442;
    double _lon = -38.5748;
    float _speedKnots = 0.5f;
    float _courseDeg = 145.0f;
    int _satellites = 9;
    float _altitudeM = 24.5f;
    uint32_t _timerMs = 0;

    uint8_t calculateChecksum(const std::string &sentence) const {
        uint8_t crc = 0;
        for (char c : sentence) {
            crc ^= (uint8_t)c;
        }
        return crc;
    }

public:
    GPSSatelliteModule(const std::string &name = "GPS_NEO6M")
        : ISatelliteModule(name, "UART_GPS", SatelliteType::UART_GPS) {}

    void setCoordinates(double lat, double lon) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _lat = lat;
        _lon = lon;
    }

    void step(uint32_t dtMs) override {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (!_plugged || !_powered || _activeFault == "DISCONNECTED") return;

        _timerMs += dtMs;
        // Emit NMEA sentences every 1000ms (1Hz standard GPS cadence)
        if (_timerMs >= 1000) {
            _timerMs = 0;
            emitNmeaSentences();
        }
    }

    void emitNmeaSentences() {
        if (_activeFault == "GARBAGE") {
            SerialGPS.injectRxString("@@\xFF\x00$$GARBAGE_GPS_CORRUPT_BYTES\r\n");
            return;
        }

        bool hasFix = (_activeFault != "NO_FIX");

        // Format UTC time (HHMMSS.00)
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char timeBuf[16], dateBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%02d%02d%02d.00", tm.tm_hour, tm.tm_min, tm.tm_sec);
        snprintf(dateBuf, sizeof(dateBuf), "%02d%02d%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year % 100);

        // Convert lat/lon to NMEA degrees & minutes
        double absLat = std::abs(_lat);
        int latDeg = (int)absLat;
        double latMin = (absLat - latDeg) * 60.0;
        char latDir = (_lat >= 0) ? 'N' : 'S';

        double absLon = std::abs(_lon);
        int lonDeg = (int)absLon;
        double lonMin = (absLon - lonDeg) * 60.0;
        char lonDir = (_lon >= 0) ? 'E' : 'W';

        // 1. $GPRMC Sentence
        std::ostringstream rmcBody;
        rmcBody << "GPRMC," << timeBuf << ","
                << (hasFix ? "A" : "V") << ","
                << std::setfill('0') << std::setw(2) << latDeg
                << std::fixed << std::setprecision(4) << std::setw(7) << latMin << ","
                << latDir << ","
                << std::setfill('0') << std::setw(3) << lonDeg
                << std::fixed << std::setprecision(4) << std::setw(7) << lonMin << ","
                << lonDir << ","
                << std::setprecision(1) << _speedKnots << ","
                << std::setprecision(1) << _courseDeg << ","
                << dateBuf << ",,,A";

        std::string rmcStr = rmcBody.str();
        uint8_t rmcCrc = (_activeFault == "CORRUPT_CHECKSUM") ? 0x00 : calculateChecksum(rmcStr);
        char rmcHex[8];
        snprintf(rmcHex, sizeof(rmcHex), "*%02X\r\n", rmcCrc);

        // 2. $GPGGA Sentence
        std::ostringstream ggaBody;
        ggaBody << "GPGGA," << timeBuf << ","
                << std::setfill('0') << std::setw(2) << latDeg
                << std::fixed << std::setprecision(4) << std::setw(7) << latMin << ","
                << latDir << ","
                << std::setfill('0') << std::setw(3) << lonDeg
                << std::fixed << std::setprecision(4) << std::setw(7) << lonMin << ","
                << lonDir << ","
                << (hasFix ? "1" : "0") << ","
                << std::setfill('0') << std::setw(2) << (hasFix ? _satellites : 0) << ","
                << "1.0,"
                << std::setprecision(1) << _altitudeM << ",M,0.0,M,,";

        std::string ggaStr = ggaBody.str();
        uint8_t ggaCrc = (_activeFault == "CORRUPT_CHECKSUM") ? 0x00 : calculateChecksum(ggaStr);
        char ggaHex[8];
        snprintf(ggaHex, sizeof(ggaHex), "*%02X\r\n", ggaCrc);

        std::string stream = "$" + rmcStr + rmcHex + "$" + ggaStr + ggaHex;
        SerialGPS.injectRxString(stream);
    }
};

#endif // GPS_SATELLITE_H
