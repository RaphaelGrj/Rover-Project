#include "DistanceSensor.h"
#include "sensors_config.h"
#include "Watchdog.h"
#include "I2CProbe.h"

namespace {

// Sentinels distinct from any real reading (VL53L0X's usable range tops
// out around 2000mm), so a STATE consumer can tell "sensor answered but
// sees nothing in range" apart from "sensor isn't there at all", even
// though neither currently gets special-cased on the Pi side.
constexpr uint16_t OUT_OF_RANGE_MM = 8190;  // VL53L0X's own "no target" sentinel
constexpr uint16_t UNAVAILABLE_MM = 9999;   // begin() never succeeded / sensor lost since

}  // namespace

void DistanceSensor::begin() {
    pinMode(ROVER_PIN_TOF_LEFT_XSHUT, OUTPUT);
    pinMode(ROVER_PIN_TOF_RIGHT_XSHUT, OUTPUT);
    // Hold both in reset first so neither answers on the shared factory
    // address while they're brought up one at a time below.
    digitalWrite(ROVER_PIN_TOF_LEFT_XSHUT, LOW);
    digitalWrite(ROVER_PIN_TOF_RIGHT_XSHUT, LOW);
    delay(10);

    beginLeft();
    RoverWatchdog::feed();  // an absent sensor can take a while to time out on I2C, see SensorHub.cpp
    beginRight();
}

void DistanceSensor::beginLeft() {
    digitalWrite(ROVER_PIN_TOF_LEFT_XSHUT, HIGH);
    delay(10);  // boot time after XSHUT release, per the VL53L0X datasheet
    // Ping the factory address first -- see I2CProbe.h for why this
    // must happen before the library's own begin(), not after.
    if (i2cDevicePresent(ROVER_TOF_DEFAULT_ADDRESS)) {
        // Re-addressed off the factory default so it can coexist once
        // the right sensor (still on that default) is released next.
        _leftOk = _left.begin(ROVER_TOF_LEFT_ADDRESS);
        if (_leftOk) _left.startRangeContinuous();
    } else {
        _leftOk = false;
    }
    _lastLeftRetryMs = millis();
    _leftMm = _leftOk ? OUT_OF_RANGE_MM : UNAVAILABLE_MM;
}

void DistanceSensor::beginRight() {
    digitalWrite(ROVER_PIN_TOF_RIGHT_XSHUT, HIGH);
    delay(10);
    if (i2cDevicePresent(ROVER_TOF_DEFAULT_ADDRESS)) {
        // Stays on the factory default -- only the left one had to
        // move, since it was already up and reachable when the right
        // boots.
        _rightOk = _right.begin(ROVER_TOF_DEFAULT_ADDRESS);
        if (_rightOk) _right.startRangeContinuous();
    } else {
        _rightOk = false;
    }
    _lastRightRetryMs = millis();
    _rightMm = _rightOk ? OUT_OF_RANGE_MM : UNAVAILABLE_MM;
}

void DistanceSensor::update() {
    unsigned long now = millis();

    // A sensor that failed begin() (not wired yet, only one unit is
    // available as of this writing -- see PROGRESS.md) gets occasional
    // retries rather than being given up on for the rest of the boot.
    if (!_leftOk && now - _lastLeftRetryMs >= ROVER_SENSOR_RETRY_PERIOD_MS) {
        beginLeft();
    }
    if (!_rightOk && now - _lastRightRetryMs >= ROVER_SENSOR_RETRY_PERIOD_MS) {
        beginRight();
    }

    if (_leftOk && _left.isRangeComplete()) {
        uint16_t mm = _left.readRangeResult();
        _leftMm = (mm >= OUT_OF_RANGE_MM) ? OUT_OF_RANGE_MM : mm;
    }
    if (_rightOk && _right.isRangeComplete()) {
        uint16_t mm = _right.readRangeResult();
        _rightMm = (mm >= OUT_OF_RANGE_MM) ? OUT_OF_RANGE_MM : mm;
    }

    evaluateObstacle();
}

void DistanceSensor::evaluateObstacle() {
    bool anyClose = (_leftOk && _leftMm < ROVER_OBSTACLE_THRESHOLD_MM) ||
                    (_rightOk && _rightMm < ROVER_OBSTACLE_THRESHOLD_MM);
    bool allClear = (!_leftOk || _leftMm > ROVER_OBSTACLE_CLEAR_MM) &&
                    (!_rightOk || _rightMm > ROVER_OBSTACLE_CLEAR_MM);
    if (anyClose) _obstacleActive = true;
    else if (allClear) _obstacleActive = false;
    // Between the two thresholds: keep whatever _obstacleActive already was.
}

void DistanceSensor::buildTelemetryFields(char* out, size_t outLen) const {
    snprintf(out, outLen, "distance_left=%u distance_right=%u", _leftMm, _rightMm);
}
