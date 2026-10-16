#ifndef MOCK_TINY_GPS_PLUS_H
#define MOCK_TINY_GPS_PLUS_H

#include <string>
#include <vector>
#include <sstream>
#include <cmath>

struct RawDegrees {
    bool negative = false;
    uint16_t deg = 0;
    uint32_t billionths = 0;
};

struct TinyGPSLocation {
    bool valid = true;
    double _lat = -3.7442;
    double _lng = -38.5748;

    bool isValid() const { return valid; }
    double lat() const { return _lat; }
    double lng() const { return _lng; }
};

struct TinyGPSDate {
    bool valid = true;
    uint16_t _year = 2026;
    uint8_t _month = 10;
    uint8_t _day = 15;

    bool isValid() const { return valid; }
    uint16_t year() const { return _year; }
    uint8_t month() const { return _month; }
    uint8_t day() const { return _day; }
};

struct TinyGPSTime {
    bool valid = true;
    uint8_t _hour = 12;
    uint8_t _minute = 30;
    uint8_t _second = 0;

    bool isValid() const { return valid; }
    uint8_t hour() const { return _hour; }
    uint8_t minute() const { return _minute; }
    uint8_t second() const { return _second; }
};

class TinyGPSPlus {
private:
    std::string _sentenceBuf;

public:
    TinyGPSLocation location;
    TinyGPSDate date;
    TinyGPSTime time;

    bool encode(char c) {
        if (c == '\r' || c == '\n') {
            if (_sentenceBuf.length() > 5) {
                parseSentence(_sentenceBuf);
                _sentenceBuf.clear();
                return true;
            }
            _sentenceBuf.clear();
            return false;
        }
        if (c == '$') {
            _sentenceBuf.clear();
        }
        _sentenceBuf += c;
        return false;
    }

private:
    void parseSentence(const std::string &s) {
        // Simple NMEA RMC parser
        if (s.rfind("$GPRMC", 0) == 0 || s.rfind("$GNRMC", 0) == 0) {
            std::stringstream ss(s);
            std::string item;
            std::vector<std::string> tokens;
            while (std::getline(ss, item, ',')) {
                tokens.push_back(item);
            }
            if (tokens.size() >= 10 && tokens[2] == "A") { // Active fix
                location.valid = true;
                date.valid = true;
                time.valid = true;

                // Time (hhmmss)
                if (tokens[1].length() >= 6) {
                    time._hour = std::stoi(tokens[1].substr(0, 2));
                    time._minute = std::stoi(tokens[1].substr(2, 2));
                    time._second = std::stoi(tokens[1].substr(4, 2));
                }

                // Latitude (ddmm.mmmm)
                if (tokens[3].length() >= 4) {
                    double raw = std::stod(tokens[3]);
                    int deg = (int)(raw / 100);
                    double min = raw - (deg * 100);
                    location._lat = deg + (min / 60.0);
                    if (tokens[4] == "S") location._lat = -location._lat;
                }

                // Longitude (dddmm.mmmm)
                if (tokens[5].length() >= 5) {
                    double raw = std::stod(tokens[5]);
                    int deg = (int)(raw / 100);
                    double min = raw - (deg * 100);
                    location._lng = deg + (min / 60.0);
                    if (tokens[6] == "W") location._lng = -location._lng;
                }

                // Date (ddmmyy)
                if (tokens[9].length() >= 6) {
                    date._day = std::stoi(tokens[9].substr(0, 2));
                    date._month = std::stoi(tokens[9].substr(2, 2));
                    date._year = 2000 + std::stoi(tokens[9].substr(4, 2));
                }
            }
        }
    }
};

#endif // MOCK_TINY_GPS_PLUS_H
