# -*- coding: utf-8 -*-
"""
Contrôle de cohérence carte <-> firmware, sans matériel.

Compare les GPIO câblés sur les deux cartes (pcb/gen/design_logique.py et
design_puissance.py) à ceux que le firmware
utilise pour cette carte (esp32/include/board_pins_pcb_v1.h). Une seule broche
différente et la fonction correspondante ne marcherait pas sur la carte réelle,
sans aucune erreur de compilation : c'est exactement ce que ce test attrape.

Usage (n'importe quel Python 3) :
    python pcb/test/check_pins.py
Code de retour 0 si tout concorde, 1 sinon.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "gen"))
import design_logique as LOG      # noqa: E402
import design_puissance as PUI    # noqa: E402
import parts                      # noqa: E402

ALL_PARTS = {"logique": LOG.PARTS, "puissance": PUI.PARTS}

HEADER = os.path.join(HERE, "..", "..", "esp32", "include", "board_pins_pcb_v1.h")

# Constante du firmware -> nom du net sur la carte.
GPIO_NETS = {
    "ROVER_PIN_MOTOR_L_IN1": "ML_IN1", "ROVER_PIN_MOTOR_L_IN2": "ML_IN2",
    "ROVER_PIN_MOTOR_R_IN1": "MR_IN1", "ROVER_PIN_MOTOR_R_IN2": "MR_IN2",
    "ROVER_PIN_ENCODER_L_A": "ENC_L_A", "ROVER_PIN_ENCODER_L_B": "ENC_L_B",
    "ROVER_PIN_ENCODER_R_A": "ENC_R_A", "ROVER_PIN_ENCODER_R_B": "ENC_R_B",
    "ROVER_PIN_I2C_SDA": "SDA", "ROVER_PIN_I2C_SCL": "SCL", "ROVER_PIN_ESTOP": "ESTOP_N",
    "ROVER_PIN_SERVO_PITCH": "SERVO",
    "ROVER_PIN_DISPLAY_SCLK": "SCR_SCK", "ROVER_PIN_DISPLAY_MOSI": "SCR_MOSI", "ROVER_PIN_DISPLAY_DC": "SCR_DC",
    "ROVER_PIN_I2S_BCLK": "I2S_BCLK", "ROVER_PIN_I2S_LRC": "I2S_LRCLK", "ROVER_PIN_I2S_DIN": "I2S_DOUT",
    "ROVER_PIN_I2S_MIC_SD": "MIC_SD",
    "ROVER_PIN_CAM_XCLK": "CAM_XCLK", "ROVER_PIN_CAM_SIOD": "CAM_SIOD", "ROVER_PIN_CAM_SIOC": "CAM_SIOC",
    "ROVER_PIN_CAM_Y9": "CAM_Y9", "ROVER_PIN_CAM_Y8": "CAM_Y8", "ROVER_PIN_CAM_Y7": "CAM_Y7",
    "ROVER_PIN_CAM_Y6": "CAM_Y6", "ROVER_PIN_CAM_Y5": "CAM_Y5", "ROVER_PIN_CAM_Y4": "CAM_Y4",
    "ROVER_PIN_CAM_Y3": "CAM_Y3", "ROVER_PIN_CAM_Y2": "CAM_Y2", "ROVER_PIN_CAM_VSYNC": "CAM_VSYNC",
    "ROVER_PIN_CAM_HREF": "CAM_HREF", "ROVER_PIN_CAM_PCLK": "CAM_PCLK",
}
# Ligne de l'expandeur TCA9554 -> nom du net sur la carte.
EXP_NETS = {
    "ROVER_EXP_TOF_RIGHT_XSHUT": "XSHUT_R", "ROVER_EXP_AMP_ENABLE": "AMP_EN", "ROVER_EXP_CAM_PWDN": "CAM_PWDN",
    "ROVER_EXP_DISPLAY_RST": "SCR_RST", "ROVER_EXP_DISPLAY_BLK": "SCR_BLK", "ROVER_EXP_DRV_FAULT": "DRV_FAULT",
    "ROVER_EXP_STATUS_LED": "LED_ST", "ROVER_EXP_SPARE": "EXP_P7",
}
# Câblages fixes du cahier des charges (puce, nom de broche) -> net.
FIXED = [
    ("puissance", "U7", "AIN1", "ML_IN1"), ("puissance", "U7", "AIN2", "ML_IN2"),
    ("puissance", "U7", "BIN1", "MR_IN1"), ("puissance", "U7", "BIN2", "MR_IN2"),
    ("puissance", "U7", "nSLEEP", "ESTOP_N"), ("puissance", "U7", "nFAULT", "DRV_FAULT"),
    ("puissance", "U7", "VM", "VBAT_SYS"),
    ("logique", "U1", "IO19", "USB_DM"), ("logique", "U1", "IO20", "USB_DP"), ("logique", "U1", "IO0", "IO0"),
    ("logique", "U11", "SA0", "+3V3"),      # IMU en 0x6B (0x6A est pris par le chargeur)
    ("logique", "U12", "SDO", "GND"),       # BME688 en 0x76
    ("logique", "U13", "A0", "GND"), ("logique", "U13", "A1", "GND"), ("logique", "U13", "A2", "GND"),  # 0x20
    ("puissance", "U16", "A0", "GND"), ("puissance", "U16", "A1", "GND"),                               # 0x40
    ("logique", "J10", "7", "GND"),         # CS de l'écran à la masse (ROVER_PIN_DISPLAY_CS = -1)
]
# Signaux qui doivent traverser le connecteur inter-cartes (présents des deux côtés).
CROSSING = ["ML_IN1", "ML_IN2", "MR_IN1", "MR_IN2", "ENC_L_A", "ENC_L_B", "ENC_R_A", "ENC_R_B",
            "SDA", "SCL", "ESTOP_N", "DRV_FAULT", "USB_DP", "USB_DM", "+3V3", "+5V", "GND"]


def pins_by_name(board, ref):
    """{nom de broche: net} pour un composant d'une des deux cartes."""
    part = [p for p in ALL_PARTS[board] if p["ref"] == ref][0]
    names = parts.PIN_NAMES.get(part["kind"], {})
    out = {}
    for pad, net in part["pins"].items():
        key = int(pad) if pad.isdigit() else pad
        out[str(names.get(key, pad))] = net
    return out


def main():
    text = open(HEADER, encoding="utf8").read()
    consts = {m.group(1): int(m.group(2)) for m in
              re.finditer(r"constexpr\s+\w+\s+(ROVER_\w+)\s*=\s*(-?\d+)\s*;", text)}
    esp, exp = pins_by_name("logique", "U1"), pins_by_name("logique", "U13")
    errors, checked = [], 0
    for const, net in GPIO_NETS.items():
        gpio = consts.get(const)
        got = esp.get("IO%d" % gpio) if gpio is not None else None
        checked += 1
        if got != net:
            errors.append("%s = GPIO%s : la carte y relie '%s', attendu '%s'" % (const, gpio, got, net))
    for const, net in EXP_NETS.items():
        bit = consts.get(const)
        got = exp.get("P%d" % bit) if bit is not None else None
        checked += 1
        if got != net:
            errors.append("%s = P%s : la carte y relie '%s', attendu '%s'" % (const, bit, got, net))
    for board, ref, pin, net in FIXED:
        got = pins_by_name(board, ref).get(pin)
        checked += 1
        if got != net:
            errors.append("%s %s.%s : relie a '%s', attendu '%s'" % (board, ref, pin, got, net))
    # Connecteur inter-cartes : même net de part et d'autre de chaque rangée, et tous les
    # signaux partagés présents sur les deux cartes.
    sock, hdr = parts.conn_pins_socket(), parts.conn_pins_header()
    for row in range(15):
        left_s, right_s = sock[2 * row + 2], sock[2 * row + 1]
        left_h, right_h = hdr[2 * (14 - row) + 1], hdr[2 * (14 - row) + 2]
        checked += 1
        if (left_s, right_s) != (left_h, right_h):
            errors.append("connecteur rangee %d : puissance %s/%s, logique %s/%s" % (row, left_s, right_s, left_h, right_h))
    for net in CROSSING:
        for board, mod in (("puissance", PUI), ("logique", LOG)):
            checked += 1
            if len(mod.nets().get(net, [])) < 2:
                errors.append("%s : le signal %s n'arrive nulle part sur la carte" % (board, net))
    # Constantes de broches du firmware que ce test ne connait pas : a ajouter ci-dessus.
    unknown = [c for c in consts if (c.startswith("ROVER_PIN_") or c.startswith("ROVER_EXP_"))
               and c not in GPIO_NETS and c not in EXP_NETS and consts[c] >= 0
               and not c.endswith("_MASK") and not c.endswith("_OUTPUTS")]
    for c in unknown:
        errors.append("constante firmware non couverte par ce test : %s = %d" % (c, consts[c]))
    print("%d verifications, %d ecart(s)" % (checked, len(errors)))
    for e in errors:
        print("  ECART :", e)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
