#pragma once

#include <cstdint>

// Phase 4 sensor pinout (WIRING.md) and tuning constants.
//
// Pin assignment is provisional like the rest of the project's wiring
// (motion_config.h, head_config.h, display_config.h): not confirmed on
// a fully-wired robot. As of this writing only the distance sensor and
// wiring terminals are en route (see PROGRESS.md 2026-08-31) -- IMU and
// BME688 addresses/behavior below are the library defaults, unconfirmed
// against real hardware.

constexpr int ROVER_PIN_I2C_SDA = 21;
constexpr int ROVER_PIN_I2C_SCL = 22;

constexpr int ROVER_PIN_TOF_LEFT_XSHUT = 0;  // strapping pin, see WIRING.md
constexpr int ROVER_PIN_TOF_RIGHT_XSHUT = 4;

// Both VL53L0X boot on the same factory I2C address; the left sensor is
// re-addressed during begin() so it can coexist with the right one,
// which stays on the default (WIRING.md, "les deux VL53L0X partagent la
// même adresse I2C").
constexpr uint8_t ROVER_TOF_DEFAULT_ADDRESS = 0x29;
constexpr uint8_t ROVER_TOF_LEFT_ADDRESS = 0x30;

// I2C address shared by the BME280 currently wired and a future BME688
// swap-in (EnvironmentSensor auto-detects which one via chip-id, see
// EnvironmentSensor.h). 2026-09-02: current board has no SDO pin,
// address fixed on the board at 0x76 (confirmed via the purchase link
// after chip-id 0x60 turned up instead of the BME688's 0x61) -- if a
// future BME688 lands on 0x77 instead (SDO tied high), this needs
// updating too, chip-id detection alone won't find it at the wrong
// address.
constexpr uint8_t ROVER_ENV_SENSOR_ADDRESS = 0x76;

// MPU6050 factory default (AD0 pin low); adjust if the real wiring ties
// AD0 high instead (0x69).
constexpr uint8_t ROVER_MPU6050_ADDRESS = 0x68;

// How often each sensor is sampled/polled from update().
constexpr unsigned long ROVER_SENSOR_UPDATE_PERIOD_MS = 200;
// How often a sensor that failed begin() gets another attempt -- not
// every loop(), so a permanently-missing sensor doesn't waste I2C bus
// time retrying dozens of times a second.
constexpr unsigned long ROVER_SENSOR_RETRY_PERIOD_MS = 5000;
// How often SensorHub's STATE frames are sent to the Pi -- decoupled
// from the per-sensor sampling rate above, no need to spam the UART at
// the same cadence the sensors are read internally.
constexpr unsigned long ROVER_SENSOR_TELEMETRY_PERIOD_MS = 500;

// EVENT name=obstacle_detected fires once a ToF reading drops below
// this, and only clears once it rises back above the (higher) clear
// threshold -- hysteresis so a reading hovering right at one fixed
// threshold doesn't flip the event on and off every cycle.
constexpr uint16_t ROVER_OBSTACLE_THRESHOLD_MM = 150;
constexpr uint16_t ROVER_OBSTACLE_CLEAR_MM = 200;
