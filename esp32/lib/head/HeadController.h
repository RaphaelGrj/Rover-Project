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

    // --- Head motion gate -------------------------------------------
    //
    // Disabled at boot, on purpose. Rover's head is driven by TWO
    // servos facing each other on a SHARED part (tandem mount,
    // confirmed on the physical robot 2026-09-20), which the pitch/yaw
    // model below does NOT describe: there is one mechanical degree of
    // freedom, not two, and the two servos must move in OPPOSITE
    // directions to rotate the part the same way.
    //
    // Until that geometry is measured (neutral point, sign of each
    // side, real end stops -- head_config.h's limits are placeholders
    // that have never met real mechanics), driving both servos from
    // independent pitch/yaw targets would make them fight each other.
    // So update() stays inert until someone explicitly enables it.
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    // --- Bring-up only ----------------------------------------------
    //
    // Drives ONE servo directly, bypassing the pitch/yaw model, and
    // leaves the other one released (no pulses, no torque). That is
    // what makes characterising the mount safe: a released servo
    // cannot oppose the one being moved, so each side's direction of
    // rotation can be established without ever putting the two in
    // conflict on the shared part.
    //
    // Deliberately named A/B rather than pitch/yaw: on this mount they
    // are not two axes, and calling them so is what would hide the
    // problem. A = ROVER_PIN_SERVO_PITCH, B = ROVER_PIN_SERVO_YAW.
    void driveSingleJoint(char which, float angleDeg);

    // Drives BOTH servos as the tandem mount actually requires:
    // B receives the OPPOSITE angle of A.
    //
    // The two servos sit face to face with concentric axes, so turning
    // the shared part one way means one servo turns clockwise while the
    // other turns counter-clockwise. Commanding them the same angle --
    // which is what the pitch/yaw model would do -- makes them pull
    // against each other and stall. This is the only command shape that
    // is mechanically correct on this mount.
    //
    // `angleDeg` is expressed in servo-A terms; B is mirrored from it.
    void driveTandem(float angleDeg);

    // Drives both servos to INDEPENDENT angles -- no mirroring, no
    // assumption that they agree.
    //
    // driveTandem()'s B = -A mirror is geometrically right only if both
    // horns were splined onto their shafts at the same relative
    // position. A servo spline is ~1.4 deg per tooth, so a few teeth of
    // difference means each servo aims the shared part somewhere else
    // and they stall against each other -- motionless, but drawing
    // current. This is the command that lets that offset be measured:
    // hold one side, sweep the other, and watch where the supply
    // current bottoms out. That minimum is the angle pair at which both
    // servos agree.
    void driveIndependent(float angleADeg, float angleBDeg);

    // --- Mechanical origin (persisted) ------------------------------
    //
    // The angle each servo must be commanded for the head to sit at its
    // reference position. Every logical angle used elsewhere (a HEAD
    // frame's pitch, driveTandem's argument) is measured FROM this
    // origin, so the protocol never has to know how the horns happened
    // to land on their splines.
    //
    // It has to be stored, not hardcoded: a servo horn is splined at
    // ~1.4 deg per tooth and any remount lands somewhere new. This is
    // exactly what went wrong on 2026-09-20 -- the horns sat so far off
    // centre that the head's neutral fell at the very end of the servo
    // travel, and every commanded angle drove the part into its bottom
    // stop. An offset in NVS makes that recoverable without a reflash.
    void loadOrigin();
    // Records the angles currently commanded as the new origin, and
    // persists them.
    void setOriginHere();
    float originA() const { return _originADeg; }
    float originB() const { return _originBDeg; }

    // Releases both servos: no pulses at all, zero torque, head limp.
    void releaseAll();

    void buildStatusFields(char* out, size_t outLen) const;

private:
    ServoJoint _pitchServo, _yawServo;

    float _targetPitchDeg = 0.0f;
    float _targetYawDeg = 0.0f;
    float _currentPitchDeg = 0.0f;
    float _currentYawDeg = 0.0f;

    unsigned long _lastUpdateMs = 0;
    bool _enabled = false;
    // Last angle commanded through driveSingleJoint, for telemetry --
    // a servo gives no position feedback, so what we asked for is the
    // only thing that can be reported.
    float _lastSingleAngleDeg = 0.0f;
    float _lastBAngleDeg = 0.0f;
    float _originADeg = 0.0f;
    float _originBDeg = 0.0f;
    char _lastSingleJoint = '-';
};
