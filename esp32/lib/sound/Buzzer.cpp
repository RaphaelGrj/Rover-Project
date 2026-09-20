#include "Buzzer.h"

namespace {

// Short, deliberately non-musical patterns -- distinguishable by
// rhythm/pitch alone, without needing to look at the screen.
constexpr Buzzer::Step BOOT_STEPS[] = {{1000, 60}, {0, 30}, {1500, 80}};
constexpr Buzzer::Step OBSTACLE_STEPS[] = {{2000, 40}, {0, 40}};
constexpr Buzzer::Step LOW_BATTERY_STEPS[] = {
    {600, 100}, {0, 100}, {600, 100}, {0, 100}, {600, 100}};
constexpr Buzzer::Step ESTOP_STEPS[] = {{2500, 300}};

}  // namespace

void Buzzer::play(BuzzerSound sound) {
    // Single gate for the whole class: with the buzzer disabled its pin
    // belongs to the yaw servo (sound_config.h), and tone() on a pin
    // driven by LEDC would fight the servo for the pad. Refusing here
    // rather than at every call site keeps main.cpp free of
    // "if (buzzer enabled)" noise -- play() simply becomes a no-op.
    if (!ROVER_BUZZER_ENABLED) return;

    switch (sound) {
        case BuzzerSound::BOOT:
            _steps = BOOT_STEPS;
            _stepCount = sizeof(BOOT_STEPS) / sizeof(Step);
            break;
        case BuzzerSound::OBSTACLE:
            _steps = OBSTACLE_STEPS;
            _stepCount = sizeof(OBSTACLE_STEPS) / sizeof(Step);
            break;
        case BuzzerSound::LOW_BATTERY:
            _steps = LOW_BATTERY_STEPS;
            _stepCount = sizeof(LOW_BATTERY_STEPS) / sizeof(Step);
            break;
        case BuzzerSound::ESTOP:
            _steps = ESTOP_STEPS;
            _stepCount = sizeof(ESTOP_STEPS) / sizeof(Step);
            break;
    }
    startStep(0);
}

void Buzzer::startStep(uint8_t index) {
    _stepIndex = index;
    _playing = true;
    _stepStartMs = millis();
    const Step& step = _steps[index];
    if (step.freqHz > 0) {
        tone(ROVER_PIN_BUZZER, step.freqHz);
    } else {
        noTone(ROVER_PIN_BUZZER);
    }
}

void Buzzer::update() {
    if (!_playing) return;
    const Step& step = _steps[_stepIndex];
    if (millis() - _stepStartMs < step.durationMs) return;

    uint8_t next = _stepIndex + 1;
    if (next >= _stepCount) {
        noTone(ROVER_PIN_BUZZER);
        _playing = false;
        return;
    }
    startStep(next);
}
