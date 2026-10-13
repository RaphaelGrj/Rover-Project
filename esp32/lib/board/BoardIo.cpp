#include "BoardIo.h"
#include <Wire.h>
#include "sensors_config.h"
#include "Tca9554.h"

namespace {

bool i2cStarted = false;

#if defined(ROVER_BOARD_PCB_V1)
Tca9554 expander;
bool expanderWasOk = true;  // "true" so a missing chip at boot is reported once
bool expanderFailurePending = false;
unsigned long lastExpanderRetryMs = 0;

// Maps a logical line to its expander pin; -1 if it has none.
int expanderPinFor(BoardLine line) {
    switch (line) {
        case BoardLine::TofRightXshut: return ROVER_EXP_TOF_RIGHT_XSHUT;
        case BoardLine::AmpEnable: return ROVER_EXP_AMP_ENABLE;
        case BoardLine::CameraPowerDown: return ROVER_EXP_CAM_PWDN;
        case BoardLine::DisplayReset: return ROVER_EXP_DISPLAY_RST;
        case BoardLine::DisplayBacklight: return ROVER_EXP_DISPLAY_BLK;
        case BoardLine::StatusLed: return ROVER_EXP_STATUS_LED;
    }
    return -1;
}

void trackExpanderHealth() {
    if (expanderWasOk && !expander.ok()) expanderFailurePending = true;
    expanderWasOk = expander.ok();
}
#endif

}  // namespace

namespace BoardIo {

void beginI2C() {
    if (i2cStarted) return;
    i2cStarted = true;
    // Explicit pins, not board defaults -- keeps this portable across
    // WROOM/S3/PCB (their default I2C pins differ).
    Wire.begin(ROVER_PIN_I2C_SDA, ROVER_PIN_I2C_SCL);
    // Fail fast on a missing device instead of the core's default I2C
    // timeout. First real-hardware test of the sensors (none wired yet)
    // hit exactly this: an absent device on an otherwise-idle bus reads
    // back ESP_ERR_TIMEOUT rather than a quick NACK, and with several
    // devices probed at boot that stacked up past the hardware
    // watchdog's 3s window (Watchdog.h) -- a boot-loop caused by a
    // *missing* sensor, exactly what ARCHITECTURE_AND_ROADMAP.md section
    // 21 says must never happen. 50ms is generous for any device present.
    Wire.setTimeOut(50);
}

void begin() {
#if defined(ROVER_BOARD_PCB_V1)
    beginI2C();
    expander.begin(ROVER_IO_EXPANDER_ADDRESS, ROVER_EXP_SAFE_OUTPUTS, ROVER_EXP_INPUT_MASK);
    lastExpanderRetryMs = millis();
    trackExpanderHealth();
#else
    // WROOM: the only line that exists is the right ToF's XSHUT, a GPIO.
    // HIGH before OUTPUT, so the sensor is never glitched into reset.
    if (ROVER_PIN_TOF_RIGHT_XSHUT >= 0) {
        digitalWrite(ROVER_PIN_TOF_RIGHT_XSHUT, HIGH);
        pinMode(ROVER_PIN_TOF_RIGHT_XSHUT, OUTPUT);
    }
#endif
}

void update() {
#if defined(ROVER_BOARD_PCB_V1)
    if (!expander.ok()) {
        unsigned long now = millis();
        if (now - lastExpanderRetryMs >= ROVER_SENSOR_RETRY_PERIOD_MS) {
            lastExpanderRetryMs = now;
            // Re-sends the levels last asked for, not the boot defaults:
            // a display that was on stays on once the chip is back.
            expander.begin(ROVER_IO_EXPANDER_ADDRESS, expander.outputLevels(), ROVER_EXP_INPUT_MASK);
        }
    }
    trackExpanderHealth();
#endif
}

bool write(BoardLine line, bool high) {
#if defined(ROVER_BOARD_PCB_V1)
    int pin = expanderPinFor(line);
    if (pin < 0) return true;
    bool ok = expander.writePin((uint8_t)pin, high);
    trackExpanderHealth();
    return ok;
#else
    if (line == BoardLine::TofRightXshut && ROVER_PIN_TOF_RIGHT_XSHUT >= 0) {
        digitalWrite(ROVER_PIN_TOF_RIGHT_XSHUT, high ? HIGH : LOW);
    }
    // Every other line is absent on the WROOM devkit (display RST/BLK
    // are wired to GPIO / 3V3 directly, no amp enable, no camera).
    return true;
#endif
}

bool hasExpander() {
#if defined(ROVER_BOARD_PCB_V1)
    return true;
#else
    return false;
#endif
}

bool expanderOk() {
#if defined(ROVER_BOARD_PCB_V1)
    return expander.ok();
#else
    return false;
#endif
}

bool readMotorFault(bool* faultOut) {
#if defined(ROVER_BOARD_PCB_V1)
    bool level = true;
    bool ok = expander.readPin(ROVER_EXP_DRV_FAULT, &level);
    trackExpanderHealth();
    if (!ok) return false;
    *faultOut = !level;  // nFAULT is active low
    return true;
#else
    (void)faultOut;
    return false;
#endif
}

bool consumeExpanderFailure() {
#if defined(ROVER_BOARD_PCB_V1)
    if (!expanderFailurePending) return false;
    expanderFailurePending = false;
    return true;
#else
    return false;
#endif
}

}  // namespace BoardIo
