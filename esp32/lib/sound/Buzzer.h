#pragma once

#include <Arduino.h>
#include "sound_config.h"

// Which built-in beep pattern to play -- see Buzzer.cpp for the actual
// frequency/duration steps. Named by event, not by tune, so main.cpp's
// dispatch code reads as "what happened" rather than "which jingle".
enum class BuzzerSound {
    BOOT,
    OBSTACLE,
    LOW_BATTERY,
    ESTOP,
};

// Non-blocking buzzer: plays one short beep pattern at a time via
// tone()/noTone(), advanced from update() using millis() instead of
// delay() -- this runs from the same loop() as PID/protocol/display,
// none of which can ever block.
//
// A passive piezo buzzer plays the requested pitch; an active buzzer
// (has its own internal oscillator) just turns on/off following the
// same on/off timing and ignores the requested pitch -- both read fine
// as a rhythm-distinguishable beep pattern either way, so this doesn't
// need to know or care which kind is wired.
class Buzzer {
public:
    struct Step {
        uint16_t freqHz;  // 0 = silence (a gap between beeps), not a tone
        uint16_t durationMs;
    };

    void begin() {
        pinMode(ROVER_PIN_BUZZER, OUTPUT);
        noTone(ROVER_PIN_BUZZER);
    }

    // Starts playing immediately, interrupting whatever was already
    // playing -- there's no queue, the most recent call always wins
    // (an OBSTACLE beep re-triggering mid-pattern should restart it,
    // not wait for a full cycle to finish first).
    void play(BuzzerSound sound);

    // Non-blocking, call every loop() iteration.
    void update();

private:
    const Step* _steps = nullptr;
    uint8_t _stepCount = 0;
    uint8_t _stepIndex = 0;
    unsigned long _stepStartMs = 0;
    bool _playing = false;

    void startStep(uint8_t index);
};
