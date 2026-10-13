#pragma once

#include <cstdint>

// Complete pinout of the custom Rover PCB, version 1 (ESP32-S3-WROOM-1
// N16R2 + integrated camera, DRV8833, 2 x MAX98357A, ICS-43434,
// LSM6DS3TR-C, BME688, INA219, BQ25887, TCA9554).
//
// ONE file on purpose, mirroring the GPIO table of
// pcb/PCB_CONNEXIONS.md section 4 line for line, so the firmware can be
// checked against the schematic in a single read. Each domain header
// (motion_config.h, display_config.h, ...) includes this instead of its
// own WROOM numbers when built with -D ROVER_BOARD_PCB_V1 (env
// "rover_pcb_v1" in platformio.ini); the WROOM pinout is untouched.
//
// NOT VALIDATED ON HARDWARE: the board has not been made yet
// (2026-10-13). Any change to the schematic must land here AND in
// pcb/PCB_CONNEXIONS.md, in the same commit.
//
// Strapping pins, for whoever edits this: GPIO0 is the BOOT button only;
// GPIO45 and GPIO46 must read LOW at reset (VDD_SPI voltage / download
// mode), which is why they carry motor inputs -- the DRV8833's internal
// pull-downs hold them low, and a motor input low at boot is also the
// safe state. Never put a pull-up on either.

#if !defined(ROVER_BOARD_S3)
#error "board_pins_pcb_v1.h targets the ESP32-S3: build with the rover_pcb_v1 environment."
#endif

// --- Drive (DRV8833, PWM on both inputs) --------------------------------
constexpr int ROVER_PIN_MOTOR_L_IN1 = 47;  // AIN1
constexpr int ROVER_PIN_MOTOR_L_IN2 = 48;  // AIN2
constexpr int ROVER_PIN_MOTOR_R_IN1 = 45;  // BIN1, strapping: low at boot
constexpr int ROVER_PIN_MOTOR_R_IN2 = 46;  // BIN2, strapping: low at boot

// Encoders: 10k pull-ups to 3V3 on the board (the open question of
// esp32/WIRING.md is settled in hardware here). GPIO35/36 are only free
// because the module is an R2 (quad PSRAM): on an R8 they belong to the
// octal PSRAM and this pinout cannot work.
constexpr int ROVER_PIN_ENCODER_L_A = 14;
constexpr int ROVER_PIN_ENCODER_L_B = 21;
constexpr int ROVER_PIN_ENCODER_R_A = 35;
constexpr int ROVER_PIN_ENCODER_R_B = 36;

// --- Sensor I2C bus (I2C0) ----------------------------------------------
// LSM6DS3TR-C 0x6B, BQ25887 0x6A, BME688 0x76, INA219 0x40, TCA9554 0x20,
// VL53L0X 0x29 (right) / 0x30 (left, re-addressed). The camera's SCCB is
// a separate bus (I2C1, owned by esp32-camera), see below.
constexpr int ROVER_PIN_I2C_SDA = 1;
constexpr int ROVER_PIN_I2C_SCL = 2;

// --- E-stop ---------------------------------------------------------------
// Reads the net that is ALSO the DRV8833's nSLEEP: pressing the button
// cuts the H-bridges in hardware, even with the firmware hung, and this
// pin only lets the firmware notice. 10k pull-up on the board.
constexpr int ROVER_PIN_ESTOP = 3;

// --- Head servo -------------------------------------------------------------
constexpr int ROVER_PIN_SERVO_PITCH = 37;
// Single servo + bearing since 2026-09-20: no second servo on this board.
constexpr int ROVER_PIN_SERVO_YAW = -1;

// --- Display (ST7789 in the head, 8-pin JST) -------------------------------
constexpr int ROVER_PIN_DISPLAY_SCLK = 42;
constexpr int ROVER_PIN_DISPLAY_MOSI = 43;  // ex-U0TXD, see PCB_CONNEXIONS.md
constexpr int ROVER_PIN_DISPLAY_DC = 44;    // ex-U0RXD
// CS is tied to GND on the board (the panel is the only SPI device), and
// RST/backlight go through the I/O expander -- see the TCA9554 section.
constexpr int ROVER_PIN_DISPLAY_CS = -1;
constexpr int ROVER_PIN_DISPLAY_RST = -1;

// --- Audio (I2S0 full duplex: 2 amps + mic share BCLK/WS) -------------------
constexpr int ROVER_PIN_I2S_BCLK = 38;
constexpr int ROVER_PIN_I2S_LRC = 39;
constexpr int ROVER_PIN_I2S_DIN = 40;      // ESP32 data OUT -> both amps' DIN
constexpr int ROVER_PIN_I2S_MIC_SD = 41;   // ICS-43434 data OUT -> ESP32

// --- Camera (OV2640 on 24-pin FPC) -------------------------------------------
// Exactly Espressif's ESP32-S3-EYE mapping (CAMERA_MODEL_ESP32S3_EYE),
// so the esp32-camera driver is used the way it is tested upstream.
// PWDN goes through the expander, RESET is an RC on the board.
constexpr int ROVER_PIN_CAM_XCLK = 15;
constexpr int ROVER_PIN_CAM_SIOD = 4;
constexpr int ROVER_PIN_CAM_SIOC = 5;
constexpr int ROVER_PIN_CAM_Y9 = 16;
constexpr int ROVER_PIN_CAM_Y8 = 17;
constexpr int ROVER_PIN_CAM_Y7 = 18;
constexpr int ROVER_PIN_CAM_Y6 = 12;
constexpr int ROVER_PIN_CAM_Y5 = 10;
constexpr int ROVER_PIN_CAM_Y4 = 8;
constexpr int ROVER_PIN_CAM_Y3 = 9;
constexpr int ROVER_PIN_CAM_Y2 = 11;
constexpr int ROVER_PIN_CAM_VSYNC = 6;
constexpr int ROVER_PIN_CAM_HREF = 7;
constexpr int ROVER_PIN_CAM_PCLK = 13;

// --- TCA9554 I/O expander (0x20) ----------------------------------------------
// Slow signals that did not fit on the 36 module pins. At power-up every
// expander pin is a high-impedance input, so the board itself holds each
// line in its safe state through a resistor until the firmware takes
// over (PCB_CONNEXIONS.md section 4.1). The default levels below are the
// SAME safe states, written before the pins are switched to outputs.
constexpr uint8_t ROVER_IO_EXPANDER_ADDRESS = 0x20;
constexpr uint8_t ROVER_EXP_TOF_RIGHT_XSHUT = 0;  // out, high = sensor on
constexpr uint8_t ROVER_EXP_AMP_ENABLE = 1;       // out, low = both amps shut down
constexpr uint8_t ROVER_EXP_CAM_PWDN = 2;         // out, low = camera on
constexpr uint8_t ROVER_EXP_DISPLAY_RST = 3;      // out, high = running
constexpr uint8_t ROVER_EXP_DISPLAY_BLK = 4;      // out, high = backlight on
constexpr uint8_t ROVER_EXP_DRV_FAULT = 5;        // in, low = DRV8833 fault (nFAULT, open drain)
constexpr uint8_t ROVER_EXP_STATUS_LED = 6;       // out, high = LED on
constexpr uint8_t ROVER_EXP_SPARE = 7;            // in, unused (test pad)

// Bit masks for the expander's registers: 1 = input in the config
// register; outputs start at the safe levels listed above.
constexpr uint8_t ROVER_EXP_INPUT_MASK =
    (1u << ROVER_EXP_DRV_FAULT) | (1u << ROVER_EXP_SPARE);
constexpr uint8_t ROVER_EXP_SAFE_OUTPUTS =
    (1u << ROVER_EXP_TOF_RIGHT_XSHUT) | (1u << ROVER_EXP_DISPLAY_RST) |
    (1u << ROVER_EXP_DISPLAY_BLK);

// --- Power path chips (I2C0) -----------------------------------------------------
constexpr uint8_t ROVER_INA219_ADDRESS = 0x40;
constexpr uint8_t ROVER_CHARGER_ADDRESS = 0x6A;  // BQ25887, fixed by the part
