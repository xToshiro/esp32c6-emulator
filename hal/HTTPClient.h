#ifndef MOCK_HTTP_CLIENT_H
#define MOCK_HTTP_CLIENT_H

#include "Arduino.h"
#include "WiFiClientSecure.h"
#include <map>
#include <vector>
#include <functional>

struct HttpCapturedRequest {
    String url;
    std::map<String, String> headers;
    String body;
    int responseCode;
    uint32_t timestamp;
};

class HTTPClient {
private:
    String _url;
    std::map<String, String> _headers;
    uint32_t _timeout = 5000;
    static std::vector<HttpCapturedRequest> _capturedRequests;
    static int _simulatedResponseCode;
    static std::function<void(const HttpCapturedRequest&)> _onCapturedCallback;

public:
    HTTPClient() = default;

    bool begin(const String &url) {
        _url = url;
        _headers.clear();
        return true;
    }

    bool begin(WiFiClientSecure &client, const String &url) {
        (void)client;
        _url = url;
        _headers.clear();
        return true;
    }

    void addHeader(const String &name, const String &value) {
        _headers[name] = value;
    }

    void setTimeout(uint32_t timeoutMs) {
        _timeout = timeoutMs;
    }

    int POST(const String &payload) {
        int code = _simulatedResponseCode;
        HttpCapturedRequest req{_url, _headers, payload, code, millis()};
        _capturedRequests.push_back(req);
        if (_onCapturedCallback) _onCapturedCallback(req);
        return code;
    }

    int GET() {
        int code = _simulatedResponseCode;
        HttpCapturedRequest req{_url, _headers, "", code, millis()};
        _capturedRequests.push_back(req);
        if (_onCapturedCallback) _onCapturedCallback(req);
        return code;
    }

    void end() {}

    String errorToString(int error) {
        if (error == -1) return "CONNECTION_REFUSED";
        if (error == -11) return "READ_TIMEOUT";
        return "GENERIC_HTTP_ERROR";
    }

    // Emulator inspection & scenario control
    static void setSimulatedResponseCode(int code) { _simulatedResponseCode = code; }
    static const std::vector<HttpCapturedRequest>& getCapturedRequests() { return _capturedRequests; }
    static void clearCapturedRequests() { _capturedRequests.clear(); }
    static void setOnCapturedCallback(std::function<void(const HttpCapturedRequest&)> cb) {
        _onCapturedCallback = cb;
    }
};

#endif // MOCK_HTTP_CLIENT_H
