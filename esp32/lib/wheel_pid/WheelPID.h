#pragma once

#include <Arduino.h>
#include "motion_config.h"

// PID controller for one wheel's closed-loop speed control (target vs.
// encoder-measured m/s -> a signed PWM command for MotorDriver).
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

    void setGains(float kp, float ki, float kd) { _kp = kp; _ki = ki; _kd = kd; }
    float kp() const { return _kp; }
    float ki() const { return _ki; }
    float kd() const { return _kd; }

private:
    float _kp = ROVER_PID_KP;
    float _ki = ROVER_PID_KI;
    float _kd = ROVER_PID_KD;
    float _integral = 0.0f;
    float _lastMeasuredMps = 0.0f;
    bool _hasLastMeasured = false;
};
