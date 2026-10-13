# Project review notes

This document captures the critical items that must be checked before the design is considered production-ready.

## Mandatory reviews

1. ESP32-S3 antenna keepout
   - module must sit on board edge
   - no copper under the antenna on all layers
   - do not place large copper pours near the RF section

2. Camera FPC connector
   - OV2640 pin order must be confirmed against the actual module or Espressif reference board
   - possible 1<->24 inversion depending on connector side
   - print at 1:1 and validate with actual cable before routing final board

3. BMS and battery protection
   - B− must not be directly connected to GND before the protection MOSFETs
   - confirm S-8252 suffix thresholds and exact resistor values from datasheet
   - check fuse/PTC and shunt placement

4. Charging path
   - BQ25887 charge control must match datasheet exact schematic
   - confirm USB-C CC resistors and VBUS path
   - ensure no reverse current from USB to battery via power path

5. Power rails
   - buck 5V and 3.3V outputs need 0Ω jumpers for staged startup
   - confirm LDO camera rails 2.8V / 1.2V
   - decoupling capacitors at every power pin

6. Driver thermal layout
   - DRV8833 and MAX98357A require thermal pad and copper pours with sufficient vias
   - keep power loops short

7. I2C and GPIO mapping
   - verify addresses and pull-ups
   - check GPIO limits: no use of GPIO26-32, USB D+/D- on 19/20, strapping pins on 0/3/45/46

8. Mechanical fit
   - board contour and mounting holes from chassis DXF
   - connector access points must match enclosure geometry
   - camera cable bend radius and head assembly clearance

9. Final software validation
   - prior to fabrication, firmware pin map must match the final PCB exactly
   - test with power-limited supply before connecting full battery pack
