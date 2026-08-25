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
| *(libres)*                        | GPIO14, GPIO25 | anciennement PWMA/PWMB dans le plan TB6612FNG, plus nécessaires avec le DRV8833 |
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
firmware devra maintenir un capteur en reset via son `XSHUT` pendant
qu'il réadresse l'autre au démarrage (séquence classique, à implémenter
en Phase 4).

------------------------------------------------------------------------

## Utilisation avec Wokwi

- **Moteurs (Phase 2)** : `esp32/diagram.json` câble 4 LEDs (+
  résistances 220 Ω) sur les pins IN1/IN2 gauche et droite, pour
  visualiser direction/PWM sans moteur réel. Aucun encodeur n'est
  simulé (pas de chip DC-motor+encodeur câblé) : la boucle PID tourne
  donc en boucle ouverte en simulation (`left_speed`/`right_speed`
  restent à 0 dans la télémétrie `STATE`), ce n'est pas un bug.
- **Tête / écran / capteurs (Phases 3--4)** : pas encore reflétés dans
  `diagram.json`, le code correspondant n'existe pas encore. Ajouter
  les pièces au fur et à mesure en réutilisant ces numéros de pin comme
  point de départ.
