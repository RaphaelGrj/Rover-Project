#pragma once

#include <esp_camera.h>

// Rover ESP32-CAM -- deported camera module, independent of the main
// ESP32 (ARCHITECTURE_AND_ROADMAP.md §4.3): "the ESP32-CAM films, the
// Pi analyzes." Config constants only; see src/main.cpp for the logic.

// WiFi credentials come from platformio.ini's build_flags (env vars,
// see esp32/OTA.md for the same convention on the main firmware) --
// the #ifndef fallback here only avoids an undefined-macro compile
// error if this file is ever included without them defined (e.g. a
// stray IDE parse), it is not a real default.
#ifndef ROVER_WIFI_SSID
#define ROVER_WIFI_SSID ""
#endif
#ifndef ROVER_WIFI_PASSWORD
#define ROVER_WIFI_PASSWORD ""
#endif

// Reachable at http://rovercam.local:81/stream on the local network
// without hunting for its DHCP IP -- same idea as the Pi's own
// `rover.local` (pi/README.md "Trouver le Pi sur le réseau").
constexpr char ROVER_CAM_MDNS_NAME[] = "rovercam";

// Kept off port 80 on purpose: a future lightweight status/snapshot
// page could live there without fighting the long-lived stream
// connection. Must match what pi/rover_control/camera.py is
// configured to fetch (--camera-url / config.json "camera_url").
constexpr uint16_t ROVER_CAM_STREAM_PORT = 81;
constexpr char ROVER_CAM_STREAM_PATH[] = "/stream";

// "Situational preview" for the control page, not a low-latency FPV
// feed (same target pi/rover_control/camera.py already documented for
// the Pi Camera it no longer uses) -- VGA is plenty and lighter on
// the Pi 3B+/WiFi than a higher resolution would be. Quality is the
// esp32-camera 0-63 scale, lower number = better quality/bigger
// frames.
constexpr framesize_t ROVER_CAM_FRAME_SIZE = FRAMESIZE_VGA;
constexpr int ROVER_CAM_JPEG_QUALITY = 12;

// Onboard flash LED (see camera_pins.h) -- off by default, no
// per-request control exposed yet.
constexpr bool ROVER_CAM_FLASH_ON = false;
