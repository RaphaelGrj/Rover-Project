#pragma once

#include <Arduino.h>

// Low-level driver for one side of a DRV8833 dual H-bridge, in
// "PWM-on-both-inputs" mode: driving IN1 spins the motor forward at that
// duty, driving IN2 spins it in reverse, and 0 on both coasts to a stop.
// There's no separate enable/PWM pin like on a TB6612FNG, so this owns
// two LEDC channels instead of one -- see motion_config.h.
class MotorDriver {
public:
    // in1Pin/in2Pin: DRV8833 AIN1/AIN2 (or BIN1/BIN2). channelFwd/channelRev
    // must be distinct, free LEDC channels.
    void begin(int in1Pin, int in2Pin, int channelFwd, int channelRev);

    // Signed duty: -255 (full reverse) .. 0 .. 255 (full forward).
    // Values outside that range are clamped. 0 coasts (no active brake).
    void setSpeed(int16_t pwm);

private:
    int _channelFwd = -1;
    int _channelRev = -1;
};
