#pragma once

#include "BatteryMonitor.h"
#include "ChargerMonitor.h"

// Deep sleep when the battery is too low to keep running (custom PCB).
//
// Why: the protection IC fitted on the PCB (S-8252AAC) only cuts a cell
// at 2.40 V, below what 18650s are specified for, and a robot that has
// stopped its motors still draws ~100 mA through the ESP32, WiFi and
// display -- enough to walk a forgotten robot down to that cut-off in a
// few hours. Deep sleep brings the whole board down to a few mA, which
// buys days instead (pcb/PCB_CONNEXIONS.md section 5.1).
//
// The thresholds are in power_config.h (ROVER_CELL_SHUTDOWN_MV,
// ROVER_CELL_RESUME_MV, ROVER_LOW_BATTERY_WAKE_PERIOD_S); the decision
// itself is PowerMath::batteryTooLowToRun, unit-tested natively. On
// boards without the INA219 every function here answers "no", so the
// WROOM never sleeps.
namespace LowBatterySleep {

// True if this boot is a timer wake-up from a low-battery sleep.
bool wokeFromLowBatterySleep();

// Call once in setup(), right after battery.begin() and charger.begin()
// and BEFORE anything power-hungry (display, WiFi, camera). Waits for
// the chips' first conversions, measures, and returns true if the robot
// must go back to sleep straight away. After a low-battery sleep the
// bar is higher (ROVER_CELL_RESUME_MV), so a cell that merely rebounded
// at rest does not trigger a full boot that drags it down again.
bool tooLowAtBoot(BatteryMonitor& battery, ChargerMonitor& charger);

// From loop(), after battery.update() and charger.update(): true when
// the robot must shut down now. Answers false until both have a live,
// trustworthy reading.
bool tooLowNow(const BatteryMonitor& battery, const ChargerMonitor& charger);

// Puts every board line in its lowest-power state, then deep-sleeps
// with a timer wake-up. Does not return. The caller stops the motors
// and sends its last frame first.
[[noreturn]] void enterSleep();

}  // namespace LowBatterySleep
