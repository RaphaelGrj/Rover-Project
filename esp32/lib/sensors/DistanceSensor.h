#pragma once

#include <Arduino.h>
#include <Adafruit_VL53L0X.h>

// Owns both front ToF distance sensors (left/right), including the
// XSHUT dance needed because they boot on the same I2C address
// (WIRING.md). Each side tracks its own health independently
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

    void beginLeft();
    void beginRight();
    void evaluateObstacle();
};
