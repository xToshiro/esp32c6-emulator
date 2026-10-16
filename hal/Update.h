#ifndef MOCK_UPDATE_H
#define MOCK_UPDATE_H

#include "Arduino.h"

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

class UpdateClass {
public:
    bool begin(size_t size = UPDATE_SIZE_UNKNOWN) { (void)size; return true; }
    size_t write(uint8_t *data, size_t len) { (void)data; return len; }
    bool end(bool evenIfFail = false) { (void)evenIfFail; return true; }
    bool hasError() { return false; }
    void printError(HardwareSerial &out) { (void)out; }
};

extern UpdateClass Update;

#endif // MOCK_UPDATE_H
