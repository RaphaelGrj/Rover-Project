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

// SLOW DECAY drive, adopted 2026-09-20 after measuring a start-up
// threshold of 200-255 PWM on a bare N20 -- one that should start
// around 60-80.
//
// The previous scheme (PWM on one input, 0 on the other) is FAST decay:
// between pulses both outputs are released and the winding current
// collapses through the flyback diodes. At 20kHz the current never gets
// to establish itself, so average torque stays tiny until duty is near
// 100%. That is exactly the curve measured: nothing at 100, a twitch at
// 160, rotation only at 255.
//
// Slow decay instead holds one input HIGH and modulates the other
// downward. Between pulses the current recirculates through the low-side
// transistors instead of collapsing, so it carries over from one pulse
// to the next and torque at a given duty is far higher. It is the mode
// TI recommends for speed control on this part.
//
// Note the inversion: for forward motion IN1 stays at 255 and IN2 is
// driven at (255 - duty). Duty 0 would therefore mean both inputs high,
// which is a BRAKE -- so zero is special-cased back to a true coast
// (both inputs low), preserving the previous meaning of "0 = coast, no
// active braking" that stop() and the SAFE state rely on.
void MotorDriver::setSpeed(int16_t pwm) {
    if (pwm > 255) pwm = 255;
    if (pwm < -255) pwm = -255;

    if (pwm == 0) {
        // True coast: both low. Never "both high", which brakes.
        ledcWrite(_channelFwd, 0);
        ledcWrite(_channelRev, 0);
        return;
    }

    if (pwm > 0) {
        ledcWrite(_channelFwd, 255);
        ledcWrite(_channelRev, 255 - pwm);
    } else {
        ledcWrite(_channelRev, 255);
        ledcWrite(_channelFwd, 255 + pwm);  // pwm is negative here
    }
}
