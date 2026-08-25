#pragma once

// Phase 3 head (pitch/yaw servos) pinout and motion constants
// (WIRING.md). PROVISIONAL, same caveats as motion_config.h.

constexpr int ROVER_PIN_SERVO_PITCH = 13;
constexpr int ROVER_PIN_SERVO_YAW = 19;

// LEDC channels 0-3 are reserved for the drive motors (motion_config.h);
// head servos start at 4.
constexpr int ROVER_PWM_CHANNEL_SERVO_PITCH = 4;
constexpr int ROVER_PWM_CHANNEL_SERVO_YAW = 5;

// Standard hobby-servo PWM: 50Hz, ~500-2500us pulse width. 16-bit LEDC
// resolution at 50Hz gives ~0.3us/step, comfortably precise -- and well
// within the ESP32 LEDC's frequency*resolution limit (freq * 2^bits <=
// ~80MHz APB clock; 50 * 2^16 is nowhere close).
constexpr int ROVER_SERVO_PWM_FREQ_HZ = 50;
constexpr int ROVER_SERVO_PWM_RESOLUTION_BITS = 16;
constexpr int ROVER_SERVO_MIN_PULSE_US = 500;
constexpr int ROVER_SERVO_MAX_PULSE_US = 2500;

// Soft limits, degrees from center (0 = looking straight ahead).
// TODO(hardware): placeholders until the real head mechanics (mount,
// range of motion before something collides) are known.
constexpr float ROVER_HEAD_PITCH_MIN_DEG = -30.0f;
constexpr float ROVER_HEAD_PITCH_MAX_DEG = 30.0f;
constexpr float ROVER_HEAD_YAW_MIN_DEG = -90.0f;
constexpr float ROVER_HEAD_YAW_MAX_DEG = 90.0f;

// Interpolation speed: a HEAD target is approached smoothly at this
// rate rather than snapping the servo instantly (ARCHITECTURE_AND_
// ROADMAP.md section 11, "interpolation" is a listed responsibility).
constexpr float ROVER_HEAD_MAX_SPEED_DEG_S = 120.0f;
constexpr unsigned long ROVER_HEAD_UPDATE_PERIOD_MS = 20;
