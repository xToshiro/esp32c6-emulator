#ifndef MOCK_WIFI_CLIENT_SECURE_H
#define MOCK_WIFI_CLIENT_SECURE_H

class WiFiClientSecure {
public:
    void setInsecure() {}
    void setTimeout(uint32_t timeoutMs) { (void)timeoutMs; }
};

#endif // MOCK_WIFI_CLIENT_SECURE_H
