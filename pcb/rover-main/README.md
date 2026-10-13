# Rover PCB draft

This folder contains a first KiCad draft produced from the project notes in:
- `pcb/PCB_CONNEXIONS.md`
- `pcb/KICAD_GUIDE.md`

This is a draft for review and iteration only. It is not a fabrication-ready PCB.

Main goals of the draft:
- reflect the logical block split recommended in the project docs
- create a place to review all major nets and interfaces
- identify critical validation points before production

Critical checks before ordering hardware:
- ESP32-S3 antenna keepout / keepout perimeter on all layers
- camera FPC 24-pin orientation and real-world net order
- BMS and charge path: BQ25887 + S-8252 + MOSFET protection
- exact footprint and part availability for JST, USB-C, DRV8833, MAX98357A, BME688, LSM6DS3TR-C
- ERC and DRC clean state in KiCad
- mechanical fit with chassis DXF and enclosure height constraints

Project tree:
- `rover-main.kicad_pro` : project file
- `rover-main.kicad_sch` : top-level schematic with hierarchical sheets
- `alimentation.kicad_sch` : battery, BMS, USB-C charge, buck regulators
- `mcu.kicad_sch` : ESP32-S3 + USB-C + GPIO mapping + TCA9554
- `camera.kicad_sch` : OV2640 FPC and supplies
- `moteurs.kicad_sch` : DRV8833 + E-stop + motor connectors
- `audio.kicad_sch` : MAX98357A + ICS-43434 + I2S
- `capteurs.kicad_sch` : IMU + BME688 + ToF + Qwiic
- `ihm.kicad_sch` : display + servo + LED + test points
- `PROJECT_NOTES.md` : explicit review checklist
- `BOM.csv` : preliminary BOM suggestion
