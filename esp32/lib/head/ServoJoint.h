#pragma once

#include <Arduino.h>

// Low-level driver for one hobby servo via LEDC PWM. Takes an angle
// relative to center (-90..+90 = mechanical 0..180 with 90 as
// "straight ahead"); HeadController is the one that knows about
// pitch/yaw and soft limits, this class only ever converts an angle to
// a pulse width.
class ServoJoint {
public:
    void begin(int pin, int channel);

    // angleDeg is relative to center (0 = straight ahead); internally
    // clamped to the servo's full physical 0..180 range as a hardware
    // safety net on top of whatever soft limits the caller applies.
    void writeAngle(float angleDeg);

private:
    int _channel = -1;
};
