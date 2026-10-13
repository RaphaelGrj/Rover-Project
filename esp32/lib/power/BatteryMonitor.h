#pragma once

#include <Arduino.h>
#include "power_config.h"
#include "Ina219.h"

// Reports a 0-100% battery estimate plus EVENT name=low_battery
// (ROVER_PROTOCOL.md §7.2), with hysteresis.
//
// Two sources, picked by power_config.h:
// - WROOM: an ADC + resistor divider, entirely disabled by default
//   (ROVER_BATTERY_MONITORING_ENABLED false) -- begin()/update() are
//   no-ops and hasReading() stays false, so main.cpp never sends a
//   fabricated STATE battery=... before the divider hardware exists.
// - Custom PCB: the INA219, which also gives the pack CURRENT (negative
//   while charging). A missing or silent INA219 means no reading and
//   one consumeFailureEvent(), never a made-up value.
class BatteryMonitor {
public:
    void begin();

    // Non-blocking, call every loop() iteration.
    void update();

    // One measurement right now, ignoring the update period. For the
    // boot-time low-battery check (LowBatterySleep), which cannot wait
    // the 2 s the periodic update() would take to produce a first value.
    // No-op while the INA219 is absent.
    void sampleNow();

    bool hasReading() const { return _hasReading; }
    uint8_t percent() const { return _percent; }
    // Pack voltage in mV; 0 until a first reading. Also what the charger
    // cross-checks its cell readings against (ChargerMonitor).
    uint32_t packMv() const { return _packMv; }
    // Pack current in mA, positive = discharging. Only meaningful with
    // the INA219 (always 0 on the ADC path).
    int32_t currentMa() const { return _currentMa; }

    // True exactly once, on the update() where battery level first
    // crossed into "low" -- caller must send EVENT name=low_battery.
    bool consumeLowBatteryEvent();

    // True exactly once per OK -> failed transition of the INA219
    // (including "absent at boot"), for an ERROR sensor_timeout report.
    bool consumeFailureEvent();

    // "battery=NN" on the ADC path; "battery=NN battery_mv=7620
    // battery_ma=-950" with the INA219 (ROVER_PROTOCOL.md §7.1).
    void buildTelemetryFields(char* out, size_t outLen) const;

private:
    Ina219 _ina;
    bool _hasReading = false;
    uint8_t _percent = 0;
    uint32_t _packMv = 0;
    int32_t _currentMa = 0;
    bool _lowActive = false;
    bool _pendingLowEvent = false;
    bool _inaWasOk = true;  // "true" so a chip missing at boot is reported once
    bool _pendingFailureEvent = false;
    unsigned long _lastUpdateMs = 0;
    unsigned long _lastRetryMs = 0;

    void measure(unsigned long now);
    void applyPercent(uint32_t packMv);
    void trackInaHealth();
};
