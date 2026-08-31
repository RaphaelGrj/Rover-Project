#include "ImuSensor.h"
#include "sensors_config.h"
#include "I2CProbe.h"

void ImuSensor::begin() {
    // Ping before the library's own begin() -- see I2CProbe.h; the
    // VL53L0X core driver was found to hang on a missing device rather
    // than fail cleanly, so every sensor here probes first defensively,
    // MPU6050 included, even though it hasn't been individually proven
    // to have the same issue.
    if (i2cDevicePresent(ROVER_MPU6050_ADDRESS)) {
        _ok = _mpu.begin(ROVER_MPU6050_ADDRESS);
        if (_ok) {
            _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
            _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
            _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        }
    } else {
        _ok = false;
    }
    _lastRetryMs = millis();
}

void ImuSensor::update() {
    unsigned long now = millis();
    if (now - _lastUpdateMs < ROVER_SENSOR_UPDATE_PERIOD_MS) return;
    _lastUpdateMs = now;

    if (!_ok) {
        // Not wired yet (or lost) -- retried periodically rather than
        // permanently given up on, see sensors_config.h.
        if (now - _lastRetryMs >= ROVER_SENSOR_RETRY_PERIOD_MS) begin();
        return;
    }

    sensors_event_t accel, gyro, temp;
    if (!_mpu.getEvent(&accel, &gyro, &temp)) {
        // Previously-working sensor stopped answering on the bus --
        // treated the same as a failed begin() so SensorHub's
        // sensor_timeout ERROR/retry logic covers this case too.
        _ok = false;
        _lastRetryMs = now;
        return;
    }
    _accelX = accel.acceleration.x;
    _accelY = accel.acceleration.y;
    _accelZ = accel.acceleration.z;
    _gyroX = gyro.gyro.x;
    _gyroY = gyro.gyro.y;
    _gyroZ = gyro.gyro.z;
}

void ImuSensor::buildTelemetryFields(char* out, size_t outLen) const {
    snprintf(out, outLen,
             "accel_x=%.2f accel_y=%.2f accel_z=%.2f gyro_x=%.2f gyro_y=%.2f gyro_z=%.2f",
             _accelX, _accelY, _accelZ, _gyroX, _gyroY, _gyroZ);
}
