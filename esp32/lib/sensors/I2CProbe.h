#pragma once

#include <Wire.h>

// Cheap existence check: a zero-length I2C transaction, ACK or not.
// Every sensor here must call this before its own library's begin() --
// first real-hardware test of this module found that the VL53L0X core
// driver (ST's official API, vendored by Adafruit_VL53L0X) can hang
// indefinitely in an internal wait-for-boot loop instead of returning
// false when nothing answers on the bus, which took the whole board
// down (never reached loop(), tripped the hardware watchdog). Probing
// first means a missing sensor is skipped entirely rather than trusting
// its library to fail cleanly.
inline bool i2cDevicePresent(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}
