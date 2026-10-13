#pragma once

// BUZZER DISABLED 2026-09-20 -- its pin (GPIO12) was handed to the yaw
// servo, and there is no GPIO left to move it to.
//
// Why: GPIO19 (the yaw servo's pin since Phase 3) turns out to emit
// nothing on this devkit. Verified by elimination on real hardware --
// both servos work on GPIO13, neither works on GPIO19, and the LEDC
// channel was swapped (5 -> 6, different timer) without change. The
// firmware attaches the channel correctly (head_b_att=1), so the pad
// itself is dead or unbonded. Driving the head matters more than a
// beep, and the user chose to give up the buzzer (2026-09-20).
//
// What is lost: boot / obstacle / low-battery / E-stop beeps. The one
// that stings is standalone mode, which beeped to explain why no
// "Rover-Pilot" AP appeared when no password was stored -- on a robot
// with no Pi attached, that diagnostic is now silent.
//
// The code is kept, not deleted: the buzzer comes straight back by
// setting this to true and giving it a pin, if one is ever freed (the
// deported Pi frees GPIO1, and GPIO3 is earmarked for the microphone).
constexpr bool ROVER_BUZZER_ENABLED = false;
constexpr int ROVER_PIN_BUZZER = -1;  // no pin assigned while disabled
// The custom PCB has no buzzer at all: its two speakers replace it, and
// a beep will be an I2S sound once the audio driver exists.

// I2S audio amplifier (MAX98357A). Pins confirmed with the user on
// 2026-09-13 -- see WIRING.md for why GPIO16/17 (originally reserved
// for a UART2 link to the Pi, never actually used since the real link
// is USB) and GPIO14 (previously the unwired battery ADC placeholder,
// see power_config.h) were the only GPIOs left to reclaim on the WROOM.
// Driver/playback code not written yet -- pins only, for wiring.
#if defined(ROVER_BOARD_PCB_V1)
// Custom PCB: two MAX98357A (left/right channel picked by their
// SD_MODE resistors) on one DIN line, plus the ICS-43434 mic on its own
// data line -- board_pins_pcb_v1.h. Both amps stay shut down
// (BoardLine::AmpEnable low) until an audio driver turns them on, so a
// board without one is silent rather than hissing.
#include "board_pins_pcb_v1.h"
#else
constexpr int ROVER_PIN_I2S_BCLK = 16;
constexpr int ROVER_PIN_I2S_LRC = 17;
constexpr int ROVER_PIN_I2S_DIN = 14;
// Planned on GPIO3 with a jumper (WIRING.md "Micro I2S"), not wired yet.
constexpr int ROVER_PIN_I2S_MIC_SD = -1;
#endif  // ROVER_BOARD_PCB_V1
