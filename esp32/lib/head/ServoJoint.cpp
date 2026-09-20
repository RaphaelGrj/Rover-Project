#include "ServoJoint.h"
#include "head_config.h"

void ServoJoint::begin(int pin, int channel) {
    _channel = channel;
    ledcSetup(_channel, ROVER_SERVO_PWM_FREQ_HZ, ROVER_SERVO_PWM_RESOLUTION_BITS);
    ledcAttachPin(pin, _channel);
    // Released, NOT centered -- see detach()'s comment for why that
    // changed on 2026-09-20.
    detach();
}

void ServoJoint::detach() {
    if (_channel < 0) return;
    ledcWrite(_channel, 0);
    _attached = false;
}

void ServoJoint::writeAngle(float angleDeg) {
    if (_channel < 0) return;
    _attached = true;
    float physicalDeg = constrain(angleDeg + 90.0f, 0.0f, 180.0f);

    long pulseUs = map((long)(physicalDeg * 100), 0, 180 * 100,
                        ROVER_SERVO_MIN_PULSE_US, ROVER_SERVO_MAX_PULSE_US);

    long periodUs = 1000000L / ROVER_SERVO_PWM_FREQ_HZ;
    long maxDuty = (1L << ROVER_SERVO_PWM_RESOLUTION_BITS) - 1;
    long duty = (pulseUs * maxDuty) / periodUs;

    ledcWrite(_channel, duty);
}
