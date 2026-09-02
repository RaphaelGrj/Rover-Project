#pragma once

#include <Arduino.h>
#include "MotorDriver.h"
#include "Encoder.h"
#include "WheelPID.h"

// Turns a MOVE-style intention (linear velocity + rotation, unicycle
// model) into left/right wheel PWM via per-wheel PID loops closed on the
// encoders. Owns both motors end to end so main.cpp only ever deals in
// velocity/rotation and never touches a motor GPIO directly
// (ARCHITECTURE_AND_ROADMAP.md section 12: "Le Pi ne doit pas être dans
// cette boucle").
class DriveController {
public:
    void begin();

    // From a MOVE command; see ROVER_PROTOCOL.md. Clamped to
    // ROVER_MAX_WHEEL_SPEED_MPS.
    void setTarget(float velocityMps, float rotationRadPerSec);

    // Non-blocking, call every loop() iteration; internally rate-limited
    // to ROVER_DRIVE_UPDATE_PERIOD_MS (motion_config.h).
    void update();

    // Immediate stop, bypassing PID smoothing -- used at boot and on
    // heartbeat timeout/SAFE. Also clears the target so a stale MOVE
    // command can't silently resume motion once ACTIVE again.
    void stop();

    // Fills "left_speed=... right_speed=..." for a STATE frame.
    void buildTelemetryFields(char* out, size_t outLen) const;

    // Applies the same gains to both wheels' PID -- see
    // esp32/lib/calibration/CalibrationStore.h and main.cpp's
    // SYSTEM action=set_pid/get_pid/reset_pid for the runtime API this
    // backs. Both wheels share one set of gains (not tuned
    // independently) since the two motors are the same part.
    void setPidGains(float kp, float ki, float kd) {
        _pidL.setGains(kp, ki, kd);
        _pidR.setGains(kp, ki, kd);
    }
    float pidKp() const { return _pidL.kp(); }
    float pidKi() const { return _pidL.ki(); }
    float pidKd() const { return _pidL.kd(); }

    // Raw, unconverted tick counters for wheel geometry calibration (see
    // SYSTEM action=raw_ticks/reset_ticks) -- independent of the PID
    // loop's own 20ms-reset counters in updateWheel().
    long rawTicksLeft() { return _encL.totalTicks(); }
    long rawTicksRight() { return _encR.totalTicks(); }
    void resetRawTicks() {
        _encL.resetTotal();
        _encR.resetTotal();
    }

private:
    void updateWheel(MotorDriver& motor, Encoder& encoder, WheelPID& pid,
                      float targetMps, float dtSeconds, float& measuredOut, float tickSign);

    MotorDriver _motorL, _motorR;
    Encoder _encL, _encR;
    WheelPID _pidL, _pidR;

    float _targetVelocity = 0.0f;  // m/s
    float _targetRotation = 0.0f;  // rad/s
    float _measuredLeftMps = 0.0f;
    float _measuredRightMps = 0.0f;

    unsigned long _lastUpdateMs = 0;
};
