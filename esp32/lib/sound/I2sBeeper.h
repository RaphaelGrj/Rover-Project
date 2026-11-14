#pragma once

#include <Arduino.h>
#include "sound_config.h"

// Beeps through the I2S amplifiers, for boards that have speakers but no
// buzzer (the custom PCB: two MAX98357A on one I2S data line). Used only
// as a back end of Buzzer (Buzzer.cpp), which keeps the patterns and the
// timing: this module only knows "play this pitch", "be quiet", "done".
//
// Everything is non-blocking: update() tops up the I2S buffer with
// whatever fits and returns. If the main loop stalls longer than the
// buffer lasts (~100 ms), the driver plays silence by itself and the
// beep has a gap -- never a stuck tone.
//
// The amplifiers are shut down between beeps (BoardLine::AmpEnable):
// they hiss slightly when idle, and they draw current for nothing.
//
// On boards without I2S beeps every function is an empty inline, so
// callers need no #if.
namespace I2sBeeper {

#if defined(ROVER_BOARD_PCB_V1)
// Installs the I2S driver. Returns false (and stays inert) if it fails.
bool begin();
// Starts or changes the pitch; wakes the amplifiers if needed.
void tone(uint16_t freqHz);
// A gap inside a pattern: no sound, amplifiers kept awake.
void silence();
// End of a pattern: no sound, amplifiers shut down shortly after.
void stop();
// Call every loop() iteration.
void update();
// False if begin() failed or was never called.
bool ready();
#else
inline bool begin() { return false; }
inline void tone(uint16_t) {}
inline void silence() {}
inline void stop() {}
inline void update() {}
inline bool ready() { return false; }
#endif

}  // namespace I2sBeeper
