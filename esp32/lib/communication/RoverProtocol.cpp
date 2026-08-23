#include "RoverProtocol.h"

bool RoverFrame::hasField(const char* key) const {
    for (uint8_t i = 0; i < fieldCount; i++) {
        if (strcmp(fields[i].key, key) == 0) return true;
    }
    return false;
}

bool RoverFrame::getField(const char* key, char* out, size_t outLen) const {
    for (uint8_t i = 0; i < fieldCount; i++) {
        if (strcmp(fields[i].key, key) == 0) {
            strncpy(out, fields[i].value, outLen - 1);
            out[outLen - 1] = '\0';
            return true;
        }
    }
    return false;
}

long RoverFrame::getInt(const char* key, long defaultValue) const {
    char buf[MAX_VALUE_LEN];
    if (!getField(key, buf, sizeof(buf))) return defaultValue;
    return strtol(buf, nullptr, 10);
}

float RoverFrame::getFloat(const char* key, float defaultValue) const {
    char buf[MAX_VALUE_LEN];
    if (!getField(key, buf, sizeof(buf))) return defaultValue;
    return strtof(buf, nullptr);
}

RoverProtocol::RoverProtocol(Stream& port) : _port(port) {}

void RoverProtocol::onFrame(FrameHandler handler) {
    _handler = handler;
}

uint8_t RoverProtocol::checksum(const char* data, size_t len) {
    uint8_t cs = 0;
    for (size_t i = 0; i < len; i++) cs ^= (uint8_t)data[i];
    return cs;
}

bool RoverProtocol::hexByte(const char* twoChars, uint8_t& out) {
    uint8_t value = 0;
    for (int i = 0; i < 2; i++) {
        char c = twoChars[i];
        value <<= 4;
        if (c >= '0' && c <= '9') value |= (c - '0');
        else if (c >= 'A' && c <= 'F') value |= (c - 'A' + 10);
        else return false;
    }
    out = value;
    return true;
}

void RoverProtocol::poll() {
    while (_port.available()) {
        char c = (char)_port.read();

        if (c == '\r') continue;

        if (c == '\n') {
            if (_overflowed) {
                sendError("frame_too_long");
            } else if (_bufferLen > 0) {
                _buffer[_bufferLen] = '\0';
                handleLine(_buffer, _bufferLen);
            }
            _bufferLen = 0;
            _overflowed = false;
            continue;
        }

        if (_bufferLen >= ROVER_MAX_FRAME_LEN) {
            _overflowed = true;
            continue;
        }

        _buffer[_bufferLen++] = c;
    }
}

void RoverProtocol::handleLine(char* line, size_t len) {
    // Split "<content> *<CS>" at the last space, per ROVER_PROTOCOL.md 3.1.
    char* lastSpace = nullptr;
    for (char* p = line + len - 1; p >= line; p--) {
        if (*p == ' ') { lastSpace = p; break; }
    }

    if (!lastSpace || lastSpace[1] != '*' || strlen(lastSpace + 2) != 2) {
        sendError("checksum_invalid");
        return;
    }

    uint8_t receivedCs;
    if (!hexByte(lastSpace + 2, receivedCs)) {
        sendError("checksum_invalid");
        return;
    }

    size_t contentLen = lastSpace - line;
    if (checksum(line, contentLen) != receivedCs) {
        sendError("checksum_invalid");
        return;
    }

    line[contentLen] = '\0';

    RoverFrame frame;
    char* saveptr = nullptr;
    char* token = strtok_r(line, " ", &saveptr);
    if (!token) {
        sendError("unknown_command");
        return;
    }
    strncpy(frame.type, token, RoverFrame::MAX_TYPE_LEN - 1);
    frame.type[RoverFrame::MAX_TYPE_LEN - 1] = '\0';

    while ((token = strtok_r(nullptr, " ", &saveptr)) != nullptr &&
           frame.fieldCount < RoverFrame::MAX_FIELDS) {
        char* eq = strchr(token, '=');
        if (!eq) continue;
        *eq = '\0';
        RoverFrame::Field& f = frame.fields[frame.fieldCount];
        strncpy(f.key, token, RoverFrame::MAX_KEY_LEN - 1);
        f.key[RoverFrame::MAX_KEY_LEN - 1] = '\0';
        strncpy(f.value, eq + 1, RoverFrame::MAX_VALUE_LEN - 1);
        f.value[RoverFrame::MAX_VALUE_LEN - 1] = '\0';
        frame.fieldCount++;
    }

    if (_handler) _handler(frame);
}

void RoverProtocol::send(const char* type, const char* fields) {
    char content[ROVER_MAX_FRAME_LEN];
    if (fields && fields[0] != '\0') {
        snprintf(content, sizeof(content), "%s %s", type, fields);
    } else {
        snprintf(content, sizeof(content), "%s", type);
    }
    uint8_t cs = checksum(content, strlen(content));
    _port.printf("%s *%02X\n", content, cs);
}

void RoverProtocol::sendError(const char* code) {
    char fields[32];
    snprintf(fields, sizeof(fields), "code=%s", code);
    send("ERROR", fields);
}
