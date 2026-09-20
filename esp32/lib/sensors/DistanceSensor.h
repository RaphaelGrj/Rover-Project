#pragma once

#include <Arduino.h>
#include <Adafruit_VL53L0X.h>

// Owns both front ToF distance sensors (left/right), including the
// XSHUT dance needed because they boot on the same I2C address
// (WIRING.md). That dance uses ONE XSHUT line, on the right sensor:
// the left one's is tied high in hardware, because the 30-pin WROOM
// devkit does not break GPIO0 out at all (found on the real board,
// 2026-09-20). Holding one sensor in reset while the other is
// re-addressed is all the separation two identical devices need.
//
// Each side tracks its own health independently
// (ARCHITECTURE_AND_ROADMAP.md section 21: one failed sensor must never
// take the other down) -- a missing/failed unit just keeps reporting an
// "unavailable" reading and gets retried periodically, instead of
// blocking begin()/update() for its sibling.
class DistanceSensor {
public:
    void begin();

    // Non-blocking, call every loop() iteration. Both sensors run in
    // continuous-ranging mode so this only ever polls for an already-
    // ready sample -- it never blocks waiting on a conversion (that
    // would stall the shared loop() that also drives PID/protocol/display).
    void update();

    bool leftOk() const { return _leftOk; }
    bool rightOk() const { return _rightOk; }

    // True once either side has crossed into "obstacle" range; see
    // sensors_config.h for the hysteresis thresholds.
    bool obstacleDetected() const { return _obstacleActive; }

    // Fills "distance_left=... distance_right=..." (millimeters) for a
    // STATE frame -- see ROVER_PROTOCOL.md section 7.1.
    void buildTelemetryFields(char* out, size_t outLen) const;

    // Bring-up diagnostics: what the last bringUpBoth() pass actually
    // observed at each step. "distance_left=9999" alone cannot tell an
    // absent sensor from one that is present but stuck on a stale
    // address, nor from one whose sibling was never really held in
    // reset -- see bringUpBoth().
    void buildBringUpFields(char* out, size_t outLen) const;

    // Forces a full re-run of the bring-up sequence, for bring-up
    // tooling that must not wait out ROVER_SENSOR_RETRY_PERIOD_MS.
    void rescan();

private:
    Adafruit_VL53L0X _left;
    Adafruit_VL53L0X _right;
    bool _leftOk = false;
    bool _rightOk = false;
    uint16_t _leftMm = 0;
    uint16_t _rightMm = 0;
    bool _obstacleActive = false;
    unsigned long _lastLeftRetryMs = 0;
    unsigned long _lastRightRetryMs = 0;
    // Timestamp of the last accepted sample for each side -- unlike
    // ImuSensor/EnvironmentSensor (whose read calls report failure
    // directly), the continuous-ranging read path here has no per-call
    // failure signal, so a sensor that goes silent after a successful
    // begin() (eg. unplugged mid-run) would otherwise never be detected.
    // update() watches these and drops leftOk()/rightOk() if a side goes
    // quiet for too long.
    unsigned long _lastLeftSampleMs = 0;
    unsigned long _lastRightSampleMs = 0;
    char _bringUpReport[80] = "tof_never_run=1";
    // Should stay at 1 for the life of a boot -- see bringUpBoth().
    uint32_t _bringUpCount = 0;

    // Both sides at once: a single XSHUT line makes them inseparable,
    // see the definition for the full reasoning.
    void bringUpBoth();
    void evaluateObstacle();
};
