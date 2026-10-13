# Aide au câblage --- niveau débutant

> Ce fichier donne, composant par composant, "quelle broche va où" en
> langage simple. Pour le détail technique complet (pourquoi ce choix de
> GPIO, pièges connus, historique des révisions), voir `esp32/WIRING.md`
> et `esp32-cam/WIRING.md`.
>
> La **partie 1** (ci-dessous) cible la carte **ESP32 WROOM** (devkit
> 30 broches), la carte physique actuelle du robot. La **partie 2** (en
> fin de fichier) concerne la **carte PCB sur mesure** : ce qu'on y
> branche, et sa création pas à pas dans KiCad.

------------------------------------------------------------------------

## Ampli audio I2S (MAX98357A)

Confirmé avec l'utilisateur le 2026-09-13. Cet ampli reçoit un signal
audio numérique (I2S) de l'ESP32 principal et pilote un petit
haut-parleur.

| Broche du module MAX98357A | Où la brancher | Remarque |
|---|---|---|
| `VIN` | 5V (pas 3.3V) | L'ampli a besoin de 5V pour driver le haut-parleur correctement |
| `GND` | GND commun (ESP32 + alim) | |
| `BCLK` | GPIO16 | |
| `LRC` (parfois noté `WS`) | GPIO17 | |
| `DIN` | GPIO14 | |
| `GAIN` | Selon le volume voulu --- relier au 3V3, au GND ou laisser flottant change le gain (voir la sérigraphie/datasheet du module précis reçu) |
| `SD` (shutdown, si présent) | 3V3 direct si pas de contrôle logiciel prévu (ampli toujours actif) |
| `+` / `−` haut-parleur | Directement les 2 fils du haut-parleur | Pas de polarité stricte à respecter en général, mais garder la même orientation sur les deux canaux si tu en câbles deux |

⚠ Ces 3 broches (GPIO16/17/14) étaient réservées à autre chose dans les
plans précédents (une liaison série vers le Raspberry Pi jamais
utilisée, et un monitoring de batterie jamais câblé) --- elles ont été
libérées spécifiquement pour cet ampli, c'est normal si tu vois une
trace de ces anciens usages ailleurs dans le dépôt.

------------------------------------------------------------------------

## Capteurs de distance VL53L0X --- gauche et droite

✅ **Câblé et validé sur le vrai robot le 2026-09-20** --- les deux
capteurs répondent et mesurent.

Les deux capteurs partagent le même bus I2C (2 fils communs à tout le
bus). **Un seul fil `XSHUT` est piloté**, celui du capteur droit.

| Broche du module VL53L0X | Où la brancher |
|---|---|
| `VIN`/`VCC` (les deux capteurs) | 3V3 |
| `GND` (les deux capteurs) | GND commun |
| `SDA` (les deux capteurs, même fil) | GPIO21 |
| `SCL` (les deux capteurs, même fil) | GPIO22 |
| `XSHUT` du capteur **gauche** | **3V3** (en permanence, comme `VIN`) |
| `XSHUT` du capteur **droit** | GPIO4 |
| `GPIO1` du module (sortie interruption) | ne rien brancher |

**Placement physique** : les deux **côte à côte en façade avant**,
tournés vers l'avant --- pas un devant et un derrière. Le code
`distance_left`/`distance_right` et le réflexe d'arrêt sur obstacle
supposent les deux orientés dans la même direction.

### ⚠ Pourquoi le `XSHUT` gauche va au 3V3 et pas sur une broche

Ce guide demandait `GPIO0` jusqu'au 2026-09-20. **C'était impossible** :
le devkit ESP32 WROOM 30 broches **ne sort pas GPIO0 du tout**. La
rangée réelle enchaîne `GND, 15, 2, 4, RX2, TX2` --- il n'y a aucun `0`
nulle part, parce que cette broche reste interne à la carte (elle est
reliée au bouton `BOOT` et au circuit qui redémarre l'ESP32 tout seul
au moment du flash).

C'est la raison pour laquelle ces deux capteurs sont restés non câblés
du 2026-09-02 au 2026-09-20 : le plan était irréalisable, sans que
personne s'en aperçoive.

Rien n'est perdu. Les deux capteurs sortent d'usine avec **la même
adresse** (0x29) et ne peuvent donc pas parler sur le bus en même temps
au démarrage. Pour les départager, il suffit d'en **éteindre un seul**
le temps de renommer l'autre :

1. le firmware éteint le capteur **droit** (`GPIO4` au niveau bas) ;
2. le capteur **gauche**, seul à répondre, est renommé en `0x30` ;
3. le firmware rallume le droit, qui prend l'adresse `0x29` laissée
   libre.

Le `XSHUT` du gauche n'a donc jamais besoin d'être piloté : il reste
branché au 3V3, c'est-à-dire « allumé en permanence ». En prime, ça
libère une broche sur une carte où il n'en reste aucune, et ça évite
une broche sensible au démarrage.

💡 **Un détail qui peut surprendre** : un VL53L0X garde son nouveau nom
(`0x30`) tant qu'il n'est pas **débranché du courant**. Redémarrer
l'ESP32 (bouton, reflash) ne suffit pas. Le firmware le remet donc
lui-même à son adresse d'usine avant chaque initialisation --- sans
quoi le capteur gauche serait porté disparu à partir du deuxième
démarrage. Rien à faire de ton côté, mais ça explique le
`SYSTEM action=tof_status` ci-dessous.

### Vérifier que le câblage est bon

- `SYSTEM action=i2c_scan` → doit lister **`0x29` et `0x30`** (en plus
  de `0x68` = capteur de mouvement et `0x76` = capteur
  température/pression).
- `SYSTEM action=tof_status` → détaille ce que la séquence de
  démarrage a vu, étape par étape (utile si un capteur manque à
  l'appel) : `tof_post29=1` et `tof_lbegin=1` signifient que le gauche
  a bien été trouvé et initialisé, `tof_rbegin=1` pareil pour le droit.
- `SYSTEM action=tof_rescan` → rejoue toute la séquence sans
  redémarrer, pratique après avoir rebranché un fil.
- Dans la télémétrie, `distance_left=`/`distance_right=` en
  millimètres. **`9999` = capteur absent**, **`8190` = capteur présent
  mais ne voit rien à portée**. Les deux se ressemblent à l'oeil et ne
  veulent pas du tout dire la même chose.

------------------------------------------------------------------------

## Servomoteur de tête (un seul, MG90S)

✅ **Câblé et validé sur le robot le 2026-09-20.**

| Fil du servo | Où le brancher |
|---|---|
| **Marron** (ou noir) | GND --- **la même masse que l'ESP32**, sinon le signal n'a aucune référence et le servo part n'importe où |
| **Rouge** | +5 à 6 V (jamais le 3V3 de l'ESP32 : un MG90S tire jusqu'à 700 mA) |
| **Orange** (ou jaune) | **GPIO13** |

### ⚠ Pourquoi un seul servo, et pas deux

Le projet prévoyait deux servos face à face entraînant la même pièce
(montage « tandem »). **Abandonné le 2026-09-20** : deux servos sur un
axe commun ne sont jamais parfaitement d'accord (leurs points neutres et
leurs courses diffèrent toujours un peu), donc ils passent leur temps à
se combattre --- ça chauffe, ça use, et ça ne tient pas la position.
Remplacé par **un servo qui entraîne + un roulement qui guide** de
l'autre côté.

### ⚠ Le piège du palonnier : à lire avant de (re)monter le bras

Le bras (« palonnier ») se fixe sur l'axe par des cannelures : il ne
peut être posé que tous les ~1,4°, et **rien n'indique où est le milieu
de la course du servo**. Le 2026-09-20, il était monté environ **90°
hors du neutre** : la position de repos de la tête tombait tout au bout
de la course du servo, si bien que **n'importe quel angle commandé
enfonçait la tête dans sa butée**, dans les deux sens. Ça a coûté une
bonne partie de la session avant d'être compris.

**La bonne méthode, à suivre à chaque remontage :**

1. **Retirer le bras** de l'axe du servo (pas seulement le dévisser de
   la pièce : l'enlever complètement).
2. Demander le centrage : le firmware place le servo au **milieu exact**
   de sa course.
3. **Alors seulement**, refixer le bras, tête en position neutre.

On dispose ainsi de ~90° de marge de chaque côté, pour une tête qui n'a
besoin que de ~20°.

💡 Le firmware enregistre cette position de référence en mémoire
(`SYSTEM action=head_origin`), et elle **survit aux redémarrages et aux
reflash**. Si le bras est remonté légèrement de travers, il suffit de
réenregistrer l'origine --- pas besoin de recompiler quoi que ce soit.

### ⚠ Ne jamais utiliser GPIO19

Cette broche était prévue pour un second servo : **elle n'émet rien sur
cette carte** (vérifié le 2026-09-20 en croisant les deux servos et deux
canaux PWM différents). C'est la deuxième broche défectueuse de ce
devkit, après GPIO0 qui n'est carrément pas sortie sur le connecteur.

------------------------------------------------------------------------

## ESP32-CAM (module caméra séparé)

Ce module est **indépendant** de l'ESP32 principal --- il ne se
connecte à rien d'autre en filaire, seulement en WiFi une fois en
service. Le câblage ci-dessous ne sert qu'à le programmer une première
fois (et pour l'alimentation).

### Pour flasher le firmware (adaptateur FTDI/USB-série requis, réglé sur 3.3V)

| Broche de l'adaptateur FTDI | Broche de l'ESP32-CAM | Remarque |
|---|---|---|
| 5V (ou VCC) | `5V` | |
| GND | `GND` | |
| TX | `U0R` | Croisé : TX de l'adaptateur va sur RX de la carte |
| RX | `U0T` | Croisé : RX de l'adaptateur va sur TX de la carte |
| (fil volant) | `GPIO0` → `GND` | **Seulement pendant le flash**, à retirer juste après |

Étapes : relier GPIO0 au GND → appuyer sur RESET → lancer l'upload →
une fois terminé, **débrancher le fil GPIO0-GND** puis reset à nouveau.

### En usage normal (une fois flashé)

Seulement `5V` et `GND` sont nécessaires --- tout le reste (flux vidéo)
passe par le WiFi vers le Raspberry Pi.

------------------------------------------------------------------------

## Raspberry Pi ↔ ESP32 principal

**Changement important du 2026-09-15 : il n'y a plus aucun câble ici.**

Avant, le Raspberry Pi était posé sur le robot et relié à l'ESP32 par un
câble USB. Maintenant, **le Raspberry Pi reste à la maison**, branché sur
le secteur, et parle à l'ESP32 **par le WiFi** --- comme le fait déjà
l'ESP32-CAM.

Ce qui reste sur le robot : l'ESP32, l'ESP32-CAM, la batterie. C'est
tout.

Pourquoi ce choix :

- le robot devient **plus petit** (plus de Raspberry Pi ni de sa carte
  SD à caser dans le châssis) ;
- la **batterie dure nettement plus longtemps** --- le Raspberry Pi était
  le plus gros consommateur quand le robot ne roule pas ;
- un **convertisseur 5V en moins** à câbler (celui qui était dédié au
  Raspberry Pi).

⚠ Contrepartie à connaître : **sans WiFi, le robot n'a plus de cerveau**
(plus d'IA, plus de voix, plus de caméra analysée). Il continue de se
protéger tout seul : il s'arrête, évite les obstacles.

**Mais il reste pilotable** --- voir juste en dessous.

------------------------------------------------------------------------

## Piloter Rover sans Pi et sans WiFi (mode « pilotage direct »)

Si le Raspberry Pi est éteint, ou s'il n'y a aucun réseau (tu emmènes
Rover dans un parc, chez quelqu'un, en panne de box...), **le robot
fabrique lui-même son propre réseau WiFi** et tu le pilotes depuis ton
téléphone.

### Ce qu'il faut faire une seule fois, avant

Choisir un mot de passe pour ce mode, sinon il refusera de démarrer
(c'est volontaire : sans mot de passe, n'importe qui à portée pourrait
faire rouler ton robot).

1. Envoyer `SYSTEM action=wifi_setup` au robot (ou le faire depuis le
   portail de configuration habituel).
2. Se connecter au réseau `Rover-Setup-XXXXXX` qui apparaît.
3. Remplir le champ **« Mot de passe pilotage direct »** --- minimum
   **8 caractères**.
4. Enregistrer : le robot redémarre.

### Comment l'utiliser, le jour où tu en as besoin

1. Allumer le robot. **Ne rien faire pendant 30 secondes** --- au bout de
   ce délai sans nouvelle du Raspberry Pi, il ouvre tout seul un réseau
   nommé `Rover-Pilot-XXXXXX`.
2. Connecter ton téléphone à ce réseau (avec le mot de passe choisi
   plus haut).
3. Ouvrir n'importe quelle page dans le navigateur (par exemple
   `192.168.4.1`) : la page de pilotage s'affiche.
4. Appuyer sur **« Activer »** pour sortir de la sécurité, puis piloter
   avec le rond tactile. Le bouton **STOP** arrête tout.

### Bon à savoir

- **Si tu fermes la page ou t'éloignes trop, le robot s'arrête seul.**
  C'est la même sécurité que d'habitude, pas une sécurité à part.
- Le bouton d'arrêt d'urgence physique reste prioritaire sur tout.
- Le robot refuse toujours d'avancer vers un obstacle détecté ---
  reculer et tourner restent possibles.
- Ce mot de passe est **différent** de celui de l'OTA : conduire le
  robot et reprogrammer le robot ne sont pas la même chose, tu peux
  confier l'un sans l'autre.

------------------------------------------------------------------------

## Micro (prévu, pas encore câblé)

Le micro n'est pas encore acheté/câblé, mais sa place est maintenant
réservée --- c'est justement le départ du Raspberry Pi qui a libéré la
broche dont il avait besoin.

Il se branchera en partageant deux fils avec l'ampli audio :

| Broche du micro (INMP441) | Où la brancher | Remarque |
|---|---|---|
| `VDD` | 3V3 | |
| `GND` | GND commun | |
| `SCK` (horloge) | GPIO16 | **le même fil que le `BCLK` de l'ampli** |
| `WS` | GPIO17 | **le même fil que le `LRC` de l'ampli** |
| `SD` (sortie audio du micro) | GPIO3 | ⚠ à câbler avec un cavalier, voir ci-dessous |
| `L/R` | GND | choisit le canal gauche, pas de broche ESP32 nécessaire |

### ⚠ Pourquoi un cavalier (jumper) sur le fil `SD`

`GPIO3` servait à la prise USB de l'ESP32. En y branchant le micro, on
perd la possibilité de brancher un câble USB pour voir les messages de
l'ESP32 ou le reprogrammer en filaire.

Ce n'est gênant que si le WiFi ne marche plus --- mais ce jour-là, on
perdrait le pilotage **et** le dépannage en même temps. D'où la
solution : **un cavalier sur ce fil, qu'on retire pour rebrancher l'USB**
(la pièce imprimée en 3D est prévue pour y accéder sans démonter le
robot). Ne surtout pas souder ce fil en dur.

Au quotidien, la mise à jour du firmware se fait par le WiFi (OTA), donc
le cavalier reste en place la plupart du temps.

------------------------------------------------------------------------

## Broches à ne surtout pas utiliser pour un nouveau composant

Si un jour tu ajoutes encore un module, évite ces broches (déjà prises
ou dangereuses au boot) --- voir `esp32/WIRING.md` pour le tableau
complet et à jour :

- GPIO1 : prise USB (sortie), ne pas toucher.
- GPIO3 : **réservée au micro** depuis le 2026-09-15 (voir plus haut) ---
  c'était la liaison USB vers le Raspberry Pi, libérée par son départ.
- GPIO6 à GPIO11 : réservées en interne à la mémoire flash, jamais
  disponibles.
- GPIO2, GPIO12, GPIO15 : sensibles au démarrage, déjà utilisées
  (écran / buzzer) mais à éviter pour tout nouveau signal qui tirerait
  fort au boot.
- GPIO19 : **ne fonctionne pas sur cette carte** (constaté le
  2026-09-20 en croisant deux servos et deux canaux PWM). Ne rien y
  brancher.
- GPIO0 : **n'existe pas sur le connecteur** de ce devkit 30 broches
  (constaté sur la carte le 2026-09-20). Inutile de la chercher : si un
  plan de câblage la mentionne, c'est le plan qui est faux.

------------------------------------------------------------------------
------------------------------------------------------------------------

# PARTIE 2 --- La carte PCB Rover (ESP32-S3)

> ⚠ **La carte n'existe pas encore** (conception du 2026-10-13). Cette
> partie dit ce qu'il faudra brancher une fois reçue, puis comment la
> dessiner dans KiCad. Les tableaux seront revalidés à la réception
> (règle `CLAUDE.md`).
>
> Références : `pcb/PCB_CONNEXIONS.md` (détail technique de chaque
> fil), `pcb/KICAD_GUIDE.md` (KiCad en détail),
> `esp32/include/board_pins_pcb_v1.h` (brochage côté firmware).

Avec cette carte, **plus aucun fil Dupont** : tout est soudé sur la
carte, il ne reste que des câbles à clipser.

------------------------------------------------------------------------

## A. Ce qu'on branche sur la carte (une fois reçue)

### Vue d'ensemble

```text
                  ┌──────────────── bord avant ───────────────┐
   ToF G [PH6] ── │                                              │ ── [PH6] ToF D
                  │  (antenne ESP32-S3 : rien autour, ni dessous)  │
   HP G  [PH2] ── │                                              │ ── [PH2] HP D
                  │  [FPC caméra]   [PH8 écran]   [servo 3 br.]    │
 Moteur G [PH6] ─ │                                              │ ─ [PH6] Moteur D
                  │  [VH3 batterie] [PH2 CTN]  [PH2 E-stop]        │
                  │  [interrupteur] [USB-C]  BOOT  RESET  LEDs      │
                  └──────────────── bord arrière ─────────────────┘
```

(Disposition indicative : la vraie dépendra du contour tiré de la CAO.)

### Tableau des connecteurs

| Connecteur sur la carte | On y branche | Sens / piège |
|---|---|---|
| **BATT** (JST-VH 3 broches) | Les 2 cellules 18650 via leur câble : noir = B−, blanc = milieu, rouge = B+ | Détrompé. **Vérifier au multimètre le câble avant la 1re fois** : B+ à ~8 V par rapport à B−, milieu à ~4 V |
| **NTC** (JST-PH 2) | La sonde de température collée entre les 2 cellules | Pas de sens |
| **Interrupteur** (sur la carte) | --- | OFF = robot éteint, **la charge USB marche quand même** |
| **USB-C** | Câble USB vers le PC (flasher, console) ou chargeur USB-C (recharger) | Un chargeur ≥ 2 A |
| **MOT G** / **MOT D** (JST-PH 6) | Câble 6 fils de chaque moteur N20 | Ordre : M1, M2, VCC, GND, C1, C2 (comme `esp32/WIRING.md`). G et D marqués sur la carte |
| **HP G** / **HP D** (JST-PH 2) | Haut-parleur gauche / droit (4 Ω, 3 W) | Garder la même orientation des 2 côtés |
| **CAM** (FPC 24 broches) | La nappe de la caméra OV2640 | Ouvrir le volet, glisser la nappe **contacts dorés du bon côté** (marqué sur la carte), refermer |
| **ÉCRAN** (JST-PH 8) | Câble vers l'écran ST7789 dans la tête | Ordre identique à la barrette de l'écran : GND, VCC, SCL, SDA, RES, DC, CS, BLK |
| **SERVO** (3 broches 2,54 mm) | Servo de tête | Marron sur **GND** (marqué −), orange vers l'intérieur |
| **ToF G** / **ToF D** (JST-PH 6) | Capteurs de distance VL53L0X : **tes modules indépendants**, fixés en façade, pas soudés sur la carte | Ordre de leur barrette : VIN, GND, SCL, SDA, GPIO1, XSHUT. **Ne pas inverser G et D** |
| **E-STOP** (JST-PH 2) | Bouton d'arrêt d'urgence | Pas de sens. Bouton appuyé = moteurs coupés **électriquement** |
| **QWIIC** (JST-SH 4, option) | Futur capteur I2C | Câble Qwiic/STEMMA QT standard |

### Boutons et voyants

| Élément | Rôle |
|---|---|
| **RESET** | Redémarre l'ESP32 |
| **BOOT** | Maintenu pendant RESET = mode flash forcé (seulement si l'USB ne répond plus) |
| LED **verte** | Carte alimentée (3,3 V présent) |
| LED **orange** | Batterie en charge |
| LED **d'état** | Allumée = moteurs armés (état `ACTIVE`) |

### Première mise sous tension

Suivre `pcb/KICAD_GUIDE.md` §9 (alimentation de labo, courant limité,
« fausse batterie »). Résumé de ce qu'on doit voir ensuite :

1. Brancher l'USB-C au PC → la carte apparaît (`/dev/ttyACM0`).
2. Flasher : `pio run -e rover_pcb_v1 -t upload` (dans `esp32/`).
3. `SYSTEM action=i2c_scan` → **0x20, 0x40, 0x6A, 0x6B, 0x76** (+ 0x29
   et 0x30 avec les ToF branchés).
4. Batterie branchée : `STATE ... cells_ok=1` avec deux tensions de
   cellules réalistes (3,0 à 4,2 V). **Si `cells_ok=0`, ne pas laisser
   charger sans surveillance** et me le signaler.
5. `SYSTEM action=charger_status` et `SYSTEM action=camera_status` →
   voir `ROVER_PROTOCOL.md` §5.1.

------------------------------------------------------------------------

## B. Créer le PCB dans KiCad, étape par étape

> Chaque étape se termine par une **case à cocher**. Ne passe à la
> suivante que quand la précédente est cochée. Les noms entre crochets
> `[ ]` sont les touches/menus de KiCad (version 9 ou plus, en
> français).
>
> 💡 À chaque fin d'étape : `git add pcb/ && git commit -m "..." && git push`.
> Je peux relire le projet à n'importe quel moment.

### Étape 0 --- Préparer (1 soirée)

1. Installer KiCad (commande dans `pcb/KICAD_GUIDE.md` §1).
2. KiCad → **[Outils → Gestionnaire de plugins et contenus]** →
   installer **JLCPCB Tools**.
3. `pip install --user easyeda2kicad`.
4. **Références LCSC** (relevées le 2026-10-13 via un moteur de
   recherche : les sites LCSC/JLCPCB étaient inaccessibles depuis la
   session). **Avant de commander, ouvrir chaque `Cxxxxx` sur lcsc.com**
   et vérifier : la référence fabricant **exacte** (les suffixes
   comptent), le stock, et le statut chez JLCPCB (« Basic » = moins
   cher à assembler, « Extended » = petit supplément par référence).

   ✅ confirmé par plusieurs sources ou une page LCSC · ⚠️ source unique
   ou réserve à lire · ❓ pas trouvé, à chercher toi-même

   **Puces principales**

   | Pièce | Référence fabricant | LCSC | | Remarque |
   |---|---|---|---|---|
   | Module ESP32-S3 | ESP32-S3-WROOM-1-**N16R2** | C2913205 | ⚠️ | Source unique (Flux). **Ne pas confondre** avec N16 (C2913199) ou N8R2 (C2913204) |
   | Driver moteur | DRV8833PWPR | C50506 | ✅ | Une capture LCSC le dit « non recommandé pour nouveaux designs » : vérifier le stock. Le DRV8833**C**PWPR (C191171) est une **autre** puce, pas un remplaçant direct |
   | Ampli ×2 | MAX98357AETE+T | C910544 | ✅ | Extended, ~10 000 en stock |
   | Micro | ICS-43434 | C5656610 | ⚠️ | Source unique |
   | IMU | LSM6DS3TR-C | C967633 | ✅ | ⚠️ Mouser l'annonce **en fin de vie**. Remplaçant possible : LSM6DSOX, géré par la même bibliothèque Adafruit (petit changement de code, je m'en occupe si besoin) |
   | Capteur env. | BME688 | C3664478 | ✅ | Stock faible et variable selon les relevés |
   | Mesure batterie | INA219AIDCNR | C87469 | ✅ | SOT-23-8 |
   | Expandeur | TCA9554PWR | C477924 | ✅ | Vérifier le boîtier (TSSOP-16) contre l'empreinte |
   | Chargeur 2S | BQ25887RGER | C2761614 | ✅ | Ne pas confondre avec BQ25883RGER (C544362, sans équilibrage). Stock faible (~126) |
   | Buck 5 V et 3,3 V (×2) | TPS563201DDCR | C116592 | ✅ | Gros stock |
   | LDO 2,8 V | XC6206P282MR | C347374 | ⚠️ | Version UMW. Autres numéros selon le fabricant (Torex C14255, Torex -G C2891263) : en choisir **un** en stock |
   | LDO 1,2 V | XC6206P122MR-G | C424699 | ⚠️ | Version Torex « -G » |
   | Protection USB | USBLC6-2SC6 | C7519 | ✅ | |

   **Protection batterie et puissance**

   | Pièce | Référence fabricant | LCSC | | Remarque |
   |---|---|---|---|---|
   | Protection 2S | S-8252**AAC**-M6T1U | C468219 | ✅ | **Choisie le 2026-10-13.** Surcharge 4,30 V, décharge profonde 2,40 V (⚠ à reconfirmer sur la datasheet). 2,4 V est un peu bas pour des 18650, mais le firmware s'endort à 3,0 V/cellule bien avant (`PCB_CONNEXIONS.md` §5.1). **Ne pas prendre** AAA/ABB (2,0 V) ni AAJ (réglée LiFePO4) |
   | Double N-MOSFET (protection) | FS8205A | C2830320 | ⚠️ | Classique des BMS (20 V, 6 A, ~25 mΩ). Un peu juste pour les pics (~2,8 A) : chauffe acceptable mais seuil de surintensité à recalculer avec son R<sub>DS(on)</sub> |
   | (alternative) | FDMW2512NZ | C3278415 | ⚠️ | 20 V, 7,2 A, drain commun, mais 34 mΩ à 2,5 V : plus de chute de tension |
   | P-MOSFET (interrupteur) | AO4407A (AOS) | C16072 | ✅ | 30 V, 12 A, SOIC-8. Seconde source UMW : C2841482 |
   | Diode Schottky ×2 | SS34 | C717457 | ✅ | Version LGE. C8678 (MDD) est marquée « non recommandée » |

   **Connecteurs**

   | Pièce | Référence fabricant | LCSC | | Remarque |
   |---|---|---|---|---|
   | USB-C 16 broches | TYPE-C-31-M-12 | C165948 | ✅ | Le plus utilisé chez JLCPCB |
   | Batterie JST-VH 3 broches | B3P-VH(LF)(SN) (droit) / B3PS-VH(LF)(SN) (coudé) | C160316 / C131336 | ✅ | Traversant, à souder soi-même. Coudé si le câble arrive par le côté |
   | FPC caméra 24 broches 0,5 mm | FPC-05F-24PH20 (XUNPU) | C2856805 | ⚠️ | Contacts **dessous**, volet basculant. ⚠️ À valider avec la vraie nappe (côté des contacts, `PCB_CONNEXIONS.md` §5.7) |
   | JST-PH 6 broches coudé CMS (moteurs, ToF) | S6B-PH-SM4-TB(LF)(SN) | C265405 | ⚠️ | Source unique. Pour info : 4 br. C265102, 5 br. C265104 |
   | JST-PH 2 broches coudé CMS (HP ×2, CTN, E-stop) | S2B-PH-SM4-TB(LF)(SN) | C295747 | ✅ | Page LCSC + JLCPCB. ⚠ Ne pas prendre « C9900150953 » (fiche JLCPCB générique sans fabricant) |
   | JST-PH 8 broches coudé CMS (écran) | S8B-PH-SM4-TB(LF)(SN) | C265121 | ✅ | Page LCSC. Ne pas confondre avec B8B (C495548, sortie verticale) |
   | JST-SH 4 broches (Qwiic) | SM04B-SRSS-TB(LF)(SN) | C160404 | ✅ | |

   **Passifs** (résistances, condensateurs, fusibles PTC, shunt 0,05 Ω
   2512, LED, boutons, interrupteur à glissière) : laisser le plugin
   **JLCPCB Tools** proposer des pièces **« Basic »** de la bonne valeur
   et du bon boîtier. Pas besoin de les chercher une par une.

5. Pour chaque pièce absente de KiCad :
   `easyeda2kicad --full --lcsc_id=Cxxxxx --output ~/kicad-libs/rover`
6. Télécharger la **datasheet** de chaque puce (dans `pcb/datasheets/`).

- [ ] Liste LCSC complète, datasheets téléchargées

### Étape 1 --- Créer le projet et ses feuilles

1. **[Fichier → Nouveau projet]** → `Rover-Project/pcb/rover-main/`.
2. **[Préférences → Gérer les bibliothèques de symboles]** puis
   **d'empreintes** → onglet **Bibliothèques du projet** → ajouter
   `~/kicad-libs/rover`.
3. Ouvrir le schéma. Avec **[S]** (feuille hiérarchique), créer 7
   feuilles : `alimentation`, `mcu`, `camera`, `moteurs`, `audio`,
   `capteurs`, `ihm`.
4. **Règle d'or des noms** : un même nom de **label global**
   **[Ctrl+L]** = un même fil. Utiliser **exactement** ces noms :

   | Labels globaux à créer |
   |---|
   | `SDA`, `SCL` (bus capteurs) |
   | `CAM_SIOD`, `CAM_SIOC`, `CAM_XCLK`, `CAM_PCLK`, `CAM_VSYNC`, `CAM_HREF`, `CAM_Y2` … `CAM_Y9` |
   | `MOT_L_IN1`, `MOT_L_IN2`, `MOT_R_IN1`, `MOT_R_IN2`, `ENC_L_A`, `ENC_L_B`, `ENC_R_A`, `ENC_R_B` |
   | `I2S_BCLK`, `I2S_WS`, `I2S_DOUT`, `I2S_DIN` |
   | `LCD_SCLK`, `LCD_MOSI`, `LCD_DC`, `LCD_RST`, `LCD_BLK` |
   | `SERVO`, `ESTOP_N`, `TOF_R_XSHUT`, `AMP_EN`, `CAM_PWDN`, `DRV_FAULT_N`, `LED_STATUS` |
   | `USB_DP`, `USB_DN`, `VBUS` |
   | Alimentations (symboles **[P]**) : `+3V3`, `+5V`, `VBAT_SYS`, `VPACK`, `GND`, `+2V8`, `+1V2` |

- [ ] 7 feuilles créées, bibliothèques ajoutées

### Étape 2 --- Dessiner chaque feuille

Méthode pour chaque puce : **[A]** placer le symbole → poser le
**schéma d'application de sa datasheet** (condensateurs, résistances)
→ relier avec **[W]** → poser les labels globaux → **[Q]** sur chaque
broche volontairement libre.

#### Feuille `mcu` (ESP32-S3, USB-C, expandeur)

| Broche ESP32-S3 | Label | | Broche ESP32-S3 | Label |
|---|---|---|---|---|
| IO0 | bouton BOOT + 10 kΩ → +3V3 | | IO21 | `ENC_L_B` |
| IO1 | `SDA` | | IO35 | `ENC_R_A` |
| IO2 | `SCL` | | IO36 | `ENC_R_B` |
| IO3 | `ESTOP_N` | | IO37 | `SERVO` |
| IO4 | `CAM_SIOD` | | IO38 | `I2S_BCLK` |
| IO5 | `CAM_SIOC` | | IO39 | `I2S_WS` |
| IO6 | `CAM_VSYNC` | | IO40 | `I2S_DOUT` |
| IO7 | `CAM_HREF` | | IO41 | `I2S_DIN` |
| IO8 | `CAM_Y4` | | IO42 | `LCD_SCLK` |
| IO9 | `CAM_Y3` | | TXD0 (IO43) | `LCD_MOSI` |
| IO10 | `CAM_Y5` | | RXD0 (IO44) | `LCD_DC` |
| IO11 | `CAM_Y2` | | IO45 | `MOT_R_IN1` |
| IO12 | `CAM_Y6` | | IO46 | `MOT_R_IN2` |
| IO13 | `CAM_PCLK` | | IO47 | `MOT_L_IN1` |
| IO14 | `ENC_L_A` | | IO48 | `MOT_L_IN2` |
| IO15 | `CAM_XCLK` | | EN | 10 kΩ → +3V3, 1 µF → GND, bouton RESET → GND |
| IO16 | `CAM_Y9` | | 3V3 | +3V3 + 10 µF + 100 nF |
| IO17 | `CAM_Y8` | | GND + pad | GND |
| IO18 | `CAM_Y7` | | | |
| IO19 | `USB_DN` | | | |
| IO20 | `USB_DP` | | | |

- USB-C : `CC1` et `CC2` → **5,1 kΩ chacune vers GND** ; A6+B6 →
  `USB_DP`, A7+B7 → `USB_DN` (via USBLC6) ; VBUS → `VBUS`.
- TCA9554 : A0/A1/A2 → GND, SDA/SCL → `SDA`/`SCL`, P0 `TOF_R_XSHUT`,
  P1 `AMP_EN`, P2 `CAM_PWDN` (+10 kΩ → GND), P3 `LCD_RST` (+10 kΩ →
  +3V3), P4 `LCD_BLK` (+10 kΩ → +3V3), P5 `DRV_FAULT_N`, P6 →
  1 kΩ → LED → GND, P7 → point de test.
- Pull-ups `SDA`/`SCL` : 4,7 kΩ → +3V3.

- [ ] Feuille `mcu` faite (cocher chaque ligne du tableau)

#### Feuille `alimentation`

Dans cet ordre, en suivant `PCB_CONNEXIONS.md` §5.1 :
1. Connecteur **BATT** VH3 (B−, MID, B+) ; MID → PTC 0,75 A → `MID` +
   1 MΩ → B−.
2. **S-8252** + double N-MOSFET entre B− et `GND` (schéma de la
   datasheet).
3. B+ → **PTC 5 A** → **shunt 0,05 Ω** → `VPACK` ; **INA219** aux bornes
   du shunt (`SDA`/`SCL`, A0/A1 → GND).
4. **BQ25887** : VBUS ← `VBUS`, BAT → `VPACK`, MID → `MID`,
   `SDA`/`SCL`, STAT → LED orange, CD → GND, ILIM → résistance (~2 A),
   TS → pont + connecteur **NTC**.
5. **Interrupteur** : P-MOSFET `VPACK` → `VBAT_SYS`, grille 100 kΩ →
   `VPACK`, interrupteur glissière grille → 10 kΩ → GND.
6. **Buck 5 V** : `VBAT_SYS` → `+5V` (pont 0 Ω en sortie).
7. **Buck 3,3 V** : (`VBAT_SYS` → SS34) + (`VBUS` → SS34) → entrée ;
   sortie `+3V3` (pont 0 Ω en sortie). LED verte.
8. LDO **2,8 V** et **1,2 V** depuis `+3V3` → `+2V8`, `+1V2`.
9. **[P]** → `PWR_FLAG` sur `VPACK`, `VBAT_SYS`, `+5V`, `+3V3`, `GND`.

- [ ] Feuille `alimentation` faite

#### Feuille `moteurs`

- DRV8833 : AIN1/AIN2 ← `MOT_L_IN1`/`MOT_L_IN2`, BIN1/BIN2 ←
  `MOT_R_IN1`/`MOT_R_IN2`, VM ← `VBAT_SYS` (+470 µF +10 µF), VINT
  2,2 µF, VCP 10 nF → VM, AISEN/BISEN → 0,2 Ω → GND, nSLEEP ←
  `ESTOP_N`, nFAULT → `DRV_FAULT_N` (+10 kΩ → +3V3).
- `ESTOP_N` : 10 kΩ → +3V3, 100 nF → GND, connecteur **E-STOP** vers GND.
- Connecteurs **MOT G/D** : 1 AOUT1/BOUT1, 2 AOUT2/BOUT2, 3 +3V3, 4 GND,
  5 `ENC_L_A`/`ENC_R_A`, 6 `ENC_L_B`/`ENC_R_B` ; 10 kΩ → +3V3 sur les 4
  `ENC_*`.

- [ ] Feuille `moteurs` faite

#### Feuille `audio`

- 2 × MAX98357A : VDD ← `+5V`, BCLK ← `I2S_BCLK`, LRCLK ← `I2S_WS`, DIN
  ← `I2S_DOUT`. SD_MODE : **gauche 10 kΩ**, **droit 220 kΩ** depuis
  `AMP_EN`. OUTP/OUTN → **HP G** / **HP D**.
- ICS-43434 : VDD +3V3, SCK `I2S_BCLK`, WS `I2S_WS`, SD → `I2S_DIN`
  (+100 kΩ → GND), L/R → GND.

- [ ] Feuille `audio` faite

#### Feuille `capteurs`

- LSM6DS3TR-C : `SDA`/`SCL`, **SA0 → +3V3** (adresse 0x6B), CS → +3V3.
- BME688 : `SDA`/`SCL`, SDO → GND (0x76), CSB → +3V3.
- **ToF G** (PH6) : 3V3, GND, `SCL`, `SDA`, libre, **XSHUT → +3V3**.
- **ToF D** (PH6) : idem, **XSHUT → `TOF_R_XSHUT`**.
- **QWIIC** (SH4) : GND, +3V3, `SDA`, `SCL`.

- [ ] Feuille `capteurs` faite

#### Feuilles `camera` et `ihm`

- FPC 24 broches : selon le tableau de `PCB_CONNEXIONS.md` §5.7
  (⚠ à vérifier sur le schéma ESP32-S3-EYE). RESET → 10 kΩ → +3V3 +
  100 nF → GND ; PWDN → `CAM_PWDN` ; 4,7 kΩ → +3V3 sur
  `CAM_SIOD`/`CAM_SIOC`.
- **ÉCRAN** (PH8) : GND, +3V3, `LCD_SCLK`, `LCD_MOSI`, `LCD_RST`,
  `LCD_DC`, **GND (CS)**, `LCD_BLK`.
- **SERVO** : GND, `+5V`, `SERVO` (+470 µF sur +5V).
- Points de test : `B-`, `MID`, `VPACK`, `VBAT_SYS`, `VBUS`, `+5V`,
  `+3V3`, `+2V8`, `+1V2`, `GND`, `SDA`, `SCL`, `ESTOP_N`.

- [ ] Feuilles `camera` et `ihm` faites

### Étape 3 --- Vérifier le schéma

1. **[Inspecter → Contrôle des règles électriques]** → **0 erreur**.
2. Erreurs fréquentes :

   | Message | Solution |
   |---|---|
   | « Entrée d'alimentation non pilotée » | Ajouter un `PWR_FLAG` sur ce rail |
   | « Broche non connectée » | Relier, ou **[Q]** si c'est voulu |
   | « Label global présent une seule fois » | Faute de frappe dans le nom (ex. `SDA` / `SDA1`) |

3. Reprendre le tableau GPIO de la feuille `mcu` : cliquer chaque label,
   il doit se surligner sur la bonne feuille.
4. **Pousser sur git et me demander une relecture du schéma.**

- [ ] ERC à 0, relecture faite

### Étape 4 --- Empreintes

**[Outils → Assigner les empreintes]** :

| Composant | Empreinte |
|---|---|
| R, C | `R_0603_1608Metric`, `C_0603_1608Metric` |
| Shunt 0,05 Ω | `R_2512_6332Metric` |
| JST-PH | `Connector_JST:JST_PH_S?B-PH-K_1x0?_P2.00mm_Horizontal` (coudé) |
| JST-VH 3 | `Connector_JST:JST_VH_S3P-VH_1x03_P3.96mm_Horizontal` |
| JST-SH 4 | `Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal` |
| Servo | `PinHeader_1x03_P2.54mm_Vertical` |
| Le reste | celle fournie par easyeda2kicad avec la pièce LCSC |

⚠ Pour chaque connecteur : **mesurer la vraie pièce au pied à
coulisse** et comparer à l'empreinte.

- [ ] Toutes les empreintes assignées

### Étape 5 --- Placer les composants

1. Ouvrir le PCB, **[Fichier → Configuration de la carte]** : 4
   couches, puis les règles JLCPCB (`KICAD_GUIDE.md` §6.1).
2. Dessiner le contour sur `Edge.Cuts` (ou importer le DXF de la CAO),
   ajouter les trous M3.
3. **[F8]** (Mettre à jour depuis le schéma) : tout arrive en tas.
4. Placer **dans cet ordre** (touche **[M]** déplacer, **[R]** tourner) :
   1. connecteurs au bord, là où arrivent les câbles ;
   2. ESP32-S3 au bord, antenne vers l'extérieur ;
   3. alimentation + chargeur dans un coin ;
   4. DRV8833 entre l'arrivée batterie et les connecteurs moteurs ;
   5. amplis à côté de leur haut-parleur ;
   6. BME688 dans un coin isolé (+ fente), IMU au centre, micro loin
      des moteurs ;
   7. chaque condensateur de découplage **collé** à sa puce.
5. **[Alt+3]** vue 3D : tout est-il à l'endroit ?

- [ ] Placement fini, vue 3D vérifiée

### Étape 6 --- Router

Touche **[X]** pour router, **[V]** pour changer de couche (pose un via).

| Ordre | Quoi | Largeur |
|---|---|---|
| 1 | B+/B−, `VPACK`, `VBAT_SYS`, sorties moteurs | ≥ 1,5 mm (ou zones) |
| 2 | `+5V`, sorties HP | ≥ 0,8 mm |
| 3 | `+3V3` | ≥ 0,5 mm |
| 4 | USB (**[Router → Paire différentielle]**, 90 Ω) | calculateur JLCPCB |
| 5 | Caméra (faisceau, XCLK court) | 0,15-0,2 mm |
| 6 | Le reste (I2C, I2S, SPI, encodeurs) | 0,2 mm |

Puis : couche **In1.Cu** = zone `GND` pleine (**[Ctrl+Shift+Z]**),
**In2.Cu** = zones d'alimentation, zones `GND` sur F.Cu et B.Cu,
**[B]** pour remplir, vias de couture `GND` tous les ~5 mm, **zone
d'exclusion sous l'antenne** sur toutes les couches.

- [ ] Plus aucun chevelu (lignes fines) visible

### Étape 7 --- Vérifier le PCB

1. **[Inspecter → Contrôle DRC]** → 0 erreur, 0 non routé.
2. Sérigraphie : nom de chaque connecteur, broche 1, G/D, +/−.
3. **[Fichier → Exporter → STEP]** → vérifier dans la CAO du châssis.
4. **[Fichier → Imprimer]** à l'échelle 1:1, poser les vraies pièces
   dessus (connecteurs, nappe caméra, module ESP32).
5. **Pousser sur git et me demander une relecture du PCB.**

- [ ] DRC à 0, impression 1:1 OK, relecture faite

### Étape 8 --- Commander

Plugin **JLCPCB Tools** → générer Gerbers + BOM + CPL → les envoyer sur
jlcpcb.com (4 couches, 1,6 mm, assemblage face du dessus) → **vérifier
l'orientation de chaque puce** dans l'aperçu → commander 5 cartes dont 2
assemblées. Détail : `pcb/KICAD_GUIDE.md` §7.

- [ ] Commande passée 🎉

### Si tu bloques

| Problème | Solution |
|---|---|
| Le chevelu n'apparaît pas dans le PCB | Schéma pas sauvegardé, ou oubli de **[F8]** |
| « Empreinte introuvable » à **[F8]** | Bibliothèque `rover` non ajoutée au projet (étape 1.2) |
| Une zone reste vide | **[B]** pour remplir, vérifier le net de la zone |
| DRC « espacement » partout | Règles JLCPCB pas importées (étape 5.1) |
| Un symbole n'a pas la bonne forme de broches | Prendre celui d'easyeda2kicad (le vrai brochage de la pièce LCSC) |

Et à n'importe quelle étape : pousse sur git et demande-moi de regarder.
