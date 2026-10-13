#include "ChargerMonitor.h"
#include <Wire.h>
#include "PowerMath.h"
#include "I2CProbe.h"
#if defined(ROVER_BOARD_PCB_V1)
#include "board_pins_pcb_v1.h"
#endif

using namespace PowerMath;

namespace {
#if defined(ROVER_BOARD_PCB_V1)
constexpr uint8_t CHARGER_ADDRESS = ROVER_CHARGER_ADDRESS;
#else
constexpr uint8_t CHARGER_ADDRESS = 0x6A;  // unused: the charger is PCB-only
#endif
}  // namespace

void ChargerMonitor::begin() {
    if (!ROVER_CHARGER_ENABLED) return;
    probeAndConfigure();
    _lastRetryMs = millis();
    trackHealth();
}

bool ChargerMonitor::charging() const {
    return _ok && bqIsCharging(_status1);
}

bool ChargerMonitor::probeAndConfigure() {
    _ok = false;
    _wrongPart = false;
    if (!i2cDevicePresent(CHARGER_ADDRESS)) return false;

    // Identity check BEFORE any write. 0x6A is also the default address
    // of the LSM6DS family, which the PCB moves to 0x6B -- a mis-strapped
    // IMU (or a sibling charger with another register map) answering
    // here must never receive charger register writes.
    uint8_t partInfo = 0;
    if (!readReg(BQ_REG_PART_INFO, &partInfo)) return false;
    if (!bqIsBq25887(partInfo)) {
        _wrongPart = true;
        return false;
    }

    _ok = applyConfig();
    return _ok;
}

bool ChargerMonitor::applyConfig() {
    // Read-modify-write on every shared register, so bits this firmware
    // has no opinion on keep the chip's own defaults.
    bool ok = writeReg(BQ_REG_VCELLREG, bqVcellregCode(ROVER_CELL_CHARGE_MV));
    ok = ok && updateBits(BQ_REG_ICHG, BQ_ICHG_MASK, bqIchgCode(ROVER_CHARGE_CURRENT_MA));
    ok = ok && updateBits(BQ_REG_IINDPM, BQ_IINDPM_MASK, bqIindpmCode(ROVER_USB_INPUT_LIMIT_MA));
    // Watchdog off. With it on (40 s default), any stall of this loop
    // longer than that would silently put the chip back to its reset
    // values -- 1.5 A instead of the gentler current set above. The
    // periodic configLooksApplied() check below re-applies the config
    // anyway if the chip ever resets for another reason.
    ok = ok && updateBits(BQ_REG_CTRL1, BQ_CTRL1_WATCHDOG_MASK, 0x00);
    // Input current is decided here, not by the chip's plug-in source
    // detection: on USB-C the data lines say nothing reliable about what
    // the charger can give, and the detection would override the limit
    // just written. EN_CHG follows the over-voltage latch.
    uint8_t ctrl2Bits = _chargeInhibited ? 0x00 : BQ_CTRL2_EN_CHG;
    ok = ok && updateBits(BQ_REG_CTRL2, BQ_CTRL2_AUTO_INDET_EN | BQ_CTRL2_EN_CHG, ctrl2Bits);
    ok = ok && writeReg(BQ_REG_ADC_CTRL, BQ_ADC_CTRL_CONTINUOUS);
    if (ok) _reconfigCount++;
    return ok;
}

// The registers this firmware writes, compared against what it wants.
// A mismatch means the chip reset behind our back (pack unplugged and
// replugged, brown-out) and the config must go back in.
bool ChargerMonitor::configLooksApplied(bool* matches) {
    uint8_t ichg = 0, ctrl1 = 0, ctrl2 = 0;
    if (!readReg(BQ_REG_ICHG, &ichg) || !readReg(BQ_REG_CTRL1, &ctrl1) ||
        !readReg(BQ_REG_CTRL2, &ctrl2)) {
        return false;
    }
    bool chargeBitRight = ((ctrl2 & BQ_CTRL2_EN_CHG) != 0) == !_chargeInhibited;
    *matches = (ichg & BQ_ICHG_MASK) == bqIchgCode(ROVER_CHARGE_CURRENT_MA) &&
               (ctrl1 & BQ_CTRL1_WATCHDOG_MASK) == 0 &&
               (ctrl2 & BQ_CTRL2_AUTO_INDET_EN) == 0 &&
               chargeBitRight;
    return true;
}

void ChargerMonitor::update(uint32_t packMv, bool packMvValid) {
    if (!ROVER_CHARGER_ENABLED) return;
    unsigned long now = millis();

    if (!_ok) {
        _cellsValid = false;
        if (now - _lastRetryMs >= ROVER_CHARGER_RETRY_PERIOD_MS) {
            _lastRetryMs = now;
            probeAndConfigure();
        }
        trackHealth();
        return;
    }

    if (now - _lastUpdateMs < ROVER_CHARGER_UPDATE_PERIOD_MS) return;
    _lastUpdateMs = now;

    bool matches = true;
    if (!configLooksApplied(&matches) || (!matches && !applyConfig()) || !readStatusAndAdc()) {
        _ok = false;
        _cellsValid = false;
        _lastRetryMs = now;
        trackHealth();
        return;
    }

    evaluateCells(packMv, packMvValid);

    // Faults are reported on change, not every 2 s while they last.
    bool thermistorAbnormal = (_ntc & 0x07) != 0;
    if ((_fault != 0 && _fault != _lastReportedFault) ||
        (thermistorAbnormal && (_ntc & 0x07) != (_lastReportedNtc & 0x07))) {
        _pendingFault = true;
    }
    _lastReportedFault = _fault;
    _lastReportedNtc = _ntc;
    trackHealth();
}

bool ChargerMonitor::readStatusAndAdc() {
    uint8_t status2 = 0;
    uint16_t vbat = 0;
    bool ok = readReg(BQ_REG_STATUS1, &_status1) && readReg(BQ_REG_STATUS2, &status2) &&
              readReg(BQ_REG_NTC, &_ntc) && readReg(BQ_REG_FAULT, &_fault) &&
              readAdc16(BQ_REG_VCELLTOP_ADC, &_cellTopMv) &&
              readAdc16(BQ_REG_VCELLBOT_ADC, &_cellBottomMv) &&
              readAdc16(BQ_REG_VBAT_ADC, &vbat) && readAdc16(BQ_REG_VBUS_ADC, &_vbusMv) &&
              readAdc16(BQ_REG_ICHG_ADC, &_ichgMa);
    if (!ok) return false;
    _usbPresent = (status2 & BQ_STATUS2_PG) != 0;
    return true;
}

void ChargerMonitor::evaluateCells(uint32_t packMv, bool packMvValid) {
    _cellsValid = packMvValid &&
                  cellsPlausible(_cellTopMv, _cellBottomMv, packMv, ROVER_CELL_SUM_TOLERANCE_MV);
    // Only a valid -> invalid transition is an event; readings that were
    // never valid (no INA219 reading yet) stay quiet.
    if (_cellsWereValid && !_cellsValid && packMvValid) _pendingImplausible = true;
    _cellsWereValid = _cellsValid;
    if (!_cellsValid) return;

    uint16_t lowest = minCellMv(_cellTopMv, _cellBottomMv);
    uint16_t highest = maxCellMv(_cellTopMv, _cellBottomMv);

    if (highest > ROVER_CELL_OVERVOLTAGE_MV && !_chargeInhibited) {
        _chargeInhibited = true;
        _pendingOvervoltage = true;
        // Best effort: if this write fails, the chip is marked lost on
        // the next pass and applyConfig() -- which honours the latch --
        // runs again on reconnection. The protection IC remains the
        // last word either way.
        updateBits(BQ_REG_CTRL2, BQ_CTRL2_EN_CHG, 0x00);
    }

    // Low: warning, with hysteresis so it does not chatter.
    if (lowest < ROVER_CELL_LOW_MV && !_lowActive) {
        _lowActive = true;
        _pendingLow = true;
    } else if (lowest > ROVER_CELL_LOW_CLEAR_MV) {
        _lowActive = false;
    }

    // Critical: only while NOT charging -- a deeply discharged cell
    // being pre-charged reads low by definition, and stopping the robot
    // for it would be noise. Re-armed once the cell is back above LOW.
    if (lowest < ROVER_CELL_CRITICAL_MV && !charging() && !_criticalActive) {
        _criticalActive = true;
        _pendingCritical = true;
    } else if (lowest > ROVER_CELL_LOW_MV) {
        _criticalActive = false;
    }
}

void ChargerMonitor::trackHealth() {
    if (_wasOk && !_ok) _pendingFailure = true;
    _wasOk = _ok;
}

bool ChargerMonitor::consumeFailureEvent() {
    if (!_pendingFailure) return false;
    _pendingFailure = false;
    return true;
}

bool ChargerMonitor::consumeCellLowEvent() {
    if (!_pendingLow) return false;
    _pendingLow = false;
    return true;
}

bool ChargerMonitor::consumeCellCriticalEvent() {
    if (!_pendingCritical) return false;
    _pendingCritical = false;
    return true;
}

bool ChargerMonitor::consumeOvervoltageEvent() {
    if (!_pendingOvervoltage) return false;
    _pendingOvervoltage = false;
    return true;
}

bool ChargerMonitor::consumeCellsImplausibleEvent() {
    if (!_pendingImplausible) return false;
    _pendingImplausible = false;
    return true;
}

bool ChargerMonitor::consumeFaultEvent(uint8_t* faultOut, const char** thermistorOut) {
    if (!_pendingFault) return false;
    _pendingFault = false;
    *faultOut = _fault;
    *thermistorOut = bqThermistorName(_ntc);
    return true;
}

void ChargerMonitor::buildTelemetryFields(char* out, size_t outLen) const {
    snprintf(out, outLen, "charge=%s usb=%d cell_top_mv=%u cell_bot_mv=%u cells_ok=%d ts=%s",
             _ok ? bqChargeStateName(_status1) : "unavailable", _usbPresent ? 1 : 0,
             _cellTopMv, _cellBottomMv, _cellsValid ? 1 : 0, bqThermistorName(_ntc));
}

void ChargerMonitor::buildStatusFields(char* out, size_t outLen) {
    if (!_ok) {
        snprintf(out, outLen, "chg_ok=0 chg_wrong_part=%d", _wrongPart ? 1 : 0);
        return;
    }
    uint8_t vreg = 0, ichg = 0, iindpm = 0;
    if (!readReg(BQ_REG_VCELLREG, &vreg) || !readReg(BQ_REG_ICHG, &ichg) ||
        !readReg(BQ_REG_IINDPM, &iindpm)) {
        snprintf(out, outLen, "chg_ok=0 chg_read_failed=1");
        return;
    }
    // Kept to 8 short fields: at full width this is ~115 bytes, just
    // under ROVER_MAX_FRAME_LEN with "STATE " in front -- the margin this
    // project has learned to count rather than guess (see main.cpp's
    // buffer comments). chg_cfg counts how many times the config was
    // (re)applied: anything above 1 means the chip reset behind our back.
    snprintf(out, outLen,
             "vreg_mv=%u ichg_ma=%u iindpm_ma=%u inhibit=%d fault=0x%02X chg_ma=%u vbus_mv=%u chg_cfg=%lu",
             bqVcellregMv(vreg), bqIchgMa(ichg), bqIindpmMa(iindpm), _chargeInhibited ? 1 : 0,
             _fault, _ichgMa, _vbusMv, (unsigned long)_reconfigCount);
}

bool ChargerMonitor::writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(CHARGER_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool ChargerMonitor::readReg(uint8_t reg, uint8_t* value) {
    Wire.beginTransmission(CHARGER_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(CHARGER_ADDRESS, (uint8_t)1) != 1) return false;
    *value = (uint8_t)Wire.read();
    return true;
}

// Both bytes in ONE transaction, MSB register first, so a continuous
// conversion cannot complete between them and pair the high byte of one
// sample with the low byte of the next. Relies on the register pointer
// auto-incrementing on multi-byte reads, as on TI's sibling chargers --
// if it did not, the value would come out wrong, fail
// PowerMath::cellsPlausible, and be reported as cells_ok=0 rather than
// acted upon.
bool ChargerMonitor::readAdc16(uint8_t msbReg, uint16_t* value) {
    Wire.beginTransmission(CHARGER_ADDRESS);
    Wire.write(msbReg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(CHARGER_ADDRESS, (uint8_t)2) != 2) return false;
    uint8_t msb = (uint8_t)Wire.read();
    uint8_t lsb = (uint8_t)Wire.read();
    *value = bqAdc16(msb, lsb);
    return true;
}

bool ChargerMonitor::updateBits(uint8_t reg, uint8_t mask, uint8_t bits) {
    uint8_t current = 0;
    if (!readReg(reg, &current)) return false;
    uint8_t next = (uint8_t)((current & ~mask) | (bits & mask));
    if (next == current) return true;
    return writeReg(reg, next);
}
