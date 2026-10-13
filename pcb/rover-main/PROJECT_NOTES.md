# Project notes

This file captures the detailed validation points relevant to the second-pass schematic draft in `pcb/rover-main`.

## 1. Design intent

The board is intended to integrate the entire Rover electronics onto a single PCB while keeping external wiring to a minimum. The exact pin assignments must remain synchronized with the firmware pin definitions and with the logical map in `pcb/PCB_CONNEXIONS.md`.

## 2. Block validation

### 2.1 Alimentation / BMS

Critical items:
- `B-` and `MID` must both be treated as protected nets until the MOSFET protection stage.
- `GND` must not be brought directly into the battery negative path.
- `BQ25887` exact schematic and component values must match the application circuit.
- `S-8252` thresholds and RC network must match the selected suffix.
- Use wide copper and short loops for `B+` to `VPACK` and `VBAT_SYS` distribution.
- Add 0Ω jumpers on the outputs of each buck for start-up debugging.

### 2.2 MCU and USB

Critical items:
- `GPIO19`/`GPIO20` reserved for USB native interface.
- `GPIO26-32` are not available on the module in this variant.
- respect strapping conditions for `GPIO0`, `GPIO3`, `GPIO45`, `GPIO46`.
- ESP32-S3 radio zone must remain free of copper and traces.
- maintain short USB paths and a clean differential pair.

### 2.3 Camera

Critical items:
- confirm actual camera module pin order and connector orientation.
- route all camera data lines close to each other and keep XCLK as short as possible.
- ensure the module is powered from the correct rails (`CAM_2V8`, `CAM_1V2`).
- keep the camera near the front and away from the motor and power sections.

### 2.4 Motor driver

Critical items:
- motor current paths should remain isolated from the ESP32 and sensor zones.
- route VM, motor outputs, and sense paths with large copper and proper thermal relief.
- confirm `nSLEEP` path and E-stop logic behavior.
- verify the exact order of JST-PH motor connector pins and encoder pins.

### 2.5 Audio

Critical items:
- verify MAX98357A SD_MODE thresholds and gain configuration.
- keep amplifier outputs short and wide.
- keep microphone away from loudspeaker and motor vibrations.

### 2.6 Sensors

Critical items:
- BME688 placed away from power components and thermal hot spots.
- IMU placed near the robot center of rotation.
- ToF sensors are on the I2C bus with correct XSHUT handling.
- maintain proper pull-ups and no I2C address conflicts.

### 2.7 Display / IHM

Critical items:
- verify cable orientation of the ST7789 display.
- include pin 1 markers, connector names, and orientation labels on the silkscreen.
- use strong control lines for reset and backlight through the TCA9554.

## 3. KiCad quality gate

The draft passes to the next stage only when:
- ERC returns zero errors on the schematic
- DRC returns zero errors on the PCB
- no net remains unrouted in the path to final placement
- all power nets are labeled and consistent with the design notes
- the mechanical enclosure is verified against the board contour

## 4. Production warning

This draft is intentionally conservative, but it is still not a guarantee of fabrication safety. The design must still be reviewed against real datasheets, actual component packages, and the physical chassis before placing any assembly order.
