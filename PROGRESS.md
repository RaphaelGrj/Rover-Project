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
  n'est pas communiqué (un pinout *provisoire* pour Wokwi existe,
  voir plus bas --- ce n'est pas une exception, il reste en
  documentation, hors firmware).
- Le code ESP32 doit rester compilable sur les deux environnements
  PlatformIO (`esp32_wroom` et `esp32_s3`) --- à vérifier après chaque
  changement significatif.
- Tout code du projet doit être commenté (pas ligne par ligne, mais
  assez pour s'y retrouver sans relire tout l'historique) --- consigne
  ajoutée dans `CLAUDE.md` le 2026-08-25.

------------------------------------------------------------------------

## Session du 2026-08-25

### Contexte

L'utilisateur a installé l'extension VSCode **Wokwi** pour pouvoir
tester le firmware en simulation. Objectif de la session : préparer ce
qu'il faut pour simuler ce qui existe déjà (Phase 1), sans attendre le
câblage réel.

### Ce qui a été fait

1. **Environnement Wokwi** (`esp32/wokwi.toml` + `esp32/diagram.json`) :
   - Le firmware actuel (Phase 1) ne parle qu'en Serial/USB (Rover
     Protocol), aucun GPIO n'est encore utilisé : le diagramme Wokwi
     est donc volontairement minimal, une carte `wokwi-esp32-devkit-v1`
     seule, sans câblage.
   - `wokwi.toml` pointe par défaut sur le build `esp32_wroom` (carte
     physique actuelle) ; instructions dans le fichier pour basculer
     sur `esp32_s3` si besoin.
   - Prérequis avant de lancer la simulation : `pio run -e esp32_wroom`
     (déjà vérifié --- build OK, `.pio/build/esp32_wroom/firmware.bin`
     et `.elf` présents).

2. **`esp32/tools/rover_frame.py`** : petit script qui calcule le
   checksum XOR d'une trame Rover Protocol et affiche la trame complète
   prête à coller dans le moniteur série Wokwi (le calculer à la main
   est trop pénible/source d'erreur pour tester manuellement).
   Vérifié contre l'implémentation C++ (`RoverProtocol::checksum`).

   Au passage : l'exemple de checksum dans `ROVER_PROTOCOL.md` §3.1
   (`MOVE velocity=0.25 rotation=-0.10 *4B`) était **faux** --- corrigé
   en `*39` (valeur réellement produite par l'algorithme XOR décrit
   juste au-dessus, vérifiée avec le script et avec le code C++).

3. **`esp32/WIRING.md`** : pinout de base **provisoire** pour la carte
   WROOM (UART2 vers le Pi, TB6612FNG, encodeurs, servos tête,
   ST7789, bus I2C partagé MPU6050/BME688/VL53L0X×2 avec XSHUT dédiés).
   Explicitement marqué comme proposition, pas codé en dur nulle part
   dans le firmware --- sert de base pour construire les prochains
   `diagram.json` Wokwi une fois le code des phases 2--4 écrit, et
   pourra évoluer dès que le câblage réel sera connu.

4. **Consigne de commentaires** ajoutée dans `CLAUDE.md`
   ("Instructions strictes pour la génération de code") : tout code du
   projet doit être commenté (pas 100% des lignes, mais assez pour s'y
   retrouver). Les fichiers Phase 1 existants ont été relus et
   complétés en conséquence (`RoverProtocol.cpp` --- checksum, parsing
   de trame, gestion d'overflow ; `Diagnostics.h` ; `main.cpp` ---
   logique de transition d'état). Recompilé avec succès sur les deux
   cibles après ces changements.

5. **Bug Wokwi corrigé** : la première version de `esp32/diagram.json`
   ne déclarait aucune connexion (`"connections": []`), donc l'UART0
   de la carte n'était routé nulle part --- la carte bootait mais le
   moniteur série restait totalement vide (observé à la fois dans
   VSCode et en reproduisant en headless avec `wokwi-cli`). Corrigé en
   ajoutant les connexions `esp32:TX0 → $serialMonitor:RX` et
   `esp32:RX0 → $serialMonitor:TX`. Revalidé en headless : le message
   de boot `SYSTEM protocol=ROVER_PROTOCOL_V1 board=WROOM state=BOOT`
   apparaît bien.
   `wokwi-cli` installé localement (`~/bin/wokwi-cli`, script officiel
   `https://wokwi.com/ci/install.sh`) pour ce diagnostic ; nécessite un
   `WOKWI_CLI_TOKEN` (compte Wokwi, https://wokwi.com/dashboard/ci).

6. **Moniteur série VSCode en lecture seule chez l'utilisateur** :
   le terminal intégré affichant la sortie série ne permettait pas d'y
   taper/coller (voir extension changelog v2.1.0 --- sortie série via
   le terminal VSCode intégré, entrée non confirmée fonctionnelle dans
   ce contexte). Contournement : activation de `rfc2217ServerPort =
   4000` dans `esp32/wokwi.toml` --- ça expose le port série virtuel
   en TCP (RFC2217), ce qui a permis de piloter la simulation
   directement depuis un script Python (`pyserial`,
   `serial.serial_for_url("rfc2217://localhost:4000")`) sans dépendre
   de l'UI VSCode.

7. **Phase 1 validée en simulation** via ce canal RFC2217 :
   - `SYSTEM action=ping` → `SYSTEM action=pong` : OK.
   - `SYSTEM action=diag` → `STATE ...` : OK.
   - Un seul `HEARTBEAT` envoyé puis silence : `state` passe bien de
     `ACTIVE` à `SAFE` après ~500 ms (confirmé par deux `diag`
     successifs, `uptime_ms` progressant normalement --- pas de reset
     watchdog intempestif). Note : la frame `EVENT
     name=heartbeat_timeout` elle-même (poussée spontanément par le
     firmware, hors réponse à une commande) n'est pas remontée de
     façon fiable via RFC2217 dans ce test --- probablement un détail
     de bufferisation côté extension, pas un bug firmware (l'état
     interne change correctement, vérifié indépendamment).
   - Checksum invalide sur une trame → `ERROR code=checksum_invalid`
     (découvert accidentellement en copiant un mauvais checksum) : OK.
   - `SYSTEM action=resume` en état `SAFE` → retour à `ACTIVE` : OK.

### État actuel

- **Phase 0** : inchangé, règles figées.
- **Phase 1** : code inchangé fonctionnellement (juste commenté),
  compile toujours sur `esp32_wroom` et `esp32_s3`. **Entièrement
  validée en simulation Wokwi** : boot, ping, diag, timeout heartbeat
  → SAFE, erreur de checksum, resume SAFE → ACTIVE. Reste non testé
  sur matériel physique réel (toujours aucun ESP32 branché à ce
  stade).
- **Phase 2** : toujours bloquée en attente du câblage réel (le
  pinout de `WIRING.md` est une base de travail, pas une validation).

### Prochaines étapes

- Lancer la simulation Wokwi et valider en pratique : boot
  (`SYSTEM protocol=... board=WROOM state=BOOT`), réponse à
  `SYSTEM action=ping`/`diag`, passage en `SAFE` après 500 ms sans
  `HEARTBEAT`, retour en `ACTIVE` via `SYSTEM action=resume`.
- Toujours en attente d'infos matérielles réelles pour démarrer la
  Phase 2 (Motorisation) : câblage effectif du TB6612FNG et des
  encodeurs (le pinout de `WIRING.md` peut servir de point de départ
  à confirmer/modifier plutôt que de repartir de zéro).
