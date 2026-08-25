#include "MotorDriver.h"
#include "motion_config.h"

void MotorDriver::begin(int in1Pin, int in2Pin, int channelFwd, int channelRev) {
    _channelFwd = channelFwd;
    _channelRev = channelRev;

    ledcSetup(_channelFwd, ROVER_PWM_FREQ_HZ, ROVER_PWM_RESOLUTION_BITS);
    ledcSetup(_channelRev, ROVER_PWM_FREQ_HZ, ROVER_PWM_RESOLUTION_BITS);
    ledcAttachPin(in1Pin, _channelFwd);
    ledcAttachPin(in2Pin, _channelRev);
    ledcWrite(_channelFwd, 0);
    ledcWrite(_channelRev, 0);
}

void MotorDriver::setSpeed(int16_t pwm) {
    if (pwm > 255) pwm = 255;
    if (pwm < -255) pwm = -255;

    // Only one direction is ever driven at a time; the other side stays
    // at 0 duty (coast), never both at once (that would be a brake, not
    // what a speed command means here).
    if (pwm >= 0) {
        ledcWrite(_channelFwd, pwm);
        ledcWrite(_channelRev, 0);
    } else {
        ledcWrite(_channelFwd, 0);
        ledcWrite(_channelRev, -pwm);
    }
}
