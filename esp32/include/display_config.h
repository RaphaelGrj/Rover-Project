#pragma once

#include <cstdint>

// Phase 3 display pinout and geometry (WIRING.md).
//
// PROVISIONAL, same caveats as motion_config.h: not confirmed against
// real hardware, exists so Phase 3 can be built and validated in Wokwi
// while waiting for the real panel/wiring. 240x240 matches Wokwi's
// "board-st7789" simulated part exactly (confirmed by its own boot log,
// "display 240x240") -- Rover's actual panel hasn't been chosen/wired
// yet and may well differ once it is (some ST7789 boards are 240x280
// or 135x240 instead, like the one on the sibling Lumi project).

constexpr int ROVER_PIN_DISPLAY_SCLK = 18;
constexpr int ROVER_PIN_DISPLAY_MOSI = 23;
constexpr int ROVER_PIN_DISPLAY_CS = 5;
constexpr int ROVER_PIN_DISPLAY_DC = 2;   // strapping pin, see WIRING.md
constexpr int ROVER_PIN_DISPLAY_RST = 15; // strapping pin, see WIRING.md
// BLK (backlight) is hardwired to 3V3 in this base wiring -- no GPIO.

constexpr int ROVER_DISPLAY_WIDTH = 240;
constexpr int ROVER_DISPLAY_HEIGHT = 240;
constexpr uint32_t ROVER_DISPLAY_SPI_HZ = 40000000;

// How often DisplayEngine::update() re-evaluates blink/look/animation
// state and redraws.
constexpr unsigned long ROVER_DISPLAY_UPDATE_PERIOD_MS = 33;  // ~30 FPS
