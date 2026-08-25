#include "ServoJoint.h"
#include "head_config.h"

void ServoJoint::begin(int pin, int channel) {
    _channel = channel;
    ledcSetup(_channel, ROVER_SERVO_PWM_FREQ_HZ, ROVER_SERVO_PWM_RESOLUTION_BITS);
    ledcAttachPin(pin, _channel);
    writeAngle(0.0f);  // center on boot, never start at an undefined pulse
}

void ServoJoint::writeAngle(float angleDeg) {
    float physicalDeg = constrain(angleDeg + 90.0f, 0.0f, 180.0f);

    long pulseUs = map((long)(physicalDeg * 100), 0, 180 * 100,
                        ROVER_SERVO_MIN_PULSE_US, ROVER_SERVO_MAX_PULSE_US);

    long periodUs = 1000000L / ROVER_SERVO_PWM_FREQ_HZ;
    long maxDuty = (1L << ROVER_SERVO_PWM_RESOLUTION_BITS) - 1;
    long duty = (pulseUs * maxDuty) / periodUs;

    ledcWrite(_channel, duty);
}
