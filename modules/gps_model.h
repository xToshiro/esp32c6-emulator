#ifndef MODULE_GPS_MODEL_H
#define MODULE_GPS_MODEL_H

#include "../hal/Arduino.h"
#include <cstdio>
#include <cmath>
#include <string>

class GPSModel {
private:
    double _latitude = -3.7442;
    double _longitude = -38.5748;
    int _year = 2026;
    int _month = 10;
    int _day = 15;
    int _hour = 12;
    int _minute = 30;
    int _second = 0;

    static uint8_t computeNmeaChecksum(const char *sentence) {
        uint8_t crc = 0;
        // Skip leading '$'
        if (*sentence == '$') sentence++;
        while (*sentence && *sentence != '*') {
            crc ^= (uint8_t)(*sentence++);
        }
        return crc;
    }

public:
    GPSModel() = default;

    void setCoordinates(double lat, double lon) {
        _latitude = lat;
        _longitude = lon;
    }

    void setDateTime(int y, int m, int d, int h, int min, int s) {
        _year = y; _month = m; _day = d;
        _hour = h; _minute = min; _second = s;
    }

    // Generates standard RMC and GGA sentences and feeds into SerialGPS
    void injectFix() {
        // Lat format: ddmm.mmmm, N/S
        double latAbs = std::abs(_latitude);
        int latDeg = (int)latAbs;
        double latMin = (latAbs - latDeg) * 60.0;
        char latDir = (_latitude >= 0) ? 'N' : 'S';

        // Lon format: dddmm.mmmm, E/W
        double lonAbs = std::abs(_longitude);
        int lonDeg = (int)lonAbs;
        double lonMin = (lonAbs - lonDeg) * 60.0;
        char lonDir = (_longitude >= 0) ? 'E' : 'W';

        char rmcBody[128];
        snprintf(rmcBody, sizeof(rmcBody), "GPRMC,%02d%02d%02d,A,%02d%07.4f,%c,%03d%07.4f,%c,000.0,360.0,%02d%02d%02d,,,A",
                 _hour, _minute, _second,
                 latDeg, latMin, latDir,
                 lonDeg, lonMin, lonDir,
                 _day, _month, _year % 100);
        uint8_t rmcCrc = computeNmeaChecksum(rmcBody);
        char rmcFull[160];
        snprintf(rmcFull, sizeof(rmcFull), "$%s*%02X\r\n", rmcBody, rmcCrc);

        char ggaBody[128];
        snprintf(ggaBody, sizeof(ggaBody), "GPGGA,%02d%02d%02d,%02d%07.4f,%c,%03d%07.4f,%c,1,08,0.9,545.4,M,46.9,M,,",
                 _hour, _minute, _second,
                 latDeg, latMin, latDir,
                 lonDeg, lonMin, lonDir);
        uint8_t ggaCrc = computeNmeaChecksum(ggaBody);
        char ggaFull[160];
        snprintf(ggaFull, sizeof(ggaFull), "$%s*%02X\r\n", ggaBody, ggaCrc);

        SerialGPS.injectRxString(std::string(rmcFull) + std::string(ggaFull));
    }
};

#endif // MODULE_GPS_MODEL_H
