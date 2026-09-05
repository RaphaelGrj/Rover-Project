#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Thin Preferences (NVS) wrapper for the WiFi/OTA credentials, same
// pattern as CalibrationStore.h but for strings and in its own
// namespace -- keeping it separate from "rover" (PID gains) means a
// wifi_forget doesn't risk touching unrelated calibration data.
//
// This is what lets RoverWifiProvisioning (the phone/PC config portal)
// and RoverOTA share credentials without either one depending on the
// other. Every read falls back to the caller-supplied default (usually
// the empty string) when nothing is stored yet -- first boot, or after
// wifi_forget -- so this must never be the thing that blocks boot.
namespace WifiCredentialsStore {

constexpr const char* NAMESPACE = "rovernet";

inline String getString(const char* key, const char* defaultValue) {
    Preferences prefs;
    // Read-only open (true) -- never creates the namespace just by
    // reading from it.
    if (!prefs.begin(NAMESPACE, true)) return String(defaultValue);
    String value = prefs.getString(key, defaultValue);
    prefs.end();
    return value;
}

inline void setString(const char* key, const String& value) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.putString(key, value);
    prefs.end();
}

inline void remove(const char* key) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.remove(key);
    prefs.end();
}

inline String getSsid() { return getString("ssid", ""); }
inline String getPassword() { return getString("pass", ""); }
inline String getOtaPassword() { return getString("ota_pass", ""); }

inline void setSsid(const String& v) { setString("ssid", v); }
inline void setPassword(const String& v) { setString("pass", v); }
inline void setOtaPassword(const String& v) { setString("ota_pass", v); }

// Clears the WiFi network credentials (SSID/password) but deliberately
// keeps the OTA password -- forgetting which router to join shouldn't
// also force re-typing the flashing password on the next setup.
inline void forgetNetwork() {
    remove("ssid");
    remove("pass");
}

}  // namespace WifiCredentialsStore
