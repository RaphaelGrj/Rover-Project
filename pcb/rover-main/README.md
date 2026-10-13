# Rover PCB draft

This folder contains a second-pass draft of the PCB design based on the design notes in:
- `pcb/PCB_CONNEXIONS.md`
- `pcb/KICAD_GUIDE.md`

This is still a design draft, not a fabrication-ready board.

The goal of this pass is to move from a loose concept into a consistent schematic map, with explicit nets, block responsibilities, and review points. The final production board still requires human validation of footprints, connector orientation, RF placement, thermal design, and real-world part availability.

## Functional blocks

The project is split into the same hierarchy as the design notes:
- alimentation
- mcu
- camera
- moteurs
- audio
- capteurs
- ihm

## Rail names and net conventions

These names are the intended design convention and should be kept consistent throughout the schematic and PCB if possible:

- `B+`, `B-`, `MID`
- `GND`
- `VPACK`
- `VBAT_SYS`
- `VIN_LOGIC`
- `5V`
- `3V3`
- `CAM_2V8`
- `CAM_1V2`
- `SDA`, `SCL`
- `ESTOP_N`
- `AMP_EN`
- `CAM_PWDN`
- `LCD_RST`, `LCD_BLK`
- `I2S_BCLK`, `I2S_LRCLK`, `I2S_DOUT`, `I2S_DIN`
- `USB_DP`, `USB_DM`
- `CAM_XCLK`, `CAM_PCLK`, `CAM_HREF`, `CAM_VSYNC`
- `MOT_L_IN1`, `MOT_L_IN2`, `MOT_R_IN1`, `MOT_R_IN2`

## Main electrical architecture

### 1. Battery and BMS

- 2S Li-ion pack enters on JST-VH 3-pin connector.
- `B-` must not be directly tied to `GND` before the protection MOSFETs.
- `MID` is mandatory for 2S charger balancing and protection logic.
- `VPACK` is the pack side after fuse/shunt.
- `VBAT_SYS` is the protected system rail feeding the motor driver and 5V buck.
- `VIN_LOGIC` is the ORed logic supply from USB and battery through Schottky diodes.

### 2. USB-C and charging

- USB-C is used for firmware flashing and battery charging.
- `VBUS` from USB-C connects to BQ25887.
- `CC1` and `CC2` must have 5.1 kΩ to GND.
- `BQ25887` configures charge current and monitored cell voltages.
- `STAT` drives orange charging LED.

### 3. Power conversion

- 5V buck feeds servo + audio amps.
- 3V3 buck feeds ESP32-S3, sensors, I2C bus, and logic.
- LDOs generate `CAM_2V8` and `CAM_1V2` for OV2640.
- Use 0Ω shunts on the regulator outputs for staged startup and safe debugging.

### 4. MCU and IO

- ESP32-S3-WROOM-1 on board edge, with antenna keepout area.
- USB native on GPIO19/20.
- I2C bus on GPIO1/2.
- I2S on GPIO38/39/40/41.
- SPI display on GPIO42/43/44.
- Motors on GPIO45/46/47/48.
- Servo on GPIO37.
- Encoders capture on GPIO14,21,35,36.
- E-stop logic on GPIO3 with `ESTOP_N` controlling DRV8833 `nSLEEP`.
- TCA9554 at 0x20 handles slow control lines.

### 5. Camera block

- OV2640 FPC 24-pin interface.
- SCCB bus on GPIO4/GPIO5.
- `CAM_XCLK` on GPIO15.
- Data bus mapped to Y0-Y9 and VSYNC/HREF/PCLK.
- `CAM_PWDN` controlled by TCA9554 P2.
- Supply rails `CAM_2V8` and `CAM_1V2` must be validated against the module datasheet.

### 6. Motor block

- DRV8833 uses `VBAT_SYS` on VM and `GND` on logic ground.
- Outputs to two JST-PH 6-pin connectors.
- `AIN1/AIN2` and `BIN1/BIN2` must match the actual motor/encoder order.
- `nSLEEP` tied to `ESTOP_N`, with hardware shutdown path.
- Thermal pad area requires vias and copper area.

### 7. Audio block

- Two MAX98357A amplifiers for stereo output.
- Shared I2S bus BCLK/LRCLK/DOUT.
- `AMP_EN` from TCA9554 controls channel enable and prevents startup pops.
- ICS-43434 microphone on same I2S interface.

### 8. Sensors block

- `LSM6DS3TR-C` at 0x6B
- `BME688` at 0x76
- `INA219` at 0x40
- `BQ25887` at 0x6A
- `TCA9554` at 0x20
- ToF sensors go on the I2C bus, left/right enabled via XSHUT.
- Keep BME688 away from heat sources and provide thermal isolation slot.

### 9. IHM / display block

- ST7789 display on SPI, with reset/brightness control through TCA9554.
- Servo head on GPIO37.
- LED output and test points for debug.
- Include explicit silkscreen names and pin 1 indicators for all connectors.

## Draft review checklist

Before any order is placed, this must be checked in KiCad and with physical parts:

- ERC = 0 errors; warnings reviewed individually
- DRC = 0 errors; no un-routed nets
- board contour and mounting holes match chassis DXF
- ESP32-S3 antenna area has no copper on all layers
- FPC camera connector orientation checked with actual cable
- battery protection path is physically correct and separated from `GND`
- all regulator datasheets copied exactly, including caps and feedback network
- thermal pads and power loops are routed with wide copper and stitching vias
- display and connector silkscreen are correct and legible
- power rails staged with 0Ω jumpers for debug-safe startup

## Next step

The next phase is PCB placement and routing, with:
1. mechanical constraints from the chassis
2. power and thermal hotspots first
3. RF zone protection for ESP32-S3
4. differential USB routing
5. camera trace length control and stable clock routing
6. final DRC and print-to-scale review

This is still a design iteration, not a final board release.
