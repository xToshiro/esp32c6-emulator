#ifndef MOCK_ESP_MDNS_H
#define MOCK_ESP_MDNS_H

#include "Arduino.h"

class MDNSResponder {
public:
    bool begin(const char *hostName) {
        (void)hostName;
        return true;
    }
    void addService(const char *service, const char *proto, uint16_t port) {
        (void)service; (void)proto; (void)port;
    }
};

extern MDNSResponder MDNS;

#endif // MOCK_ESP_MDNS_H
