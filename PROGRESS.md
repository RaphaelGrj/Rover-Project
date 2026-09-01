# ROVER --- État actuel (résumé de reprise)

> Fichier de reprise rapide --- objectif : que je puisse me repérer sans
> tout relire. Statut détaillé par phase : voir
> `ARCHITECTURE_AND_ROADMAP.md`. Détail complet de chaque session
> (mesures, hypothèses écartées, méthode de diagnostic) : voir
> `PROGRESS_ARCHIVE.md` --- à rouvrir seulement pour retrouver le contexte
> fin d'une décision ou d'un test passé précis ; le journal court
> ci-dessous suffit normalement à se repérer.

------------------------------------------------------------------------

## État actuel (fil ouvert, mis à jour en continu)

- **Moteurs** : les 2 tournent ensemble (résolu 2026-09-01 --- marge de
  tension/courant insuffisante à 6V sous charge combinée, réglé en
  passant à 9V). À revalider vers 6-7V une fois les connexions soudées,
  pour ne pas faire tourner les N20 en surrégime en continu.
- **Encodeurs** : **aucun des deux n'est défectueux** --- correction par
  rapport à la conclusion précédente (LED éteinte malgré 3.3V présents,
  qui s'est avérée être un mauvais contact, pas un capteur mort). Chaque
  encodeur s'allume normalement testé seul. **Problème restant, non
  résolu à la pause** : les deux ne s'allument jamais ensemble, même
  câblés sur deux fils 3V3 distincts --- c'est toujours le côté OUT3/OUT4
  qui gagne, l'autre reste éteint dès que les deux sont branchés.
  Suspecté : rail 3V3 de la breadboard dégradé à un endroit précis,
  jamais confirmé. Prochaine session : tester en inversant l'ordre de
  branchement, sinon déplacer sur un point neuf du rail, sinon souder.
  Une fois réglé, reconfirmer avec `move_diagnostic.py --move` que
  `left_speed`/`right_speed` sont comparables.
- Breadboard peu fiable sur les manipulations longues (plusieurs faux
  positifs aujourd'hui à cause de contacts qui bougent) --- souder les
  points qui ont posé problème (jumpers GPIO↔driver, VCC/GND/signal
  encodeur) est recommandé avant la prochaine session de bring-up.

## Prochaines étapes

1. Résoudre le partage du rail 3V3 entre les 2 encodeurs (voir ci-dessus
   --- ordre de branchement, point neuf, ou soudure).
2. Confirmer par télémétrie (`move_diagnostic.py --move`) que les deux
   vitesses sont comparables une fois les deux LED allumées ensemble.
3. Souder les connexions instables identifiées aujourd'hui (jumpers
   GPIO↔driver, VCC/GND/signal encodeur).
4. Revalider à une tension proche du nominal (6-7V) une fois soudé.
5. Calibrer PID + géométrie roue une fois le mouvement stable des deux
   côtés.
6. Commande AliExpress en cours (nouveaux moteurs N20 --- pas strictement
   nécessaire, les actuels ne sont pas défectueux --- + système
   d'alimentation : 2×18650 en série (7.4V), BMS 2S avec charge USB-C
   intégrée, buck converters servos/logique). Reste à trancher : le
   Raspberry Pi tournera-t-il sur ce même pack ou une alim séparée ---
   ça dimensionne le BMS/buck à choisir.
7. Servos tête : câblage signal documenté (`WIRING.md`/`head_config.h`),
   mais pas encore testés sur matériel réel, et leur alimentation
   (partager le rail moteurs, pas l'ESP32) reste à ajouter à `WIRING.md`.
8. Reste identique par ailleurs (voir `ARCHITECTURE_AND_ROADMAP.md`) :
   VL53L0X/MPU6050/BME688, Raspberry Pi physique, caméra, MQTT contre un
   vrai broker, buzzer audible, VPN en conditions réelles.

------------------------------------------------------------------------

## Journal court (une ligne par session --- détail complet dans PROGRESS_ARCHIVE.md)

- **2026-09-01 (aujourd'hui)** --- Bring-up moteurs résolu (9V) ; LED
  encodeur d'une roue trouvée éteinte puis rallumée en rebranchant les
  câbles (mauvais contact, pas un capteur mort --- correction en cours de
  session) ; PROGRESS.md scindé en résumé + archive pour alléger les
  reprises de session.
- **2026-09-01 (suite, pause)** --- Chasse à l'instabilité ESP32 sous
  charge : câble USB défectueux trouvé (cause principale), découplage
  driver ajouté, 2 boards DRV8833 chacune avec un canal faible différent
  identifiées ; combiner les deux a dégradé les deux canaux --- session
  interrompue en pause, non résolue à ce stade.
- **2026-09-01** --- Premier bring-up moteurs/encodeurs réels : 3 bugs
  matériels trouvés et corrigés (SLEEP non câblé, encodeurs sans pull-up,
  1er board DRV8833 canal B mort → remplacé).
- **2026-08-31 (suite 7)** --- Phase 6 terminée : guide VPN WireGuard
  auto-hébergé complet (non testé, pas de Pi/routeur disponibles).
- **2026-08-31 (suite 6)** --- Commandes HEAD branchées dans l'UI de
  contrôle (pad tactile + manette), validé de bout en bout contre l'ESP32
  réel.
- **2026-08-31 (suite 5)** --- Buzzer (GPIO12), réflexe d'arrêt sur
  obstacle (Pi), comportement "sommeil" en idle, 2 animations
  (LOOK_AROUND/WAKE_UP), publication MQTT minimale.
- **2026-08-31 (suite 4)** --- Tests Python (25, bug hmac sur chaîne vide
  corrigé), HTTPS/WSS, service systemd (non testable sous Windows),
  encodeur Wokwi investigué mais volontairement pas implémenté,
  calibration PID sans reflash (NVS, validée à travers un vrai reboot).
- **2026-08-31 (suite 3)** --- Validation visuelle UI (claude-in-chrome)
  + 9 chantiers sécurité/robustesse : E-stop, batterie (désactivée par
  défaut), OTA (fail-closed, aucun secret en dur), config/logs Pi, mDNS,
  authentification obligatoire partout, dépendances épinglées,
  permissions CI minimales, vidéo MJPEG (pas de caméra).
- **2026-08-31 (suite 2)** --- CI GitHub Actions, tests unitaires natifs
  (RoverProtocol/WheelPID/Emotion), IMU simulé dans Wokwi, machine à
  états comportementale `rover_core`, UI capteurs fusionnée.
- **2026-08-31 (suite)** --- Phase 4 (capteurs) écrite et testée sur ESP32
  réel sans capteur câblé : 2 bugs trouvés (VL53L0X bloquant sans sonde
  de présence I2C, buffers ERROR/STATE trop petits, tronquaient en
  silence).
- **2026-08-31** --- Premier test matériel réel (écran) : driver CP210x
  installé, résolution corrigée 240×280 paysage, glitch retravaillé ---
  rendu visuel confirmé sur écran physique réel.
- **2026-08-25 (suite 6)** --- Retouche des yeux : forme carrée, glitch
  permanent, clignement lissé, mouvement "breathing" continu (toujours
  pas de vérif visuelle possible en headless).
- **2026-08-25 (suite 5)** --- Phase 3 terminée au niveau code : yeux/
  écran/tête, dispatch HEAD/FACE/ANIMATION, validé en simulation
  protocole seulement (pas de rendu visuel capturable en headless).
- **2026-08-25 (suite 4)** --- Retour d'état dans l'UI (validé) ; Phase 3
  démarrée (HeadController/ServoJoint écrits mais pas encore branchés).
- **2026-08-25 (suite 3)** --- Rover pilotable en local : `pi/` créé
  (protocole, RoverCore, serveur de contrôle + UI joystick/manette),
  validé de bout en bout contre la simulation Wokwi.
- **2026-08-25 (suite 2)** --- Durcissement Phase 2 : spinlock ESP32 pour
  l'encodeur, dérivée PID recalculée sur la mesure (pas l'erreur), garde
  NaN/Inf + clamp rotation.
- **2026-08-25 (suite)** --- Phase 2 (motorisation) écrite : DRV8833
  (remplace le TB6612FNG initialement prévu), moteurs/encodeurs/PID,
  validée en simulation Wokwi (boucle ouverte, pas d'encodeur simulé).
- **2026-08-25** --- Environnement Wokwi mis en place ; Phase 1 validée
  entièrement en simulation (boot, ping, diag, timeout heartbeat,
  checksum, resume).
- **2026-08-23** --- Phase 0 (architecture) figée ; scaffold Phase 1 ESP32
  écrit (Rover Protocol, heartbeat, watchdog, diagnostics), compile sur
  les deux cibles, pas encore testé sur matériel réel.
