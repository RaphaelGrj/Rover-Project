# -*- coding: utf-8 -*-
"""
Éléments communs aux deux cartes ROVER (puissance et logique) : empreintes,
noms de broches, connecteur inter-cartes et petite classe `Board` qui collecte
les composants d'une carte.

Les deux cartes font 75 x 50 mm, se superposent (logique au-dessus) et sont
reliées par un connecteur 2 x 15 broches au pas de 2,54 mm : embase femelle sur
le dessus de la carte puissance, barrette mâle sous la carte logique. Les deux
empreintes sont posées aux mêmes coordonnées X/Y, de sorte que chaque broche
tombe en face de son homologue.
"""

BOARD_W = 75.0
BOARD_H = 50.0
HOLES = [(2.7, 2.7), (72.3, 2.7), (2.7, 47.3), (72.3, 47.3)]   # 4 entretoises M2 communes
# Zone sans cuivre sous l'antenne du module ESP32 (sur les DEUX cartes : la carte
# puissance est 11 mm sous l'antenne, un plan de masse à cet endroit la désaccorderait).
ANTENNA = (0.0, 15.4, 6.7, 34.6)

# --- Empreintes (bibliothèques standard KiCad 10, sauf "rover:") -------------
FP = {
    "R0402": "Resistor_SMD:R_0402_1005Metric",
    "R1206": "Resistor_SMD:R_1206_3216Metric",
    "R2512": "Resistor_SMD:R_2512_6332Metric",
    "C0402": "Capacitor_SMD:C_0402_1005Metric",
    "C0603": "Capacitor_SMD:C_0603_1608Metric",
    "C0805": "Capacitor_SMD:C_0805_2012Metric",
    "CPOL": "Capacitor_SMD:CP_Elec_8x10",
    "LED": "LED_SMD:LED_0603_1608Metric",
    "L4030": "Inductor_SMD:L_Changjiang_FNR4030S",
    "L5040": "Inductor_SMD:L_Changjiang_FNR5040S",
    "SOT23": "Package_TO_SOT_SMD:SOT-23",
    "SOT236": "Package_TO_SOT_SMD:SOT-23-6",
    "SOT238": "Package_TO_SOT_SMD:SOT-23-8",
    "SOIC8": "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
    "TSSOP16": "Package_SO:TSSOP-16_4.4x5mm_P0.65mm",
    "SMA": "Diode_SMD:D_SMA",
    "ESP": "rover:ESP32-S3-WROOM-1_bord",
    "USBC": "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12",
    "RGE24": "Package_DFN_QFN:Texas_RGE0024H_VQFN-24-1EP_4x4mm_P0.5mm_EP2.7x2.7mm",
    "TQFN16": "Package_DFN_QFN:TQFN-16-1EP_3x3mm_P0.5mm_EP1.23x1.23mm",
    "HTSSOP16": "Package_SO:HTSSOP-16-1EP_4.4x5mm_P0.65mm_EP3.4x5mm",
    "MIC": "Sensor_Audio:InvenSense_ICS-43434-6_3.5x2.65mm",
    "LGA14": "Package_LGA:LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y",
    "BME": "Package_LGA:Bosch_LGA-8_3x3mm_P0.8mm_ClockwisePinNumbering",
    "FUSE1812": "Fuse:Fuse_1812_4532Metric",
    "FUSE1206": "Fuse:Fuse_1206_3216Metric",
    "BTN": "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2",
    "SLIDE": "Button_Switch_SMD:SW_SPDT_PCM12",
    "VH3": "Connector_JST:JST_VH_B3PS-VH_1x03_P3.96mm_Horizontal",
    "SOCK30": "Connector_PinSocket_2.54mm:PinSocket_2x15_P2.54mm_Vertical",
    "HDR30": "Connector_PinHeader_2.54mm:PinHeader_2x15_P2.54mm_Vertical",
    "PH2": "Connector_JST:JST_PH_S2B-PH-SM4-TB_1x02-1MP_P2.00mm_Horizontal",
    "PH6": "Connector_JST:JST_PH_S6B-PH-SM4-TB_1x06-1MP_P2.00mm_Horizontal",
    "PH8": "Connector_JST:JST_PH_S8B-PH-SM4-TB_1x08-1MP_P2.00mm_Horizontal",
    "SH4": "Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal",
    "FPC24": "Connector_FFC-FPC:Hirose_FH12-24S-0.5SH_1x24-1MP_P0.50mm_Horizontal",
    "HDR3": "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
    "TP": "TestPoint:TestPoint_Pad_D1.0mm",
    "JP": "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm",
    "HOLE": "MountingHole:MountingHole_2.2mm_M2",
}

# Noms des broches des circuits intégrés (pour dessiner le schéma). Brochages
# relus sur les fiches techniques (BQ25887, MAX98357A, S-8252) ou sur les
# symboles officiels KiCad (les autres).
PIN_NAMES = {
    "ESP": {1: "GND", 2: "3V3", 3: "EN", 4: "IO4", 5: "IO5", 6: "IO6", 7: "IO7", 8: "IO15",
            9: "IO16", 10: "IO17", 11: "IO18", 12: "IO8", 13: "IO19", 14: "IO20", 15: "IO3",
            16: "IO46", 17: "IO9", 18: "IO10", 19: "IO11", 20: "IO12", 21: "IO13", 22: "IO14",
            23: "IO21", 24: "IO47", 25: "IO48", 26: "IO45", 27: "IO0", 28: "IO35", 29: "IO36",
            30: "IO37", 31: "IO38", 32: "IO39", 33: "IO40", 34: "IO41", 35: "IO42", 36: "IO44",
            37: "IO43", 38: "IO2", 39: "IO1", 40: "GND", 41: "GND_EP"},
    "BQ25887": {1: "nPG", 2: "STAT", 3: "CD", 4: "SDA", 5: "SCL", 6: "nINT", 7: "TS", 8: "ILIM",
                9: "MID", 10: "CBSET", 11: "REGN", 12: "BTST", 13: "BAT", 14: "BAT", 15: "SNS",
                16: "SNS", 17: "SW", 18: "SW", 19: "GND", 20: "GND", 21: "PMID", 22: "PMID",
                23: "VBUS", 24: "PSEL", 25: "EP"},
    "S8252": {1: "DO", 2: "CO", 3: "VM", 4: "VC", 5: "VDD", 6: "VSS"},
    "NFET": {1: "G", 2: "S", 3: "D"},
    "PFET8": {1: "S", 2: "S", 3: "S", 4: "G", 5: "D", 6: "D", 7: "D", 8: "D"},
    "TPS563201": {1: "GND", 2: "SW", 3: "VIN", 4: "VFB", 5: "EN", 6: "VBST"},
    "XC6206": {1: "GND", 2: "VOUT", 3: "VIN"},
    "INA219": {1: "IN+", 2: "IN-", 3: "GND", 4: "VS", 5: "SCL", 6: "SDA", 7: "A0", 8: "A1"},
    "TCA9554": {1: "A0", 2: "A1", 3: "A2", 4: "P0", 5: "P1", 6: "P2", 7: "P3", 8: "GND", 9: "P4",
                10: "P5", 11: "P6", 12: "P7", 13: "nINT", 14: "SCL", 15: "SDA", 16: "VCC"},
    "LSM6DS3": {1: "SA0", 2: "SDX", 3: "SCX", 4: "INT1", 5: "VDDIO", 6: "GND", 7: "GND", 8: "VDD",
                9: "INT2", 10: "NC", 11: "NC", 12: "CS", 13: "SCL", 14: "SDA"},
    "DRV8833": {1: "nSLEEP", 2: "AOUT1", 3: "AISEN", 4: "AOUT2", 5: "BOUT2", 6: "BISEN",
                7: "BOUT1", 8: "nFAULT", 9: "BIN1", 10: "BIN2", 11: "VCP", 12: "VM", 13: "GND",
                14: "VINT", 15: "AIN2", 16: "AIN1", 17: "EP"},
    "MAX98357": {1: "DIN", 2: "GAIN", 3: "GND", 4: "SD_MODE", 5: "NC", 6: "NC", 7: "VDD",
                 8: "VDD", 9: "OUTP", 10: "OUTN", 11: "GND", 12: "NC", 13: "NC", 14: "LRCLK",
                 15: "GND", 16: "BCLK", 17: "EP"},
    "ICS43434": {1: "WS", 2: "LR", 3: "GND", 4: "SCK", 5: "VDD", 6: "SD"},
    "BME688": {1: "GND", 2: "CSB", 3: "SDI", 4: "SCK", 5: "SDO", 6: "VDDIO", 7: "GND", 8: "VDD"},
    "USBLC6": {1: "IO1", 2: "GND", 3: "IO2", 4: "IO2", 5: "VBUS", 6: "IO1"},
    "USBC": {"A1": "GND", "A4": "VBUS", "A5": "CC1", "A6": "D+", "A7": "D-", "A8": "SBU1",
             "A9": "VBUS", "A12": "GND", "B1": "GND", "B4": "VBUS", "B5": "CC2", "B6": "D+",
             "B7": "D-", "B8": "SBU2", "B9": "VBUS", "B12": "GND", "SH": "SHIELD"},
    "FPC24": dict((i, n) for i, n in enumerate(
        ["NC", "AGND", "SIO_D", "AVDD", "SIO_C", "RESET", "VSYNC", "PWDN", "HREF", "DVDD", "DOVDD",
         "Y9", "XCLK", "Y8", "DGND", "Y7", "PCLK", "Y6", "Y2", "Y5", "Y3", "Y4", "Y1", "Y0"], 1)),
    "SLIDE": {1: "A", 2: "COM", 3: "B"},
    "BTN": {1: "1", 2: "2"},
}



# --- Connecteur inter-cartes -------------------------------------------------
CONN_XL, CONN_XR = 28.46, 31.0      # colonnes gauche / droite (mm)
CONN_Y0, CONN_PITCH = 7.22, 2.54    # première rangée, pas
# (net colonne gauche, net colonne droite) pour chacune des 15 rangées, de haut en bas.
CONN_ROWS = [
    ("+5V", "+5V"), ("+5V", "+5V"), ("SCL", "GND"), ("SDA", "GND"), ("ENC_R_B", "ENC_R_A"),
    ("MR_IN1", "ML_IN2"), ("ML_IN1", "ENC_L_B"), ("ENC_L_A", "GND"), ("GND", "DRV_FAULT"),
    ("MR_IN2", "GND"), ("ESTOP_N", "GND"), ("USB_DM", "USB_DP"), ("GND", "GND"),
    ("+3V3", "+3V3"), ("+3V3", "+3V3"),
]


def conn_pins_socket():
    """Embase femelle, carte puissance, face dessus, origine (CONN_XR, CONN_Y0) :
    broches impaires à droite, paires à gauche, rangées vers le bas."""
    return {n: CONN_ROWS[(n - 1) // 2][1 if n % 2 else 0] for n in range(1, 31)}


def conn_pins_header():
    """Barrette mâle, carte logique, face dessous (empreinte retournée), origine
    (CONN_XL, CONN_Y0 + 14 pas) : broches impaires à gauche, rangées vers le haut."""
    return {n: CONN_ROWS[14 - (n - 1) // 2][0 if n % 2 else 1] for n in range(1, 31)}


class Board:
    """Collecte les composants d'une carte (voir design_puissance.py / design_logique.py)."""

    def __init__(self):
        self.parts = []

    def part(self, ref, value, fp, x, y, rot, pins, kind=None, side="F", lcsc="", dnp=False):
        """`pins` associe numéro de pastille -> nom de net ; une pastille absente est
        volontairement non connectée. `dnp=True` : empreinte présente, composant non monté."""
        self.parts.append(dict(ref=ref, value=value, fp=FP[fp], kind=kind or fp, x=x, y=y, rot=rot, side=side,
                               pins={str(k): v for k, v in pins.items()}, lcsc=lcsc, dnp=dnp))

    def R(self, ref, value, x, y, rot, n1, n2, fp="R0402", side="F", dnp=False):
        self.part(ref, value, fp, x, y, rot, {1: n1, 2: n2}, kind="R", side=side, dnp=dnp)

    def C(self, ref, value, x, y, rot, n1, n2, fp="C0402", side="F", dnp=False):
        self.part(ref, value, fp, x, y, rot, {1: n1, 2: n2}, kind="C", side=side, dnp=dnp)

    def nets(self):
        out = {}
        for p in self.parts:
            for pad, net in p["pins"].items():
                out.setdefault(net, []).append((p["ref"], pad))
        return out

    def check(self):
        refs = [p["ref"] for p in self.parts]
        dup = sorted(set(r for r in refs if refs.count(r) > 1))
        assert not dup, "references en double : %s" % dup
        return len(self.parts), len(self.nets())
