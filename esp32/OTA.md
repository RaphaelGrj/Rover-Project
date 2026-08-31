# Mise à jour du firmware par WiFi (OTA)

> Complémentaire à `WIRING.md`. Fonctionnalité **optionnelle**, inactive
> tant qu'elle n'est pas configurée --- aucun impact sur le
> fonctionnement normal du robot (voir `esp32/lib/ota/RoverOTA.h`).

------------------------------------------------------------------------

## Pourquoi

Une fois le robot assemblé, rouvrir la tête pour brancher un câble USB
à chaque flash de firmware devient pénible. L'OTA permet de flasher par
WiFi à la place, une fois le robot déjà sur le réseau local.

Ce n'est **pas** une entorse à la règle "l'ESP32 ne doit jamais dépendre
d'Internet" (`ARCHITECTURE_AND_ROADMAP.md` §22) : c'est un canal de
maintenance **réseau local uniquement**, jamais utilisé par le
fonctionnement normal du robot (le Rover Protocol reste 100% UART), et
entièrement inactif tant qu'un développeur ne l'active pas
explicitement.

------------------------------------------------------------------------

## ⚠️ Sécurité --- important pour un projet open source

Ce dépôt est public. **Aucun identifiant WiFi ni mot de passe OTA ne
doit jamais être écrit en dur dans un fichier commité** --- ni ici, ni
dans `platformio.ini`, ni dans le code. Un mot de passe par défaut
partagé dans un projet open source serait une vraie faille : tout le
monde qui télécharge le projet aurait le même, jusqu'à ce que quelqu'un
pense à le changer (et personne ne le fera systématiquement).

À la place, les identifiants sont lus depuis des **variables
d'environnement au moment de la compilation**, jamais stockés dans le
dépôt :

```
ROVER_WIFI_SSID
ROVER_WIFI_PASSWORD
ROVER_OTA_PASSWORD
```

Si elles ne sont pas définies, l'OTA reste désactivée --- c'est le cas
par défaut pour tout le monde, y compris toi tant que tu ne les as pas
définies. Le firmware **refuse aussi de démarrer l'OTA sans mot de
passe OTA** (`ROVER_OTA_PASSWORD` vide), pour ne jamais exposer un canal
de flash sans authentification à n'importe qui sur le réseau local.

------------------------------------------------------------------------

## Configuration (Linux Mint / bash)

```bash
export ROVER_WIFI_SSID="TonReseauWifi"
export ROVER_WIFI_PASSWORD="TonMotDePasseWifi"
export ROVER_OTA_PASSWORD="un-mot-de-passe-different-et-solide"

cd esp32
pio run -e esp32_wroom -t upload --upload-port /dev/ttyUSB0   # premier flash, USB obligatoire
```

Ces `export` ne persistent que pour le terminal courant --- ajoute-les à
`~/.bashrc` (ou un fichier chargé par ton shell, **jamais commité**) si
tu veux les garder d'une session à l'autre.

## Flasher par WiFi une fois l'OTA active

Une fois le firmware avec OTA activée tourne sur le robot et rejoint le
réseau configuré, PlatformIO peut cibler son adresse IP au lieu d'un
port USB :

```bash
pio run -e esp32_wroom -t upload --upload-port <IP_DU_ROVER>
```

(trouver l'IP via ton routeur/`arp -a`, ou --- une fois le point "mDNS"
de `PROGRESS.md` en place côté Pi --- via `rover.local`).

## Windows (PowerShell)

```powershell
$env:ROVER_WIFI_SSID = "TonReseauWifi"
$env:ROVER_WIFI_PASSWORD = "TonMotDePasseWifi"
$env:ROVER_OTA_PASSWORD = "un-mot-de-passe-different-et-solide"
```

------------------------------------------------------------------------

## Limites connues

- Non testé sur matériel réel (pas de réseau WiFi configuré pendant
  cette session de développement) --- vérifié uniquement à la
  compilation sur `esp32_wroom`/`esp32_s3`, et que le firmware boote
  normalement **sans** OTA configurée (comportement par défaut).
- GPIO14 (ADC batterie, voir `WIRING.md`) est en ADC2 et devient
  illisible pendant que le WiFi est actif --- pas un problème tant que
  le monitoring batterie reste désactivé par défaut, à revoir si les
  deux fonctionnalités sont activées ensemble un jour.
- Le mot de passe OTA protège contre un flash non autorisé, mais le
  trafic OTA lui-même n'est pas chiffré (limite d'`ArduinoOTA` telle
  qu'utilisée ici) --- acceptable pour un usage réseau local de
  confiance, pas pour un réseau partagé/non fiable.
