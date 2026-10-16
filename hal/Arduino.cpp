#include "Arduino.h"
#include <map>
#include <cstdarg>

static auto g_startTime = std::chrono::steady_clock::now();
static uint8_t g_adcBits = 12;

// Virtual pin state storage
static std::map<uint8_t, uint8_t> g_pinModes;
static std::map<uint8_t, int> g_pinDigitalValues;
static std::map<uint8_t, int> g_pinAnalogValues;
static std::vector<PinChangeCallback> g_pinCallbacks;
static std::mutex g_pinMutex;

unsigned long millis() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime).count();
}

unsigned long micros() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - g_startTime).count();
}

void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void delayMicroseconds(unsigned int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

void analogReadResolution(uint8_t bits) {
    g_adcBits = bits;
}

void pinMode(uint8_t pin, uint8_t mode) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    g_pinModes[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t val) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    g_pinDigitalValues[pin] = val;
    for (auto &cb : g_pinCallbacks) {
        if (cb) cb(pin, val);
    }
}

int digitalRead(uint8_t pin) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    auto it = g_pinDigitalValues.find(pin);
    if (it != g_pinDigitalValues.end()) return it->second;
    // Default high for pullups, low otherwise
    if (g_pinModes[pin] == INPUT_PULLUP) return HIGH;
    return LOW;
}

int analogRead(uint8_t pin) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    auto it = g_pinAnalogValues.find(pin);
    if (it != g_pinAnalogValues.end()) {
        int maxVal = (1 << g_adcBits) - 1;
        return std::clamp(it->second, 0, maxVal);
    }
    return 0;
}

void registerPinChangeCallback(PinChangeCallback cb) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    g_pinCallbacks.push_back(cb);
}

void setSimulatedAnalogRead(uint8_t pin, int rawValue) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    g_pinAnalogValues[pin] = rawValue;
}

void setSimulatedDigitalRead(uint8_t pin, int value) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    g_pinDigitalValues[pin] = value;
}

void HardwareSerial::begin(unsigned long baud, uint32_t config, int rxPin, int txPin) {
    (void)baud; (void)config; (void)rxPin; (void)txPin;
}

int HardwareSerial::available() {
    std::lock_guard<std::mutex> lock(_mutex);
    return (int)_rxBuffer.size();
}

int HardwareSerial::read() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_rxBuffer.empty()) return -1;
    char c = _rxBuffer.front();
    _rxBuffer.erase(_rxBuffer.begin());
    return (int)(unsigned char)c;
}

String HardwareSerial::readStringUntil(char terminator) {
    std::lock_guard<std::mutex> lock(_mutex);
    size_t pos = _rxBuffer.find(terminator);
    if (pos == std::string::npos) {
        String res = _rxBuffer;
        _rxBuffer.clear();
        return res;
    }
    String res = _rxBuffer.substr(0, pos);
    _rxBuffer.erase(0, pos + 1);
    return res;
}

size_t HardwareSerial::print(const String &s) {
    std::cout << s;
    return s.size();
}

size_t HardwareSerial::print(const char *s) {
    if (!s) return 0;
    std::cout << s;
    return strlen(s);
}

size_t HardwareSerial::print(int n) {
    std::cout << n;
    return 1;
}

size_t HardwareSerial::print(unsigned long n) {
    std::cout << n;
    return 1;
}

size_t HardwareSerial::print(double n, int decimals) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, n);
    std::cout << buf;
    return strlen(buf);
}

size_t HardwareSerial::println(const String &s) {
    std::cout << s << std::endl;
    return s.size() + 1;
}

size_t HardwareSerial::println(const char *s) {
    if (!s) { std::cout << std::endl; return 1; }
    std::cout << s << std::endl;
    return strlen(s) + 1;
}

size_t HardwareSerial::println(int n) {
    std::cout << n << std::endl;
    return 1;
}

size_t HardwareSerial::println(unsigned long n) {
    std::cout << n << std::endl;
    return 1;
}

size_t HardwareSerial::println(double n, int decimals) {
    print(n, decimals);
    std::cout << std::endl;
    return 1;
}

size_t HardwareSerial::write(uint8_t byte) {
    std::cout.put((char)byte);
    return 1;
}

size_t HardwareSerial::write(const char *str) {
    if (!str) return 0;
    std::cout << str;
    return strlen(str);
}

int HardwareSerial::printf(const char *format, ...) {
    char buf[1024];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    std::cout << buf;
    return written;
}

void HardwareSerial::injectRxString(const std::string &data) {
    std::lock_guard<std::mutex> lock(_mutex);
    _rxBuffer.append(data);
}

void ESPClass::restart() {
    std::cout << "[MOCK ESP] ESP.restart() chamado! Reiniciando ciclo de simulacao..." << std::endl;
}

HardwareSerial Serial(0);
HardwareSerial SerialGPS(1);
ESPClass ESP;
