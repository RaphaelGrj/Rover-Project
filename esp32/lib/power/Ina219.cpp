#include "Ina219.h"
#include <Wire.h>
#include "PowerMath.h"
#include "I2CProbe.h"

bool Ina219::begin(uint8_t address) {
    _address = address;
    _ok = false;
    // Probe first, like every other I2C device in this firmware
    // (I2CProbe.h): an absent chip must cost one quick NACK, nothing more.
    if (!i2cDevicePresent(_address)) return false;
    if (!writeReg16(PowerMath::INA219_REG_CONFIG, PowerMath::INA219_CONFIG)) return false;
    uint16_t readBack = 0;
    if (!readReg16(PowerMath::INA219_REG_CONFIG, &readBack)) return false;
    _ok = (readBack == PowerMath::INA219_CONFIG);
    return _ok;
}

bool Ina219::read(uint32_t* busMvOut, int32_t* currentMaOut, uint16_t shuntMilliOhms) {
    if (!_ok) return false;
    uint16_t shuntRaw = 0;
    uint16_t busRaw = 0;
    if (!readReg16(PowerMath::INA219_REG_SHUNT, &shuntRaw) ||
        !readReg16(PowerMath::INA219_REG_BUS, &busRaw)) {
        _ok = false;
        return false;
    }
    *busMvOut = PowerMath::ina219BusMv(busRaw);
    *currentMaOut = PowerMath::ina219CurrentMa((int16_t)shuntRaw, shuntMilliOhms);
    return true;
}

// INA219 registers are 16-bit, most significant byte first.
bool Ina219::writeReg16(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(_address);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xFF));
    return Wire.endTransmission() == 0;
}

bool Ina219::readReg16(uint8_t reg, uint16_t* value) {
    Wire.beginTransmission(_address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(_address, (uint8_t)2) != 2) return false;
    uint8_t msb = (uint8_t)Wire.read();
    uint8_t lsb = (uint8_t)Wire.read();
    *value = (uint16_t)((msb << 8) | lsb);
    return true;
}
