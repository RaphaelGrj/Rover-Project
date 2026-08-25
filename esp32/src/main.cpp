#include <Arduino.h>
#include "board_config.h"
#include "motion_config.h"
#include "RoverProtocol.h"
#include "HeartbeatMonitor.h"
#include "Watchdog.h"
#include "Diagnostics.h"
#include "DriveController.h"

// UART port TBD once the WROOM/S3 wiring is fixed (ROVER_PROTOCOL.md
// section 2); Serial keeps this testable over USB in the meantime.
RoverProtocol protocol(Serial);
HeartbeatMonitor heartbeat;
RoverState state = RoverState::BOOT;
DriveController drive;
unsigned long lastTelemetryMs = 0;

// Called by RoverProtocol for every validated incoming frame.
void onFrame(const RoverFrame& frame) {
    // Any valid frame counts as proof of life from the Pi.
    heartbeat.reset();
    // First frame after boot promotes us out of READY; SAFE can only be
    // left via an explicit SYSTEM action=resume (see below), not just
    // because traffic resumed -- avoids silently un-safing the robot.
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

    if (strcmp(frame.type, "MOVE") == 0) {
        // Only honored while ACTIVE: a MOVE arriving during SAFE must
        // never re-arm the motors (ARCHITECTURE_AND_ROADMAP.md section
        // 27 rule 6, "préserver le fonctionnement du mode SAFE") --
        // only an explicit SYSTEM action=resume can do that.
        if (state == RoverState::ACTIVE) {
            float velocity = frame.getFloat("velocity", 0.0f);
            float rotation = frame.getFloat("rotation", 0.0f);
            drive.setTarget(velocity, rotation);
        }
        return;
    }

    // HEAD/FACE/ANIMATION/AUDIO/LIGHT are dispatched to their own modules
    // once those exist (Phase 3 of ARCHITECTURE_AND_ROADMAP.md).
}

void setup() {
    Serial.begin(115200);
    RoverWatchdog::begin();
    protocol.onFrame(onFrame);
    // Attaches motor/encoder pins and immediately commands 0 speed, so
    // the driver never has stale/undefined PWM before the first MOVE.
    drive.begin();

    char fields[64];
    snprintf(fields, sizeof(fields), "protocol=%s board=%s state=BOOT",
             ROVER_PROTOCOL_VERSION, ROVER_BOARD_NAME);
    protocol.send("SYSTEM", fields);

    state = RoverState::READY;
}

void loop() {
    protocol.poll();
    drive.update();

    if (state == RoverState::ACTIVE && heartbeat.isTimedOut()) {
        state = RoverState::SAFE;
        drive.stop();
        protocol.send("EVENT", "name=heartbeat_timeout");
    }

    // Periodic wheel-speed telemetry while ACTIVE (ROVER_PROTOCOL.md §8,
    // "STATE left_speed=... right_speed=...").
    if (state == RoverState::ACTIVE) {
        unsigned long now = millis();
        if (now - lastTelemetryMs >= ROVER_DRIVE_TELEMETRY_PERIOD_MS) {
            lastTelemetryMs = now;
            char fields[64];
            drive.buildTelemetryFields(fields, sizeof(fields));
            protocol.send("STATE", fields);
        }
    }

    RoverWatchdog::feed();
}
