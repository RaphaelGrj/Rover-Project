#include "HeadController.h"
#include "head_config.h"

void HeadController::begin() {
    _pitchServo.begin(ROVER_PIN_SERVO_PITCH, ROVER_PWM_CHANNEL_SERVO_PITCH);
    _yawServo.begin(ROVER_PIN_SERVO_YAW, ROVER_PWM_CHANNEL_SERVO_YAW);
    _lastUpdateMs = millis();
}

void HeadController::setTarget(float pitchDeg, float yawDeg) {
    // Same lesson as DriveController::setTarget (Phase 2): a
    // checksum-valid frame doesn't guarantee semantically valid floats,
    // and constrain() doesn't clamp NaN.
    if (isnan(pitchDeg) || isinf(pitchDeg)) pitchDeg = 0.0f;
    if (isnan(yawDeg) || isinf(yawDeg)) yawDeg = 0.0f;

    _targetPitchDeg = constrain(pitchDeg, ROVER_HEAD_PITCH_MIN_DEG, ROVER_HEAD_PITCH_MAX_DEG);
    _targetYawDeg = constrain(yawDeg, ROVER_HEAD_YAW_MIN_DEG, ROVER_HEAD_YAW_MAX_DEG);
}

namespace {
// Moves `current` toward `target` by at most maxStep, without
// overshooting -- the shared step used for both axes.
float approach(float current, float target, float maxStep) {
    float diff = target - current;
    if (diff > maxStep) return current + maxStep;
    if (diff < -maxStep) return current - maxStep;
    return target;
}
}  // namespace

void HeadController::update() {
    unsigned long now = millis();
    if (now - _lastUpdateMs < ROVER_HEAD_UPDATE_PERIOD_MS) return;
    float dtSeconds = (now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    float maxStep = ROVER_HEAD_MAX_SPEED_DEG_S * dtSeconds;
    _currentPitchDeg = approach(_currentPitchDeg, _targetPitchDeg, maxStep);
    _currentYawDeg = approach(_currentYawDeg, _targetYawDeg, maxStep);

    _pitchServo.writeAngle(_currentPitchDeg);
    _yawServo.writeAngle(_currentYawDeg);
}
