#pragma once

#if !defined(ROVER_BOARD_WROOM) && !defined(ROVER_BOARD_S3)
#error "No Rover board target defined. Build with the esp32_wroom or esp32_s3 PlatformIO environment."
#endif

#ifndef ROVER_BOARD_NAME
#define ROVER_BOARD_NAME "UNKNOWN"
#endif

#ifndef ROVER_PROTOCOL_VERSION
#define ROVER_PROTOCOL_VERSION "ROVER_PROTOCOL_V1"
#endif

// See ROVER_PROTOCOL.md section 6.
constexpr unsigned long ROVER_HEARTBEAT_TIMEOUT_MS = 500;

// See ROVER_PROTOCOL.md section 3.2.
constexpr size_t ROVER_MAX_FRAME_LEN = 128;

enum class RoverState {
    BOOT,
    READY,
    ACTIVE,
    SAFE,
    ERROR
};
