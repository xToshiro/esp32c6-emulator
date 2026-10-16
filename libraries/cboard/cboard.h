#ifndef CBOARD_H
#define CBOARD_H

// ─────────────────────────────
// CONSTANTS AND GLOBAL VARIABLES
// ─────────────────────────────
#define MUX_ADDR 0x70
#define MCP_ADDR 0x20
#define IODIR_ADDR 0x00
#define OLAT_ADDR  0x0A
#define MULTICHANNEL_ADDR 0x08
#define GPIO1 1
#define GPIO2 2
#define GPIO3 3
#define GPIO4 4
#define GPIO5 5
#define ON true
#define OFF false

extern uint8_t gpioState;

// ─────────────────────────────
// FUNCTIONS
// ─────────────────────────────

void enableChannels(uint8_t mask);
void writeMCP(uint8_t reg, uint8_t value);
void setSensorPower(uint8_t pin, bool state);
void scanI2C(const char* label);

#endif
