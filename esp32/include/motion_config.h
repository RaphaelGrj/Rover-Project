#pragma once

// Phase 2 pinout and motion-control constants (ARCHITECTURE_AND_ROADMAP.md
// Phase 2, esp32/WIRING.md).
//
// PROVISIONAL: these pin numbers and gains are not confirmed against real
// hardware. They exist so Phase 2 can be built and validated in Wokwi
// simulation while we wait for the real wiring/motor/encoder specs (see
// PROGRESS.md, session where this was introduced). Once real hardware
// info is available, only this file should need to change -- nothing
// downstream should hardcode a pin number of its own.
//
// Same pin numbers are used for WROOM and S3 for now; the S3 mapping
// still needs its own per-pin sanity check (different strapping pins),
// see WIRING.md -- it isn't inherited "for free" just because it compiles.

// --- DRV8833 (dual H-bridge, PWM-on-both-inputs mode) driving N20 6V
// gear motors with encoders (confirmed hardware, PROGRESS.md 2026-08-25).
// Unlike the TB6612FNG originally sketched here, the DRV8833 has no
// separate direction+enable pins: each side is one PWM-capable pin per
// direction (IN1 driven = forward at that duty, IN2 driven = reverse,
// both 0 = coast/stop). That means only 4 GPIOs for both motors instead
// of 6. SLP (sleep/enable) is hardwired to 3V3 in this base wiring (see
// WIRING.md), so there is no GPIO for it here.
constexpr int ROVER_PIN_MOTOR_L_IN1 = 27;  // forward PWM, left motor
constexpr int ROVER_PIN_MOTOR_L_IN2 = 26;  // reverse PWM, left motor
constexpr int ROVER_PIN_MOTOR_R_IN1 = 33;  // forward PWM, right motor
constexpr int ROVER_PIN_MOTOR_R_IN2 = 32;  // reverse PWM, right motor
// GPIO14 and GPIO25 (PWMA/PWMB in the old TB6612FNG plan) are unused now.

// --- Encoders (quadrature, x1 decode) ---
constexpr int ROVER_PIN_ENCODER_L_A = 34;
constexpr int ROVER_PIN_ENCODER_L_B = 35;
constexpr int ROVER_PIN_ENCODER_R_A = 36;
constexpr int ROVER_PIN_ENCODER_R_B = 39;

// --- LEDC (ESP32 hardware PWM) setup for the motor driver. One channel
// per direction pin (4 total) since the DRV8833 PWMs both inputs.
// Channels 0-3 are reserved for the drive motors; a future module (e.g.
// head servos in Phase 3) needing LEDC should start at channel 4 to
// avoid silently fighting these for the same PWM timer/channel. ---
constexpr int ROVER_PWM_CHANNEL_L_FWD = 0;
constexpr int ROVER_PWM_CHANNEL_L_REV = 1;
constexpr int ROVER_PWM_CHANNEL_R_FWD = 2;
constexpr int ROVER_PWM_CHANNEL_R_REV = 3;
constexpr int ROVER_PWM_FREQ_HZ = 20000;      // above audible range
constexpr int ROVER_PWM_RESOLUTION_BITS = 8;  // duty cycle 0..255

// --- Robot geometry / encoder specs ---
// TODO(hardware): replace with the real values once the exact N20+encoder
// variant/gear ratio is confirmed and measured. These are only
// placeholders so the PID loop has a plausible order of magnitude to
// work with in simulation -- N20 encoder ticks/rev varies a lot by gear
// ratio (common values range from a few hundred to a few thousand).
constexpr float ROVER_WHEEL_DIAMETER_M = 0.065f;
constexpr float ROVER_WHEEL_BASE_M = 0.15f;
constexpr float ROVER_ENCODER_TICKS_PER_REV = 700.0f;
constexpr float ROVER_MAX_WHEEL_SPEED_MPS = 0.3f;
// Sanity cap on MOVE's rotation field, independent of velocity -- keeps a
// malformed/unexpectedly large command from commanding an unbounded spin
// (the per-wheel PID output is clamped anyway, but this keeps the target
// itself meaningful). Not a measured limit, just a plausible ceiling.
constexpr float ROVER_MAX_ROTATION_RAD_S = 4.0f;

// --- Per-wheel PID gains ---
// TODO(hardware): retune once real motors/encoders are available; these
// are untuned placeholders, not measured values.
constexpr float ROVER_PID_KP = 180.0f;
constexpr float ROVER_PID_KI = 300.0f;
constexpr float ROVER_PID_KD = 0.0f;

// How often DriveController recomputes PID output / reads encoders.
constexpr unsigned long ROVER_DRIVE_UPDATE_PERIOD_MS = 20;
// How often wheel-speed telemetry (STATE left_speed=... right_speed=...)
// is broadcast to the Pi while ACTIVE.
constexpr unsigned long ROVER_DRIVE_TELEMETRY_PERIOD_MS = 200;
