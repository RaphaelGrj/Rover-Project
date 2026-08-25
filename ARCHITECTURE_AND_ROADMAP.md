# ROVER --- Architecture de référence et feuille de route

> **Document de référence pour le développement logiciel et électronique
> de Rover.**
>
> Ce document définit l'architecture cible du robot, la séparation des
> responsabilités entre le Raspberry Pi et l'ESP32, leur protocole de
> communication, les règles de développement et la roadmap.
>
> **Principe fondateur :**
>
> **Le Raspberry Pi décide *quoi faire*. L'ESP32 décide *comment le
> faire physiquement*.**

------------------------------------------------------------------------

## 1. Objectif du document

Ce fichier doit servir de **contrat d'architecture** pour Rover.

Toute IA, développeur ou contributeur qui intervient sur le projet doit
commencer par respecter ce document avant de modifier l'architecture.

L'objectif est d'éviter qu'au fil du développement :

-   la logique soit dupliquée entre le Raspberry Pi et l'ESP32 ;
-   le Raspberry Pi pilote directement des GPIO ou des périphériques
    temps réel ;
-   l'ESP32 se retrouve à gérer de l'IA ou de la logique métier complexe
    ;
-   une fonctionnalité casse une autre partie du robot ;
-   le protocole de communication devienne une suite de commandes
    improvisées ;
-   une IA de programmation reconstruise l'architecture différemment à
    chaque nouvelle session.

Ce document est donc la **source de vérité architecturale** de Rover.

------------------------------------------------------------------------

# 2. Vision générale de Rover

Rover est l'évolution mobile de Lumi.

Lumi est un compagnon de bureau.

Rover reprend son expressivité et son identité R-Bot, mais ajoute :

-   la mobilité ;
-   la perception de l'environnement ;
-   la vision ;
-   la navigation ;
-   l'interaction avec la maison ;
-   une IA capable de prendre des décisions ;
-   une personnalité dynamique.

Rover doit être considéré comme un **système robotique distribué**, et
non comme un simple ESP32 connecté à un Raspberry Pi.

L'architecture est divisée en deux niveaux :

``` text
                    ROVER
                      │
          ┌───────────┴───────────┐
          │                       │
     RASPBERRY PI 3B+             ESP32
      "CERVEAU"             "SYSTÈME NERVEUX"
          │                       │
    décision / IA             temps réel
    vision / réseau           moteurs
    navigation                servos
    personnalité              capteurs
    Home Assistant            écran
    caméra                    sécurité
```

------------------------------------------------------------------------

# 3. Règle d'or de l'architecture

## Raspberry Pi

Le Raspberry Pi décide :

> **QUOI faire**

Exemples :

-   avancer vers une personne ;
-   regarder quelqu'un ;
-   lancer une animation ;
-   aller dans une pièce ;
-   suivre une personne ;
-   effectuer une patrouille ;
-   parler ;
-   réagir à une alerte ;
-   transmettre une information à Home Assistant.

## ESP32

L'ESP32 décide :

> **COMMENT réaliser physiquement cette action**

Exemples :

-   générer le PWM des moteurs ;
-   gérer les PID ;
-   lire les encodeurs ;
-   commander les servomoteurs ;
-   actualiser l'écran ;
-   lire les capteurs I2C ;
-   appliquer les limites mécaniques ;
-   arrêter les moteurs en cas de perte de communication.

### Règle absolue

Le Raspberry Pi **ne doit pas piloter directement** les moteurs, servos,
écran ou capteurs temps réel.

L'ESP32 **ne doit pas gérer** l'IA, la navigation complexe, Home
Assistant ou la logique métier principale.

------------------------------------------------------------------------

# 4. Architecture matérielle

## 4.1 Raspberry Pi 3B+

Le Raspberry Pi est l'ordinateur de bord.

Responsabilités :

-   IA ;
-   vision ;
-   caméra ;
-   reconnaissance ;
-   logique comportementale ;
-   personnalité ;
-   navigation haut niveau ;
-   communication réseau ;
-   Home Assistant ;
-   MQTT/API ;
-   serveur vidéo ;
-   audio haut niveau ;
-   gestion des services Linux ;
-   journalisation ;
-   configuration globale.

Le Pi peut évoluer vers une plateforme plus puissante sans remettre en
cause l'architecture.

------------------------------------------------------------------------

## 4.2 ESP32

L'ESP32 est le contrôleur temps réel.

Responsabilités :

-   moteurs ;
-   encodeurs ;
-   PID ;
-   servomoteurs de tête ;
-   écran ST7789 ;
-   capteurs I2C ;
-   états matériels ;
-   sécurité ;
-   watchdog ;
-   alimentation/état matériel si nécessaire ;
-   audio bas niveau si cette partie est conservée sur l'ESP32.

L'ESP32 doit rester fonctionnel même si le Raspberry Pi est lent, occupé
ou temporairement indisponible.

------------------------------------------------------------------------

# 5. Périphériques et propriétaire matériel

Chaque périphérique doit avoir un propriétaire clair.

  Périphérique        Propriétaire   Rôle
  ------------------- -------------- --------------------------
  Moteur gauche       ESP32          PWM + PID
  Moteur droit        ESP32          PWM + PID
  Encodeurs           ESP32          vitesse / odométrie
  DRV8833             ESP32          puissance moteurs
  Servo Pitch         ESP32          mouvement tête
  Servo Yaw           ESP32          mouvement tête
  ST7789              ESP32          yeux / animations
  MPU6050             ESP32          IMU
  VL53L0X gauche      ESP32          distance
  VL53L0X droite      ESP32          distance
  BME688              ESP32          environnement
  Caméra              Raspberry Pi   vision
  IA                  Raspberry Pi   raisonnement
  Réseau              Raspberry Pi   Wi-Fi / services
  Home Assistant      Raspberry Pi   domotique
  Navigation          Raspberry Pi   décisions de déplacement
  Audio haut niveau   Raspberry Pi   STT / TTS / logique

Cette table doit rester la référence avant l'ajout d'un nouveau
composant.

------------------------------------------------------------------------

# 6. Communication Raspberry Pi ↔ ESP32

## 6.1 Technologie retenue

La communication principale cible est :

**UART série.**

L'I2C peut rester disponible pour certains besoins matériels
spécifiques, mais **I2C ne doit pas être le protocole principal de
communication entre les deux ordinateurs**.

Pourquoi ?

Le Pi et l'ESP32 sont deux systèmes informatiques qui échangent des
messages structurés.

UART est plus adapté à ce rôle :

``` text
Raspberry Pi
     │
     │ UART
     │
     ▼
 ESP32
```

------------------------------------------------------------------------

# 7. Rover Protocol

La communication entre les deux systèmes doit être encapsulée dans un
protocole nommé :

**Rover Protocol**

Le protocole doit être documenté séparément dans le projet lorsqu'il
sera implémenté.

Il doit permettre deux grandes catégories de messages :

``` text
COMMAND
STATE / EVENT
```

------------------------------------------------------------------------

## 7.1 COMMAND

Les commandes sont envoyées principalement :

``` text
Raspberry Pi → ESP32
```

Exemples :

``` text
MOVE
HEAD
FACE
ANIMATION
AUDIO
LIGHT
SYSTEM
```

### MOVE

Exemple conceptuel :

``` text
MOVE velocity=0.25 rotation=-0.10
```

Le Pi ne demande pas :

``` text
GPIO 12 HIGH
GPIO 13 PWM 73
```

Il demande une **intention physique** :

``` text
avance à telle vitesse
avec telle rotation
```

L'ESP32 transforme ensuite cette intention en commande moteur.

------------------------------------------------------------------------

## 7.2 HEAD

Exemple :

``` text
HEAD pitch=15 yaw=-20
```

L'ESP32 gère :

-   limites mécaniques ;
-   interpolation ;
-   vitesse ;
-   accélération ;
-   position réelle si disponible ;
-   sécurité.

------------------------------------------------------------------------

## 7.3 FACE

Exemple :

``` text
FACE emotion=happy
```

L'ESP32 choisit l'affichage correspondant.

Le Raspberry Pi ne doit pas dessiner chaque pixel de l'écran.

------------------------------------------------------------------------

## 7.4 ANIMATION

Exemple :

``` text
ANIMATION name=GLITCH_03
```

Les animations doivent être stockées côté ESP32.

Cela permet de conserver un comportement graphique fluide même si le Pi
est occupé.

------------------------------------------------------------------------

# 8. Messages ESP32 → Raspberry Pi

L'ESP32 doit pouvoir transmettre :

``` text
STATE
EVENT
ERROR
ACK
```

Exemples :

``` text
STATE battery=82
STATE left_speed=0.24 right_speed=0.26
STATE distance_left=420
STATE distance_right=380
STATE temperature=24.3
```

Événements :

``` text
EVENT obstacle_detected
EVENT robot_lifted
EVENT head_limit
EVENT low_battery
```

Erreurs :

``` text
ERROR motor_overcurrent
ERROR sensor_timeout
ERROR display_failure
```

------------------------------------------------------------------------

# 9. Heartbeat et sécurité

Le protocole doit intégrer un **heartbeat**.

Le Raspberry Pi envoie régulièrement :

``` text
HEARTBEAT
```

L'ESP32 surveille la dernière réception.

Si aucune communication valide n'est reçue pendant un délai défini :

``` text
TIMEOUT
   ↓
STOP MOTORS
   ↓
SAFE STATE
```

Valeur initiale recommandée pour les essais :

**500 ms**

Cette valeur pourra être ajustée après tests.

### Important

La sécurité ne doit jamais dépendre du Raspberry Pi.

Si Linux plante :

``` text
Pi ❌
       ↓
ESP32 détecte absence de heartbeat
       ↓
moteurs = 0
       ↓
robot sécurisé
```

------------------------------------------------------------------------

# 10. Architecture logicielle du Raspberry Pi

Le Raspberry Pi doit être organisé autour de services/modules
indépendants.

Architecture cible :

``` text
Raspberry Pi
│
├── rover-core
│   └── état global / orchestration
│
├── rover-ai
│   └── IA / raisonnement / personnalité
│
├── rover-vision
│   └── caméra / détection / reconnaissance
│
├── rover-navigation
│   └── déplacement haut niveau / navigation
│
├── rover-audio
│   └── STT / TTS / audio
│
├── rover-homeassistant
│   └── MQTT / API / domotique
│
├── rover-esp32
│   └── Rover Protocol
│
└── rover-config
    └── configuration globale
```

Les noms exacts de dossiers et de services peuvent évoluer.

La séparation des responsabilités, elle, ne doit pas disparaître.

------------------------------------------------------------------------

# 11. Architecture logicielle ESP32

L'ESP32 doit également être modulaire.

Architecture cible :

``` text
ESP32
│
├── communication
│   └── Rover Protocol
│
├── motors
│   ├── driver
│   ├── encoders
│   └── PID
│
├── head
│   ├── pitch
│   └── yaw
│
├── display
│   ├── renderer
│   ├── eyes
│   ├── emotions
│   └── animations
│
├── sensors
│   ├── IMU
│   ├── ToF
│   └── BME688
│
├── safety
│   ├── watchdog
│   ├── communication timeout
│   └── limits
│
└── system
    ├── state
    └── diagnostics
```

------------------------------------------------------------------------

# 12. Les moteurs : séparation stricte

Le Raspberry Pi peut décider :

``` text
"Je veux aller à 0.30 m/s avec une rotation de 0.10 rad/s."
```

L'ESP32 reçoit cette consigne.

Ensuite :

``` text
vitesse linéaire + rotation
            ↓
   vitesse gauche/droite
            ↓
       PID moteurs
            ↓
         PWM
            ↓
        moteurs
```

Les encodeurs fournissent le retour :

``` text
moteur
  ↓
encodeur
  ↓
ESP32
  ↓
PID
  ↓
correction PWM
```

Le Pi ne doit pas être dans cette boucle.

------------------------------------------------------------------------

# 13. Navigation

La navigation appartient au Raspberry Pi.

Le Pi peut utiliser :

-   caméra ;
-   capteurs remontés par l'ESP32 ;
-   odométrie ;
-   IMU ;
-   algorithmes de navigation ;
-   OpenCV ;
-   éventuellement ROS 2 plus tard.

Le Pi décide :

``` text
où aller
```

L'ESP32 exécute :

``` text
comment faire tourner les moteurs pour y aller
```

------------------------------------------------------------------------

# 14. Vision

La caméra appartient au Raspberry Pi.

Chaîne cible :

``` text
Caméra
  ↓
Raspberry Pi
  ↓
Vision
  ├── personne
  ├── visage
  ├── animal
  ├── objet
  └── obstacle
  ↓
Rover Core
  ↓
Décision
```

Exemple :

``` text
PERSONNE DÉTECTÉE
        ↓
position X = 72 %
        ↓
Rover Core
        ↓
tourner tête vers X
        ↓
ESP32
```

------------------------------------------------------------------------

# 15. Personnalité et machine à émotions

La personnalité appartient au Raspberry Pi.

Le Pi décide de l'état émotionnel de Rover :

``` text
HAPPY
CURIOUS
SLEEPY
CONFUSED
ALERT
SAD
EXCITED
IDLE
```

Il transmet ensuite une intention :

``` text
FACE = CURIOUS
```

ou :

``` text
ANIMATION = CURIOUS_LOOK
```

L'ESP32 réalise l'expression physique.

Cela permet de séparer :

**émotion logique**

de

**rendu physique de l'émotion**.

------------------------------------------------------------------------

# 16. Écran et héritage de Lumi

Le système d'animation de Lumi doit être considéré comme une base
réutilisable.

Objectif :

``` text
LUMI DISPLAY ENGINE
        ↓
R-BOT DISPLAY ENGINE
        ↓
Lumi / Rover / futurs robots
```

Le moteur graphique doit idéalement être indépendant du comportement IA.

L'IA dit :

``` text
HAPPY
```

Le moteur graphique décide :

``` text
comment HAPPY est affiché
```

------------------------------------------------------------------------

# 17. Audio

Le traitement audio haut niveau appartient au Raspberry Pi.

Architecture conceptuelle :

``` text
Micro
 ↓
Audio processing
 ↓
Speech-to-Text
 ↓
IA
 ↓
réponse
 ↓
Text-to-Speech
 ↓
Audio
```

Le matériel audio temps réel peut être connecté à l'ESP32 ou au Pi selon
la conception finale.

La règle reste :

**la décision conversationnelle appartient au Pi.**

------------------------------------------------------------------------

# 18. Home Assistant

Home Assistant est une couche externe.

Architecture :

``` text
Home Assistant
      ↕
 Raspberry Pi
      ↕
 Rover Core
      ↕
    ESP32
```

Rover ne doit pas transformer l'ESP32 en contrôleur domotique.

Le Pi gère :

-   MQTT ;
-   API ;
-   événements ;
-   scènes ;
-   notifications ;
-   états de la maison.

------------------------------------------------------------------------

# 19. Gestion de l'énergie

Le système doit distinguer :

``` text
BATTERY STATE
```

et

``` text
POWER SAFETY
```

L'ESP32 peut surveiller les informations électriques disponibles et
déclencher :

``` text
LOW_BATTERY
CRITICAL_BATTERY
```

Le Pi peut alors prendre des décisions :

``` text
"Je dois retourner à ma base."
"Je passe en mode économie."
"Je préviens l'utilisateur."
```

Mais les protections électriques matérielles doivent rester
indépendantes du logiciel.

------------------------------------------------------------------------

# 20. États globaux de Rover

Rover devrait posséder une machine à états globale.

Exemple :

``` text
BOOT
 ↓
INITIALIZING
 ↓
IDLE
 ├── INTERACTING
 ├── MOVING
 ├── EXPLORING
 ├── PATROLLING
 ├── FOLLOWING
 ├── CHARGING
 ├── SLEEPING
 └── ERROR
```

Le Raspberry Pi possède la logique de haut niveau.

L'ESP32 possède sa propre machine d'état matérielle :

``` text
BOOT
 ↓
READY
 ↓
ACTIVE
 ↓
SAFE
 ↓
ERROR
```

Les deux états ne doivent pas être confondus.

------------------------------------------------------------------------

# 21. Gestion des erreurs

Chaque couche doit pouvoir échouer sans rendre tout le robot incohérent.

Exemple :

### Caméra HS

``` text
Vision ❌
 ↓
Rover continue à fonctionner
 ↓
mode dégradé
```

### BME688 HS

``` text
BME688 ❌
 ↓
ESP32 signale ERROR
 ↓
Rover continue sans données météo
```

### Raspberry Pi HS

``` text
Pi ❌
 ↓
ESP32 détecte timeout
 ↓
STOP
 ↓
SAFE
```

### Capteur ToF HS

``` text
ToF ❌
 ↓
navigation informée
 ↓
mode de sécurité
```

------------------------------------------------------------------------

# 22. Ce que l'ESP32 ne doit jamais faire

Sauf décision architecturale explicite ultérieure, l'ESP32 ne doit pas :

-   appeler une API Internet ;
-   exécuter Gemini ou une IA conversationnelle ;
-   gérer Home Assistant ;
-   faire du SLAM ;
-   gérer la logique principale de navigation ;
-   analyser des images complexes ;
-   gérer la personnalité globale ;
-   dépendre d'Internet pour assurer la sécurité ;
-   dépendre du Raspberry Pi pour arrêter les moteurs.

------------------------------------------------------------------------

# 23. Ce que le Raspberry Pi ne doit jamais faire

Le Raspberry Pi ne doit pas :

-   générer directement les PWM moteurs ;
-   gérer directement les PID moteurs ;
-   piloter directement les servos ;
-   dessiner directement chaque frame du ST7789 ;
-   dépendre d'une boucle Linux temps réel pour sécuriser les moteurs ;
-   envoyer des commandes GPIO arbitraires à l'ESP32.

Le Pi communique par **intentions et commandes abstraites**.

------------------------------------------------------------------------

# 24. Principe d'abstraction

Mauvais :

``` text
Pi → GPIO 12 = HIGH
```

Bon :

``` text
Pi → MOVE velocity=0.20 rotation=0
```

Mauvais :

``` text
Pi → Servo PWM 1540 µs
```

Bon :

``` text
Pi → HEAD pitch=10 yaw=-20
```

Mauvais :

``` text
Pi → dessine ce pixel
```

Bon :

``` text
Pi → FACE=HAPPY
```

C'est ce principe qui permettra à Rover d'évoluer sans transformer le
code en spaghetti cyberpunk.

------------------------------------------------------------------------

# 25. Roadmap officielle

## PHASE 0 --- Architecture

Objectif : figer les règles.

-   [ ] Valider cette architecture.
-   [ ] Créer le Rover Protocol.
-   [ ] Définir les messages.
-   [ ] Définir les états.
-   [ ] Définir les erreurs.
-   [ ] Définir le heartbeat.
-   [ ] Définir l'arborescence logicielle.

**Aucune IA complexe à cette étape.**

------------------------------------------------------------------------

# PHASE 1 --- ESP32 minimum viable

Objectif : faire vivre la plateforme.

-   [x] Initialiser ESP32.
-   [x] Initialiser Rover Protocol.
-   [x] Communication UART.
-   [x] Heartbeat.
-   [x] Watchdog.
-   [x] État READY/SAFE.
-   [x] Diagnostic série.

Résultat attendu :

``` text
Pi ↔ ESP32
```

fonctionne de manière fiable.

------------------------------------------------------------------------

# PHASE 2 --- Motorisation

Objectif : obtenir une plateforme roulante fiable.

-   [x] DRV8833 (remplace le TB6612FNG initialement prévu ici --- voir
      PROGRESS.md 2026-08-25, moteurs N20 6V avec encodeur confirmés).
-   [x] Moteur gauche.
-   [x] Moteur droit.
-   [x] Encodeurs.
-   [x] Lecture vitesse.
-   [x] PID gauche.
-   [x] PID droit.
-   [x] Commande MOVE.
-   [x] Arrêt immédiat.
-   [x] Timeout sécurité.

Complet au niveau code et validé en simulation Wokwi (voir PROGRESS.md) ;
pas encore testé sur moteurs/encodeurs physiques réels, ni calibré
(gains PID et géométrie roue dans `motion_config.h` sont des valeurs de
départ arbitraires).

Résultat :

``` text
Pi → MOVE
ESP32 → moteurs
ESP32 → STATE
```

------------------------------------------------------------------------

# PHASE 3 --- Tête et écran

Objectif : donner une personnalité physique à Rover.

-   [x] Servo Pitch.
-   [x] Servo Yaw.
-   [x] Limites mécaniques (souples, placeholders --- voir
      `head_config.h`).
-   [x] Interpolation.
-   [x] Portage du moteur d'yeux de Lumi --- **réécriture**, pas un
      copier-coller (Lumi n'a pas de classe réutilisable, voir
      PROGRESS.md), technique RGB-split reprise fidèlement.
-   [x] Expressions --- les 8 émotions de la section 15, mais en
      premier jet **paramétrique** (un seul modèle d'œil de base
      modulé par émotion : ouverture/regard/clignement/glitch), pas
      encore d'art dédié par émotion.
-   [x] Animations --- mécanisme générique (`ANIMATION name=...`) avec
      **une seule** animation concrète (`GLITCH`) comme preuve de
      fonctionnement, pas une bibliothèque complète.
-   [x] Glitch engine --- l'effet RGB-split (utilisé par `ALERT` et par
      l'animation `GLITCH`).
-   [x] Commandes FACE.
-   [x] Commandes ANIMATION.

Complet au niveau code et compile sur les deux cibles ; validé en
simulation Wokwi **au niveau protocole** (dispatch sans crash) mais
**pas visuellement** --- voir PROGRESS.md pour la limite technique
rencontrée (capture d'écran headless impossible avec le chip ST7789
simulé utilisé). Pas testé sur servos/écran physiques réels.

Résultat :

Rover peut :

``` text
regarder
sourire
être curieux
réagir
```

------------------------------------------------------------------------

# PHASE 4 --- Capteurs

Objectif : donner des sens au robot.

-   [ ] MPU6050.
-   [ ] VL53L0X gauche.
-   [ ] VL53L0X droite.
-   [ ] BME688.
-   [ ] Lecture périodique.
-   [ ] Gestion des erreurs.
-   [ ] Transmission STATE.
-   [ ] Transmission EVENT.

Résultat :

Le Pi possède une représentation de l'état physique de Rover.

------------------------------------------------------------------------

# PHASE 5 --- Raspberry Pi

Objectif : créer le cerveau.

-   [ ] OS.
-   [ ] Services Rover.
-   [x] Rover Core (`pi/rover_core/`) --- minimal : gère la connexion
      ESP32 et le heartbeat/resume, pas encore de machine à états de
      haut niveau (IDLE/INTERACTING/MOVING/... §20).
-   [x] Rover ESP32 Interface (`pi/rover_esp32/`) --- pont Rover
      Protocol complet (encode/decode conforme à
      `RoverProtocol.cpp`, testé trame par trame), voir PROGRESS.md.
-   [ ] Configuration.
-   [ ] Logs.
-   [ ] Gestion des événements.
-   [ ] Machine à états.

Résultat :

Le Pi devient l'orchestrateur central. **Tranche minimale seulement**
--- suffisante pour piloter Rover (Phase 6 minimale), pas
l'orchestrateur complet décrit ici.

------------------------------------------------------------------------

# PHASE 6 --- Caméra et contrôle distant

Objectif : voir et piloter Rover.

-   [ ] Caméra.
-   [ ] Flux vidéo local.
-   [x] Interface de contrôle (`pi/rover_control/`, page web
      autonome, pas de build).
-   [x] Commandes MOVE (validé de bout en bout en simulation, voir
      PROGRESS.md : navigateur → WebSocket → Rover Protocol → ESP32).
-   [ ] Commandes HEAD (pas de cible : Phase 3 --- servos tête --- pas
      encore écrite côté ESP32).
-   [ ] Retour d'état (les trames `STATE`/`EVENT`/`ERROR` sont bien
      reçues et journalisées côté Pi, mais pas encore affichées dans
      l'interface).
-   [x] Contrôle smartphone (joystick tactile, Pointer Events) --- code
      écrit, backend validé ; reste à valider au toucher sur un vrai
      téléphone.
-   [x] Support manette / gamepad (Gamepad API) --- idem, code écrit,
      reste à valider avec une manette physique branchée.
-   [ ] Accès distant sécurisé (VPN) pour un pilotage hors réseau local.
-   [ ] Authentification --- **aucune pour l'instant** : le serveur de
      contrôle n'a ni auth ni chiffrement, à ne pas exposer hors d'un
      réseau local de confiance en l'état.

Résultat :

Rover peut être piloté à distance (smartphone ou manette) avec retour
vidéo, y compris hors du réseau local via VPN. **Partiellement atteint
en réseau local seulement** : pilotage MOVE fonctionnel, ni vidéo ni
accès distant/VPN/auth (voir PROGRESS.md pour l'état détaillé).

Cas d'usage prioritaire : surveillance de la maison à distance
(déplacement + vidéo en temps réel) en complément du pilotage.

Cette couche reste entièrement portée par le Raspberry Pi (réseau,
VPN, authentification, flux vidéo) ; l'ESP32 continue de ne recevoir
que des commandes abstraites (`MOVE`, `HEAD`, ...) via le Rover
Protocol, quelle que soit l'origine de la commande (app locale,
smartphone distant ou manette).

------------------------------------------------------------------------

# PHASE 7 --- Audio et IA

Objectif : permettre l'interaction naturelle.

-   [ ] Micro.
-   [ ] Audio pipeline.
-   [ ] Speech-to-Text.
-   [ ] IA conversationnelle.
-   [ ] Text-to-Speech.
-   [ ] Personality Engine.
-   [ ] Machine à émotions.
-   [ ] Connexion aux commandes Rover.

Résultat :

Rover comprend :

``` text
"Viens me voir."
"Regarde derrière toi."
"Quelle est la température ?"
"Que se passe-t-il ?"
```

et transforme ces intentions en actions.

------------------------------------------------------------------------

# PHASE 8 --- Vision

Objectif : permettre à Rover de comprendre son environnement.

-   [ ] Détection de personnes.
-   [ ] Détection d'objets.
-   [ ] Détection d'animaux.
-   [ ] Suivi de cible.
-   [ ] Reconnaissance selon les capacités matérielles.
-   [ ] Fusion vision + capteurs.

Résultat :

``` text
VISION
  ↓
PERCEPTION
  ↓
DECISION
  ↓
ACTION
```

------------------------------------------------------------------------

# PHASE 9 --- Navigation autonome

Objectif : permettre à Rover d'explorer.

Commencer simple :

-   [ ] Évitement d'obstacles.
-   [ ] Rotation.
-   [ ] Déplacement vers une cible.
-   [ ] Odométrie.
-   [ ] Correction de trajectoire.

Puis :

-   [ ] Localisation.
-   [ ] Cartographie.
-   [ ] Navigation autonome.
-   [ ] Patrouilles.

ROS 2 peut être introduit à ce stade si sa complexité apporte une vraie
valeur.

**ROS 2 n'est pas une obligation pour Rover V1.**

------------------------------------------------------------------------

# PHASE 10 --- Home Assistant

Objectif : intégrer Rover à la maison.

-   [ ] MQTT.
-   [ ] États Rover.
-   [ ] Capteurs.
-   [ ] Événements.
-   [ ] Commandes.
-   [ ] Scènes.
-   [ ] Notifications.
-   [ ] Interaction avec les appareils domestiques.

------------------------------------------------------------------------

# PHASE 11 --- Intelligence comportementale

Objectif : passer de "robot avec fonctions" à "compagnon".

Rover doit pouvoir combiner :

``` text
PERCEPTION
     ↓
CONTEXTE
     ↓
PERSONNALITÉ
     ↓
DÉCISION
     ↓
ACTION
     ↓
RÉACTION
```

Exemple :

``` text
Batterie faible
+
Utilisateur absent
+
Rover loin de sa zone de repos
        ↓
Décision
        ↓
Retour
        ↓
Animation sleepy
        ↓
Recharge
```

------------------------------------------------------------------------

# 26. Priorité de développement

L'ordre recommandé est :

``` text
1. Sécurité
2. Communication
3. Motorisation
4. Tête
5. Écran
6. Capteurs
7. Raspberry Pi Core
8. Caméra
9. Audio
10. IA
11. Vision
12. Navigation
13. Home Assistant
14. Comportements avancés
```

Ne pas commencer par l'IA.

Un robot qui dit :

> "Bonjour, je suis Rover !"

mais qui fonce dans un mur à 100 % de batterie est beaucoup moins
impressionnant.

------------------------------------------------------------------------

# 27. Règles pour les IA qui programment Rover

Toute IA intervenant sur le projet doit :

1.  Lire ce document avant de modifier l'architecture.
2.  Identifier si la fonctionnalité appartient au Pi ou à l'ESP32.
3.  Ne pas déplacer une responsabilité d'une plateforme vers l'autre
    sans justification.
4.  Respecter Rover Protocol.
5.  Ne pas créer une nouvelle méthode de communication parallèle sans
    nécessité.
6.  Préserver le fonctionnement du mode SAFE.
7.  Ne jamais supprimer le heartbeat/watchdog pour simplifier un test.
8.  Préférer les commandes abstraites aux commandes matérielles.
9.  Ne pas introduire ROS 2 simplement parce qu'il existe.
10. Ne pas ajouter une dépendance lourde lorsqu'une solution simple
    suffit.
11. Documenter toute modification architecturale.
12. Tester chaque couche indépendamment.
13. Ne pas casser la compatibilité du protocole sans versionnement.
14. Préserver la possibilité de remplacer le Raspberry Pi par une
    plateforme plus puissante.
15. Préserver la possibilité de faire évoluer l'ESP32 sans réécrire le
    cerveau de Rover.

------------------------------------------------------------------------

# 28. Règle de modification architecturale

Toute modification importante doit répondre à ces questions :

### 1. Qui possède cette fonctionnalité ?

``` text
ESP32
ou
Raspberry Pi
```

### 2. Pourquoi ?

### 3. Quel message est nécessaire ?

### 4. Que se passe-t-il si la communication est interrompue ?

### 5. Quel est le comportement SAFE ?

### 6. Comment tester la fonctionnalité sans le reste du robot ?

### 7. Est-ce compatible avec l'architecture actuelle ?

------------------------------------------------------------------------

# 29. Versionnement du Rover Protocol

Le protocole doit être versionné.

Exemple :

``` text
ROVER_PROTOCOL_V1
```

Une future évolution :

``` text
ROVER_PROTOCOL_V2
```

Les changements incompatibles doivent provoquer une nouvelle version.

L'objectif est d'éviter :

``` text
ESP32 firmware nouveau
        +
Pi software ancien
        =
💥
```

------------------------------------------------------------------------

# 30. Tests

Chaque couche doit pouvoir être testée séparément.

## Test ESP32

Sans Raspberry Pi :

``` text
ESP32
 ↓
moteurs
 ↓
servos
 ↓
écran
 ↓
capteurs
```

## Test protocole

``` text
PC
 ↕
UART
 ↕
ESP32
```

## Test Pi

``` text
Raspberry Pi
 ↓
simulation ESP32
```

## Test complet

``` text
Pi
 ↕
ESP32
 ↕
Rover physique
```

Le simulateur de l'ESP32 sera particulièrement intéressant pour
développer les fonctions IA/navigation sans avoir Rover physiquement
devant soi.

------------------------------------------------------------------------

# 31. Mode simulation

À terme, Rover devrait pouvoir fonctionner en mode :

``` text
REAL
SIMULATION
```

En simulation :

``` text
Rover Core
     ↓
Fake ESP32
     ↓
états simulés
```

Cela permet de tester :

-   IA ;
-   navigation ;
-   personnalité ;
-   Home Assistant ;
-   commandes ;
-   scénarios.

sans déplacer physiquement le robot.

------------------------------------------------------------------------

# 32. Philosophie finale

Rover doit être conçu comme une plateforme robotique modulaire.

Le robot physique est :

``` text
ESP32 + moteurs + capteurs + écran
```

Le cerveau est :

``` text
Raspberry Pi + logiciels
```

Le lien entre les deux est :

``` text
Rover Protocol
```

La personnalité est :

``` text
Rover Core + AI + Display Engine
```

La sécurité est :

``` text
ESP32
```

La vision et l'intelligence sont :

``` text
Raspberry Pi
```

------------------------------------------------------------------------

# 33. Architecture finale résumée

``` text
                         INTERNET
                            │
                     IA / API / HA
                            │
                            ▼
                ┌─────────────────────┐
                │    RASPBERRY PI     │
                │                     │
                │    ROVER CORE       │
                │         │           │
                │  ┌──────┼───────┐   │
                │  │      │       │   │
                │ AI    Vision   Nav  │
                │  │      │       │   │
                │ Audio   Caméra   │   │
                │  │              │   │
                │ Home Assistant  │   │
                └─────────┬───────────┘
                          │
                    ROVER PROTOCOL
                       UART / USB
                          │
                          ▼
                ┌─────────────────────┐
                │      ESP32       │
                │                     │
                │ Communication       │
                │ Safety / Watchdog   │
                │        │            │
                │ ┌──────┼─────────┐  │
                │ │      │         │  │
                │Motor   Head    Display│
                │ │      │         │  │
                │PID   Pitch/Yaw  Eyes │
                │ │                │   │
                │Encoders       Glitch │
                │                     │
                │ Sensors             │
                │ IMU / ToF / BME688  │
                └─────────────────────┘
```

------------------------------------------------------------------------

# 34. Phrase directrice du projet

> **Rover est un robot distribué : le Raspberry Pi pense, l'ESP32
> ressent et agit.**

Ou, dans une version plus R-Bot :

> **The Pi decides. The ESP32 makes it real.**

Cette séparation doit rester le principe fondamental de Rover tant
qu'une raison technique majeure ne justifie pas de la remettre en cause.

------------------------------------------------------------------------

## Statut du document

**Architecture cible : V1.0**

Ce document est une base de référence et doit évoluer uniquement lorsque
l'architecture du robot évolue réellement.

Toute modification majeure doit être documentée et versionnée.
