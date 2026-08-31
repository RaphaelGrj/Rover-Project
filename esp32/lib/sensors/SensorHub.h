#pragma once

#include <Arduino.h>
#include "DistanceSensor.h"
#include "ImuSensor.h"
#include "EnvironmentSensor.h"

// Owns all Phase 4 sensors and turns their raw health/readings into the
// STATE/EVENT/ERROR frames main.cpp sends -- main.cpp only calls
// begin()/update() and drains the two event queues below, it never
// touches a sensor object directly (same ownership pattern
// DriveController/HeadController already use for their own domains).
class SensorHub {
public:
    void begin();

    // Non-blocking, call every loop() iteration.
    void update();

    void buildDistanceFields(char* out, size_t outLen) const { _distance.buildTelemetryFields(out, outLen); }
    void buildImuFields(char* out, size_t outLen) const { _imu.buildTelemetryFields(out, outLen); }
    void buildEnvironmentFields(char* out, size_t outLen) const { _environment.buildTelemetryFields(out, outLen); }

    // True exactly once, the update() where the obstacle first became
    // active -- caller must send EVENT name=obstacle_detected right
    // away, this isn't latched for a second read.
    bool consumeObstacleEvent();

    // Pops one pending sensor failure per call ("tof_left", "tof_right",
    // "imu" or "bme688") until none remain -- call in a loop so
    // simultaneous failures (eg. several sensors unwired at boot) are
    // never silently dropped, only the single most recent one would be
    // if this returned just a bool.
    bool consumeSensorFailure(const char** sensorNameOut);

private:
    DistanceSensor _distance;
    ImuSensor _imu;
    EnvironmentSensor _environment;

    bool _obstacleWasActive = false;
    bool _pendingObstacleEvent = false;

    // Start "true" (not "false") so a sensor that fails its very first
    // begin() at boot -- the common case today, IMU/BME688 aren't wired
    // yet -- is still treated as an OK->failed transition and reported
    // once, the same way a later dropout would be
    // (ARCHITECTURE_AND_ROADMAP.md section 21 wants a failed sensor
    // signaled regardless of whether it ever worked).
    bool _leftWasOk = true, _rightWasOk = true, _imuWasOk = true, _envWasOk = true;
    bool _leftFailurePending = false, _rightFailurePending = false,
         _imuFailurePending = false, _envFailurePending = false;

    void checkFailures();
};
