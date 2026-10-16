#ifndef MOCK_WEB_SERVER_H
#define MOCK_WEB_SERVER_H

#include "Arduino.h"
#include <map>
#include <functional>

#define HTTP_GET  1
#define HTTP_POST 2

struct HTTPUpload {
    int status;
    String filename;
    String name;
    String type;
    size_t totalSize;
    size_t currentSize;
    uint8_t buf[1024];
};

enum HTTPUploadStatus {
    UPLOAD_FILE_START,
    UPLOAD_FILE_WRITE,
    UPLOAD_FILE_END,
    UPLOAD_FILE_ABORTED
};

class WebServer {
public:
    using HandlerFunction = std::function<void()>;

private:
    int _port;
    std::map<String, HandlerFunction> _routes;
    HandlerFunction _notFoundHandler;
    std::map<String, String> _args;
    HTTPUpload _currentUpload;

public:
    WebServer(int port = 80) : _port(port) {}

    void on(const String &uri, HandlerFunction handler) {
        _routes[uri] = handler;
    }

    void on(const String &uri, int method, HandlerFunction handler) {
        (void)method;
        _routes[uri] = handler;
    }

    void on(const String &uri, int method, HandlerFunction handler, HandlerFunction uploadHandler) {
        (void)method; (void)uploadHandler;
        _routes[uri] = handler;
    }

    void onNotFound(HandlerFunction handler) {
        _notFoundHandler = handler;
    }

    void begin() {}

    void handleClient() {}

    void send(int code, const char *contentType, const String &content) {
        (void)code; (void)contentType; (void)content;
    }

    void sendHeader(const String &name, const String &value, bool first = false) {
        (void)name; (void)value; (void)first;
    }

    bool hasArg(const String &name) {
        return _args.find(name) != _args.end();
    }

    String arg(const String &name) {
        auto it = _args.find(name);
        if (it != _args.end()) return it->second;
        return String();
    }

    HTTPUpload& upload() {
        return _currentUpload;
    }

    // Emulator injection to simulate web requests
    void simulateRequest(const String &uri, const std::map<String, String> &params = {}) {
        _args = params;
        auto it = _routes.find(uri);
        if (it != _routes.end()) {
            it->second();
        } else if (_notFoundHandler) {
            _notFoundHandler();
        }
    }
};

#endif // MOCK_WEB_SERVER_H
