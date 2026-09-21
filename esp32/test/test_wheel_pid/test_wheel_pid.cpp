#include <unity.h>
#include "WheelPID.h"
#include "motion_config.h"

void setUp(void) {}
void tearDown(void) {}

void test_zero_error_gives_zero_output(void) {
    WheelPID pid;
    TEST_ASSERT_EQUAL_INT16(0, pid.update(0.0f, 0.0f, 0.02f));
}

void test_positive_error_drives_positive_output(void) {
    WheelPID pid;
    TEST_ASSERT_GREATER_THAN_INT16(0, pid.update(0.5f, 0.0f, 0.02f));
}

void test_negative_error_drives_negative_output(void) {
    WheelPID pid;
    TEST_ASSERT_LESS_THAN_INT16(0, pid.update(-0.5f, 0.0f, 0.02f));
}

// MotorDriver::setSpeed expects +/-255; a huge target error must
// saturate there, never overflow int16_t or exceed the documented range.
void test_output_is_clamped_to_pwm_range(void) {
    WheelPID pid;
    int16_t out = pid.update(1000.0f, 0.0f, 0.02f);
    TEST_ASSERT_LESS_OR_EQUAL_INT16(255, out);
    out = pid.update(-1000.0f, 0.0f, 0.02f);
    TEST_ASSERT_GREATER_OR_EQUAL_INT16(-255, out);
}

// dtSeconds<=0 (eg. a stale/repeated millis() read) must be a safe no-op,
// not a division-by-zero or a garbage derivative term.
void test_non_positive_dt_returns_zero(void) {
    WheelPID pid;
    TEST_ASSERT_EQUAL_INT16(0, pid.update(0.5f, 0.0f, 0.0f));
    TEST_ASSERT_EQUAL_INT16(0, pid.update(0.5f, 0.0f, -0.01f));
}

// reset() must clear accumulated integral windup -- otherwise a wheel
// commanded to speed then to stop would carry a residual push from the
// old target instead of actually reaching zero output at zero error.
void test_reset_clears_integral_windup(void) {
    WheelPID pid;
    for (int i = 0; i < 50; i++) pid.update(1.0f, 0.0f, 0.02f);
    pid.reset();
    TEST_ASSERT_EQUAL_INT16(0, pid.update(0.0f, 0.0f, 0.02f));
}


// --- Feed-forward and anti-windup (2026-09-21) -----------------------
//
// These three cover the pathologies that made a working control loop
// look like dead motors. See WheelPID::update() for the full reasoning
// and the measured numbers.

// The duty needed for a given speed is known in advance, so a full-scale
// command must produce a usable duty on the FIRST step -- not after the
// integrator has spent seconds rediscovering it.
void test_feed_forward_gives_usable_duty_immediately(void) {
    WheelPID pid;
    int16_t first = pid.update(ROVER_MAX_WHEEL_SPEED_MPS, 0.0f, 0.02f);
    TEST_ASSERT_GREATER_OR_EQUAL_INT16(200, first);
}

// Centring the joystick only sets the target to zero -- it does not go
// through reset() the way STOP/SAFE/E-stop do. Without clearing the
// integral here, the bench measured 39 SECONDS of continued driving
// after letting go. "Let go and it stops" must hold under any tuning.
void test_zero_target_stops_immediately_after_a_long_drive(void) {
    WheelPID pid;
    // Drive hard for 5s against a wheel that cannot keep up, which is
    // the condition that builds the bias in the first place.
    for (int i = 0; i < 250; i++) pid.update(ROVER_MAX_WHEEL_SPEED_MPS, 0.005f, 0.02f);
    int16_t afterRelease = pid.update(0.0f, 0.005f, 0.02f);
    TEST_ASSERT_LESS_OR_EQUAL_INT16(0, afterRelease);
}

// An unreachable setpoint -- the normal case on this chassis until the
// real top speed is measured -- must not let the integral accumulate a
// bias that then has to be burned off before the loop responds again.
void test_integral_does_not_wind_up_while_saturated(void) {
    WheelPID pid;
    for (int i = 0; i < 500; i++) pid.update(10.0f, 0.0f, 0.02f);  // 10 s, hopeless target
    // Now ask for something modest. A wound-up integral would hold the
    // output pinned at 255 for seconds; a guarded one tracks at once.
    int16_t out = pid.update(ROVER_MAX_WHEEL_SPEED_MPS * 0.2f, 0.0f, 0.02f);
    TEST_ASSERT_LESS_THAN_INT16(255, out);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_error_gives_zero_output);
    RUN_TEST(test_positive_error_drives_positive_output);
    RUN_TEST(test_negative_error_drives_negative_output);
    RUN_TEST(test_output_is_clamped_to_pwm_range);
    RUN_TEST(test_non_positive_dt_returns_zero);
    RUN_TEST(test_reset_clears_integral_windup);
    RUN_TEST(test_feed_forward_gives_usable_duty_immediately);
    RUN_TEST(test_zero_target_stops_immediately_after_a_long_drive);
    RUN_TEST(test_integral_does_not_wind_up_while_saturated);
    return UNITY_END();
}
