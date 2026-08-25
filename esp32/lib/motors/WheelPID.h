#pragma once

#include <Arduino.h>

// PID controller for one wheel's closed-loop speed control (target vs.
// encoder-measured m/s -> a signed PWM command for MotorDriver).
// Gains are untuned placeholders (motion_config.h) until real N20 motors
// + encoders are on hand to tune against.
class WheelPID {
public:
    // Clears accumulated integral/derivative state; call whenever a wheel
    // is stopped so a stale error doesn't cause a jump on restart.
    void reset();

    // target/measured in m/s; dtSeconds is time since the previous call.
    // Returns a signed PWM command, see MotorDriver::setSpeed.
    int16_t update(float targetMps, float measuredMps, float dtSeconds);

private:
    float _integral = 0.0f;
    float _lastError = 0.0f;
};
