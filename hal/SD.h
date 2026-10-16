#ifndef MOCK_SD_H
#define MOCK_SD_H

#include "Arduino.h"
#include <fstream>
#include <memory>
#include <filesystem>
#include <cstdarg>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

class File {
private:
    std::string _path;
    std::shared_ptr<std::fstream> _stream;
    bool _isOpen = false;

public:
    File() = default;
    File(const std::string &path, const char *mode) : _path(path) {
        std::ios_base::openmode om = std::ios_base::in;
        if (strcmp(mode, FILE_WRITE) == 0) {
            om = std::ios_base::out | std::ios_base::trunc;
        } else if (strcmp(mode, FILE_APPEND) == 0) {
            om = std::ios_base::out | std::ios_base::app;
        }
        _stream = std::make_shared<std::fstream>(_path, om);
        _isOpen = _stream && _stream->is_open();
    }

    operator bool() const {
        return _isOpen;
    }

    size_t size() {
        if (!_isOpen) return 0;
        try {
            return (size_t)std::filesystem::file_size(_path);
        } catch (...) {
            return 0;
        }
    }

    void println(const char *str) {
        if (_isOpen) (*_stream) << str << std::endl;
    }

    void printf(const char *format, ...) {
        if (!_isOpen) return;
        char buf[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        (*_stream) << buf;
        _stream->flush();
    }

    void close() {
        if (_isOpen && _stream) {
            _stream->close();
            _isOpen = false;
        }
    }
};

class SDClass {
private:
    std::string _rootDir = "sim_sdcard";
    bool _initialized = false;

public:
    bool begin(uint8_t ssPin = 0) {
        (void)ssPin;
        std::filesystem::create_directories(_rootDir);
        _initialized = true;
        return true;
    }

    void end() {
        _initialized = false;
    }

    bool isInitialized() const {
        return _initialized;
    }

    File open(const char *path, const char *mode = FILE_READ) {
        if (!_initialized) return File();
        std::string fullPath = _rootDir;
        if (path[0] != '/' && path[0] != '\\') fullPath += "/";
        fullPath += path;
        return File(fullPath, mode);
    }
};

extern SDClass SD;

#endif // MOCK_SD_H
