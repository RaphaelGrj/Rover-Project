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

- `GPIO0`, `GPIO2`, `GPIO12`, `GPIO15` : broches de *strapping* (état
  au boot). Utilisées seulement pour des signaux jugés à faible risque
  (voir tableau), jamais pour un signal avec pull-up externe fort
  (I2C, etc.). `GPIO12` en particulier n'est utilisée nulle part
  (risque de sélection de tension flash incorrecte au boot).
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
| DRV8833 SLP (sleep/enable)        | 3V3 direct | pas de contrôle logiciel dans cette base ; à passer sur un GPIO si un mode veille piloté est nécessaire plus tard |
| Bouton E-stop                     | GPIO25    | `INPUT_PULLUP` (voir `esp32/lib/safety/EStop.h`) --- bouton entre cette broche et GND, pressé = LOW. **Non câblé pour l'instant** : la broche flotte HIGH (relâché) grâce au pull-up interne, le firmware fonctionne à l'identique avec ou sans bouton physique |
| Diviseur de tension batterie (ADC) | GPIO14   | ⚠ Désactivé par défaut (`ROVER_BATTERY_MONITORING_ENABLED = false`, `power_config.h`) tant que le diviseur n'est pas câblé/calibré. GPIO14 est en ADC2, illisible pendant que le WiFi est actif (OTA) --- à reconsidérer si les deux fonctions sont utilisées en même temps |
| Encodeur gauche A                 | GPIO34    | entrée seule (pas de pull interne, prévoir pull-up externe si besoin) |
| Encodeur gauche B                 | GPIO35    | entrée seule |
| Encodeur droit A                  | GPIO36    | entrée seule |
| Encodeur droit B                  | GPIO39    | entrée seule |
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
