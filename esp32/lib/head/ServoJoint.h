#pragma once

#include <Arduino.h>

// Low-level driver for one hobby servo via LEDC PWM. Takes an angle
// relative to center (-90..+90 = mechanical 0..180 with 90 as
// "straight ahead"); HeadController is the one that knows about
// pitch/yaw and soft limits, this class only ever converts an angle to
// a pulse width.
class ServoJoint {
public:
    // Attaches the pin but leaves the servo RELEASED -- see detach().
    void begin(int pin, int channel);

    // angleDeg is relative to center (0 = straight ahead); internally
    // clamped to the servo's full physical 0..180 range as a hardware
    // safety net on top of whatever soft limits the caller applies.
    // Also re-attaches a released servo.
    void writeAngle(float angleDeg);

    // Stops emitting pulses entirely (duty 0). A hobby servo with no
    // pulse train holds nothing and produces no torque -- it goes limp.
    //
    // This is why begin() no longer centers the servo, which it did
    // until 2026-09-20 ("never start at an undefined pulse"). The
    // reasoning was sound for a single free servo; it is dangerous for
    // Rover's actual head, where two servos face each other and drive
    // ONE shared part (ARCHITECTURE_AND_ROADMAP.md, head section).
    // There, commanding both to "center" the instant power arrives
    // makes each one drag the part toward its own mechanical centre --
    // and if those two centres disagree by even a few degrees, the
    // servos fight each other continuously, stalled, until something
    // gives. Duty 0 is not an undefined pulse: it is a defined, silent,
    // zero-torque state, and it is the only safe thing to do before the
    // real geometry is known.
    void detach();

    bool isAttached() const { return _attached; }

private:
    int _channel = -1;
    bool _attached = false;
};
