#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "Watchdog.h"
#include "WifiCredentialsStore.h"

// Fall back to an empty string when the build didn't set this -- the
// common case for anyone who hasn't opted into OTA via build-time
// config. Never hardcode a real password here: this project is open
// source, and a shared default baked into the repo would be a real
// vulnerability for every downstream user who doesn't change it. See
// platformio.ini and esp32/OTA.md for how to set it through an
// environment variable at build time, outside version control entirely.
//
// This macro is only the fallback path: WifiCredentialsStore (NVS, set
// through the RoverWifiProvisioning phone/PC portal) takes priority
// whenever it holds a non-empty password.
#ifndef ROVER_OTA_PASSWORD
#define ROVER_OTA_PASSWORD ""
#endif

// Optional ArduinoOTA support: flashing firmware over the LAN without
// opening up the robot.
//
// This deliberately puts a network service on the ESP32, which
// ARCHITECTURE_AND_ROADMAP.md §22 otherwise reserves for the Pi. The
// distinction: this is a local-LAN-only maintenance channel, completely
// inert unless a developer deliberately configures a password, and it
// never touches the Rover Protocol / safety path.
//
// What changed on 2026-09-20 (ARCHITECTURE_AND_ROADMAP.md §6.2,
// question "5 ter")
// -------------------------------------------------------------------
// This class used to *own* the WiFi connection: it called WiFi.begin()
// and blocked up to 10s at boot waiting for the network. That made an
// explicitly optional maintenance module the only thing connecting the
// robot to its network -- untenable once the command link itself runs
// over that network. Connectivity now lives in RoverNetwork.h and this
// class is one of its clients: it contributes nothing but ArduinoOTA.
//
// The practical gain beyond ownership: arm() is driven by
// RoverNetwork's onConnected callback, so OTA now becomes available on
// *any* successful connection rather than only on one that happened to
// complete during the boot window. A robot powered on before its
// router, or one that reconnected after an outage, used to stay
// un-flashable until the next power cycle.
//
// Note the two gates remain independent, which is what makes the
// portal's "WiFi first, OTA password later" flow work (esp32/OTA.md):
// joining the network only needs a SSID, but ArduinoOTA itself still
// refuses to start without a password -- no unauthenticated flashing by
// anyone on the LAN.
class RoverOTA {
public:
    // Call once the station link is up (RoverNetwork::onConnected).
    // Idempotent: a reconnection calls this again, and re-running
    // ArduinoOTA.begin() on an already-armed instance is pointless
    // rather than harmful, so it is guarded.
    //
    // Returns false when no OTA password is configured -- the robot is
    // on the network but not flashable, which is a legitimate and
    // common state, not an error.
    bool arm() {
        if (_active) return true;

        String otaPassword = WifiCredentialsStore::getOtaPassword();
        if (otaPassword.length() == 0) otaPassword = ROVER_OTA_PASSWORD;
        if (otaPassword.length() == 0) return false;

        ArduinoOTA.setPassword(otaPassword.c_str());
        ArduinoOTA.setHostname("rover-esp32");
        // Once a transfer is in progress, ArduinoOTA.handle() blocks
        // internally (reading the TCP stream + writing flash) for as
        // long as the update takes -- it does not return to loop() in
        // between chunks. Without feeding the watchdog from here, a
        // real OTA over WiFi tripped the 3s hardware watchdog
        // (Watchdog.h) partway through and rebooted the board mid-flash
        // (found on real hardware, 2026-09-05: consistently failed
        // around 15% every time). onProgress is the documented hook for
        // exactly this.
        ArduinoOTA.onProgress([](unsigned int, unsigned int) { RoverWatchdog::feed(); });
        ArduinoOTA.begin();
        _active = true;
        return true;
    }

    // Non-blocking, call every loop() iteration.
    void update() {
        if (_active) ArduinoOTA.handle();
    }

    // Kept for the SYSTEM action=wifi_status reply. `networkUp` comes
    // from RoverNetwork -- this class no longer tracks connectivity
    // itself, precisely because it is no longer the thing that owns it.
    void buildStatusFields(char* out, size_t outLen, bool networkUp) const {
        if (_active && networkUp) {
            snprintf(out, outLen, "wifi_mode=ota ip=%s", WiFi.localIP().toString().c_str());
        } else if (networkUp) {
            // On the network, but no OTA password registered yet --
            // still useful (reachable for a future SYSTEM
            // action=wifi_setup to add one, and for the command link
            // itself), just not flashable.
            snprintf(out, outLen, "wifi_mode=wifi ip=%s", WiFi.localIP().toString().c_str());
        } else {
            snprintf(out, outLen, "wifi_mode=off");
        }
    }

    bool isActive() const { return _active; }

private:
    bool _active = false;
};
