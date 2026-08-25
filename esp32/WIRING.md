# Câblage ESP32 --- Base provisoire (Wokwi)

> **Statut : proposition, pas une décision.** Aucun de ces numéros de
> pin n'est codé en dur dans le firmware. Ce document sert de point de
> départ pour construire les prochains `diagram.json` Wokwi (Phase 2
> et suivantes) et pourra changer dès que le câblage réel du robot
> sera communiqué --- voir la règle correspondante dans
> `ARCHITECTURE_AND_ROADMAP.md` §27 et le journal `PROGRESS.md`.
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
| Moteur gauche AIN1                | GPIO27    | TB6612FNG |
| Moteur gauche AIN2                | GPIO26    | TB6612FNG |
| Moteur gauche PWMA                | GPIO25    | sortie LEDC (PWM) |
| Moteur droit BIN1                 | GPIO33    | TB6612FNG |
| Moteur droit BIN2                 | GPIO32    | TB6612FNG |
| Moteur droit PWMB                 | GPIO14    | sortie LEDC (PWM) |
| TB6612FNG STBY                    | 3V3 direct | pas de contrôle logiciel dans cette base ; à passer sur un GPIO si un mode veille piloté est nécessaire plus tard |
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

Ce tableau n'est pas encore reflété dans `diagram.json` : le firmware
actuel (Phase 1) n'utilise aucun GPIO, seulement l'UART console/USB.
Quand le code Phase 2 (moteurs) sera écrit, ajouter les pièces
correspondantes (`wokwi-tb6612`, roues/châssis simulé, etc.) dans
`diagram.json` en réutilisant ces numéros de pin comme point de départ.
