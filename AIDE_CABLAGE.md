# Aide au câblage --- niveau débutant

> Ce fichier donne, composant par composant, "quelle broche va où" en
> langage simple. Pour le détail technique complet (pourquoi ce choix de
> GPIO, pièges connus, historique des révisions), voir `esp32/WIRING.md`
> et `esp32-cam/WIRING.md`.
>
> Toutes les broches ci-dessous ciblent la carte **ESP32 WROOM** (devkit
> 30 broches), la carte physique actuelle du robot.

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

Les deux capteurs partagent le même bus I2C (2 fils communs à tout le
bus) plus une broche individuelle chacun pour pouvoir les distinguer au
démarrage.

| Broche du module VL53L0X | Où la brancher |
|---|---|
| `VIN`/`VCC` (les deux capteurs) | 3V3 |
| `GND` (les deux capteurs) | GND commun |
| `SDA` (les deux capteurs, même fil) | GPIO21 |
| `SCL` (les deux capteurs, même fil) | GPIO22 |
| `XSHUT` du capteur **gauche** | GPIO0 |
| `XSHUT` du capteur **droit** | GPIO4 |

⚠ GPIO0 est une broche sensible au démarrage de l'ESP32 (elle doit
rester haute/flottante pendant le boot) --- ne rien brancher dessus qui
tire fort vers le GND en permanence.

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

Rien à souder ici : la liaison est un simple **câble USB** entre le
Raspberry Pi et le port USB du devkit ESP32 WROOM (le même câble qui
sert à programmer l'ESP32 depuis un PC). Le Pi voit l'ESP32 comme un
port série (`/dev/ttyUSB0`).

------------------------------------------------------------------------

## Broches à ne surtout pas utiliser pour un nouveau composant

Si un jour tu ajoutes encore un module, évite ces broches (déjà prises
ou dangereuses au boot) --- voir `esp32/WIRING.md` pour le tableau
complet et à jour :

- GPIO1, GPIO3 : liaison USB/Pi, ne pas toucher.
- GPIO6 à GPIO11 : réservées en interne à la mémoire flash, jamais
  disponibles.
- GPIO2, GPIO12, GPIO15 : sensibles au démarrage, déjà utilisées
  (écran / buzzer) mais à éviter pour tout nouveau signal qui tirerait
  fort au boot.
