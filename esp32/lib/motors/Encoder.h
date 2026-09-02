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

    // Separate cumulative counter, untouched by readAndResetTicks() (the
    // PID loop calls that every 20ms, so it can never hold still long
    // enough to hand-count a wheel turn against it) -- for one-off wheel
    // geometry calibration (ticks/revolution) via SYSTEM action=raw_ticks.
    // See motion_config.h ROVER_ENCODER_TICKS_PER_REV.
    long totalTicks();
    void resetTotal();

private:
    static void IRAM_ATTR onPinAChange(void* arg);

    uint8_t _pinA = 0;
    uint8_t _pinB = 0;
    volatile long _ticks = 0;
    volatile long _totalTicks = 0;
    // ESP32-specific spinlock, not noInterrupts()/interrupts(): those only
    // suspend the current core, which isn't enough if the GPIO ISR ever
    // ends up scheduled on the other core than the one calling
    // readAndResetTicks(). portMUX is the correct primitive for data
    // shared between an ISR and task code on ESP32.
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
