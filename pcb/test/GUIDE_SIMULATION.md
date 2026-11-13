# Tester les cartes ROVER sans les avoir fabriquées

Trois niveaux de test sont possibles avant de commander. Aucun ne remplace
la mise en service de la carte réelle (`pcb/KICAD_GUIDE.md` §9) : ils
servent à éliminer les erreurs qu'on peut trouver depuis un ordinateur.

| Niveau | Ce qu'il vérifie | Ce qu'il ne vérifie pas |
|---|---|---|
| 1. Cohérence cartes ↔ firmware | Chaque GPIO câblé est bien celui que le firmware utilise ; les deux cartes se raccordent broche à broche | Le comportement électrique |
| 2. Compilation et tests unitaires | Le firmware de la carte compile, sa logique pure est juste | Tout ce qui touche au matériel |
| 3. Simulation Wokwi | Démarrage, protocole série, PWM moteurs, servo, écran, arrêt d'urgence | Chargeur, batterie, caméra, audio, capteurs I2C de la carte, alimentation, routage |

**Ce qu'aucune simulation ne couvre ici** : l'électronique de puissance
(chargeur BQ25887, protection S-8252, convertisseurs 5 V / 3,3 V), le
tracé des pistes, l'échauffement, le bruit, l'antenne. Il n'existe pas de
modèle de simulation pour ces puces dans ce projet. Ces points se
valident sur la carte réelle, rail par rail, grâce aux ponts 0 Ω
(`R152`, `R162`) et aux points de test de la face de dessous.

------------------------------------------------------------------------

## Niveau 1 --- cohérence carte ↔ firmware (10 secondes)

Deux scripts. Le premier compare les descriptions des deux cartes
(`pcb/gen/design_logique.py`, `design_puissance.py`) à
`esp32/include/board_pins_pcb_v1.h` (ce que le firmware croit câblé), et
vérifie que chaque signal partagé traverse bien le connecteur
inter-cartes :

```bash
python3 pcb/test/check_pins.py
```

Résultat attendu : `108 verifications, 0 ecart(s)`.

Le second lit les deux fichiers KiCad et vérifie que chaque broche du
connecteur inter-cartes tombe exactement en face de son homologue, avec
le même signal, et que les trous de fixation coïncident. Il a besoin du
Python de KiCad :

```
"C:\Program Files\KiCad.0in\python.exe" pcb	est\check_connecteur.py
```

Résultat attendu : `30 broches comparees, 4 trous, 0 ecart(s)`.

À relancer **à chaque modification** du brochage, d'un côté ou de l'autre.
Sous Windows, sans Python installé, utiliser celui de KiCad :
`"C:\Program Files\KiCad\10.0\bin\python.exe" pcb\test\check_pins.py`.

## Niveau 2 --- compilation et tests unitaires (2 minutes)

```bash
cd esp32
pio run -e rover_pcb_v1      # firmware de la carte : doit finir par SUCCESS
pio test -e native           # tests de logique pure : 54 tests, tous verts
```

Résultat relevé le 2026-11-11 : `rover_pcb_v1 SUCCESS`, flash utilisée
15 %, `54 test cases: 54 succeeded`.

## Niveau 3 --- simulation Wokwi (firmware sur un ESP32-S3 virtuel)

Le projet utilise déjà Wokwi pour la carte WROOM (`esp32/diagram.json`).
Le dossier `pcb/test/wokwi/` contient l'équivalent pour les cartes PCB :
un ESP32-S3 virtuel câblé avec **le brochage du PCB** (la simulation ne
connaît pas la séparation en deux cartes, seulement les GPIO).

> **Statut : fichiers préparés, simulation non exécutée.** Elle n'a pas pu
> être lancée depuis la session qui a écrit ce guide (Wokwi demande
> l'extension VS Code et une licence activée). Le premier lancement est
> donc aussi le premier essai de ces deux fichiers : voir « Si ça ne
> démarre pas » plus bas.

### Lancer la simulation

1. Installer l'extension **Wokwi Simulator** dans VS Code (comme pour la
   simulation WROOM) et activer la licence (`F1` → « Wokwi: Request a
   new License »).
2. Compiler le firmware de la carte :
   ```bash
   cd esp32
   pio run -e rover_pcb_v1
   ```
3. Dans VS Code, ouvrir le dossier `pcb/test/wokwi/` puis `F1` →
   « Wokwi: Start Simulator ».

### Ce qui est simulé

| Élément de la carte | Dans la simulation | GPIO |
|---|---|---|
| Entrées du driver moteur DRV8833 | 4 LED (verte = IN1, rouge = IN2) dont la luminosité suit le PWM | 47, 48, 45, 46 |
| Servo de tête | Servo Wokwi | 37 |
| Écran ST7789 | Écran Wokwi (240 × 240, la vraie dalle fait 240 × 280) | 42, 43, 44 |
| Bouton d'arrêt d'urgence | Bouton poussoir vers GND | 3 |
| Console / Rover Protocol | Moniteur série Wokwi (USB natif du S3) | 19, 20 |

### Ce qui manque, et ce que le firmware en dit

Wokwi n'a pas de modèle pour ces puces. Le firmware est écrit pour
tourner sans elles et le signale :

| Absent de la simulation | Message attendu |
|---|---|
| Expandeur TCA9554 | `ERROR sensor=io_expander`, nouvel essai toutes les 5 s |
| INA219, chargeur BQ25887 | pas de télémétrie batterie |
| IMU LSM6DS3, BME688, VL53L0X | capteurs signalés absents |
| Caméra OV2640 | pas de flux vidéo |
| Encodeurs | les moteurs « tournent » sans retour de vitesse (boucle ouverte) |
| Amplis, micro | aucun pilote audio n'existe encore dans le firmware |

Conséquence de l'absence d'expandeur : le reset de l'écran et le
rétroéclairage, pilotés par l'expandeur sur la vraie carte, sont câblés
en dur au 3,3 V dans la simulation.

### Essais à faire

Dans le moniteur série, envoyer des trames du Rover Protocol
(`ROVER_PROTOCOL.md`) :

1. **Démarrage** : la bannière de démarrage s'affiche, le robot passe en
   `READY`.
2. **Moteurs** : une commande `MOVE` vers l'avant allume les deux LED
   vertes ; en arrière, les deux rouges ; en rotation, une verte et une
   rouge.
3. **Chien de garde** : arrêter d'envoyer le battement de cœur ; au bout
   de 500 ms les 4 LED s'éteignent et le robot passe en `SAFE`.
4. **Arrêt d'urgence** : appuyer sur le bouton rouge ; les LED
   s'éteignent et l'événement est signalé.
5. **Servo** : une commande `HEAD` fait tourner le servo.
6. **Écran** : les yeux s'affichent.

### Si ça ne démarre pas

- **Rien dans le moniteur série** : sur un S3, la console passe par
  l'USB natif. Le fichier `diagram.json` le demande avec
  `"serialInterface": "USB_SERIAL_JTAG"` ; si votre version de Wokwi ne
  connaît pas cet attribut, le retirer et relier à la place `esp:TX` et
  `esp:RX` à `$serialMonitor` (il faut alors débrancher l'écran de ces
  deux broches, GPIO43/44 étant partagées avec lui sur la carte).
- **« Firmware not found »** : le firmware n'a pas été compilé (étape 2),
  ou le dossier ouvert dans VS Code n'est pas `pcb/test/wokwi/`.
- **Nom de broche refusé** (`esp:47`...) : ouvrir le composant dans
  l'éditeur Wokwi pour lire le nom exact des broches de la carte S3 de
  votre version, et corriger `diagram.json`.
