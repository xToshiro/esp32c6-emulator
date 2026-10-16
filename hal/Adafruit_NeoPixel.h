#ifndef MOCK_ADAFRUIT_NEOPIXEL_H
#define MOCK_ADAFRUIT_NEOPIXEL_H

#include "Arduino.h"
#include <cstdint>
#include <functional>

#define NEO_GRB 0
#define NEO_KHZ800 0

struct NeoPixelColor {
    uint8_t r = 0, g = 0, b = 0;
};

class Adafruit_NeoPixel {
private:
    uint16_t _numPixels;
    uint8_t _pin;
    NeoPixelColor _currentColor;
    static std::function<void(uint8_t r, uint8_t g, uint8_t b)> _onColorChangedCallback;

public:
    Adafruit_NeoPixel(uint16_t n, uint8_t p, uint32_t t)
        : _numPixels(n), _pin(p) { (void)t; }

    void begin() {}

    uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    void setPixelColor(uint16_t n, uint32_t c) {
        (void)n;
        _currentColor.r = (c >> 16) & 0xFF;
        _currentColor.g = (c >> 8) & 0xFF;
        _currentColor.b = c & 0xFF;
    }

    void show() {
        if (_onColorChangedCallback) {
            _onColorChangedCallback(_currentColor.r, _currentColor.g, _currentColor.b);
        }
    }

    NeoPixelColor getCurrentColor() const { return _currentColor; }

    static void setOnColorChangedCallback(std::function<void(uint8_t r, uint8_t g, uint8_t b)> cb) {
        _onColorChangedCallback = cb;
    }
};

#endif // MOCK_ADAFRUIT_NEOPIXEL_H
