# ROVER --- Journal d'avancement

> Fichier de reprise de session. Objectif : permettre de reprendre le
> travail rapidement sans relire tout l'historique de conversation.
> Ce fichier est complémentaire à `ARCHITECTURE_AND_ROADMAP.md` (la
> référence architecturale) et `ROVER_PROTOCOL.md` (la spec du
> protocole) --- il ne fait que suivre l'avancement dans le temps.

------------------------------------------------------------------------

## Session du 2026-08-23

### Contexte matériel confirmé

- Carte actuelle : **ESP32 WROOM** (pas de S3 pour l'instant).
- Le firmware doit rester **portable WROOM ↔ S3** (build PlatformIO à
  double environnement), car une évolution vers un S3 est prévue plus
  tard. Le Pi fait le gros du travail, donc l'ESP32 reste peu chargé
  sur WROOM.
- Aucun câblage réel (pins moteurs/servos/capteurs) n'est encore
  connu : rien n'a été codé en dur à ce sujet.

### Ce qui a été fait

1. **`ROVER_PROTOCOL.md`** créé : spec V1 complète du Rover Protocol
   (format de trame ASCII `TYPE clé=valeur *CS\n`, checksum XOR,
   catalogue COMMAND/STATE/EVENT/ERROR/ACK/HEARTBEAT, séquence de
   boot, machine d'état ESP32, actions `SYSTEM` réservées :
   `ping`/`resume`/`diag`).

2. **Scaffold firmware `esp32/`** (PlatformIO) :
   - `platformio.ini` : deux environnements `esp32_wroom` et
     `esp32_s3`.
   - `include/board_config.h` : macros de board, `RoverState`
     (BOOT/READY/ACTIVE/SAFE/ERROR), constantes protocole.
   - `lib/communication/RoverProtocol.{h,cpp}` : parseur/émetteur de
     trames, générique sur un `Stream` Arduino (pas lié à un port UART
     précis, puisque le câblage final n'est pas connu).
   - `lib/safety/HeartbeatMonitor.h` : suivi du heartbeat Pi → ESP32,
     timeout 500 ms.
   - `lib/safety/Watchdog.h` : watchdog matériel `esp_task_wdt`
     (timeout 3 s), couche de sécurité **indépendante** du heartbeat
     (détecte un firmware qui plante, pas seulement une perte de
     liaison avec le Pi).
   - `lib/system/Diagnostics.h` : construit les champs de diagnostic
     (`uptime_ms`, `free_heap`, `state`, `board`, `protocol`).
   - `src/main.cpp` : boot (`SYSTEM protocol=... board=... state=BOOT`),
     dispatch `HEARTBEAT`/`SYSTEM`, transition automatique vers `SAFE`
     sur timeout heartbeat, réponse à `SYSTEM action=diag`.

3. **Roadmap mise à jour** (`ARCHITECTURE_AND_ROADMAP.md`) :
   - Phase 1 (ESP32 minimum viable) : toutes les cases cochées.
   - Phase 6 : ajout du support manette/gamepad, de l'accès distant
     VPN, et du cas d'usage prioritaire "surveillance de la maison à
     distance".

4. **`.gitignore`** ajouté (`.pio/`, `.vscode/`, objets de build).

5. **`CLAUDE.md`** mis à jour : ESP32 WROOM (au lieu de S3/CAM), et
   consigne explicite de portabilité WROOM/S3 pour tout code ESP32
   généré.

### Problèmes rencontrés et résolutions

- **Erreur de compilation `board_config.h: No such file or directory`**
  lors du build des libs (`lib/communication`, `lib/safety`) alors que
  `src/main.cpp` le trouvait sans problème.
  → Cause : le dossier `include/` du projet n'est pas automatiquement
  ajouté au chemin d'inclusion des libs par le LDF de PlatformIO dans
  cette configuration.
  → Résolution : ajout explicite de `-I include` dans les
  `build_flags` globaux de `platformio.ini` (section `[env]`,
  héritée par les deux environnements).

### État actuel

- **Phase 0** (Architecture) : règles figées, protocole et
  arborescence logicielle définis.
- **Phase 1** (ESP32 minimum viable) : **complète** au niveau code.
  Compilation réelle vérifiée avec succès sur `esp32_wroom` **et**
  `esp32_s3` (`pio run -e esp32_wroom -e esp32_s3`).
  RAM ~6%, Flash ~8-20% selon la cible.
- **Aucun test sur matériel réel** : pas d'ESP32 branché pendant cette
  session (`/dev/ttyUSB*` / `/dev/ttyACM*` absents). Le firmware n'a
  donc été validé qu'à la compilation, pas au flash/à l'exécution.
- Rien côté Raspberry Pi (Phase 5) n'a encore été commencé.

### Prochaines étapes (Phase 2 --- Motorisation)

Bloqué en attente d'informations matérielles à fournir par
l'utilisateur avant de pouvoir coder concrètement :

- Câblage du TB6612FNG (pins STBY, AIN1/AIN2/PWMA, BIN1/BIN2/PWMB).
- GPIO utilisées pour les moteurs gauche/droite.
- GPIO utilisées pour les encodeurs (gauche/droite).

En parallèle, si du matériel est disponible, un premier flash réel du
firmware Phase 1 sur l'ESP32 WROOM permettrait de valider le
heartbeat/watchdog/diagnostic en conditions réelles (actuellement testé
à la compilation seulement).

### Décisions/contraintes à ne pas oublier

- Toute logique de contrôle distant (VPN, auth, streaming vidéo,
  smartphone, manette) reste **100% côté Raspberry Pi** ; l'ESP32 ne
  doit recevoir que des commandes abstraites (`MOVE`, `HEAD`, ...) via
  le Rover Protocol, quelle que soit la source de la commande.
- Ne jamais coder en dur des numéros de pins tant que le câblage réel
  n'est pas communiqué.
- Le code ESP32 doit rester compilable sur les deux environnements
  PlatformIO (`esp32_wroom` et `esp32_s3`) --- à vérifier après chaque
  changement significatif.
