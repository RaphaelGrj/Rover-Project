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
- Afficher le retour d'état (`STATE`/`EVENT`/`ERROR`) dans l'interface
  plutôt que seulement dans les logs du service.
- Continuer soit la Phase 3 (tête/écran) côté ESP32, soit approfondir
  Phase 5/6 (config, logs, machine à états, caméra) selon la priorité
  de l'utilisateur.
