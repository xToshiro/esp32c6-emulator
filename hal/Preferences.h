#ifndef MOCK_PREFERENCES_H
#define MOCK_PREFERENCES_H

#include "Arduino.h"
#include <map>
#include <string>
#include <mutex>
#include <fstream>

class Preferences {
private:
    std::string _namespace;
    bool _readOnly = false;
    static std::map<std::string, std::map<std::string, std::string>> _storage;
    static std::mutex _mtx;

public:
    Preferences() = default;

    bool begin(const char *name, bool readOnly = false) {
        std::lock_guard<std::mutex> lock(_mtx);
        _namespace = name ? name : "default";
        _readOnly = readOnly;
        return true;
    }

    void end() {
        // flush or finish
    }

    bool clear() {
        std::lock_guard<std::mutex> lock(_mtx);
        _storage[_namespace].clear();
        return true;
    }

    bool remove(const char *key) {
        std::lock_guard<std::mutex> lock(_mtx);
        _storage[_namespace].erase(key);
        return true;
    }

    // Bool
    size_t putBool(const char *key, bool value) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_readOnly) return 0;
        _storage[_namespace][key] = value ? "1" : "0";
        return 1;
    }

    bool getBool(const char *key, bool defaultValue = false) {
        std::lock_guard<std::mutex> lock(_mtx);
        auto it = _storage[_namespace].find(key);
        if (it != _storage[_namespace].end()) {
            return it->second == "1";
        }
        return defaultValue;
    }

    // UInt
    size_t putUInt(const char *key, uint32_t value) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_readOnly) return 0;
        _storage[_namespace][key] = std::to_string(value);
        return 4;
    }

    uint32_t getUInt(const char *key, uint32_t defaultValue = 0) {
        std::lock_guard<std::mutex> lock(_mtx);
        auto it = _storage[_namespace].find(key);
        if (it != _storage[_namespace].end()) {
            try { return (uint32_t)std::stoul(it->second); } catch (...) {}
        }
        return defaultValue;
    }

    // Float
    size_t putFloat(const char *key, float value) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_readOnly) return 0;
        _storage[_namespace][key] = std::to_string(value);
        return 4;
    }

    float getFloat(const char *key, float defaultValue = 0.0f) {
        std::lock_guard<std::mutex> lock(_mtx);
        auto it = _storage[_namespace].find(key);
        if (it != _storage[_namespace].end()) {
            try { return std::stof(it->second); } catch (...) {}
        }
        return defaultValue;
    }

    // String
    size_t putString(const char *key, const String &value) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_readOnly) return 0;
        _storage[_namespace][key] = value.c_str();
        return value.length();
    }

    String getString(const char *key, const String &defaultValue = String()) {
        std::lock_guard<std::mutex> lock(_mtx);
        auto it = _storage[_namespace].find(key);
        if (it != _storage[_namespace].end()) {
            return String(it->second.c_str());
        }
        return defaultValue;
    }
};

#endif // MOCK_PREFERENCES_H
