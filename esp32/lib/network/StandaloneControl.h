#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <functional>
#include "WifiCredentialsStore.h"
#include "Watchdog.h"

// Standalone piloting: the ESP32 opens its OWN WPA2 access point and
// serves a small joystick page, so Rover can still be driven with no
// Raspberry Pi and no home network at all -- a phone joins the robot
// directly (ARCHITECTURE_AND_ROADMAP.md §6.3).
//
// Why this exists
// ---------------
// Deporting the Pi onto the LAN (§6.2) made the robot dependent on two
// things at once: a working network AND a reachable Pi. That was
// accepted at the time, but it means a WiFi outage, a Pi left off, or
// simply carrying Rover somewhere without infrastructure leaves an
// inert robot. This module is the floor under that: whatever else is
// down, Rover stays drivable by someone standing next to it.
//
// Is this "business logic on the ESP32" (§22)?
// --------------------------------------------
// No, and the distinction matters. §22 forbids the ESP32 owning AI,
// navigation, Home Assistant, the main decision loop -- the "QUOI
// faire". This is direct teleoperation: the decision comes from a
// human thumb on a screen, and the ESP32 does exactly what it already
// does for a MOVE frame from the Pi ("COMMENT le faire"). The page is
// just another source of the same commands, not a new decision-maker.
// Nothing here plans, chooses, or remembers anything.
//
// Safety: reuses, never duplicates
// --------------------------------
// This module owns NO safety logic of its own. Every existing layer
// applies unchanged, which is the whole point of routing through the
// same handlers main.cpp already uses:
//   - the browser beats the SAME HeartbeatMonitor the Pi does, so
//     closing the tab / walking out of range stops the motors via the
//     existing timeout, with no second code path to disagree;
//   - the E-stop is checked independently in loop() and still wins;
//   - the local obstacle reflex (DriveController::setForwardBlocked)
//     still refuses forward motion;
//   - entering this mode does NOT re-arm the motors. The robot is in
//     SAFE when it gets here (that is usually why it got here), and
//     only an explicit press on the page's "Activer" calls resume --
//     exactly like SYSTEM action=resume from the Pi.
//
// Access control
// --------------
// The AP is WPA2 and this refuses to start without a stored password,
// deliberately mirroring RoverOTA's "no unauthenticated flashing by
// anyone on the LAN". An open AP here would be strictly worse than the
// OTA case: it does not flash firmware, it drives a physical robot.
// Note this is the opposite choice from RoverWifiProvisioning's open
// portal -- that one only *collects* credentials for a few minutes and
// moves nothing, this one moves the robot.
class StandaloneControl {
public:
    // Fired when the page asks for something. Wired by main.cpp so this
    // module never touches DriveController/HeadController/state itself
    // -- same ownership discipline as RoverProtocol::onFrame.
    std::function<void(float velocity, float rotation)> onMove;
    std::function<void(float pitch, float yaw)> onLook;
    std::function<void()> onResume;
    std::function<void()> onStop;
    // Any request from the page counts as proof of life, exactly like
    // any valid frame from the Pi does (ROVER_PROTOCOL.md §6).
    std::function<void()> onHeartbeat;

    // Radio arbitration (added 2026-09-20 with RoverNetwork.h, see
    // ARCHITECTURE_AND_ROADMAP.md §6.2 "5 ter"). This mode needs the
    // radio in AP mode, which by definition drops the station link the
    // Pi talks over. Announcing the borrow and the return -- instead of
    // calling WiFi.mode() directly -- keeps a single module in charge
    // of that link. main.cpp routes both to
    // RoverNetwork::suspend()/resume().
    std::function<void()> onRadioTaken;
    std::function<void()> onRadioReleased;
    // "ACTIVE"/"SAFE"/... plus whether forward motion is currently
    // blocked, so the page can show why it is not moving.
    std::function<String()> statusProvider;

    // Returns false and changes nothing if no standalone password is
    // stored -- see the class comment. Caller is expected to report
    // that refusal (main.cpp sends an EVENT), never to fall back to an
    // open AP.
    bool start() {
        if (_active) return true;
        String password = WifiCredentialsStore::getStandalonePassword();
        // WPA2 requires >= 8 characters; a shorter one makes softAP()
        // silently fall back to an OPEN network, which is exactly the
        // outcome this whole module refuses to allow.
        if (password.length() < 8) return false;

        if (onRadioTaken) onRadioTaken();
        WiFi.mode(WIFI_AP);
        _apStarted = WiFi.softAP(buildApSsid().c_str(), password.c_str());
        if (!_apStarted) {
            // Hand the radio back rather than leaving it half-configured
            // in AP mode with no AP actually running.
            releaseRadio();
            return false;
        }

        _server.on("/", HTTP_GET, [this]() { handlePage(); });
        _server.on("/c", HTTP_GET, [this]() { handleCommand(); });
        _server.onNotFound([this]() { handlePage(); });
        _server.begin();
        // Cleared, not carried over: a timestamp left from a previous
        // session would make isBeingUsed() true before anyone has
        // actually connected this time.
        _lastRequestMs = 0;
        _active = true;
        return true;
    }

    // Restores station mode instead of switching the radio off, so the
    // Pi link can come back on its own once the network does. (The
    // WIFI_OFF version of this in RoverWifiProvisioning was a hard
    // lockout until reboot -- fixed 2026-09-15, same reasoning.)
    void stop() {
        if (!_active) return;
        _server.stop();
        WiFi.softAPdisconnect(true);
        releaseRadio();
        _active = false;
        _apStarted = false;
    }

    // Non-blocking, call every loop() iteration whether active or not.
    void update() {
        if (!_active) return;
        _server.handleClient();
    }

    bool isActive() const { return _active; }

    // True while a browser is actively driving. Lets main.cpp avoid
    // tearing the AP down under an operator's feet the moment the Pi
    // reappears -- the page polls every 200ms, so a few seconds of
    // silence means nobody is holding the phone any more.
    bool isBeingUsed() const {
        return _active && _lastRequestMs != 0 && (millis() - _lastRequestMs) < USED_WINDOW_MS;
    }

    void buildStatusFields(char* out, size_t outLen) const {
        if (_active) {
            snprintf(out, outLen, "wifi_mode=standalone ap_ssid=%s ap_ip=%s",
                     buildApSsid().c_str(), WiFi.softAPIP().toString().c_str());
        } else {
            snprintf(out, outLen, "wifi_mode=idle");
        }
    }

    String buildApSsid() const {
        // Same MAC-suffix scheme as the provisioning portal so two
        // Rovers on one site stay distinguishable, with a different
        // prefix so an operator can tell at a glance which mode the
        // robot is offering.
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char suffix[7];
        snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
        return String("Rover-Pilot-") + suffix;
    }

private:
    // Gives the radio back to whoever owns the station link. The
    // WiFi.mode()/reconnect() branch is a fallback for the case where
    // nobody wired the callback: leaving the radio in AP mode with no
    // AP running would be worse than the duplication. main.cpp always
    // wires it, so in practice only the callback runs.
    void releaseRadio() {
        if (onRadioReleased) {
            onRadioReleased();
        } else {
            WiFi.mode(WIFI_STA);
            WiFi.reconnect();
        }
    }

    void handlePage() {
        // Served straight from flash (PROGMEM), never built into a
        // String: this page is ~2KB and RAM is the scarcer resource
        // once WiFi is up.
        _server.send_P(200, "text/html", PAGE);
    }

    // GET /c?v=<velocity>&r=<rotation>[&p=&y=][&a=1][&s=1]
    // One endpoint, not five: the page polls it ~5x/second anyway to
    // keep the heartbeat alive, so folding the commands into that same
    // request halves the traffic and keeps the joystick responsive.
    void handleCommand() {
        _lastRequestMs = millis();
        if (onHeartbeat) onHeartbeat();

        if (_server.hasArg("s")) {
            // Explicit STOP button -- handled before anything else so it
            // always wins over a stale joystick value in the same request.
            if (onStop) onStop();
        } else if (_server.hasArg("a")) {
            if (onResume) onResume();
        } else {
            if (onMove) {
                onMove(_server.arg("v").toFloat(), _server.arg("r").toFloat());
            }
            if (_server.hasArg("p") || _server.hasArg("y")) {
                if (onLook) onLook(_server.arg("p").toFloat(), _server.arg("y").toFloat());
            }
        }

        String status = statusProvider ? statusProvider() : String("?");
        _server.send(200, "text/plain", status);
    }

    static constexpr unsigned long USED_WINDOW_MS = 3000;

    WebServer _server{80};
    bool _active = false;
    bool _apStarted = false;
    unsigned long _lastRequestMs = 0;

    // Self-contained: no CDN, no external asset -- there is no Internet
    // on this AP by definition. Kept deliberately plain (no framework,
    // ARCHITECTURE_AND_ROADMAP.md §27 rule 10).
    static const char PAGE[] PROGMEM;
};

inline const char StandaloneControl::PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset=utf-8>
<meta name=viewport content='width=device-width,initial-scale=1,user-scalable=no'>
<title>Rover - pilotage direct</title><style>
body{font-family:sans-serif;margin:0;padding:1em;background:#111;color:#eee;
 -webkit-user-select:none;user-select:none;touch-action:none}
h1{font-size:1.1em;margin:0 0 .5em}
#pad{width:100%;max-width:340px;aspect-ratio:1;margin:1em auto;border-radius:50%;
 background:#1d1d24;border:2px solid #333;position:relative;touch-action:none}
#knob{width:22%;height:22%;border-radius:50%;background:#4a9;position:absolute;
 left:39%;top:39%;pointer-events:none}
button{width:100%;padding:.9em;font-size:1em;margin:.3em 0;border:0;border-radius:6px}
#stop{background:#b33;color:#fff;font-weight:bold}
#arm{background:#284;color:#fff}
#st{font-family:monospace;font-size:.85em;color:#9ab;min-height:1.2em}
</style></head><body>
<h1>Rover &mdash; pilotage direct</h1>
<div id=st>...</div>
<div id=pad><div id=knob></div></div>
<button id=stop>STOP</button>
<button id=arm>Activer (sortir de SAFE)</button>
<script>
var pad=document.getElementById('pad'),knob=document.getElementById('knob'),
    st=document.getElementById('st');
var v=0,r=0,pending=null;

function setKnob(nx,ny){knob.style.left=(39+nx*39)+'%';knob.style.top=(39+ny*39)+'%';}

function fromEvent(e){
  var t=e.touches?e.touches[0]:e, b=pad.getBoundingClientRect();
  // -1..1 on each axis, clamped to the circle so a drag outside the pad
  // saturates instead of producing a larger-than-full-scale command.
  var nx=((t.clientX-b.left)/b.width)*2-1, ny=((t.clientY-b.top)/b.height)*2-1;
  var m=Math.hypot(nx,ny); if(m>1){nx/=m;ny/=m;}
  setKnob(nx,ny);
  // Up on screen = forward. Values stay small on purpose: this is a
  // walk-alongside fallback control, not a racing mode.
  v=(-ny*0.30).toFixed(2); r=(nx*1.50).toFixed(2);
}
function release(){v=0;r=0;setKnob(0,0);}

pad.addEventListener('touchstart',fromEvent);
pad.addEventListener('touchmove',fromEvent);
pad.addEventListener('touchend',release);
pad.addEventListener('mousedown',function(e){pad._d=1;fromEvent(e);});
pad.addEventListener('mousemove',function(e){if(pad._d)fromEvent(e);});
window.addEventListener('mouseup',function(){pad._d=0;release();});

function send(extra){
  var q='/c?v='+v+'&r='+r+(extra||'');
  var x=new XMLHttpRequest();
  x.open('GET',q,true);
  x.timeout=1500;
  x.onload=function(){st.textContent=x.responseText;};
  x.onerror=x.ontimeout=function(){st.textContent='-- lien perdu --';};
  x.send();
}
document.getElementById('stop').onclick=function(){release();send('&s=1');};
document.getElementById('arm').onclick=function(){send('&a=1');};

// 200ms: comfortably under the firmware's heartbeat timeout, so simply
// having this page open keeps the robot out of SAFE -- and closing it
// (or walking out of WiFi range) stops the motors on its own, through
// the exact same timeout the Pi relies on. No separate safety path.
setInterval(send,200);
</script></body></html>)HTML";
