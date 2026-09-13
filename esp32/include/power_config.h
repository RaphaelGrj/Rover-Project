#pragma once

#include <cstdint>

// Battery voltage monitoring via a resistor divider into an ADC pin.
// PROVISIONAL and DISABLED BY DEFAULT (see ROVER_BATTERY_MONITORING_ENABLED
// below): no divider hardware exists yet (BOM.md, "Alimentation" -- pas
// conçue). Unlike I2C sensors (esp32/lib/sensors/I2CProbe.h), a plain
// ADC pin has no way to detect "nothing is actually wired here" -- an
// unconnected pin floats and reads noise, which would be actively
// misleading if reported as a real battery percentage. Flip the flag
// below to true only once the divider is physically wired and the
// constants below are measured/calibrated against it.
constexpr bool ROVER_BATTERY_MONITORING_ENABLED = false;

// GPIO14 was the placeholder pin here, but it was reassigned to the I2S
// audio amplifier's DIN line on 2026-09-13 (see sound_config.h,
// WIRING.md) -- no divider was ever actually wired to it, so nothing
// physical changes. Battery monitoring stays disabled below; if it's
// ever revived, this needs a free ADC1 pin (ADC2 pins like GPIO14 can't
// be read while WiFi is active, which conflicts with OTA anyway). No
// GPIO is currently free on the WROOM -- one would need to be reclaimed
// first, same situation the ampli was just in.
constexpr int ROVER_PIN_BATTERY_ADC = -1;  // unassigned

// TODO(hardware): measure against the real divider once it exists --
// these are placeholders, not derived from any actual resistor values.
// Assumes a 2S Li-ion/LiPo pack (7.4V nominal) and a divider that roughly
// halves it into the ESP32's 0-3.3V ADC range.
constexpr float ROVER_BATTERY_DIVIDER_RATIO = 2.0f;  // Vbatt = Vadc * ratio
constexpr float ROVER_BATTERY_EMPTY_V = 6.0f;        // maps to 0%
constexpr float ROVER_BATTERY_FULL_V = 8.4f;         // maps to 100%

constexpr unsigned long ROVER_BATTERY_UPDATE_PERIOD_MS = 2000;

// Hysteresis, same reasoning as the obstacle-detection thresholds
// (sensors_config.h) -- avoids EVENT low_battery chattering right at
// one fixed cutoff.
constexpr uint8_t ROVER_BATTERY_LOW_PERCENT = 20;
constexpr uint8_t ROVER_BATTERY_LOW_CLEAR_PERCENT = 30;
