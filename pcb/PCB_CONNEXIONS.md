# PCB Rover --- liste complète des connexions (carte S3 tout-en-un)

> **Statut : plan de conception, rien n'est encore dessiné ni commandé
> (2026-10-13).** Ce document sert de cahier des charges pour le
> schéma KiCad. Il part du câblage réel validé sur le WROOM
> (`esp32/WIRING.md`, `AIDE_CABLAGE.md`) et le transpose sur un
> **ESP32-S3** qui absorbe aussi l'ancien ESP32-CAM.
>
> Guide pas-à-pas KiCad pour débutant : `pcb/KICAD_GUIDE.md`.
>
> Légende : ⚠ = point à vérifier dans la datasheet/sur la pièce réelle
> avant de router ; 🔧 = choix de conception à valider avec l'utilisateur.

------------------------------------------------------------------------

## 1. Objectif et ce qui change par rapport au robot actuel

Sur le PCB, il ne reste à brancher **que des câbles** :

| À brancher sur la carte | Connecteur |
|---|---|
| Batterie : 2 cellules 18650 en série (B−, point milieu, B+) | **JST-VH 3 broches** (un seul connecteur, détrompé, ~7 A) |
| Sonde de température des cellules (CTN 10 kΩ) | JST-PH 2 broches |
| Moteur gauche, moteur droit (N20 + encodeur, 6 fils) | 2 × JST-PH 6 broches |
| Haut-parleur gauche, haut-parleur droit | 2 × JST-PH 2 broches |
| Nappe caméra (OV2640) | FPC 24 broches pas 0,5 mm |
| Écran ST7789 (dans la tête) | JST-PH 8 broches |
| Servo de tête | barrette 3 broches 2,54 mm |
| Capteurs de distance VL53L0X × 2 (façade) | 2 × JST-PH 6 broches |
| Bouton d'arrêt d'urgence | JST-PH 2 broches |
| USB-C (flash, console **et recharge de la batterie**) | USB-C 16 broches |
| Extension I2C future (option) | JST-SH 4 broches (format Qwiic/STEMMA QT) |

**Intégrés sur la carte** (plus de modules sur fils) : ESP32-S3, driver
moteur DRV8833, 2 amplis MAX98357A (stéréo), micro I2S, IMU,
capteur température/humidité/pression/gaz BME688, **BMS 2S complet**
(chargeur USB-C avec équilibrage BQ25887 + protection S-8252),
mesure batterie INA219, interrupteur marche/arrêt, expandeur d'E/S
TCA9554, alimentations 5 V et 3,3 V, régulateurs caméra 2,8 V et 1,2 V.

**Ce qui disparaît** :
- l'ESP32-CAM (le S3 pilote directement la caméra) ;
- le buzzer : les haut-parleurs le remplacent (un bip se joue en I2S) ;
- le problème « micro sur GPIO3 = plus de console USB » : le S3 a un
  **USB natif** (GPIO19/20) séparé de tout le reste, donc plus besoin de
  cavalier sur la piste du micro.

------------------------------------------------------------------------

## 2. Choix des composants principaux

| Bloc | Composant proposé | Pourquoi |
|---|---|---|
| MCU | **ESP32-S3-WROOM-1-N16R2** (16 Mo flash, 2 Mo PSRAM *quad*) | 🔧 **Pas la N16R8.** Sur les variantes à PSRAM *octale* (R8), GPIO35/36/37 sont pris par la PSRAM. Avec la caméra (14 broches), il manque alors des GPIO. La R2 garde ces 3 broches libres, et 2 Mo de PSRAM suffisent pour les tampons d'image JPEG. |
| Caméra | OV2640 avec nappe 24 broches (celle de l'ESP32-CAM est réutilisable) | Même capteur qu'aujourd'hui, mapping de broches identique à la carte de référence Espressif **ESP32-S3-EYE** (déjà connue de la bibliothèque `esp32-camera`) |
| Driver moteur | **DRV8833** (HTSSOP-16 avec pad thermique) | Celui du robot actuel : même firmware (PWM direct sur IN1-4) |
| Ampli | **2 × MAX98357A** (un par canal) | Celui déjà prévu ; deux puces = stéréo, un haut-parleur par côté |
| Micro | **ICS-43434** (MEMS I2S, port en dessous) | Successeur direct de l'INMP441 (même protocole I2S, l'INMP441 n'est plus fabriqué) |
| IMU | 🔧 **LSM6DS3TR-C** (I2C, **0x6B**) | Le MPU-6050 est en fin de vie (beaucoup de contrefaçons). Le LSM6DS3TR-C a une bibliothèque Adafruit (`Adafruit_LSM6DS`) avec la même interface `getEvent()` : seul `ImuSensor.cpp` change. Garder un MPU-6050 reste possible si on veut zéro changement de code. |
| Environnement | **BME688** (I2C, 0x76) | Température + humidité + pression + **gaz** en une puce. Déjà détecté automatiquement par le firmware (`EnvironmentSensor.cpp`). |
| Mesure batterie | **INA219** + shunt 0,05 Ω (I2C, 0x40) | Pas de GPIO ADC disponible. En plus de la tension, il donne le **courant** : on voit un moteur bloqué, ce qui aurait aidé pendant la panne moteur de septembre. |
| Batterie | 🔧 **2 × Li-ion 18650 de marque** (Samsung 35E ou LG MJ1 3500 mAh ; Samsung 30Q ou Molicel P28A si on veut plus de marge en courant), chacune dans **son propre support** de qualité (ex. Keystone 1042) | Choix le plus sûr pour un robot qui roule et cogne : boîtier acier rigide, chimie et qualité connues, cellules amovibles, pas de pack à souder. Un LiPo souple se perce ou gonfle au premier choc. Deux supports séparés donnent naturellement accès au point milieu. ~25 Wh, soit plusieurs heures d'autonomie. Rester sur le 4,2 V/cellule classique. Le LiFePO4 serait encore plus sûr, mais les seuils du chargeur et de la protection ne sont pas prévus pour. |
| Chargeur 2S | **BQ25887** (TI, VQFN-24 4×4 mm, I2C) | Charge un pack 2S **depuis le 5 V de l'USB** (montée de tension interne jusqu'à 8,4 V), jusqu'à 2 A, **équilibrage des cellules intégré**. Son ADC donne la tension **de chaque cellule** en I2C. Seul chargeur USB-5 V → 2S courant qui équilibre sans puce supplémentaire. |
| Protection 2S | **S-8252** (ABLIC, SOT-23-6) + double N-MOSFET | Coupe la batterie en cas de surcharge, de décharge profonde (cellule < 2,4 V avec la variante **AAC** retenue), de surintensité ou de court-circuit, **cellule par cellule**. Les cellules 18650 « nues » n'ont aucune protection : sans elle, un robot oublié allumé les détruit (et une cellule trop déchargée devient dangereuse à recharger). Alternative : HY2120. |
| Interrupteur | Petit interrupteur à glissière + P-MOSFET | L'interrupteur ne commande que la grille : les 3 A passent dans le MOSFET, pas dans l'interrupteur. Robot éteint = la batterie peut quand même se recharger. |
| Expandeur | **TCA9554** (8 E/S I2C, 0x20) | Prend les signaux lents (XSHUT, mute ampli, reset écran…) pour libérer des GPIO |
| 5 V | Buck synchrone 3 A (ex. TPS563201, ou équivalent 4,5-17 V / 3 A) | Servo + 2 amplis |
| 3,3 V | Buck synchrone 2-3 A (même puce que le 5 V, autres résistances de retour) | ESP32-S3 (pics WiFi ~500 mA) + caméra + capteurs ; un régulateur linéaire 5→3,3 V chaufferait trop |
| Caméra 2,8 V / 1,2 V | 2 petits LDO (ex. XC6206P282 / XC6206P122) | Comme sur l'ESP32-CAM |

⚠ Les références de régulateurs sont des **exemples**. Avant de les
placer, vérifier qu'ils sont en stock chez l'assembleur (JLCPCB/LCSC),
puis recopier **exactement** le schéma d'application de leur datasheet
(inductance, condensateurs, résistances de retour).

------------------------------------------------------------------------

## 3. Schéma bloc

```text
  2 × 18650 en série ── JST-VH 3 broches (B−, MID, B+)  + CTN collée aux cellules
        │
  B− ─[Protection S-8252 + 2 N-MOSFET]─ GND        (coupe le négatif du pack)
  B+ ─[Fusible PTC]─[Shunt 0,05 Ω INA219]─ VPACK
                                            │
              ┌─────────────────────────────┤
              ▼                             ▼
  USB-C VBUS → BQ25887 (charge + équilibrage via MID)   [Interrupteur → P-MOSFET]
                                                            │
                                                            ▼ VBAT_SYS
     ┌──────────────────────┬─────────────────────────┬─────┘
     ▼                      ▼                         ▼
  DRV8833 VM          Buck 5 V / 3 A          Diode ─┐
  (moteurs)           → servo, 2 amplis              ├→ Buck 3,3 V → ESP32-S3, capteurs,
                                    USB-C VBUS → Diode┘    │          expandeur, micro, chargeur (I2C)
                                                           ├→ LDO 2,8 V → caméra AVDD
                                                           └→ LDO 1,2 V → caméra DVDD

  ESP32-S3 ── DVP 8 bits + SCCB ──────────── FPC caméra
           ── PWM ×4 ─────────── DRV8833 ── JST moteur G / D (+ encodeurs → ESP32)
           ── I2S (BCLK, WS) ─┬─ MAX98357A G ── JST HP gauche
                     DOUT ────┼─ MAX98357A D ── JST HP droit
                     DIN  ────┴─ ICS-43434 (micro)
           ── SPI (SCLK, MOSI, DC) ──────── JST écran
           ── I2C capteurs ── LSM6DS3, BME688, INA219, BQ25887, TCA9554, JST ToF ×2, Qwiic
           ── PWM servo ─────────────────── servo tête
           ── USB natif (D+/D−) ──────────── USB-C
```

L'USB-C alimente **deux** choses : le chargeur (recharge du pack, même
interrupteur sur arrêt) et, par une diode, le buck 3,3 V. On peut donc
**flasher la carte sans batterie**, et l'ESP32 reste allumé pendant une
charge, ce qui lui permet de surveiller la charge. Dans ce cas le 5 V n'existe pas : servo et son
restent éteints, ce qui est voulu. Le 5 V est fabriqué uniquement depuis
VBAT, ainsi l'USB ne peut jamais remonter dans la batterie ni dans le
buck 5 V.

------------------------------------------------------------------------

## 4. Affectation des GPIO de l'ESP32-S3

Broches **interdites** :
- GPIO26 à 32 : flash/PSRAM interne, non sorties sur le module.
- GPIO19/20 : USB natif (D−/D+).
- GPIO33/34 : n'existent pas sur le module WROOM-1.

**Les 36 broches du module sont toutes attribuées** (voir §4.1 pour la
marge restante via l'expandeur).

| GPIO | Fonction | Remarque |
|---|---|---|
| 0 | Bouton **BOOT** | Strapping : pull-up 10 kΩ + bouton vers GND. Rien d'autre dessus. |
| 1 | I2C capteurs **SDA** | Pull-up 4,7 kΩ vers 3V3 |
| 2 | I2C capteurs **SCL** | Pull-up 4,7 kΩ vers 3V3 |
| 3 | **Arrêt d'urgence** (lecture) | Relié au net `ESTOP_N` = nSLEEP du DRV8833, voir §5.3. Strapping JTAG, ignoré tant que l'eFuse correspondant n'est pas gravé (cas par défaut). |
| 4 | Caméra **SIOD** (SCCB data) | Bus séparé du bus capteurs, pull-up 4,7 kΩ |
| 5 | Caméra **SIOC** (SCCB clock) | Pull-up 4,7 kΩ |
| 6 | Caméra **VSYNC** | |
| 7 | Caméra **HREF** | |
| 8 | Caméra **Y4** (D2) | |
| 9 | Caméra **Y3** (D1) | |
| 10 | Caméra **Y5** (D3) | |
| 11 | Caméra **Y2** (D0) | |
| 12 | Caméra **Y6** (D4) | |
| 13 | Caméra **PCLK** | |
| 14 | **Encodeur gauche A** | Pull-up 10 kΩ vers 3V3 sur la carte |
| 15 | Caméra **XCLK** | Horloge 20 MHz : piste courte |
| 16 | Caméra **Y9** (D7) | |
| 17 | Caméra **Y8** (D6) | |
| 18 | Caméra **Y7** (D5) | |
| 19 | **USB D−** | Vers USB-C via protection ESD |
| 20 | **USB D+** | idem |
| 21 | **Encodeur gauche B** | Pull-up 10 kΩ |
| 35 | **Encodeur droit A** | Pull-up 10 kΩ. ⚠ Libre **uniquement** sur une variante R2 (pas R8). |
| 36 | **Encodeur droit B** | idem |
| 37 | **Servo de tête** (signal) | idem |
| 38 | I2S **BCLK** | Commun aux 2 amplis + micro |
| 39 | I2S **WS / LRC** | Commun aux 2 amplis + micro |
| 40 | I2S **DOUT** → amplis | Même fil pour les 2 amplis (chacun choisit son canal, §5.4) |
| 41 | I2S **DIN** ← micro | |
| 42 | Écran **SCLK** | |
| 43 | Écran **MOSI** | Ex-TXD0 : bavarde au démarrage (log ROM), sans effet car SCLK reste immobile |
| 44 | Écran **DC** | Ex-RXD0 |
| 45 | **Moteur droit IN1** (DRV8833 BIN1) | Strapping : doit être **bas au boot**. Les pull-down internes du DRV8833 et du S3 le garantissent, donc moteur à l'arrêt pendant le boot. Ne **jamais** y mettre de pull-up. |
| 46 | **Moteur droit IN2** (BIN2) | Strapping : bas au boot, même raisonnement |
| 47 | **Moteur gauche IN1** (AIN1) | |
| 48 | **Moteur gauche IN2** (AIN2) | |

Le mapping caméra (GPIO4-13, 15-18) est **exactement** celui de la
carte Espressif ESP32-S3-EYE (`CAMERA_MODEL_ESP32S3_EYE` dans les
exemples Arduino). Le firmware caméra fonctionnera donc sans deviner.

### 4.1 Expandeur TCA9554 (adresse 0x20) --- signaux lents

À la mise sous tension, toutes les broches du TCA9554 sont des entrées
(haute impédance). Chaque sortie a donc une résistance qui fixe un état
sûr **avant** que le firmware prenne la main.

| Broche | Fonction | Sens | Résistance de repos | État sûr au boot |
|---|---|---|---|---|
| P0 | XSHUT VL53L0X **droit** | sortie | (pull-up déjà sur le module ToF) | capteur allumé |
| P1 | **AMP_EN** (SD_MODE des 2 amplis, §5.4) | sortie | pull-down interne de l'ampli | **amplis coupés** (pas de souffle au boot) |
| P2 | **CAM_PWDN** | sortie | 10 kΩ vers GND | caméra allumée |
| P3 | **Écran RST** | sortie | 10 kΩ vers 3V3 | écran pas bloqué en reset |
| P4 | **Écran BLK** (rétroéclairage on/off) | sortie | 10 kΩ vers 3V3 | rétroéclairage allumé |
| P5 | **DRV8833 nFAULT** | entrée | 10 kΩ vers 3V3 (nFAULT est à drain ouvert) | --- |
| P6 | **LED d'état** (+ 1 kΩ) | sortie | --- | éteinte |
| P7 | **Libre** | --- | --- | sortie sur une pastille de test pour un usage futur |

A0/A1/A2 du TCA9554 → GND (adresse 0x20). Sa broche INT n'est pas
reliée (plus de GPIO) : le firmware lit nFAULT en scrutation, ce qui
suffit pour un signal de défaut.

------------------------------------------------------------------------

## 5. Connexions détaillées par bloc

### 5.1 Alimentation et BMS 2S (batterie, charge USB-C, protection)

**Principe** : deux cellules Li-ion 18650, chacune dans son support,
sont reliées en série par le câblage et arrivent sur la carte par **un
seul connecteur 3 broches** : `B−`, point milieu, `B+`. La carte les
protège, les recharge par l'USB-C en équilibrant les deux cellules, et
mesure tension et courant.

Le point milieu est indispensable : sans lui, la carte ne pourrait ni
équilibrer ni protéger chaque cellule séparément. Il arrive dans le
même connecteur que la puissance, il ne peut donc pas être oublié. Si
son fil casse, une résistance de 1 MΩ le tire vers `B−` : la
protection voit alors une cellule « vide » et **coupe**. Sécurité par
défaut, voulue.

#### Nets d'alimentation

| Net | Ce que c'est |
|---|---|
| `B+` / `B−` | Bornes brutes du pack |
| `MID` | Point milieu (entre les deux cellules), broche centrale du connecteur |
| `GND` | Masse de toute la carte = `B−` **après** les MOSFET de protection |
| `VPACK` | `B+` après fusible et shunt : là où se rejoignent chargeur et robot |
| `VBAT_SYS` | `VPACK` après l'interrupteur : alimente tout le robot |
| `VIN_LOGIC` | Entrée du buck 3,3 V (OU de diodes entre `VBAT_SYS` et l'USB) |
| `5V`, `3V3`, `CAM_2V8`, `CAM_1V2` | Rails régulés |

#### Batterie et connecteur

**Câblage côté batterie** (hors PCB, dans le châssis) :

```text
  Support cellule 1 (bas)        Support cellule 2 (haut)
   (−) ────────────── fil noir 18 AWG ─────────────── broche 1 : B−
   (+) ──┬─────────── fil blanc 22 AWG ────────────── broche 2 : MID
         └── (−)
                         (+) ──── fil rouge 18 AWG ── broche 3 : B+
```

| Connecteur | Broche | Net | Remarque |
|---|---|---|---|
| **Batterie : JST-VH 3 broches** (pas 3,96 mm, version coudée ou droite à souder sur PCB) | 1 (noir) | `B−` | **Jamais relié directement à `GND`**, voir protection. Pistes ≥ 2 mm. |
| | 2 (blanc) | `MID`, via un **PTC 0,75 A** (1206) + **1 MΩ vers `B−`** | Porte seulement le courant d'équilibrage (≤ 400 mA). Le PTC protège ce fil fin si quelque chose tourne mal côté carte. |
| | 3 (rouge) | `B+` | Pistes ≥ 2 mm |
| **Sonde de température : JST-PH 2 broches** | 1, 2 | CTN 10 kΩ → `TS` du chargeur | CTN « à perle » collée au ruban Kapton entre les deux cellules. ⚠ Prendre le coefficient B (3435 ou 3950) indiqué par la datasheet BQ25887 pour calculer le pont `TS`. |

Pourquoi ce connecteur : le JST-VH supporte ~7 A par contact (pic du
robot ~2,8 A), il est détrompé, se verrouille, et existe en version à
souder sur PCB. Un JST-PH (2 A) serait trop faible, et deux
connecteurs (puissance + équilibrage, comme sur un LiPo) laisseraient
oublier le second.

Côté mécanique :
- **Supports séparés de qualité** (contacts à ressort épais, type
  Keystone 1042). Les supports bas de gamme ont des ressorts fins qui
  chauffent à 3 A et perdent le contact dans les vibrations.
- **Maintenir chaque cellule** (collier ou capot imprimé) : un choc ne
  doit pas pouvoir la faire sortir de son support.
- Fils **courts**, torsadés, loin des arêtes vives et des pièces
  mobiles. Gaine ou capot ignifuge autour des cellules (`BOM.md`).
- Repérer la polarité **+/−** en gros sur la pièce imprimée autour de
  chaque support.

#### Choix des cellules

| Cellule | Capacité | Courant max | Pour le Rover |
|---|---|---|---|
| **Samsung INR18650-35E** | 3500 mAh | 8 A | ✅ recommandé : meilleure autonomie, 8 A suffit largement (pic ~2,8 A) |
| LG INR18650-MJ1 | 3500 mAh | 10 A | ✅ équivalent |
| Samsung INR18650-30Q / Molicel P28A | 3000 / 2800 mAh | 15 / 35 A | ✅ plus de marge en courant, un peu moins d'autonomie |

Autonomie : ~25 Wh, soit environ **3 à 5 h** d'usage mixte (à mesurer
avec l'INA219). Charge à **1 A** (~0,3C, doux pour les cellules) :
~4 h pour une charge complète. Le chargeur tire alors ~2 A sur l'USB-C.

#### Protection 2S (S-8252 + double N-MOSFET, côté négatif)

Recopier le schéma d'application de la datasheet ABLIC S-8252. Le
principe :

| Broche S-8252 | Connexion |
|---|---|
| `VDD` | `B+` via ~470 Ω, + 100 nF vers `B−` (filtre RC de la datasheet) |
| `VC` | `MID` via ~470 Ω, + 100 nF vers `B−` |
| `VSS` | `B−` |
| `DO` | Grille du MOSFET « décharge » (source sur `B−`) |
| `CO` | Grille du MOSFET « charge » (source sur `GND`) |
| `VM` | `GND` via ~2 kΩ (détection surintensité / chargeur) |

Les deux MOSFET sont en série, **drains reliés** : `B−` → MOSFET
décharge → MOSFET charge → `GND`. Choisir un double N-MOSFET ≥ 8 A et
R<sub>DS(on)</sub> ≤ 15 mΩ (logique 2,5 V). Le seuil de surintensité vaut
« tension de détection du S-8252 / (2 × R<sub>DS(on)</sub>) ». Le régler
vers **6-8 A** : au-dessus des pics normaux, en dessous d'un
court-circuit.

**Variante retenue : S-8252AAC-M6T1U (LCSC C468219)**, choisie le
2026-10-13 parmi celles que LCSC vend. Seuils lus dans le tableau de la
datasheet ABLIC (extraits indexés, ⚠ à reconfirmer sur le PDF à la
réception) :

| Seuil | Valeur | Commentaire |
|---|---|---|
| Surcharge (coupure / réarmement) | **4,30 V** / 4,10 V | Au-dessus des 4,20 V du chargeur : il finit sa charge sans être coupé |
| Décharge profonde (coupure / réarmement) | **2,40 V** / 3,00 V | ⚠ Sous la fin de décharge des cellules (35E : 2,65 V, MJ1 : 2,5 V) |
| Surintensité en décharge | 0,20 V aux bornes des MOSFET | Voir le calcul ci-dessus |

Pourquoi l'accepter : la variante idéale (**AAH**, 4,25 V / 3,0 V)
n'est pas chez LCSC. AAA et ABB coupent à 2,0 V (pire), et AAJ est
réglée pour du LiFePO4 (3,9 V, elle couperait des Li-ion avant la fin de
la charge). Les 2,4 V ne sont **jamais atteints en usage normal**, car le
firmware s'arrête bien avant :
- 3,1 V/cellule → `cell_critical` : moteurs coupés, robot en `SAFE` ;
- 3,0 V/cellule → **sommeil profond** de l'ESP32
  (`EVENT name=battery_shutdown`). La carte ne consomme alors presque
  plus rien, et le robot se réveille tout seul pour vérifier, ou dès
  qu'on branche l'USB.

La coupure à 2,4 V ne sert donc qu'en dernier recours (firmware planté
ou absent). Alternative si on veut mieux : HY2120 (autre fabricant,
même principe), à rechercher avec des seuils ~4,25 V / ~2,8 V.

#### Chargeur BQ25887 (USB-C 5 V → charge 2S, équilibrage intégré)

Recopier le schéma d'application de la datasheet TI (inductance,
condensateurs `VBUS`/`PMID`/`BAT`, `BTST`, `REGN`). Connexions propres
au Rover :

| Broche BQ25887 | Connexion |
|---|---|
| `VBUS` | USB-C `VBUS` (direct, pas de diode) + condensateurs de la datasheet |
| `BAT` | `VPACK` (côté robot du shunt) |
| `MID` | `MID` (via le PTC 0,75 A, voir connecteur batterie) |
| `SDA` / `SCL` | Bus I2C capteurs (GPIO1/GPIO2) |
| `STAT` | LED orange « en charge » + 2,2 kΩ vers `3V3` |
| `INT` | Non relié (le firmware scrute le chargeur en I2C) |
| `CD` (arrêt de charge) | `GND` via 0 Ω (charge autorisée ; le firmware peut aussi la couper par I2C) |
| `ILIM` | Résistance vers `GND` qui fixe un **plafond matériel** du courant d'entrée USB, actif même firmware planté (fonction activée par défaut dans la puce, bit `EN_ILIM`). Viser **~2 A**, valeur selon la formule de la datasheet (§ « ILIM »). Le firmware règle en plus la limite logicielle à 2 A (`ROVER_USB_INPUT_LIMIT_MA`) : c'est la plus basse des deux qui s'applique. |
| `TS` (thermistance) | **CTN 10 kΩ collée entre les deux cellules** (connecteur JST-PH 2), dans le pont de résistances de la datasheet. Le chargeur suspend la charge si les cellules sont trop chaudes ou trop froides. Un pont de soudure permet de mettre à la place une résistance fixe qui simule 25 °C, pour les essais sans sonde. |
| `D+` / `D−` (ou `PSEL`) | Ponts 0 Ω **non montés** vers les lignes USB. Le firmware désactive la détection automatique de source (`AUTO_INDET_EN`) et règle lui-même la limite de courant USB en I2C. ⚠ Les registres de la puce parlent d'une broche `PSEL` (choix de la source) : vérifier sur le brochage de la datasheet si le boîtier a `D+`/`D−`, `PSEL`, ou les deux. Si `PSEL` existe, la relier à `GND` (« adaptateur ») via 0 Ω. |
| `GND` + pad | `GND` (beaucoup de vias) |

⚠ Les noms de broches ci-dessus viennent du schéma simplifié de TI.
Les vérifier sur le brochage complet de la datasheet (VQFN-24).

Points de fonctionnement :
- Le BQ25887 **monte** le 5 V USB jusqu'à 8,4 V : pour 1 A de charge,
  il tire ~2 A sur l'USB. Régler le courant de charge à **≤ 1C** du pack
  (1 A pour un 1000-1500 mAh) et la limite d'entrée à 1,5-2 A.
  Branché sur un port USB faible, sa régulation de tension d'entrée
  (VINDPM) réduit le courant d'elle-même.
- **Pas de « power path »** : le robot et le chargeur sont tous les
  deux sur le pack. Rouler pendant la charge fonctionne, mais fausse la
  détection de fin de charge. Recharger robot à l'arrêt (interrupteur
  sur OFF).
- Le chargeur se branche **avant** l'interrupteur : on recharge robot
  éteint.

#### Fusible, mesure, interrupteur, régulateurs

| De | Vers | Remarque |
|---|---|---|
| `B+` | Fusible PTC (maintien ~4-5 A, 1812 ou 2920) | Dernier rempart indépendant de la protection électronique |
| PTC | Shunt 0,05 Ω (2512, 1 W) → `VPACK` | INA219 `IN+` côté amont, `IN−` côté `VPACK`. Le shunt voit **tout** le courant du pack : positif en décharge, **négatif en charge** (l'INA219 mesure dans les deux sens). |
| `VPACK` | **Interrupteur** : P-MOSFET (source sur `VPACK`, drain sur `VBAT_SYS`) | P-MOSFET ≥ 6 A, V<sub>GS</sub> max ±20 V, R<sub>DS(on)</sub> faible. Grille → `VPACK` par 100 kΩ (bloqué par défaut). Petit interrupteur à glissière entre la grille et `GND` (via 10 kΩ) : ON = grille à la masse = MOSFET passant. L'interrupteur ne voit aucun courant de puissance. |
| `VBAT_SYS` | DRV8833 `VM` | + **470 µF / 16 V** électrolytique **et** 10 µF céramique au plus près du DRV8833 |
| `VBAT_SYS` | Entrée buck 5 V | Condensateurs selon datasheet |
| `VBAT_SYS` → diode Schottky (SS34) | `VIN_LOGIC` | |
| USB-C `VBUS` → diode Schottky (SS34) | `VIN_LOGIC` | « OU » de diodes : la plus haute tension alimente |
| `VIN_LOGIC` | Buck 3,3 V → `3V3` | ESP32-S3, capteurs, TCA9554, micro, chargeur (I2C), LDO caméra, pull-ups |
| Sortie buck 5 V | Rail `5V` | Servo, 2 × MAX98357A. + 470 µF près du connecteur servo |
| `3V3` | LDO 2,8 V → `CAM_2V8` | AVDD caméra |
| `3V3` | LDO 1,2 V → `CAM_1V2` | DVDD caméra |
| `3V3` | LED verte + 1 kΩ → GND | Témoin « carte alimentée » |

Comportement selon ce qui est branché :

| Situation | Ce qui marche |
|---|---|
| Pack branché, interrupteur ON | Tout |
| Pack branché, interrupteur OFF | Rien (consommation ≈ protection + INA219 + chargeur en veille, quelques dizaines de µA ⚠ à vérifier) |
| USB seul, sans pack | ESP32 + capteurs (flash, console). Pas de moteurs, servo ni son (pas de 5 V). |
| USB + pack, interrupteur OFF | **Recharge** + ESP32 allumé (il peut afficher l'état de charge) |

🔧 Pour le premier prototype, prévoir un **pont 0 Ω (ou un cavalier de
soudure)** en sortie de chaque buck. On démarre la carte buck par buck
sans risquer le reste (voir `KICAD_GUIDE.md` §9).

🔥 **Sécurité Li-ion** : ne pas laisser charger sans surveillance les
premières fois. Acheter les cellules chez un revendeur spécialisé
reconnu : les fausses « 9900 mAh » sont la première cause d'incident
(`BOM.md`). Ne jamais utiliser une cellule dont la gaine plastique est
déchirée (le boîtier métallique est le pôle −), ni une cellule cabossée.
Une cellule restée sous ~2,5 V se recycle, elle ne se recharge pas.
Toujours remplacer les deux cellules ensemble (même modèle, même
âge).

### 5.2 ESP32-S3-WROOM-1 --- éléments autour du module

| Broche module | Connexion |
|---|---|
| `3V3` | Rail `3V3` + 10 µF + 100 nF au plus près |
| `GND` (+ pad central EPAD) | GND, pad central avec vias vers le plan de masse |
| `EN` | 10 kΩ vers 3V3 + **1 µF** vers GND (délai de démarrage recommandé par Espressif) + bouton **RESET** vers GND |
| `IO0` | 10 kΩ vers 3V3 + bouton **BOOT** vers GND |
| `IO19` / `IO20` | USB D− / D+ (paire différentielle 90 Ω), via puce ESD **USBLC6-2SC6** |

**USB-C** (connecteur 16 broches « USB 2.0 only ») :

| Broche USB-C | Connexion |
|---|---|
| `VBUS` | Diode Schottky → `VIN_LOGIC` (§5.1) |
| `CC1`, `CC2` | **5,1 kΩ chacune vers GND** (indispensable sinon un chargeur USB-C ne fournit pas le 5 V) |
| `D+` (A6 et B6 reliés) | USBLC6 → GPIO20 |
| `D−` (A7 et B7 reliés) | USBLC6 → GPIO19 |
| `GND`, blindage | GND (blindage via 1 MΩ ∥ 4,7 nF, ou directement à GND) |

⚠ **Antenne** : placer le module **en bord de carte**, antenne vers
l'extérieur. Aucun cuivre, aucune piste et aucun plan de masse sous la
zone d'antenne, sur toutes les couches. Ne pas l'enfermer dans du métal.

### 5.3 Driver moteur DRV8833 + arrêt d'urgence

| Broche DRV8833 | Connexion |
|---|---|
| `VM` | `VBAT_SYS` (+ 470 µF + 10 µF, §5.1) |
| `VINT` | 2,2 µF vers GND (régulateur interne, rien d'autre dessus) |
| `VCP` | 10 nF (16 V) vers `VM` (pompe de charge, ⚠ valeur à confirmer dans la datasheet) |
| `AIN1` / `AIN2` | GPIO47 / GPIO48 (moteur gauche) |
| `BIN1` / `BIN2` | GPIO45 / GPIO46 (moteur droit) |
| `AOUT1` / `AOUT2` | Connecteur moteur gauche `M1` / `M2` |
| `BOUT1` / `BOUT2` | Connecteur moteur droit `M1` / `M2` |
| `AISEN`, `BISEN` | Résistance de mesure vers GND, **0,2 Ω** (1206/2010). Le driver limite alors chaque moteur à ~1 A (200 mV / R) : un moteur bloqué ne grille plus. 🔧 Remplaçable par un pont 0 Ω si les moteurs manquent de couple. |
| `nSLEEP` | Net **`ESTOP_N`** : 10 kΩ vers 3V3 + bouton E-stop vers GND + GPIO3 (lecture) + 100 nF vers GND (antirebond) |
| `nFAULT` | 10 kΩ vers 3V3 → TCA9554 P5 |
| `GND` + pad thermique | GND, avec **beaucoup de vias** sous le pad (c'est son radiateur) |

**Pourquoi l'E-stop agit directement sur nSLEEP** : appuyer sur le
bouton met nSLEEP à la masse, ce qui coupe les ponts en H **dans le
matériel**, même si le firmware est planté. GPIO3 lit le même fil, donc
le firmware voit l'appui, comme aujourd'hui (`EStop.h` : entrée avec
pull-up, appuyé = LOW). Sans bouton branché, le pull-up garde les moteurs
actifs (comportement actuel).

🔧 Un bouton **NO** (comme prévu dans `BOM.md`) garde le comportement
actuel. Un bouton **NC** serait plus sûr, car un fil arraché arrêterait
le robot. Mais il inverse la logique : à décider avant de router.

**Connecteur moteur** (×2, **JST-PH 2,0 mm, 6 broches**). Ordre des
broches = ordre du tableau « Câblage moteur → encodeur » de
`esp32/WIRING.md` (marquages relevés sur le PCB encodeur le 2026-09-02).
L'utilisateur adaptera le câble si le connecteur réel diffère :

| Broche | Fil | Marquage encodeur | Moteur gauche | Moteur droit |
|---|---|---|---|---|
| 1 | Blanc | `M1` | AOUT1 | BOUT1 |
| 2 | Rouge | `M2` | AOUT2 | BOUT2 |
| 3 | Noir | `VCC` (encodeur) | **3V3** (jamais VBAT) | 3V3 |
| 4 | Bleu | `GND` | GND | GND |
| 5 | Vert | `C1` (canal A) | GPIO14 | GPIO35 |
| 6 | Jaune | `C2` (canal B) | GPIO21 | GPIO36 |

⚠ Les broches 1-2 (moteur, jusqu'à 8,4 V pulsé) sont voisines de la
broche 3 (3V3 encodeur). Une inversion VCC/M1 a déjà coûté une session
(2026-09-02) : sérigraphier les noms des 6 broches à côté du connecteur.
Pistes M1/M2 ≥ 0,8 mm.

Pull-ups encodeur : 10 kΩ vers 3V3 sur C1/C2 (question laissée ouverte
dans `WIRING.md`). Ajouter une empreinte 100 pF vers GND sur chaque
signal, non montée par défaut, en réserve si les fils moteur parasitent
les encodeurs.

### 5.4 Audio : 2 × MAX98357A + 2 haut-parleurs

| Broche MAX98357A | Ampli **gauche** | Ampli **droit** |
|---|---|---|
| `VDD` (×2) | `5V` + 10 µF + 100 nF | idem |
| `GND` (+ pad) | GND | GND |
| `BCLK` | GPIO38 | GPIO38 |
| `LRCLK` | GPIO39 | GPIO39 |
| `DIN` | GPIO40 | GPIO40 |
| `GAIN_SLOT` | 🔧 gain : GND = 12 dB, flottant = 9 dB (voir tableau de la datasheet). Prévoir 2 empreintes de résistance pour pouvoir changer. | idem |
| `SD_MODE` | **10 kΩ** depuis `AMP_EN` (TCA9554 P1) | **220 kΩ** depuis `AMP_EN` |
| `OUTP` / `OUTN` | JST-PH 2 broches HP **gauche** | JST-PH 2 broches HP **droit** |

**Comment un seul fil `DIN` donne deux canaux** : la tension sur
`SD_MODE` choisit le canal. Elle résulte du pont entre la résistance
ci-dessus et la pull-down interne de ~100 kΩ de l'ampli :
- < 0,16 V → ampli coupé. C'est le cas quand `AMP_EN` est bas, donc au
  boot : pas de souffle.
- 0,77-1,4 V → canal **droit** : 3,3 V × 100k/(100k+220k) ≈ 1,0 V.
- > 1,4 V → canal **gauche** : 3,3 V × 100k/(100k+10k) ≈ 3,0 V.

⚠ Ces seuils viennent du guide Adafruit du MAX98357A. Les recontrôler
dans le tableau « SD_MODE » de la datasheet Maxim avant de figer les
valeurs.

Placement : chaque ampli **à côté de son connecteur**, chacun d'un côté
de la carte. Les pistes `OUTP`/`OUTN` vont par paire, courtes et larges
(≥ 0,5 mm). Haut-parleurs : 4 Ω / 3 W (`BOM.md`). Les fils de
haut-parleur font ~10 cm au plus : l'ampli « sans filtre » s'en
accommode. Prévoir des empreintes de ferrite en option si l'ESP32 capte
des parasites.

### 5.5 Micro ICS-43434

| Broche | Connexion |
|---|---|
| `VDD` | 3V3 + 100 nF au plus près |
| `GND` | GND |
| `SCK` | GPIO38 (même fil que BCLK des amplis) |
| `WS` | GPIO39 (même fil que LRCLK) |
| `SD` | GPIO41 (+ 100 kΩ vers GND recommandé par la datasheet, la sortie étant en haute impédance la moitié du temps) |
| `L/R` (SELECT) | GND → canal gauche (comme l'INMP441 prévu) |

⚠ **Micro à port inférieur** : percer un **trou de 0,8-1 mm** dans le
PCB sous le port, sans cuivre autour (anneau de garde GND), et aligner
une ouverture dans la coque imprimée. L'éloigner des moteurs, du
DRV8833, des bucks et des haut-parleurs (bruit et larsen).

### 5.6 Capteurs I2C sur la carte (bus GPIO1/GPIO2)

| Puce | Adresse | Broches particulières |
|---|---|---|
| LSM6DS3TR-C (IMU) | **0x6B** | `SDO/SA0` → **3V3** (0x6A est pris par le chargeur BQ25887), `CS` → 3V3 (force le mode I2C), `INT1/INT2` non reliés, 100 nF + 10 µF sur VDD/VDDIO |
| BME688 | **0x76** | `SDO` → GND (même adresse que le BME280 actuel, `sensors_config.h` inchangé), `CSB` → 3V3, 100 nF |
| INA219 | **0x40** | `A0`, `A1` → GND, `IN+`/`IN−` aux bornes du shunt (pistes de mesure séparées, « Kelvin »), 100 nF |
| BQ25887 (chargeur) | **0x6A** | Adresse fixe. Confirmée par le pilote open source `bq25887` (crate Rust, transcrit de la datasheet), pas encore lue sur la datasheet TI elle-même. Le firmware vérifie le numéro de pièce avant toute écriture. Voir §5.1. |
| TCA9554 | **0x20** | `A0-A2` → GND, `INT` non relié, 100 nF |
| VL53L0X gauche (connecteur) | 0x29 → **0x30** au démarrage | XSHUT → 3V3 (logique actuelle) |
| VL53L0X droit (connecteur) | **0x29** | XSHUT → TCA9554 P0 |
| Connecteur Qwiic (option) | --- | réserve pour un futur capteur |

Aucun conflit d'adresse. La caméra (0x30 aussi) est sur **son propre
bus SCCB** (GPIO4/5), donc pas de collision avec le ToF gauche réadressé.

Pull-ups du bus : **4,7 kΩ** sur la carte. Les modules VL53L0X ont
souvent leurs propres pull-ups 10 kΩ : l'ensemble en parallèle reste
dans une plage correcte.

⚠ **Placement du BME688** : en **bord de carte**, loin des bucks, du
DRV8833, de l'ESP32 et des amplis, entouré d'une **fente fraisée**
(découpe en U dans le PCB) qui l'isole thermiquement. Sinon il mesure
la chaleur de la carte au lieu de l'air. La coque doit laisser passer
l'air à cet endroit.

⚠ **Placement de l'IMU** : près du **centre de rotation** du robot, axes
alignés sur ceux du châssis. Reporter les flèches X/Y de la datasheet en
sérigraphie.

### 5.7 Connecteur caméra (FPC 24 broches, pas 0,5 mm)

Brochage courant des modules OV2640 « type ESP32-CAM » :

| Broche FPC | Signal caméra | Connexion |
|---|---|---|
| 1 | NC | --- |
| 2 | AGND | GND |
| 3 | SIO_D | GPIO4 |
| 4 | AVDD | `CAM_2V8` + 1 µF |
| 5 | SIO_C | GPIO5 |
| 6 | RESET | 10 kΩ vers 3V3 + 100 nF vers GND (reset à la mise sous tension) |
| 7 | VSYNC | GPIO6 |
| 8 | PWDN | TCA9554 P2 + 10 kΩ vers GND |
| 9 | HREF | GPIO7 |
| 10 | DVDD | `CAM_1V2` + 1 µF |
| 11 | DOVDD | `3V3` + 1 µF |
| 12 | Y9 | GPIO16 |
| 13 | XCLK | GPIO15 |
| 14 | Y8 | GPIO17 |
| 15 | DGND | GND |
| 16 | Y7 | GPIO18 |
| 17 | PCLK | GPIO13 |
| 18 | Y6 | GPIO12 |
| 19 | Y2 | GPIO11 |
| 20 | Y5 | GPIO10 |
| 21 | Y3 | GPIO9 |
| 22 | Y4 | GPIO8 |
| 23 | Y1 | NC (bits de poids faible inutilisés en 8 bits) |
| 24 | Y0 | NC |

⚠ **Point le plus risqué de la carte.** Ce brochage est celui
couramment cité pour les OV2640 des ESP32-CAM, mais je n'ai pas pu le
confirmer sur un schéma officiel. Selon le côté des contacts du
connecteur (haut/bas), la numérotation peut aussi se retrouver
**inversée** (1↔24). Avant de router :
1. Le comparer au **schéma officiel de l'ESP32-S3-EYE** (publié par
   Espressif) ou à la datasheet du module caméra acheté.
2. Choisir un connecteur FPC dont le côté de contact correspond à la
   nappe : contacts **en dessous** si la nappe arrive avec les pistes
   dorées vers le PCB.
3. Imprimer le PCB à l'échelle 1:1 sur papier et poser la nappe dessus
   pour vérifier le sens.

⚠ **Longueur de nappe** : la caméra est dans la tête, la carte dans le
corps. La nappe livrée sur l'ESP32-CAM (~2 cm) sera trop courte. Il
existe des OV2640 avec nappe de 75 mm ou 100 mm. Au-delà de ~10-15 cm,
l'image se dégrade : baisser alors XCLK de 20 à 10 MHz dans le firmware.
La nappe plie à chaque mouvement de la tête : prévoir en CAO un **grand
rayon de courbure** et un serrage qui ne la cisaille pas.

### 5.8 Connecteur écran ST7789 (JST-PH 8 broches)

Ordre identique à la barrette des modules ST7789 1,69" courants : un
câble droit suffit.

| Broche | Signal | Connexion |
|---|---|---|
| 1 | GND | GND |
| 2 | VCC | 3V3 |
| 3 | SCL (SCLK) | GPIO42 |
| 4 | SDA (MOSI) | GPIO43 |
| 5 | RES | TCA9554 P3 |
| 6 | DC | GPIO44 |
| 7 | CS | **GND** (seul périphérique SPI : il reste sélectionné en permanence) |
| 8 | BLK | TCA9554 P4 |

⚠ Vérifier l'ordre sur la sérigraphie du module écran réellement
acheté, certains modules n'ont pas de broche CS. Avec un câble de 15-20 cm vers la tête,
limiter l'horloge SPI à ~20-27 MHz au lieu de 40 MHz.

### 5.9 Capteurs ToF VL53L0X (2 × JST-PH 6 broches)

Les deux VL53L0X ne sont **pas soudés sur la carte** : ce sont des
modules indépendants (type GY-530), fixés en façade du châssis là où
ils doivent voir, et reliés à la carte par un câble chacun. La carte
n'a que leurs deux connecteurs.

Ordre identique à la barrette des modules GY-530 (`VIN GND SCL SDA
GPIO1 XSHUT`) :

| Broche | Signal | ToF gauche | ToF droit |
|---|---|---|---|
| 1 | VIN | 3V3 | 3V3 |
| 2 | GND | GND | GND |
| 3 | SCL | GPIO2 | GPIO2 |
| 4 | SDA | GPIO1 | GPIO1 |
| 5 | GPIO1 (interruption) | NC | NC |
| 6 | XSHUT | **3V3** | **TCA9554 P0** |

Les deux connecteurs sont identiques. **Les marquer « G » et « D » en
sérigraphie**, car les inverser inverse `distance_left`/`distance_right`.

### 5.10 Servo, E-stop, Qwiic, points de test

| Connecteur | Broches |
|---|---|
| Servo (barrette 3 × 2,54 mm) | 1 = GND (marron), 2 = `5V` (rouge), 3 = signal GPIO37 (orange) --- ordre standard des servos, détrompé en sérigraphie |
| E-stop (JST-PH 2) | 1 = `ESTOP_N`, 2 = GND |
| Qwiic / STEMMA QT (JST-SH 4, option) | 1 = GND, 2 = 3V3, 3 = SDA (GPIO1), 4 = SCL (GPIO2) |
| Points de test (pastilles) | `B−`, `MID`, `VPACK`, `VBAT_SYS`, `VBUS`, `5V`, `3V3`, `CAM_2V8`, `CAM_1V2`, GND (×2), SDA, SCL, `ESTOP_N`, TCA9554 P7 |

------------------------------------------------------------------------

## 6. Récapitulatif des bus

| Bus | Broches | Participants |
|---|---|---|
| I2C capteurs (I2C0) | SDA=1, SCL=2 | LSM6DS3TR-C 0x6B, BQ25887 0x6A, BME688 0x76, INA219 0x40, TCA9554 0x20, VL53L0X G 0x30 / D 0x29, Qwiic |
| SCCB caméra (I2C1) | SIOD=4, SIOC=5 | OV2640 0x30 |
| I2S0 full-duplex | BCLK=38, WS=39, DOUT=40, DIN=41 | 2 × MAX98357A (G/D), ICS-43434 |
| SPI écran | SCLK=42, MOSI=43, DC=44 | ST7789 |
| PWM (LEDC) | 45, 46, 47, 48, 37 + XCLK 15 | moteurs, servo, horloge caméra |
| USB natif | D−=19, D+=20 | USB-C |

------------------------------------------------------------------------

## 7. Firmware de la carte (2026-10-13 : compile en CI, **jamais testé sur matériel**)

Le firmware principal (`esp32/`) gère la carte via l'environnement
PlatformIO **`rover_pcb_v1`**. Le WROOM et le S3 de dev compilent
toujours avec leur brochage d'origine (règle `CLAUDE.md`).

✅ **Compile** : la CI GitHub construit `rover_pcb_v1` à chaque
modification du firmware, avec le WROOM et le S3. ⚠ **Jamais testé sur
matériel** : la carte n'existe pas encore. Tout ce qui suit est à
valider à la mise en service (`pcb/KICAD_GUIDE.md` §9).

| Point | Où dans le code | Remarque |
|---|---|---|
| Brochage complet de la carte | `esp32/include/board_pins_pcb_v1.h` | Un seul fichier, calqué ligne à ligne sur le §4. Toute modification du schéma doit y être reportée dans le même commit. |
| Environnement de build | `esp32/platformio.ini`, `[env:rover_pcb_v1]` | N16R2 (`qio_qspi`), 16 Mo, partitions OTA 2 × 6,5 Mo, PSRAM, console sur USB natif (`ARDUINO_USB_CDC_ON_BOOT=1`) |
| Lignes lentes (expandeur) | `esp32/lib/board/BoardIo.*`, `Tca9554.*` | XSHUT, AMP_EN, PWDN caméra, RST/BLK écran, LED, nFAULT. Le reste du code dit « quoi » (`BoardLine::DisplayReset`) sans savoir « où » : GPIO sur le WROOM, expandeur sur le PCB. Expandeur absent : `ERROR sensor=io_expander`, nouvel essai toutes les 5 s. |
| Caméra intégrée | `esp32/lib/camera/RoverCamera.*`, `include/camera_config.h` | Reprise du firmware ESP32-CAM. Flux sur `http://rover-esp32.local:81/stream` (**changer `camera_url` côté Pi**). Canal/timer LEDC 7/3 (le défaut 0/0 volerait le PWM du moteur gauche). Un seul flux à la fois. N'écrit jamais sur `Serial` (protocole). |
| IMU | `esp32/lib/sensors/ImuSensor.*` | LSM6DS3TR-C en 0x6B sur le PCB, MPU-6050 ailleurs : mêmes champs `STATE` |
| Batterie | `esp32/lib/power/BatteryMonitor.*`, `Ina219.*` | Pilote INA219 maison (chaque transaction I2C vérifiée). Télémétrie `battery= battery_mv= battery_ma=` (négatif en charge). |
| Chargeur | `esp32/lib/power/ChargerMonitor.*` | Vérifie l'identité de la puce avant toute écriture. Configure 4,20 V/cellule, 1 A de charge, 2 A max sur l'USB, chien de garde coupé. Remet la config si la puce se réinitialise. Surveille chaque cellule : `cell_low` à 3,3 V ; `cell_critical` à 3,1 V, qui met le robot en `SAFE` ; `cell_overvoltage` à 4,25 V, qui bloque la charge jusqu'au reboot. |
| Sommeil profond batterie basse | `esp32/lib/power/LowBatterySleep.*` | Sous 3,0 V/cellule (hors USB et hors charge), le robot coupe amplis, écran et caméra, puis l'ESP32 s'endort (`EVENT name=battery_shutdown`). Réveil toutes les 15 min pour mesurer. Il ne redémarre vraiment qu'au-dessus de 3,3 V/cellule ou avec l'USB. Compense la coupure basse à 2,4 V de la S-8252AAC. |
| Calculs « purs » (encodages registres, cohérence cellules) | `esp32/lib/power_math/PowerMath.h` + `esp32/test/test_power_math` | Chaque encodage est vérifié contre les **valeurs de reset documentées**. |
| Pas de servo de lacet | `head_config.h` → `ROVER_PIN_SERVO_YAW = -1` | `ServoJoint` ignore une broche négative |
| Écran | `display_config.h` | SPI ramené à 26,7 MHz (câble vers la tête), CS à la masse, reset via l'expandeur |

**Ce qui n'est PAS fait** (rien de bloquant pour dessiner le PCB) :
- **Pilote audio I2S** (amplis + micro) : il n'existe sur aucune carte.
  Les amplis restent coupés (`AMP_EN` bas) tant qu'il n'est pas écrit.
- **Échelle des ADC du chargeur** (1 mV par pas supposé). Elle n'a pas
  pu être lue dans la datasheet. Le firmware s'en protège : les mesures
  de cellules ne servent que si leur somme égale la tension du pack
  mesurée par l'INA219 (sinon `cells_ok=0` et
  `EVENT name=cells_implausible`). **Premier contrôle à la mise en
  service : `cells_ok=1` avec des cellules réalistes.**
- **Côté Pi** : changer `camera_url` pour
  `http://rover-esp32.local:81/stream`.

------------------------------------------------------------------------

## 8. Questions ouvertes (à trancher avant le routage)

| # | Question | Ma proposition |
|---|---|---|
| 1 | Dimensions et fixations du PCB | À extraire de la CAO du châssis (export DXF du contour + trous) |
| 2 | ~~Connecteur des moteurs N20~~ | **Tranché le 2026-10-13** : JST-PH 6 broches dans l'ordre de `esp32/WIRING.md` (§5.3), câble adapté par l'utilisateur si besoin |
| 3 | E-stop NO (comme aujourd'hui) ou NC (plus sûr) ? | NC si le bouton n'est pas encore acheté |
| 4 | IMU : LSM6DS3TR-C (nouvelle lib) ou MPU-6050 (zéro code) ? | LSM6DS3TR-C |
| 5 | Assemblage : tout par JLCPCB (PCBA) ou soudure maison ? | **PCBA obligatoire** pour BME688, LSM6DS3, MAX98357A, ICS-43434 (boîtiers LGA/QFN impossibles au fer). Seuls les connecteurs traversants à la main. |
| 6 | 2 ou 4 couches ? | **4 couches** (signal / GND / alim / signal) : un plan de masse continu pardonne beaucoup d'erreurs de débutant, pour quelques euros de plus |
| 7 | Modèle et longueur de la nappe caméra | OV2640, nappe 75 mm, à commander en même temps que le PCB |
| 8 | ~~Type de batterie et connecteur~~ | **Tranché le 2026-10-13** (choix délégué par l'utilisateur, critère sécurité) : 2 × 18650 en supports séparés, JST-VH 3 broches + CTN (§5.1) |
| 9 | Emplacement des 2 supports 18650 dans le châssis (~2 × 21 × 77 mm) | À placer en CAO : bas et centré (centre de gravité), loin des moteurs, accessibles pour changer les cellules |
