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

## MQTT / Home Assistant (premier pas minimal)

Publication optionnelle de l'état de Rover sur un broker MQTT ---
**pas** l'intégration Home Assistant complète (Phase 10 de
`ARCHITECTURE_AND_ROADMAP.md`), juste un instantané périodique (état
comportemental + toute la télémétrie connue) sur `<préfixe>/state`.
Rien n'est activé par défaut : ni le broker (aucun host configuré), ni
la dépendance (`paho-mqtt` n'est pas dans `requirements.txt` de base).

```bash
pip install -r requirements-mqtt.txt
python -m rover_core.main --port /dev/ttyUSB0 --mqtt-host 192.168.1.50
```

Options : `--mqtt-port` (1883 par défaut), `--mqtt-topic-prefix`
(`rover` par défaut), ou les mêmes clés dans `config.json`
(`mqtt_host`, `mqtt_port`, `mqtt_topic_prefix`,
`mqtt_publish_period_s`). Si `paho-mqtt` n'est pas installé ou que le
broker est injoignable, la publication reste simplement désactivée
(log d'info/avertissement), le reste de `rover_core` continue de
fonctionner normalement.

## Accès distant sécurisé (VPN)

Piloter Rover depuis l'extérieur du réseau local, sans exposer
directement `rover_control` sur Internet --- voir `pi/VPN.md` pour le
guide complet (WireGuard, auto-hébergé, un seul port UDP à ouvrir sur
la box). Le token d'accès reste obligatoire une fois connecté au VPN,
les deux se cumulent.

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

La caméra n'est **pas** locale au Pi --- c'est un module **ESP32-CAM**
déporté et indépendant (voir `ARCHITECTURE_AND_ROADMAP.md` §4.3 et
`esp32-cam/`), qui sert son propre flux MJPEG sur le réseau local
(`http://rovercam.local:81/stream` par défaut). `rover_control/camera.py`
**relaie** ce flux (reverse proxy, via `aiohttp`) à travers le même
endpoint authentifié `/video` --- rien n'est jamais exposé directement
et sans authentification.

Configurer l'URL de l'ESP32-CAM (`--camera-url` ou `camera_url` dans
`config.json`) :

```bash
python -m rover_core.main --port /dev/ttyUSB0 --camera-url http://rovercam.local:81/stream
```

Sans `camera_url` configuré, ou si l'ESP32-CAM est injoignable au
moment d'une requête, `/video` répond `503 camera unavailable` --- la
page de contrôle affiche "Caméra indisponible" dans ce cas plutôt
qu'une image cassée, jamais un crash du reste du serveur.

## rover-ai

Fournisseur IA interchangeable (API cloud ou LLM sur le réseau local),
voir `ARCHITECTURE_AND_ROADMAP.md` §17.1 pour l'architecture complète.
**Panneau de configuration + test en texte disponibles à
`http://<pi>:8080/ai?token=...`** (lien "IA" en haut à droite de la page
de pilotage) : choisir le fournisseur, entrer la clé/l'adresse, tester
une conversation directement dans le navigateur. **Toujours pas branché
dans un pipeline audio** (micro/STT/TTS, Phase 7 pas commencée) --- ce
panneau permet de configurer/valider rover-ai indépendamment, avant que
le reste de la Phase 7 existe.

**Personality Engine (`rover_ai/personality.py`, §15/§17.1)** :
`AIPanel.ask()` ne fait plus un appel "sec" au fournisseur --- chaque
message passe par un `PersonalityEngine` qui construit l'`AIContext`
(persona Rover + historique de la conversation en cours, borné à 10
échanges) et réagit à l'échange en pilotant l'expression du visage
(`RoverCore.set_emotion`, câblé dans `rover_core/main.py`) : `curious`
pendant que Rover "réfléchit", `happy` une fois la réponse reçue,
`confused` si le fournisseur échoue. Un fournisseur indisponible reste
sans crash (§21) --- juste l'émotion qui change et l'erreur `503`
habituelle. Nouvelle route `POST /ai/reset` (bouton "Nouvelle
conversation" sur la page `/ai`) pour repartir sans historique sans
toucher à la configuration du fournisseur ; changer de fournisseur/
vendeur via `/ai/config` réinitialise aussi l'historique automatiquement
(ne pas faire hériter une conversation d'un autre backend).

- `rover_ai.AIProvider` : interface commune, `async ask(message, context)
  -> str`. `AIProviderError` en cas d'échec (réseau, HTTP, réponse
  malformée, pas configuré) --- jamais d'exception silencieuse, jamais de
  crash (§21 "mode dégradé").
- Fournisseurs cloud (`rover_ai/cloud.py`) : `AnthropicProvider`,
  `OpenAIProvider`, `GeminiProvider` --- chacun un format de requête
  différent, une clé API différente. Plus cinq agrégateurs compatibles
  API OpenAI (donc juste une URL/clé/modèle par défaut, aucune logique
  de requête séparée) : `QwenCloudProvider` (Qwen officiel, cloud
  Alibaba/DashScope), `OpenRouterProvider`, `TogetherProvider`,
  `FireworksProvider`, `DeepInfraProvider` --- ces quatre derniers
  hébergent aussi bien Qwen que des modèles communautaires non censurés
  (Dolphin, variantes "abliterated", ...), catalogue à vérifier sur le
  site du fournisseur avant de figer un `model` en prod (ça bouge).
- Fournisseur local (`rover_ai/local.py`) : `LocalAIProvider`, n'importe
  quel serveur compatible API OpenAI sur le réseau (Ollama est la
  cible de référence citée par §17.1). Aucune clé requise, juste une
  adresse (`http://<ip>:<port>/v1`). Qwen 2.5 (l'exemple du doc) ou un
  modèle non censuré (ex. un tag "abliterated"/uncensored servi par
  Ollama) passent par la même classe --- seul le nom du modèle change,
  pas de code séparé par modèle.
- `rover_ai.credentials` : stockage des identifiants dans
  `pi/ai_credentials.json` (git-ignoré, permissions `0600`, jamais dans
  `config.json` --- même logique que `ROVER_CONTROL_TOKEN`). Voir
  `pi/ai_credentials.example.json` pour le gabarit des clés acceptées.
- `rover_ai.create_provider(credentials)` : construit le fournisseur
  actif à partir de ces identifiants -- le futur appelant (`RoverCore`)
  n'a jamais besoin d'importer une classe de fournisseur en particulier.

## rover_audio (Speech-to-Text / Text-to-Speech)

**Panneau de configuration + test disponible à
`http://<pi>:8080/audio?token=...`** (lien "Voix" sur la page de
pilotage), voir `ARCHITECTURE_AND_ROADMAP.md` §17.2. STT et TTS sont
configurés indépendamment (fournisseur/vendeur/clé différents possibles
pour chacun) : choisir cloud ou réseau local pour chacun, tester une
transcription (uploader un fichier audio, obtenir le texte) et une
synthèse (taper du texte, écouter l'audio généré) directement dans le
navigateur --- utilisable dès maintenant, sans attendre le micro/
haut-parleur du robot (Phase 7 "Micro"/"Audio pipeline", pas commencés).

- `rover_audio.SpeechToTextProvider`/`TextToSpeechProvider` : interfaces
  communes (`async transcribe(audio) -> str` / `async synthesize(texte)
  -> bytes`), `AudioProviderError` en cas d'échec (réseau, HTTP, réponse
  malformée, pas configuré) --- même contrat "jamais de crash, mode
  dégradé" que `rover_ai.AIProviderError`.
- Fournisseur cloud (`rover_audio/cloud.py`) : `OpenAIWhisperSTT`
  (`/audio/transcriptions`) et `OpenAITTS` (`/audio/speech`) --- un seul
  vendeur pour l'instant, même chemin de croissance que rover-ai (parti
  de 3 fournisseurs, monté à 8 sur demande) si un autre vendeur s'avère
  utile plus tard.
- Fournisseur local (`rover_audio/local.py`) : `LocalSTT`/`LocalTTS`,
  n'importe quel serveur auto-hébergé exposant une API compatible OpenAI
  pour l'audio (ex. un whisper.cpp/faster-whisper wrappé, ou
  openedai-speech pour la synthèse). Aucune clé requise.
- `rover_audio.credentials` : stockage dans `pi/audio_credentials.json`
  (git-ignoré, permissions `0600`, jamais dans `config.json`) --- même
  logique que `pi/ai_credentials.json`. Voir
  `pi/audio_credentials.example.json` pour le gabarit.
- **Pas encore fait** : rien ne relie ce module à un vrai micro/
  haut-parleur, ni à `rover_ai.personality.PersonalityEngine.converse()`
  (§17.1) --- ce sera l'"Audio pipeline" de la Phase 7, une fois le
  micro I2S (INMP441, BOM) câblé.

## Réalité augmentée (casque Meta Quest 3 / WebXR)

**Page à `http://<pi>:8080/ar?token=...`** (lien "RA" en haut à droite de
la page de pilotage), voir `ARCHITECTURE_AND_ROADMAP.md` §13.1 pour
l'architecture complète. Ouvre directement dans le navigateur du casque
(Meta Quest Browser ou tout autre navigateur compatible WebXR) --- pas
d'app à installer, pas de compte développeur Meta.

**Premier incrément seulement** : un HUD de télémétrie (distance, IMU,
environnement, batterie, état comportemental --- même flux `/ws` que la
page de pilotage) superposé au passthrough caméra via la feature WebXR
`dom-overlay`. Pas encore de carte 3D --- ça dépend de la Phase 9
(Cartographie/Localisation), qui n'a pas commencé.

Dégrade proprement sur un appareil sans WebXR AR (`immersive-ar` non
supporté --- la plupart des navigateurs desktop et téléphones) : la même
page retombe sur un affichage 2D classique, aucune fonctionnalité perdue
à part la superposition en RA elle-même.

**Jamais testé sur un vrai casque** (pas de Meta Quest 3 disponible
cette session) --- écrit au plus près de la spec WebXR Device API +
module `dom-overlay`, uniquement exercé ici via le chemin de repli 2D
(qui partage le même code de rendu du HUD que le mode RA).

## Structure

- `rover_esp32/` : couche protocole pure (`protocol.py`, encode/decode
  de trames, aucune I/O) + `link.py` (port série, thread de lecture).
- `rover_core/` : `RoverCore` (état heartbeat/resume + machine à états
  comportementale, voir les commentaires dans `core.py`), `config.py`
  (fichier de config optionnel) et le point d'entrée `main.py`.
- `rover_control/` : serveur web (`aiohttp`) + page de contrôle statique
  (`static/index.html`, HTML/JS/CSS en un seul fichier, pas de build) +
  `auth.py` (token d'accès) + `camera.py` (flux vidéo optionnel).
- `rover_mqtt/` : `publisher.py`, publication MQTT optionnelle
  (`paho-mqtt`, extra séparé).
- `rover_ai/` : fournisseur IA interchangeable (voir ci-dessus).
  `rover_control/ai_panel.py` : logique du panneau web (config + test) ;
  `rover_control/static/ai.html` : la page elle-même.

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
- **Clé API `rover-ai`** (`rover_ai/credentials.py`) : même principe
  encore, `pi/ai_credentials.json` git-ignoré, permissions `0600`. Le
  panneau web (`/ai`) ne réaffiche jamais la clé en clair une fois
  enregistrée --- un placeholder redacté (`********`) est renvoyé à sa
  place, laisser le champ vide au prochain enregistrement la conserve
  telle quelle.
- **Ne jamais exposer ce serveur directement sur Internet** (port
  forwarding, etc.) sans le VPN prévu en Phase 6 --- le token protège
  contre un accès depuis le réseau local, pas contre une attaque depuis
  Internet (pas de chiffrement TLS sur le trafic HTTP/WebSocket en
  l'état).
