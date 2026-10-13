#pragma once

#include <Arduino.h>
#include "power_config.h"

// BQ25887 2S USB charger with cell balancing, on the custom PCB only
// (pcb/PCB_CONNEXIONS.md section 5.1). Configures the charge, then
// watches it and the two cells.
//
// What this does NOT do: decide whether the cells are being charged
// safely. The BQ25887 runs the charge by itself (constant current,
// then constant voltage, termination, thermistor window, safety timer)
// and the S-8252 protection IC disconnects the pack on any cell
// over/under-voltage -- both in hardware, with or without this code.
// This class adds a third, firmware-level layer on top: it configures
// a gentle charge current, reports what is happening, stops the robot
// before the protection IC has to, and inhibits charging if a cell ever
// reads above ROVER_CELL_OVERVOLTAGE_MV.
//
// Safety rules followed throughout:
// - Nothing is ever written to a chip whose part number does not read
//   as a BQ25887 (PowerMath::bqIsBq25887).
// - Cell readings drive no decision unless they add up to the pack
//   voltage measured independently by the INA219
//   (PowerMath::cellsPlausible) -- the ADC scale is assumed, not
//   verified against the datasheet (see PowerMath.h).
// - Every I2C transaction is checked; a failure marks the chip lost and
//   it is re-probed later, never assumed configured.
class ChargerMonitor {
public:
    void begin();

    // Non-blocking, call every loop() iteration. packMv is the INA219's
    // pack voltage, packMvValid whether it is a live reading -- without
    // it the cell readings cannot be cross-checked and are not used.
    void update(uint32_t packMv, bool packMvValid);

    // One measurement right now, ignoring the update period -- see
    // BatteryMonitor::sampleNow. No-op while the chip is absent.
    void sampleNow(uint32_t packMv, bool packMvValid);

    bool ok() const { return _ok; }
    bool usbPresent() const { return _usbPresent; }
    bool charging() const;
    // True when the last cell readings passed the plausibility check.
    bool cellsValid() const { return _cellsValid; }
    uint16_t cellTopMv() const { return _cellTopMv; }
    uint16_t cellBottomMv() const { return _cellBottomMv; }

    // One-shot events, each true exactly once per occurrence -- same
    // consume pattern as SensorHub/BatteryMonitor.
    bool consumeFailureEvent();          // chip absent, lost, or not a BQ25887
    bool consumeCellLowEvent();          // a cell dropped under ROVER_CELL_LOW_MV
    bool consumeCellCriticalEvent();     // under ROVER_CELL_CRITICAL_MV, not charging
    bool consumeOvervoltageEvent();      // a cell over ROVER_CELL_OVERVOLTAGE_MV: charge inhibited
    bool consumeCellsImplausibleEvent(); // readings stopped adding up
    // A new non-zero fault byte or a thermistor out of its normal window.
    bool consumeFaultEvent(uint8_t* faultOut, const char** thermistorOut);

    // Periodic STATE: "charge=fast usb=1 cell_top_mv=4012
    // cell_bot_mv=4010 cells_ok=1 ts=normal".
    void buildTelemetryFields(char* out, size_t outLen) const;
    // On demand (SYSTEM action=charger_status): the configuration read
    // back from the chip, plus currents -- for bring-up.
    void buildStatusFields(char* out, size_t outLen);

private:
    bool _ok = false;
    bool _wrongPart = false;
    bool _usbPresent = false;
    uint8_t _status1 = 0;
    uint8_t _ntc = 0;
    uint8_t _fault = 0;
    uint16_t _cellTopMv = 0;
    uint16_t _cellBottomMv = 0;
    uint16_t _vbusMv = 0;
    uint16_t _ichgMa = 0;
    bool _cellsValid = false;
    bool _cellsWereValid = true;
    // Latched until reboot once a cell reads over-voltage: a cell that
    // went there once is not trusted to be charged again unattended.
    bool _chargeInhibited = false;
    bool _lowActive = false;
    bool _criticalActive = false;
    uint32_t _reconfigCount = 0;

    bool _wasOk = true;  // "true" so a chip missing at boot is reported once
    bool _pendingFailure = false;
    bool _pendingLow = false;
    bool _pendingCritical = false;
    bool _pendingOvervoltage = false;
    bool _pendingImplausible = false;
    bool _pendingFault = false;
    uint8_t _lastReportedFault = 0;
    uint8_t _lastReportedNtc = 0;

    unsigned long _lastUpdateMs = 0;
    unsigned long _lastRetryMs = 0;

    void measure(unsigned long now, uint32_t packMv, bool packMvValid);
    bool probeAndConfigure();
    bool applyConfig();
    bool configLooksApplied(bool* matches);
    bool readStatusAndAdc();
    void evaluateCells(uint32_t packMv, bool packMvValid);
    void trackHealth();

    bool writeReg(uint8_t reg, uint8_t value);
    bool readReg(uint8_t reg, uint8_t* value);
    bool readAdc16(uint8_t msbReg, uint16_t* value);
    bool updateBits(uint8_t reg, uint8_t mask, uint8_t bits);
};
