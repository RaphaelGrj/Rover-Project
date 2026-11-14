# Commander les cartes ROVER chez JLCPCB --- guide pas à pas

L'électronique du Rover tient sur **deux cartes de 75 × 50 mm qui se
superposent** : la carte **puissance** en bas, la carte **logique** en
haut. Ce guide part des fichiers déjà générés et va jusqu'au paiement.
Aucune manipulation dans KiCad n'est nécessaire.

> **Avant de payer, lire la section 7 « Ce qui n'a pas été vérifié ».**
> Les deux cartes passent tous les contrôles automatiques de KiCad, mais
> elles n'ont jamais été fabriquées, et leurs pistes ont été tracées par
> un routeur automatique, pas par un concepteur expérimenté. Deux cartes
> assemblées coûtent de l'ordre de 150 à 300 € : faire relire le projet
> avant de commander reste prudent.

------------------------------------------------------------------------

## 1. Ce qu'il y a sur chaque carte

| | Carte **puissance** (bas) | Carte **logique** (haut) |
|---|---|---|
| Dossier | `pcb/rover_puissance/` | `pcb/rover_logique/` |
| Rôle | Batterie 2S, protection, chargeur USB-C, 5 V et 3,3 V, driver moteurs | ESP32-S3, caméra, audio, capteurs, écran |
| À brancher | Batterie, 2 moteurs, USB-C, arrêt d'urgence, sonde CTN | Nappe caméra, écran, 2 haut-parleurs, 2 capteurs de distance, servo, Qwiic |
| Composants CMS | 88, **tous sur le dessus** | 54, **sur les deux faces** |
| Aperçus | `apercu_top.png`, `apercu_bottom.png` | idem |

Les deux cartes ont les mêmes 4 trous M2 (à 2,7 mm des bords) et sont
reliées par un connecteur **2 × 15 broches au pas de 2,54 mm** (`J20`) :
embase femelle sur le dessus de la carte puissance, barrette mâle sous la
carte logique. L'écart entre cartes est d'environ **11 mm** (hauteur
standard embase 8,5 mm + barrette 2,5 mm) ; il est nécessaire, car la
carte puissance porte deux condensateurs de 10 mm de haut.

### Connecteurs

| Carte | Sérigraphie | Réf. | Broches (1 → n) |
|---|---|---|---|
| Puissance | BATTERIE 2S | J2 | B−, MID (point milieu), B+ (JST-VH coudé ; repères B+/MID/B− au dos) |
| Puissance | MOTEUR G | J3 | M1, M2, 3V3 encodeur, GND, canal A, canal B |
| Puissance | MOTEUR D (au dos) | J4 | M1, M2, 3V3 encodeur, GND, canal A, canal B |
| Puissance | STOP | J11 | arrêt d'urgence : ESTOP_N, GND |
| Puissance | CTN | J13 | sonde de température 10 kΩ des cellules |
| Puissance | (bord arrière) | J1 | USB-C : recharge, flash et console |
| Logique | HP G / HP D (au dos) | J6 / J7 | haut-parleur +, − |
| Logique | TOF G / TOF D | J8 / J9 | VIN, GND, SCL, SDA, (libre), XSHUT |
| Logique | ECRAN | J10 | GND, VCC, SCL, SDA, RES, DC, CS, BLK |
| Logique | CAMERA | J14 | nappe OV2640 24 broches, pas 0,5 mm, contacts dessous |
| Logique | SERVO | J5 | GND, 5 V, signal |
| Logique | QWIIC | J12 | GND, 3V3, SDA, SCL |

⚠ **Sans sonde CTN branchée sur `J13`, le chargeur refuse de charger**
(il croit la batterie trop froide). Pour un essai sans sonde, fermer le
cavalier de soudure `JP1` : il met une résistance fixe à la place.

## 2. Les fichiers à envoyer

Chaque carte a son dossier `fabrication/` :

| Fichier | Rôle | Où le déposer chez JLCPCB |
|---|---|---|
| `<carte>-gerber.zip` | Dessin des 4 couches, vernis, sérigraphie, perçages | « Add gerber file » |
| `<carte>-bom.csv` | Composants à souder, avec leur référence LCSC | « PCB Assembly » → « Add BOM File » |
| `<carte>-cpl.csv` | Position et rotation de chaque composant | « PCB Assembly » → « Add CPL File » |

Il faut passer **deux commandes** (une par carte), que l'on peut mettre
dans le même panier pour ne payer le transport qu'une fois.

## 3. Commander le circuit imprimé nu (à faire pour chaque carte)

Sur <https://jlcpcb.com> → **Order now** → déposer le `-gerber.zip`.
Vérifier l'aperçu : carte de 75 × 50 mm, 4 trous aux coins ; la carte
logique a en plus deux encoches sur le petit côté de l'antenne.

| Option | Valeur | Pourquoi |
|---|---|---|
| Base Material | FR-4 | Standard |
| Layers | **4** | Les deux cartes ont 4 couches |
| PCB Qty | 5 | Minimum |
| PCB Thickness | 1.6 mm | Standard, requis par le connecteur USB-C |
| Surface Finish | **ENIG** | Plus plat pour les boîtiers à pas fin (chargeur, amplis, BME688, IMU) |
| Outer Copper Weight | **1 oz** | Les isolements de 0,15 mm ne sont pas réalisables en 2 oz |
| Inner Copper Weight | 0.5 oz | Standard |
| Via Covering | Tented | Vias recouverts de vernis |
| Remove Order Number | « Specify a location » | Sinon JLCPCB imprime un numéro où il veut |

Règles de fabrication utilisées (dans les capacités standard de JLCPCB en
4 couches) : isolement 0,15 mm minimum ; pistes de 0,15 mm, avec de
courts rétrécissements à **0,09 mm** à l'arrivée sur les pastilles les
plus fines (c'est la limite basse de JLCPCB) ; perçage 0,3 mm minimum.

## 4. Ajouter l'assemblage

Activer **PCB Assembly**.

| Option | Carte puissance | Carte logique |
|---|---|---|
| PCBA Type | **Standard** (à cause des composants traversants) | **Standard** |
| Assembly Side | **Top Side** | **Both Sides** |
| PCBA Qty | 2 ou 5 | 2 ou 5 |
| Confirm Parts Placement | **Yes** | **Yes** |

Puis déposer le `-bom.csv` et le `-cpl.csv` de la carte.

### Composants traversants (pour recevoir les cartes « sans soudure »)

Trois types de pièces ne sont pas des CMS. Pour que JLCPCB les soude,
choisir pour chacune une pièce de sa bibliothèque à l'étape de la
nomenclature ; l'assemblage « Standard » les prend en charge
(supplément de soudure manuelle).

| Réf. | Carte | Pièce à choisir |
|---|---|---|
| `J20` | Puissance | Embase **femelle** 2 × 15, pas 2,54 mm, droite, hauteur 8,5 mm |
| `J20` | Logique (face dessous) | Barrette **mâle** 2 × 15, pas 2,54 mm, droite |
| `J2` | Puissance | JST **B3PS-VH** (coudé), LCSC C131336 |
| `J5` | Logique | Barrette mâle 1 × 3, pas 2,54 mm |

⚠ La barrette `J20` de la carte logique se soude **sous** la carte,
broches vers le bas. Le préciser dans le champ de remarques de la
commande (« J20 male header on BOTTOM side, pins pointing down ») et le
contrôler sur la photo de validation.

## 5. Vérifier la nomenclature (l'étape qui demande le plus d'attention)

1. **Lignes avec une référence LCSC** : puces et connecteurs relevés dans
   `AIDE_CABLAGE.md` (étape 0, relevé du 2026-10-13). Pour chacune,
   vérifier que la **référence fabricant affichée** est exactement celle
   de la colonne « Comment », et qu'elle est **en stock**.
2. **Lignes sans référence** (résistances, condensateurs, LED,
   inductances, fusibles, boutons, interrupteur, connecteur de nappe) :
   cliquer sur la loupe et choisir une pièce **« Basic »** de même
   valeur et même boîtier.

   | Composant | À respecter |
   |---|---|
   | Condensateurs « 25V » | Tension ≥ 25 V (ils sont côté batterie ou chargeur) |
   | Condensateurs 470 µF (`C45`, `C70`) | Aluminium CMS 8 × 10 mm, ≥ 16 V, **hauteur ≤ 10,5 mm** |
   | Shunt 0,05 Ω (`RS1`) | Boîtier 2512, ≥ 1 W, 1 % |
   | Résistances 0,2 Ω (`R23`, `R24`) | Boîtier 1206, ≥ 0,25 W |
   | Ponts 0 Ω (`R60`, `R61`, `R152`, `R162`) | Boîtier 1206, courant ≥ 2 A |
   | Inductances | Boîtier 4 × 4 mm (`L1`, ≥ 4 A) ou 5 × 5 mm (`L15`, `L16`, ≥ 3 A) |
   | Fusible PTC 1812 (`F1`) | Maintien ~4 A, ≥ 16 V |
   | Fusible PTC 1206 (`F2`) | Maintien 0,75 A |
   | Connecteur de nappe (`J14`) | 24 broches, pas 0,5 mm, contacts **dessous**, compatible Hirose FH12-24S-0.5SH |
   | Interrupteur (`SW1`) | Compatible C&K PCM12 (ex. MSK-12C02) : comparer le dessin des pastilles |

3. **Pièces à risque de rupture** : BQ25887RGER, BME688, LSM6DS3TR-C,
   ICS-43434. Si l'une manque, ne pas la remplacer par une pièce
   « proche » sans vérifier brochage et empreinte.

## 6. Vérifier le placement, puis payer

- **Rotation** : le point de chaque puce doit tomber sur la broche 1 de
  son empreinte. Les conventions de JLCPCB diffèrent de celles de KiCad
  pour certains boîtiers (SOT-23, QFN, condensateurs polarisés) :
  corriger dans l'aperçu ce qui est visiblement tourné. « Confirm Parts
  Placement » fait revérifier ce point par un technicien.
- **Polarité** : LED, diodes `D4` et `D5`, condensateurs `C45` et `C70`.
- **Face** : sur la carte logique, les pièces du dessous doivent
  apparaître en « Bottom ».

## 7. Ce qui n'a pas été vérifié

| Point | Détail |
|---|---|
| **Stock et références** | Les références LCSC viennent d'un relevé du 2026-10-13 fait par moteur de recherche. Aucune n'a été recontrôlée le jour de la préparation de ces fichiers. |
| **Routage** | Tracé par le routeur automatique Freerouting, puis pistes de puissance élargies par script. Les règles d'isolement sont respectées, mais la disposition des convertisseurs à découpage n'est pas optimisée comme le ferait un concepteur : bruit et échauffement possibles. |
| **Largeur des pistes de puissance** | Chemin batterie : 0,8 à 1,2 mm (le cahier des charges demandait 2 mm). Moteurs : 0,3 à 0,5 mm (demandé : 0,8 mm). Suffisant pour les courants prévus sur ces courtes distances, sans marge. Par endroits, à l'arrivée sur les broches du chargeur, la piste se resserre à 0,25 mm. |
| **Vias de puissance** | Les nets de puissance changent de couche par un seul via (0,8/0,4 mm sur le chemin batterie, 0,6/0,3 mm ailleurs). |
| **Pistes dans les plans internes** | Le routeur a fait passer environ 190 segments de signal dans les couches internes de la carte puissance (265 sur la carte logique). Le contrôle de connexion est bon, mais ces pistes entaillent les plans de masse et d'alimentation. |
| **Adresse I2C du chargeur** | La fiche technique du BQ25887 se contredit (0x6A dans le texte, 0x6B dans la table des registres). Cartes et firmware utilisent 0x6A, valeur donnée comme réelle par un signalement sur le forum TI. Si c'était 0x6B, le chargeur entrerait en collision avec l'IMU. À contrôler en premier à la réception (scan du bus I2C). |
| **Connecteur de la nappe caméra** | Empreinte Hirose FH12 ; brochage repris de `PCB_CONNEXIONS.md` §5.7, signalé là-bas comme « le point le plus risqué ». À comparer au module caméra réellement acheté. |
| **Paire USB** | D+ et D− passent par le connecteur inter-cartes et ne sont pas routées en paire d'impédance contrôlée. Sans conséquence attendue à 12 Mbit/s, non mesuré. |
| **Antenne** | Pas de cuivre sous l'antenne, sur les deux cartes. Portée WiFi à mesurer. |
| **Hauteur de l'empilement** | Calculée (11 mm d'écart pour 10,2 mm de condensateur), pas vérifiée sur un modèle 3D. |
| **Rotations d'assemblage** | Le fichier de placement sort de KiCad sans correction pour les conventions JLCPCB. |
| **Avertissements KiCad restants** | 3 pastilles de masse reliées par une seule languette thermique, et quelques textes de sérigraphie trop près d'un bord ou d'une pastille. Sans effet électrique. |
| **Matériel réel** | Rien n'a été mis sous tension. Suivre `KICAD_GUIDE.md` §9 à la réception, rail par rail grâce aux ponts `R152` (5 V) et `R162` (3,3 V). |

## 8. Écarts par rapport à `PCB_CONNEXIONS.md`

| Cahier des charges | Sur ces cartes | Raison |
|---|---|---|
| Une seule carte | Deux cartes superposées + connecteur `J20` | Une carte unique de 75 × 50 mm est trop dense : ni le routeur maison ni Freerouting n'ont pu la terminer |
| Double N-MOSFET (FS8205A) | 4 × AO3400A (2 en parallèle par position) | Brochage vérifié sur fiche technique, pièce « Basic », ~14 mΩ par position |
| Batterie : JST-VH droit ou coudé | Coudé (B3PS-VH) | Le connecteur droit ne passe pas sous la carte du haut |
| Ponts 0 Ω sur `CD` et `PSEL` du chargeur | Reliés directement à la masse | Gain de place ; le firmware peut toujours couper la charge par I2C |
| --- | Ponts 0 Ω `R60` (batterie → chargeur) et `R61` (USB → chargeur), 10 Ω `R62`/`R63` (mesure INA219), 0 Ω `R64` (mesure protection) | Séparent les tronçons fins autour des puces des pistes larges ; utiles aussi pour isoler le chargeur à la mise en service |
| 2 empreintes de gain par ampli | 1 par ampli, non montée (gain 9 dB ; la monter = 12 dB) | Gain de place |
| Empreintes de ferrite sur les haut-parleurs | Absentes | Gain de place |
| Connecteur caméra XUNPU | Empreinte Hirose FH12 | Seule empreinte vérifiable disponible |
| Flèches X/Y de l'IMU en sérigraphie | Absentes | L'IMU est posée broche 1 en haut à gauche, vue de dessus antenne à gauche |
| Trou du micro 0,8-1 mm | 0,5 mm (empreinte du fabricant) | Empreinte KiCad d'InvenSense |
| Points de test | Sur la face de dessous de chaque carte | Place |

## 9. Régénérer les cartes

Tout est produit par les scripts de `pcb/gen/` à partir de
`design_puissance.py` et `design_logique.py`. Voir `pcb/gen/README.md`.
