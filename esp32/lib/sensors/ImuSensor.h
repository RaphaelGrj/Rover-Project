#pragma once

#include <Arduino.h>
#include <Adafruit_MPU6050.h>

// Wraps the MPU6050 IMU. Independent begin()/health tracking from the
// other I2C sensors (ARCHITECTURE_AND_ROADMAP.md section 21): a
// missing/failed IMU must never affect ToF or environment readings.
class ImuSensor {
public:
    void begin();

    // Non-blocking, call every loop() iteration; internally rate-limited
    // to ROVER_SENSOR_UPDATE_PERIOD_MS.
    void update();

    bool ok() const { return _ok; }

    // Fills "accel_x=... accel_y=... accel_z=... gyro_x=... gyro_y=...
    // gyro_z=..." (m/s^2, rad/s) for a STATE frame.
    void buildTelemetryFields(char* out, size_t outLen) const;

private:
    Adafruit_MPU6050 _mpu;
    bool _ok = false;
    float _accelX = 0.0f, _accelY = 0.0f, _accelZ = 0.0f;
    float _gyroX = 0.0f, _gyroY = 0.0f, _gyroZ = 0.0f;
    unsigned long _lastUpdateMs = 0;
    unsigned long _lastRetryMs = 0;
};
