#pragma once

#include <Arduino.h>
#include "board_config.h"

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

inline void buildDiagnosticsFields(char* out, size_t outLen, RoverState state) {
    snprintf(out, outLen,
             "uptime_ms=%lu free_heap=%u state=%s board=%s protocol=%s",
             millis(), ESP.getFreeHeap(), roverStateName(state),
             ROVER_BOARD_NAME, ROVER_PROTOCOL_VERSION);
}
