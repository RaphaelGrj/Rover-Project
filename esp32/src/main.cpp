#include <Arduino.h>
#include "board_config.h"
#include "motion_config.h"
#include "sensors_config.h"
#include "RoverProtocol.h"
#include "HeartbeatMonitor.h"
#include "Watchdog.h"
#include "Diagnostics.h"
#include "DriveController.h"
#include "HeadController.h"
#include "DisplayEngine.h"
#include "Emotion.h"
#include "SensorHub.h"
#include "EStop.h"
#include "BatteryMonitor.h"
#include "RoverOTA.h"

// UART port TBD once the WROOM/S3 wiring is fixed (ROVER_PROTOCOL.md
// section 2); Serial keeps this testable over USB in the meantime.
RoverProtocol protocol(Serial);
HeartbeatMonitor heartbeat;
RoverState state = RoverState::BOOT;
DriveController drive;
HeadController head;
DisplayEngine display;
SensorHub sensors;
EStop estop;
BatteryMonitor battery;
RoverOTA ota;
unsigned long lastTelemetryMs = 0;
unsigned long lastSensorTelemetryMs = 0;
unsigned long lastBatteryTelemetryMs = 0;

// Called by RoverProtocol for every validated incoming frame.
void onFrame(const RoverFrame& frame) {
    // Any valid frame counts as proof of life from the Pi.
    heartbeat.reset();
    // First frame after boot promotes us out of READY; SAFE can only be
    // left via an explicit SYSTEM action=resume (see below), not just
    // because traffic resumed -- avoids silently un-safing the robot.
    // Guarded on the E-stop too: if the button is already held down at
    // boot, the very first frame must not promote straight to ACTIVE
    // just because loop()'s own estop check hasn't run yet this cycle.
    if (state == RoverState::READY && !estop.isPressed()) {
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
        } else if (strcmp(action, "resume") == 0 && state == RoverState::SAFE && !estop.isPressed()) {
            // A resume must never override a physically-held E-stop --
            // that would defeat the entire point of a hardware safety
            // layer. Only the heartbeat-timeout SAFE can be resumed this
            // way; releasing the button is necessary but not itself
            // sufficient (still requires this same explicit resume).
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

    if (strcmp(frame.type, "HEAD") == 0) {
        // Same gating as MOVE, for the same reason: never obey a stale
        // command during SAFE, only an explicit resume re-arms it.
        if (state == RoverState::ACTIVE) {
            float pitch = frame.getFloat("pitch", 0.0f);
            float yaw = frame.getFloat("yaw", 0.0f);
            head.setTarget(pitch, yaw);
        }
        return;
    }

    if (strcmp(frame.type, "FACE") == 0) {
        // Not gated on ACTIVE/SAFE: showing an emotion has no physical
        // safety implication the way motor/servo motion does.
        char emotionName[16];
        Emotion emotion;
        if (frame.getField("emotion", emotionName, sizeof(emotionName)) &&
            parseEmotion(emotionName, emotion)) {
            display.setEmotion(emotion);
        }
        return;
    }

    if (strcmp(frame.type, "ANIMATION") == 0) {
        char animationName[16];
        if (frame.getField("name", animationName, sizeof(animationName))) {
            display.playAnimation(animationName);
        }
        return;
    }

    // AUDIO/LIGHT are dispatched to their own modules once those exist.
}

void setup() {
    Serial.begin(115200);
    RoverWatchdog::begin();
    protocol.onFrame(onFrame);
    // Attaches motor/encoder pins and immediately commands 0 speed, so
    // the driver never has stale/undefined PWM before the first MOVE.
    drive.begin();
    head.begin();
    display.begin();
    sensors.begin();
    estop.begin();
    battery.begin();
    // Entirely opt-in (see RoverOTA.h) -- a no-op that returns
    // immediately unless WiFi/OTA credentials were set at build time.
    ota.begin();

    char fields[64];
    snprintf(fields, sizeof(fields), "protocol=%s board=%s state=BOOT",
             ROVER_PROTOCOL_VERSION, ROVER_BOARD_NAME);
    protocol.send("SYSTEM", fields);

    // Report anything that failed to initialize right away (eg. a
    // sensor not wired yet) instead of waiting for loop()'s first pass.
    const char* failedSensor;
    while (sensors.consumeSensorFailure(&failedSensor)) {
        // 48, not 32: "code=sensor_timeout sensor=tof_right" alone is
        // 37 bytes with the terminator -- a first hardware test caught
        // this being silently truncated (mid-word) by too small a buffer.
        char errFields[48];
        snprintf(errFields, sizeof(errFields), "code=sensor_timeout sensor=%s", failedSensor);
        protocol.send("ERROR", errFields);
    }

    state = RoverState::READY;
}

void loop() {
    protocol.poll();
    drive.update();
    head.update();
    display.update();
    sensors.update();
    estop.update();
    battery.update();
    ota.update();

    if (state == RoverState::ACTIVE && heartbeat.isTimedOut()) {
        state = RoverState::SAFE;
        drive.stop();
        protocol.send("EVENT", "name=heartbeat_timeout");
    }

    // Hardware E-stop takes priority over everything else and is
    // checked independently of the heartbeat timeout above -- it must
    // stop the robot even while the Pi link is perfectly healthy.
    static bool wasEstopPressed = false;
    if (estop.isPressed() && !wasEstopPressed) {
        state = RoverState::SAFE;
        drive.stop();
        protocol.send("EVENT", "name=estop_pressed");
    }
    wasEstopPressed = estop.isPressed();

    if (battery.hasReading()) {
        if (battery.consumeLowBatteryEvent()) {
            protocol.send("EVENT", "name=low_battery");
        }
        unsigned long batteryNow = millis();
        if (batteryNow - lastBatteryTelemetryMs >= ROVER_BATTERY_UPDATE_PERIOD_MS) {
            lastBatteryTelemetryMs = batteryNow;
            char batteryFields[16];
            battery.buildTelemetryFields(batteryFields, sizeof(batteryFields));
            protocol.send("STATE", batteryFields);
        }
    }

    // Sensor EVENT/ERROR/STATE are never gated on ACTIVE/SAFE: unlike
    // MOVE/HEAD (physical actuation, must stay off outside ACTIVE),
    // situational awareness (obstacles, sensor health) stays useful to
    // the Pi even while the robot is stopped.
    const char* failedSensor;
    while (sensors.consumeSensorFailure(&failedSensor)) {
        // 48, not 32: "code=sensor_timeout sensor=tof_right" alone is
        // 37 bytes with the terminator -- a first hardware test caught
        // this being silently truncated (mid-word) by too small a buffer.
        char errFields[48];
        snprintf(errFields, sizeof(errFields), "code=sensor_timeout sensor=%s", failedSensor);
        protocol.send("ERROR", errFields);
    }
    if (sensors.consumeObstacleEvent()) {
        protocol.send("EVENT", "name=obstacle_detected");
    }

    unsigned long sensorNow = millis();
    if (sensorNow - lastSensorTelemetryMs >= ROVER_SENSOR_TELEMETRY_PERIOD_MS) {
        lastSensorTelemetryMs = sensorNow;
        // 96, not 64: the IMU's six fields alone need ~75 bytes even
        // with every value at "0.00" -- another truncation caught on
        // the same first hardware test as the ERROR buffer above.
        char sensorFields[96];
        sensors.buildDistanceFields(sensorFields, sizeof(sensorFields));
        protocol.send("STATE", sensorFields);
        sensors.buildImuFields(sensorFields, sizeof(sensorFields));
        protocol.send("STATE", sensorFields);
        sensors.buildEnvironmentFields(sensorFields, sizeof(sensorFields));
        protocol.send("STATE", sensorFields);
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
