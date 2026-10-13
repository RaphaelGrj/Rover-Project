#include "ImuSensor.h"
#include "I2CProbe.h"

uint8_t ImuSensor::address() const {
#if defined(ROVER_IMU_LSM6DS3TRC)
    return ROVER_LSM6DS3TRC_ADDRESS;
#else
    return ROVER_MPU6050_ADDRESS;
#endif
}

void ImuSensor::begin() {
    // Ping before the library's own begin() -- see I2CProbe.h; the
    // VL53L0X core driver was found to hang on a missing device rather
    // than fail cleanly, so every sensor here probes first defensively,
    // the IMU included, even though it hasn't been individually proven
    // to have the same issue.
    if (i2cDevicePresent(address())) {
#if defined(ROVER_IMU_LSM6DS3TRC)
        _ok = _imu.begin_I2C(address(), &Wire);
        if (_ok) {
            // Same ranges as the MPU6050 setup below, so telemetry
            // saturates at the same values on both boards. 104 Hz is
            // ample for a 200 ms read period and keeps the chip's own
            // filtering meaningful.
            _imu.setAccelRange(LSM6DS_ACCEL_RANGE_8_G);
            _imu.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
            _imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
            _imu.setGyroDataRate(LSM6DS_RATE_104_HZ);
        }
#else
        _ok = _imu.begin(address());
        if (_ok) {
            _imu.setAccelerometerRange(MPU6050_RANGE_8_G);
            _imu.setGyroRange(MPU6050_RANGE_500_DEG);
            _imu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        }
#endif
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

    // Presence check before every read: neither Adafruit driver reports
    // a failed register read through getEvent()'s return value in
    // practice (both return true and hand back stale/garbage values), so
    // without this an IMU unplugged mid-run would keep "reporting" its
    // last numbers forever. One zero-length transaction every 200 ms.
    sensors_event_t accel, gyro, temp;
    if (!i2cDevicePresent(address()) || !_imu.getEvent(&accel, &gyro, &temp)) {
        // Treated the same as a failed begin() so SensorHub's
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
