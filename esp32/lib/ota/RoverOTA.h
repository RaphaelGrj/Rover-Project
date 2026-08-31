#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "Watchdog.h"

// Fall back to empty strings when the build didn't set these -- the
// common case for anyone who hasn't opted into OTA. Never hardcode a
// real SSID/password here: this project is open source, and a shared
// default WiFi/OTA credential baked into the repo would be a real
// vulnerability for every downstream user who doesn't change it. See
// platformio.ini (-D ROVER_WIFI_SSID=\"${sysenv.ROVER_WIFI_SSID}\" etc.)
// and esp32/OTA.md for how to set these via environment variables at
// build time, outside version control entirely.
#ifndef ROVER_WIFI_SSID
#define ROVER_WIFI_SSID ""
#endif
#ifndef ROVER_WIFI_PASSWORD
#define ROVER_WIFI_PASSWORD ""
#endif
#ifndef ROVER_OTA_PASSWORD
#define ROVER_OTA_PASSWORD ""
#endif

// Optional WiFi + ArduinoOTA support, entirely opt-in via the build-time
// environment variables above.
//
// This deliberately puts networking on the ESP32, which
// ARCHITECTURE_AND_ROADMAP.md section 22 otherwise reserves for the Pi
// ("l'ESP32 ne doit jamais... dépendre d'Internet pour assurer la
// sécurité"). The distinction: this is a local-LAN-only maintenance
// channel for flashing firmware without opening up the robot, it is
// completely inert unless a developer deliberately configures
// credentials, it refuses to start without an OTA password (no
// unauthenticated flashing by anyone on the LAN), and it never touches
// the Rover Protocol / safety path -- the robot's actual operation
// still never depends on network connectivity or Internet access.
class RoverOTA {
public:
    // Returns false immediately (no WiFi radio activity at all) if OTA
    // isn't configured -- the common case. Never blocks more than
    // ROVER_OTA_WIFI_CONNECT_TIMEOUT_MS: this is a convenience feature,
    // never something the boot sequence should hang on because a
    // configured network isn't in range.
    bool begin() {
        if (strlen(ROVER_WIFI_SSID) == 0) return false;
        if (strlen(ROVER_OTA_PASSWORD) == 0) {
            // Fail closed: refuse to bring up a WiFi/OTA channel that
            // would accept unauthenticated firmware flashes from
            // anyone on the LAN, rather than silently being insecure.
            return false;
        }

        WiFi.mode(WIFI_STA);
        WiFi.begin(ROVER_WIFI_SSID, ROVER_WIFI_PASSWORD);
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

        ArduinoOTA.setPassword(ROVER_OTA_PASSWORD);
        ArduinoOTA.setHostname("rover-esp32");
        ArduinoOTA.begin();
        _active = true;
        return true;
    }

    // Non-blocking, call every loop() iteration.
    void update() {
        if (_active) ArduinoOTA.handle();
    }

    bool isActive() const { return _active; }

private:
    static constexpr unsigned long ROVER_OTA_WIFI_CONNECT_TIMEOUT_MS = 10000;
    bool _active = false;
};
