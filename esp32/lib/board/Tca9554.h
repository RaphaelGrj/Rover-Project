#pragma once

#include <Arduino.h>

// Minimal driver for a TCA9554 / PCA9554 8-bit I2C I/O expander.
//
// Raw Wire rather than a library, in the same spirit as I2CProbe.h:
// four registers, and every transaction's result is checked -- a failed
// write must be reported as failed, not assumed to have happened
// (CLAUDE.md: communication errors are always handled).
class Tca9554 {
public:
    // Probes the chip, then writes the OUTPUT register BEFORE the
    // direction register: pins come out of reset as inputs, and this
    // order guarantees each one turns into an output already at its
    // safe level -- never glitching through the opposite level first.
    // inputMask: 1 = input. Returns false if the chip did not answer.
    bool begin(uint8_t address, uint8_t outputLevels, uint8_t inputMask);

    // Sets one output pin. The cached output byte is updated even on
    // failure, so a later begin() (retry) restores the level the
    // firmware last asked for, not the boot default.
    bool writePin(uint8_t pin, bool high);

    // Reads one pin's input level. false = I2C failure, *high untouched.
    bool readPin(uint8_t pin, bool* high);

    bool ok() const { return _ok; }
    uint8_t outputLevels() const { return _outputs; }

private:
    static constexpr uint8_t REG_INPUT = 0x00;
    static constexpr uint8_t REG_OUTPUT = 0x01;
    static constexpr uint8_t REG_CONFIG = 0x03;

    bool writeReg(uint8_t reg, uint8_t value);
    bool readReg(uint8_t reg, uint8_t* value);

    uint8_t _address = 0x20;
    uint8_t _outputs = 0x00;
    uint8_t _inputMask = 0xFF;
    bool _ok = false;
};
