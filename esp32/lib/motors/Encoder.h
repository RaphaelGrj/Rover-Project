#pragma once

#include <Arduino.h>

// Quadrature encoder tick counter using a GPIO interrupt. Only channel A
// triggers the ISR; channel B is sampled at that instant to work out
// direction (x1 decode -- simpler and cheaper than x4, revisit if the
// real N20 encoders need finer resolution once measured on hardware).
class Encoder {
public:
    void begin(uint8_t pinA, uint8_t pinB);

    // Ticks accumulated since the last call; resets the running counter.
    // Meant to be called at a fixed period to derive wheel speed.
    long readAndResetTicks();

private:
    static void IRAM_ATTR onPinAChange(void* arg);

    uint8_t _pinA = 0;
    uint8_t _pinB = 0;
    volatile long _ticks = 0;
};
