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

### État actuel (fin de session)

- **Phase 0** : inchangé, règles figées.
- **Phase 1** : **entièrement validée en simulation Wokwi** (boot,
  ping, diag, timeout heartbeat → SAFE, erreur de checksum, resume
  SAFE → ACTIVE). Reste non testée sur matériel physique réel.
- **Phase 2** : voir session suivante --- passée de "bloquée" à "code
  complet, validée en simulation" dans la même journée, décision
  explicite de l'utilisateur de continuer à avancer via Wokwi plutôt
  que d'attendre le câblage réel définitif.

------------------------------------------------------------------------

## Session du 2026-08-25 (suite) --- Phase 2 : Motorisation

### Contexte

Décision de l'utilisateur : ne plus bloquer sur l'absence de câblage
réel confirmé --- avancer avec le pinout provisoire de `WIRING.md` et
laisser Wokwi valider le travail au fur et à mesure. Pendant
l'implémentation, correction matérielle reçue : le driver moteur n'est
**pas** un TB6612FNG comme initialement esquissé, mais un **DRV8833**,
avec des moteurs **N20 6V à encodeur intégré**.

Différence importante DRV8833 vs TB6612FNG : pas de pin PWM séparée
--- chaque moteur utilise directement 2 pins PWM (IN1 = avant, IN2 =
arrière, les deux à 0 = roue libre). 4 GPIO pour les deux moteurs au
lieu de 6. `esp32/WIRING.md` et `esp32/include/motion_config.h` mis à
jour en conséquence (GPIO14/25, anciennement PWMA/PWMB, sont
maintenant libres).

### Ce qui a été fait

1. **`esp32/include/motion_config.h`** (nouveau) : centralise tout le
   pinout Phase 2 + les constantes de mouvement (gains PID, géométrie
   roue, ticks encodeur/tour). Explicitement commenté comme
   provisoire/non calibré --- seul ce fichier devra changer si le
   câblage ou le matériel réel diverge.

2. **`esp32/lib/motors/`** (nouveau module) :
   - `MotorDriver` : pilotage bas niveau d'un canal DRV8833 (2 canaux
     LEDC par moteur, un par sens).
   - `Encoder` : comptage de ticks par interruption GPIO (x1 decode),
     sur les pins input-only (GPIO34/35/36/39) choisies justement pour
     ça dans `WIRING.md`.
   - `WheelPID` : PID complet (P/I/D, anti-windup sur l'intégrale) par
     roue, gains placeholders.
   - `DriveController` : modèle unicycle (vitesse + rotation → vitesse
     gauche/droite), boucle PID par roue à période fixe, `stop()`
     immédiat (bypass PID), télémétrie `STATE left_speed=...
     right_speed=...`.

3. **`main.cpp`** : dispatch `MOVE velocity=... rotation=...` vers
   `DriveController::setTarget()`, mais **seulement en état `ACTIVE`**
   --- une trame `MOVE` reçue en `SAFE` est ignorée, jamais de
   réarmement moteur hors `SYSTEM action=resume` explicite (règle
   architecture §27.6). `drive.stop()` appelé au boot (avant toute
   commande) et sur timeout heartbeat (`SAFE`), en plus de l'`EVENT`
   déjà existant. Télémétrie `STATE` périodique (200 ms) tant
   qu'`ACTIVE`.

4. **`esp32/diagram.json`** étendu : 4 LEDs + résistances 220 Ω sur
   les pins IN1/IN2 gauche/droite, pour visualiser direction/PWM sans
   moteur réel. Validé avec `wokwi-cli lint` (pins de
   `wokwi-esp32-devkit-v1` nécessitent le préfixe `D`, ex. `D27`, pas
   juste `27` --- erreur détectée et corrigée via le lint).

5. **Compilation** vérifiée sur `esp32_wroom` et `esp32_s3` après
   chaque étape.

6. **Validé en simulation** (headless `wokwi-cli` + RFC2217, même
   méthode que pour la Phase 1) :
   - Boot avec le nouveau code (init moteurs/encodeurs/PID) : pas de
     crash, pas de watchdog intempestif.
   - `MOVE velocity=0.20 rotation=0.0` : accepté, aucun crash.
   - `SYSTEM action=diag` après `MOVE` : `state=ACTIVE`, uptime/heap
     stables.
   - Télémétrie `STATE left_speed=0.00 right_speed=0.00` reçue
     périodiquement --- valeurs à 0 normales, **aucun encodeur n'est
     simulé** dans `diagram.json` (pas de chip DC-motor+encodeur
     câblé), donc la boucle PID tourne en boucle ouverte en
     simulation. Ce n'est pas un bug, juste une limite de la
     simulation actuelle.

### Ce qui n'est PAS validé

- Le retour d'encodeur réel et donc l'asservissement PID en boucle
  fermée (aucun encodeur simulé pour l'instant --- possible plus tard
  avec un chip Wokwi tiers de type "DC motor + encoder", pas
  investigué en détail cette session).
- Toute valeur physique (`ROVER_ENCODER_TICKS_PER_REV`,
  `ROVER_WHEEL_DIAMETER_M`, gains PID) --- ce sont des placeholders
  explicitement marqués comme tels dans `motion_config.h`.
- Le câblage réel sur le robot physique (toujours aucun matériel
  branché à ce stade).

### État actuel

- **Phase 2** : **code complet, compile sur les deux cibles, validée
  en simulation Wokwi** (sans boucle fermée encodeur). Non testée sur
  matériel physique.

------------------------------------------------------------------------

## Session du 2026-08-25 (suite 2) --- Durcissement Phase 2

### Contexte

Demande explicite de l'utilisateur : avant d'enchaîner sur la suite,
revue critique du code Phase 2 pour éliminer les bugs latents ---
objectif "aucune erreur de code une fois au câblage", code évolutif.
Revue faite fichier par fichier (`MotorDriver`, `Encoder`, `WheelPID`,
`DriveController`), 3 corrections retenues (pas de simple nettoyage
cosmétique, des bugs réels ou des risques concrets identifiés) :

1. **`Encoder` : `noInterrupts()`/`interrupts()` → `portMUX_TYPE`
   (spinlock ESP32)**. `noInterrupts()`/`interrupts()` ne suspendent
   que le cœur courant ; sur un ESP32 (dual-core), si l'ISR de
   l'encodeur finissait un jour planifiée sur l'autre cœur que celui
   qui appelle `readAndResetTicks()`, la section critique ne protégeait
   plus rien (race condition silencieuse sur `_ticks`). `portMUX_TYPE`
   + `portENTER/EXIT_CRITICAL(_ISR)` est le mécanisme correct pour du
   partagé ISR/tâche sur ESP32, quel que soit le cœur.

2. **`WheelPID` : dérivée calculée sur l'erreur → sur la mesure**. Avec
   Kd calculée sur l'erreur, chaque nouvelle commande `MOVE` change la
   consigne instantanément, ce qui provoque un pic de dérivée
   ("derivative kick") même si la roue n'a pas encore bougé. Invisible
   aujourd'hui (`ROVER_PID_KD = 0`), mais serait redécouvert --- et
   probablement mal diagnostiqué --- au moment de calibrer Kd sur le
   vrai moteur. Corrigé maintenant pendant que c'est simple, plutôt que
   plus tard en pleine calibration matérielle.

3. **`DriveController::setTarget` : garde NaN/Inf + clamp rotation**.
   Un champ `velocity=nan` dans une trame `MOVE` passe la validation
   checksum sans problème (le checksum ne valide que les octets, pas la
   sémantique) ; `strtof` renvoie alors `NaN`, que `constrain()` ne
   filtre pas (toute comparaison avec `NaN` est fausse), et caster un
   `NaN`/`Inf` en `int16_t` ensuite est un comportement indéfini en
   C++. Corrigé en filtrant `isnan()`/`isinf()` avant tout calcul en
   aval. Au passage, la `rotation` n'était pas bornée du tout (seule la
   `velocity` l'était) --- ajout de `ROVER_MAX_ROTATION_RAD_S` dans
   `motion_config.h`.

4. **`motion_config.h`** : commentaire ajouté réservant les canaux LEDC
   0--3 aux moteurs, pour que la Phase 3 (servos tête) ne vienne pas
   les réutiliser par erreur --- pensé pour rester évolutif.

### Validation

- Recompilé avec succès sur `esp32_wroom` et `esp32_s3` après chaque
  correction.
- Testé en simulation Wokwi (RFC2217, instance VSCode de
  l'utilisateur, redémarrée pour charger le nouveau binaire) :
  - `MOVE velocity=0.20 rotation=0.0` en état `ACTIVE` → accepté,
    télémétrie `STATE left_speed=0.00 right_speed=0.00` reçue (0.00
    normal, toujours pas d'encodeur simulé).
  - `MOVE` reçu en état `SAFE` (heartbeat expiré entre deux commandes
    manuelles, comportement attendu en l'absence de flux `HEARTBEAT`
    continu) → correctement ignoré, confirmé par un `diag` de suivi.
  - `MOVE velocity=nan rotation=0` → aucun crash, `uptime_ms` et
    `free_heap` restent stables/cohérents sur les diag suivants (pas de
    reboot, pas de fuite mémoire).

### État actuel

- **Phase 2** : code complet, durci contre les entrées malformées et
  les risques multi-cœur, validée en simulation Wokwi. Toujours non
  testée sur matériel physique ; gains PID et géométrie roue toujours
  des placeholders.

### Prochaines étapes

- Continuer la Phase 3 (Tête et écran) ou affiner encore la Phase 2
  selon la priorité de l'utilisateur.
- Dès que le matériel physique (ESP32 + DRV8833 + moteurs N20) est
  disponible : flasher, mesurer les vraies specs encodeur (ticks/tour
  réels selon le ratio de réduction), calibrer les gains PID, confirmer
  ou ajuster le pinout de `WIRING.md`/`motion_config.h`.

------------------------------------------------------------------------

## Session du 2026-08-25 (suite 3) --- Rover pilotable (Phase 5 + 6 minimales)

### Contexte

Question de l'utilisateur : à partir de quelle phase du roadmap
Rover devient-il pilotable (manette + interface smartphone) ? Réponse :
Phase 6, mais elle n'a rien à quoi se connecter tant que la Phase 5
(rien côté Pi jusqu'ici) n'existe pas au moins a minima. Décision :
implémenter une tranche minimale des deux --- juste de quoi piloter
`MOVE` en local, pas encore caméra/VPN/authentification.

Choix technique validé par l'utilisateur : **Python** côté Pi (cohérent
avec les scripts de test déjà utilisés cette session, `pyserial` pour
l'UART).

### Ce qui a été fait

Nouveau répertoire `pi/` (nouveau langage dans le dépôt --- premier
code Python du projet) :

1. **`pi/rover_esp32/`** --- couche protocole :
   - `protocol.py` : encode/decode de trames Rover Protocol, pur (pas
     d'I/O), recopie fidèle de l'algorithme de `RoverProtocol.cpp`
     (checksum XOR, découpage `*CS`). Vérifié par comparaison directe
     avec `esp32/tools/rover_frame.py` et l'exemple corrigé de
     `ROVER_PROTOCOL.md` --- valeurs identiques.
   - `link.py` : `RoverLink`, connexion série via
     `serial.serial_for_url()` --- accepte aussi bien un port réel
     (`/dev/ttyUSB0`) qu'une URL `rfc2217://...`, donc le **même code**
     pilote le robot physique ou la simulation Wokwi. Thread de lecture
     dédié (`serial.threaded.ReaderThread`, pyserial n'a pas de support
     asyncio natif).

2. **`pi/rover_core/`** :
   - `core.py` (`RoverCore`) : ne duplique pas la sécurité de l'ESP32
     --- quand aucun client de contrôle n'est connecté, arrête
     simplement d'envoyer `HEARTBEAT` et laisse l'ESP32 passer en
     `SAFE` tout seul (`ARCHITECTURE_AND_ROADMAP.md` §9). Envoie
     `SYSTEM action=resume` à la connexion d'un client (nécessaire :
     `HEARTBEAT` seul ne sort pas de `SAFE`). Gère plusieurs clients de
     contrôle simultanés sans qu'une déconnexion prématurée coupe tout
     pour les autres.
   - `main.py` : point d'entrée (`python -m rover_core.main --port
     ...`), construit `RoverCore` **à l'intérieur** de la boucle
     asyncio effectivement utilisée par `aiohttp` (`asyncio.run` +
     `AppRunner`/`TCPSite`, pas `web.run_app`) --- pour éviter un bug
     classique de boucle asyncio incohérente entre le thread lecteur
     série et le serveur web.

3. **`pi/rover_control/`** : serveur `aiohttp` (page `/` + WebSocket
   `/ws`) + `static/index.html` --- page de contrôle autonome, sans
   framework JS ni étape de build (une seule dépendance ajoutée :
   `aiohttp`, cf. règle §27.10 "pas de dépendance lourde si une
   solution simple suffit) :
   - Joystick tactile (Pointer Events, un seul modèle pour souris et
     tactile).
   - Manette physique (Gamepad API), prioritaire sur le joystick
     tactile si détectée.
   - Envoi `{velocity, rotation}` en JSON à 10 Hz, indépendant de la
     fréquence `HEARTBEAT` du pont Pi↔ESP32.

4. **`.gitignore`** : ajout de `pi/.venv/`, `__pycache__/`, `*.pyc`.

### Validation

Test de bout en bout, en conditions réelles (pas de mock) contre la
simulation Wokwi déjà en cours (`rfc2217://localhost:4000`) :

- `python -m rover_core.main --port rfc2217://localhost:4000` : connexion
  OK, `serial.serial_for_url()` fonctionne identiquement pour un port
  réel et une URL RFC2217.
- Page `/` servie correctement (bug initial trouvé et corrigé :
  `add_static(show_index=True)` servait un listing de dossier au lieu
  de `index.html` --- remplacé par une route explicite).
- Client WebSocket simulé (script Python, `aiohttp.ClientSession`) :
  - Connexion → `SYSTEM action=resume` puis boucle `HEARTBEAT` (~150 ms)
    envoyés automatiquement.
  - `MOVE velocity=0.20 rotation=0.00` envoyé en boucle → reçu et traité
    par le firmware (mêmes trames que les tests manuels précédents).
  - `STATE left_speed=... right_speed=...` reçu en retour et journalisé
    côté Pi.
  - Déconnexion → `MOVE velocity=0.00 rotation=0.00` immédiat +
    arrêt de la boucle `HEARTBEAT` (l'ESP32 repasse en `SAFE` de
    lui-même ensuite, pas besoin que le Pi le lui dise).
- `protocol.py` revérifié isolément : `encode_frame`/`decode_frame`
  produisent des valeurs identiques à `esp32/tools/rover_frame.py` et
  rejettent bien un checksum invalide.

### Ce qui n'est PAS validé

- Le joystick tactile et la manette (Gamepad API) n'ont pas été
  testés avec un vrai doigt/une vraie manette --- seul le backend
  (WebSocket → `RoverCore.move()`) a été validé par un client simulé.
  Le code utilise des API web standard (Pointer Events, Gamepad API)
  mais reste à confirmer en conditions réelles sur téléphone/manette.
- Aucune authentification ni chiffrement sur le serveur de contrôle ---
  ne pas l'exposer hors d'un réseau local de confiance en l'état.
- Pas de caméra, pas de VPN, pas d'accès distant (reste de la Phase 6).
- Pas testé sur Raspberry Pi physique (développé et validé sur la
  machine de dev Linux Mint, contre la simulation Wokwi).

### État actuel

- **Rover est pilotable en réseau local** (Phase 5 + 6 minimales) :
  interface web (manette ou joystick tactile) → `MOVE` → ESP32,
  validé de bout en bout en simulation.

### Prochaines étapes

- Valider le joystick tactile / la manette avec un vrai téléphone et
  une vraie manette (ouvrir `http://<ip>:8080/` depuis un navigateur).
- Continuer soit la Phase 3 (tête/écran) côté ESP32, soit approfondir
  Phase 5/6 (config, logs, machine à états, caméra) selon la priorité
  de l'utilisateur.

------------------------------------------------------------------------

## Session du 2026-08-25 (suite 4) --- Retour d'état + Phase 3 (en cours)

### Contexte

Demande de l'utilisateur : afficher le retour d'état ESP32 dans
l'interface web, et démarrer la Phase 3 (tête/écran). Session
interrompue par l'utilisateur avant la fin de la Phase 3 --- ce qui
suit distingue clairement ce qui est **fait et validé** de ce qui est
**écrit mais pas encore branché/testé**.

### Retour d'état dans l'UI --- fait et validé

- `RoverCore` (`pi/rover_core/core.py`) expose un pub-sub
  (`add_listener`/`remove_listener`) : n'importe quel abonné reçoit
  chaque trame `STATE`/`EVENT`/`ERROR` reçue de l'ESP32, sans que
  `RoverCore` ait besoin de connaître aiohttp/WebSocket.
- `rover_control/server.py` pousse ces trames en JSON au client
  WebSocket concerné (+ la dernière connue immédiatement à la
  connexion, pour qu'un client qui rejoint en cours de session ne voie
  pas un panneau vide).
- `index.html` affiche trois lignes : `STATE` (permanente, dernière
  valeur), `EVENT`/`ERROR` (s'effacent après 5 s --- ce sont des
  occurrences, pas un statut permanent).
- **Validé** contre la simulation Wokwi en cours : un client WebSocket
  simulé a bien reçu `{"type": "STATE", ...}` poussé en temps réel
  après un `MOVE`.
- Commité (`a0be37c`).

### Phase 3 (tête + écran) --- démarrée, PAS encore intégrée ni testée

Recherche préalable : l'architecture du moteur d'yeux de Lumi
(`/home/raphael/R-Bot-Data/Git/Lumi-Project`) a été étudiée (agent de
survol, lecture seule). Constat : ~180 lignes de logique de rendu
autonome (yeux "glitch" RGB-split, easing blink/regard, icône WiFi),
basée sur Adafruit_GFX + Adafruit_ST7789, mais **pas** une classe
réutilisable --- une tâche FreeRTOS avec des globales. Portage prévu
comme réécriture propre en classe (`DisplayEngine`/`EyeRenderer`), pas
copier-coller.

**Écrit et compile, mais RIEN de tout ça n'est encore branché à
`main.cpp` ni testé (ni en simulation, ni autrement)** :

1. `esp32/include/head_config.h` (nouveau) : pinout servos pitch/yaw
   (GPIO13/19, déjà dans `WIRING.md`), canaux LEDC 4/5 (0--3 réservés
   aux moteurs), limites souples (placeholders, ±30°/±90°), vitesse
   d'interpolation.
2. `esp32/include/display_config.h` (nouveau) : pinout écran ST7789
   (GPIO18/23/5/2/15, déjà dans `WIRING.md`), résolution 240×280
   (reprise de Lumi comme hypothèse de départ, pas confirmée pour
   Rover --- beaucoup d'écrans ST7789 sont 240×240 ou autre).
3. `esp32/lib/head/ServoJoint.{h,cpp}` : pilotage bas niveau d'un servo
   via LEDC (angle relatif au centre → largeur d'impulsion).
4. `esp32/lib/head/HeadController.{h,cpp}` : cible pitch/yaw → servos,
   avec les mêmes precautions que `DriveController` en Phase 2 (garde
   NaN/Inf, clamp aux limites souples, interpolation non bloquante).
5. `esp32/platformio.ini` : ajout de `Adafruit GFX Library` et
   `Adafruit ST7735 and ST7789 Library` en dépendances (résolues avec
   succès par PlatformIO, confirmé par la compilation).

**Compilation vérifiée** sur `esp32_wroom` et `esp32_s3` --- mais
comme rien n'est encore `#include`-é depuis `main.cpp`, le LDF de
PlatformIO ne compile même pas encore ces fichiers (taille du firmware
inchangée par rapport au commit précédent) : ça ne prouve donc **pas**
que `HeadController`/`ServoJoint` compilent réellement ensemble, juste
que le reste du projet n'est pas cassé.

**Pas commencé du tout** : `DisplayEngine`/`EyeRenderer` (rendu des
yeux), `Emotion` enum, dispatch des commandes `HEAD`/`FACE`/`ANIMATION`
dans `main.cpp`, tout test Wokwi (diagramme non étendu pour servos/écran).

### Prochaines étapes

1. Terminer l'intégration `HeadController` dans `main.cpp` (dispatch
   `HEAD`, `head.update()` dans `loop()`) et vérifier que ça compile
   **avec** les fichiers réellement inclus cette fois.
2. Écrire `DisplayEngine`/`EyeRenderer` (yeux + blink/look idle +
   mapping `Emotion`) et le dispatch `FACE`/`ANIMATION`.
3. Étendre `esp32/diagram.json` (servo(s) + écran ST7789 Wokwi) et
   valider en simulation, même méthode que les phases précédentes
   (headless `wokwi-cli`, checksum via `tools/rover_frame.py`).
4. Continuer le retour d'état : valider joystick/manette sur vrai
   téléphone/manette (toujours en attente).

------------------------------------------------------------------------

## Session du 2026-08-25 (suite 5) --- Phase 3 terminée (code + simulation protocole)

### Contexte

L'utilisateur a demandé de terminer la Phase 3 malgré la pause prévue
plus tôt. Suite directe de la session précédente : `HeadController`/
`ServoJoint` existaient déjà mais n'étaient pas branchés, et le moteur
d'yeux n'était pas commencé.

### Ce qui a été fait

1. **`esp32/lib/display/`** (nouveau module) :
   - `EyeState.h` : struct pure décrivant l'état visuel à dessiner
     (regard gauche/droite, ouverture, intensité glitch) --- séparation
     volontaire entre "quoi dessiner" et "pourquoi", pour que le rendu
     reste une fonction pure de l'état.
   - `EyeRenderer.{h,cpp}` : rendu des yeux "glitch" (rectangles arrondis
     RGB décalés cyan/magenta/blanc + iris noire), technique reprise du
     survol de Lumi mais **réécrite** en fonction pure sur
     `Adafruit_GFX`, pas copiée (Lumi n'a pas de classe réutilisable,
     juste une tâche FreeRTOS avec des globales --- voir le survol
     précédent).
   - `Emotion.{h,cpp}` : les 8 émotions de la section 15 de
     `ARCHITECTURE_AND_ROADMAP.md` (IDLE/HAPPY/CURIOUS/SLEEPY/
     CONFUSED/ALERT/SAD/EXCITED), parsing du champ `emotion=` d'une
     trame `FACE`.
   - `DisplayEngine.{h,cpp}` : possède l'écran ST7789 + un profil par
     émotion (ouverture de base, cadence de clignement, biais de
     regard, "wander" idle, glitch continu pour `ALERT`,
     regard asymétrique pour `CONFUSED`) --- **premier jet
     paramétrique** réutilisant un seul modèle d'œil, pas d'art dédié
     par émotion (aucune ressource de design fournie). Une seule
     animation concrète (`GLITCH`, effet RGB-split renforcé, 900 ms)
     démontre le mécanisme `ANIMATION name=...`, pas une bibliothèque
     complète.
   - Détail d'intégration important : `SPI.begin(SCLK, -1, MOSI, CS)`
     avec MISO explicitement désactivé (`-1`), comme sur Lumi --- sinon
     `SPI.begin()` par défaut réserve GPIO19 (MISO VSPI par défaut) qui
     est utilisé ici pour le servo Yaw. Sans ce détail, ça aurait été un
     conflit de pin silencieux.

2. **`main.cpp`** : dispatch `HEAD` (gated sur `ACTIVE`, même logique
   que `MOVE`), `FACE` et `ANIMATION` (non gated --- afficher une
   émotion n'a aucune implication de sécurité physique, contrairement
   au mouvement). `head.update()`/`display.update()` appelés dans
   `loop()`. Un nom d'émotion inconnu est ignoré silencieusement (même
   tolérance qu'une action `SYSTEM` inconnue).

3. **`esp32/diagram.json`** étendu : 2 `wokwi-servo` (pitch/yaw) + 1
   écran `board-st7789` (le nom exact trouvé par tâtonnement via
   `wokwi-cli lint`, qui liste les pins valides d'un type de pièce en
   cas d'erreur --- `wokwi-st7789` n'existe pas).
   **Découverte en testant** : ce chip simulé est fixe en 240×240, pas
   configurable ---`display_config.h` corrigé pour correspondre (240×240
   au lieu de 240×280 repris de Lumi), pour que la simulation et le
   firmware restent cohérents entre eux.

4. **Compilation** vérifiée sur `esp32_wroom` et `esp32_s3` (RAM 6.1%,
   Flash 9.4%) après chaque étape.

5. **Validé en simulation** (headless `wokwi-cli` + RFC2217, sur un
   port isolé 4009 pour ne pas interrompre la session VSCode de
   l'utilisateur, remis à 4000 après coup) :
   - Boot avec tout le code Phase 3 chargé (moteurs + tête + écran) :
     pas de crash, pas de conflit de pin.
   - `HEAD pitch=15 yaw=-20` accepté en état `ACTIVE`.
   - `FACE emotion=happy` accepté.
   - `ANIMATION name=GLITCH` accepté.
   - `FACE emotion=bogus` (émotion inconnue) : ignoré silencieusement,
     pas de crash, pas de trame `ERROR`.
   - `SYSTEM action=diag` après tout ça : `uptime_ms`/`free_heap`
     stables et cohérents --- pas de reset, pas de fuite mémoire.

### Ce qui n'est PAS validé

- **Le rendu visuel des yeux n'a pas pu être vérifié**, seulement le
  dispatch protocole. Tentative de capture d'écran via `wokwi-cli
  --screenshot-part display` : échec, `"Part does not have a valid
  framebuffer: display"` --- limite du chip communautaire
  `github:Whiteeeey/chip-st7789` utilisé par `board-st7789`, pas un
  bug du firmware. Reste à vérifier à l'œil dans le simulateur VSCode
  (interface graphique, hors de portée de ce terminal) ou sur écran
  physique.
- Aucune calibration des limites d'angle des servos ni des profils
  d'émotion contre du matériel réel (tout est provisoire, comme
  d'habitude cette phase-ci).
- Rien testé sur servos/écran physiques.

### État actuel

- **Phase 3 (Tête et écran) : complète au niveau code**, compile sur
  les deux cibles, validée en simulation au niveau protocole
  (dispatch `HEAD`/`FACE`/`ANIMATION` sans crash). Rendu visuel non
  confirmé. Pas de test matériel physique.
- **Phases 0, 1, 2 : inchangées** depuis les sessions précédentes.

### Prochaines étapes

- Vérifier visuellement le rendu des yeux dans VSCode (l'utilisateur
  peut le faire directement, capture impossible depuis ce terminal).
- Phase 4 (Capteurs) ou calibration matérielle des Phases 2/3 dès que
  le robot physique est disponible.
- Retour d'état : toujours en attente de validation joystick/manette
  sur un vrai téléphone/manette.

------------------------------------------------------------------------

## Session du 2026-08-25 (suite 6) --- Retouche des yeux (design + fiabilité)

### Contexte

Retour de l'utilisateur sur le rendu des yeux : forme carrée à coins
arrondis (pas la forme plus rectangulaire du premier jet), effet
glitch cyan/rose **visible en permanence** (pas réservé à `ALERT`),
et une exigence explicite de fiabilité/optimisation + impression de
vie (clignement + mouvement, jamais statique). Dernière étape avant
fin de journée.

### Ce qui a changé

1. **`EyeRenderer.cpp`** :
   - Yeux désormais **carrés** (une seule dimension `eyeSize`, ~36% de
     la largeur d'écran) au lieu du rectangle 35%×45% du premier jet.
   - Rayon d'arrondi recalculé sur `min(largeur, hauteur)` au lieu de
     la largeur seule --- sinon le rayon dépassait la moitié de la
     hauteur pendant un clignement (`h` rétrécit fortement), ce que
     `fillRoundRect` gère mal.
   - **Optimisation** : `EyeRenderer::draw()` ne fait plus
     `fillScreen()` (tout l'écran) à chaque frame (~30 Hz) --- calcule
     désormais une "cellule" fixe par œil (assez grande pour couvrir
     l'ouverture max + le décalage glitch max) et ne redessine que ces
     deux cellules. Le reste de l'écran, jamais dessiné ailleurs, est
     nettoyé une seule fois au boot. Réduction significative du trafic
     SPI par frame.
   - Garde-fous ajoutés : largeur/hauteur jamais négatives, `lookX`/
     `lookY` re-clampés juste avant utilisation dans le calcul de
     position de l'iris.

2. **`DisplayEngine.cpp`** :
   - **Glitch permanent** : `glitchIntensity` de base non nulle pour
     *toutes* les émotions (0.25 à 0.50 selon l'émotion, 0.70 pour
     `ALERT`), plus l'effet `GLITCH` de l'animation par-dessus ---
     avant, seul `ALERT`/l'animation l'activait, le reste était
     "propre". Couleurs cyan/magenta (rose) inchangées, dosage à
     ajuster plus tard selon le retour de l'utilisateur.
   - **Clignement lissé** : remplacé le bascule binaire
     ouvert/fermé par une enveloppe (`sin`) sur 180 ms --- fermeture
     puis réouverture progressive, plus naturel qu'un flash.
   - **Mouvement permanent** : ajout d'un léger balancement continu
     ("breathing", deux sinusoïdes légèrement déphasées) recalculé à
     chaque frame et ajouté *après* le lissage regard/wander --- pas
     accumulé dans l'état, pour ne jamais dériver. Résultat : plus
     aucune émotion n'est parfaitement statique, y compris
     `CURIOUS`/`SLEEPY`/`ALERT`/`SAD` qui n'ont pas de "wander" actif.
   - `profileFor()` appelé une seule fois par cycle `update()` (au lieu
     de deux, une fois dans le calcul du mouvement et une fois dans le
     rendu) --- petite simplification, pas de gain mesurable à ce
     niveau de charge, mais une seule source de vérité par frame.

3. **Compilation** vérifiée sur `esp32_wroom`/`esp32_s3` (RAM 6.1%,
   Flash 9.5%).

4. **Revalidé en simulation** (même méthode : port RFC2217 isolé 4009,
   remis à 4000 après coup) : séquence `ping` → `FACE curious` → 1.5 s
   d'attente (laisser tourner clignement/mouvement) → `ANIMATION
   GLITCH` → 1.5 s → `FACE alert` → 1.5 s → `diag`. Résultat après
   ~4.5 s de simulation (~135 frames de rendu) : aucun crash,
   `uptime_ms`/`free_heap` cohérents, pas de fuite mémoire.

### Ce qui n'est toujours PAS validé

- Toujours pas de vérification visuelle (même limite que la session
  précédente : le chip `board-st7789` simulé n'expose pas de
  framebuffer capturable par `wokwi-cli`). Le dosage exact des
  couleurs/intensité du glitch est une estimation à ajuster une fois
  visible --- l'utilisateur l'a explicitement anticipé ("on ajustera
  après").
- Rien testé sur écran physique.

### État actuel

- Phase 3 inchangée dans son état fonctionnel (toujours complète au
  niveau code/protocole) --- cette session ne fait qu'affiner le rendu
  et durcir `EyeRenderer`/`DisplayEngine`.

### Prochaines étapes

- Vérifier visuellement le rendu (VSCode ou matériel réel) et ajuster
  le dosage du glitch/les proportions selon le retour de l'utilisateur.
- Reste identique à la session précédente : Phase 4, calibration
  matérielle Phases 2/3, validation joystick/manette physique.

------------------------------------------------------------------------

## Session du 2026-08-31 --- Premier test matériel réel (écran) + rendu des yeux validé

### Contexte

Premier branchement de matériel physique sur le projet (ESP32 WROOM +
écran ST7789), demande explicite de l'utilisateur : valider uniquement
l'affichage (yeux/glitch), sans moteurs/servos câblés. Session menée
depuis une machine Windows (pas l'environnement Linux Mint habituel du
projet).

### Mise en place de l'outillage de test

- **Driver CP210x manquant** : Windows ne détectait aucun port COM pour
  l'ESP32 (`CP2102 USB to UART Bridge Controller` en erreur, code 28
  "drivers not installed"). Résolu par l'utilisateur en installant le
  driver Silicon Labs officiel --- port `COM10` apparu ensuite.
- **PlatformIO CLI installé** (`pip install platformio`) plutôt que de
  passer par l'extension VS Code : `PROGRESS.md` (session 2026-08-25)
  documentait déjà un moniteur série VS Code peu fiable en saisie pour
  ce genre de test ; le CLI + un script `pyserial` dédié
  (`rover_test.py`, même logique que `esp32/tools/rover_frame.py` +
  `pi/rover_esp32/link.py`) permet d'envoyer les trames et de lire les
  réponses directement, sans dépendre de l'UI.
- Build + upload confirmés fonctionnels via
  `pio run -e esp32_wroom -t upload --upload-port COM10`.

### Écran : deux problèmes identifiés au premier essai, corrigés

1. **Mauvaise résolution/orientation** : `display_config.h` utilisait
   240x240 (valeur calée sur le chip simulé Wokwi, jamais confirmée sur
   le vrai panneau --- voir PROGRESS.md 2026-08-25 suite 5). Le panneau
   réel est en fait **240x280** (comme celui de Lumi) et s'affichait en
   portrait alors que Rover est prévu en paysage. Corrigé :
   - `display_config.h` distingue maintenant les dimensions natives du
     panneau (`ROVER_DISPLAY_PANEL_WIDTH/HEIGHT` = 240x280, portrait)
     des dimensions logiques utilisées par tout le rendu
     (`ROVER_DISPLAY_WIDTH/HEIGHT` = 280x240, paysage).
   - `DisplayEngine::begin()` appelle `_tft.init()` avec les dimensions
     natives puis `_tft.setRotation(ROVER_DISPLAY_ROTATION)` (valeur
     `1`, 90° --- `3` en secours si l'image sort inversée/miroir selon
     le sens de câblage du ruban, ce que le logiciel ne peut pas
     deviner).
   - Conséquence pour Wokwi : le chip simulé `board-st7789` reste fixe
     en 240x240, donc la simulation ne représente plus fidèlement le
     rendu visuel réel (elle reste valide au niveau protocole
     seulement, ce qui était déjà sa seule garantie jusqu'ici).

2. **Effet glitch "fouillis de couleur"** : la première version
   remplissait l'œil trois fois (copie cyan décalée, copie magenta
   décalée, blanc par-dessus) --- correct en théorie (le blanc ne
   laissait dépasser que de fines bandes cyan/magenta sur les bords),
   mais rendu réel jugé confus par l'utilisateur. Retravaillé en trois
   passes de retouche (`EyeRenderer.cpp`) suite aux retours successifs :
   - Œil toujours un remplissage blanc unique et propre (jamais teinté).
   - Contour permanent : une ligne cyan et une ligne rose tracées
     (`drawRoundRect`, pas un remplissage) légèrement décalées de
     part et d'autre de l'œil --- épaissi à 2px sur un offset de 2px
     après un premier essai jugé pas assez visible.
   - Glitch animé : quelques traits fins (1px d'épaisseur) positionnés
     aléatoirement à chaque frame (~30 Hz, aucun état mémorisé --- un
     nouveau tirage indépendant à chaque redraw, ce qui donne l'effet
     de scintillement), favorisés à 65% horizontaux après retour
     explicite de l'utilisateur, fréquence/nombre de traits liés à
     `glitchIntensity` (donc plus dense en `ALERT`/`ANIMATION=GLITCH`).
   - Rayon des coins arrondis augmenté (`taille/4` → `taille/3`) sur
     demande.
   - Un bug de compilation `esp32_s3` a été corrigé au passage :
     `std::max(int, long)` ne compile pas (déduction de template
     ambiguë) --- `random()` renvoie un `long`, les appels ont été
     explicitement castés en `int`.

### Validation

- Compilation vérifiée sur `esp32_wroom` et `esp32_s3` après chaque
  itération (RAM ~6%, Flash ~9.6%).
- Flash réel + script de test (`SYSTEM ping` → 7 émotions `FACE` →
  `ANIMATION GLITCH` → `SYSTEM diag`) rejoué à chaque itération :
  aucune trame `ERROR`, `uptime_ms`/`free_heap` stables (pas de fuite,
  pas de reset) sur toute la session.
- **Rendu visuel confirmé par l'utilisateur sur écran physique réel**
  (validation humaine, pas automatisable) après la dernière itération :
  taille des yeux, orientation paysage, coins arrondis, contour
  cyan/rose et glitch jugés corrects.

### État actuel

- **Phase 3 (Tête et écran)** : le volet écran est maintenant
  **validé visuellement sur matériel réel**, en plus du code/protocole
  déjà validés précédemment. Les servos (pitch/yaw) restent non testés
  sur matériel physique --- prochaine étape naturelle si du matériel
  reste disponible.
- `display_config.h` porte maintenant la résolution réelle confirmée
  (240x280) au lieu d'une valeur provisoire calée sur Wokwi.

### Prochaines étapes

- Tester les servos tête (pitch/yaw) sur matériel réel, même méthode
  (script `pyserial` + trames `HEAD`, mais nécessite `SYSTEM
  action=resume`/un flux `HEARTBEAT` puisque `HEAD` est gated sur
  `ACTIVE`, contrairement à `FACE`/`ANIMATION`).
- Phase 4 (capteurs) ou calibration matérielle Phase 2 (moteurs/PID)
  dès que ce matériel est disponible.
- Validation joystick/manette sur un vrai téléphone/manette (toujours
  en attente depuis la session du 2026-08-25).

------------------------------------------------------------------------

## Session du 2026-08-31 (suite) --- Phase 4 (capteurs) écrite, deux bugs réels trouvés au test matériel

### Contexte

Le châssis est encore en impression 3D et les moteurs/capteur de
distance/borniers ne sont pas encore montés/reçus au moment d'écrire ce
code -- décision de l'utilisateur : avancer sur le code de la Phase 4
(capteurs) en s'appuyant sur le roadmap, pendant l'attente matérielle,
plutôt que de rester bloqué. Demande explicite : prévoir des phases de
test/validation par étapes pour quand le matériel arrivera (voir la
section "Plan de test" ci-dessous).

### Ce qui a été écrit

Nouveau module `esp32/lib/sensors/` (Phase 4 complète au niveau code) :

- `DistanceSensor.{h,cpp}` : les deux VL53L0X, séquence XSHUT (gauche
  réadressé 0x30, droit reste 0x29), mode "continuous ranging" (pas de
  lecture bloquante dans `update()`), `EVENT name=obstacle_detected`
  avec hystérésis (deux seuils, `sensors_config.h`).
- `ImuSensor.{h,cpp}` : MPU6050, lecture accel/gyro périodique.
- `EnvironmentSensor.{h,cpp}` : BME688 (registres compatibles BME680 --
  température/humidité/pression/gaz), non-bloquant via
  `beginReading()`/`endReading()` au lieu du `performReading()` bloquant
  de la bibliothèque (une conversion BME680 prend 150-300 ms, ce qui
  aurait gelé le PID moteurs/l'écran à chaque cycle).
- `SensorHub.{h,cpp}` : agrège les trois, détecte les transitions
  OK→échec (y compris dès le premier `begin()` au boot, pas seulement
  une perte en cours de route) pour émettre `ERROR
  code=sensor_timeout sensor=<nom>` une seule fois par capteur --
  jamais de spam à chaque nouvelle tentative de reconnexion (toutes les
  5 s, `ROVER_SENSOR_RETRY_PERIOD_MS`).
- `main.cpp` : `sensors.begin()`/`update()`, drain des erreurs capteur
  et de l'événement obstacle à chaque `loop()`, télémétrie `STATE`
  (distance/IMU/environnement) à 500 ms -- **jamais gated sur
  ACTIVE/SAFE**, contrairement à `MOVE`/`HEAD` : la conscience
  situationnelle (obstacle, santé capteur) reste utile au Pi même robot
  à l'arrêt.
- `platformio.ini` : ajout des dépendances Adafruit (BusIO, Unified
  Sensor, MPU6050, `Adafruit_VL53L0X`, BME680 Library) -- résolues et
  compilées avec succès sur les deux cibles (RAM 6.8%, Flash 11.1% sur
  S3).
- `ROVER_PROTOCOL.md` : documenté les nouveaux champs `STATE`
  (humidity/pressure/gas_kohm, accel_x..gyro_z) et le champ optionnel
  `sensor=` sur `ERROR code=sensor_timeout`.
- `WIRING.md` : adresses I2C (VL53L0X gauche réadressé 0x30, MPU6050
  0x68, BME688 0x77 -- ces deux dernières non confirmées sur le matériel
  réel) + note sur la sonde de présence I2C obligatoire (voir bug
  ci-dessous).

### Deux bugs réels trouvés en testant sur l'ESP32 WROOM physique (aucun capteur câblé)

Le test volontaire "aucun capteur branché" (situation exacte du jour :
tout est en attente de livraison/impression) a servi de test de
robustesse -- et a immédiatement révélé deux bugs qu'aucune compilation
ni simulation Wokwi n'aurait pu attraper :

1. **Plantage en boucle au boot (watchdog matériel).** `sensors.begin()`
   plantait systématiquement le firmware ~6 s après le boot (jamais
   atteint `loop()`). Diagnostic par instrumentation temporaire
   (`Serial.printf` entre chaque `begin()` de capteur) : le blocage
   était entièrement à l'intérieur de `DistanceSensor::begin()` --
   l'API VL53L0X d'origine de ST (vendue telle quelle par
   `Adafruit_VL53L0X`, fichiers `vl53l0x_api*.cpp`) reste bloquée
   indéfiniment dans une boucle d'attente interne (`VL53L0X_WaitDevice-
   Booted` ou équivalent) au lieu d'échouer proprement quand rien ne
   répond sur le bus I2C -- un vrai piège pour ce cas précis (0 capteur
   câblé). Une première tentative de correction (nourrir le watchdog +
   réduire le timeout I2C via `Wire.setTimeOut(50)`) n'a rien changé
   (le blocage est interne à un seul appel bibliothèque, invisible de
   l'extérieur). **Correction retenue** : sonder la présence du capteur
   (`Wire.beginTransmission`/`endTransmission`, `I2CProbe.h`) **avant**
   d'appeler le `begin()` de la bibliothèque, jamais après -- appliqué
   aux trois capteurs (VL53L0X x2, MPU6050, BME688) par précaution,
   même si seul le VL53L0X a été prouvé bloquant. Root cause vérifiée :
   confirmé par la trace de debug avant/après.
2. **Trames `ERROR`/`STATE` tronquées en silence.** Une fois le crash
   réglé, les trames sorties étaient coupées au milieu d'un mot (`ERROR
   ... sensor=tof_` au lieu de `tof_left`/`tof_right`, `STATE ...
   gyro_y=0.00` sans `gyro_z` du tout) -- buffers `char[32]`/`char[64]`
   dans `main.cpp` trop petits pour le contenu réel (`sensor=tof_right`
   à lui seul fait 37 octets avec le terminateur ; les 6 champs IMU à
   zéro font ~75 octets). Corrigé en passant à `char[48]`/`char[96]`
   avec un commentaire expliquant le calcul, pour que ça ne se
   reproduise pas silencieusement si un nom de champ s'allonge plus
   tard.

### Validation

- Compilation vérifiée sur `esp32_wroom` et `esp32_s3` après chaque
  correction.
- Flash réel + script de test (`SYSTEM ping` → 7 émotions `FACE` →
  `ANIMATION GLITCH` → `SYSTEM diag`, script `rover_test.py`) : plus
  aucun crash, boot stable, les 4 `ERROR code=sensor_timeout
  sensor=<tof_left|tof_right|imu|bme688>` bien émis une seule fois au
  boot (aucun capteur câblé, comportement attendu), `STATE
  distance_left=9999 distance_right=9999` (sentinelle "indisponible"),
  `STATE accel_x=0.00 ... gyro_z=0.00` et `STATE temperature=0.0 ...
  gas_kohm=0.0` complets et bien formés, `uptime_ms`/`free_heap`
  stables sur `SYSTEM action=diag`.
- **Lecture effective d'un capteur réel non testée** (aucun n'est
  encore câblé) -- seule la robustesse "capteur absent" est validée à
  ce stade.

### Plan de test pour la suite (par étapes, au fur et à mesure du matériel)

1. **VL53L0X + borniers (attendu aujourd'hui, 2026-08-31)** : câbler
   un ou deux VL53L0X selon ce qui est reçu (XSHUT gauche/droit, SDA/SCL
   partagés -- voir `WIRING.md`). Reflasher le firmware actuel (aucun
   changement de code nécessaire). Attendu : plus d'`ERROR
   sensor_timeout sensor=tof_left/tof_right` pour le(s) capteur(s)
   câblé(s), `STATE distance_left=...`/`distance_right=...` avec des
   valeurs plausibles en mm, `EVENT name=obstacle_detected` en
   approchant la main à moins de 150 mm.
2. **Moteurs + PID (dès châssis + borniers prêts)** : reprendre la
   Phase 2, câblage propre au lieu de fils volants, calibrer les gains
   PID et `ROVER_ENCODER_TICKS_PER_REV`/`ROVER_WHEEL_DIAMETER_M` dans
   `motion_config.h` contre les vraies roues/moteurs N20.
3. **Servos tête** (dès la tête montée sur le châssis) : `HEAD
   pitch=... yaw=...` via le même script de test, ajuster les limites
   souples de `head_config.h` si nécessaire.
4. **MPU6050 / BME688** (dès réception) : même principe qu'à l'étape 1
   -- câbler, reflasher sans changement de code, vérifier que l'`ERROR`
   correspondant disparaît et que les valeurs `STATE` sont plausibles
   (accel_z proche de 9.8 au repos, température proche de l'ambiante).
5. **Raspberry Pi** (après validation moteurs, décision explicite de
   l'utilisateur) : brancher l'UART réel Pi↔ESP32, lancer `rover_core`/
   `rover_control` déjà écrits (Phase 5/6 minimales) contre le matériel
   réel au lieu de la simulation Wokwi.

### État actuel

- **Phase 4 (capteurs)** : complète au niveau code, compile sur les
  deux cibles, **validée sur ESP32 WROOM physique** pour le cas "aucun
  capteur câblé" (robustesse). Lecture réelle des capteurs à valider
  dès réception/câblage (étape 1 du plan ci-dessus).
- **Phases 0, 1, 2, 3** : inchangées.
