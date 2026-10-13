#pragma once

#include <stdint.h>

// Pure conversion and decision helpers for the custom PCB's power path:
// the INA219 pack monitor and the BQ25887 2S charger
// (pcb/PCB_CONNEXIONS.md section 5.1).
//
// Kept free of Arduino/Wire on purpose, like WheelPID and RoverProtocol:
// every number below is a register encoding read off a datasheet, and
// a wrong one is exactly the kind of mistake that only shows up on a
// charging battery. Isolated here, each encoding is pinned by a native
// unit test (test/test_power_math) instead of being trusted.
namespace PowerMath {

// --- INA219 (pack voltage + current through the 0.05 ohm shunt) ---------

// Config register value: 32V bus range, PGA /8 (+/-320 mV shunt range,
// i.e. +/-6.4 A on 50 mohm -- well above the ~2.8 A peak), 12-bit with
// 128-sample averaging on both channels (68 ms per conversion, plenty
// for a 2 s battery update), continuous shunt + bus mode.
constexpr uint16_t INA219_CONFIG = 0x3FFF;
constexpr uint8_t INA219_REG_CONFIG = 0x00;
constexpr uint8_t INA219_REG_SHUNT = 0x01;
constexpr uint8_t INA219_REG_BUS = 0x02;

// Shunt register: signed, 10 uV per LSB whatever the PGA setting.
// Current is computed from it directly (uV / mohm = mA) rather than
// through the chip's calibration/current registers, so the shunt value
// lives in one place (power_config.h) and no calibration constant can
// silently disagree with it.
inline int32_t ina219CurrentMa(int16_t shuntRaw, uint16_t shuntMilliOhms) {
    if (shuntMilliOhms == 0) return 0;
    return ((int32_t)shuntRaw * 10) / (int32_t)shuntMilliOhms;
}

// Bus register: bits 15..3 hold the voltage, 4 mV per LSB. Bits 1..0
// are flags about the chip's own current/power math (CNVR, OVF), which
// this firmware does not use -- the calibration register is left at 0
// and current comes from the shunt register instead -- so they are
// simply masked off.
inline uint32_t ina219BusMv(uint16_t busRaw) {
    return (uint32_t)(busRaw >> 3) * 4u;
}

// --- BQ25887 (2S USB boost charger with cell balancing) ----------------
//
// Register map taken from the bq25887 Rust crate's manifest
// (src/bq25887.yaml, v0.1.11), itself transcribed from TI's datasheet
// SLUSD89B -- TI's own site was unreachable from the session that wrote
// this. The unit tests check every encoding against the documented
// RESET values, which is the cross-check available without the PDF.

constexpr uint8_t BQ_REG_VCELLREG = 0x00;       // cell charge voltage limit
constexpr uint8_t BQ_REG_ICHG = 0x01;           // EN_HIZ | EN_ILIM | ICHG[5:0]
constexpr uint8_t BQ_REG_IINDPM = 0x03;         // FORCE_ICO | FORCE_INDET | EN_ICO | IINDPM[4:0]
constexpr uint8_t BQ_REG_CTRL1 = 0x05;          // EN_TERM | STAT_DIS | WATCHDOG[5:4] | ...
constexpr uint8_t BQ_REG_CTRL2 = 0x06;          // AUTO_INDET_EN[6] | TREG | EN_CHG[3] | ...
constexpr uint8_t BQ_REG_STATUS1 = 0x0B;        // ... | WD_STAT[3] | CHRG_STAT[2:0]
constexpr uint8_t BQ_REG_STATUS2 = 0x0C;        // PG_STAT[7] | VBUS_STAT[6:4] | ICO_STAT
constexpr uint8_t BQ_REG_NTC = 0x0D;            // TS_STAT[2:0]
constexpr uint8_t BQ_REG_FAULT = 0x0E;          // VBUS_OVP[7] | TSHUT[6] | TMR[4]
constexpr uint8_t BQ_REG_ADC_CTRL = 0x15;       // ADC_EN[7] | ADC_RATE[6] | ADC_SAMPLE[5:4]
constexpr uint8_t BQ_REG_ICHG_ADC = 0x19;       // 16-bit, MSB first
constexpr uint8_t BQ_REG_VBUS_ADC = 0x1B;
constexpr uint8_t BQ_REG_VBAT_ADC = 0x1D;
constexpr uint8_t BQ_REG_VCELLTOP_ADC = 0x1F;
constexpr uint8_t BQ_REG_PART_INFO = 0x25;      // REG_RST | PN[6:3] | DEV_REV[2:0]
constexpr uint8_t BQ_REG_VCELLBOT_ADC = 0x26;

constexpr uint8_t BQ_ICHG_MASK = 0x3F;
constexpr uint8_t BQ_IINDPM_MASK = 0x1F;
constexpr uint8_t BQ_CTRL1_WATCHDOG_MASK = 0x30;
constexpr uint8_t BQ_CTRL2_AUTO_INDET_EN = 0x40;
constexpr uint8_t BQ_CTRL2_EN_CHG = 0x08;
constexpr uint8_t BQ_STATUS2_PG = 0x80;
// ADC on, continuous conversion, 15-bit resolution (24 ms). Continuous
// rather than one-shot so a value is always fresh when update() reads.
constexpr uint8_t BQ_ADC_CTRL_CONTINUOUS = 0xB0;

// ICHG[5:0]: 50 mA per step with a 50 mA offset, 100..2200 mA
// (reset 0x1D = 1500 mA).
inline uint8_t bqIchgCode(uint16_t mA) {
    if (mA < 100) mA = 100;
    if (mA > 2200) mA = 2200;
    return (uint8_t)(mA / 50 - 1);
}
inline uint16_t bqIchgMa(uint8_t code) {
    return (uint16_t)(((code & BQ_ICHG_MASK) + 1) * 50);
}

// IINDPM[4:0]: 100 mA per step from 500 mA, up to 3300 mA
// (reset 0x19 = 3000 mA).
inline uint8_t bqIindpmCode(uint16_t mA) {
    if (mA < 500) mA = 500;
    if (mA > 3300) mA = 3300;
    return (uint8_t)((mA - 500) / 100);
}
inline uint16_t bqIindpmMa(uint8_t code) {
    return (uint16_t)(500 + (code & BQ_IINDPM_MASK) * 100);
}

// VCELLREG: 5 mV per step from 3400 mV, up to 4600 mV
// (reset 0xA0 = 4200 mV).
inline uint8_t bqVcellregCode(uint16_t mV) {
    if (mV < 3400) mV = 3400;
    if (mV > 4600) mV = 4600;
    return (uint8_t)((mV - 3400) / 5);
}
inline uint16_t bqVcellregMv(uint8_t code) {
    return (uint16_t)(3400 + code * 5);
}

// PART_INFORMATION PN[6:3] reads 0b0101 on a BQ25887 (reset value 0x28).
// Checked at boot so that a different chip answering at 0x6A -- a
// sibling charger with another register layout, or simply the IMU if
// someone strapped it to the wrong address -- is never written to.
inline bool bqIsBq25887(uint8_t partInfo) {
    return ((partInfo >> 3) & 0x0F) == 0x05;
}

inline uint16_t bqAdc16(uint8_t msb, uint8_t lsb) {
    return (uint16_t)(((uint16_t)msb << 8) | lsb);
}

// CHRG_STAT[2:0] of STATUS1, as the short token sent in telemetry.
inline const char* bqChargeStateName(uint8_t status1) {
    switch (status1 & 0x07) {
        case 0: return "none";
        case 1: return "trickle";
        case 2: return "precharge";
        case 3: return "fast";
        case 4: return "taper";
        case 5: return "topoff";
        case 6: return "done";
        default: return "unknown";
    }
}

inline bool bqIsCharging(uint8_t status1) {
    uint8_t s = status1 & 0x07;
    return s >= 1 && s <= 5;
}

// TS_STAT[2:0] of the NTC register.
inline const char* bqThermistorName(uint8_t ntc) {
    switch (ntc & 0x07) {
        case 0: return "normal";
        case 2: return "warm";
        case 3: return "cool";
        case 5: return "cold";
        case 6: return "hot";
        default: return "unknown";
    }
}

// --- Cell readings ------------------------------------------------------

// The ADC scale (1 mV per LSB for VCELLTOP/VCELLBOT/VBAT) is assumed
// from the sibling chargers and could NOT be confirmed against the
// datasheet when this was written. Rather than trust it, every reading
// must agree with itself before any decision is taken on it: the two
// cells must add up to the pack voltage (measured independently by the
// INA219), and each must sit in a physically possible Li-ion range. If
// the assumption is wrong -- or a balance wire is broken -- this fails
// and the cell-level protections stand down, loudly, instead of acting
// on nonsense.
constexpr uint16_t CELL_MIN_PLAUSIBLE_MV = 2000;
constexpr uint16_t CELL_MAX_PLAUSIBLE_MV = 4500;

inline bool cellsPlausible(uint16_t topMv, uint16_t bottomMv, uint32_t packMv, uint16_t toleranceMv) {
    if (topMv < CELL_MIN_PLAUSIBLE_MV || topMv > CELL_MAX_PLAUSIBLE_MV) return false;
    if (bottomMv < CELL_MIN_PLAUSIBLE_MV || bottomMv > CELL_MAX_PLAUSIBLE_MV) return false;
    uint32_t sum = (uint32_t)topMv + bottomMv;
    uint32_t diff = sum > packMv ? sum - packMv : packMv - sum;
    return diff <= toleranceMv;
}

inline uint16_t minCellMv(uint16_t a, uint16_t b) {
    return a < b ? a : b;
}

inline uint16_t maxCellMv(uint16_t a, uint16_t b) {
    return a > b ? a : b;
}

// Linear pack-voltage -> percent estimate, clamped to 0..100. Crude for
// Li-ion (the curve is flat in the middle and sags under motor load),
// but the same estimate the project has always reported; cell-level
// thresholds, not this number, are what actually protect the cells.
inline uint8_t batteryPercentFromMv(uint32_t packMv, uint32_t emptyMv, uint32_t fullMv) {
    if (fullMv <= emptyMv) return 0;
    if (packMv <= emptyMv) return 0;
    if (packMv >= fullMv) return 100;
    return (uint8_t)(((packMv - emptyMv) * 100u) / (fullMv - emptyMv));
}

}  // namespace PowerMath
