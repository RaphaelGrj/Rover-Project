#pragma once

#include <Arduino.h>
#include "board_config.h"

// Human-readable name of a RoverState, used both for STATE frames sent
// to the Pi and for anything logged locally.
inline const char* roverStateName(RoverState state) {
    switch (state) {
        case RoverState::BOOT: return "BOOT";
        case RoverState::READY: return "READY";
        case RoverState::ACTIVE: return "ACTIVE";
        case RoverState::SAFE: return "SAFE";
        case RoverState::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

// Builds the "key=value key=value ..." field list for a diagnostic
// STATE frame (answer to `SYSTEM action=diag`, see ROVER_PROTOCOL.md).
// `out` must be at least ~90 bytes; caller owns the buffer so this stays
// allocation-free (no heap use in the hot path).
inline void buildDiagnosticsFields(char* out, size_t outLen, RoverState state) {
    snprintf(out, outLen,
             "uptime_ms=%lu free_heap=%u state=%s board=%s protocol=%s",
             millis(), ESP.getFreeHeap(), roverStateName(state),
             ROVER_BOARD_NAME, ROVER_PROTOCOL_VERSION);
}
