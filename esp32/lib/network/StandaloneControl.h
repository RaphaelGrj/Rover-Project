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
    // Arms/disarms the local obstacle reflex -- same switch as
    // SYSTEM action=obstacle_reflex, routed through main.cpp rather
    // than reaching into DriveController from here (this module owns no
    // safety logic of its own, see the class comment).
    std::function<void(bool enabled)> onObstacleReflex;
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
    // reappears -- the page sends at least one request every 250ms
    // (keep-alive or command), so a few seconds of silence means nobody
    // is holding the phone any more.
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
        // String: this page is ~7KB (it grew with the speed bar and the
        // obstacle switch) and RAM is the scarcer resource once WiFi is up.
        _server.send_P(200, "text/html", PAGE);
    }

    // GET /c?v=<velocity>&r=<rotation>[&p=&y=][&a=1][&s=1][&h=1][&o=0|1]
    // One endpoint, not five: the page polls it ~5x/second anyway to
    // keep the heartbeat alive, so folding the commands into that same
    // request halves the traffic and keeps the joystick responsive.
    //
    // h=1 is that poll and nothing else: "I am still here", with no
    // drive command attached. Since the speed-bar rework (2026-09-21)
    // the page only sends v/r when the operator actually changes
    // something -- a fixed speed plus a direction-only joystick means
    // the command is a handful of distinct values, not a new float
    // five times a second. The heartbeat floor (ROVER_HEARTBEAT_TIMEOUT_MS,
    // 500ms) still forces a request several times a second, so the win
    // is not in the request count: it is that a keep-alive no longer
    // re-targets the PID with a value that never changed.
    void handleCommand() {
        _lastRequestMs = millis();
        if (onHeartbeat) onHeartbeat();

        if (_server.hasArg("s")) {
            // Explicit STOP button -- handled before anything else so it
            // always wins over a stale joystick value in the same request.
            if (onStop) onStop();
        } else if (_server.hasArg("a")) {
            if (onResume) onResume();
        } else if (_server.hasArg("o")) {
            // Obstacle-reflex switch. Its own branch rather than a flag
            // riding along with v/r: it is an operator decision, not
            // part of a drive command, and pairing the two would make
            // every joystick update re-assert a safety setting.
            if (onObstacleReflex) onObstacleReflex(_server.arg("o").toInt() != 0);
        } else if (_server.hasArg("h")) {
            // Keep-alive only: the heartbeat above is the whole point of
            // the request. Deliberately leaves the current target alone
            // -- exactly like a HEARTBEAT frame from the Pi, which also
            // holds ACTIVE without restating the last MOVE.
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
#row{display:flex;align-items:center;justify-content:center;gap:1em;margin:1em 0}
#pad{flex:0 1 300px;max-width:300px;aspect-ratio:1;border-radius:50%;
 background:#1d1d24;border:2px solid #333;position:relative;touch-action:none}
#knob{width:22%;height:22%;border-radius:50%;background:#4a9;position:absolute;
 left:39%;top:39%;pointer-events:none}
#spd{display:flex;flex-direction:column;align-items:center;gap:.4em}
/* Vertical range input: writing-mode is the standard way (Chrome 121+,
   Safari 17.4+), -webkit-appearance the older WebKit/Blink one. Both are
   declared because this page has to work on whatever browser the phone
   in the operator's hand happens to run -- there is no CDN here to
   polyfill anything. */
#sp{writing-mode:vertical-lr;direction:rtl;-webkit-appearance:slider-vertical;
 width:2.4em;height:min(60vw,260px);accent-color:#4a9}
#spv{font-family:monospace;font-size:.9em;color:#eee}
#spl{font-size:.75em;color:#9ab}
button{width:100%;padding:.9em;font-size:1em;margin:.3em 0;border:0;border-radius:6px}
#stop{background:#b33;color:#fff;font-weight:bold}
#arm{background:#284;color:#fff}
/* Armed = quiet grey (that is the normal, safe state, it should not
   shout). Disarmed = amber, because a disabled safety reflex must never
   look like business as usual. */
#obs{background:#2a2a33;color:#ccd}
#obs.off{background:#a60;color:#fff;font-weight:bold}
#st{font-family:monospace;font-size:.85em;color:#9ab;min-height:1.2em}
</style></head><body>
<h1>Rover &mdash; pilotage direct</h1>
<div id=st>...</div>
<div id=row>
  <div id=pad><div id=knob></div></div>
  <div id=spd>
    <div id=spv>40%</div>
    <input id=sp type=range min=0 max=100 step=5 value=40 orient=vertical aria-label=Vitesse>
    <div id=spl>vitesse</div>
  </div>
</div>
<button id=stop>STOP</button>
<button id=arm>Activer (sortir de SAFE)</button>
<button id=obs>Arret sur obstacle : ACTIF</button>
<script>
// Speed bar + direction-only joystick (2026-09-21). The joystick used to
// set BOTH direction and magnitude, which had two problems: the speed
// changed every time the thumb drifted, and every drift was a new float
// pushed to the ESP32 several times a second. Here the bar alone decides
// how fast, the pad alone decides where -- so a steady drive is a single
// command repeated, not a stream of slightly different ones (see
// handleCommand()'s h=1 keep-alive).
var pad=document.getElementById('pad'),knob=document.getElementById('knob'),
    st=document.getElementById('st'),sp=document.getElementById('sp'),
    spv=document.getElementById('spv');

// Ceilings, reached at 100% on the bar.
//
// VMAX mirrors ROVER_MAX_WHEEL_SPEED_MPS, which since 2026-09-21 is a
// MEASURED figure (0.029 m/s at full duty) rather than the aspirational
// 0.30 that was there before -- a ceiling ten times the achievable
// speed made every command an unreachable setpoint and pinned the PWM
// at 255. Unlike the Pi's page, this one cannot learn the robot's
// calibrated value (SYSTEM action=set_speed): keep it in step by hand
// if that gets re-measured. Asking for less than the robot can give is
// the safe direction to be wrong in.
//
// RMAX is DERIVED, not chosen: the unicycle model gives each wheel
// v +/- rotation * wheelbase/2, so the fastest spin the wheels can
// actually deliver is 2 * VMAX / wheelbase (0.15m). The old 1.50 rad/s
// asked each wheel for 0.11 m/s -- nearly four times what this chassis
// can do -- so every single turn saturated.
var VMAX=0.03, RMAX=(2*0.03)/0.15;
// Below this fraction of the pad radius the thumb counts as centred.
// A direction-only stick has no small commands to ease into, so without
// a deadzone the tiniest touch is a full-speed start.
var DEAD=0.25;

var dx=0,dy=0;                 // unit direction, 0/0 = stopped
var lastSent='',lastReqMs=0;   // for the change-driven send below

function throttle(){ return sp.value/100; }
// Rounded first, THEN de-signed: a centred axis is rarely exactly zero
// (normalising a near-centre touch leaves something like -1e-17), so
// testing the number would miss it -- only the rendered string shows
// the problem. "-0.00" parses fine on the firmware side, but this
// project reads its own telemetry by eye far too often to leave it in.
// Four decimals, not two: the whole speed range is 0 to 0.03 m/s, so
// two decimals collapsed this bar's 21 positions into FOUR distinct
// commands and made anything under 15% of the bar round to 0.00 -- the
// robot simply would not move. A format has to out-resolve the range it
// carries; two decimals only ever sufficed because the old ceiling was
// ten times too high. The de-signing is cosmetic: a centred axis is
// rarely exactly zero (normalising a near-centre touch leaves something
// like -1e-17), so only the rendered string shows the problem.
function fmt(x){ var t=x.toFixed(4); return t==='-0.0000'?'0.0000':t; }
function vel(){ return fmt(-dy*throttle()*VMAX); }
function rot(){ return fmt(dx*throttle()*RMAX); }

function setKnob(nx,ny){knob.style.left=(39+nx*39)+'%';knob.style.top=(39+ny*39)+'%';}

function fromEvent(e){
  var t=e.touches?e.touches[0]:e, b=pad.getBoundingClientRect();
  // -1..1 on each axis, clamped to the circle so a drag outside the pad
  // saturates instead of escaping it.
  var nx=((t.clientX-b.left)/b.width)*2-1, ny=((t.clientY-b.top)/b.height)*2-1;
  var m=Math.hypot(nx,ny); if(m>1){nx/=m;ny/=m;m=1;}
  // The knob follows the finger (that is the feedback the thumb expects),
  // but the COMMAND only keeps the direction: normalised to unit length,
  // so half-way out and all the way out drive at the same speed -- the
  // one the bar is set to. Up on screen = forward.
  setKnob(nx,ny);
  if(m<DEAD){dx=0;dy=0;} else {dx=nx/m;dy=ny/m;}
}
function release(){dx=0;dy=0;setKnob(0,0);}

pad.addEventListener('touchstart',fromEvent);
pad.addEventListener('touchmove',fromEvent);
pad.addEventListener('touchend',release);
pad.addEventListener('mousedown',function(e){pad._d=1;fromEvent(e);});
pad.addEventListener('mousemove',function(e){if(pad._d)fromEvent(e);});
window.addEventListener('mouseup',function(){pad._d=0;release();});
sp.addEventListener('input',function(){spv.textContent=sp.value+'%';});

// `commits` is the command this request would deliver, recorded ONLY
// once the robot has acknowledged it. Recording it up front (which is
// what this did at first) meant a request lost to a timeout was never
// retried: the tick below only resends when the command CHANGES, while
// the h=1 keep-alives went on holding the heartbeat -- so a dropped
// "stop" left the robot driving on its last target indefinitely. An
// unacknowledged command now simply goes out again on the next tick.
function send(q,commits){
  lastReqMs=Date.now();
  var x=new XMLHttpRequest();
  x.open('GET',q,true);
  x.timeout=1500;
  x.onload=function(){if(commits!==undefined)lastSent=commits;st.textContent=x.responseText;};
  x.onerror=x.ontimeout=function(){st.textContent='-- lien perdu --';};
  x.send();
}
function sendMove(extra){
  send('/c?v='+vel()+'&r='+rot()+(extra||''), vel()+','+rot());
}
document.getElementById('stop').onclick=function(){release();sendMove('&s=1');};
document.getElementById('arm').onclick=function(){
  // Forget what was last delivered: entering SAFE cleared the robot's
  // target (DriveController::stop), so anything remembered here is
  // stale the moment it re-arms. Without this, a stick still held
  // produces no MOVE at all after "Activer" -- the command has not
  // changed, so nothing is sent, and the robot sits there. RoverCore.
  // resume() drops its own cache for exactly this reason.
  lastSent='';
  send('/c?a=1');
};

// Obstacle reflex switch. The DISTANCES keep being measured and shown
// either way -- this only stops the robot acting on them, it does not
// blind it.
//
// The button flips at once and ROLLS BACK if the request does not land,
// rather than claiming a state the robot never adopted. This page has
// no telemetry channel to re-read the truth from (unlike the Pi's,
// which follows STATE obstacle_reflex=), so not rolling back would
// leave it lying with nothing to correct it. The status line above is
// the authority either way: it spells out "reflexe obstacle desactive".
// An ESP32 reboot re-arms the reflex, but it also drops this AP, so
// that case shows up as "-- lien perdu --" rather than a stale button.
var obs=document.getElementById('obs'),obsOn=true;
function paintObs(){
  obs.textContent='Arret sur obstacle : '+(obsOn?'ACTIF':'DESACTIVE');
  obs.className=obsOn?'':'off';
}
obs.onclick=function(){
  var wanted=!obsOn;
  obsOn=wanted;paintObs();
  var x=new XMLHttpRequest();
  x.open('GET','/c?o='+(wanted?1:0),true);
  x.timeout=1500;
  lastReqMs=Date.now();
  x.onload=function(){st.textContent=x.responseText;};
  x.onerror=x.ontimeout=function(){
    obsOn=!wanted;paintObs();
    st.textContent='-- commande obstacle perdue --';
  };
  x.send();
};

// 100ms tick, but two different requests come out of it:
//   - the command changed -> send it now (so the pad still feels instant);
//   - nothing changed -> a bare keep-alive every 250ms, which holds the
//     heartbeat without restating a target the firmware already has.
// 250ms stays comfortably under the firmware's heartbeat timeout, so
// simply having this page open keeps the robot out of SAFE -- and
// closing it (or walking out of WiFi range) stops the motors on its own,
// through the exact same timeout the Pi relies on. No separate safety path.
setInterval(function(){
  if(vel()+','+rot()!==lastSent) sendMove();
  else if(Date.now()-lastReqMs>=250) send('/c?h=1');
},100);
</script></body></html>)HTML";
