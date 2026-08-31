#pragma once

#include <Arduino.h>
#include <cstdarg>
#include <string>
#include <vector>

// Test double for the Stream RoverProtocol reads/writes through. feed()
// queues bytes for available()/read() to hand back one at a time
// (mimics serial data arriving byte-by-byte); every printf() call is
// captured as one entry in `sent`, in order, for tests to assert against.
class FakeStream : public Stream {
public:
    void feed(const char* text) { _input += text; }

    int available() override { return (int)(_input.size() - _pos); }

    int read() override {
        if (_pos >= _input.size()) return -1;
        return (unsigned char)_input[_pos++];
    }

    size_t printf(const char* fmt, ...) override {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        int n = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        sent.push_back(buf);
        return n > 0 ? (size_t)n : 0;
    }

    std::vector<std::string> sent;

private:
    std::string _input;
    size_t _pos = 0;
};
