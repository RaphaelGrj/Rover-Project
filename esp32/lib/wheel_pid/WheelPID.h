#pragma once

#include <Arduino.h>
#include "motion_config.h"

// PID controller for one wheel's closed-loop speed control (target vs.
// encoder-measured m/s -> a signed PWM command for MotorDriver), with a
// feed-forward term and anti-windup -- see update() for why neither is
// optional on this hardware.
//
// Gains default to the compile-time placeholders (motion_config.h) but
// are runtime-settable (setGains()) and can be persisted across reboots
// via CalibrationStore -- see main.cpp's SYSTEM action=set_pid/get_pid/
// reset_pid -- so tuning against real motors doesn't require a reflash
// per attempt.
class WheelPID {
public:
    // Clears accumulated integral/derivative state; call whenever a wheel
    // is stopped so a stale error doesn't cause a jump on restart.
    void reset();

    // target/measured in m/s; dtSeconds is time since the previous call.
    // Returns a signed PWM command, see MotorDriver::setSpeed.
    int16_t update(float targetMps, float measuredMps, float dtSeconds);

    void setGains(float kp, float ki, float kd, float kff) {
        _kp = kp; _ki = ki; _kd = kd; _kff = kff;
    }
    float kp() const { return _kp; }
    float ki() const { return _ki; }
    float kd() const { return _kd; }
    float kff() const { return _kff; }

    // How far the integral term may wind, in m/s.s. Not a tuning knob:
    // it is what stops a wheel that cannot reach its target from
    // accumulating an unbounded bias. See update() for the anti-windup
    // that makes this limit a backstop rather than a destination.
    static constexpr float INTEGRAL_LIMIT = 1.0f;

    // Below this commanded speed (m/s), the target counts as "stop" and
    // the integral is cleared rather than serviced -- see update().
    static constexpr float ZERO_TARGET_EPS = 1e-4f;

private:
    float _kp = ROVER_PID_KP;
    float _ki = ROVER_PID_KI;
    float _kd = ROVER_PID_KD;
    float _kff = ROVER_PID_KFF;
    float _integral = 0.0f;
    float _lastMeasuredMps = 0.0f;
    bool _hasLastMeasured = false;
};
