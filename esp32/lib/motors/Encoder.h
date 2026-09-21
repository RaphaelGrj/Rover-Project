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

    // Raw ISR invocations since boot -- NEVER decremented, unlike the
    // tick counters, which add or subtract depending on the decoded
    // direction. That difference is the whole point: a floating input
    // (these pins have no internal pull-up, see begin()) produces
    // edges by the thousand whose directions cancel, so ticks stay
    // near zero while the ISR eats the CPU that loop() needs to drive
    // the motors. Ticks alone cannot tell that apart from a wheel that
    // simply is not turning; edges can.
    unsigned long totalEdges();

    // Call once per control cycle. Returns true on the single cycle
    // where an implausible edge rate makes this encoder disable its own
    // interrupt.
    //
    // A floating input (no internal pull-up on GPIO34-39, see begin())
    // fires this ISR continuously and starves the loop() that drives
    // the motors -- the wheels stop responding because the CPU never
    // gets back to them, which reads from outside as a dead motor.
    // Detaching trades the speed reading for a robot that still runs:
    // the PID then sees zero speed and saturates, which at least keeps
    // driving, where a starved loop does nothing at all.
    //
    // Not self-healing: it stays detached until resetTotal() re-arms it
    // (SYSTEM action=reset_ticks), because the cause is a wire and
    // silently retrying would just hide it again.
    bool pollStorm(unsigned long nowMs);

    bool stormed() const { return _stormed; }

private:
    // An N20 with this encoder produces ~320 edges/s at the speed this
    // chassis actually reaches, and ~3200/s at the old aspirational top
    // speed. 20000/s is far beyond anything a real wheel can generate,
    // so crossing it means the signal, not the motion.
    static constexpr unsigned long MAX_PLAUSIBLE_EDGES_PER_S = 20000;
    static constexpr unsigned long STORM_WINDOW_MS = 1000;

    static void IRAM_ATTR onPinAChange(void* arg);

    uint8_t _pinA = 0;
    uint8_t _pinB = 0;
    volatile long _ticks = 0;
    volatile long _totalTicks = 0;
    volatile unsigned long _totalEdges = 0;
    unsigned long _stormWindowStartMs = 0;
    unsigned long _stormWindowEdges = 0;
    bool _stormed = false;
    // ESP32-specific spinlock, not noInterrupts()/interrupts(): those only
    // suspend the current core, which isn't enough if the GPIO ISR ever
    // ends up scheduled on the other core than the one calling
    // readAndResetTicks(). portMUX is the correct primitive for data
    // shared between an ISR and task code on ESP32.
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
