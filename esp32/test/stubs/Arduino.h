#pragma once

// Minimal native-test stand-in for Arduino.h -- provides only what the
// pure-logic modules under test (RoverProtocol, WheelPID) actually use,
// so they can compile and run with the host's own compiler (`pio test
// -e native`) instead of needing a real ESP32. Not a general-purpose
// Arduino compatibility layer; extend only as new tests need more of it.

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

// Just enough of Arduino's Stream interface for RoverProtocol: a source
// of bytes (available/read) and a way to write formatted output
// (printf, as used by RoverProtocol::send). test/stubs/FakeStream.h
// implements this for tests.
class Stream {
public:
    virtual ~Stream() = default;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t printf(const char* fmt, ...) = 0;
};
