# -*- coding: utf-8 -*-
"""
Carte PUISSANCE du ROVER (carte du bas, 75 x 50 mm, 4 couches).

Contient tout ce qui touche à l'énergie : connecteur batterie 2S, protection,
fusibles, mesure INA219, interrupteur, chargeur USB-C BQ25887, convertisseurs
5 V et 3,3 V, driver moteur DRV8833 et ses connecteurs. Transcrit
`pcb/PCB_CONNEXIONS.md` §5.1 à §5.3 ; le reste est sur la carte logique.

Repère : origine en haut à gauche vue de dessus, X vers l'arrière du robot,
Y vers le bas, en mm. `side="B"` = face de dessous.
"""
from parts import (Board, BOARD_W, BOARD_H, HOLES, ANTENNA, FP, PIN_NAMES,   # noqa: F401
                   CONN_XR, CONN_Y0, conn_pins_socket)

NAME = "rover_puissance"
TITLE = "ROVER - carte puissance (bas)"
OUTLINE = [(0, 0), (75, 0), (75, 50), (0, 50)]
NOTCHES = []
# Couches internes : masse pleine + plan VBAT_SYS (batterie après l'interrupteur),
# qui alimente driver moteur et convertisseurs sans longues pistes.
PLANES = {"In1": "GND", "In2": "VBAT_SYS"}

B = Board()
part, R, C = B.part, B.R, B.C
PARTS = B.parts
nets = B.nets

GND, V33, V5, VBUS = "GND", "+3V3", "+5V", "VBUS"
VPACK, VSYS, VLOGIC = "VPACK", "VBAT_SYS", "VIN_LOGIC"
SDA, SCL = "SDA", "SCL"

# Largeur des pistes par net (voir freeroute.CLASSES).
NETCLASS = {}
for _n in ("B+", "B-", "BAT_F", "VPACK", "FET_MID"):
    NETCLASS[_n] = "Batterie"
for _n in (VBUS, V5, "BUCK3_OUT", "BUCK5_OUT"):
    NETCLASS[_n] = "Rail"
for _n in (VLOGIC, V33, "SW5", "SW6", "ML_M1", "ML_M2", "MR_M1", "MR_M2", "ML_SENSE", "MR_SENSE"):
    NETCLASS[_n] = "Puissance"
# Le routeur garde la largeur de piste jusqu'à la pastille. Autour du chargeur (broches au
# pas de 0,5 mm) les nets sont donc des tronçons locaux fins, séparés des nets larges par
# des ponts 0 ohm (R60, R61) ; même principe pour les prises de mesure (R62 à R64).
for _n in ("CHG_VBUS", "CHG_BAT", "CHG_PMID", "CHG_SW", "CHG_SNS",
           "MID", "MID_RAW", "CHG_CBSET", "CHG_REGN"):
    NETCLASS[_n] = "Moyen"

# Largeur visée après routage (widen.py élargit chaque segment autant que la place le permet).
WIDEN = {"B+": 1.2, "B-": 1.2, "BAT_F": 1.2, "VPACK": 1.2, "FET_MID": 1.2,
         VBUS: 0.8, "CHG_VBUS": 0.8, "CHG_PMID": 0.8, "CHG_SW": 0.8, "CHG_SNS": 0.6, "CHG_BAT": 0.6,
         V5: 0.8, "BUCK5_OUT": 0.8, "BUCK3_OUT": 0.8, V33: 0.5, VLOGIC: 0.5, "SW5": 0.6, "SW6": 0.6,
         "ML_M1": 0.5, "ML_M2": 0.5, "MR_M1": 0.5, "MR_M2": 0.5, "ML_SENSE": 0.5, "MR_SENSE": 0.5}

# =============================================================================
# 1. Connecteur inter-cartes (embase femelle, vers la carte logique au-dessus)
# =============================================================================
part("J20", "Vers carte logique (embase 2x15)", "SOCK30", CONN_XR, CONN_Y0, 0, conn_pins_socket(), kind="CONN")

# =============================================================================
# 2. USB-C : recharge ; les lignes de données montent vers l'ESP32
# =============================================================================
part("J1", "USB-C TYPE-C-31-M-12", "USBC", 71.6, 25.05, 90, {
    "A1": GND, "A12": GND, "B1": GND, "B12": GND, "SH": GND,
    "A4": VBUS, "A9": VBUS, "B4": VBUS, "B9": VBUS, "A5": "CC1", "B5": "CC2",
    "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DM", "B7": "USB_DM",
}, kind="USBC", lcsc="C165948")
R("R5", "5.1k", 65.3, 26.3, 0, GND, "CC1")
R("R6", "5.1k", 65.3, 23.3, 0, GND, "CC2")
part("U2", "USBLC6-2SC6", "SOT236", 64.0, 19.4, 0,
     {1: "USB_DP", 2: GND, 3: "USB_DM", 4: "USB_DM", 5: VBUS, 6: "USB_DP"}, kind="USBLC6", lcsc="C7519")

# =============================================================================
# 3. Chargeur 2S BQ25887 (5 V USB -> 8,4 V, équilibrage par MID)
# =============================================================================
part("U3", "BQ25887RGER", "RGE24", 56.05, 24.05, 270, {
    2: "CHG_STAT", 3: GND, 4: SDA, 5: SCL,
    7: "CHG_TS", 8: "CHG_ILIM", 9: "CHG_MID", 10: "CHG_CBSET", 11: "CHG_REGN", 12: "CHG_BTST",
    13: "CHG_BAT", 14: "CHG_BAT", 15: "CHG_SNS", 16: "CHG_SNS", 17: "CHG_SW", 18: "CHG_SW",
    19: GND, 20: GND, 21: "CHG_PMID", 22: "CHG_PMID", 23: "CHG_VBUS", 24: GND, 25: GND,
}, kind="BQ25887", lcsc="C2761614")
part("L1", "1uH 4A (FNR4030S1R0)", "L4030", 61.2, 27.6, 270, {1: "CHG_PMID", 2: "CHG_SW"}, kind="L")
C("C4", "1uF 25V", 60.2, 21.5, 0, "CHG_VBUS", GND, "C0603")
R("R61", "0 (pont VBUS)", 62.5, 16.0, 0, "CHG_VBUS", VBUS, "R1206")
R("R60", "0 (pont batterie)", 46.6, 24.6, 0, VPACK, "CHG_BAT", "R1206")
C("C5", "22uF 25V", 60.6, 23.7, 0, "CHG_PMID", GND, "C0805")
C("C7", "22uF 25V", 56.3, 28.5, 90, GND, "CHG_SNS", "C0805")
C("C8", "22uF 25V", 53.5, 30.0, 0, GND, "CHG_SNS", "C0805")
C("C9", "10uF 25V", 50.6, 28.6, 90, GND, "CHG_BAT", "C0805")
C("C10", "4.7uF", 50.6, 24.85, 0, GND, "CHG_REGN", "C0603")
C("C11", "47nF", 51.4, 26.2, 0, "CHG_SW", "CHG_BTST")
R("R7", "620", 51.4, 22.5, 0, GND, "CHG_ILIM")            # plafond matériel d'entrée USB ~2 A
R("R8", "330", 51.4, 23.55, 0, "MID", "CHG_MID")           # protège la puce si le pack est branché à l'envers
R("R9", "100", 47.0, 20.5, 90, "MID", "CHG_CBSET", "R1206")   # courant d'équilibrage ~40 mA
R("R10", "5.23k", 51.4, 21.45, 0, "CHG_REGN", "CHG_TS")    # pont TS de la fiche technique (CTN 103AT)
R("R11", "30.1k", 51.4, 20.4, 0, GND, "CHG_TS")
part("D2", "LED orange (charge)", "LED", 57.0, 17.0, 90, {1: "CHG_STAT", 2: "LED_CHG_A"}, kind="LED")
R("R13", "2.2k", 57.0, 19.65, 90, "LED_CHG_A", V33)
# Sonde de température des cellules (CTN 10k). JP1 fermé (goutte de soudure) met R12
# (10k fixe = 25 °C simulés) à la place de la sonde, pour les essais sans sonde.
part("J13", "CTN 10k cellules", "PH2", 64.9, 5.2, 180, {1: "CHG_TS", 2: GND}, kind="CONN", lcsc="C295747")
part("JP1", "CTN simulee (essais)", "JP", 47.3, 27.4, 0, {1: "CHG_TS", 2: "TS_SIM"}, kind="JP")
R("R12", "10k", 47.3, 29.6, 0, GND, "TS_SIM")

# =============================================================================
# 4. Batterie 2S : connecteur, fusibles, mesure, protection, interrupteur
# =============================================================================
part("J2", "Batterie 2S JST-VH coude (1:B- 2:MID 3:B+)", "VH3", 59.8, 42.0, 90,
     {1: "B-", 2: "MID_RAW", 3: "B+"}, kind="CONN", lcsc="C131336")
part("F1", "PTC 4A 16V (1812)", "FUSE1812", 52.6, 33.3, 0, {1: "BAT_F", 2: "B+"}, kind="F")
part("F2", "PTC 0.75A (1206)", "FUSE1206", 56.7, 33.4, 90, {1: "MID", 2: "MID_RAW"}, kind="F")
R("R19", "1M", 36.8, 36.6, 0, "MID", "B-_S")
R("R64", "0 (pont mesure)", 39.0, 36.6, 0, "B-_S", "B-")                 # fil MID cassé -> la protection coupe
R("RS1", "0.05 1W (shunt)", 41.2, 29.0, 0, VPACK, "BAT_F", "R2512")
part("U16", "INA219AIDCNR", "SOT238", 34.9, 27.0, 0,
     {1: "INA_P", 2: "INA_N", 3: GND, 4: V33, 5: SCL, 6: SDA, 7: GND, 8: GND}, kind="INA219", lcsc="C87469")
R("R62", "10", 38.0, 25.2, 0, "INA_P", "BAT_F")      # prises de mesure du shunt (filtrage)
R("R63", "10", 38.0, 26.3, 0, "INA_N", VPACK)
C("C40", "100nF", 34.9, 24.4, 0, V33, GND)
part("U4", "S-8252AAC-M6T1U", "SOT236", 40.5, 33.5, 0,
     {1: "PROT_DO", 2: "PROT_CO", 3: "PROT_VM", 4: "PROT_VC", 5: "PROT_VDD", 6: "B-_S"},
     kind="S8252", lcsc="C468219")
# MOSFET N dans le fil négatif, deux en parallèle par position (~14 mOhm) :
# Q1/Q4 coupent la décharge, Q2/Q5 la charge, drains communs (FET_MID).
for _r, _x, _g, _s in (("Q1", 55.5, "PROT_DO", "B-"), ("Q4", 51.5, "PROT_DO", "B-"),
                       ("Q2", 47.5, "PROT_CO", GND), ("Q5", 43.5, "PROT_CO", GND)):
    part(_r, "AO3400A", "SOT23", _x, 37.6, 0, {1: _g, 2: _s, 3: "FET_MID"}, kind="NFET", lcsc="C20917")
R("R14", "470", 48.9, 33.3, 90, "B+", "PROT_VDD")
C("C12", "100nF", 44.5, 33.5, 0, "B-_S", "PROT_VDD")
R("R15", "470", 44.5, 34.6, 0, "PROT_VC", "MID")
C("C13", "100nF", 46.6, 33.5, 0, "PROT_VC", "B-_S")
R("R16", "2k", 36.8, 33.5, 0, GND, "PROT_VM")
# Interrupteur général : MOSFET P commandé par l'interrupteur à glissière.
part("Q3", "AO4407A", "SOIC8", 40.5, 21.5, 0,
     {1: VPACK, 2: VPACK, 3: VPACK, 4: "PWR_G", 5: VSYS, 6: VSYS, 7: VSYS, 8: VSYS},
     kind="PFET8", lcsc="C16072")
R("R17", "100k", 38.6, 16.9, 90, "PWR_G", VPACK)
C("C14", "100nF", 40.5, 16.9, 90, "PWR_G", VPACK)
R("R18", "10k", 42.4, 16.9, 90, "PWR_G", "PWR_SW")
part("SW1", "Marche/Arret", "SLIDE", 72.9, 12.0, 90, {2: "PWR_SW", 1: GND}, kind="SLIDE")


# =============================================================================
# 5. Convertisseurs 5 V et 3,3 V (TPS563201)
# =============================================================================
def buck(n, value_fb, ox, oy, vin, raw, rail, ind):
    """Bloc abaisseur TPS563201 (schéma d'application de la fiche technique).
    Un pont 0 ohm en sortie permet de mettre la carte en service rail par rail."""
    u, sw, bst, fb = "U%d" % n, "SW%d" % n, "BST%d" % n, "FB%d" % n
    part(u, "TPS563201DDCR", "SOT236", ox, oy, 0,
         {1: GND, 2: sw, 3: vin, 4: fb, 5: vin, 6: bst}, kind="TPS563201", lcsc="C116592")
    part("L1%d" % n, ind, "L5040", ox - 5.0, oy, 0, {1: raw, 2: sw}, kind="L")
    C("C1%d0" % n, "100nF", ox, oy - 2.3, 0, sw, bst)
    C("C1%d1" % n, "10uF 25V", ox, oy + 2.9, 0, vin, GND, "C0805")
    R("R1%d0" % n, value_fb, ox + 3.2, oy + 0.95, 0, fb, raw)
    R("R1%d1" % n, "10k", ox + 3.2, oy + 2.1, 0, fb, GND)
    C("C1%d2" % n, "22uF 10V", ox - 8.9, oy, 90, raw, GND, "C0805")
    C("C1%d3" % n, "22uF 10V", ox - 11.0, oy, 90, raw, GND, "C0805")
    R("R1%d2" % n, "0 (pont)", ox - 13.3, oy, 90, raw, rail, "R1206")


buck(6, "33.2k", 22.0, 14.0, VLOGIC, "BUCK3_OUT", V33, "2.2uH 3A (FNR5040S2R2)")
buck(5, "54.9k", 22.0, 21.2, VSYS, "BUCK5_OUT", V5, "3.3uH 3A (FNR5040S3R3)")
# "OU" de diodes : le 3,3 V vit sur la batterie OU sur l'USB (flash sans batterie).
part("D4", "SS34", "SMA", 37.0, 12.6, 0, {1: VLOGIC, 2: VSYS}, kind="D", lcsc="C717457")
part("D5", "SS34", "SMA", 44.6, 12.6, 0, {1: VLOGIC, 2: VBUS}, kind="D", lcsc="C717457")
part("C70", "470uF 16V", "CPOL", 38.5, 5.6, 0, {1: V5, 2: GND}, kind="CP")       # réserve 5 V (servo, amplis)

# =============================================================================
# 6. Moteurs : DRV8833, arrêt d'urgence matériel, encodeurs
# =============================================================================
part("U7", "DRV8833PWPR", "HTSSOP16", 17.5, 31.0, 90, {
    1: "ESTOP_N", 2: "ML_M1", 3: "ML_SENSE", 4: "ML_M2", 5: "MR_M2", 6: "MR_SENSE", 7: "MR_M1",
    8: "DRV_FAULT", 9: "MR_IN1", 10: "MR_IN2", 11: "MOT_VCP", 12: VSYS, 13: GND, 14: "MOT_VINT",
    15: "ML_IN2", 16: "ML_IN1", 17: GND,
}, kind="DRV8833", lcsc="C50506")
# Résistances de mesure : le driver limite chaque moteur à ~1 A (200 mV / 0,2 ohm).
R("R23", "0.2 (limite 1A)", 13.5, 38.0, 0, GND, "ML_SENSE", "R1206")
R("R24", "0.2 (limite 1A)", 21.5, 38.0, 0, "MR_SENSE", GND, "R1206")
part("C45", "470uF 16V", "CPOL", 14.5, 5.6, 0, {1: VSYS, 2: GND}, kind="CP")
C("C46", "10uF 25V", 17.5, 26.3, 0, VSYS, GND, "C0805")
C("C47", "2.2uF 6.3V", 12.9, 28.6, 0, GND, "MOT_VINT")
C("C48", "10nF 25V", 22.1, 28.6, 0, "MOT_VCP", VSYS)
# E-stop : le bouton tire nSLEEP à la masse -> ponts en H coupés même firmware planté.
R("R25", "10k", 23.5, 31.5, 0, V33, "ESTOP_N")
C("C49", "100nF", 23.5, 33.0, 0, GND, "ESTOP_N")
R("R26", "10k", 23.5, 34.5, 0, V33, "DRV_FAULT")
part("J11", "Arret d'urgence (1:ESTOP_N 2:GND)", "PH2", 55.5, 5.2, 180, {1: "ESTOP_N", 2: GND},
     kind="CONN", lcsc="C295747")
MOT = "1:M1 2:M2 3:3V3 4:GND 5:A 6:B"
part("J3", "Moteur gauche (%s)" % MOT, "PH6", 14.5, 44.8, 0,
     {1: "ML_M1", 2: "ML_M2", 3: V33, 4: GND, 5: "ENC_L_A", 6: "ENC_L_B"}, kind="CONN", lcsc="C265405")
part("J4", "Moteur droit (%s)" % MOT, "PH6", 43.0, 44.8, 0,
     {1: "MR_M1", 2: "MR_M2", 3: V33, 4: GND, 5: "ENC_R_A", 6: "ENC_R_B"}, kind="CONN", lcsc="C265405")
# Tirages 10k des encodeurs + emplacements 100 pF de réserve (non montés).
R("R30", "10k", 25.6, 36.0, 0, V33, "ENC_L_B")
R("R31", "10k", 25.6, 37.1, 0, V33, "ENC_L_A")
C("C50", "100pF (option)", 25.6, 38.2, 0, GND, "ENC_L_B", dnp=True)
C("C51", "100pF (option)", 25.6, 34.9, 0, GND, "ENC_L_A", dnp=True)
R("R32", "10k", 34.5, 17.0, 0, V33, "ENC_R_B")
R("R33", "10k", 34.5, 18.2, 0, V33, "ENC_R_A")
C("C52", "100pF (option)", 34.5, 19.4, 0, GND, "ENC_R_B", dnp=True)
C("C53", "100pF (option)", 34.5, 20.6, 0, GND, "ENC_R_A", dnp=True)

# =============================================================================
# 7. Points de test (pastilles nues, face de dessous) et trous de fixation M2
# =============================================================================
_TP = [("B-", 55.0, 46.0), ("MID", 50.0, 30.0), (VPACK, 44.0, 26.0), (VSYS, 46.0, 18.0), (VBUS, 62.0, 22.0),
       (V5, 10.0, 21.0), (V33, 10.0, 14.0), (GND, 36.0, 40.5), (GND, 10.0, 30.0), ("ESTOP_N", 24.0, 30.0),
       (SDA, 34.5, 23.0), (SCL, 36.6, 23.0)]
for _i, (_net, _x, _y) in enumerate(_TP):
    part("TP%d" % (_i + 1), _net, "TP", _x, _y, 0, {1: _net}, kind="TP", side="B")
for _i, (_x, _y) in enumerate(HOLES):
    part("H%d" % (_i + 1), "M2", "HOLE", _x, _y, 0, {}, kind="HOLE")

# Sérigraphie : (texte, x, y, angle, hauteur, face)
SILK = [
    ("ROVER PUISSANCE V1", 48.0, 41.0, 0, 1.0, "B"),
    ("MOTEUR G", 14.5, 39.0, 0, 0.8, "F"), ("MOTEUR D", 43.0, 40.6, 0, 0.8, "B"),
    ("STOP", 55.5, 11.2, 0, 0.8, "F"), ("CTN", 64.9, 11.2, 0, 0.8, "F"),
    ("BATTERIE 2S", 66.0, 46.4, 0, 0.8, "F"),
    # repères des 3 broches batterie, côté soudure (la face dessus est sous le connecteur)
    ("B+", 63.0, 34.1, 0, 1.0, "B"), ("MID", 63.4, 38.0, 0, 1.0, "B"), ("B-", 63.0, 42.0, 0, 1.0, "B"),
]

if __name__ == "__main__":
    print(NAME, "%d composants, %d nets" % B.check())
    for name, pads in sorted(nets().items()):
        if len(pads) < 2:
            print("NET A UNE SEULE BROCHE :", name, pads)
