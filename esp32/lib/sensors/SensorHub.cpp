#include "SensorHub.h"
#include <Wire.h>
#include "sensors_config.h"
#include "Watchdog.h"

void SensorHub::begin() {
    // Explicit pins, not board defaults -- keeps this portable across
    // WROOM/S3 (their default I2C pins can differ), same approach
    // DisplayEngine already uses for SPI.
    Wire.begin(ROVER_PIN_I2C_SDA, ROVER_PIN_I2C_SCL);
    // Fail fast on a missing device instead of the core's default I2C
    // timeout. First real-hardware test of this module (no sensors
    // wired at all yet) hit exactly this: an absent device on an
    // otherwise-idle bus reads back ESP_ERR_TIMEOUT rather than a quick
    // NACK, and with up to four sensors probed below, that stacked up
    // past the hardware watchdog's 3s window (Watchdog.h) before
    // setup() ever reached loop()'s first feed() -- a boot-loop crash
    // caused by a *missing* sensor, exactly what section 21 says must
    // never happen. 50ms is generous for any device actually present.
    Wire.setTimeOut(50);

    _distance.begin();
    RoverWatchdog::feed();  // each begin() below can take a while with nothing answering on the bus
    _imu.begin();
    RoverWatchdog::feed();
    _environment.begin();
    RoverWatchdog::feed();

    // Reuses update()'s edge-detection so a sensor that fails right at
    // boot is reported exactly once, same as one that drops out later.
    checkFailures();
}

void SensorHub::update() {
    _distance.update();
    _imu.update();
    _environment.update();

    if (_distance.obstacleDetected() && !_obstacleWasActive) {
        _pendingObstacleEvent = true;
    }
    _obstacleWasActive = _distance.obstacleDetected();

    checkFailures();
}

void SensorHub::checkFailures() {
    if (_leftWasOk && !_distance.leftOk()) _leftFailurePending = true;
    if (_rightWasOk && !_distance.rightOk()) _rightFailurePending = true;
    if (_imuWasOk && !_imu.ok()) _imuFailurePending = true;
    if (_envWasOk && !_environment.ok()) _envFailurePending = true;

    _leftWasOk = _distance.leftOk();
    _rightWasOk = _distance.rightOk();
    _imuWasOk = _imu.ok();
    _envWasOk = _environment.ok();
}

bool SensorHub::consumeObstacleEvent() {
    if (!_pendingObstacleEvent) return false;
    _pendingObstacleEvent = false;
    return true;
}

bool SensorHub::consumeSensorFailure(const char** sensorNameOut) {
    if (_leftFailurePending) { _leftFailurePending = false; *sensorNameOut = "tof_left"; return true; }
    if (_rightFailurePending) { _rightFailurePending = false; *sensorNameOut = "tof_right"; return true; }
    if (_imuFailurePending) { _imuFailurePending = false; *sensorNameOut = "imu"; return true; }
    if (_envFailurePending) { _envFailurePending = false; *sensorNameOut = "bme688"; return true; }
    return false;
}
