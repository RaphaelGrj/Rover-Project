#include "WheelPID.h"
#include <Arduino.h>
#include "motion_config.h"

void WheelPID::reset() {
    _integral = 0.0f;
    _lastError = 0.0f;
}

int16_t WheelPID::update(float targetMps, float measuredMps, float dtSeconds) {
    if (dtSeconds <= 0.0f) return 0;

    float error = targetMps - measuredMps;

    _integral += error * dtSeconds;
    // Clamp the integral term so a wheel that can't reach its target (no
    // encoder wired yet in simulation, stalled, etc.) can't wind up to a
    // huge offset that then overshoots wildly once it does move.
    _integral = constrain(_integral, -1.0f, 1.0f);

    float derivative = (error - _lastError) / dtSeconds;
    _lastError = error;

    float output = ROVER_PID_KP * error + ROVER_PID_KI * _integral + ROVER_PID_KD * derivative;
    return (int16_t)constrain(output, -255.0f, 255.0f);
}
