#include "Adafruit_NeoPixel.h"

std::function<void(uint8_t r, uint8_t g, uint8_t b)> Adafruit_NeoPixel::_onColorChangedCallback = nullptr;
