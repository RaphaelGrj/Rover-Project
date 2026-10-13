# Faire le PCB Rover avec KiCad --- guide pour débutant

> Ce guide suit l'ordre réel de travail : installer → schéma → vérifier
> → dessiner le PCB → vérifier → commander → mettre en service. Le
> **quoi brancher où** est dans `pcb/PCB_CONNEXIONS.md` : ce guide-ci
> explique seulement **comment** le faire dans KiCad.
>
> Conseil global : pour une première carte, tout vise le **« ça marche
> du premier coup »**, pas le PCB le plus petit possible. Des points de
> test, des ponts 0 Ω et un peu de place coûtent quelques millimètres,
> et évitent de devoir refaire la carte.

------------------------------------------------------------------------

## 1. Installer KiCad sur Linux Mint

Utiliser le dépôt officiel KiCad, pas la version des dépôts Mint, qui
est souvent en retard d'une version majeure :

```bash
# Remplacer X par la dernière version stable annoncée sur kicad.org (9, 10...)
sudo add-apt-repository ppa:kicad/kicad-X.0-releases
sudo apt update
sudo apt install kicad
```

Outils complémentaires, une seule fois. Dans KiCad : **Outils →
Gestionnaire de plugins et de contenus** (« Plugin and Content
Manager »), installer :
- **JLCPCB Tools** (Bouni) : associe chaque composant à sa référence
  LCSC et génère les fichiers d'assemblage ;
- **Fabrication Toolkit** (optionnel) : produit Gerbers + BOM + CPL en
  un clic.

Pour récupérer symbole + empreinte + modèle 3D d'une pièce LCSC qui
n'existe pas dans KiCad :

```bash
pip install --user easyeda2kicad
easyeda2kicad --full --lcsc_id=C123456 --output ~/kicad-libs/rover
```

(`C123456` = numéro LCSC de la pièce, affiché sur lcsc.com.)

------------------------------------------------------------------------

## 2. Les 4 notions à comprendre avant de commencer

| Notion | Ce que c'est |
|---|---|
| **Symbole** | Le dessin logique d'une puce dans le schéma (un rectangle avec des broches nommées) |
| **Empreinte** (footprint) | Les pastilles de cuivre réelles sur le PCB, aux bonnes dimensions |
| **Net** | Un « fil » logique : toutes les broches reliées entre elles. Dans le schéma, deux fils qui portent le même **label** sont reliés, même sans trait entre eux. |
| **Chevelu** (ratsnest) | Dans l'éditeur PCB, les fines lignes droites qui montrent ce qui reste à relier. On les transforme une par une en vraies pistes. |

Le flux KiCad est toujours : **schéma → associer une empreinte à chaque
symbole → « Mettre à jour le PCB depuis le schéma » → placer → router**.

------------------------------------------------------------------------

## 3. Créer le projet

1. **Fichier → Nouveau projet**, dans `Rover-Project/pcb/rover-main/`.
   Le versionner avec git comme le reste : les fichiers KiCad sont du
   texte. Ignorer les sauvegardes `*-backups/` et `fp-info-cache`.
2. **Préférences → Gérer les bibliothèques de symboles/empreintes** :
   ajouter le dossier `~/kicad-libs/rover` (créé par easyeda2kicad),
   portée « projet ».

------------------------------------------------------------------------

## 4. Le schéma (Éditeur de schéma)

### 4.1 Découper en feuilles hiérarchiques

Un seul grand schéma devient vite illisible. Créer **une feuille par
bloc** (outil « Feuille hiérarchique »), dans le même ordre que
`PCB_CONNEXIONS.md` §5 :

1. `alimentation` --- connecteurs pack (puissance + équilibrage), protection S-8252,
   chargeur BQ25887, fusible, INA219, interrupteur, bucks, LDO caméra
2. `mcu` --- ESP32-S3, USB-C, boutons, TCA9554
3. `camera` --- connecteur FPC
4. `moteurs` --- DRV8833, connecteurs moteurs, E-stop
5. `audio` --- 2 × MAX98357A, micro, connecteurs HP
6. `capteurs` --- IMU, BME688, connecteurs ToF, Qwiic
7. `ihm` --- connecteur écran, servo, LED, points de test

Entre les feuilles, relier par **labels globaux** (`SDA`, `SCL`,
`I2S_BCLK`, `ESTOP_N`, `AMP_EN`...) et par les **symboles
d'alimentation** (`+3V3`, `+5V`, `VBAT_SYS`, `VPACK`, `GND`). Utiliser exactement
les noms de net de `PCB_CONNEXIONS.md` : on pourra comparer ligne à
ligne.

### 4.2 Où trouver les symboles

| Composant | Dans KiCad |
|---|---|
| ESP32-S3-WROOM-1 | `RF_Module:ESP32-S3-WROOM-1`, déjà fourni |
| USB-C | `Connector:USB_C_Receptacle_USB2.0_16P` |
| Connecteurs JST | `Connector_Generic:Conn_01x0N` + empreinte `Connector_JST:JST_PH_B6B-PH-K...` (PH vertical) ou `S6B-PH` (coudé) |
| Servo / barrette | `Connector_Generic:Conn_01x03` + empreinte `PinHeader_1x03_P2.54mm` |
| DRV8833, MAX98357A, ICS-43434, LSM6DS3TR-C, BME688, INA219, TCA9554, FPC 24 broches, buck | Chercher par nom dans le sélecteur (touche **A**) ; sinon easyeda2kicad avec la référence LCSC |

### 4.3 Méthode qui évite 90 % des erreurs

- **Recopier le schéma d'application de la datasheet** pour chaque puce
  (buck, DRV8833, MAX98357A...) avant d'y brancher quoi que ce soit.
  Tout ce qui est dans la datasheet est là pour une raison.
- **Un condensateur de découplage de 100 nF par broche
  d'alimentation**, dessiné à côté de la puce qu'il sert : au placement,
  il devra être collé à elle.
- Remplir le champ **Valeur** et la **référence LCSC** de chaque
  composant (via le plugin JLCPCB Tools). L'assemblage en dépend.
- Mettre un **symbole `PWR_FLAG`** sur chaque rail qui sort d'un
  connecteur ou d'un régulateur (`VPACK`, `VBAT_SYS`, `+5V`, `+3V3`, `GND`), sinon
  le contrôle électrique signale des « entrées d'alimentation non
  alimentées ».
- Broche volontairement inutilisée (ex. `Y0`/`Y1` caméra) : poser un
  **marqueur « non connecté »** (touche **Q**) pour dire « c'est voulu ».

### 4.4 Vérifier le schéma

1. **Inspecter → Contrôle des règles électriques (ERC)**. Corriger
   jusqu'à 0 erreur et lire chaque avertissement.
2. **Relecture croisée** : ouvrir `PCB_CONNEXIONS.md` §4 (table des
   GPIO) et vérifier chaque ligne dans le schéma en cliquant sur le net
   (il se surligne).
3. 💡 Pousser le projet sur une branche git : **je peux relire le
   schéma** (fichiers `.kicad_sch`) contre la liste des connexions et
   les datasheets avant que tu passes au PCB. C'est le moment où une
   erreur coûte le moins cher.

------------------------------------------------------------------------

## 5. Associer les empreintes

**Outils → Assigner les empreintes**. Pour chaque symbole :
- résistances et condensateurs : **0603** (`Resistor_SMD:R_0603_1608Metric`),
  assez petit pour gagner de la place et assez grand pour être
  réparable au fer ;
- condensateurs électrolytiques 470 µF : boîtier CMS ou traversant,
  selon la hauteur disponible dans le châssis ;
- shunt INA219 : 2512 ;
- connecteurs : **vérifier le pas au pied à coulisse sur la pièce
  réelle** (PH = 2,0 mm, ZH = 1,5 mm, SH = 1,0 mm).

------------------------------------------------------------------------

## 6. Le PCB (Éditeur de PCB)

### 6.1 Contour et réglages de départ

1. **Fichier → Configuration de la carte → Empilement** : 4 couches.
   Puis **Règles de conception** : importer les règles JLCPCB
   (piste/espace mini 0,127 mm, via 0,3/0,6 mm en 4 couches). Les
   valeurs exactes sont sur la page « capabilities » de JLCPCB.
2. **Contour** : dans la CAO du châssis, exporter la forme du support du
   PCB en **DXF**, puis **Fichier → Importer → Graphiques** sur la
   couche `Edge.Cuts`. Ajouter les **trous de fixation** (empreinte
   `MountingHole_3.2mm_M3` pour du M3).
3. **Outils → Mettre à jour le PCB depuis le schéma** (F8) : tous les
   composants apparaissent en tas, reliés par le chevelu.

### 6.2 Placement (le plus important, et le plus long)

Placer **avant** de router, dans cet ordre :

1. **Ce qui est imposé par la mécanique** : connecteurs à l'endroit où
   arrivent les câbles (moteurs côté moteurs, HP gauche à gauche, HP
   droit à droite, FPC caméra côté tête, USB-C accessible coque fermée,
   connecteurs du pack LiPo, interrupteur accessible), trous de fixation.
2. **ESP32-S3 en bord de carte**, antenne dépassant du bord ou au-dessus
   d'une zone **sans cuivre sur les 4 couches** (zone d'exclusion
   dessinée dans l'empreinte).
3. **Alimentation regroupée dans un coin**, loin de l'antenne et du
   BME688. Boucle d'entrée de chaque buck (condensateur d'entrée →
   puce → GND) **la plus courte possible** : la datasheet montre un
   exemple de placement, le copier.
4. **DRV8833 près des connecteurs moteur**, entre l'arrivée batterie et
   les moteurs. Les gros courants ne doivent pas traverser la zone
   ESP32/capteurs.
5. **BME688 dans un coin isolé** avec sa fente (`PCB_CONNEXIONS.md`
   §5.6), **IMU au centre**, **micro loin** des moteurs et des HP.
6. **Découplages collés** à la broche qu'ils servent.

Astuce : la **vue 3D** (Alt+3) montre tout de suite un connecteur à
l'envers ou un composant trop haut. **Exporter en STEP** (Fichier →
Exporter → STEP) et importer le PCB dans la CAO du châssis pour
vérifier l'encombrement **avant** de commander.

### 6.3 Empilement 4 couches conseillé

| Couche | Usage |
|---|---|
| F.Cu (dessus) | Composants + signaux |
| In1.Cu | **Plan de masse GND plein, sans coupure** |
| In2.Cu | Plans d'alimentation (`3V3`, `5V`, `VBAT_SYS` en zones) |
| B.Cu (dessous) | Signaux restants |

Un plan GND intact juste sous les signaux fait la moitié du travail en
compatibilité électromagnétique. **Ne jamais le découper** par des
pistes.

### 6.4 Routage (ordre conseillé)

1. **Puissance** (`B+`/`B−`, `VPACK`, `VBAT_SYS`, sorties moteur, HP) : pistes larges. Repère :
   ~1 mm par ampère en couche externe (calculateur intégré : **Outils →
   Calculateur → Largeur de piste**). Batterie → DRV8833 : ≥ 1,5 mm,
   ou une zone de cuivre.
2. **USB D+/D−** : **Router → Paire différentielle**, impédance 90 Ω
   (le calculateur JLCPCB donne la largeur pour leur empilement),
   piste courte, sans via si possible.
3. **Caméra** : les 8 données + PCLK + HREF + VSYNC en faisceau,
   longueurs proches, XCLK le plus court possible et loin des données.
4. **I2S, SPI écran** : courts, chacun au-dessus du plan GND.
5. **Le reste** (I2C, encodeurs, signaux lents).
6. **Remplissage** : zones GND sur F.Cu et B.Cu reliées au plan interne
   par des vias tous les ~5 mm (« via stitching »), en évitant la zone
   d'antenne.

### 6.5 Sérigraphie (elle sert à la mise en service)

Pour chaque connecteur : son nom et le **repère broche 1**. Pour les
paires symétriques : **G / D** (moteurs, HP, ToF). Polarité batterie
`+`/`−`. Axes X/Y de l'IMU. Nom de chaque point de test. Version de la
carte et date.

### 6.6 Vérifier le PCB

1. **Inspecter → Contrôle des règles de conception (DRC)** : 0 erreur,
   **0 net non routé**.
2. Vue 3D, puis export STEP vers la CAO (§6.2).
3. **Imprimer à l'échelle 1:1** sur papier, poser les vrais connecteurs,
   la nappe caméra et le module ESP32-S3 dessus : on voit tout de suite
   une empreinte fausse.
4. 💡 Pousser sur git : je peux faire une revue du `.kicad_pcb`
   (largeurs de pistes de puissance, zone d'antenne, découplages,
   cohérence avec `PCB_CONNEXIONS.md`).

------------------------------------------------------------------------

## 7. Commander chez JLCPCB (fabrication + assemblage)

1. Plugin **JLCPCB Tools** (ou Fabrication Toolkit) → génère :
   **Gerbers + perçages (.zip)**, **BOM (.csv)**, **CPL / positions
   (.csv)**.
2. Sur jlcpcb.com : envoyer le zip, choisir 4 couches, épaisseur 1,6 mm.
   Activer **« PCB Assembly »**, face du dessus, puis envoyer BOM + CPL.
3. **Contrôler l'aperçu d'assemblage** : la rotation des composants est
   la première source d'erreur (puce tournée de 90°/180°, LED ou diode
   inversée). Le plugin corrige la plupart des cas, mais vérifier puce
   par puce le repère broche 1.
4. Commander **5 PCB dont 2 assemblés** : une carte de rechange si la
   première souffre pendant la mise en service.
5. Souder soi-même ce qui est traversant (queue de cochon batterie,
   barrette servo), voire
   les connecteurs JST si l'assembleur ne les a pas.

------------------------------------------------------------------------

## 8. Pièges classiques de première carte

| Piège | Parade |
|---|---|
| Empreinte de connecteur en miroir | Vérifier en vue 3D + impression 1:1 avec la vraie pièce |
| Mauvais côté de contact du FPC caméra | `PCB_CONNEXIONS.md` §5.7, tester avec la nappe réelle |
| Broche 1 d'une puce mal orientée à l'assemblage | Contrôle de l'aperçu JLCPCB, puce par puce |
| CC1/CC2 USB-C sans résistances 5,1 kΩ | Un chargeur USB-C ne donne alors pas de 5 V |
| Plan de masse sous l'antenne | Zone d'exclusion sur toutes les couches |
| Pull-up sur GPIO45/46 | Le S3 ne démarre plus (broches de strapping, `PCB_CONNEXIONS.md` §4) |
| Pas de pastille sous le pad thermique DRV8833/MAX98357A | Le driver chauffe et se coupe en protection thermique |
| Capteur de température collé au régulateur | Fente d'isolation + placement en bord de carte |
| `B−` du pack relié directement à `GND` | Toute la protection batterie est court-circuitée : `B−` ne rejoint `GND` **qu'à travers** les MOSFET de protection (`PCB_CONNEXIONS.md` §5.1) |
| Inductance du chargeur mal placée | Boucle `SW` → inductance → condensateurs la plus courte possible, copier le placement de la datasheet / carte d'évaluation BQ25887EVM |

------------------------------------------------------------------------

## 9. Mise en service de la carte reçue (pas à pas)

Utiliser l'**alimentation de labo** (`BOM.md`) à la place de la
batterie : on limite le courant, donc une erreur ne brûle rien.

⚠ Avec le BMS intégré, la carte refuse de démarrer sans point milieu
(`PCB_CONNEXIONS.md` §5.1). Fabriquer une **« fausse batterie »** :
l'alim de labo sur le connecteur de puissance, plus une prise JST-XH
3 broches dont le fil du milieu sort d'un pont de deux résistances
1 kΩ entre `+` et `−`. **Ne pas brancher l'USB** pendant ces essais
(le chargeur essaierait de « charger » l'alim de labo).

1. **À vide, sans rien brancher** : à l'ohmmètre, vérifier qu'il n'y a
   pas de court-circuit entre `VPACK`/`VBAT_SYS`/`5V`/`3V3` et GND, et entre `B+` et `B−`.
2. Ponts 0 Ω des bucks **ouverts**, interrupteur sur ON. Alim labo à
   **7,4 V, limite 200 mA**, via la fausse batterie. Vérifier d'abord
   que `GND` ↔ `B−` ≈ 0 V (protection passante), puis mesurer la sortie
   de chaque buck à vide : 5,0 V et 3,3 V. Interrupteur sur OFF : plus
   rien en aval.
3. Fermer le pont `3V3` → le témoin s'allume. Brancher l'USB-C : la
   carte doit apparaître (`/dev/ttyACM0`), puis flasher un firmware de
   test.
4. `SYSTEM action=i2c_scan` → doit lister **0x20, 0x40, 0x6A (chargeur),
   0x6B (IMU), 0x76**
   (puis 0x29/0x30 une fois les ToF branchés).
5. Fermer le pont `5V`, monter la limite à 1 A, puis tester dans
   l'ordre : servo, son, écran, caméra.
6. **Moteurs en dernier**, roues en l'air, avec `pi/tools/motor_triage.py`.
   Vérifier qu'un appui sur l'E-stop coupe **réellement** les moteurs,
   même firmware planté.
7. Seulement ensuite, passer sur la **vraie batterie** (les deux
   prises).
8. **Charge**, pack branché, interrupteur OFF, à côté de la carte :
   brancher l'USB-C → LED orange « en charge ». Lire en I2C la tension
   des deux cellules et le courant (négatif sur l'INA219). Toucher le
   chargeur et le pack de temps en temps pendant la première charge
   complète : ils doivent rester tièdes.

À chaque étape validée, la noter dans `PROGRESS.md`, puis mettre à jour
`AIDE_CABLAGE.md` (règle `CLAUDE.md`).
