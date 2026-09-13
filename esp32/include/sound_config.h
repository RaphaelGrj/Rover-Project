#pragma once

// Piezo buzzer (esp32/lib/sound/Buzzer.h). GPIO12 chosen deliberately
// -- see WIRING.md "Pins évitées volontairement" for why this deviates
// from the project's earlier "never use GPIO12" rule (strapping pin,
// flash voltage selection risk at boot): explicitly accepted by the
// user (2026-08-31) since every other GPIO on the WROOM devkit was
// already spoken for. A passive piezo buzzer's high impedance rarely
// pulls a strapping pin hard enough during the boot sampling window to
// matter in practice -- watch the boot log after wiring this for real,
// this pin is the first suspect if boot ever looks wrong.
constexpr int ROVER_PIN_BUZZER = 12;

// I2S audio amplifier (MAX98357A). Pins confirmed with the user on
// 2026-09-13 -- see WIRING.md for why GPIO16/17 (originally reserved
// for a UART2 link to the Pi, never actually used since the real link
// is USB) and GPIO14 (previously the unwired battery ADC placeholder,
// see power_config.h) were the only GPIOs left to reclaim on the WROOM.
// Driver/playback code not written yet -- pins only, for wiring.
constexpr int ROVER_PIN_I2S_BCLK = 16;
constexpr int ROVER_PIN_I2S_LRC = 17;
constexpr int ROVER_PIN_I2S_DIN = 14;
