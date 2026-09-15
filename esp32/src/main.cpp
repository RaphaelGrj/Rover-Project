#include <Arduino.h>
#include <Wire.h>
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
#include "RoverWifiProvisioning.h"
#include "StandaloneControl.h"
#include "WifiCredentialsStore.h"
#include "CalibrationStore.h"
#include "Buzzer.h"

// RoverProtocol takes a Stream&, never a HardwareSerial& -- that is what
// keeps the transport swappable (ROVER_PROTOCOL.md section 2). The Pi is
// deported over WiFi since 2026-09-15 (ARCHITECTURE_AND_ROADMAP.md
// section 6.2), so this is meant to become a WiFiClient (also a Stream),
// with Serial kept as the fallback. NOT DONE YET: the firmware still
// speaks over the USB cable only -- see the "Pi deporte" work item in
// PROGRESS.md for the intended order (transport first, measure, then
// tune the heartbeat thresholds).
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
RoverWifiProvisioning wifiProvisioning;
StandaloneControl standalone;
Buzzer buzzer;
// How long without any valid frame from the Pi before Rover offers its
// own piloting AP (StandaloneControl.h). Deliberately far longer than
// ROVER_HEARTBEAT_TIMEOUT_MS: that one is a safety reflex measured in
// hundreds of milliseconds, this is "the Pi is really not coming back",
// and raising an access point on every brief WiFi hiccup would be both
// useless and a standing door onto the robot.
constexpr unsigned long ROVER_STANDALONE_FALLBACK_MS = 30000;
// When the *Pi* last sent a valid frame -- tracked separately from
// HeartbeatMonitor, which is fed by the standalone page too. Using the
// monitor here would be circular: the page keeps it fresh, so the robot
// would keep concluding the Pi was back and shut the AP down mid-drive.
// Starts at 0, so before the first frame this reads as "time since
// boot" -- which is what makes "powered on with no Pi at all" reach the
// fallback, the primary case standalone piloting exists for.
unsigned long lastPiFrameMs = 0;
unsigned long lastTelemetryMs = 0;
unsigned long lastSensorTelemetryMs = 0;
unsigned long lastBatteryTelemetryMs = 0;

// Called by RoverProtocol for every validated incoming frame.
void onFrame(const RoverFrame& frame) {
    // Any valid frame counts as proof of life from the Pi.
    heartbeat.reset();
    // Separately from the heartbeat above, which the standalone page
    // also feeds -- see lastPiFrameMs's declaration.
    lastPiFrameMs = millis();
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
        } else if (strcmp(action, "set_pid") == 0) {
            // Runtime PID tuning without a reflash per attempt -- any
            // field left out keeps its current value (getFloat's
            // default), so eg. "set_pid kp=200" alone only touches Kp.
            float kp = frame.getFloat("kp", drive.pidKp());
            float ki = frame.getFloat("ki", drive.pidKi());
            float kd = frame.getFloat("kd", drive.pidKd());
            if (isnan(kp) || isinf(kp) || isnan(ki) || isinf(ki) || isnan(kd) || isinf(kd) ||
                kp < 0.0f || ki < 0.0f || kd < 0.0f) {
                protocol.sendError("invalid_pid_gains");
            } else {
                drive.setPidGains(kp, ki, kd);
                CalibrationStore::setFloat("pid_kp", kp);
                CalibrationStore::setFloat("pid_ki", ki);
                CalibrationStore::setFloat("pid_kd", kd);
                char fields2[64];
                snprintf(fields2, sizeof(fields2), "pid_kp=%.2f pid_ki=%.2f pid_kd=%.2f", kp, ki, kd);
                protocol.send("STATE", fields2);
            }
        } else if (strcmp(action, "get_pid") == 0) {
            char fields2[64];
            snprintf(fields2, sizeof(fields2), "pid_kp=%.2f pid_ki=%.2f pid_kd=%.2f",
                     drive.pidKp(), drive.pidKi(), drive.pidKd());
            protocol.send("STATE", fields2);
        } else if (strcmp(action, "reset_pid") == 0) {
            // Reverts to the compiled-in placeholders (motion_config.h)
            // and forgets the NVS override, rather than just resetting
            // the in-memory value -- a reboot after this must not bring
            // the old override back.
            drive.setPidGains(ROVER_PID_KP, ROVER_PID_KI, ROVER_PID_KD);
            CalibrationStore::remove("pid_kp");
            CalibrationStore::remove("pid_ki");
            CalibrationStore::remove("pid_kd");
            char fields2[64];
            snprintf(fields2, sizeof(fields2), "pid_kp=%.2f pid_ki=%.2f pid_kd=%.2f",
                     ROVER_PID_KP, ROVER_PID_KI, ROVER_PID_KD);
            protocol.send("STATE", fields2);
        } else if (strcmp(action, "raw_ticks") == 0) {
            // Bring-up only: raw cumulative encoder counts, untouched by
            // the PID loop's 20ms readAndResetTicks() -- lets a hand
            // rotation of a known number of turns be counted precisely,
            // to measure the real ROVER_ENCODER_TICKS_PER_REV instead of
            // the motion_config.h placeholder. See action=reset_ticks.
            char fields2[64];
            snprintf(fields2, sizeof(fields2), "raw_ticks_left=%ld raw_ticks_right=%ld",
                     drive.rawTicksLeft(), drive.rawTicksRight());
            protocol.send("STATE", fields2);
        } else if (strcmp(action, "reset_ticks") == 0) {
            drive.resetRawTicks();
            protocol.send("STATE", "raw_ticks_left=0 raw_ticks_right=0");
        } else if (strcmp(action, "i2c_scan") == 0) {
            // Bring-up only: lists every address that ACKs a zero-length
            // transaction (same check as I2CProbe.h's i2cDevicePresent),
            // so a sensor's real address can be found directly instead of
            // guessing from datasheet/library defaults one at a time.
            char fields[128];
            int len = snprintf(fields, sizeof(fields), "i2c_found=");
            bool any = false;
            for (uint8_t addr = 0x03; addr <= 0x77 && len < (int)sizeof(fields) - 6; addr++) {
                Wire.beginTransmission(addr);
                if (Wire.endTransmission() == 0) {
                    len += snprintf(fields + len, sizeof(fields) - len, "%s0x%02X", any ? "," : "", addr);
                    any = true;
                }
            }
            if (!any) snprintf(fields + len, sizeof(fields) - len, "none");
            protocol.send("STATE", fields);
        } else if (strcmp(action, "bme_chip_id") == 0) {
            // Bring-up only: reads the raw chip-id register (0xD0) the
            // BME68x driver itself checks against 0x61 before accepting
            // the sensor -- narrows "begin() fails" down to "wrong chip
            // id" (clone/different part) vs. "register read itself
            // failed" (bus/timing) vs. "soft-reset write failed" (can't
            // tell apart from this alone, but chip_id=0xFF/no ACK here
            // points at a bus-level problem beyond the simple probe).
            Wire.beginTransmission(ROVER_ENV_SENSOR_ADDRESS);
            Wire.write((uint8_t)0xD0);
            uint8_t err = Wire.endTransmission(false);
            char fields[48];
            if (err != 0) {
                snprintf(fields, sizeof(fields), "bme_chip_id_error=%d", err);
            } else {
                Wire.requestFrom((uint8_t)ROVER_ENV_SENSOR_ADDRESS, (uint8_t)1);
                if (Wire.available()) {
                    uint8_t id = Wire.read();
                    snprintf(fields, sizeof(fields), "bme_chip_id=0x%02X", id);
                } else {
                    snprintf(fields, sizeof(fields), "bme_chip_id=no_response");
                }
            }
            protocol.send("STATE", fields);
        } else if (strcmp(action, "wifi_setup") == 0) {
            // Opens the on-demand config portal (RoverWifiProvisioning.h)
            // -- a temporary AP any PC/phone can join to enter the real
            // WiFi/OTA credentials. Never automatic at boot, only via
            // this explicit request (see that file's header comment for
            // why). Interrupts any active OTA connection, since both
            // share the same WiFi radio.
            wifiProvisioning.start();
            protocol.send("STATE", "wifi_mode=setup");
        } else if (strcmp(action, "wifi_status") == 0) {
            // 96, not 64: "wifi_mode=setup ap_ssid=Rover-Setup-XXXXXX
            // ap_started=1 ap_ip=255.255.255.255" alone is ~75 bytes --
            // a bring-up test on real hardware caught this being
            // silently truncated mid-field by too small a buffer (same
            // class of bug as the ERROR/STATE buffers in Phase 4).
            char fields2[96];
            if (wifiProvisioning.isActive()) {
                wifiProvisioning.buildStatusFields(fields2, sizeof(fields2));
            } else if (standalone.isActive()) {
                standalone.buildStatusFields(fields2, sizeof(fields2));
            } else {
                ota.buildStatusFields(fields2, sizeof(fields2));
            }
            protocol.send("STATE", fields2);
        } else if (strcmp(action, "standalone") == 0) {
            // Manual entry, mainly for testing the mode without waiting
            // out ROVER_STANDALONE_FALLBACK_MS -- and for deliberately
            // handing the robot over to a phone before walking away
            // from the Pi. Note this drops the station link, so over a
            // WiFi Rover Protocol link this command is one-way: the
            // reply below may not reach the Pi.
            if (standalone.start()) {
                char fields2[96];
                standalone.buildStatusFields(fields2, sizeof(fields2));
                protocol.send("STATE", fields2);
            } else {
                protocol.sendError("standalone_no_password");
            }
        } else if (strcmp(action, "standalone_off") == 0) {
            standalone.stop();
            protocol.send("STATE", "wifi_mode=idle");
        } else if (strcmp(action, "wifi_forget") == 0) {
            // Clears the stored home-network credentials (keeps the OTA
            // password, see WifiCredentialsStore::forgetNetwork) so the
            // next wifi_setup starts from a clean SSID field. Does not
            // itself reboot or touch the current connection.
            WifiCredentialsStore::forgetNetwork();
            protocol.send("STATE", "wifi_mode=forgotten");
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
    // Any PID gains saved via a previous SYSTEM action=set_pid survive
    // reboots -- falls back to the compiled defaults (motion_config.h)
    // when nothing has ever been stored (first boot, or after
    // action=reset_pid).
    drive.setPidGains(
        CalibrationStore::getFloat("pid_kp", ROVER_PID_KP),
        CalibrationStore::getFloat("pid_ki", ROVER_PID_KI),
        CalibrationStore::getFloat("pid_kd", ROVER_PID_KD)
    );
    head.begin();
    display.begin();
    sensors.begin();
    estop.begin();
    battery.begin();
    buzzer.begin();
    // Entirely opt-in (see RoverOTA.h) -- a no-op that returns
    // immediately unless WiFi/OTA credentials were set at build time.
    ota.begin();

    // Standalone piloting (StandaloneControl.h): every callback below
    // routes into the SAME code the Pi's frames go through, rather than
    // reaching into the motors directly. That is the whole safety
    // argument for this mode -- there is no second control path that
    // could drift out of agreement with the first one.
    standalone.onHeartbeat = []() {
        // A request from the page is proof of life exactly like a frame
        // from the Pi, so it feeds the same monitor and the same
        // timeout stops the motors when the tab closes or the phone
        // walks out of range.
        heartbeat.reset();
    };
    standalone.onMove = [](float velocity, float rotation) {
        // Same ACTIVE gate as a MOVE frame: a page left open must not
        // re-arm a robot sitting in SAFE.
        if (state == RoverState::ACTIVE) drive.setTarget(velocity, rotation);
    };
    standalone.onLook = [](float pitch, float yaw) {
        if (state == RoverState::ACTIVE) head.setTarget(pitch, yaw);
    };
    standalone.onResume = []() {
        // Same E-stop guard as SYSTEM action=resume -- the operator is
        // standing next to the robot here, which makes honouring a held
        // button more important, not less.
        //
        // One deliberate difference: READY is accepted too, which the
        // Pi's resume does not allow. Normally the Pi's first frame is
        // what promotes READY -> ACTIVE, so a robot powered on with no
        // Pi at all would sit in READY forever and this whole mode
        // would be unusable in exactly the case it exists for. The
        // press on "Activer" is the equivalent explicit act.
        if ((state == RoverState::SAFE || state == RoverState::READY) && !estop.isPressed()) {
            state = RoverState::ACTIVE;
        }
    };
    standalone.onStop = []() {
        drive.stop();
        state = RoverState::SAFE;
    };
    standalone.statusProvider = []() {
        const char* name = "?";
        switch (state) {
            case RoverState::BOOT:   name = "BOOT";   break;
            case RoverState::READY:  name = "READY";  break;
            case RoverState::ACTIVE: name = "ACTIVE"; break;
            case RoverState::SAFE:   name = "SAFE";   break;
            case RoverState::ERROR:  name = "ERROR";  break;
        }
        // Shown verbatim on the page: without forward_blocked, an
        // obstacle reflex reads as "the robot ignores my joystick".
        return String(name) + (drive.forwardBlocked() ? " | obstacle: marche avant bloquee" : "");
    };

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

    buzzer.play(BuzzerSound::BOOT);
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
    wifiProvisioning.update();
    standalone.update();
    buzzer.update();

    // Standalone piloting fallback: after a long silence from the Pi,
    // Rover raises its own AP so it stays drivable by whoever is
    // standing next to it (ARCHITECTURE_AND_ROADMAP.md §6.3). Covers
    // both "the link died" and "there is no Pi at all" -- see
    // HeartbeatMonitor::millisSinceLast on why booting without a Pi
    // reaches this too.
    //
    // Never while the provisioning portal is up: both want the radio in
    // AP mode and port 80, and provisioning is an explicit operator
    // action that must not be interrupted by an automatic one.
    if (!standalone.isActive() && !wifiProvisioning.isActive() &&
        millis() - lastPiFrameMs >= ROVER_STANDALONE_FALLBACK_MS) {
        if (standalone.start()) {
            char fields[96];
            standalone.buildStatusFields(fields, sizeof(fields));
            protocol.send("STATE", fields);
            protocol.send("EVENT", "name=standalone_started");
        } else {
            // Refused for want of a stored password (StandaloneControl
            // never falls back to an open AP). Reported once per boot,
            // not every loop: with no Pi listening this frame may well
            // go nowhere, but the buzzer tells whoever is next to the
            // robot why no "Rover-Pilot-..." network is appearing.
            static bool refusalReported = false;
            if (!refusalReported) {
                refusalReported = true;
                protocol.send("EVENT", "name=standalone_unavailable reason=no_password");
                buzzer.play(BuzzerSound::LOW_BATTERY);
            }
        }
    }
    // The Pi came back -- hand the radio back rather than leaving an AP
    // open indefinitely. Reachable in practice over USB, or over WiFi
    // once the station link returns; AP mode drops the station link, so
    // this mostly matters for the wired case today.
    //
    // Guarded on isBeingUsed(): yanking the AP out from under someone
    // actively driving would stop the robot mid-manoeuvre (the page's
    // heartbeat dies with the network, so SAFE follows within
    // ROVER_HEARTBEAT_TIMEOUT_MS). Safe, but a nasty surprise for
    // whoever is holding the phone -- the Pi returning is not urgent
    // enough to justify it, so we wait for the operator to let go.
    if (standalone.isActive() && !standalone.isBeingUsed() &&
        millis() - lastPiFrameMs < ROVER_STANDALONE_FALLBACK_MS) {
        standalone.stop();
        protocol.send("EVENT", "name=standalone_stopped");
    }

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
        buzzer.play(BuzzerSound::ESTOP);
    }
    wasEstopPressed = estop.isPressed();

    if (battery.hasReading()) {
        if (battery.consumeLowBatteryEvent()) {
            protocol.send("EVENT", "name=low_battery");
            buzzer.play(BuzzerSound::LOW_BATTERY);
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
    // LOCAL obstacle reflex -- the ESP32 refuses to drive further into an
    // obstacle by itself, without consulting the Pi. Level-driven (not
    // the edge event below) so the block holds for as long as the
    // obstacle is there, and releases on its own once it clears.
    //
    // Before 2026-09-15 this clamp existed ONLY on the Pi
    // (rover_core/core.py move()), which was fine while the Pi sat on
    // the robot at the end of a USB cable. With the Pi deported over
    // WiFi (ARCHITECTURE_AND_ROADMAP.md §6.2) a Pi-side-only reflex
    // would have to cross a lossy link to stop the robot hitting
    // something -- so it is duplicated here deliberately. The Pi keeps
    // its own clamp: two independent layers, neither load-bearing alone.
    //
    // Reads false while no ToF sensor is healthy, so this is inert until
    // the VL53L0X are actually wired and can never immobilize the robot
    // on a phantom reading from a missing sensor.
    drive.setForwardBlocked(sensors.obstacleDetected());

    if (sensors.consumeObstacleEvent()) {
        protocol.send("EVENT", "name=obstacle_detected");
        buzzer.play(BuzzerSound::OBSTACLE);
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
            // 128, not 96: adding forward_blocked= (2026-09-15) pushed
            // the worst case to ~84 bytes, leaving only 12 spare -- the
            // exact margin that produced this file's three previous
            // truncation bugs. RoverProtocol::send() now detects a
            // truncated frame instead of emitting a silently-cut one,
            // but detection is the backstop, not the plan.
            char fields[128];
            drive.buildTelemetryFields(fields, sizeof(fields));
            protocol.send("STATE", fields);
        }
    }

    RoverWatchdog::feed();
}
