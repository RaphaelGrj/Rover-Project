#include "DriveController.h"
#include "motion_config.h"

namespace {

// Encoder ticks over dtSeconds -> linear wheel speed in m/s.
float ticksToMps(long ticks, float dtSeconds) {
    if (dtSeconds <= 0.0f) return 0.0f;
    float revs = ticks / ROVER_ENCODER_TICKS_PER_REV;
    float meters = revs * (PI * ROVER_WHEEL_DIAMETER_M);
    return meters / dtSeconds;
}

}  // namespace

void DriveController::begin() {
    _motorL.begin(ROVER_PIN_MOTOR_L_IN1, ROVER_PIN_MOTOR_L_IN2,
                  ROVER_PWM_CHANNEL_L_FWD, ROVER_PWM_CHANNEL_L_REV);
    _motorR.begin(ROVER_PIN_MOTOR_R_IN1, ROVER_PIN_MOTOR_R_IN2,
                  ROVER_PWM_CHANNEL_R_FWD, ROVER_PWM_CHANNEL_R_REV);
    _encL.begin(ROVER_PIN_ENCODER_L_A, ROVER_PIN_ENCODER_L_B);
    _encR.begin(ROVER_PIN_ENCODER_R_A, ROVER_PIN_ENCODER_R_B);
    stop();
    _lastUpdateMs = millis();
}

void DriveController::setTarget(float velocityMps, float rotationRadPerSec) {
    // A malformed MOVE frame (eg. "velocity=nan", passes the checksum
    // check just fine since that only validates bytes, not semantics)
    // could hand strtof()'s NaN/Inf straight through. constrain() does
    // NOT clamp NaN (every comparison against NaN is false), and casting
    // a NaN/Inf float to int16_t later is undefined behavior -- so this
    // has to be caught here, before anything downstream sees it.
    if (isnan(velocityMps) || isinf(velocityMps)) velocityMps = 0.0f;
    if (isnan(rotationRadPerSec) || isinf(rotationRadPerSec)) rotationRadPerSec = 0.0f;

    _targetVelocity = constrain(velocityMps, -ROVER_MAX_WHEEL_SPEED_MPS, ROVER_MAX_WHEEL_SPEED_MPS);
    _targetRotation = constrain(rotationRadPerSec, -ROVER_MAX_ROTATION_RAD_S, ROVER_MAX_ROTATION_RAD_S);
}

void DriveController::stop() {
    _targetVelocity = 0.0f;
    _targetRotation = 0.0f;
    _pidL.reset();
    _pidR.reset();
    _motorL.setSpeed(0);
    _motorR.setSpeed(0);
}

void DriveController::updateWheel(MotorDriver& motor, Encoder& encoder, WheelPID& pid,
                                   float targetMps, float dtSeconds, float& measuredOut,
                                   float tickSign, int16_t& pwmOut) {
    long ticks = encoder.readAndResetTicks();
    measuredOut = ticksToMps(ticks, dtSeconds) * tickSign;
    int16_t pwm = pid.update(targetMps, measuredOut, dtSeconds);
    motor.setSpeed(pwm);
    pwmOut = pwm;
}

void DriveController::driveRaw(int16_t leftPwm, int16_t rightPwm, unsigned long durationMs) {
    // Bounded on purpose: an open-loop duty must not be able to outlive
    // the operator's attention (see the header).
    // 15s, raised from 5s on 2026-09-20: with this chassis needing
    // near-full duty just to creep, 5s produced too little rotation to
    // judge anything by. Still bounded -- an open-loop duty answers to
    // nothing, so it must not be able to outlive the operator's
    // attention (see the header).
    if (durationMs > 15000) durationMs = 15000;
    _rawUntilMs = millis() + durationMs;
    _motorL.setSpeed(leftPwm);
    _motorR.setSpeed(rightPwm);
}

void DriveController::update() {
    unsigned long now = millis();

    // Raw bring-up drive owns the motors while it lasts: running the
    // PID underneath would fight it for the same H-bridge.
    if (_rawUntilMs != 0) {
        if ((long)(now - _rawUntilMs) < 0) return;
        _rawUntilMs = 0;
        stop();
        return;
    }

    if (now - _lastUpdateMs < ROVER_DRIVE_UPDATE_PERIOD_MS) return;
    float dtSeconds = (now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    // Local obstacle reflex: applied here rather than in setTarget() so
    // it also holds for a target that was set BEFORE the obstacle
    // appeared, and for a stale target left behind by a dropped link.
    // Forward only -- reversing away and turning in place stay
    // available. See setForwardBlocked() for the full reasoning.
    float velocity = _targetVelocity;
    if (_forwardBlocked && velocity > 0.0f) velocity = 0.0f;

    // Unicycle model: linear velocity + rotation -> per-wheel target speed
    // (ARCHITECTURE_AND_ROADMAP.md section 12).
    float halfTrack = ROVER_WHEEL_BASE_M / 2.0f;
    float targetLeft = velocity - _targetRotation * halfTrack;
    float targetRight = velocity + _targetRotation * halfTrack;

    // Bring-up finding (2026-09-02): both wheels settle into a stable,
    // reproducible runaway (large negative measured speed while PID
    // saturates output) once commanded forward -- positive rather than
    // negative feedback, consistent with the encoder's counting direction
    // being opposite to the motor's actual drive direction on this
    // hardware. Flipped in software rather than re-swapping C1/C2 again on
    // the breadboard. Per-wheel signs (motion_config.h) since 2026-09-10:
    // a reconnect during that session's bring-up made the right wheel
    // alone runaway-spin, so its sign needed to move independently of the
    // left one -- see motion_config.h's ROVER_TICK_SIGN_* comment.
    updateWheel(_motorL, _encL, _pidL, targetLeft, dtSeconds, _measuredLeftMps,
                ROVER_TICK_SIGN_LEFT, _lastPwmLeft);
    updateWheel(_motorR, _encR, _pidR, targetRight, dtSeconds, _measuredRightMps,
                ROVER_TICK_SIGN_RIGHT, _lastPwmRight);
}

void DriveController::buildTelemetryFields(char* out, size_t outLen) const {
    // forward_blocked is reported, not just acted on: without it, the
    // local obstacle reflex (setForwardBlocked) looks from the Pi like
    // "the robot ignores my MOVE" with no way to tell it apart from a
    // dead motor, a saturated PID or a lost link -- and the Pi is now
    // deported, so nobody can watch the robot while reading the logs.
    snprintf(out, outLen, "left_speed=%.2f right_speed=%.2f left_pwm=%d right_pwm=%d forward_blocked=%d",
             _measuredLeftMps, _measuredRightMps, _lastPwmLeft, _lastPwmRight, _forwardBlocked ? 1 : 0);
}
