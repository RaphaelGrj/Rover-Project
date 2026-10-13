#pragma once

#include <cstdint>

// Camera integrated on the custom PCB (OV2640 on the S3's DVP port),
// replacing the separate ESP32-CAM module (ARCHITECTURE_AND_ROADMAP.md
// §4.3) -- same job, same stream URL shape, now served by the main
// firmware. Only the PCB has a camera: on the WROOM/S3 devkits this
// header defines nothing that gets used, and RoverCamera compiles to
// no-ops (CLAUDE.md: S3-only peripherals stay behind #ifdef).
#if defined(ROVER_BOARD_PCB_V1)
#define ROVER_HAS_CAMERA 1
#endif

// Reachable at http://rover-esp32.local:81/stream -- the robot's
// existing mDNS name (RoverOTA.h), not a second one: one board, one
// name. pi/rover_control/camera.py's camera_url must point here instead
// of the old http://rovercam.local:81/stream.
constexpr char ROVER_MDNS_HOSTNAME[] = "rover-esp32";
// Same port/path as the ESP32-CAM firmware (esp32-cam/include/
// cam_config.h), so only the host part of the Pi's URL changes. Off
// port 80, which the provisioning portal and standalone page use.
constexpr uint16_t ROVER_CAM_STREAM_PORT = 81;
constexpr char ROVER_CAM_STREAM_PATH[] = "/stream";

// "Situational preview", not low-latency FPV -- same reasoning as the
// ESP32-CAM: VGA (set in RoverCamera.cpp, where esp_camera.h's
// framesize_t is visible) at quality 12 (0-63, lower = better/bigger).
constexpr int ROVER_CAM_JPEG_QUALITY = 12;

// 20 MHz is what the OV2640 is characterised at. If the image breaks up
// on the long ribbon up into the head (pcb/PCB_CONNEXIONS.md §5.7),
// 10 MHz is the first thing to try.
constexpr int ROVER_CAM_XCLK_HZ = 20000000;

// LEDC resources for XCLK. The S3 has only 8 LEDC channels / 4 timers
// (vs 16/8 on the WROOM): motors use channels 0-3 (timers 0-1), the head
// servo channel 4 (timer 2, shared with the unused channel 5). Channel 7
// on timer 3 is the one pair nothing else touches -- the camera's
// default (channel 0, timer 0) would silently steal the left motor's
// PWM. Values are the raw ledc_channel_t / ledc_timer_t numbers.
constexpr int ROVER_CAM_LEDC_CHANNEL = 7;
constexpr int ROVER_CAM_LEDC_TIMER = 3;

// A camera that failed to start (ribbon reseated after power-up, for
// instance) is retried at this period rather than left dead until reboot.
constexpr unsigned long ROVER_CAM_RETRY_PERIOD_MS = 5000;
