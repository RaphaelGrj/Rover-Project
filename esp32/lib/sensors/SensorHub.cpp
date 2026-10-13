#include "SensorHub.h"
#include <Wire.h>
#include "sensors_config.h"
#include "Watchdog.h"
#include "BoardIo.h"

void SensorHub::begin() {
    // Explicit pins + a short timeout (a missing device must not stall
    // boot past the watchdog) -- see BoardIo::beginI2C for the full
    // story. Idempotent: on the PCB, BoardIo::begin() has already
    // started the bus for the I/O expander before we get here.
    BoardIo::beginI2C();

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
