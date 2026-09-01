# Câblage ESP32 --- Base provisoire (Wokwi)

> **Statut : base de travail, pas une validation matérielle finale.**
> Le driver moteur (DRV8833) et les moteurs (N20 6V avec encodeur) sont
> confirmés (voir `PROGRESS.md` 2026-08-25), mais l'affectation précise
> des GPIO ci-dessous n'a **jamais été vérifiée sur un robot physique**.
> Depuis la Phase 2, ces numéros sont réellement codés en dur dans
> `esp32/include/motion_config.h` (décision explicite de continuer à
> avancer via Wokwi en attendant le câblage réel définitif --- voir
> `ARCHITECTURE_AND_ROADMAP.md` §27 et `PROGRESS.md`). Si le câblage
> réel diverge, seul `motion_config.h` doit changer.
>
> Les numéros ci-dessous ciblent la carte **ESP32 WROOM** (devkit
> 30 broches), la carte physique actuelle du robot. La cible S3 aura
> probablement un mapping différent (broches de strapping et broches
> réservées différentes) --- à revalider pin par pin le moment venu,
> pas simplement recopié.

------------------------------------------------------------------------

## Pins évitées volontairement

- `GPIO0`, `GPIO2`, `GPIO15` : broches de *strapping* (état au boot).
  Utilisées seulement pour des signaux jugés à faible risque (voir
  tableau), jamais pour un signal avec pull-up externe fort (I2C, etc.).
- `GPIO12` : broche de *strapping* (sélection de tension flash au
  boot) --- **règle assouplie le 2026-08-31**, décision explicite de
  l'utilisateur : plus aucun GPIO totalement libre sur le WROOM une
  fois tous les périphériques Phases 2-4 + E-stop/batterie/buzzer
  attribués. Utilisée pour le buzzer (voir tableau) --- un buzzer
  piezo passif est une charge haute impédance qui tire rarement assez
  fort sur la broche pendant la fenêtre de boot pour changer la
  détection de tension flash, mais **si le boot du WROOM se comporte
  bizarrement après ce câblage (mode/taille de flash mal détecté, sortie
  série très précoce corrompue), cette broche est la première suspecte**.
- `GPIO1`, `GPIO3` : UART0, réservé console/programmation USB.
- `GPIO6`--`GPIO11` : reliés à la flash SPI interne, jamais disponibles.

## Tableau de correspondance

| Fonction                        | Pin ESP32 | Remarque |
|----------------------------------|-----------|----------|
| UART2 RX (← Pi TX)               | GPIO16    | Lien Rover Protocol, cf. `ROVER_PROTOCOL.md` §2 |
| UART2 TX (→ Pi RX)               | GPIO17    | |
| Moteur gauche IN1 (avant)         | GPIO27    | DRV8833, PWM direct (pas de pin PWM séparée comme sur un TB6612FNG) |
| Moteur gauche IN2 (arrière)       | GPIO26    | DRV8833, idem |
| Moteur droit IN1 (avant)          | GPIO33    | DRV8833 |
| Moteur droit IN2 (arrière)        | GPIO32    | DRV8833 |
| DRV8833 SLP (sleep/enable)        | 3V3 direct | pas de contrôle logiciel dans cette base ; à passer sur un GPIO si un mode veille piloté est nécessaire plus tard. ⚠ **Confirmé bloquant en test matériel (2026-09-01)** : sur le breakout utilisé (pins `SLEEP`/`FAULT`/`OUT1-4`/`IN1-4`/`VCC`/`GND`), `SLEEP` non câblé ne flotte pas vers un état actif par défaut --- le driver reste désactivé en permanence (0 A consommé, aucun moteur ne répond, quoi qu'envoient IN1-4) tant que cette broche n'est pas explicitement reliée au 3V3 |
| Bouton E-stop                     | GPIO25    | `INPUT_PULLUP` (voir `esp32/lib/safety/EStop.h`) --- bouton entre cette broche et GND, pressé = LOW. **Non câblé pour l'instant** : la broche flotte HIGH (relâché) grâce au pull-up interne, le firmware fonctionne à l'identique avec ou sans bouton physique |
| Diviseur de tension batterie (ADC) | GPIO14   | ⚠ Désactivé par défaut (`ROVER_BATTERY_MONITORING_ENABLED = false`, `power_config.h`) tant que le diviseur n'est pas câblé/calibré. GPIO14 est en ADC2, illisible pendant que le WiFi est actif (OTA) --- à reconsidérer si les deux fonctions sont utilisées en même temps |
| Buzzer (bip sonore)                | GPIO12    | ⚠ Strapping, voir "Pins évitées volontairement" ci-dessus --- `esp32/lib/sound/Buzzer.h`. Buzzer passif ou actif, les deux fonctionnent avec `tone()`/`noTone()` pour un simple signal on/off rythmé |
| Encodeur gauche A                 | GPIO34    | entrée seule, pas de pull interne --- ⚠ pull-up externe **requise** (voir note ci-dessous) |
| Encodeur gauche B                 | GPIO35    | entrée seule, idem |
| Encodeur droit A                  | GPIO36    | entrée seule, idem --- marqué `VP` (ou `SVP`) sur le silkscreen de la plupart des devkits 30 broches, pas "36" |
| Encodeur droit B                  | GPIO39    | entrée seule, idem --- marqué `VN` (ou `SVN`) sur le silkscreen, pas "39" |
| Servo tête Pitch                  | GPIO13    | sortie LEDC (PWM servo) |
| Servo tête Yaw                    | GPIO19    | sortie LEDC (PWM servo) |
| Écran ST7789 SCLK                 | GPIO18    | SPI logiciel/matriciel |
| Écran ST7789 MOSI                 | GPIO23    | |
| Écran ST7789 CS                   | GPIO5     | |
| Écran ST7789 DC                   | GPIO2     | ⚠ strapping ; laisser flottant/sans pull externe pendant le boot |
| Écran ST7789 RST                  | GPIO15    | ⚠ strapping ; idem |
| Écran ST7789 BLK (rétroéclairage) | 3V3 direct | pas de contrôle logiciel dans cette base |
| I2C SDA (MPU6050, BME688, VL53L0X ×2, bus partagé) | GPIO21 | |
| I2C SCL (bus partagé)             | GPIO22    | |
| XSHUT VL53L0X gauche              | GPIO0     | ⚠ strapping ; doit rester HIGH/flottant au boot, piloté HIGH par le firmware ensuite pour l'adressage I2C séquentiel |
| XSHUT VL53L0X droite              | GPIO4     | |

Les deux `VL53L0X` partagent la même adresse I2C par défaut (0x29) : le
firmware maintient le droit en reset via son `XSHUT` pendant qu'il
réadresse le gauche (0x30) au démarrage, puis relâche le droit qui reste
sur 0x29 (implémenté en Phase 4, `esp32/lib/sensors/DistanceSensor.cpp`).

------------------------------------------------------------------------

## Câblage moteur → encodeur (N20 6 fils)

Chaque moteur N20 utilisé a 6 fils : 2 pour la puissance (vers le driver,
voir tableau ci-dessus), 4 pour l'encodeur magnétique intégré. Code
couleur utilisé (convention la plus courante pour ce type de moteur,
tension VCC/GND vérifiée au multimètre sur le matériel réel du robot ---
l'assignation exacte canal A/canal B entre jaune/vert n'a pas de
conséquence si elle est inversée, ça inverse juste le signe du comptage) :

| Fil    | Fonction          | Destination |
|--------|-------------------|-------------|
| Rouge  | Moteur +          | `OUT1`/`OUT3` du driver (selon le moteur) |
| Noir   | Moteur −          | `OUT2`/`OUT4` du driver |
| Blanc  | Encodeur VCC      | **3V3 ESP32** (pas le VCC 6V du driver --- les GPIO ne tolèrent pas le 5-6V) |
| Bleu   | Encodeur GND      | GND commun (ESP32/driver/batterie) |
| Jaune  | Encodeur canal A  | GPIO34 (gauche) / GPIO36 = `VP` (droit) |
| Vert   | Encodeur canal B  | GPIO35 (gauche) / GPIO39 = `VN` (droit) |

⚠ **Pull-up externe probablement requise sur les 4 fils de signal
(jaune/vert des deux moteurs).** Observé en test matériel (2026-09-01) :
avec VCC (3V3)/GND encodeur vérifiés corrects au multimètre et les
moteurs confirmés tournants physiquement, les deux encodeurs restaient
muets (0 tick compté) --- hypothèse retenue : GPIO34/35/36/39 (entrée
seule, aucun pull interne sur l'ESP32) et la carte encodeur de ce modèle
probablement en sortie collecteur ouvert plutôt que push-pull. Correctif
proposé, **pas encore vérifié comme ayant résolu le problème** : une
résistance de **10 kΩ entre chaque fil de signal et le 3V3** (en
parallèle du fil existant vers le GPIO, pas en série) --- 4 résistances
au total. À mettre à jour dès confirmation.

Adresses I2C par défaut des autres capteurs Phase 4 (non confirmées sur
le matériel réel, valeurs des bibliothèques utilisées) : MPU6050 = 0x68
(AD0 à la masse), BME688 = 0x77 (SDO au 3V3 -- 0x76 si SDO est à la
masse à la place). Voir `esp32/include/sensors_config.h`.

**Important pour tout nouveau capteur I2C** : chaque module Phase 4
sonde son adresse (`Wire.beginTransmission`/`endTransmission`) avant
d'appeler le `begin()` de sa bibliothèque -- premier test matériel réel
(2026-08-31) où le driver VL53L0X d'origine (l'API officielle ST,
utilisée telle quelle par `Adafruit_VL53L0X`) est resté bloqué
indéfiniment dans une boucle d'attente interne au lieu d'échouer
proprement quand rien ne répondait sur le bus, ce qui a fait planter
tout le firmware (watchdog matériel déclenché, jamais atteint `loop()`).
Voir `esp32/lib/sensors/I2CProbe.h`.

------------------------------------------------------------------------

## Utilisation avec Wokwi

- **Moteurs (Phase 2)** : `esp32/diagram.json` câble 4 LEDs (+
  résistances 220 Ω) sur les pins IN1/IN2 gauche et droite, pour
  visualiser direction/PWM sans moteur réel. Aucun encodeur n'est
  simulé (pas de chip DC-motor+encodeur câblé) : la boucle PID tourne
  donc en boucle ouverte en simulation (`left_speed`/`right_speed`
  restent à 0 dans la télémétrie `STATE`), ce n'est pas un bug.
- **Tête + écran (Phase 3)** : `esp32/diagram.json` câble aussi 2
  `wokwi-servo` (pitch/yaw) et 1 `board-st7789` (yeux). Ce chip ST7789
  simulé est **fixe en 240×240** (pas configurable via `attrs`) ---
  `ROVER_DISPLAY_WIDTH/HEIGHT` dans `display_config.h` sont alignés
  dessus, pas sur le panneau 240×280 de Lumi (voir `PROGRESS.md`).
  Limite connue : ce chip communautaire n'expose pas de framebuffer
  exploitable par `wokwi-cli` (`--screenshot-part` échoue avec "Part
  does not have a valid framebuffer") --- le rendu des yeux n'a donc pu
  être validé qu'au niveau protocole (`FACE`/`ANIMATION` acceptés sans
  crash), **pas visuellement** --- reste à vérifier à l'œil dans
  VSCode (probablement fonctionnel là où la capture headless ne
  l'est pas, mais non confirmé).
- **Capteurs (Phase 4)** : pas encore reflétés dans `diagram.json`, le
  code correspondant n'existe pas encore.
