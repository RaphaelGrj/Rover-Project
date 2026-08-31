# Rover Pi core (Phase 5 minimal + Phase 6 minimal)

Tranche minimale de `rover-core`/`rover-esp32` (ARCHITECTURE_AND_ROADMAP.md
§10) et de contrôle distant (Phase 6) : un service qui parle Rover
Protocol à l'ESP32 sur un port série, et sert une page de pilotage
(manette via Gamepad API, ou joystick tactile pour smartphone) qui
envoie des commandes `MOVE`, avec un flux vidéo optionnel. Protégé par
un token d'accès (voir "Sécurité" ci-dessous) --- **projet open source,
lis cette section avant d'exposer le serveur au-delà de ta machine**.
Pas encore de VPN (reste de la Phase 6) --- voir `PROGRESS.md` pour
l'état exact.

## Installation

```bash
cd pi
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Lancer

Sur le robot réel, une fois l'ESP32 branché en USB/UART :

```bash
python -m rover_core.main --port /dev/ttyUSB0
```

Le terminal affiche une ligne du type :

```
Open the control page at http://<host>:<port>/?token=<un-token-genere-aleatoirement-ici>
```

Ouvrir **exactement cette URL** (avec le `?token=...`) sur un téléphone
ou un navigateur avec une manette branchée --- sans lui, le serveur
répond `403 Forbidden` à toute requête (page, WebSocket, vidéo). Voir
"Sécurité" ci-dessous pour fixer ce token au lieu d'en avoir un nouveau
à chaque redémarrage.

## Configuration

Un fichier `pi/config.json` (optionnel, voir le gabarit
`pi/config.example.json`) peut fixer les valeurs par défaut de `--port`,
`--baudrate`, `--http-host`, `--http-port`, `--log-level`, `--log-dir`
--- pratique pour ne pas les retaper à chaque lancement. Un argument
`--xxx` explicite passé en ligne de commande a toujours priorité sur le
fichier. `pi/config.json` est ignoré par git (comme toute config locale)
--- copie `config.example.json` vers `config.json` et adapte-le.

## Logs

Par défaut, les logs vont uniquement sur la sortie standard. Passer
`--log-dir pi/logs` (ou l'équivalent dans `config.json`) active en plus
un fichier journal tournant (`rover_core.log`, 1 Mo × 5 fichiers max) ---
utile pour rejouer ce qui s'est passé après coup. `pi/logs/` est ignoré
par git.

## HTTPS/WSS (chiffrement)

Par défaut, le serveur parle en HTTP/WebSocket **non chiffrés** --- le
token d'accès circule donc en clair sur le WiFi local, lisible par
quelqu'un d'autre sur le même réseau. Pour chiffrer, générer un
certificat auto-signé (jamais à commiter, `pi/*.crt`/`pi/*.key` sont
ignorés par git) :

```bash
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout pi/rover.key -out pi/rover.crt -days 365 -subj "/CN=rover.local"
```

puis lancer avec :

```bash
python -m rover_core.main --port /dev/ttyUSB0 --tls-cert rover.crt --tls-key rover.key
```

Le navigateur affichera un avertissement de sécurité (certificat
auto-signé, normal) --- accepter/continuer manuellement. La page
bascule automatiquement le WebSocket en `wss://` selon le protocole de
la page (`index.html`, déjà écrit pour ça). `--tls-cert` et `--tls-key`
doivent être donnés ensemble ou pas du tout --- le serveur refuse de
démarrer sinon plutôt que de retomber silencieusement en clair.

## Démarrage automatique (systemd)

`pi/rover-core.service` est un gabarit d'unité systemd pour lancer
`rover_core` au démarrage du Pi et le relancer automatiquement en cas
de plantage. Ne contient aucun secret --- le token (`ROVER_CONTROL_TOKEN`)
vit dans un fichier séparé, jamais commité :

```bash
cp pi/rover.env.example pi/rover.env
nano pi/rover.env   # y mettre ton propre token

sudo cp pi/rover-core.service /etc/systemd/system/
sudo nano /etc/systemd/system/rover-core.service   # adapter les chemins/utilisateur

sudo systemctl daemon-reload
sudo systemctl enable --now rover-core
sudo journalctl -u rover-core -f   # suivre les logs (utile pour récupérer le token si rover.env est vide)
```

## Trouver le Pi sur le réseau (mDNS)

Raspberry Pi OS inclut Avahi (mDNS) par défaut : une fois l'hostname du
Pi choisi (`sudo raspi-config` → System Options → Hostname, par exemple
`rover`), il est joignable en `http://rover.local:8080/` sur le réseau
local sans avoir à chercher son IP. Rien à installer côté Python pour
ça --- c'est une fonctionnalité du système d'exploitation, pas de ce
code.

## Tester sans robot physique (Wokwi)

Le même code peut piloter la simulation Wokwi de l'ESP32 (voir
`esp32/wokwi.toml`, `rfc2217ServerPort = 4000`) : lancer la simulation
dans VSCode, puis :

```bash
python -m rover_core.main --port rfc2217://localhost:4000
```

et ouvrir `http://localhost:8080/`. Aucune différence de code entre les
deux cas : `RoverLink` utilise `serial.serial_for_url()`, qui traite
`/dev/ttyUSB0` et `rfc2217://...` de la même façon.

## Vidéo

`rover_control/camera.py` sert un flux MJPEG (`/video`, protégé par le
même token) via `picamera2` --- **aucune caméra n'est encore choisie
pour Rover** (voir `BOM.md`, Phase 6 toujours "hors périmètre"), donc en
pratique `/video` répond `503 camera unavailable` pour l'instant, y
compris sur un vrai Raspberry Pi sans caméra branchée. La page de
contrôle affiche "Caméra indisponible" dans ce cas plutôt qu'une image
cassée. Rien à faire de spécial une fois une caméra branchée --- ça
s'active tout seul si `picamera2` peut l'initialiser.

## Structure

- `rover_esp32/` : couche protocole pure (`protocol.py`, encode/decode
  de trames, aucune I/O) + `link.py` (port série, thread de lecture).
- `rover_core/` : `RoverCore` (état heartbeat/resume + machine à états
  comportementale, voir les commentaires dans `core.py`), `config.py`
  (fichier de config optionnel) et le point d'entrée `main.py`.
- `rover_control/` : serveur web (`aiohttp`) + page de contrôle statique
  (`static/index.html`, HTML/JS/CSS en un seul fichier, pas de build) +
  `auth.py` (token d'accès) + `camera.py` (flux vidéo optionnel).

## Sécurité : ce qui est garanti où

**Ce projet est open source et sera téléchargé par d'autres personnes
sur leur propre réseau --- lis cette section avant de considérer une
modification liée à la sécurité comme un détail.**

- **Coupure moteur** : ne dépend jamais de ce code côté Pi --- garantie
  par le timeout heartbeat de l'ESP32 (`ROVER_HEARTBEAT_TIMEOUT_MS`,
  500 ms, voir `board_config.h` et `ROVER_PROTOCOL.md` §9) et, si câblé,
  par le bouton d'arrêt d'urgence physique
  (`esp32/lib/safety/EStop.h`) --- deux couches indépendantes de ce
  service Python. `RoverCore` ne fait qu'arrêter d'envoyer des
  `HEARTBEAT` quand plus aucun client n'est connecté, et laisse l'ESP32
  se mettre en `SAFE` de lui-même. L'arrêt moteur immédiat envoyé en
  plus à la déconnexion (`RoverCore.client_disconnected`) est un confort
  de réactivité, pas une garantie de sécurité.
- **Accès au serveur de contrôle** : protégé par un token
  (`rover_control/auth.py`). **Aucun token par défaut n'est codé en dur
  dans ce dépôt** --- volontaire, un secret partagé par tout le monde
  qui télécharge un projet open source n'en est pas un. Définis
  `ROVER_CONTROL_TOKEN` toi-même (variable d'environnement) pour garder
  la même URL d'une session à l'autre ; sinon un token aléatoire est
  généré à chaque démarrage et affiché dans les logs.
- **Identifiants WiFi/OTA** (`esp32/OTA.md`) : même principe, jamais
  commités, lus depuis des variables d'environnement au moment de la
  compilation du firmware.
- **Ne jamais exposer ce serveur directement sur Internet** (port
  forwarding, etc.) sans le VPN prévu en Phase 6 --- le token protège
  contre un accès depuis le réseau local, pas contre une attaque depuis
  Internet (pas de chiffrement TLS sur le trafic HTTP/WebSocket en
  l'état).
