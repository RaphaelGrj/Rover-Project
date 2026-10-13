#include "BatteryMonitor.h"
#include "PowerMath.h"
#include "board_config.h"
#if defined(ROVER_BOARD_PCB_V1)
#include "board_pins_pcb_v1.h"
#endif

void BatteryMonitor::begin() {
    if (!ROVER_BATTERY_MONITORING_ENABLED) return;
    if (ROVER_BATTERY_USES_INA219) {
#if defined(ROVER_BOARD_PCB_V1)
        _ina.begin(ROVER_INA219_ADDRESS);
#endif
        _lastRetryMs = millis();
        trackInaHealth();
    } else {
        pinMode(ROVER_PIN_BATTERY_ADC, INPUT);
    }
}

void BatteryMonitor::update() {
    if (!ROVER_BATTERY_MONITORING_ENABLED) return;
    unsigned long now = millis();

    if (ROVER_BATTERY_USES_INA219) {
#if defined(ROVER_BOARD_PCB_V1)
        if (!_ina.ok()) {
            // A stale value must not keep being reported as current:
            // the robot would believe a charged battery while it drains.
            _hasReading = false;
            if (now - _lastRetryMs >= ROVER_BATTERY_RETRY_PERIOD_MS) {
                _lastRetryMs = now;
                _ina.begin(ROVER_INA219_ADDRESS);
            }
            trackInaHealth();
            return;
        }
#endif
    }

    if (now - _lastUpdateMs < ROVER_BATTERY_UPDATE_PERIOD_MS) return;
    _lastUpdateMs = now;
    measure(now);
}

void BatteryMonitor::sampleNow() {
    if (!ROVER_BATTERY_MONITORING_ENABLED) return;
    if (ROVER_BATTERY_USES_INA219 && !_ina.ok()) return;
    unsigned long now = millis();
    _lastUpdateMs = now;
    measure(now);
}

void BatteryMonitor::measure(unsigned long now) {
    if (ROVER_BATTERY_USES_INA219) {
        uint32_t mv = 0;
        int32_t ma = 0;
        if (!_ina.read(&mv, &ma, ROVER_INA219_SHUNT_MILLIOHMS)) {
            _hasReading = false;
            _lastRetryMs = now;
            trackInaHealth();
            return;
        }
        _currentMa = ma;
        applyPercent(mv);
    } else {
        // 12-bit ADC (0-4095) over the ESP32's ~3.3V reference.
        float vAdc = (analogRead(ROVER_PIN_BATTERY_ADC) / 4095.0f) * 3.3f;
        float vBatt = vAdc * ROVER_BATTERY_DIVIDER_RATIO;
        applyPercent((uint32_t)(vBatt * 1000.0f));
    }
}

void BatteryMonitor::applyPercent(uint32_t packMv) {
    _packMv = packMv;
    _percent = PowerMath::batteryPercentFromMv(packMv, ROVER_BATTERY_EMPTY_MV, ROVER_BATTERY_FULL_MV);
    _hasReading = true;

    bool low = _percent < ROVER_BATTERY_LOW_PERCENT;
    bool clear = _percent > ROVER_BATTERY_LOW_CLEAR_PERCENT;
    if (low && !_lowActive) _pendingLowEvent = true;
    if (low) _lowActive = true;
    else if (clear) _lowActive = false;
}

void BatteryMonitor::trackInaHealth() {
    if (_inaWasOk && !_ina.ok()) _pendingFailureEvent = true;
    _inaWasOk = _ina.ok();
}

bool BatteryMonitor::consumeLowBatteryEvent() {
    if (!_pendingLowEvent) return false;
    _pendingLowEvent = false;
    return true;
}

bool BatteryMonitor::consumeFailureEvent() {
    if (!_pendingFailureEvent) return false;
    _pendingFailureEvent = false;
    return true;
}

void BatteryMonitor::buildTelemetryFields(char* out, size_t outLen) const {
    if (ROVER_BATTERY_USES_INA219) {
        snprintf(out, outLen, "battery=%u battery_mv=%lu battery_ma=%ld",
                 _percent, (unsigned long)_packMv, (long)_currentMa);
    } else {
        snprintf(out, outLen, "battery=%u", _percent);
    }
}
