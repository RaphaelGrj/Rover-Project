#include <unity.h>
#include "WheelPID.h"

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

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_error_gives_zero_output);
    RUN_TEST(test_positive_error_drives_positive_output);
    RUN_TEST(test_negative_error_drives_negative_output);
    RUN_TEST(test_output_is_clamped_to_pwm_range);
    RUN_TEST(test_non_positive_dt_returns_zero);
    RUN_TEST(test_reset_clears_integral_windup);
    return UNITY_END();
}
