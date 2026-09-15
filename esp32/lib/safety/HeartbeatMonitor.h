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

    // NOTE: this monitor is deliberately source-agnostic -- reset() is
    // called both by a frame from the Pi and by a request from the
    // standalone piloting page (StandaloneControl.h), because the
    // safety question it answers ("is anyone still driving this
    // robot?") is the same either way.
    //
    // That is exactly why main.cpp tracks "when did the *Pi* last speak"
    // separately (lastPiFrameMs) rather than reading this one: deciding
    // whether to raise the standalone access point needs to distinguish
    // the two sources, and using this timestamp for it would be circular
    // -- the page keeps it fresh, so the robot would conclude the Pi was
    // back and tear the AP down under the operator's feet.

private:
    unsigned long _lastMs = 0;
    bool _started = false;
};
