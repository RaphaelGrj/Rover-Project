# Rover Pi core (Phase 5 minimal + Phase 6 minimal)

Tranche minimale de `rover-core`/`rover-esp32` (ARCHITECTURE_AND_ROADMAP.md
§10) et de contrôle distant (Phase 6) : un service qui parle Rover
Protocol à l'ESP32 sur un port série, et sert une page de pilotage
(manette via Gamepad API, ou joystick tactile pour smartphone) qui
envoie des commandes `MOVE`. Pas encore de caméra/VPN/authentification
(reste de la Phase 6) --- voir `PROGRESS.md` pour l'état exact.

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

Puis ouvrir `http://<ip-du-pi>:8080/` sur un téléphone ou un
navigateur avec une manette branchée.

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

## Structure

- `rover_esp32/` : couche protocole pure (`protocol.py`, encode/decode
  de trames, aucune I/O) + `link.py` (port série, thread de lecture).
- `rover_core/` : `RoverCore` (état heartbeat/resume, voir les
  commentaires dans `core.py` pour la logique de sécurité) et le point
  d'entrée `main.py`.
- `rover_control/` : serveur web (`aiohttp`) + page de contrôle statique
  (`static/index.html`, HTML/JS/CSS en un seul fichier, pas de build).

## Sécurité : ce qui est garanti où

La coupure moteur sur perte de liaison **ne dépend jamais de ce code**
--- elle est garantie par le timeout heartbeat de l'ESP32
(`ROVER_HEARTBEAT_TIMEOUT_MS`, 500 ms, voir `board_config.h` et
`ROVER_PROTOCOL.md` §9). `RoverCore` ne fait qu'arrêter d'envoyer des
`HEARTBEAT` quand plus aucun client de contrôle n'est connecté, et
laisse l'ESP32 se mettre en `SAFE` de lui-même. L'arrêt moteur immédiat
envoyé en plus à la déconnexion (`RoverCore.client_disconnected`) est un
confort de réactivité, pas une garantie de sécurité.
