#pragma once

#include <Arduino.h>
#include "MotorDriver.h"
#include "Encoder.h"
#include "WheelPID.h"
#include "motion_config.h"

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

    // Local obstacle reflex (ARCHITECTURE_AND_ROADMAP.md §6.2 question
    // 5). Blocks FORWARD motion only: backing away and turning in place
    // stay available, deliberately the same semantics as the Pi's own
    // clamp in rover_core/core.py move() -- this is a safety clamp, not
    // navigation, so the decision of what to do next still belongs to
    // the Pi.
    //
    // Why this exists on the ESP32 at all, given the Pi already does it:
    // since the Pi is deported over WiFi, a Pi-side-only reflex would
    // have to cross a lossy link to stop the robot hitting something.
    // Re-applied on every update(), not just on a new MOVE, so a *stale*
    // command can't keep driving into an obstacle after the link drops.
    void setForwardBlocked(bool blocked) { _obstacleSeen = blocked; }

    // Whether forward motion is ACTUALLY being clamped right now, which
    // is what an operator asking "why is it not moving?" needs -- not
    // just whether a sensor sees something (obstacleSeen()).
    bool forwardBlocked() const { return _obstacleSeen && _obstacleReflexEnabled; }

    // What the ToF sensors say, regardless of whether the reflex is
    // allowed to act on it. Kept separate so turning the reflex off
    // never costs the operator the reading itself.
    bool obstacleSeen() const { return _obstacleSeen; }

    // Runtime override for the reflex above (SYSTEM action=obstacle_reflex,
    // and the button on both control pages).
    //
    // Why this is allowed to exist at all, on a safety clamp: the reflex
    // went live on 2026-09-20 when the VL53L0X were finally wired, and a
    // sensor that sees the chassis, a track, a cable or the floor ahead
    // of it silently zeroes every forward command -- indistinguishable,
    // from the outside, from the dead motors this project has been
    // chasing for weeks. Being able to take one suspect out of the
    // picture in one press is worth more during bring-up than a reflex
    // that cannot be questioned.
    //
    // Deliberately NOT persisted: a disabled safety reflex must not
    // survive a power cycle, so every boot starts with it armed again
    // (contrast CalibrationStore, which does persist PID gains -- those
    // are a tuning choice, this is a guard rail).
    void setObstacleReflexEnabled(bool enabled) { _obstacleReflexEnabled = enabled; }
    bool obstacleReflexEnabled() const { return _obstacleReflexEnabled; }

    // Bring-up only: drives raw PWM straight to the H-bridge, with NO
    // PID and no encoder feedback, for `durationMs` and then stops.
    //
    // Why this is worth having: when the wheels do not turn, the PID's
    // output is useless as evidence -- it saturates at 255 precisely
    // BECAUSE nothing moves, so "PWM 255 and speed 0" is what a
    // mechanical jam, a dead driver and a miswired encoder all look
    // like. Commanding a fixed duty removes the loop from the picture
    // entirely: if the motor still does not move, nothing upstream of
    // the H-bridge can be blamed.
    //
    // Self-limiting by design: a raw duty with no feedback has no
    // reason to outlive its test, so it stops on its own. Still
    // subordinate to stop() and to the SAFE state, which call through
    // the same motors.
    void driveRaw(int16_t leftPwm, int16_t rightPwm, unsigned long durationMs);
    bool rawActive() const { return _rawUntilMs != 0; }

    // Fills "left_speed=... right_speed=... left_pwm=... right_pwm=..."
    // for a periodic STATE frame.
    void buildTelemetryFields(char* out, size_t outLen) const;

    // The obstacle flags, as their own frame. Split off from the above
    // because the two together overran ROVER_MAX_FRAME_LEN once the
    // speeds gained the resolution they needed -- and they belong apart
    // anyway: speeds change constantly, these change on an event.
    void buildObstacleFields(char* out, size_t outLen) const;

    // Applies the same gains to both wheels' PID -- see
    // esp32/lib/calibration/CalibrationStore.h and main.cpp's
    // SYSTEM action=set_pid/get_pid/reset_pid for the runtime API this
    // backs. Both wheels share one set of gains (not tuned
    // independently) since the two motors are the same part.
    void setPidGains(float kp, float ki, float kd, float kff) {
        _pidL.setGains(kp, ki, kd, kff);
        _pidR.setGains(kp, ki, kd, kff);
    }
    float pidKp() const { return _pidL.kp(); }
    float pidKi() const { return _pidL.ki(); }
    float pidKd() const { return _pidL.kd(); }
    float pidKff() const { return _pidL.kff(); }

    // Top speed a MOVE may ask for, in m/s (SYSTEM action=set_speed,
    // persisted in CalibrationStore). Runtime-settable rather than a
    // fixed constant because the compiled default is one measurement,
    // not a specification -- and getting it wrong is not cosmetic: a
    // ceiling above what the wheels can do makes every command an
    // unreachable setpoint, which pins the PWM at 255 and leaves the
    // speed control with nothing to control. See
    // ROVER_MAX_WHEEL_SPEED_MPS for how the default was arrived at.
    //
    // Ignores a non-positive or non-finite value rather than accepting
    // a ceiling of zero, which would silently immobilise the robot.
    void setMaxSpeed(float maxMps) {
        if (isnan(maxMps) || isinf(maxMps) || maxMps <= 0.0f) return;
        _maxSpeedMps = maxMps;
    }
    float maxSpeed() const { return _maxSpeedMps; }

    // Raw, unconverted tick counters for wheel geometry calibration (see
    // SYSTEM action=raw_ticks/reset_ticks) -- independent of the PID
    // loop's own 20ms-reset counters in updateWheel().
    long rawTicksLeft() { return _encL.totalTicks(); }
    long rawTicksRight() { return _encR.totalTicks(); }
    // Raw ISR edge counts -- see Encoder::totalEdges(). Not reset by
    // resetRawTicks(): they measure the health of the input itself, not
    // a calibration run, and zeroing them would throw away the baseline
    // an ISR storm shows up against.
    unsigned long rawEdgesLeft() { return _encL.totalEdges(); }
    unsigned long rawEdgesRight() { return _encR.totalEdges(); }

    // Pops one encoder that just disabled itself on an implausible edge
    // rate -- see Encoder::pollStorm(). Same shape as
    // SensorHub::consumeSensorFailure so main.cpp reports it the same
    // way; call in a loop, both wheels can storm at once.
    bool consumeEncoderStorm(const char** wheelNameOut);
    void resetRawTicks() {
        _encL.resetTotal();
        _encR.resetTotal();
    }

private:
    unsigned long _rawUntilMs = 0;
    bool _pendingStormLeft = false;
    bool _pendingStormRight = false;

    void updateWheel(MotorDriver& motor, Encoder& encoder, WheelPID& pid,
                      float targetMps, float dtSeconds, float& measuredOut,
                      float tickSign, int16_t& pwmOut);

    MotorDriver _motorL, _motorR;
    Encoder _encL, _encR;
    WheelPID _pidL, _pidR;

    float _maxSpeedMps = ROVER_MAX_WHEEL_SPEED_MPS;  // see setMaxSpeed()
    float _targetVelocity = 0.0f;  // m/s
    float _targetRotation = 0.0f;  // rad/s
    bool _obstacleSeen = false;          // see setForwardBlocked()
    bool _obstacleReflexEnabled = true;  // see setObstacleReflexEnabled()
    float _measuredLeftMps = 0.0f;
    float _measuredRightMps = 0.0f;
    // Last PWM actually sent to each motor (motion_config.h: +/-255). Kept
    // around purely for telemetry -- lets a bring-up session see whether
    // the PID ever reaches full duty on a wheel that isn't moving, which
    // tells apart "PID too weak to break static friction" from "full duty
    // applied and the wheel still doesn't turn" (electrical/mechanical
    // fault beyond what firmware can fix).
    int16_t _lastPwmLeft = 0;
    int16_t _lastPwmRight = 0;

    unsigned long _lastUpdateMs = 0;
};
