#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>
#include <functional>
#include <memory>

// Arduino Types
typedef uint8_t byte;
typedef bool boolean;
typedef uint16_t word;

#define HIGH 1
#define LOW  0

#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

#define HEX 16
#define DEC 10
#define OCT 8
#define BIN 2

#define SERIAL_8N1 0

// ESP32 ADC resolution
void analogReadResolution(uint8_t bits);

// Time functions
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);

// Virtual GPIO & Pin State Manager
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);

// Virtual Pin State Callback type (e.g. for Web Dashboard)
using PinChangeCallback = std::function<void(uint8_t pin, int value)>;
void registerPinChangeCallback(PinChangeCallback cb);
void setSimulatedAnalogRead(uint8_t pin, int rawValue);
void setSimulatedDigitalRead(uint8_t pin, int value);

// ESP32 String compatibility wrapper
class String : public std::string {
public:
    String() : std::string() {}
    String(const char *s) : std::string(s ? s : "") {}
    String(const std::string &s) : std::string(s) {}
    String(int val) : std::string(std::to_string(val)) {}
    String(unsigned int val) : std::string(std::to_string(val)) {}
    String(long val) : std::string(std::to_string(val)) {}
    String(unsigned long val) : std::string(std::to_string(val)) {}
    String(float val, int decimals = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        assign(buf);
    }
    String(double val, int decimals = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        assign(buf);
    }

    void trim() {
        while (!empty() && (front() == ' ' || front() == '\t' || front() == '\r' || front() == '\n')) erase(begin());
        while (!empty() && (back() == ' ' || back() == '\t' || back() == '\r' || back() == '\n')) pop_back();
    }

    bool equalsIgnoreCase(const String &other) const {
        if (size() != other.size()) return false;
        for (size_t i = 0; i < size(); ++i) {
            if (tolower((*this)[i]) != tolower(other[i])) return false;
        }
        return true;
    }

    bool startsWith(const String &prefix) const {
        if (prefix.size() > size()) return false;
        return compare(0, prefix.size(), prefix) == 0;
    }

    String substring(size_t from, size_t to = std::string::npos) const {
        if (from >= size()) return String("");
        if (to == std::string::npos) return String(substr(from));
        return String(substr(from, to - from));
    }

    int indexOf(char c, size_t fromIndex = 0) const {
        size_t pos = find(c, fromIndex);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }

    int toInt() const {
        try { return std::stoi(*this); } catch (...) { return 0; }
    }

    float toFloat() const {
        try { return std::stof(*this); } catch (...) { return 0.0f; }
    }
};

// IPAddress class mock
class IPAddress {
public:
    uint8_t bytes[4] = {0, 0, 0, 0};
    IPAddress() = default;
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        bytes[0] = a; bytes[1] = b; bytes[2] = c; bytes[3] = d;
    }
    String toString() const {
        char buf[20];
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
        return String(buf);
    }
};

// Serial & HardwareSerial
class HardwareSerial {
private:
    int _uart_nr;
    std::string _rxBuffer;
    std::mutex _mutex;
public:
    HardwareSerial(int uart_nr = 0) : _uart_nr(uart_nr) {}
    void begin(unsigned long baud, uint32_t config = 0, int rxPin = -1, int txPin = -1);
    int available();
    int read();
    String readStringUntil(char terminator);
    size_t print(const String &s);
    size_t print(const char *s);
    size_t print(int n);
    size_t print(unsigned long n);
    size_t print(double n, int decimals = 2);
    size_t println(const String &s);
    size_t println(const char *s = "");
    size_t println(int n);
    size_t println(unsigned long n);
    size_t println(double n, int decimals = 2);
    size_t write(uint8_t byte);
    size_t write(const char *str);
    int printf(const char *format, ...);

    // Injection interface for test harnesses and emulator
    void injectRxString(const std::string &data);
};

extern HardwareSerial Serial;
extern HardwareSerial SerialGPS;

// ESP singleton mock
class ESPClass {
public:
    uint32_t getFreeHeap() { return 180000; }
    void restart();
};
extern ESPClass ESP;

// RTC Data Mock
#define RTC_DATA_ATTR

#endif // MOCK_ARDUINO_H
