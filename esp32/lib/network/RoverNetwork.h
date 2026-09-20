#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <functional>
#include "WifiCredentialsStore.h"

// Fall back to empty strings when the build didn't set these. Never
// hardcode a real SSID/password here: this project is open source, and
// a shared default baked into the repo would be a real vulnerability
// for every downstream user who doesn't change it. See platformio.ini
// (-D ROVER_WIFI_SSID=\"${sysenv.ROVER_WIFI_SSID}\") and esp32/OTA.md
// for how to set these through environment variables at build time.
//
// These macros are only the fallback path: WifiCredentialsStore (NVS,
// filled through the RoverWifiProvisioning portal) takes priority
// whenever it holds a non-empty SSID, which is what lets the network be
// (re)configured without a reflash.
#ifndef ROVER_WIFI_SSID
#define ROVER_WIFI_SSID ""
#endif
#ifndef ROVER_WIFI_PASSWORD
#define ROVER_WIFI_PASSWORD ""
#endif

// The single owner of the WiFi station connection.
//
// Why this module exists (ARCHITECTURE_AND_ROADMAP.md §6.2, question
// "5 ter", a blocking prerequisite of the deported-Pi work)
// -------------------------------------------------------------------
// Until now the *only* thing that joined a network was RoverOTA::begin()
// -- a module whose explicit contract is to be optional, "entirely
// inert unless a developer deliberately configures credentials". That
// was harmless while the Pi hung off a USB cable and the radio only
// ever carried firmware updates. Once the Rover Protocol itself travels
// over WiFi (§6.2), it would make the *command link* depend on an
// optional maintenance module, which contradicts §4.2 ("l'ESP32 doit
// rester fonctionnel") and the ownership hierarchy of §22.
//
// So connectivity moves here and both RoverOTA and (next, the socket
// transport) become plain clients of it. Three concrete defects of the
// old arrangement are fixed on the way:
//
//   1. It connected ONCE, at boot, and blocked up to 10s doing it. A
//      robot powered on before its router finished booting stayed
//      offline until someone power-cycled it. begin() below returns
//      immediately and update() keeps trying forever.
//   2. Reconnection relied on the ESP32 core's default auto-reconnect,
//      never asserted anywhere in this codebase. It is asserted here,
//      and backed by an explicit supervisor besides -- same reasoning
//      as pi/rover_esp32/link.py, which supervises the other end of
//      this very link rather than trusting the transport to heal.
//   3. Nothing turned off WiFi power save, which buffers packets to
//      save energy and adds tens of milliseconds of jitter. Harmless
//      for a firmware upload, not for a 500ms safety heartbeat -- and
//      measuring link jitter (the next step of the chantier) with
//      power save on would measure the radio's nap schedule instead of
//      the network.
//
// Radio arbitration
// -----------------
// Two other modules legitimately need the radio in AP mode and cannot
// share it: RoverWifiProvisioning (the config portal) and
// StandaloneControl (piloting with no Pi at all, §6.3). They call
// suspend() before taking it and resume() when they hand it back,
// rather than each one reaching for WiFi.mode()/reconnect() on its own
// -- which is how the "portal timeout leaves the radio in WIFI_OFF
// until reboot" lockout happened in the first place (fixed 2026-09-15).
// While suspended this module does nothing at all: no mode changes, no
// reconnect attempts, no fighting the module that currently owns the AP.
//
// Never blocks, never delay()s: update() is called every loop()
// iteration alongside the motor/safety path.
class RoverNetwork {
public:
    // Fired once each time the station link comes up (first connection
    // and every reconnection after a drop). RoverOTA hooks this to arm
    // ArduinoOTA, which used to only ever happen at boot -- an OTA that
    // was unavailable because the router was slow to come up stayed
    // unavailable for the whole session.
    std::function<void()> onConnected;

    // Reads credentials and starts connecting. Returns false and leaves
    // the radio untouched when no network is configured at all -- the
    // common case for anyone who hasn't opted in, and the reason a
    // freshly cloned build has zero radio activity.
    //
    // Returning true means "we started trying", NOT "we are connected":
    // nothing here waits for the network. Use isConnected() for that.
    bool begin() {
        _ssid = WifiCredentialsStore::getSsid();
        _password = WifiCredentialsStore::getPassword();
        // NVS empty (never provisioned through the portal) -- fall back
        // to whatever was baked in at build time, if anything.
        if (_ssid.length() == 0) {
            _ssid = ROVER_WIFI_SSID;
            _password = ROVER_WIFI_PASSWORD;
        }
        if (_ssid.length() == 0) return false;

        _configured = true;
        connect();
        return true;
    }

    // Non-blocking, call every loop() iteration.
    void update() {
        if (!_configured || _suspended) return;

        bool connected = (WiFi.status() == WL_CONNECTED);

        if (connected) {
            if (!_connected) {
                // Edge, not level: the callback must fire once per
                // connection, not once per loop() iteration.
                _connected = true;
                _connectionCount++;
                _retryDelayMs = RETRY_MIN_DELAY_MS;
                if (onConnected) onConnected();
            }
            return;
        }

        if (_connected) {
            _connected = false;
            _disconnectionCount++;
            // Retry promptly after a drop: a brief hiccup should not
            // cost the full backoff the "router is off" case deserves.
            _retryDelayMs = RETRY_MIN_DELAY_MS;
            _lastAttemptMs = millis();
            return;
        }

        // Still down. WiFi.begin() was already called (by begin() or a
        // previous pass) and the core retries on its own; re-issuing it
        // periodically is the backstop for the cases where it doesn't
        // -- an AP that vanished entirely, or credentials accepted only
        // after the router finished booting.
        if (millis() - _lastAttemptMs >= _retryDelayMs) {
            connect();
            _retryDelayMs = _retryDelayMs * 2 > RETRY_MAX_DELAY_MS
                                ? RETRY_MAX_DELAY_MS
                                : _retryDelayMs * 2;
        }
    }

    // Called by whoever is about to take the radio into AP mode. Idempotent.
    void suspend() {
        _suspended = true;
        _connected = false;
    }

    // Called when the AP is handed back. Does not block waiting for the
    // link to return -- update() takes it from here, and the Pi's own
    // supervisor (pi/rover_esp32/link.py) retries indefinitely on its
    // side, so recovery is automatic once the network is reachable.
    void resume() {
        if (!_suspended) return;
        _suspended = false;
        if (!_configured) return;
        // Straight back to connecting rather than waiting out a backoff
        // the AP interlude had nothing to do with.
        _retryDelayMs = RETRY_MIN_DELAY_MS;
        connect();
    }

    bool isConfigured() const { return _configured; }
    bool isSuspended() const { return _suspended; }
    bool isConnected() const { return _connected; }
    String localIP() const { return WiFi.localIP().toString(); }

    // Bring-up/diagnostic counters. A deported Pi cannot see the
    // robot's console, so "how often does this link actually drop?" has
    // to be answerable from telemetry alone -- and answering it is
    // precisely what step 2 of the chantier (measure before tuning the
    // heartbeat thresholds) needs.
    uint32_t connectionCount() const { return _connectionCount; }
    uint32_t disconnectionCount() const { return _disconnectionCount; }

    void buildStatusFields(char* out, size_t outLen) const {
        if (!_configured) {
            snprintf(out, outLen, "net=unconfigured");
        } else if (_suspended) {
            snprintf(out, outLen, "net=suspended");
        } else if (_connected) {
            snprintf(out, outLen, "net=up ip=%s rssi=%d drops=%lu",
                     WiFi.localIP().toString().c_str(), (int)WiFi.RSSI(),
                     (unsigned long)_disconnectionCount);
        } else {
            snprintf(out, outLen, "net=down drops=%lu", (unsigned long)_disconnectionCount);
        }
    }

private:
    // 10s, measured rather than guessed: at 1s (the first version of
    // this file) a real WROOM logged "wifi:sta is connecting, return
    // error" and "begin(): connect failed!" on every pass, because an
    // association attempt takes several seconds and update() kept
    // re-issuing WiFi.begin() straight into the one already running
    // (found on hardware, 2026-09-20). There is no status code that
    // distinguishes "still trying" from "idle after a failure" -- both
    // read as WL_DISCONNECTED -- so the retry interval has to be longer
    // than an attempt takes instead. 30s max so a robot sitting in a
    // room with no network doesn't burn power and log noise all day.
    // Same shape as RECONNECT_MIN/MAX_DELAY_S in pi/rover_esp32/link.py
    // -- both ends of this link back off the same way on purpose.
    static constexpr unsigned long RETRY_MIN_DELAY_MS = 10000;
    static constexpr unsigned long RETRY_MAX_DELAY_MS = 30000;

    void connect() {
        WiFi.mode(WIFI_STA);
        // Cancels any attempt still in flight so begin() below starts
        // from a clean state rather than being refused outright. Both
        // flags false: don't switch the radio off, don't erase the
        // stored credentials -- we are about to reconnect with them.
        WiFi.disconnect(false, false);
        // Asserted rather than assumed: the core's default is true, but
        // this link is load-bearing now and a silent upstream change to
        // that default would be very hard to diagnose from a deported Pi.
        WiFi.setAutoReconnect(true);
        // Modem sleep buffers incoming packets between beacons, which
        // costs tens of milliseconds of jitter -- unacceptable under a
        // heartbeat measured in hundreds. Set on every connect() because
        // it does not survive a mode change.
        WiFi.setSleep(false);
        WiFi.begin(_ssid.c_str(), _password.c_str());
        _lastAttemptMs = millis();
    }

    String _ssid;
    String _password;
    bool _configured = false;
    bool _suspended = false;
    bool _connected = false;
    unsigned long _lastAttemptMs = 0;
    unsigned long _retryDelayMs = RETRY_MIN_DELAY_MS;
    uint32_t _connectionCount = 0;
    uint32_t _disconnectionCount = 0;
};
