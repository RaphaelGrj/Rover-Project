#pragma once

#include <Arduino.h>
#include <functional>
#include "board_config.h"

// Wire format: see ROVER_PROTOCOL.md section 3.
class RoverFrame {
public:
    static constexpr uint8_t MAX_FIELDS = 8;
    static constexpr uint8_t MAX_TYPE_LEN = 16;
    static constexpr uint8_t MAX_KEY_LEN = 16;
    static constexpr uint8_t MAX_VALUE_LEN = 48;

    char type[MAX_TYPE_LEN] = {0};
    uint8_t fieldCount = 0;

    bool getField(const char* key, char* out, size_t outLen) const;
    long getInt(const char* key, long defaultValue = 0) const;
    float getFloat(const char* key, float defaultValue = 0.0f) const;
    bool hasField(const char* key) const;

    struct Field {
        char key[MAX_KEY_LEN];
        char value[MAX_VALUE_LEN];
    };
    Field fields[MAX_FIELDS];
};

class RoverProtocol {
public:
    using FrameHandler = std::function<void(const RoverFrame&)>;

    explicit RoverProtocol(Stream& port);

    // Call every loop() iteration; non-blocking.
    void poll();

    void onFrame(FrameHandler handler);

    // fields must already be formatted as "key=value key=value", or empty.
    void send(const char* type, const char* fields = "");
    void sendError(const char* code);

private:
    Stream& _port;
    char _buffer[ROVER_MAX_FRAME_LEN + 1];
    size_t _bufferLen = 0;
    bool _overflowed = false;
    FrameHandler _handler;

    void handleLine(char* line, size_t len);
    static uint8_t checksum(const char* data, size_t len);
    static bool hexByte(const char* twoChars, uint8_t& out);
};
