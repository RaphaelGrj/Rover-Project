#pragma once

#include <Arduino.h>

// Minimal INA219 driver: pack voltage and current through the custom
// PCB's 0.05 ohm shunt (pcb/PCB_CONNEXIONS.md section 5.1).
//
// Raw Wire instead of Adafruit_INA219 for two reasons: that library's
// read calls do not report a failed I2C transaction (a vanished chip
// keeps "reading" a value), and its calibration presets assume a
// 0.1 ohm shunt. Here every transaction is checked, and the current is
// computed from the shunt register with the real resistor value
// (PowerMath::ina219CurrentMa).
class Ina219 {
public:
    // Probes the chip and writes the configuration, then reads it back:
    // a chip that ACKs but does not hold the value is not trusted.
    bool begin(uint8_t address);

    // One measurement. false = I2C failure (and ok() drops to false);
    // outputs are only written on success.
    bool read(uint32_t* busMvOut, int32_t* currentMaOut, uint16_t shuntMilliOhms);

    bool ok() const { return _ok; }

private:
    bool writeReg16(uint8_t reg, uint16_t value);
    bool readReg16(uint8_t reg, uint16_t* value);

    uint8_t _address = 0x40;
    bool _ok = false;
};
