#pragma once

#include <Arduino.h>
#include "power_config.h"

// Reads the battery voltage divider and reports a 0-100% estimate plus
// EVENT name=low_battery (ROVER_PROTOCOL.md §7.2), with hysteresis.
//
// Entirely disabled when ROVER_BATTERY_MONITORING_ENABLED is false (the
// default, see power_config.h) -- begin()/update() become no-ops and
// hasReading() stays false, so main.cpp never sends a fabricated
// STATE battery=... before the real divider hardware exists.
class BatteryMonitor {
public:
    void begin() {
        if (!ROVER_BATTERY_MONITORING_ENABLED) return;
        pinMode(ROVER_PIN_BATTERY_ADC, INPUT);
    }

    // Non-blocking, call every loop() iteration.
    void update() {
        if (!ROVER_BATTERY_MONITORING_ENABLED) return;
        unsigned long now = millis();
        if (now - _lastUpdateMs < ROVER_BATTERY_UPDATE_PERIOD_MS) return;
        _lastUpdateMs = now;

        // 12-bit ADC (0-4095) over the ESP32's ~3.3V reference.
        float vAdc = (analogRead(ROVER_PIN_BATTERY_ADC) / 4095.0f) * 3.3f;
        float vBatt = vAdc * ROVER_BATTERY_DIVIDER_RATIO;
        float pct = (vBatt - ROVER_BATTERY_EMPTY_V) /
                    (ROVER_BATTERY_FULL_V - ROVER_BATTERY_EMPTY_V) * 100.0f;
        _percent = (uint8_t)constrain(pct, 0.0f, 100.0f);
        _hasReading = true;

        bool low = _percent < ROVER_BATTERY_LOW_PERCENT;
        bool clear = _percent > ROVER_BATTERY_LOW_CLEAR_PERCENT;
        if (low && !_lowActive) _pendingLowEvent = true;
        if (low) _lowActive = true;
        else if (clear) _lowActive = false;
    }

    bool hasReading() const { return _hasReading; }
    uint8_t percent() const { return _percent; }

    // True exactly once, on the update() where battery level first
    // crossed into "low" -- caller must send EVENT name=low_battery.
    bool consumeLowBatteryEvent() {
        if (!_pendingLowEvent) return false;
        _pendingLowEvent = false;
        return true;
    }

    // Fills "battery=NN" (ROVER_PROTOCOL.md §7.1 example) for a STATE frame.
    void buildTelemetryFields(char* out, size_t outLen) const {
        snprintf(out, outLen, "battery=%u", _percent);
    }

private:
    bool _hasReading = false;
    uint8_t _percent = 0;
    bool _lowActive = false;
    bool _pendingLowEvent = false;
    unsigned long _lastUpdateMs = 0;
};
