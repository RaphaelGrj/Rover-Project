#pragma once

// Phase 3 head (pitch/yaw servos) pinout and motion constants
// (WIRING.md). PROVISIONAL, same caveats as motion_config.h.

constexpr int ROVER_PIN_SERVO_PITCH = 13;
// Moved off GPIO19 on 2026-09-20: that pad emits nothing on this
// devkit. Established by elimination on real hardware -- both servos
// move on GPIO13, neither moves on GPIO19, and swapping the LEDC
// channel (5 -> 6, hence a different timer) changed nothing while the
// firmware kept reporting the channel as attached. GPIO12 is the pin
// the buzzer used to have; the buzzer is disabled in exchange
// (sound_config.h), there being no free GPIO left on this board.
//
// WARNING: GPIO12 is a strapping pin (flash voltage select). Held high
// during boot, the ESP32 may not start at all. A servo's signal input
// is high impedance, so this is unlikely -- but if the board ever
// fails to boot after this change, this pin is the first suspect.
constexpr int ROVER_PIN_SERVO_YAW = 12;

// LEDC channels 0-3 are reserved for the drive motors (motion_config.h);
// head servos start at 4.
constexpr int ROVER_PWM_CHANNEL_SERVO_PITCH = 4;
// Back to 5 after the channel was cleared as a suspect: moving the yaw
// to channel 6 (a different LEDC timer) did not revive GPIO19, so the
// channel was never the problem -- the pin was.
constexpr int ROVER_PWM_CHANNEL_SERVO_YAW = 5;

// Standard hobby-servo PWM: 50Hz, ~500-2500us pulse width. 16-bit LEDC
// resolution at 50Hz gives ~0.3us/step, comfortably precise -- and well
// within the ESP32 LEDC's frequency*resolution limit (freq * 2^bits <=
// ~80MHz APB clock; 50 * 2^16 is nowhere close).
constexpr int ROVER_SERVO_PWM_FREQ_HZ = 50;
constexpr int ROVER_SERVO_PWM_RESOLUTION_BITS = 16;
constexpr int ROVER_SERVO_MIN_PULSE_US = 500;
constexpr int ROVER_SERVO_MAX_PULSE_US = 2500;

// Soft limits, degrees from the STORED ORIGIN (HeadController::
// loadOrigin), not from the servo's raw centre.
//
// Narrowed to +/-20 on 2026-09-20: the real end stops are still
// unmeasured, and the user reported roughly 35 deg of total travel, so
// +/-20 stays inside that with margin while the head is driven for the
// first time. The previous +/-30 pitch / +/-90 yaw were invented in
// Phase 3 and had never met real mechanics.
// TODO(hardware): replace with the measured stops once the head is
// swept to its limits.
constexpr float ROVER_HEAD_PITCH_MIN_DEG = -20.0f;
constexpr float ROVER_HEAD_PITCH_MAX_DEG = 20.0f;
// TANDEM MOUNT: there is no yaw mechanism on this chassis. The two
// servos face each other on ONE shared axis (confirmed on the robot
// 2026-09-20), so the head has a single degree of freedom, driven by
// pitch. Yaw is accepted from the protocol and ignored, rather than
// removed: ROVER_PROTOCOL.md defines HEAD pitch/yaw, clients send both,
// and rejecting the field would break them for no gain. Should a yaw
// mechanism ever be added, this is where it comes back.
constexpr float ROVER_HEAD_YAW_MIN_DEG = 0.0f;
constexpr float ROVER_HEAD_YAW_MAX_DEG = 0.0f;

// Interpolation speed: a HEAD target is approached smoothly at this
// rate rather than snapping the servo instantly (ARCHITECTURE_AND_
// ROADMAP.md section 11, "interpolation" is a listed responsibility).
constexpr float ROVER_HEAD_MAX_SPEED_DEG_S = 120.0f;
constexpr unsigned long ROVER_HEAD_UPDATE_PERIOD_MS = 20;
