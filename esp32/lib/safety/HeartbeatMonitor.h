#pragma once

#include <Arduino.h>
#include "board_config.h"

// See ROVER_PROTOCOL.md section 6: SAFE state must not trigger before
// the first HEARTBEAT has ever been received (still BOOT/READY).
class HeartbeatMonitor {
public:
    void reset() {
        _lastMs = millis();
        _started = true;
    }

    bool isTimedOut() const {
        return _started && (millis() - _lastMs > ROVER_HEARTBEAT_TIMEOUT_MS);
    }

private:
    unsigned long _lastMs = 0;
    bool _started = false;
};
