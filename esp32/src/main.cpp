#include <Arduino.h>
#include "board_config.h"
#include "RoverProtocol.h"
#include "HeartbeatMonitor.h"
#include "Watchdog.h"
#include "Diagnostics.h"

// UART port TBD once the WROOM/S3 wiring is fixed (ROVER_PROTOCOL.md
// section 2); Serial keeps this testable over USB in the meantime.
RoverProtocol protocol(Serial);
HeartbeatMonitor heartbeat;
RoverState state = RoverState::BOOT;

void onFrame(const RoverFrame& frame) {
    heartbeat.reset();
    if (state == RoverState::READY) {
        state = RoverState::ACTIVE;
    }

    if (strcmp(frame.type, "HEARTBEAT") == 0) {
        return;
    }

    if (strcmp(frame.type, "SYSTEM") == 0) {
        char action[16];
        if (!frame.getField("action", action, sizeof(action))) return;

        if (strcmp(action, "ping") == 0) {
            protocol.send("SYSTEM", "action=pong");
        } else if (strcmp(action, "resume") == 0 && state == RoverState::SAFE) {
            state = RoverState::ACTIVE;
        } else if (strcmp(action, "diag") == 0) {
            char fields[96];
            buildDiagnosticsFields(fields, sizeof(fields), state);
            protocol.send("STATE", fields);
        }
        return;
    }

    // MOVE/HEAD/FACE/ANIMATION/AUDIO/LIGHT are dispatched to their own
    // modules once those exist (Phase 2/3 of ARCHITECTURE_AND_ROADMAP.md).
}

void setup() {
    Serial.begin(115200);
    RoverWatchdog::begin();
    protocol.onFrame(onFrame);

    char fields[64];
    snprintf(fields, sizeof(fields), "protocol=%s board=%s state=BOOT",
             ROVER_PROTOCOL_VERSION, ROVER_BOARD_NAME);
    protocol.send("SYSTEM", fields);

    state = RoverState::READY;
}

void loop() {
    protocol.poll();

    if (state == RoverState::ACTIVE && heartbeat.isTimedOut()) {
        state = RoverState::SAFE;
        protocol.send("EVENT", "name=heartbeat_timeout");
    }

    RoverWatchdog::feed();
}
