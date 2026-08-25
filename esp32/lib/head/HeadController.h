#pragma once

#include <Arduino.h>
#include "ServoJoint.h"

// Turns a HEAD command (pitch/yaw, see ROVER_PROTOCOL.md) into smooth,
// limited servo motion. Owns both servos end to end so main.cpp only
// ever deals in pitch/yaw degrees, same separation of concerns as
// DriveController for the wheels.
class HeadController {
public:
    void begin();

    // From a HEAD command. Clamped to ROVER_HEAD_*_MIN/MAX_DEG
    // (head_config.h) -- soft limits, independent of the servo's own
    // hardware 0..180 clamp in ServoJoint.
    void setTarget(float pitchDeg, float yawDeg);

    // Non-blocking, call every loop() iteration; internally rate-limited
    // to ROVER_HEAD_UPDATE_PERIOD_MS. Eases the current position toward
    // the target at ROVER_HEAD_MAX_SPEED_DEG_S rather than snapping.
    void update();

private:
    ServoJoint _pitchServo, _yawServo;

    float _targetPitchDeg = 0.0f;
    float _targetYawDeg = 0.0f;
    float _currentPitchDeg = 0.0f;
    float _currentYawDeg = 0.0f;

    unsigned long _lastUpdateMs = 0;
};
