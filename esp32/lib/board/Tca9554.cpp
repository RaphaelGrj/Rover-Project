#include "Tca9554.h"
#include <Wire.h>

bool Tca9554::begin(uint8_t address, uint8_t outputLevels, uint8_t inputMask) {
    _address = address;
    _outputs = outputLevels;
    _inputMask = inputMask;
    // Output register first, then direction -- see the header.
    _ok = writeReg(REG_OUTPUT, _outputs) && writeReg(REG_CONFIG, _inputMask);
    return _ok;
}

bool Tca9554::writePin(uint8_t pin, bool high) {
    if (pin > 7) return false;
    if (high) {
        _outputs |= (uint8_t)(1u << pin);
    } else {
        _outputs &= (uint8_t)~(1u << pin);
    }
    if (!_ok) return false;  // BoardIo's retry will push _outputs later
    if (!writeReg(REG_OUTPUT, _outputs)) {
        _ok = false;
        return false;
    }
    return true;
}

bool Tca9554::readPin(uint8_t pin, bool* high) {
    if (pin > 7 || !_ok) return false;
    uint8_t value = 0;
    if (!readReg(REG_INPUT, &value)) {
        _ok = false;
        return false;
    }
    *high = (value & (1u << pin)) != 0;
    return true;
}

bool Tca9554::writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool Tca9554::readReg(uint8_t reg, uint8_t* value) {
    Wire.beginTransmission(_address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(_address, (uint8_t)1) != 1) return false;
    *value = (uint8_t)Wire.read();
    return true;
}
