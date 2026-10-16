#ifndef MOCK_BLE_DEVICE_H
#define MOCK_BLE_DEVICE_H

#include "Arduino.h"

class BLEDescriptor {};
class BLE2902 : public BLEDescriptor {};

class BLECharacteristic {
public:
    void setValue(const String &val) { (void)val; }
    void setValue(uint8_t *data, size_t size) { (void)data; (void)size; }
    void notify() {}
    void addDescriptor(BLEDescriptor *pDesc) { (void)pDesc; }
};

class BLEService {
public:
    BLECharacteristic* createCharacteristic(const char *uuid, uint32_t properties) {
        (void)uuid; (void)properties;
        return new BLECharacteristic();
    }
    void start() {}
};

class BLEServerCallbacks {};

class BLEServer {
public:
    void setCallbacks(BLEServerCallbacks *cb) { (void)cb; }
    BLEService* createService(const char *uuid) {
        (void)uuid;
        return new BLEService();
    }
};

class BLEAdvertising {
public:
    void addServiceUUID(const char *uuid) { (void)uuid; }
    void setScanResponse(bool set) { (void)set; }
    void setMinPreferred(uint8_t min) { (void)min; }
    void setMaxPreferred(uint8_t max) { (void)max; }
    void start() {}
};

class BLEDevice {
public:
    static void init(const String &name) { (void)name; }
    static BLEServer* createServer() { return new BLEServer(); }
    static BLEAdvertising* getAdvertising() { return new BLEAdvertising(); }
    static void startAdvertising() {}
};

#endif // MOCK_BLE_DEVICE_H
