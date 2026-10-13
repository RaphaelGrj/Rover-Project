#pragma once

#include <Arduino.h>
#include "camera_config.h"

// The camera of the custom PCB, served as an MJPEG stream on
// ROVER_CAM_STREAM_PORT for the Raspberry Pi to relay (the Pi stays the
// authenticated gateway, pi/README.md "Sécurité"). Ported from the
// standalone ESP32-CAM firmware (esp32-cam/src/main.cpp), with the
// changes living inside the main firmware requires:
//
// - It never writes to Serial: Serial carries the Rover Protocol, and
//   stray text lines (from another task, mid-frame) would corrupt it.
//   Failures are counted here and reported by main.cpp as frames.
// - One stream at a time: the sensor can only be re-initialized safely
//   while no handler is inside esp_camera_fb_get(), and the Pi is the
//   only expected client anyway. A second client gets a 503.
// - XCLK uses an LEDC channel/timer the motors and servo do not
//   (camera_config.h).
//
// On boards without a camera every method is a no-op, so main.cpp
// calls them unconditionally.
class RoverCamera {
public:
    // Initializes the sensor. Non-fatal: a missing/failed camera is
    // retried from update(), and the rest of the robot runs regardless.
    void begin();

    // Non-blocking, call every loop() iteration. Retries the sensor and
    // starts the HTTP server as soon as the network stack is up (STA or
    // the standalone AP) -- starting it earlier would crash in lwIP.
    void update();

    // Call when the station link comes up: makes sure the robot answers
    // on ROVER_MDNS_HOSTNAME.local even when OTA (which normally starts
    // mDNS) is not armed.
    void onStationConnected();

    bool ready() const;

    // True once per transition into "camera not working" (failed init,
    // or a stream that gave up on a wedged sensor), for an ERROR report.
    bool consumeFailureEvent();

    // "camera_ready=1 cam_server=1 cam_streaming=0 cam_fails=0".
    void buildStatusFields(char* out, size_t outLen) const;
};
