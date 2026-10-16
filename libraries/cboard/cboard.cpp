#include <Wire.h>
#include <Arduino.h>
#include "cboard.h"

uint8_t gpioState = 0x00;

// ─────────────────────────────
// PCA9548A
// ─────────────────────────────
void enableChannels(uint8_t mask) {
  Wire.beginTransmission(MUX_ADDR);
  Wire.write(mask);
  Wire.endTransmission();
}

// ─────────────────────────────
// MCP23008
// ─────────────────────────────
void writeMCP(uint8_t reg, uint8_t value) {
  gpioState |= (1 << 0);
  enableChannels(gpioState);
  delay(10);

  Wire.beginTransmission(MCP_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();

  gpioState &= ~(1 << 0);
  enableChannels(gpioState);
  delay(10);
}

// ─────────────────────────────
// LIGAR / DESLIGAR SENSOR
// ─────────────────────────────

void setSensorPower(uint8_t pin, bool state) {
  if (state) {
    gpioState |= (1 << pin);
    writeMCP(OLAT_ADDR, gpioState);
    delay(1);
    if (pin) { // PINO 0 É O MCP23008, QUE NÃO VAMOS DESCONECTAR O CANAL I2C. ENTÃO O CÓDIGO SÓ RODA SE pin != 0
      enableChannels(gpioState);
    }

  } else {
    gpioState &= ~(1 << pin);
    if (pin) {
      enableChannels(gpioState);
    }
    delay(1);
    writeMCP(OLAT_ADDR, gpioState);
  }
}

// ─────────────────────────────
// SCAN I2C
// ─────────────────────────────
void scanI2C(const char* label) {
  Serial.println(label);

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
    }
  }

  Serial.println();
}
