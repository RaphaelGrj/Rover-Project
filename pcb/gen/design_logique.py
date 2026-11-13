# -*- coding: utf-8 -*-
"""
Carte LOGIQUE du ROVER (carte du haut, 75 x 50 mm, 4 couches).

Contient l'ESP32-S3 (antenne vers l'avant), la nappe caméra, les deux amplis
audio et le micro, l'IMU, le BME688, l'expandeur d'E/S, et les connecteurs
écran, capteurs de distance, servo et Qwiic. Transcrit `pcb/PCB_CONNEXIONS.md`
§4 et §5.2, §5.4 à §5.10 ; les GPIO sont ceux de
`esp32/include/board_pins_pcb_v1.h`. L'énergie vient de la carte puissance
par le connecteur inter-cartes J20.

Repère : origine en haut à gauche vue de dessus, X vers l'arrière du robot,
Y vers le bas, en mm. `side="B"` = face de dessous.
"""
from parts import (Board, BOARD_W, BOARD_H, HOLES, ANTENNA, FP, PIN_NAMES,   # noqa: F401
                   CONN_XL, CONN_Y0, CONN_PITCH, conn_pins_header)

NAME = "rover_logique"
TITLE = "ROVER - carte logique (haut)"
# Les deux encoches du bord avant isolent thermiquement le BME688 (§5.6) : il reste
# porté par une languette reliée au reste de la carte par son seul côté droit.
NOTCHES = [(0.0, 36.2, 5.2, 37.0), (0.0, 41.0, 5.2, 41.8)]
OUTLINE = [(0, 0), (75, 0), (75, 50), (0, 50), (0, 41.8), (5.2, 41.8), (5.2, 41.0), (0, 41.0),
           (0, 37.0), (5.2, 37.0), (5.2, 36.2), (0, 36.2)]
PLANES = {"In1": "GND", "In2": "+3V3"}

B = Board()
part, R, C = B.part, B.R, B.C
PARTS = B.parts
nets = B.nets

GND, V33, V5 = "GND", "+3V3", "+5V"
SDA, SCL = "SDA", "SCL"

# Le +5V arrive sur des broches d'ampli au pas de 0,5 mm : pistes de 0,3 mm.
NETCLASS = {V5: "Moyen"}
for _n in ("SPKL_P", "SPKL_N", "SPKR_P", "SPKR_N", "CAM_2V8", "CAM_1V2"):
    NETCLASS[_n] = "Moyen"

# Largeur visée après routage (widen.py élargit chaque segment autant que la place le permet).
WIDEN = {V5: 0.5, "SPKL_P": 0.4, "SPKL_N": 0.4, "SPKR_P": 0.4, "SPKR_N": 0.4}

# =============================================================================
# 1. ESP32-S3-WROOM-1-N16R2 (antenne vers l'avant). GPIO = PCB_CONNEXIONS.md §4
# =============================================================================
part("U1", "ESP32-S3-WROOM-1-N16R2", "ESP", 13.0, 25.0, 90, {
    1: GND, 2: V33, 3: "EN", 40: GND, 41: GND,
    4: "CAM_SIOD", 5: "CAM_SIOC", 6: "CAM_VSYNC", 7: "CAM_HREF", 8: "CAM_XCLK",   # IO4,5,6,7,15
    9: "CAM_Y9", 10: "CAM_Y8", 11: "CAM_Y7", 12: "CAM_Y4",                         # IO16,17,18,8
    13: "USB_DM", 14: "USB_DP",                                                     # IO19,20
    15: "ESTOP_N", 16: "MR_IN2",                                                    # IO3, IO46
    17: "CAM_Y3", 18: "CAM_Y5", 19: "CAM_Y2", 20: "CAM_Y6", 21: "CAM_PCLK",        # IO9..13
    22: "ENC_L_A", 23: "ENC_L_B", 24: "ML_IN1", 25: "ML_IN2", 26: "MR_IN1",        # IO14,21,47,48,45
    27: "IO0", 28: "ENC_R_A", 29: "ENC_R_B", 30: "SERVO",                           # IO0,35,36,37
    31: "I2S_BCLK", 32: "I2S_LRCLK", 33: "I2S_DOUT", 34: "MIC_SD",                  # IO38..41
    35: "SCR_SCK", 36: "SCR_DC", 37: "SCR_MOSI",                                    # IO42,44,43
    38: SCL, 39: SDA,                                                               # IO2, IO1
}, kind="ESP", lcsc="C2913205")
C("C1", "10uF 10V", 9.6, 31.4, 0, V33, GND, "C0805", side="B")
C("C2", "100nF", 9.6, 29.4, 0, V33, GND, side="B")
R("R1", "10k", 12.8, 31.4, 90, V33, "EN", side="B")
C("C3", "1uF", 14.6, 31.2, 90, GND, "EN", "C0603", side="B")
R("R2", "10k", 65.5, 18.6, 0, V33, "IO0")   # près du bouton BOOT
part("SW2", "RESET", "BTN", 70.2, 11.0, 0, {1: "EN", 2: GND}, kind="BTN")
part("SW3", "BOOT", "BTN", 70.2, 16.0, 0, {1: "IO0", 2: GND}, kind="BTN")
R("R3", "4.7k", 9.0, 20.0, 90, V33, SDA, side="B")
R("R4", "4.7k", 10.6, 20.0, 90, V33, SCL, side="B")

# =============================================================================
# 2. Connecteur inter-cartes (barrette mâle sous la carte, vers la carte puissance)
# =============================================================================
part("J20", "Vers carte puissance (barrette 2x15)", "HDR30", CONN_XL, CONN_Y0 + 14 * CONN_PITCH, 0,
     conn_pins_header(), kind="CONN", side="B")


# =============================================================================
# 3. Audio : 2 amplis I2S (stéréo), micro
# =============================================================================
def amp(u, j, x, name, sd_value, n):
    sp, sn, sd, gain = "SPK%s_P" % name, "SPK%s_N" % name, "AMP_SD_%s" % name, "AMP_GAIN_%s" % name
    part(u, "MAX98357AETE+T", "TQFN16", x + 0.05, 12.75, 90, {
        1: "I2S_DOUT", 2: gain, 3: GND, 4: sd, 7: V5, 8: V5, 9: sp, 10: sn, 11: GND,
        14: "I2S_LRCLK", 15: GND, 16: "I2S_BCLK", 17: GND,
    }, kind="MAX98357", lcsc="C910544")
    part(j, "Haut-parleur %s (1:+ 2:-)" % name, "PH2", x, 5.2, 180, {1: sp, 2: sn}, kind="CONN", lcsc="C295747")
    C("C%d" % (60 + n), "10uF 10V", x - 2.6, 11.4, 0, V5, GND, "C0805", side="B")
    C("C%d" % (62 + n), "100nF", x + 2.7, 11.4, 90, V5, GND, side="B")
    # SD_MODE : la valeur de la résistance choisit le canal (10k = gauche, 220k = droit).
    # (placées à y=13,6 et non sous le connecteur : à cet endroit, la carte puissance porte
    #  un condensateur de 10 mm de haut juste en dessous)
    R("R%d" % (40 + n), sd_value, x - 1.0, 13.6, 0, "AMP_EN", sd, side="B")
    R("R%d" % (42 + n), "0 (gain 12dB, option)", x + 1.2, 13.6, 0, gain, GND, side="B", dnp=True)


amp("U8", "J6", 10.4, "L", "10k", 0)
amp("U9", "J7", 19.8, "R", "220k", 1)
part("U10", "ICS-43434", "MIC", 3.4, 11.0, 0,
     {1: "I2S_LRCLK", 2: GND, 3: GND, 4: "I2S_BCLK", 5: V33, 6: "MIC_SD"}, kind="ICS43434", lcsc="C5656610")
C("C64", "100nF", 3.6, 14.2, 0, V33, GND, side="B")
R("R44", "100k", 6.0, 13.6, 0, "MIC_SD", GND, side="B")

# =============================================================================
# 4. Capteurs I2C sur la carte, expandeur d'E/S, voyants
# =============================================================================
part("U11", "LSM6DS3TR-C", "LGA14", 47.5, 14.5, 0, {
    1: V33, 2: GND, 3: GND, 5: V33, 6: GND, 7: GND, 8: V33, 12: V33, 13: SCL, 14: SDA,
}, kind="LSM6DS3", lcsc="C967633")
C("C65", "100nF", 44.6, 14.5, 90, V33, GND)
C("C66", "10uF 10V", 47.5, 18.0, 0, V33, GND, "C0805")
part("U12", "BME688", "BME", 3.0, 39.0, 0,
     {1: GND, 2: V33, 3: SDA, 4: SCL, 5: GND, 6: V33, 7: GND, 8: V33}, kind="BME688", lcsc="C3664478")
C("C67", "100nF", 6.3, 39.0, 90, V33, GND)
part("U13", "TCA9554PWR", "TSSOP16", 21.4, 29.3, 0, {
    1: GND, 2: GND, 3: GND, 4: "XSHUT_R", 5: "AMP_EN", 6: "CAM_PWDN", 7: "SCR_RST", 8: GND,
    9: "SCR_BLK", 10: "DRV_FAULT", 11: "LED_ST", 12: "EXP_P7", 14: SCL, 15: SDA, 16: V33,
}, kind="TCA9554", side="B", lcsc="C477924")
C("C68", "100nF", 21.4, 33.0, 0, V33, GND, side="B")
R("R45", "10k", 33.6, 37.6, 90, V33, "SCR_RST", side="B")
R("R46", "10k", 35.4, 37.6, 90, V33, "SCR_BLK", side="B")
part("D3", "LED bleue (etat)", "LED", 61.2, 23.0, 90, {1: GND, 2: "LED_ST_A"}, kind="LED")
R("R47", "1k", 61.2, 25.65, 90, "LED_ST_A", "LED_ST")
part("D1", "LED verte (3V3)", "LED", 59.2, 23.0, 90, {1: "LED_PWR_K", 2: V33}, kind="LED")
R("R22", "1k", 59.2, 25.65, 90, "LED_PWR_K", GND)

# =============================================================================
# 5. Caméra OV2640 : nappe 24 broches (brochage PCB_CONNEXIONS.md §5.7) et ses rails
# =============================================================================
part("J14", "Camera OV2640 (FPC 24p 0.5mm, contacts dessous)", "FPC24", 15.05, 45.0, 0, {
    2: GND, 3: "CAM_SIOD", 4: "CAM_2V8", 5: "CAM_SIOC", 6: "CAM_RESET", 7: "CAM_VSYNC",
    8: "CAM_PWDN", 9: "CAM_HREF", 10: "CAM_1V2", 11: V33, 12: "CAM_Y9", 13: "CAM_XCLK",
    14: "CAM_Y8", 15: GND, 16: "CAM_Y7", 17: "CAM_PCLK", 18: "CAM_Y6", 19: "CAM_Y2",
    20: "CAM_Y5", 21: "CAM_Y3", 22: "CAM_Y4",
}, kind="FPC24")
part("U14", "XC6206P282MR (2.8V)", "SOT23", 10.5, 37.5, 0, {1: GND, 2: "CAM_2V8", 3: V33},
     kind="XC6206", side="B", lcsc="C347374")
part("U15", "XC6206P122MR-G (1.2V)", "SOT23", 16.0, 37.5, 0, {1: GND, 2: "CAM_1V2", 3: V33},
     kind="XC6206", side="B", lcsc="C424699")
C("C41", "1uF", 13.25, 37.5, 90, V33, GND, "C0603", side="B")
C("C42", "1uF", 10.5, 40.4, 0, "CAM_2V8", GND, "C0603", side="B")
C("C43", "1uF", 16.0, 40.4, 0, "CAM_1V2", GND, "C0603", side="B")
C("C44", "1uF", 21.3, 40.4, 0, V33, GND, "C0603", side="B")
R("R48", "4.7k", 12.0, 34.6, 90, V33, "CAM_SIOD", side="B")
R("R49", "4.7k", 13.6, 34.6, 90, V33, "CAM_SIOC", side="B")
R("R50", "10k", 18.4, 34.6, 90, GND, "CAM_PWDN", side="B")
R("R51", "10k", 20.6, 37.6, 90, V33, "CAM_RESET", side="B")
C("C69", "100nF", 22.3, 37.6, 90, GND, "CAM_RESET", side="B")

# =============================================================================
# 6. Écran, capteurs de distance, servo, Qwiic
# =============================================================================
part("J10", "Ecran ST7789 (GND VCC SCL SDA RES DC CS BLK)", "PH8", 44.0, 44.8, 0,
     {1: GND, 2: V33, 3: "SCR_SCK", 4: "SCR_MOSI", 5: "SCR_RST", 6: "SCR_DC", 7: GND, 8: "SCR_BLK"},
     kind="CONN", lcsc="C265121")
TOF = "VIN GND SCL SDA GPIO1 XSHUT"
part("J8", "VL53L0X gauche (%s)" % TOF, "PH6", 40.5, 5.2, 180,
     {1: V33, 2: GND, 3: SCL, 4: SDA, 6: V33}, kind="CONN", lcsc="C265405")
part("J9", "VL53L0X droit (%s)" % TOF, "PH6", 58.5, 5.2, 180,
     {1: V33, 2: GND, 3: SCL, 4: SDA, 6: "XSHUT_R"}, kind="CONN", lcsc="C265405")
part("J5", "Servo (1:GND 2:5V 3:signal)", "HDR3", 71.5, 22.0, 0, {1: GND, 2: V5, 3: "SERVO"}, kind="CONN")
part("J12", "Qwiic (GND 3V3 SDA SCL)", "SH4", 60.0, 46.6, 0, {1: GND, 2: V33, 3: SDA, 4: SCL},
     kind="CONN", lcsc="C160404")

# =============================================================================
# 7. Points de test (pastilles nues, face de dessous) et trous de fixation M2
# =============================================================================
_TP = [(V33, 40.0, 25.0), (V5, 36.0, 13.0), (GND, 50.0, 30.0), (SDA, 45.0, 25.0), (SCL, 45.0, 28.0),
       ("CAM_2V8", 7.4, 40.4), ("CAM_1V2", 18.7, 40.4), ("EXP_P7", 13.0, 22.5), ("ESTOP_N", 36.0, 33.0)]
for _i, (_net, _x, _y) in enumerate(_TP):
    part("TP%d" % (_i + 1), _net, "TP", _x, _y, 0, {1: _net}, kind="TP", side="B")
for _i, (_x, _y) in enumerate(HOLES):
    part("H%d" % (_i + 1), "M2", "HOLE", _x, _y, 0, {}, kind="HOLE")

# Sérigraphie : (texte, x, y, angle, hauteur, face)
SILK = [
    ("ROVER LOGIQUE V1", 52.0, 34.0, 0, 1.2, "F"),
    ("HP G", 10.4, 2.4, 0, 0.9, "B"), ("HP D", 19.8, 2.4, 0, 0.9, "B"),
    ("TOF G", 40.5, 11.3, 0, 0.9, "F"), ("TOF D", 58.5, 11.3, 0, 0.9, "F"),
    ("ECRAN", 44.0, 38.6, 0, 0.9, "F"), ("CAMERA", 15.0, 39.6, 0, 0.8, "F"), ("QWIIC", 60.0, 41.9, 0, 0.8, "F"),
    ("SERVO", 66.5, 24.5, 90, 0.8, "F"), ("RST", 65.6, 11.0, 0, 0.8, "F"), ("BOOT", 65.2, 16.0, 0, 0.8, "F"),
]

if __name__ == "__main__":
    print(NAME, "%d composants, %d nets" % B.check())
    for name, pads in sorted(nets().items()):
        if len(pads) < 2:
            print("NET A UNE SEULE BROCHE :", name, pads)
