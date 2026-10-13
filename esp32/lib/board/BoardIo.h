#pragma once

#include <Arduino.h>

// Slow, board-specific control lines, addressed by WHAT they do rather
// than by pin number.
//
// Why this exists: on the WROOM devkit the right ToF's XSHUT is a plain
// GPIO, while on the custom PCB it -- and the display reset, backlight,
// amp enable, camera power-down and status LED -- sits on an I2C I/O
// expander (pcb/PCB_CONNEXIONS.md section 4.1: the S3 module has no GPIO
// left). Callers say "pull the right ToF's XSHUT low" and never learn
// which of the two it is, so DistanceSensor/DisplayEngine stay the same
// code on both boards.
enum class BoardLine : uint8_t {
    TofRightXshut,
    AmpEnable,
    CameraPowerDown,
    DisplayReset,
    DisplayBacklight,
    StatusLed,
};

namespace BoardIo {

// Starts the shared sensor I2C bus. Idempotent: SensorHub calls it too,
// and whichever runs first does the work. Lives here rather than in
// SensorHub because on the PCB the expander -- hence the display reset
// -- needs the bus before the display is initialized.
void beginI2C();

// Call once, early in setup(), before any module that uses a BoardLine.
// Brings every line to its safe state (see board_pins_pcb_v1.h).
void begin();

// Non-blocking; retries a missing/lost expander every
// ROVER_SENSOR_RETRY_PERIOD_MS, restoring the levels last requested.
void update();

// Drives a line. Returns true when the line now has that level OR does
// not exist on this board (nothing to do); false only on a communication
// failure, which callers may report.
bool write(BoardLine line, bool high);

// Lowest-power state for every line, before a low-battery deep sleep
// (LowBatterySleep): amps shut down, backlight off, camera powered
// down, right ToF in reset, LED off. Best effort -- an I2C failure here
// cannot be reported any more, and the protection IC still guards the
// cells. On the WROOM only the ToF line exists.
void prepareForSleep();

// True if this board routes lines through an expander at all.
bool hasExpander();
// False while the expander is absent or stopped answering.
bool expanderOk();

// DRV8833 nFAULT (PCB only). Returns false when it cannot be read (no
// such line on this board, or I2C failure); otherwise sets *faultOut.
bool readMotorFault(bool* faultOut);

// True exactly once per OK -> failed transition of the expander, so
// main.cpp reports it once instead of every retry (same pattern as
// SensorHub::consumeSensorFailure).
bool consumeExpanderFailure();

}  // namespace BoardIo
