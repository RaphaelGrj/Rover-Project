#pragma once

#include <cstdint>

// Battery monitoring. Two very different sources depending on the board:
//
// - WROOM devkit: a resistor divider into an ADC pin. PROVISIONAL and
//   DISABLED BY DEFAULT: no divider hardware exists yet (BOM.md,
//   "Alimentation"). Unlike I2C sensors (esp32/lib/sensors/I2CProbe.h),
//   a plain ADC pin has no way to detect "nothing is actually wired
//   here" -- an unconnected pin floats and reads noise, which would be
//   actively misleading if reported as a real battery percentage.
//
// - Custom PCB (ROVER_BOARD_PCB_V1): an INA219 on the I2C bus measures
//   pack voltage AND current (pcb/PCB_CONNEXIONS.md section 5.1). It
//   can be probed, so it is enabled by default: absent chip = no
//   reading and an ERROR, never a made-up percentage.
#if defined(ROVER_BOARD_PCB_V1)
constexpr bool ROVER_BATTERY_MONITORING_ENABLED = true;
constexpr bool ROVER_BATTERY_USES_INA219 = true;
#else
constexpr bool ROVER_BATTERY_MONITORING_ENABLED = false;
constexpr bool ROVER_BATTERY_USES_INA219 = false;
#endif

// GPIO14 was the placeholder pin here, but it was reassigned to the I2S
// audio amplifier's DIN line on 2026-09-13 (see sound_config.h,
// WIRING.md) -- no divider was ever actually wired to it, so nothing
// physical changes. Battery monitoring stays disabled on the WROOM; if
// it's ever revived, this needs a free ADC1 pin (ADC2 pins like GPIO14
// can't be read while WiFi is active, which conflicts with OTA anyway).
// No GPIO is currently free on the WROOM -- one would need to be
// reclaimed first, same situation the ampli was just in. The PCB has
// no ADC pin for this at all: the INA219 replaces it.
constexpr int ROVER_PIN_BATTERY_ADC = -1;  // unassigned

// TODO(hardware): measure against the real divider once it exists --
// these are placeholders, not derived from any actual resistor values.
// Assumes a 2S Li-ion/LiPo pack (7.4V nominal) and a divider that roughly
// halves it into the ESP32's 0-3.3V ADC range.
constexpr float ROVER_BATTERY_DIVIDER_RATIO = 2.0f;  // Vbatt = Vadc * ratio

// Pack voltages mapped to 0% / 100% (linear estimate, see
// PowerMath::batteryPercentFromMv for its limits). 2 x 18650 in series:
// 3.2 V/cell is "empty enough" -- well above the protection IC's ~2.8 V
// cut-off, so the robot stops itself long before the hardware has to --
// and 4.15 V/cell is "full" since a resting cell drifts below the
// 4.20 V the charger ends at.
constexpr uint32_t ROVER_BATTERY_EMPTY_MV = 6400;  // maps to 0%
constexpr uint32_t ROVER_BATTERY_FULL_MV = 8300;   // maps to 100%

constexpr unsigned long ROVER_BATTERY_UPDATE_PERIOD_MS = 2000;
// How often a missing INA219 is probed again, same spirit as
// ROVER_SENSOR_RETRY_PERIOD_MS for the Phase 4 sensors.
constexpr unsigned long ROVER_BATTERY_RETRY_PERIOD_MS = 5000;

// Hysteresis, same reasoning as the obstacle-detection thresholds
// (sensors_config.h) -- avoids EVENT low_battery chattering right at
// one fixed cutoff.
constexpr uint8_t ROVER_BATTERY_LOW_PERCENT = 20;
constexpr uint8_t ROVER_BATTERY_LOW_CLEAR_PERCENT = 30;

// INA219 shunt, in milliohms: 0.05 ohm 2512 on the PCB. The current is
// computed from the shunt voltage with THIS value, so a different
// resistor fitted on the board only needs this line changed.
constexpr uint16_t ROVER_INA219_SHUNT_MILLIOHMS = 50;

// --- BQ25887 charger (custom PCB only) ----------------------------------
#if defined(ROVER_BOARD_PCB_V1)
constexpr bool ROVER_CHARGER_ENABLED = true;
#else
constexpr bool ROVER_CHARGER_ENABLED = false;
#endif

// 1 A into 2 x 3500 mAh 18650 is ~0.3C: gentle on the cells, about 4 h
// for a full charge, and ~2 A drawn from the USB-C at 5 V -- which any
// USB-C charger provides. Raising it shortens the charge but pulls more
// than many USB ports can give (pcb/PCB_CONNEXIONS.md section 5.1).
constexpr uint16_t ROVER_CHARGE_CURRENT_MA = 1000;
// USB input current ceiling. The board's ILIM resistor sets a hardware
// cap on top of this; the chip's own input-voltage regulation (VINDPM)
// backs off by itself on a weak port.
constexpr uint16_t ROVER_USB_INPUT_LIMIT_MA = 2000;
// Standard Li-ion end-of-charge voltage. Written explicitly even though
// it is the chip's reset value, so the intent is visible in code and a
// chip that somehow came up configured otherwise gets corrected.
constexpr uint16_t ROVER_CELL_CHARGE_MV = 4200;

// Cell-level thresholds, applied only when the two cell readings are
// plausible (PowerMath::cellsPlausible -- the ADC scale is unverified).
// LOW warns; CRITICAL stops the robot (SAFE) well before the protection
// IC disconnects the pack at ~2.8 V; OVERVOLTAGE disables charging as a
// firmware layer under the protection IC's own ~4.28 V cut-off.
constexpr uint16_t ROVER_CELL_LOW_MV = 3300;
constexpr uint16_t ROVER_CELL_LOW_CLEAR_MV = 3450;
constexpr uint16_t ROVER_CELL_CRITICAL_MV = 3100;
constexpr uint16_t ROVER_CELL_OVERVOLTAGE_MV = 4250;
// Max disagreement between (top cell + bottom cell) and the INA219's
// pack voltage. Covers ADC tolerances and the small drop across the
// shunt and protection MOSFETs; a wrong scale or a broken balance wire
// is off by volts, not by this.
constexpr uint16_t ROVER_CELL_SUM_TOLERANCE_MV = 150;

constexpr unsigned long ROVER_CHARGER_UPDATE_PERIOD_MS = 2000;
constexpr unsigned long ROVER_CHARGER_RETRY_PERIOD_MS = 5000;
