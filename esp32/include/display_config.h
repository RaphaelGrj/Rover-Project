#pragma once

#include <cstdint>

// Phase 3 display pinout and geometry (WIRING.md).
//
// Pin assignment is still provisional (same caveats as motion_config.h),
// but the panel itself is now confirmed on real hardware: a 240x280
// ST7789 (same as the sibling Lumi project), native orientation
// portrait. Wokwi's "board-st7789" simulated part is fixed 240x240, so
// it no longer matches this exactly -- fine for protocol-level
// simulation (dispatch only), but the visual layout below is sized for
// the real panel, not Wokwi's.
//
// Rover is meant to run landscape, so the panel is rotated 90 degrees
// in software (ROVER_DISPLAY_ROTATION) rather than physically -- the
// WIDTH/HEIGHT constants below are the LOGICAL (post-rotation)
// dimensions that EyeRenderer/DisplayEngine lay out against, while
// ROVER_DISPLAY_PANEL_WIDTH/HEIGHT are the native (pre-rotation) values
// passed to Adafruit_ST7789::init().

constexpr int ROVER_PIN_DISPLAY_SCLK = 18;
constexpr int ROVER_PIN_DISPLAY_MOSI = 23;
constexpr int ROVER_PIN_DISPLAY_CS = 5;
constexpr int ROVER_PIN_DISPLAY_DC = 2;   // strapping pin, see WIRING.md
constexpr int ROVER_PIN_DISPLAY_RST = 15; // strapping pin, see WIRING.md
// BLK (backlight) is hardwired to 3V3 in this base wiring -- no GPIO.

constexpr int ROVER_DISPLAY_PANEL_WIDTH = 240;   // native panel, portrait
constexpr int ROVER_DISPLAY_PANEL_HEIGHT = 280;
// 1 = 90 degrees clockwise from native. If the image comes out
// upside-down or mirrored on the real panel, try 3 (90 degrees the
// other way) instead -- this is the one value that depends on which
// physical edge the panel's ribbon/pins are wired from, which varies
// by board and isn't something software can infer.
constexpr uint8_t ROVER_DISPLAY_ROTATION = 1;
constexpr int ROVER_DISPLAY_WIDTH = ROVER_DISPLAY_PANEL_HEIGHT;   // logical, landscape
constexpr int ROVER_DISPLAY_HEIGHT = ROVER_DISPLAY_PANEL_WIDTH;
constexpr uint32_t ROVER_DISPLAY_SPI_HZ = 40000000;

// How often DisplayEngine::update() re-evaluates blink/look/animation
// state and redraws.
constexpr unsigned long ROVER_DISPLAY_UPDATE_PERIOD_MS = 33;  // ~30 FPS
