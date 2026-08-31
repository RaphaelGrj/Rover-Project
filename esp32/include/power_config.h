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

// GPIO14 (freed up alongside GPIO25, see motion_config.h's note on the
// TB6612FNG->DRV8833 switch) -- it's an ADC2 pin, which the ESP32
// cannot read while WiFi is active. That's a real conflict with OTA
// (esp32/lib/ota, WiFi-based): if both battery monitoring and OTA end
// up wanted at the same time, this needs to move to a free ADC1 pin
// instead (GPIO32-39 are all already spoken for by motors/encoders
// today, so that would mean freeing one up first).
constexpr int ROVER_PIN_BATTERY_ADC = 14;

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
