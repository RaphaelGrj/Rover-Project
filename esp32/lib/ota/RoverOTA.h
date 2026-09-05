#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "Watchdog.h"
#include "WifiCredentialsStore.h"

// Fall back to empty strings when the build didn't set these -- the
// common case for anyone who hasn't opted into OTA via build-time
// config. Never hardcode a real SSID/password here: this project is
// open source, and a shared default WiFi/OTA credential baked into the
// repo would be a real vulnerability for every downstream user who
// doesn't change it. See platformio.ini
// (-D ROVER_WIFI_SSID=\"${sysenv.ROVER_WIFI_SSID}\" etc.) and
// esp32/OTA.md for how to set these via environment variables at build
// time, outside version control entirely.
//
// These macros are now only the fallback path: WifiCredentialsStore
// (NVS, set through the RoverWifiProvisioning phone/PC portal, see
// esp32/WIFI_SETUP.md) takes priority whenever it holds a non-empty
// SSID, which lets the network be (re)configured without a reflash.
#ifndef ROVER_WIFI_SSID
#define ROVER_WIFI_SSID ""
#endif
#ifndef ROVER_WIFI_PASSWORD
#define ROVER_WIFI_PASSWORD ""
#endif
#ifndef ROVER_OTA_PASSWORD
#define ROVER_OTA_PASSWORD ""
#endif

// Optional WiFi + ArduinoOTA support, entirely opt-in: either through
// the build-time environment variables above, or (preferred) through
// the NVS-stored credentials set via the on-demand config portal
// (RoverWifiProvisioning.h).
//
// This deliberately puts networking on the ESP32, which
// ARCHITECTURE_AND_ROADMAP.md section 22 otherwise reserves for the Pi
// ("l'ESP32 ne doit jamais... dépendre d'Internet pour assurer la
// sécurité"). The distinction: this is a local-LAN-only maintenance
// channel for flashing firmware without opening up the robot, it is
// completely inert unless a developer deliberately configures
// credentials, and it never touches the Rover Protocol / safety path --
// the robot's actual operation still never depends on network
// connectivity or Internet access.
//
// WiFi connection and ArduinoOTA are two independent gates, not one:
// joining the network only needs a SSID (so the portal's "WiFi first,
// OTA later" flow -- see esp32/OTA.md -- actually works if someone
// submits the form without an OTA password yet), but ArduinoOTA itself
// still refuses to start without one (no unauthenticated flashing by
// anyone on the LAN).
class RoverOTA {
public:
    // Returns false immediately (no WiFi radio activity at all) if no
    // network is configured -- the common case. Never blocks more than
    // ROVER_OTA_WIFI_CONNECT_TIMEOUT_MS: this is a convenience feature,
    // never something the boot sequence should hang on because a
    // configured network isn't in range.
    bool begin() {
        String ssid = WifiCredentialsStore::getSsid();
        String password = WifiCredentialsStore::getPassword();
        String otaPassword = WifiCredentialsStore::getOtaPassword();
        // NVS is empty (never provisioned through the portal yet) --
        // fall back to whatever was baked in at build time, if anything.
        if (ssid.length() == 0) {
            ssid = ROVER_WIFI_SSID;
            password = ROVER_WIFI_PASSWORD;
            otaPassword = ROVER_OTA_PASSWORD;
        }

        if (ssid.length() == 0) return false;

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < ROVER_OTA_WIFI_CONNECT_TIMEOUT_MS) {
            delay(100);
            // This loop runs from setup(), before the first loop()
            // iteration ever gets to feed the watchdog -- a slow/absent
            // network would otherwise trip the 3s hardware watchdog
            // (Watchdog.h) well before this 10s timeout does. A Phase 4
            // sensor bug this same project hit taught this the hard way
            // (PROGRESS.md 2026-08-31): never let setup() block for a
            // while without feeding it.
            RoverWatchdog::feed();
        }
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.mode(WIFI_OFF);
            return false;
        }
        _wifiConnected = true;

        if (otaPassword.length() > 0) {
            ArduinoOTA.setPassword(otaPassword.c_str());
            ArduinoOTA.setHostname("rover-esp32");
            // Once a transfer is in progress, ArduinoOTA.handle() blocks
            // internally (reading the TCP stream + writing flash) for as
            // long as the update takes -- it does not return to loop()
            // in between chunks. Without feeding the watchdog from here,
            // a real OTA over WiFi tripped the 3s hardware watchdog
            // (Watchdog.h) partway through and rebooted the board mid-
            // flash (found on real hardware, 2026-09-05: consistently
            // failed around 15% every time). onProgress is the
            // documented hook for exactly this.
            ArduinoOTA.onProgress([](unsigned int, unsigned int) { RoverWatchdog::feed(); });
            ArduinoOTA.begin();
            _otaActive = true;
        }
        return true;
    }

    // Non-blocking, call every loop() iteration.
    void update() {
        if (_otaActive) ArduinoOTA.handle();
    }

    void buildStatusFields(char* out, size_t outLen) const {
        if (_otaActive) {
            snprintf(out, outLen, "wifi_mode=ota ip=%s", WiFi.localIP().toString().c_str());
        } else if (_wifiConnected) {
            // Joined the network, but no OTA password registered yet --
            // still useful (reachable for a future SYSTEM action=wifi_setup
            // to add one, or any other network use), just not flashable.
            snprintf(out, outLen, "wifi_mode=wifi ip=%s", WiFi.localIP().toString().c_str());
        } else {
            snprintf(out, outLen, "wifi_mode=off");
        }
    }

    bool isActive() const { return _otaActive; }

private:
    static constexpr unsigned long ROVER_OTA_WIFI_CONNECT_TIMEOUT_MS = 10000;
    bool _wifiConnected = false;
    bool _otaActive = false;
};
