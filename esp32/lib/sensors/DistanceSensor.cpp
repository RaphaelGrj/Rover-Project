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

// How long a side can go without a fresh continuous-ranging sample
// before it's declared failed. Generous relative to the sensor's own
// ranging period (tens of ms) -- this only needs to catch "gone quiet
// for real" (unplugged, wedged), not ordinary timing jitter.
constexpr unsigned long STALE_SAMPLE_TIMEOUT_MS = 1000;

}  // namespace

void DistanceSensor::begin() {
    // Only the RIGHT sensor's XSHUT is driven; the left one is tied
    // high in hardware. See bringUpBoth() for why one line is enough,
    // and WIRING.md for why it has to be (the 30-pin WROOM devkit does
    // not break GPIO0 out at all, found on the real board 2026-09-20).
    pinMode(ROVER_PIN_TOF_RIGHT_XSHUT, OUTPUT);
    bringUpBoth();
}

// Resets a VL53L0X that is still carrying a previously-programmed
// address back to the factory default, by writing the 7-bit address
// into its I2C_SLAVE_DEVICE_ADDRESS register (0x8A).
//
// Needed because the left sensor has no XSHUT line any more: a
// VL53L0X keeps its programmed address until it loses POWER, and an
// ESP32 reboot (firmware upload, EN button, watchdog reset) does not
// cut the sensor's 3V3. So on the second and every later boot the left
// sensor would still be sitting on ROVER_TOF_LEFT_ADDRESS, answer
// nothing on the factory address, and be reported missing until
// someone power-cycled the whole robot. Raw Wire rather than the
// library, in the same spirit as I2CProbe.h: this has to work on a
// device no Adafruit_VL53L0X instance is bound to yet.
static bool resetToFactoryAddress(uint8_t currentAddress) {
    Wire.beginTransmission(currentAddress);
    Wire.write((uint8_t)0x8A);
    Wire.write((uint8_t)(ROVER_TOF_DEFAULT_ADDRESS & 0x7F));
    if (Wire.endTransmission() != 0) return false;
    delay(10);
    return true;
}

// One XSHUT line is enough to separate two sensors sharing a factory
// address: hold one in reset, re-address the other, then release the
// first onto the address just vacated.
//
// Both sides are brought up together rather than independently,
// because with a single XSHUT they are no longer separable: recovering
// the left one means putting the right one back into reset. A healthy
// sensor therefore gets briefly re-initialized when its sibling is
// retried -- a few tens of milliseconds every
// ROVER_SENSOR_RETRY_PERIOD_MS, and only while one of them is actually
// failing. Accepted deliberately: the alternative is leaving a
// recoverable sensor permanently dead.
void DistanceSensor::bringUpBoth() {
    // Counts every pass. A healthy robot runs this ONCE at boot: any
    // growth over time means the retry path is re-firing, which with a
    // shared sequence can self-sustain (re-initializing both sides
    // costs the healthy one its fresh sample, which can make IT go
    // stale, which triggers the next pass). Without this counter that
    // loop is invisible -- it only shows up as telemetry flickering
    // between a real reading and 8190.
    _bringUpCount++;

    // 1. Right sensor into reset, so the left one is alone on the bus
    //    as far as the factory address is concerned.
    digitalWrite(ROVER_PIN_TOF_RIGHT_XSHUT, LOW);
    delay(10);

    // Everything below feeds the diagnostic snapshot reported by
    // SYSTEM action=tof_status. This sequence has four independent ways
    // to fail (right sensor not actually held in reset, stale address
    // not cleared, nothing on the factory address, library begin()
    // refusing) and from the outside they are indistinguishable -- all
    // four read as "distance_left=9999". Recording what each step saw
    // is what turns that into a single answer.
    bool sawDefaultBefore = i2cDevicePresent(ROVER_TOF_DEFAULT_ADDRESS);
    bool sawLeftAddrBefore = i2cDevicePresent(ROVER_TOF_LEFT_ADDRESS);
    bool addressReset = false;

    // 2. If the left sensor is still on the address a previous run gave
    //    it, hand it back to the factory default first (see above).
    if (sawLeftAddrBefore) {
        addressReset = resetToFactoryAddress(ROVER_TOF_LEFT_ADDRESS);
    }

    // 3. Left sensor: probe before the library's begin() -- see
    //    I2CProbe.h for why that order is not optional -- then move it
    //    off the factory address so the right one can have it.
    bool sawDefaultAfterReset = i2cDevicePresent(ROVER_TOF_DEFAULT_ADDRESS);
    _leftOk = false;
    if (sawDefaultAfterReset) {
        _leftOk = _left.begin(ROVER_TOF_LEFT_ADDRESS);
        if (_leftOk) _left.startRangeContinuous();
    }

    snprintf(_bringUpReport, sizeof(_bringUpReport),
             "tof_pre29=%d tof_pre30=%d tof_areset=%d tof_post29=%d tof_lbegin=%d",
             sawDefaultBefore ? 1 : 0, sawLeftAddrBefore ? 1 : 0,
             addressReset ? 1 : 0, sawDefaultAfterReset ? 1 : 0,
             _leftOk ? 1 : 0);

    _lastLeftRetryMs = millis();
    _lastLeftSampleMs = millis();  // don't start the staleness clock already expired
    _leftMm = _leftOk ? OUT_OF_RANGE_MM : UNAVAILABLE_MM;

    RoverWatchdog::feed();  // an absent sensor can take a while to time out on I2C, see SensorHub.cpp

    // 4. Release the right sensor onto the now-free factory address.
    digitalWrite(ROVER_PIN_TOF_RIGHT_XSHUT, HIGH);
    delay(10);  // boot time after XSHUT release, per the VL53L0X datasheet
    _rightOk = false;
    if (i2cDevicePresent(ROVER_TOF_DEFAULT_ADDRESS)) {
        _rightOk = _right.begin(ROVER_TOF_DEFAULT_ADDRESS);
        if (_rightOk) _right.startRangeContinuous();
    }
    _lastRightRetryMs = millis();
    _lastRightSampleMs = millis();
    _rightMm = _rightOk ? OUT_OF_RANGE_MM : UNAVAILABLE_MM;
}

void DistanceSensor::update() {
    unsigned long now = millis();

    // A sensor that failed begin() (not wired yet, loose contact) gets
    // occasional retries rather than being given up on for the rest of
    // the boot. Both sides go through the same full sequence, because
    // with a single XSHUT line they can no longer be brought up
    // independently -- see bringUpBoth().
    bool leftDue = !_leftOk && now - _lastLeftRetryMs >= ROVER_SENSOR_RETRY_PERIOD_MS;
    bool rightDue = !_rightOk && now - _lastRightRetryMs >= ROVER_SENSOR_RETRY_PERIOD_MS;
    if (leftDue || rightDue) {
        bringUpBoth();
    }

    if (_leftOk) {
        if (_left.isRangeComplete()) {
            uint16_t mm = _left.readRangeResult();
            _leftMm = (mm >= OUT_OF_RANGE_MM) ? OUT_OF_RANGE_MM : mm;
            _lastLeftSampleMs = now;
        } else if (now - _lastLeftSampleMs >= STALE_SAMPLE_TIMEOUT_MS) {
            // Continuous mode has no per-call failure signal the way
            // ImuSensor/EnvironmentSensor's read calls do -- a silent
            // sensor (eg. unplugged mid-run) is only caught this way.
            _leftOk = false;
            _leftMm = UNAVAILABLE_MM;
            _lastLeftRetryMs = now;
        }
    }
    if (_rightOk) {
        if (_right.isRangeComplete()) {
            uint16_t mm = _right.readRangeResult();
            _rightMm = (mm >= OUT_OF_RANGE_MM) ? OUT_OF_RANGE_MM : mm;
            _lastRightSampleMs = now;
        } else if (now - _lastRightSampleMs >= STALE_SAMPLE_TIMEOUT_MS) {
            _rightOk = false;
            _rightMm = UNAVAILABLE_MM;
            _lastRightRetryMs = now;
        }
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

void DistanceSensor::buildBringUpFields(char* out, size_t outLen) const {
    snprintf(out, outLen, "%s tof_rbegin=%d tof_bringups=%lu",
             _bringUpReport, _rightOk ? 1 : 0, (unsigned long)_bringUpCount);
}

void DistanceSensor::rescan() {
    bringUpBoth();
}
