#pragma once

#include <stdint.h>
#include <stddef.h>

// Pure tone-generation arithmetic for the I2S beeper (lib/sound/
// I2sBeeper.*). Kept free of any hardware call so it can be unit-tested
// on the host (test/test_tone_math), like PowerMath.h for the charger.
namespace ToneMath {

// One full sine period, 64 points, full 16-bit scale.
constexpr int16_t SINE_TABLE[64] = {
         0,   3212,   6393,   9512,  12539,  15446,  18204,  20787,
     23170,  25329,  27245,  28898,  30273,  31356,  32137,  32609,
     32767,  32609,  32137,  31356,  30273,  28898,  27245,  25329,
     23170,  20787,  18204,  15446,  12539,   9512,   6393,   3212,
         0,  -3212,  -6393,  -9512, -12539, -15446, -18204, -20787,
    -23170, -25329, -27245, -28898, -30273, -31356, -32137, -32609,
    -32767, -32609, -32137, -31356, -30273, -28898, -27245, -25329,
    -23170, -20787, -18204, -15446, -12539,  -9512,  -6393,  -3212,
};

// The phase is a 32-bit counter where 2^32 is one full period, so it
// wraps by itself. This is how much it advances per sample for a given
// pitch; 0 Hz (or a pitch at/above half the sample rate, which could
// not be reproduced) gives 0, i.e. silence.
inline uint32_t phaseStep(uint16_t freqHz, uint32_t sampleRateHz) {
    if (sampleRateHz == 0 || (uint32_t)freqHz * 2 >= sampleRateHz) return 0;
    return (uint32_t)(((uint64_t)freqHz << 32) / sampleRateHz);
}

// Sample for a phase, scaled to `amplitude` (0..32767). The top 6 bits
// of the phase index the table; no interpolation, a beep does not need it.
inline int16_t sample(uint32_t phase, int16_t amplitude) {
    return (int16_t)(((int32_t)SINE_TABLE[phase >> 26] * amplitude) / 32767);
}

// Fills `frames` stereo frames (left, right interleaved, same value on
// both: each amplifier picks its own channel) and returns the phase to
// continue from, so consecutive calls join without a click.
inline uint32_t fillStereo(int16_t* out, size_t frames, uint32_t phase, uint32_t step, int16_t amplitude) {
    for (size_t i = 0; i < frames; i++) {
        int16_t s = sample(phase, amplitude);
        out[2 * i] = s;
        out[2 * i + 1] = s;
        phase += step;
    }
    return phase;
}

}  // namespace ToneMath
