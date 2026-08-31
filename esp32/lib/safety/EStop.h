#pragma once

#include <Arduino.h>
#include "safety_config.h"

// Physical emergency-stop button -- a HARDWARE-level safety layer,
// independent of and in addition to the heartbeat timeout
// (ARCHITECTURE_AND_ROADMAP.md section 9): it must stop the robot even
// if the Pi link is perfectly healthy and actively sending MOVE.
//
// INPUT_PULLUP means an unwired pin reads a stable "released" -- this
// class is always active from boot, whether or not a physical button
// exists yet, and never blocks normal operation while unwired.
class EStop {
public:
    void begin() {
        pinMode(ROVER_PIN_ESTOP, INPUT_PULLUP);
        _rawPressed = readRaw();
        _stablePressed = _rawPressed;
        _lastChangeMs = millis();
    }

    // Non-blocking, call every loop() iteration.
    void update() {
        bool raw = readRaw();
        unsigned long now = millis();
        if (raw != _rawPressed) {
            _rawPressed = raw;
            _lastChangeMs = now;
        }
        if (_rawPressed != _stablePressed && now - _lastChangeMs >= ROVER_ESTOP_DEBOUNCE_MS) {
            _stablePressed = _rawPressed;
        }
    }

    // Debounced state: true while the button is physically held down.
    bool isPressed() const { return _stablePressed; }

private:
    bool readRaw() const { return digitalRead(ROVER_PIN_ESTOP) == LOW; }

    bool _rawPressed = false;
    bool _stablePressed = false;
    unsigned long _lastChangeMs = 0;
};
