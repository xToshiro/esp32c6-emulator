#ifndef SDCARD_SATELLITE_H
#define SDCARD_SATELLITE_H

#include "satellite_base.h"
#include "../../hal/SD.h"
#include <iostream>

// Satellite SD Card Module
// Simulates SPI microSD card slot (CS Pin 7) and filesystem operations (/datalog.csv).
// Can be ejected/unplugged or fault-injected at any moment during execution.
class SDCardSatelliteModule : public ISatelliteModule {
private:
    uint8_t _csPin = 7;
    size_t _bytesWritten = 0;
    size_t _recordsCount = 0;

public:
    SDCardSatelliteModule(uint8_t csPin = 7, const std::string &name = "SD_CARD_MICROSD")
        : ISatelliteModule(name, "SPI_SD", SatelliteType::STORAGE_SPI), _csPin(csPin) {}

    void setPlugged(bool plugged) override {
        ISatelliteModule::setPlugged(plugged);
        if (!plugged) {
            SD.end();
        } else {
            SD.begin(_csPin);
        }
    }

    void step(uint32_t dtMs) override {
        (void)dtMs;
    }

    bool isCardInserted() const {
        return isOperational() && (_activeFault != "EJECTED") && (_activeFault != "IO_ERROR");
    }

    size_t getBytesWritten() const { return _bytesWritten; }
    size_t getRecordsCount() const { return _recordsCount; }

    void recordWriteEvent(size_t bytes) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _bytesWritten += bytes;
        _recordsCount++;
    }
};

#endif // SDCARD_SATELLITE_H
