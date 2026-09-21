#include "WheelPID.h"
#include <Arduino.h>

void WheelPID::reset() {
    _integral = 0.0f;
    _lastMeasuredMps = 0.0f;
    _hasLastMeasured = false;
}

// Two things here are not decoration on this robot, so both are spelled
// out: a FEED-FORWARD term, and ANTI-WINDUP on the integral.
//
// Feed-forward (_kff * target)
// ----------------------------
// The duty a wheel needs to hold a given speed is roughly proportional
// to that speed, and it is known in advance -- there is no reason to
// make the integrator rediscover it one 20ms step at a time. Without
// it, the integral IS the only path to a usable duty, and on this
// chassis (which needs 200+ PWM just to break away) the measured cost
// was ~5s of held joystick before anything moved at 40% of the speed
// bar, and ~10s at 20%. That delay is indistinguishable from a dead
// motor on a short test, which is exactly how it has been read.
//
// Anti-windup (conditional integration)
// -------------------------------------
// The integral is only allowed to grow while the output is NOT already
// pinned against its rail in the same direction. Without that guard,
// an unreachable setpoint -- which is the normal case here, see
// ROVER_MAX_WHEEL_SPEED_MPS -- parks the integral at INTEGRAL_LIMIT,
// where it becomes stored energy nobody asked for: measured on the
// native bench, driving at 0.12 m/s for 5s and then centring the
// joystick left the wheel at full duty for **81 more seconds**. The
// robot does not stop when you let go. That has been invisible only
// because the wheels have not been turning; it would have been the
// first thing to bite the day they did.
//
// Note stop() (and therefore SAFE, the E-stop and the heartbeat
// timeout) calls reset() and clears the integral outright -- those
// paths were never affected. It is the ordinary "centre the stick"
// case, which only sets the target to zero, that this fixes.
int16_t WheelPID::update(float targetMps, float measuredMps, float dtSeconds) {
    if (dtSeconds <= 0.0f) return 0;

    float error = targetMps - measuredMps;

    // "Stop" is an intention, not a setpoint to servo towards. A
    // commanded zero means the operator let go, so whatever bias the
    // integrator had accumulated to hold the previous speed is not
    // evidence about anything any more -- it is just stored energy that
    // would keep the wheel turning while it bleeds off. Measured on the
    // native bench with the feed-forward disabled: 39s of continued
    // driving after centring the stick. Anti-windup alone does NOT fix
    // that (the integral is not against a rail on the way down, it
    // simply has a long way to fall), so this is the guard that makes
    // "let go and it stops" true under ANY tuning, including kff=0.
    //
    // Epsilon rather than == 0: a centred stick sends "0.00", which
    // parses to exactly 0.0f, but nothing downstream should depend on
    // that. 1e-4 m/s is 0.1mm/s -- three orders of magnitude below the
    // slowest speed this robot can command.
    if (targetMps > -ZERO_TARGET_EPS && targetMps < ZERO_TARGET_EPS) {
        _integral = 0.0f;
    }

    // Derivative on measurement, not on error: a new MOVE target changes
    // `error` instantly, which would otherwise spike the derivative term
    // every time the Pi sends a command ("derivative kick") even though
    // the wheel itself hasn't moved yet. Skipped on the very first sample
    // since there's no previous measurement to compare against.
    float derivative = 0.0f;
    if (_hasLastMeasured) {
        derivative = -(measuredMps - _lastMeasuredMps) / dtSeconds;
    }
    _lastMeasuredMps = measuredMps;
    _hasLastMeasured = true;

    // Computed on a CANDIDATE integral, so the decision below can be
    // made on the output this step would actually produce.
    float candidateIntegral = constrain(_integral + error * dtSeconds,
                                        -INTEGRAL_LIMIT, INTEGRAL_LIMIT);
    float output = _kff * targetMps + _kp * error + _ki * candidateIntegral + _kd * derivative;

    // Keep the new integral unless it would wind further into a rail the
    // output is already against. When the error reverses -- the operator
    // lets go, or the wheel finally catches up -- integration resumes on
    // the very next step, with no stored bias to burn off first.
    bool saturated = (output > 255.0f) || (output < -255.0f);
    bool pushingFurther = (output > 0.0f) == (error > 0.0f);
    if (!(saturated && pushingFurther)) {
        _integral = candidateIntegral;
    }

    return (int16_t)constrain(output, -255.0f, 255.0f);
}

