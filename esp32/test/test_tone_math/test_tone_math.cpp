#include <unity.h>

#include "ToneMath.h"

// Host-side checks of the beep generator's arithmetic (I2sBeeper feeds
// the amplifiers with it). What matters on the robot: the right pitch,
// no click between two writes, silence when asked.

void setUp() {}
void tearDown() {}

void test_phase_step_matches_pitch() {
    // 4 kHz at 16 kHz is a quarter period per sample.
    TEST_ASSERT_EQUAL_UINT32(0x40000000u, ToneMath::phaseStep(4000, 16000));
    // 1 kHz: one sixteenth.
    TEST_ASSERT_EQUAL_UINT32(0x10000000u, ToneMath::phaseStep(1000, 16000));
}

void test_phase_step_is_zero_when_unplayable() {
    TEST_ASSERT_EQUAL_UINT32(0, ToneMath::phaseStep(0, 16000));
    // At or above half the sample rate the pitch cannot be reproduced.
    TEST_ASSERT_EQUAL_UINT32(0, ToneMath::phaseStep(8000, 16000));
    TEST_ASSERT_EQUAL_UINT32(0, ToneMath::phaseStep(20000, 16000));
    TEST_ASSERT_EQUAL_UINT32(0, ToneMath::phaseStep(1000, 0));
}

void test_sample_follows_a_sine() {
    TEST_ASSERT_EQUAL_INT16(0, ToneMath::sample(0x00000000u, 32767));
    TEST_ASSERT_EQUAL_INT16(32767, ToneMath::sample(0x40000000u, 32767));
    TEST_ASSERT_EQUAL_INT16(0, ToneMath::sample(0x80000000u, 32767));
    TEST_ASSERT_EQUAL_INT16(-32767, ToneMath::sample(0xC0000000u, 32767));
}

void test_sample_scales_with_amplitude() {
    TEST_ASSERT_EQUAL_INT16(8000, ToneMath::sample(0x40000000u, 8000));
    TEST_ASSERT_EQUAL_INT16(-8000, ToneMath::sample(0xC0000000u, 8000));
    TEST_ASSERT_EQUAL_INT16(0, ToneMath::sample(0x40000000u, 0));
}

void test_fill_writes_both_channels_and_advances() {
    int16_t out[8];
    uint32_t step = ToneMath::phaseStep(4000, 16000);
    uint32_t next = ToneMath::fillStereo(out, 4, 0, step, 8000);
    const int16_t expected[8] = {0, 0, 8000, 8000, 0, 0, -8000, -8000};
    TEST_ASSERT_EQUAL_INT16_ARRAY(expected, out, 8);
    // Four quarter periods: back to the start, ready for the next write.
    TEST_ASSERT_EQUAL_UINT32(0, next);
}

void test_fill_continues_across_calls() {
    // Two writes of 2 frames must give the same samples as one of 4,
    // otherwise every write boundary would click.
    int16_t whole[8];
    int16_t parts[8];
    uint32_t step = ToneMath::phaseStep(1000, 16000);
    ToneMath::fillStereo(whole, 4, 0, step, 8000);
    uint32_t middle = ToneMath::fillStereo(parts, 2, 0, step, 8000);
    ToneMath::fillStereo(parts + 4, 2, middle, step, 8000);
    TEST_ASSERT_EQUAL_INT16_ARRAY(whole, parts, 8);
}

void test_zero_step_is_silence() {
    int16_t out[6] = {1, 1, 1, 1, 1, 1};
    uint32_t next = ToneMath::fillStereo(out, 3, 0, 0, 8000);
    const int16_t expected[6] = {0, 0, 0, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT16_ARRAY(expected, out, 6);
    TEST_ASSERT_EQUAL_UINT32(0, next);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_phase_step_matches_pitch);
    RUN_TEST(test_phase_step_is_zero_when_unplayable);
    RUN_TEST(test_sample_follows_a_sine);
    RUN_TEST(test_sample_scales_with_amplitude);
    RUN_TEST(test_fill_writes_both_channels_and_advances);
    RUN_TEST(test_fill_continues_across_calls);
    RUN_TEST(test_zero_step_is_silence);
    return UNITY_END();
}
