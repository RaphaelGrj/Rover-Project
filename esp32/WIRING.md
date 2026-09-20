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
- `GPIO1`, `GPIO3` : UART0, console/programmation USB. ⚠ **Statut changé
  le 2026-09-15** --- ces deux broches étaient les seules non attribuées
  du WROOM, et elles l'étaient parce qu'elles portaient le câble USB
  vers le Raspberry Pi. Le Pi étant désormais déporté sur le réseau
  (`ARCHITECTURE_AND_ROADMAP.md` §6.2), ce câble n'existe plus et
  `GPIO3` devient utilisable pour le micro I2S. Voir la section
  « Micro I2S » ci-dessous avant d'y toucher : ce n'est pas gratuit.
- `GPIO6`--`GPIO11` : reliés à la flash SPI interne, jamais disponibles.

## Tableau de correspondance

| Fonction                        | Pin ESP32 | Remarque |
|----------------------------------|-----------|----------|
| Ampli I2S (MAX98357A) BCLK        | GPIO16    | ⚠ Pin réaffectée le 2026-09-13 --- prévue à l'origine pour une UART2 vers le Pi (`ROVER_PROTOCOL.md` §2), jamais utilisée en pratique : le lien réel Pi↔ESP32 était alors le câble USB (`/dev/ttyUSB0`, UART0/GPIO1-3), confirmé lors du bring-up matériel. Récupérée pour l'ampli faute de GPIO libre restant. *(Ce câble USB n'existe plus depuis le 2026-09-15 --- Pi déporté en WiFi, voir plus haut ; la réaffectation reste valable.)* |
| Ampli I2S (MAX98357A) LRC (WS)    | GPIO17    | idem, voir remarque ci-dessus |
| Ampli I2S (MAX98357A) DIN         | GPIO14    | ⚠ Pin réaffectée le 2026-09-13, récupérée sur le monitoring batterie (jamais câblé, désactivé par défaut --- voir note GPIO14 obsolète ci-dessous et `power_config.h`) |
| Moteur gauche IN1 (avant)         | GPIO27    | DRV8833, PWM direct (pas de pin PWM séparée comme sur un TB6612FNG) |
| Moteur gauche IN2 (arrière)       | GPIO26    | DRV8833, idem |
| Moteur droit IN1 (avant)          | GPIO33    | DRV8833 |
| Moteur droit IN2 (arrière)        | GPIO32    | DRV8833 |
| DRV8833 SLP (sleep/enable)        | 3V3 direct | pas de contrôle logiciel dans cette base ; à passer sur un GPIO si un mode veille piloté est nécessaire plus tard. ⚠ **Confirmé bloquant en test matériel (2026-09-01)** : sur le breakout utilisé (pins `SLEEP`/`FAULT`/`OUT1-4`/`IN1-4`/`VCC`/`GND`), `SLEEP` non câblé ne flotte pas vers un état actif par défaut --- le driver reste désactivé en permanence (0 A consommé, aucun moteur ne répond, quoi qu'envoient IN1-4) tant que cette broche n'est pas explicitement reliée au 3V3 |
| Bouton E-stop                     | GPIO25    | `INPUT_PULLUP` (voir `esp32/lib/safety/EStop.h`) --- bouton entre cette broche et GND, pressé = LOW. **Non câblé pour l'instant** : la broche flotte HIGH (relâché) grâce au pull-up interne, le firmware fonctionne à l'identique avec ou sans bouton physique |
| Diviseur de tension batterie (ADC) | **libre / à réassigner** | ⚠ GPIO14 (utilisé jusqu'ici comme placeholder) a été réaffecté à l'ampli I2S le 2026-09-13 (voir tableau ci-dessus) --- monitoring toujours désactivé par défaut (`ROVER_BATTERY_MONITORING_ENABLED = false`, `power_config.h`), aucun diviseur câblé à ce jour. Si ce monitoring est remis en service plus tard, il faudra choisir un GPIO ADC1 libre (les ADC2 comme GPIO14 sont illisibles pendant le WiFi/OTA) --- **toujours bloqué au 2026-09-15** : le déport du Pi libère `GPIO1`/`GPIO3`, mais ni l'une ni l'autre n'est sur l'ADC1, et les 6 broches ADC1 du WROOM (GPIO32/33 moteurs, GPIO34/35/36/39 encodeurs) sont toutes prises. Il faudra donc toujours en libérer une |
| Buzzer (bip sonore)                | GPIO12    | ⚠ Strapping, voir "Pins évitées volontairement" ci-dessus --- `esp32/lib/sound/Buzzer.h`. Buzzer passif ou actif, les deux fonctionnent avec `tone()`/`noTone()` pour un simple signal on/off rythmé |
| Encodeur gauche A                 | GPIO34    | entrée seule, pas de pull interne --- ⚠ pull-up externe **requise** (voir note ci-dessous) |
| Encodeur gauche B                 | GPIO35    | entrée seule, idem |
| Encodeur droit A                  | GPIO36    | entrée seule, idem --- marqué `VP` (ou `SVP`) sur le silkscreen de la plupart des devkits 30 broches, pas "36" |
| Encodeur droit B                  | GPIO39    | entrée seule, idem --- marqué `VN` (ou `SVN`) sur le silkscreen, pas "39" |
| Servo tête (unique)               | GPIO13    | sortie LEDC (PWM servo). **Seul servo de tête depuis le 2026-09-20** : un axe mécanique unique, piloté par `pitch` (`yaw` est inerte, voir `head_config.h`). Origine mécanique persistée en NVS (`SYSTEM action=head_origin`), donc un remontage de palonnier se rattrape sans reflasher |
| ~~Servo tête Yaw~~                | ~~GPIO19~~ | ❌ **GPIO19 NE FONCTIONNE PAS sur ce devkit** (constaté 2026-09-20). Établi par élimination : les deux servos fonctionnent sur GPIO13, aucun ne fonctionne sur GPIO19, et changer de canal LEDC (5 → 6, donc de timer) n'y change rien alors que le firmware rapporte le canal attaché. Pad mort ou non connecté. **2e anomalie de cette carte** après GPIO0. Ne pas réutiliser cette broche |
| *(2e servo tête --- supprimé)*     | *GPIO12*  | Le montage tandem (2 servos face à face sur un axe commun) a été **abandonné le 2026-09-20** au profit d'**un seul servo + un roulement de maintien** : deux servos sur un même axe ne sont jamais d'accord et se combattent. GPIO12 (ex-buzzer) avait été réquisitionné pour ce 2e servo, **il est donc redevenu libre** --- le buzzer peut y revenir (`ROVER_BUZZER_ENABLED`, `sound_config.h`) |
| Écran ST7789 SCLK                 | GPIO18    | SPI logiciel/matriciel |
| Écran ST7789 MOSI                 | GPIO23    | |
| Écran ST7789 CS                   | GPIO5     | |
| Écran ST7789 DC                   | GPIO2     | ⚠ strapping ; laisser flottant/sans pull externe pendant le boot |
| Écran ST7789 RST                  | GPIO15    | ⚠ strapping ; idem |
| Écran ST7789 BLK (rétroéclairage) | 3V3 direct | pas de contrôle logiciel dans cette base |
| I2C SDA (MPU6050, BME688, VL53L0X ×2, bus partagé) | GPIO21 | |
| I2C SCL (bus partagé)             | GPIO22    | |
| XSHUT VL53L0X gauche              | **3V3 direct** | ⚠ **Corrigé le 2026-09-20 sur matériel réel.** Prévu sur `GPIO0` jusque-là --- **impossible : le devkit WROOM 30 broches ne sort pas GPIO0** (rangée réelle : `GND, 15, 2, 4, RX2, TX2`, aucun 0 ; GPIO0 reste interne, relié au bouton BOOT et au circuit d'auto-reset). C'est pourquoi ces capteurs sont restés non câblés du 2026-09-02 au 2026-09-20. Aucune perte : séparer deux capteurs partageant l'adresse d'usine ne demande d'en éteindre qu'**un seul** (voir ci-dessous) |
| XSHUT VL53L0X droite              | GPIO4     | seule ligne XSHUT pilotée ; maintenue LOW le temps que le gauche soit réadressé | |

## Micro I2S --- planifié, pas encore câblé (2026-09-15)

Le micro est resté « non décidé » (`BOM.md`) tout au long des Phases 2-4
pour une raison simple : **il ne restait aucune broche**. Le déport du
Raspberry Pi (`ARCHITECTURE_AND_ROADMAP.md` §6.2) débloque la situation.

Un micro I2S type **INMP441** peut partager l'horloge de l'ampli
MAX98357A en mode full-duplex sur I2S0 --- il ne réclame donc qu'**une
seule broche supplémentaire** :

| Fonction | Pin ESP32 | Remarque |
|---|---|---|
| Micro I2S BCLK | GPIO16 | **partagé** avec l'ampli (même horloge) |
| Micro I2S WS (LRC) | GPIO17 | **partagé** avec l'ampli |
| Micro I2S SD (data out du micro) | GPIO3 | ⚠ ex-UART0 RX, voir l'avertissement ci-dessous |
| Micro L/R (sélection canal) | GND ou 3V3 direct | pas de GPIO nécessaire |

`GPIO3` plutôt que `GPIO1` : c'est une **entrée**, ce qui correspond au
sens du signal (data out du micro → entrée ESP32), et contrairement à
`GPIO1`/TX elle n'émet pas le log de boot de l'ESP32.

### ⚠ Le coût : plus de console série USB

Occuper `GPIO3` revient à perdre la console série filaire. Le scénario
gênant est identifié et assumé : **si le WiFi tombe, on perd le
pilotage *et* le moyen de déboguer au même instant.**

**Mitigation retenue avec l'utilisateur le 2026-09-15 : un cavalier
(jumper) sur la piste du micro**, à retirer pour flasher ou déboguer en
filaire, la CAO étant adaptée pour le rendre accessible sans démonter le
robot. Ne pas souder ce fil en dur.

En usage normal, la mise à jour du firmware passe par l'**OTA**
(`esp32/OTA.md`, validée sur matériel réel) --- le cavalier n'est qu'un
filet de sécurité pour le jour où l'OTA ne répond plus.

------------------------------------------------------------------------

## Raspberry Pi --- plus aucun câble (2026-09-15)

Il n'y a plus de liaison filaire entre l'ESP32 et le Raspberry Pi. Le
Rover Protocol passe par une **socket TCP sur le WiFi**
(`ROVER_PROTOCOL.md` §2). L'UART0 (`GPIO1`/`GPIO3`) n'est donc plus
réservé à cet usage --- c'est ce qui libère la broche du micro ci-dessus.

------------------------------------------------------------------------

Les deux `VL53L0X` partagent la même adresse I2C par défaut (0x29) : le
firmware maintient le droit en reset via son `XSHUT` pendant qu'il
réadresse le gauche (0x30) au démarrage, puis relâche le droit qui reste
sur 0x29 (implémenté en Phase 4, `esp32/lib/sensors/DistanceSensor.cpp`).

------------------------------------------------------------------------

## Câblage moteur → encodeur (N20 6 fils)

Chaque moteur N20 utilisé a 6 fils : 2 pour la puissance (vers le driver,
voir tableau ci-dessus), 4 pour l'encodeur magnétique intégré. Code
couleur **confirmé sur le silkscreen du PCB encodeur lui-même**
(2026-09-02) --- l'assignation exacte canal A/canal B entre C1/C2 n'a pas
de conséquence si elle est inversée, ça inverse juste le signe du
comptage :

| Fil    | Marquage PCB | Fonction          | Destination |
|--------|--------------|-------------------|-------------|
| Blanc  | `M1`         | Moteur +/−        | `OUT1`/`OUT3` du driver (selon le moteur) |
| Rouge  | `M2`         | Moteur −/+        | `OUT2`/`OUT4` du driver |
| Noir   | `VCC`        | Encodeur VCC      | **3V3 ESP32** (pas le VCC 6-9V du driver --- les GPIO ne tolèrent pas cette tension) |
| Bleu   | `GND`        | Encodeur GND      | GND commun (ESP32/driver/batterie) |
| Vert   | `C1`         | Encodeur canal A  | GPIO34 (gauche) / GPIO36 = `VP` (droit) |
| Jaune  | `C2`         | Encodeur canal B  | GPIO35 (gauche) / GPIO39 = `VN` (droit) |

⚠ **Erreur de câblage corrigée le 2026-09-02** : la table précédente
partait d'une convention de couleur générique (rouge/noir = fils moteur)
qui ne correspond **pas** à ce modèle de carte encodeur. En réalité
Blanc/Rouge sont les fils moteur (`M1`/`M2`) et Noir est le VCC encodeur
--- Blanc avait donc été câblé sur le rail 3V3 ESP32 en pensant que
c'était le VCC encodeur (en réalité une borne moteur posée en direct sur
le rail logique), et Noir avait été câblé sur une sortie driver en
pensant que c'était Moteur − (en réalité le VCC encodeur, exposé au PWM
moteur pulsé jusqu'à 9V au lieu d'un 3.3V stable). C'est très
probablement la cause réelle du symptôme "les deux encodeurs ne
s'allument jamais ensemble" (le fil moteur sur le rail 3V3 le
perturbait dès que le driver PWMait cette phase) --- pas un rail de
breadboard dégradé comme suspecté initialement. Recâblé selon le
marquage PCB ci-dessus le 2026-09-02 : **les deux LED encodeur
s'allument ensemble**, correction confirmée.

Note pull-up : une résistance de 10 kΩ entre chaque fil de signal et le
3V3 avait été envisagée en 2026-09-01 pour un souci de canaux muets
(hypothèse sortie collecteur ouvert) --- à revalider séparément une fois
que la télémétrie confirme ou non des ticks côté encodeur avec le bon
câblage ci-dessus.

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
