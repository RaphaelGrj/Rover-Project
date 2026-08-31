#pragma once

// Physical emergency-stop button. PROVISIONAL: no button is wired yet
// (see PROGRESS.md) -- GPIO25 was freed up when the DRV8833 replaced
// the TB6612FNG plan (motion_config.h, "anciennement PWMA/PWMB").
//
// Uses the ESP32's internal pull-up (EStop::begin() -> INPUT_PULLUP),
// so an unconnected pin reads a stable HIGH ("released") instead of
// floating -- the robot must work exactly the same with or without a
// button physically wired, same "hardware optional" principle already
// used for Phase 4 sensors (see esp32/lib/sensors/I2CProbe.h), applied
// here to a GPIO input instead of an I2C bus. Wire the button between
// this pin and GND; pressed = pin pulled LOW.
constexpr int ROVER_PIN_ESTOP = 25;

// Debounce window: a mechanical button bouncing between HIGH/LOW for a
// few ms around a press/release must not read as multiple rapid
// press/release cycles.
constexpr unsigned long ROVER_ESTOP_DEBOUNCE_MS = 30;
