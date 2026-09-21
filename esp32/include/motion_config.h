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
// Measured 2026-09-10 with the final wheels mounted on ROVER (replaces the
// old 0.065m placeholder, which was sized off the bare 6.82mm D-shaft with
// no wheel attached). PID gains (180/300/0) were still only characterized
// unloaded/no wheel -- expect retuning once tested under real ground
// load/friction.
constexpr float ROVER_WHEEL_DIAMETER_M = 0.03183f;
constexpr float ROVER_WHEEL_BASE_M = 0.15f;
// Measured 2026-09-02 (SYSTEM action=raw_ticks, bare shaft, 1 hand-counted
// revolution) -- was 700 (placeholder). Single-turn measurement, decent
// but not high-precision; revisit with a multi-turn measurement once a
// real wheel is mounted if odometry accuracy matters later.
constexpr float ROVER_ENCODER_TICKS_PER_REV = 1073.0f;
// MEASURED, not chosen (2026-09-21). The old value here was 0.3 m/s,
// which this robot has never come close to and cannot reach: the
// 2026-09-20 cold bring-up counted 3125 ticks in 10s at PWM 255, and
// with ROVER_ENCODER_TICKS_PER_REV ticks and a 100.0mm wheel
// circumference that is 17.5 rpm = 0.029 m/s. 0.3 was therefore ten
// times the achievable speed, which made EVERY commanded speed an
// unreachable setpoint: the error never closed, the integral parked on
// its limit and the PWM sat pinned at 255 forever. "left_pwm=255
// left_speed=0.00" was read as a dead motor for weeks; it was the
// arithmetic working exactly as written.
//
// Rounded up slightly from the measurement so full bar asks for a
// little more than the wheel can give, rather than a little less.
//
// This is a first honest figure from ONE measurement on a chassis that
// may have been partly loaded, not a specification -- so it is
// overridable at runtime and persisted (SYSTEM action=set_speed,
// CalibrationStore), the same way the PID gains are. Re-measure with
// reset_ticks / motor_raw left=255 right=255 ms=10000 / raw_ticks, cold,
// wheels up and then on the ground, and set it from that.
constexpr float ROVER_MAX_WHEEL_SPEED_MPS = 0.03f;
// Sanity cap on MOVE's rotation field, independent of velocity -- keeps a
// malformed/unexpectedly large command from commanding an unbounded spin
// (the per-wheel PID output is clamped anyway, but this keeps the target
// itself meaningful). Not a measured limit, just a plausible ceiling.
constexpr float ROVER_MAX_ROTATION_RAD_S = 4.0f;

// --- Per-wheel encoder tick sign (DriveController.cpp) ---
// Corrects a mismatch between encoder counting direction and actual motor
// drive direction (positive-feedback runaway otherwise -- see
// DriveController::update()'s comment). Originally one shared constant
// (both wheels needed -1.0f on 2026-09-02). Split into two here
// (2026-09-10) during a session where reconnects on the breadboard kept
// flipping which side needed which sign -- verified on real hardware
// (move_diagnostic.py, COM10) after each change rather than guessed:
// - RIGHT: briefly needed 1.0f (right wheel runaway-spinning after an
//   earlier reconnect that session), but a later reconnect put it back to
//   needing -1.0f, confirmed by a clean convergence to the commanded
//   speed (no saturation).
// - LEFT: stayed at -1.0f through all of that, but then needed 1.0f after
//   the left motor's power leads were resoldered to the driver (reversed
//   its polarity). NOTE: only one of {this sign, the L_IN1/L_IN2 pin
//   assignment} should ever be flipped to compensate a polarity
//   reversal, never both -- flipping both is a no-op for the closed loop
//   (confirmed by measurement: doing both by mistake here reproduced the
//   exact same "stable but backwards" numbers as flipping neither).
// Net lesson: don't trust either constant across a session where the
// wiring gets touched -- re-verify with move_diagnostic.py (clean
// convergence near the commanded speed = correct; saturating at the
// speed cap = wrong sign) any time a motor/encoder connection is
// disturbed, rather than assuming last session's values still hold.
// 2026-09-11 (driver board replaced): move_diagnostic.py showed a stable
// NEGATIVE left_speed while commanded forward, PWM saturated at 255. Tried
// both single-variable fixes in turn -- swapping ROVER_PIN_MOTOR_L_IN1/IN2,
// then (reverted) flipping this sign instead -- and got the same negative
// ~-0.04 result either way. That rules out a simple polarity/encoder-sign
// swap as the explanation (one of the two should have flipped the sign to
// positive) and points at the underlying weak/intermittent left-side
// connection instead: PWM never stops saturating, so the loop likely isn't
// seeing genuine controlled rotation at all, just noise (chassis vibration
// from the right wheel, gear backlash). Left at the last confirmed-good
// value (2026-09-10) rather than an unverified guess -- don't trust it
// until the left-side wiring is solid enough to get a clean read.
constexpr float ROVER_TICK_SIGN_LEFT = 1.0f;
constexpr float ROVER_TICK_SIGN_RIGHT = -1.0f;

// --- Per-wheel PID gains ---
// TODO(hardware): retune once real motors/encoders are available; these
// are untuned placeholders, not measured values.
constexpr float ROVER_PID_KP = 180.0f;
constexpr float ROVER_PID_KI = 300.0f;
constexpr float ROVER_PID_KD = 0.0f;
// Feed-forward: PWM per m/s of commanded speed, so the duty a wheel
// needs is sent straight out instead of being rediscovered by the
// integrator one 20ms step at a time. 255 / 0.03 = 8500, i.e. full
// scale on the speed bar asks for full duty immediately -- which is
// exactly what SYSTEM action=motor_raw left=255 already does by hand.
//
// Before this term existed, the integral was the ONLY route to a usable
// duty, and on a chassis needing 200+ PWM to break away that meant ~5s
// of held joystick at 40% of the bar before anything moved (~10s at
// 20%) -- indistinguishable from a dead motor on a short test.
//
// Keep it consistent with ROVER_MAX_WHEEL_SPEED_MPS: this is 255
// divided by that. Runtime-settable and persisted like the gains
// (SYSTEM action=set_pid kff=...).
constexpr float ROVER_PID_KFF = 8500.0f;

// How often DriveController recomputes PID output / reads encoders.
constexpr unsigned long ROVER_DRIVE_UPDATE_PERIOD_MS = 20;
// How often wheel-speed telemetry (STATE left_speed=... right_speed=...)
// is broadcast to the Pi while ACTIVE.
constexpr unsigned long ROVER_DRIVE_TELEMETRY_PERIOD_MS = 200;
