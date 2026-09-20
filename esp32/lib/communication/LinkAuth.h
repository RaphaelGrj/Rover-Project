#pragma once

#include <Arduino.h>
#include <stddef.h>
#include "RoverProtocol.h"

// Access control for the Rover Protocol command link.
//
// Why this exists (ARCHITECTURE_AND_ROADMAP.md §6.2, question "5 bis",
// a blocking prerequisite of the deported-Pi work)
// -------------------------------------------------------------------
// While the link was a USB cable, authentication was physical: to send
// a MOVE you had to be in the room with a hand on the robot. Nobody
// ever wrote that down because nobody needed to.
//
// Moving the link onto a TCP socket removes that protection entirely
// and replaces it with nothing: the Rover Protocol has no notion of
// identity, so a WiFiServer accepting the first connection that comes
// along hands anyone on the WiFi full control of the motors, the servos
// and the SAFE mode -- no password, no trace. That would make the
// command link the only unauthenticated channel on the robot, and the
// most dangerous of the three: the control server already requires a
// token (pi/rover_control/auth.py) and OTA refuses to start without a
// password (RoverOTA.h).
//
// Requirement: the ESP32 must not leave READY on a network connection
// until a shared secret has been presented.
//
// Threat model, stated plainly
// ----------------------------
// The secret travels in clear text inside an AUTH frame, protected only
// by the WPA2 encryption of the network underneath. So this defends
// against "anyone who can reach the robot's IP", not against "anyone
// who already has the WiFi password and is capturing packets". That is
// the same bar ArduinoOTA and the control token sit at, and it is the
// bar §6.2 set. A challenge/response (nonce + HMAC) would raise it and
// is the natural next increment; it is deliberately not done here
// because it needs a hash on both ends and this needs to land before
// the USB cable is unplugged, not after.
//
// Fail closed, on purpose
// -----------------------
// A network transport with NO secret stored refuses every frame, rather
// than falling back to open access. This follows StandaloneControl
// ("refuses to start without a stored password") and RoverOTA ("no
// unauthenticated flashing by anyone on the LAN") -- in this project an
// unconfigured secret means locked, never wide open. The robot is not
// bricked by that: it is still drivable through standalone piloting
// (§6.3) by whoever is standing next to it, and still reachable over
// USB, where this whole mechanism stays inert.
//
// Inert on the cable
// ------------------
// requireAuth(false) -- the default -- lets every frame straight
// through, so today's USB firmware behaves exactly as before. The
// socket transport (step 1 of the chantier) is what will turn it on.
//
// No hardware, no Arduino peripherals, no network: this is pure logic
// so it can be exercised by `pio test -e native` like RoverProtocol.
class LinkAuth {
public:
    // What the caller should do with the frame it just received.
    enum class Decision {
        // Not an auth matter: dispatch the frame normally.
        Allow,
        // A valid AUTH frame: the link is now authenticated. Do NOT
        // dispatch it -- AUTH is consumed here, it is not a command.
        Accepted,
        // Refuse and disconnect. The caller sends ERROR code=<reason()>
        // and drops the connection: a peer that failed to authenticate
        // gets one attempt, not an open socket to keep guessing on.
        Rejected,
    };

    // Longest secret that fits in a frame field (RoverFrame limits a
    // value to MAX_VALUE_LEN including its terminator). Anything longer
    // would be silently truncated by the parser and then never match,
    // which is a miserable thing to debug -- so the portal rejects
    // over-long secrets up front instead.
    static constexpr size_t MAX_SECRET_LEN = RoverFrame::MAX_VALUE_LEN - 1;

    // Minimum length accepted when storing a secret. Not a
    // cryptographic threshold, just the same "don't let someone set
    // something trivially guessable" floor WPA2 imposes on the
    // standalone password, and the reason the portal can refuse one.
    static constexpr size_t MIN_SECRET_LEN = 8;

    // Empty secret = nothing configured. On a network transport that
    // means everything is refused (see "fail closed" above).
    void setSecret(const char* secret) {
        if (secret == nullptr) secret = "";
        strncpy(_secret, secret, sizeof(_secret) - 1);
        _secret[sizeof(_secret) - 1] = '\0';
    }

    // Set by the transport, not by policy: false for the USB cable
    // (physical access is the authentication), true for a socket.
    void requireAuth(bool required) {
        _required = required;
        if (!required) _authenticated = false;
    }

    // Call on every new connection. Authentication is per-connection,
    // never remembered: a link that drops and comes back is a new peer
    // as far as this is concerned, and must prove itself again.
    void reset() { _authenticated = false; }

    bool isRequired() const { return _required; }
    bool isAuthenticated() const { return !_required || _authenticated; }
    bool hasSecret() const { return _secret[0] != '\0'; }

    // Why the last Rejected happened -- sent verbatim as the ERROR
    // code, so a failure is diagnosable from the Pi's log alone. The
    // distinction matters in practice: "I typed the wrong secret" and
    // "the robot has no secret stored" look identical from the outside
    // otherwise, and the second one is fixed at the robot, not the Pi.
    const char* reason() const { return _reason; }

    Decision evaluate(const RoverFrame& frame) {
        if (!_required) return Decision::Allow;

        bool isAuthFrame = (strcmp(frame.type, "AUTH") == 0);

        if (_authenticated) {
            // A second AUTH on an already-authenticated link is
            // accepted and consumed rather than treated as a command.
            // It is what a Pi that reconnected without the ESP32
            // noticing the drop would send, and there is nothing to
            // gain by refusing it.
            return isAuthFrame ? Decision::Accepted : Decision::Allow;
        }

        if (!isAuthFrame) {
            // The whole point: no MOVE, no SYSTEM, not even a
            // HEARTBEAT gets through before the peer has proven itself.
            _reason = "unauthenticated";
            return Decision::Rejected;
        }

        if (!hasSecret()) {
            _reason = "link_secret_not_set";
            return Decision::Rejected;
        }

        char candidate[RoverFrame::MAX_VALUE_LEN];
        if (!frame.getField("secret", candidate, sizeof(candidate))) {
            _reason = "unauthenticated";
            return Decision::Rejected;
        }

        if (!constantTimeEquals(candidate, _secret)) {
            _reason = "unauthenticated";
            return Decision::Rejected;
        }

        _authenticated = true;
        return Decision::Accepted;
    }

    // Compares MAX_SECRET_LEN bytes with no early exit whatsoever, so
    // neither the number of matching leading characters nor the length
    // of the stored secret is observable through timing. Mirrors what
    // pi/rover_control/auth.py already does with hmac.compare_digest on
    // the control token -- a minor concern on a hobby LAN, but this
    // code is run unmodified by people who did not write it, and not
    // cutting the corner costs nothing.
    //
    // PRECONDITION, and the reason this is safe: both arguments must be
    // buffers of at least MAX_SECRET_LEN bytes, fully initialized. Both
    // call sites satisfy it through strncpy, which pads the whole
    // destination with '\0' rather than stopping at the terminator --
    // _secret via setSecret(), the candidate via RoverFrame::getField().
    // A shorter string therefore compares as itself followed by zeros,
    // which is exactly what makes a fixed-length walk both correct and
    // in-bounds. Do not call this with a bare string literal.
    static bool constantTimeEquals(const char* a, const char* b) {
        uint8_t diff = 0;
        for (size_t i = 0; i < MAX_SECRET_LEN; i++) {
            diff |= (uint8_t)((uint8_t)a[i] ^ (uint8_t)b[i]);
        }
        return diff == 0;
    }

private:
    char _secret[MAX_SECRET_LEN + 1] = {0};
    bool _required = false;
    bool _authenticated = false;
    const char* _reason = "unauthenticated";
};
