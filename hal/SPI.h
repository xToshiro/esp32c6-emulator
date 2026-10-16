#ifndef MOCK_SPI_H
#define MOCK_SPI_H

#include <cstdint>

class SPIClass {
public:
    void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1) {
        (void)sck; (void)miso; (void)mosi; (void)ss;
    }
};

extern SPIClass SPI;

#endif // MOCK_SPI_H
