# Rover PCB net map

This file is a reference map for the schematic and PCB work. It is not a replacement for the final KiCad net labels, but it captures the intended naming and logical grouping.

## Power nets

- `B+` : positive terminal of the 2S pack
- `B-` : negative terminal of the battery pack before protection
- `MID` : cell midpoint, used by charger and protection
- `GND` : logic and chassis ground after the protection stage
- `VPACK` : pack rail after fuse/shunt and before system switch
- `VBAT_SYS` : battery rail after the ON/OFF switch and before 5V buck
- `VIN_LOGIC` : ORed logic supply from battery or USB
- `5V` : regulated 5V system rail
- `3V3` : regulated 3.3V rail for logic and sensors
- `CAM_2V8` : camera analog rail
- `CAM_1V2` : camera digital rail

## Control and interface nets

- `SDA` : I2C data
- `SCL` : I2C clock
- `ESTOP_N` : emergency stop signal; hardware disables motor driver
- `AMP_EN` : amplifier enable, active high
- `CAM_PWDN` : camera power down control
- `LCD_RST` : display reset
- `LCD_BLK` : display backlight enable
- `I2S_BCLK` : I2S bit clock
- `I2S_LRCLK` : I2S left/right clock
- `I2S_DOUT` : digital audio output to amplifiers
- `I2S_DIN` : digital audio input from microphone
- `USB_DP`, `USB_DM` : native USB data pair
- `CAM_XCLK` : camera clock
- `CAM_VSYNC`, `CAM_HREF`, `CAM_PCLK` : camera timing signals

## Driver and motion nets

- `MOT_L_IN1`, `MOT_L_IN2`
- `MOT_R_IN1`, `MOT_R_IN2`
- `MOT_L_OUT1`, `MOT_L_OUT2`
- `MOT_R_OUT1`, `MOT_R_OUT2`
- `ENC_L_A`, `ENC_L_B`
- `ENC_R_A`, `ENC_R_B`
- `SERVO_PWM`

## Sensor block nets

- `IMU_INT1`, `IMU_INT2` if used
- `TOF_L_XSHUT`, `TOF_R_XSHUT`
- `TCA_INT`
- `BATT_SENSE_V`, `BATT_SENSE_I`

## Notes

Keep these names consistent in the KiCad schematic, PCB, and firmware. The files in this repo assume this naming scheme; if a net name changes, the firmware and hardware documentation must be changed in the same patch.
