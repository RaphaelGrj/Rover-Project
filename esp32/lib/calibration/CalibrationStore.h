#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Thin wrapper around the ESP32's NVS (via the Arduino core's
// Preferences library) for persisting a handful of named calibration
// floats across reboots -- currently just the drive PID gains
// (main.cpp's SYSTEM action=set_pid/get_pid/reset_pid), but written
// generically enough to hold head/servo calibration too later without
// a new storage mechanism.
//
// Every read falls back to the caller-supplied default when nothing is
// stored yet (first boot, or after a firmware update that adds a new
// key) -- this must never be the thing that blocks boot just because
// NVS is empty or was erased.
namespace CalibrationStore {

constexpr const char* NAMESPACE = "rover";

inline float getFloat(const char* key, float defaultValue) {
    Preferences prefs;
    // Read-only open (true) -- never creates the namespace just by
    // reading from it.
    if (!prefs.begin(NAMESPACE, true)) return defaultValue;
    float value = prefs.getFloat(key, defaultValue);
    prefs.end();
    return value;
}

inline void setFloat(const char* key, float value) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.putFloat(key, value);
    prefs.end();
}

inline void remove(const char* key) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.remove(key);
    prefs.end();
}

}  // namespace CalibrationStore
