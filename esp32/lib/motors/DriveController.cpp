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
    _targetVelocity = constrain(velocityMps, -ROVER_MAX_WHEEL_SPEED_MPS, ROVER_MAX_WHEEL_SPEED_MPS);
    _targetRotation = rotationRadPerSec;
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
                                   float targetMps, float dtSeconds, float& measuredOut) {
    long ticks = encoder.readAndResetTicks();
    measuredOut = ticksToMps(ticks, dtSeconds);
    int16_t pwm = pid.update(targetMps, measuredOut, dtSeconds);
    motor.setSpeed(pwm);
}

void DriveController::update() {
    unsigned long now = millis();
    if (now - _lastUpdateMs < ROVER_DRIVE_UPDATE_PERIOD_MS) return;
    float dtSeconds = (now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    // Unicycle model: linear velocity + rotation -> per-wheel target speed
    // (ARCHITECTURE_AND_ROADMAP.md section 12).
    float halfTrack = ROVER_WHEEL_BASE_M / 2.0f;
    float targetLeft = _targetVelocity - _targetRotation * halfTrack;
    float targetRight = _targetVelocity + _targetRotation * halfTrack;

    updateWheel(_motorL, _encL, _pidL, targetLeft, dtSeconds, _measuredLeftMps);
    updateWheel(_motorR, _encR, _pidR, targetRight, dtSeconds, _measuredRightMps);
}

void DriveController::buildTelemetryFields(char* out, size_t outLen) const {
    snprintf(out, outLen, "left_speed=%.2f right_speed=%.2f",
             _measuredLeftMps, _measuredRightMps);
}
