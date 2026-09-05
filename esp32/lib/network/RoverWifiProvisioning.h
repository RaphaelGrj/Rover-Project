#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "WifiCredentialsStore.h"
#include "Watchdog.h"

// On-demand WiFi configuration portal: the ESP32 opens its own
// temporary access point (no home network needed) serving a single web
// page, so any PC or phone can join that AP and fill in the *real*
// home WiFi SSID/password plus the OTA flashing password -- no reflash
// over USB required to (re)point the robot at a different network.
//
// Deliberately NOT started automatically on every boot: an always-open
// configuration AP would be a standing door into the robot's network
// settings for anyone in WiFi range. Instead it is only brought up by
// an explicit `SYSTEM action=wifi_setup` frame (see main.cpp) -- sent
// from a USB-serial terminal during bring-up, or later from the Pi --
// and it auto-closes after ROVER_PROVISIONING_TIMEOUT_MS of inactivity
// if nobody finishes the form.
//
// Entirely separate from the Rover Protocol / safety path: update()
// only pumps the DNS/HTTP servers, never touches motors/heartbeat, and
// never blocks (no delay()) so it can be called every loop() iteration
// alongside everything else without risking the watchdog.
class RoverWifiProvisioning {
public:
    // Non-blocking to call: WiFi.softAP() itself takes only a few ms.
    void start() {
        WiFi.mode(WIFI_AP);
        String apSsid = buildApSsid();
        // Open AP, no password: this is a short-lived, operator-
        // triggered local hotspot (see class comment), not a permanent
        // credential -- there is no shared secret here to leak, unlike
        // the home WiFi/OTA passwords the portal collects. Documented
        // trade-off in WIFI_SETUP.md.
        _apStarted = WiFi.softAP(apSsid.c_str());

        // Redirects every DNS lookup to this device's own IP, which is
        // what makes phones/PCs pop the "sign in to network" captive
        // portal prompt automatically instead of requiring the user to
        // type 192.168.4.1 by hand.
        _dnsServer.start(53, "*", WiFi.softAPIP());

        _server.on("/", HTTP_GET, [this]() { handleForm(); });
        _server.on("/save", HTTP_POST, [this]() { handleSave(); });
        // Any unknown path (including the OS's captive-portal probe
        // URLs) also serves the form -- good enough for this hobby
        // use case without hand-tracking every OS's exact probe path.
        _server.onNotFound([this]() { handleForm(); });
        _server.begin();

        _active = true;
        _pendingRestartAtMs = 0;
        _lastActivityMs = millis();
    }

    void stop() {
        if (!_active) return;
        _server.stop();
        _dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
        _active = false;
    }

    // Non-blocking, call every loop() iteration regardless of whether
    // provisioning is active (it is a no-op when it is not).
    void update() {
        if (!_active) return;

        if (_pendingRestartAtMs != 0) {
            // Gives handleSave()'s HTTP response time to actually reach
            // the browser before the AP disappears out from under it.
            if (millis() >= _pendingRestartAtMs) {
                ESP.restart();
            }
            RoverWatchdog::feed();
            return;
        }

        _dnsServer.processNextRequest();
        _server.handleClient();

        if (millis() - _lastActivityMs >= ROVER_PROVISIONING_TIMEOUT_MS) {
            // Nobody finished the form in time -- close the door rather
            // than leaving an open configuration AP running forever.
            stop();
        }
    }

    bool isActive() const { return _active; }

    void buildStatusFields(char* out, size_t outLen) const {
        if (_active) {
            // ap_started/ap_ip are bring-up diagnostics -- softAP() can
            // fail silently (returns false) without them, which looked
            // exactly like "portal not reachable" from the outside.
            snprintf(out, outLen, "wifi_mode=setup ap_ssid=%s ap_started=%d ap_ip=%s",
                     buildApSsid().c_str(), _apStarted ? 1 : 0, WiFi.softAPIP().toString().c_str());
        } else {
            snprintf(out, outLen, "wifi_mode=idle");
        }
    }

private:
    static constexpr unsigned long ROVER_PROVISIONING_TIMEOUT_MS = 10UL * 60UL * 1000UL;
    bool _apStarted = false;

    String buildApSsid() const {
        // Last 3 bytes of the MAC keep this unique across multiple
        // Rovers on the same site without needing any config of its own.
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char suffix[7];
        snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
        return String("Rover-Setup-") + suffix;
    }

    void handleForm() {
        // Pre-fills the SSID (not the passwords -- never echo a stored
        // secret back into HTML served on an open AP) so re-running
        // setup just to change a password doesn't require retyping it.
        String currentSsid = WifiCredentialsStore::getSsid();
        String hasOta = WifiCredentialsStore::getOtaPassword().length() > 0 ? "oui" : "non";

        String page;
        page.reserve(1024);
        page += "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>";
        page += "<title>Configuration WiFi Rover</title>";
        page += "<style>body{font-family:sans-serif;max-width:420px;margin:2em auto;padding:0 1em}";
        page += "input{width:100%;padding:.5em;margin:.3em 0 1em;box-sizing:border-box}";
        page += "button{width:100%;padding:.7em;font-size:1em}</style></head><body>";
        page += "<h2>Configuration WiFi Rover</h2>";
        page += "<form method=POST action=/save>";
        page += "<label>Reseau WiFi (SSID)</label>";
        page += "<input name=ssid value='" + currentSsid + "' required>";
        page += "<label>Mot de passe WiFi (laisser vide pour garder l'actuel)</label>";
        page += "<input name=pass type=password>";
        page += "<label>Mot de passe OTA (laisser vide pour garder l'actuel -- deja configure : " + hasOta + ")</label>";
        page += "<input name=ota_pass type=password>";
        page += "<button type=submit>Enregistrer et redemarrer</button>";
        page += "</form></body></html>";

        _server.send(200, "text/html", page);
        _lastActivityMs = millis();
    }

    void handleSave() {
        if (!_server.hasArg("ssid") || _server.arg("ssid").length() == 0) {
            _server.send(400, "text/plain", "SSID requis");
            return;
        }

        WifiCredentialsStore::setSsid(_server.arg("ssid"));
        // Blank means "keep the current one", same as ota_pass below --
        // the field is never pre-filled with a stored secret (see
        // handleForm), so leaving it empty is the only way to resubmit
        // the form (eg. just to add an OTA password) without retyping
        // the WiFi password every time. Trade-off: switching to a
        // genuinely open (no-password) network needs typing a single
        // space rather than leaving the field empty -- documented in
        // OTA.md, an acceptable wrinkle for how rarely that happens.
        if (_server.arg("pass").length() > 0) {
            WifiCredentialsStore::setPassword(_server.arg("pass"));
        }
        if (_server.arg("ota_pass").length() > 0) {
            WifiCredentialsStore::setOtaPassword(_server.arg("ota_pass"));
        }

        _server.send(200, "text/html",
            "<html><body><p>Enregistre. Redemarrage du robot...</p></body></html>");
        // Scheduled, not immediate -- see update()'s comment on why.
        _pendingRestartAtMs = millis() + 1500;
        _lastActivityMs = millis();
    }

    WebServer _server{80};
    DNSServer _dnsServer;
    bool _active = false;
    unsigned long _lastActivityMs = 0;
    unsigned long _pendingRestartAtMs = 0;
};
