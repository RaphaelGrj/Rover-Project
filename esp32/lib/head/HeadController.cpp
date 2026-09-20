#include "HeadController.h"
#include "head_config.h"
#include "CalibrationStore.h"

void HeadController::begin() {
    _pitchServo.begin(ROVER_PIN_SERVO_PITCH, ROVER_PWM_CHANNEL_SERVO_PITCH);
    _yawServo.begin(ROVER_PIN_SERVO_YAW, ROVER_PWM_CHANNEL_SERVO_YAW);
    _lastUpdateMs = millis();
}

void HeadController::loadOrigin() {
    // Defaults to 0/0 -- the servos' own electrical centre -- which is
    // the right fallback on a board that has never been calibrated.
    _originADeg = CalibrationStore::getFloat("head_org_a", 0.0f);
    _originBDeg = CalibrationStore::getFloat("head_org_b", 0.0f);
}

void HeadController::setOriginHere() {
    _originADeg = _lastSingleAngleDeg;
    _originBDeg = _lastBAngleDeg;
    CalibrationStore::setFloat("head_org_a", _originADeg);
    CalibrationStore::setFloat("head_org_b", _originBDeg);
}

void HeadController::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) {
        releaseAll();
        return;
    }
    // Re-entering normal motion from wherever the interpolator last
    // was would step the servos abruptly. Start from the commanded
    // targets instead, so the first update() has nothing to catch up.
    _currentPitchDeg = _targetPitchDeg;
    _currentYawDeg = _targetYawDeg;
}

void HeadController::releaseAll() {
    _pitchServo.detach();
    _yawServo.detach();
    _lastSingleJoint = '-';
}

void HeadController::driveSingleJoint(char which, float angleDeg) {
    if (isnan(angleDeg) || isinf(angleDeg)) return;
    // Bring-up clamp widened to the servo's FULL physical range on
    // 2026-09-20. It was +/-40 deg, chosen to keep early moves small --
    // but a full sweep of that interval never lifted the head once, in
    // either direction. For a monotonic device that means the neutral
    // point lies outside it, and a clamp that cannot reach the neutral
    // point makes the mount impossible to characterise at all. A servo
    // cannot have its neutral outside its own travel, so +/-90 is
    // guaranteed to contain it. ServoJoint still clamps to the physical
    // 0..180 as the last line of defence.
    angleDeg = constrain(angleDeg, -90.0f, 90.0f);

    // Normal motion must be off, otherwise update() would immediately
    // overwrite whatever this wrote.
    _enabled = false;

    if (which == 'a') {
        _yawServo.detach();  // released: cannot oppose the one moving
        _pitchServo.writeAngle(angleDeg);
    } else if (which == 'b') {
        _pitchServo.detach();
        _yawServo.writeAngle(angleDeg);
    } else {
        return;
    }
    _lastSingleJoint = which;
    _lastSingleAngleDeg = angleDeg;
}

void HeadController::driveTandem(float angleDeg) {
    if (isnan(angleDeg) || isinf(angleDeg)) return;
    // Same bring-up clamp as driveSingleJoint -- and it MUST stay the
    // same. It was left at +/-40 when driveSingleJoint was widened to
    // +/-90 (2026-09-20), so the three entry points silently clamped
    // differently: "which=ab a=0 b=60" came back as b=40 while
    // "which=b angle=60" went through. Three paths into the same
    // hardware must not disagree about their limits; that turns every
    // measurement into a guess about which path was taken.
    angleDeg = constrain(angleDeg, -90.0f, 90.0f);

    // Normal pitch/yaw motion must stay off, otherwise update() would
    // overwrite both servos on its next pass.
    _enabled = false;

    // Measured FROM the stored origin, not from the servo's raw centre
    // -- see loadOrigin(). The two servos take the SAME sign here:
    // established on the robot 2026-09-20, because the horns are
    // mounted in mirror image, which cancels the inversion that facing
    // shafts would otherwise impose.
    _pitchServo.writeAngle(_originADeg + angleDeg);
    _yawServo.writeAngle(_originBDeg + angleDeg);

    _lastSingleJoint = 'p';  // 'p' for pair
    _lastSingleAngleDeg = _originADeg + angleDeg;
    _lastBAngleDeg = _originBDeg + angleDeg;
}

void HeadController::driveIndependent(float angleADeg, float angleBDeg) {
    if (isnan(angleADeg) || isinf(angleADeg)) return;
    if (isnan(angleBDeg) || isinf(angleBDeg)) return;
    // Same limits as the other two entry points -- see driveTandem.
    angleADeg = constrain(angleADeg, -90.0f, 90.0f);
    angleBDeg = constrain(angleBDeg, -90.0f, 90.0f);

    _enabled = false;
    _pitchServo.writeAngle(angleADeg);
    _yawServo.writeAngle(angleBDeg);

    _lastSingleJoint = 'i';  // 'i' for independent
    _lastSingleAngleDeg = angleADeg;
    _lastBAngleDeg = angleBDeg;
}

void HeadController::buildStatusFields(char* out, size_t outLen) const {
    snprintf(out, outLen,
             "head_enabled=%d head_a_att=%d head_b_att=%d head_mode=%c "
             "head_a_deg=%.1f head_b_deg=%.1f head_org_a=%.1f head_org_b=%.1f",
             _enabled ? 1 : 0, _pitchServo.isAttached() ? 1 : 0,
             _yawServo.isAttached() ? 1 : 0, _lastSingleJoint,
             _lastSingleAngleDeg, _lastBAngleDeg, _originADeg, _originBDeg);
}

void HeadController::setTarget(float pitchDeg, float yawDeg) {
    // Same lesson as DriveController::setTarget (Phase 2): a
    // checksum-valid frame doesn't guarantee semantically valid floats,
    // and constrain() doesn't clamp NaN.
    if (isnan(pitchDeg) || isinf(pitchDeg)) pitchDeg = 0.0f;
    if (isnan(yawDeg) || isinf(yawDeg)) yawDeg = 0.0f;

    _targetPitchDeg = constrain(pitchDeg, ROVER_HEAD_PITCH_MIN_DEG, ROVER_HEAD_PITCH_MAX_DEG);
    _targetYawDeg = constrain(yawDeg, ROVER_HEAD_YAW_MIN_DEG, ROVER_HEAD_YAW_MAX_DEG);

    // A HEAD frame is an explicit request to move the head, so it arms
    // motion. The gate exists to keep the servos silent until someone
    // deliberately drives them (see setEnabled), not to require a
    // separate arming command the protocol does not define. Entering
    // through setEnabled() rather than by touching _enabled keeps the
    // interpolator from jumping on the first update().
    if (!_enabled) setEnabled(true);
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
    // Inert until explicitly enabled -- see setEnabled()'s comment.
    if (!_enabled) return;

    unsigned long now = millis();
    if (now - _lastUpdateMs < ROVER_HEAD_UPDATE_PERIOD_MS) return;
    float dtSeconds = (now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    float maxStep = ROVER_HEAD_MAX_SPEED_DEG_S * dtSeconds;
    _currentPitchDeg = approach(_currentPitchDeg, _targetPitchDeg, maxStep);
    _currentYawDeg = approach(_currentYawDeg, _targetYawDeg, maxStep);

    // TANDEM: both servos take the SAME pitch angle, offset from their
    // own stored origin. Not pitch on one and yaw on the other -- that
    // was the Phase 3 model of two independent perpendicular axes,
    // which this chassis does not have. Here one shared part is driven
    // from both ends, so sending them different angles would have them
    // pull against each other (see head_config.h).
    _pitchServo.writeAngle(_originADeg + _currentPitchDeg);
    _yawServo.writeAngle(_originBDeg + _currentPitchDeg);
}
