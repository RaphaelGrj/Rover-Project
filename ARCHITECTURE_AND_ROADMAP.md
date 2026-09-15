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
    vision (ESP32-CAM)        sécurité
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

> ⚠ **« De bord » n'est plus littéral depuis le 2026-09-15** : le Pi
> n'est plus embarqué sur le châssis, c'est une machine du réseau local
> (§6.2). Ses responsabilités ci-dessous sont **inchangées** --- seul son
> emplacement physique et le transport du Rover Protocol changent.
> Corollaire : « le Pi » peut désormais être n'importe quelle machine du
> LAN, ce qui ouvre la Phase 8 (Vision) et un LLM local, hors de portée
> d'un 3B+.

Le Raspberry Pi est l'ordinateur de bord.

Responsabilités :

-   IA ;
-   vision (réception et analyse du flux vidéo --- caméra déportée sur
    ESP32-CAM, voir §4.3) ;
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

## 4.3 ESP32-CAM (caméra déportée)

Décision (2026-09-06) : la caméra n'est **pas** rattachée au Raspberry
Pi ni à l'ESP32 principal, mais portée par un **module ESP32-CAM**
indépendant, avec sa propre caméra intégrée.

Raison : contrainte CAO --- la tête de Rover n'a pas la place pour un
module caméra Raspberry Pi (même déporté par nappe CSI). Un module
ESP32-CAM autonome, alimenté et connecté en WiFi seul (pas de nappe à
faire tenir dans la tête), contourne le problème d'encombrement sans
changer de plateforme matérielle pour le reste du robot.

Rôle --- volontairement minimal, aucune logique de vision dessus :

``` text
L'ESP32-CAM filme.
Le Raspberry Pi analyse.
```

Conséquences architecturales :

-   L'ESP32-CAM est **indépendant de l'ESP32 principal** --- aucun lien
    avec le Rover Protocol (UART/USB) ni le reste de la table §5. Sa
    seule liaison est le WiFi, directement vers le Raspberry Pi.
-   Le Raspberry Pi **récupère le flux vidéo sur le réseau** plutôt que
    de dépendre d'une caméra locale : `pi/rover_control/camera.py` a été
    réécrit (2026-09-06) --- il ne dépend plus de `picamera2`, il relaie
    (reverse proxy `aiohttp`) le flux MJPEG exposé par le firmware
    ESP32-CAM (endpoint HTTP `/stream`, port 81) à travers le même
    endpoint `/video` authentifié déjà en place côté Pi, plutôt que
    d'exposer l'ESP32-CAM directement et sans authentification sur le
    réseau local. Testé avec un faux serveur MJPEG (pas encore
    l'ESP32-CAM réel, voir PROGRESS.md).
-   Le firmware de l'ESP32-CAM (capture + serveur MJPEG) est un projet
    séparé du firmware `esp32/` principal de ce dépôt --- pas de
    portabilité WROOM/S3 à respecter dessus, c'est un module caméra
    dédié. Écrit et compile (2026-09-06, `esp32-cam/`, board AI-Thinker
    ESP32-CAM) --- **pas encore flashé/testé sur le module physique**
    (câblage vers un adaptateur FTDI/USB-TTL prévu pour une prochaine
    session, voir `esp32-cam/WIRING.md`).

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
  Caméra (capture)    ESP32-CAM      filme (flux MJPEG WiFi, §4.3)
  Vision (analyse)    Raspberry Pi   interprétation du flux vidéo
  IA                  Raspberry Pi   raisonnement
  Réseau              Raspberry Pi   Wi-Fi / services
  Home Assistant      Raspberry Pi   domotique
  Navigation          Raspberry Pi   décisions de déplacement
  Audio haut niveau   Raspberry Pi   STT / TTS / logique

Cette table doit rester la référence avant l'ajout d'un nouveau
composant.

------------------------------------------------------------------------

# 6. Communication Raspberry Pi ↔ ESP32

> ⚠ **Mise à jour (2026-09-15)** : le Raspberry Pi n'est plus embarqué
> sur le robot (voir §6.2). Le transport physique décrit en §6.1 est
> **remplacé par une socket TCP sur le WiFi** ; le raisonnement de §6.1
> (pourquoi un flux d'octets orienté message plutôt qu'I2C) reste
> valable et explique pourquoi la bascule coûte si peu.

## 6.1 Technologie retenue (transport historique : UART)

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

## 6.2 Raspberry Pi déporté --- décision (2026-09-15)

**Le Raspberry Pi quitte le châssis.** Il devient une machine du réseau
local, et la liaison Rover Protocol passe du câble USB à une **socket
TCP sur le WiFi**.

Le robot embarque désormais : ESP32 WROOM + ESP32-CAM + batterie.
Rien d'autre.

### 1. Qui possède cette fonctionnalité ?

Inchangé --- et c'est le point clé de cette décision. La règle d'or
(§3) ne bouge pas d'un pouce : le Pi décide **quoi**, l'ESP32 décide
**comment**. Seul le *transport* entre les deux change. Aucune
responsabilité ne migre d'un côté à l'autre.

### 2. Pourquoi ?

Trois raisons, par ordre d'importance réelle :

-   **Encombrement et batterie.** Le Pi 3B+ pèse ~7,5 W sur un budget
    de ~32 W en pic (§19, `BOM.md`). Mais ce pic correspond aux moteurs
    à fond : **au repos ou en déplacement lent, le Pi est de loin le
    premier consommateur**. Le gain d'autonomie en usage réel dépasse
    donc largement les 23 % du pic. On supprime au passage le
    convertisseur buck 5V/3A dédié au Pi, son volume, son câblage --- et
    le risque de brownout/corruption de carte SD qui avait justement
    imposé de séparer les rails (§19).
-   **Le Pi n'a plus besoin d'être un Pi.** Une fois sur le réseau, le
    cerveau peut être n'importe quelle machine du LAN. La Phase 8
    (Vision) et un LLM local, irréalistes sur un 3B+, redeviennent
    envisageables. À terme c'est probablement le gain principal, devant
    la batterie.
-   **Ça débloque les broches du micro.** Voir question 7 ci-dessous ---
    résultat inattendu, mais décisif.

### 3. Quel message est nécessaire ?

**Aucun nouveau message.** Le Rover Protocol V1 est inchangé : mêmes
trames, même checksum, même séquence de démarrage. C'est un changement
de couche physique, pas de protocole (`ROVER_PROTOCOL.md` §2).

La bascule est peu coûteuse parce que les deux extrémités étaient déjà
abstraites --- sans que ç'ait été prévu pour ça :

-   **Côté Pi** : `pi/rover_esp32/link.py` utilise
    `serial.serial_for_url()`, qui accepte `socket://host:port` aussi
    bien que `/dev/ttyUSB0`. Le changement est une ligne de
    configuration, pas de code.
-   **Côté ESP32** : `RoverProtocol` prend un `Stream&`
    (`esp32/lib/communication/RoverProtocol.h`), pas un
    `HardwareSerial&`. Un `WiFiClient` *est* un `Stream`.

Le WiFi lui-même n'est pas une nouveauté sur l'ESP32 principal : il est
déjà provisionné et validé sur matériel réel pour l'OTA
(`esp32/lib/network/`, `esp32/lib/ota/`).

### 4. Que se passe-t-il si la communication est interrompue ?

**C'est la vraie difficulté de cette décision, et elle mérite d'être
nommée franchement.** Sur USB, une coupure est un événement rare et
franc. Sur WiFi, les micro-coupures sont *normales* : gigue, économie
d'énergie radio, itinérance entre points d'accès, congestion.

Le timeout heartbeat de **500 ms** (§9) était calibré pour un câble.
Appliqué tel quel au WiFi, il couperait les moteurs en permanence : le
robot avancerait par à-coups. Allonger bêtement le délai n'est pas une
réponse acceptable --- ça dégraderait la sécurité.

La réponse retenue est une **dégradation graduée** plutôt qu'un
basculement binaire, détaillée en §9.

### 5. Quel est le comportement SAFE ?

Inchangé dans son principe (moteurs à zéro, état `SAFE`), mais il faut
**remonter l'autonomie de l'ESP32 d'un cran**, puisque le lien devient
moins fiable :

-   Les réflexes d'obstacle sont **locaux** à l'ESP32.
    ⚠ **Correction du 2026-09-15 (revue de code)** : la première version
    de cette section affirmait que c'était déjà le cas « parce que les
    VL53L0X sont du côté ESP32 ». **C'était faux.** Les *capteurs*
    étaient côté ESP32, mais le *réflexe* ne l'était pas : le firmware
    se contentait d'émettre `EVENT name=obstacle_detected`, et le seul
    blocage réel vivait côté Pi (`rover_core/core.py`, `move()`).
    Autrement dit, l'argument de sécurité central de cette décision
    reposait sur quelque chose qui n'existait pas --- et avec le Pi
    déporté, le réflexe aurait traversé le WiFi pour empêcher le robot
    de percuter un obstacle.
    Corrigé le jour même : `DriveController::setForwardBlocked()`,
    appliqué dans `update()` (donc aussi à une commande *périmée*, pas
    seulement à un nouveau `MOVE`), câblé sur
    `SensorHub::obstacleDetected()` dans `main.cpp`. Bloque la marche
    **avant uniquement** --- reculer et tourner sur place restent
    possibles, exactement la même sémantique que le clamp du Pi, qui est
    **conservé** : deux couches indépendantes, aucune porteuse à elle
    seule. Inerte tant qu'aucun ToF n'est sain, donc un capteur absent
    ne peut pas immobiliser le robot.
    ⚠ **Jamais exécuté sur matériel** : les VL53L0X ne sont pas câblés
    (voir `PROGRESS.md`), donc ce chemin n'a pour l'instant été validé
    que par compilation et relecture.
-   Une commande `MOVE` doit être réinterprétée comme *« avance à
    vitesse X pendant **au plus** N ms »* au lieu de *« avance jusqu'à
    nouvel ordre »*. Ainsi, une perte de lien ne laisse jamais le robot
    en mouvement libre : l'ordre expire tout seul, même si l'ESP32
    n'avait pas encore détecté le timeout.
-   `WiFi.setSleep(false)` devient obligatoire : l'économie d'énergie
    radio de l'ESP32 ajoute une latence très irrégulière, incompatible
    avec un heartbeat serré.

### 5 bis. Qui a le droit de piloter ? (bloquant, ajouté le 2026-09-15)

Question absente de la première version de cette section, relevée à la
revue de code. Elle est **bloquante pour l'étape 1 du chantier**.

Tant que le lien était un **câble USB**, l'authentification était
physique : pour envoyer un `MOVE`, il fallait être dans la pièce, une
main sur le robot. Ce n'était écrit nulle part parce que personne
n'avait besoin de l'écrire.

En passant sur une socket TCP, **cette protection disparaît entièrement,
et rien ne la remplace** : le Rover Protocol n'a aucune notion
d'identité. Un `WiFiServer` qui accepte la première connexion venue
donne à **n'importe qui sur le réseau WiFi** le contrôle complet des
moteurs, des servos et du mode SAFE --- sans mot de passe, sans trace.

Le contraste est net avec le reste du projet, par ailleurs rigoureux
là-dessus : le serveur de contrôle exige un token
(`rover_control/auth.py`, comparaison à temps constant, aucun défaut
codé en dur) et l'OTA refuse de démarrer sans mot de passe (`RoverOTA.h`
: « no unauthenticated flashing by anyone on the LAN »). Le lien de
commande serait le **seul** canal ouvert --- et le plus dangereux des
trois, puisque c'est celui qui fait bouger le robot.

**Exigence retenue** : l'ESP32 ne doit pas quitter l'état `READY` sur
une connexion réseau tant qu'un secret partagé n'a pas été présenté.

-   Stockage du secret : NVS via `WifiCredentialsStore`, à côté du mot
    de passe OTA --- même mécanisme, même portail de provisioning, rien
    de nouveau à inventer et jamais commité dans le dépôt.
-   Forme : une première trame obligatoire du Pi après connexion ; toute
    autre trame reçue avant est rejetée (`ERROR code=unauthenticated`)
    et la connexion fermée.
-   Comparaison à temps constant, comme `auth.py` le fait déjà côté Pi.
-   Une seule connexion active à la fois : une seconde est refusée plutôt
    que de laisser deux pilotes se disputer les moteurs.

⚠ **Rien de cela n'est implémenté.** C'est la raison pour laquelle le
câble USB ne doit pas être débranché avant que ce point soit traité :
aujourd'hui, le lien filaire *est* l'authentification.

### 5 ter. À qui appartient la connexion WiFi ? (dette à solder)

Autre constat de la revue : aujourd'hui, la seule chose qui connecte
l'ESP32 au réseau est `RoverOTA::begin()` --- un module dont le contrat
explicite est d'être *optionnel*, « entirely inert unless a developer
deliberately configures credentials ».

Faire passer le Rover Protocol par le WiFi rendrait donc **le lien de
commande dépendant d'un module de maintenance facultatif**, ce qui
contredit §4.2 (« l'ESP32 doit rester fonctionnel ») et la hiérarchie de
§22. Deux symptômes concrets déjà présents :

-   `RoverOTA::begin()` ne tente la connexion **qu'une fois, au boot**.
    Aucune reconnexion explicite (on dépend du `setAutoReconnect` par
    défaut du core ESP32, jamais affirmé dans le code).
-   `SYSTEM action=wifi_setup` bascule la radio en mode AP --- **la
    commande arrive donc par le lien qu'elle s'apprête à couper**, et le
    Pi ne peut plus rien envoyer jusqu'à la fin du portail.
    *(Le cas le plus grave --- `stop()` laissait la radio en `WIFI_OFF`
    sans jamais la rallumer, verrouillage dur jusqu'au reboot --- a été
    corrigé le 2026-09-15 : retour en `WIFI_STA` + `reconnect()`.)*

**À faire avant l'étape 1** : extraire la gestion de la connexion WiFi
de `RoverOTA` vers un module autonome et toujours actif, dont l'OTA et
le Rover Protocol deviennent tous deux de simples clients.

### 6. Comment tester la fonctionnalité sans le reste du robot ?

Le point fort de cette bascule : la testabilité ne régresse pas.

-   `serial_for_url()` accepte déjà `socket://` --- les tests Pi
    existants tournent contre une socket locale sans aucun ESP32.
-   Le firmware reste testable hors matériel via `pio test -e native`
    (`RoverProtocol` ne dépend d'aucun périphérique).
-   La liaison WiFi doit être éprouvée **avant** toute modification
    mécanique : mesurer la gigue et le taux de perte en conditions
    réelles (robot en mouvement, à distance du point d'accès) décide du
    réglage final des seuils de §9.

### 7. Est-ce compatible avec l'architecture actuelle ?

Oui, et l'ESP32-CAM en est la preuve : il est déporté en WiFi depuis le
2026-09-06 (§4.3) et le Pi récupère déjà son flux par le réseau. Le
modèle « périphérique sur le LAN plutôt qu'au bout d'un câble » est
donc déjà en service et validé dans ce projet.

Deux conséquences matérielles méritent d'être notées :

**Le paradoxe des broches.** `esp32/WIRING.md` constate qu'il ne reste
*aucun* GPIO libre sur le WROOM : 24 broches utilisables sur 26 sont
attribuées, et les deux dernières (`GPIO1`/`GPIO3`) sont réservées à
l'UART0 --- c'est-à-dire **au câble USB vers le Pi**. Déporter le Pi est
donc précisément ce qui libère les broches dont le micro a besoin, alors
que le micro est justement le composant qui n'avait nulle part où aller
(`BOM.md`, « Micro/haut-parleur --- non décidé »). Voir §17.3.

**Le robot devient strictement dépendant du réseau.** Plus de WiFi =
plus de cerveau du tout. Les fournisseurs STT/TTS/IA retenus sont de
toute façon en ligne (§17.1, §17.2), donc la perte de *l'intelligence*
hors réseau est assumée.

⚠ **Mais la perte du *pilotage* ne l'est pas** --- révision du
2026-09-15, à la demande de l'utilisateur : Rover doit **rester
pilotable hors connexion et sans Pi**. Voir §6.3, qui rattrape
précisément ce point.

### Carte choisie

**ESP32 WROOM, décision confirmée le 2026-09-15.** Un ESP32-S3 est
disponible mais volontairement gardé de côté : l'objectif est de
terminer le projet sur le WROOM. Mesures réelles à l'appui (compilation
du firmware au 2026-09-15) :

``` text
RAM:   16.2 %  (53 144 / 327 680 octets)
Flash: 71.0 %  (930 353 / 1 310 720 octets)  -- déjà en partition OTA double
```

La ressource tendue est la **flash** (~380 Ko libres), pas la RAM
(~274 Ko libres au link, moins ~40-60 Ko pris par la pile WiFi une fois
connectée). Le S3 (PSRAM) resterait utile pour une étape ultérieure ---
détection de mot-clé embarquée, assistance vision locale --- mais aucune
de ces fonctions n'est nécessaire au périmètre « Pi déporté + voix ».

Côté CPU, aucune action requise : le WROOM est bi-cœur, WiFi/LwIP
tourne sur le cœur 0 et la boucle Arduino (PID, encodeurs, interruptions
d'encodeur) sur le cœur 1 par défaut. `esp32/src/main.cpp` n'épingle
aucune tâche manuellement --- c'est déjà la bonne configuration, à ne
pas « corriger » par inadvertance.

------------------------------------------------------------------------

## 6.3 Pilotage autonome --- sans Pi et sans réseau (2026-09-15)

**Exigence** : quoi qu'il arrive au Pi ou au réseau, Rover doit rester
pilotable par quelqu'un qui se tient à côté de lui.

C'est le plancher sous la décision §6.2. Déporter le Pi rendait le robot
dépendant de **deux** choses à la fois --- un réseau qui marche *et* un
Pi joignable. Une coupure WiFi, un Pi resté éteint, ou simplement
emporter Rover quelque part sans infrastructure : dans les trois cas le
robot devenait inerte. §6.3 supprime ce mode de défaillance.

**Solution** : l'ESP32 ouvre **son propre point d'accès WPA2** et sert
une petite page joystick (`esp32/lib/network/StandaloneControl.h`). Un
téléphone se connecte directement au robot. Aucun Pi, aucun réseau,
aucune box, aucun Internet.

### Est-ce que ça viole la §22 ?

La question se pose sérieusement, puisque §22 interdit à l'ESP32 la
logique métier. **Non**, et la distinction est nette :

-   §22 interdit à l'ESP32 de décider **QUOI faire** --- IA, navigation,
    Home Assistant, boucle de décision.
-   Ici, la décision vient d'un **pouce humain sur un écran**. L'ESP32
    fait exactement ce qu'il fait déjà d'une trame `MOVE` venue du Pi :
    du **COMMENT**. La page est une *source de commandes*
    supplémentaire, pas un nouveau décideur.

Rien dans ce module ne planifie, ne choisit ni ne mémorise quoi que ce
soit. C'est de la téléopération directe.

### Sécurité : réutiliser, jamais dupliquer

C'est le principe de conception central, et la raison pour laquelle les
commandes de la page passent par **les mêmes gestionnaires** que celles
du Pi plutôt que de toucher les moteurs directement. Ce module ne
contient **aucune logique de sécurité propre** :

-   le navigateur alimente le **même** `HeartbeatMonitor` que le Pi ---
    donc fermer l'onglet ou sortir de portée arrête les moteurs via le
    timeout existant, sans second chemin de code qui pourrait diverger ;
-   l'E-stop est vérifié indépendamment dans `loop()` et prime toujours ;
-   le réflexe d'obstacle local (§6.2 question 5) s'applique tel quel ;
-   entrer dans ce mode **ne réarme pas les moteurs** : le robot est en
    `SAFE` en y arrivant (c'est en général pourquoi il y arrive), et
    seule une pression explicite sur « Activer » appelle le resume.

Une seule divergence assumée : « Activer » accepte aussi l'état `READY`,
que le resume du Pi refuse. Sans ça, un robot allumé sans Pi resterait
en `READY` à vie et le mode serait inutilisable dans le cas même pour
lequel il existe.

### Contrôle d'accès : WPA2 obligatoire

L'AP est en **WPA2 et le mode refuse de démarrer sans mot de passe
stocké** --- posture calquée sur celle de `RoverOTA` (« no
unauthenticated flashing by anyone on the LAN »). Un AP ouvert serait
ici strictement pire que dans le cas OTA : il ne flashe pas un firmware,
il **déplace un robot physique**.

C'est le choix **inverse** de celui de `RoverWifiProvisioning`, dont le
portail est délibérément ouvert --- et la différence se justifie : ce
portail ne fait que *collecter* des identifiants pendant quelques
minutes et ne bouge rien, celui-ci fait rouler le robot.

Le mot de passe est distinct de celui de l'OTA (`solo_pass` en NVS,
collecté par le portail existant) : conduire le robot et remplacer son
firmware n'ont pas la même portée, un opérateur doit pouvoir confier
l'un sans l'autre.

### Déclenchement

-   **Automatique** après `ROVER_STANDALONE_FALLBACK_MS` (30 s) sans
    trame valide du Pi. Couvre les deux cas : « le lien est mort » *et*
    « il n'y a pas de Pi du tout » (avant la première trame, le compteur
    mesure depuis le boot --- voulu, c'est le cas d'usage principal).
-   **Manuel** via `SYSTEM action=standalone` / `standalone_off`.
-   **Jamais** pendant que le portail de provisioning est actif : les
    deux veulent la radio en mode AP et le port 80, et le provisioning
    est une action opérateur explicite qu'une bascule automatique ne doit
    pas interrompre.

⚠ Piège de conception évité à l'écriture : la page alimentant elle-même
le heartbeat, utiliser ce dernier pour décider « le Pi est revenu »
serait **circulaire** --- le robot conclurait sans cesse au retour du Pi
et couperait l'AP sous les pieds du pilote. `main.cpp` suit donc
`lastPiFrameMs` séparément, et ne referme l'AP que si personne n'est en
train de piloter (`isBeingUsed()`).

### Coût mesuré

+8 Ko de flash (71,0 % → 71,6 % sur WROOM), +400 octets de RAM ---
modeste parce que `WebServer` était déjà lié par le portail de
provisioning. La page est servie depuis la flash (`PROGMEM`), jamais
construite en RAM.

⚠ **Jamais exécuté sur matériel réel** : validé par compilation
(WROOM et S3) et relecture uniquement.

### Améliorations possibles

-   Afficher le SSID/mot de passe sur l'écran ST7789 au démarrage du
    mode : seul quelqu'un physiquement présent le verrait, ce qui
    supprimerait l'étape de configuration préalable sans ouvrir l'AP.
-   Entrée par appui long sur le bouton E-stop (pas de GPIO libre à
    trouver, et le bouton met déjà le robot en `SAFE` avant) --- à
    envisager une fois ce bouton réellement câblé.

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

### Dégradation graduée --- requis par le Pi déporté (2026-09-15)

Les 500 ms ci-dessus ont été calibrées pour un **câble USB**, où une
coupure est un événement rare et franc. Depuis que le Pi est déporté
(§6.2), le lien est une socket WiFi : les micro-coupures y sont
*normales*. Un seuil unique à 500 ms ferait avancer le robot par
à-coups, et l'allonger bêtement dégraderait la sécurité.

Le seuil unique est donc remplacé par **trois paliers** :

``` text
< 500 ms      NOMINAL    -- rien à signaler
500 ms        DEGRADED   -- vitesse plafonnée (~30 %), EVENT link_degraded
~1500 ms      TIMEOUT    -- STOP MOTORS -> SAFE (comportement historique)
```

Raison du palier intermédiaire : une gigue WiFi passagère ne doit pas
provoquer un arrêt complet, mais elle ne doit pas non plus passer
inaperçue. Ralentir laisse au lien le temps de se rétablir tout en
réduisant l'énergie cinétique --- donc les dégâts possibles --- pendant
la fenêtre d'incertitude.

Cette dégradation ne remplace pas les protections locales, elle s'y
ajoute :

-   les réflexes d'obstacle (VL53L0X) restent **entièrement locaux** à
    l'ESP32 et ne dépendent à aucun moment du lien ;
-   toute commande `MOVE` porte sa propre échéance (« vitesse X pendant
    au plus N ms »), de sorte qu'un ordre expire seul même si le timeout
    n'a pas encore été détecté ;
-   `WiFi.setSleep(false)` est obligatoire --- l'économie d'énergie
    radio introduit une latence trop irrégulière pour un heartbeat
    serré.

⚠ **Valeurs à confirmer par la mesure.** Les 500 ms / 1500 ms ci-dessus
sont un point de départ raisonné, pas un résultat : le réglage
définitif doit venir d'une mesure de gigue et de taux de perte en
conditions réelles (robot en mouvement, à distance du point d'accès).
Tant que cette mesure n'a pas été faite, ces chiffres sont des
hypothèses.

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
│   └── IA / raisonnement / personnalité --- fournisseur API cloud ou
│       LLM réseau local interchangeable (voir §17.1)
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

## 13.1 Visualisation AR (casque Meta Quest 3 / WebXR)

**Idée de l'utilisateur (2026-09-11)** : coupler Rover à un casque
AR/VR (Meta Quest 3 en main) pour visualiser la cartographie --- un
argument fort pour un projet destiné à être partagé en open source
(peu de robots domestiques grand public exposent leur carte en RA).

### Choix technique : WebXR, pas une app Quest native

Comme le reste de `pi/rover_control` (§27 règle 10, "ne pas ajouter une
dépendance lourde lorsqu'une solution simple suffit") : une page web
servie par le Pi, ouverte directement dans le navigateur du Quest 3
(Meta Quest Browser, WebXR intégré), **pas** une app Unity/Meta SDK
native. Raisons :

-   **Zéro installation, zéro build** --- ouvrir une URL suffit, cohérent
    avec `pi/rover_control/static/*.html` (page statique, pas de
    framework, pas d'étape de build).
-   **Zéro friction open source** --- n'importe qui avec un casque
    compatible WebXR (Quest 3, Quest Pro, et au-delà de Meta) peut
    utiliser ce que ce dépôt contient déjà, sans compte développeur
    Meta ni passage par le store.
-   **Réutilise l'infrastructure existante** --- même serveur
    (`rover_control`), même authentification par token
    (`rover_control/auth.py`), même flux `/ws` (STATE/EVENT/ERROR)
    déjà utilisé par la page de pilotage.
-   WebXR AR (mode `immersive-ar`) exige un contexte sécurisé (HTTPS) :
    déjà supporté par ce projet (`--tls-cert`/`--tls-key`,
    `rover_core/main.py`) --- rien à ajouter pour ça.

### Deux incréments, volontairement séparés

1. **HUD télémétrie en RA (buildable dès maintenant)** ---
   `pi/rover_control/static/ar-hud.html` : superpose le panneau de
   télémétrie déjà existant (distance/IMU/environnement/batterie/état
   comportemental, mêmes champs `STATE`/`EVENT`/`ERROR` que
   `index.html`) sur le passthrough caméra du casque, via la feature
   WebXR `dom-overlay` (un simple overlay HTML/CSS pendant une session
   `immersive-ar` --- pas de rendu 3D/WebGL nécessaire pour cette
   première étape). Ne dépend d'aucune donnée qui n'existe pas encore :
   c'est le même flux `/ws` que la page de pilotage, juste affiché
   flottant dans la pièce plutôt que sur un écran de téléphone. Dégrade
   proprement (§21) sur un appareil sans WebXR AR (desktop, la plupart
   des téléphones) : la page retombe sur un affichage 2D classique.
2. **Superposition de la carte (dépend de la Phase 9 : Cartographie/
   Localisation ci-dessous)** --- une fois qu'une vraie carte
   (occupancy grid ou nuage de points) et une pose (x, y, cap) existent
   côté Pi, un rendu 3D (WebGL/Three.js cette fois, `dom-overlay` ne
   suffit plus) ancre le plan de la maison dans la pièce réelle et
   affiche la position de Rover en direct. **Conçu, pas encore
   implémenté** --- il n'existe aujourd'hui aucune donnée de
   cartographie à afficher (Phase 9 n'a pas commencé). Contrat de
   données prévu, à respecter quand ce jour arrive : un instantané de
   la grille (peu changeant, récupéré une fois) + un flux de pose
   (fréquent, sur le même `/ws` ou un canal dédié) --- même séparation
   "gros objet rarement mis à jour vs. petit état fréquent" que MQTT
   (§18) applique déjà à l'état de Rover.

------------------------------------------------------------------------

# 14. Vision

La capture vidéo appartient à l'ESP32-CAM, module déporté indépendant
(voir §4.3) --- l'analyse (vision) reste du ressort du Raspberry Pi, qui
récupère le flux sur le réseau.

Chaîne cible :

``` text
ESP32-CAM (filme)
  ↓ flux MJPEG, WiFi
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

## 17.1 Fournisseur IA (module `rover-ai`)

**Statut (2026-09-11) : backend écrit et testé (`pi/rover_ai/`), panneau
web de configuration + test texte disponible (`/ai`, voir
`pi/README.md` "rover-ai"), et maintenant branché dans `RoverCore` via un
Personality Engine (`pi/rover_ai/personality.py`) : chaque échange passe
par une persona + un historique de conversation (l'`AIContext` que les
fournisseurs savaient déjà consommer, jamais alimenté jusqu'ici) et
déclenche une émotion (`FACE emotion=...`) sur l'ESP32 --- `curious`
pendant l'appel, `happy` sur une réponse, `confused` sur une erreur.
Toujours pas branché dans un pipeline audio (micro/STT/TTS), voir Phase
7.** Conçu et documenté ci-dessous depuis le 2026-09-05.

Interface commune (`AIProvider.ask`) implémentée, huit fournisseurs
cloud et un fournisseur réseau local générique :
- `AnthropicProvider`, `OpenAIProvider`, `GeminiProvider` --- chacun son
  propre format de requête/réponse.
- `QwenCloudProvider` (Qwen officiel via Alibaba Cloud DashScope,
  endpoint compatible OpenAI), `OpenRouterProvider`, `TogetherProvider`,
  `FireworksProvider`, `DeepInfraProvider` --- cinq agrégateurs qui
  parlent tous l'API compatible OpenAI, donc juste une URL/clé/modèle
  par défaut, aucune logique de requête séparée. Demandés par
  l'utilisateur (2026-09-09) comme alternative cloud à l'auto-hébergement
  pour Qwen et des modèles communautaires non censurés (Dolphin,
  variantes "abliterated", ...) --- catalogue de modèles à vérifier chez
  chaque fournisseur avant de figer un `model` en prod, ça bouge, et
  même un modèle "non censuré" hébergé chez un agrégateur peut rester
  soumis à la propre modération du service (garantie totale seulement en
  auto-hébergement local).
- `LocalAIProvider` --- réseau local générique (API compatible OpenAI,
  Ollama en référence). Confirmé (2026-09-09) : n'importe quel serveur
  qui implémente `/chat/completions` et n'importe quel nom de modèle
  qu'il reconnaît passent sans toucher au code, juste une valeur de
  config différente (URL + nom de modèle) --- Qwen 2.5 ou un modèle non
  censuré ne sont qu'un nom de modèle différent.

Stockage des identifiants (`pi/ai_credentials.json`, git-ignoré,
permissions `0600`) fait selon le même principe que
`ROVER_CONTROL_TOKEN`/`WifiCredentialsStore.h`. Testé en isolation
(`pi/tests/test_rover_ai.py`, requêtes/réponses de chaque fournisseur
simulées, jamais de vrai réseau) --- **jamais appelé depuis
`RoverCore`, aucune clé API réelle testée contre un vrai service**, ce
sera fait avec le reste de la Phase 7 (micro/STT/TTS).

**Panneau web (2026-09-09)** : ce réglage est désormais accessible
depuis l'interface web existante, comme voulu par cette section ---
`rover_control/ai_panel.py` (logique, testée
`pi/tests/test_ai_panel.py`) + `rover_control/static/ai.html` (page),
routes `/ai` (page), `/ai/config` (GET état actuel/clé redactée, POST
mise à jour) et `/ai/ask` (POST, teste une conversation texte), toutes
protégées par le même token que le reste de `rover_control`. Un champ
clé API laissé vide au ré-enregistrement garde la valeur déjà stockée
(même convention que le mot de passe OTA du portail WiFi ESP32). Testé
manuellement de bout en bout (serveur en mémoire, pas encore sur le Pi
réel) : page/config/ask répondent, token invalide rejeté (`403`),
fournisseur injoignable rapporté proprement (`503`) plutôt qu'un crash.
**Bug réel trouvé et corrigé pendant l'écriture des tests** : passer de
"cloud" à "local" laissait la clé API cloud trainer dans le fichier
stocké (la logique "champ vide = garder la clé actuelle" s'appliquait
même quand le nouveau fournisseur n'utilise pas de clé du tout) ---
corrigé, la clé n'est conservée que si le fournisseur reste "cloud".

Le "IA" de la chaîne ci-dessus (§17) n'est pas un fournisseur figé : Rover
doit pouvoir utiliser, au choix de l'utilisateur et **modifiable à tout
moment sans reflash ni redéploiement** :

``` text
                    rover-ai
                       │
           ┌───────────┴───────────┐
           │                       │
   Fournisseur API cloud    Fournisseur réseau local
   (Gemini, ChatGPT,        (ex. un second Raspberry
    Claude, ...)             Pi sur le même WiFi, Qwen
                              2.5 via Ollama ou équivalent)
```

### Pourquoi ce choix appartient à l'utilisateur, pas au code

- Une API cloud est simple et puissante, mais dépend d'Internet, d'une
  clé payante, et envoie les conversations à un tiers.
- Un LLM local (deuxième Raspberry Pi/PC dédié sur le réseau local,
  servant par ex. Qwen 2.5 via Ollama) ne dépend d'aucun service
  externe et garde tout en local, au prix de réponses plus lentes/moins
  capables selon le matériel.

Rover ne doit imposer ni l'un ni l'autre : les deux sont interchangeables
derrière une interface commune, et **le choix se fait à l'usage**, pas au
build.

### Interface commune

`rover-ai` définit une interface neutre (indépendante du fournisseur actif) :

``` text
rover-ai.ask(message, context) -> réponse
```

Deux implémentations :

- **`CloudAIProvider`** --- appelle l'API du fournisseur choisi (Gemini,
  ChatGPT/OpenAI, Claude, ...) avec la clé fournie par l'utilisateur.
- **`LocalAIProvider`** --- appelle un serveur LLM sur le réseau local
  (adresse IP/port fournis par l'utilisateur, ex. l'API compatible
  OpenAI d'Ollama) --- aucune clé requise, juste une adresse réseau.

Le reste de Rover (Rover Core, comportement, personnalité) n'a jamais
besoin de savoir lequel des deux est actif.

### Configuration : via le portail de pilotage, à tout moment

Contrairement au portail WiFi/OTA de l'ESP32 (provisionnement ponctuel,
un point d'accès temporaire), ce réglage doit rester accessible **en
permanence** depuis l'interface web existante (`pi/rover_control`, celle
qui pilote déjà Rover) --- un panneau "IA" protégé par le même jeton
d'accès (`rover_control/auth.py`) que le reste du portail, où
l'utilisateur peut à tout instant :

- choisir "API cloud" ou "réseau local" ;
- s'il choisit cloud : le fournisseur (Gemini/ChatGPT/Claude/...), le
  modèle, et sa clé API ;
- s'il choisit local : l'adresse (IP:port) du serveur LLM sur le réseau.

### Stockage des identifiants

Comme `ROVER_CONTROL_TOKEN` (`rover_control/auth.py`) et comme
`config.py` le rappelle explicitement dans son propre docstring
("Never put secrets here"), **la clé API ne doit jamais aller dans
`config.json`**. Elle vit dans un fichier dédié, git-ignoré, permissions
restreintes (lecture propriétaire seulement), écrit par le serveur quand
l'utilisateur valide le formulaire --- même logique que
`WifiCredentialsStore` côté ESP32 (`esp32/lib/network/`), transposée en
Python sur le Pi.

### Comportement en cas d'échec

Un fournisseur IA indisponible (API cloud en panne/quota dépassé, second
Pi local éteint ou injoignable) ne doit jamais faire planter Rover ni
bloquer le reste du robot --- mode dégradé (§21) : Rover continue de
fonctionner (mouvement, capteurs, télémétrie), signale juste
l'indisponibilité de la conversation.

------------------------------------------------------------------------

## 17.2 Speech-to-Text / Text-to-Speech (module `rover_audio`)

**Statut (2026-09-11) : écrit et testé (`pi/rover_audio/`), panneau web
de configuration + test disponible (`/audio`, voir `pi/README.md`
"rover_audio"), et maintenant chaîné bout en bout côté logiciel ---
`pi/rover_control/voice_panel.py` (`VoicePanel.converse`, route `POST
/audio/converse`) enchaîne STT → `AIPanel.ask()` (persona/historique
§17.1) → TTS en une seule requête, chaque étage identifié en cas
d'échec. **Toujours pas branché à un micro/haut-parleur réel** ("Micro"
ci-dessus, pas câblé) --- testable dès maintenant depuis `/audio` avec un
fichier audio à la place d'un micro.**

Même principe interchangeable "cloud ou réseau local" que §17.1, appliqué
aux deux bouts de la chaîne audio plutôt qu'à la conversation elle-même :

-   `SpeechToTextProvider.transcribe(audio) -> texte` --- alimente
    `AIPanel.ask()` (donc `PersonalityEngine.converse()`, §17.1) via
    `VoicePanel`, exactement comme le fait déjà le panneau `/ai` texte.
-   `TextToSpeechProvider.synthesize(texte) -> audio` --- consomme la
    réponse que `PersonalityEngine.converse()` produit déjà.

Un seul fournisseur cloud pour l'instant (`OpenAIWhisperSTT`/`OpenAITTS`,
`pi/rover_audio/cloud.py`) --- rover-ai (§17.1) est parti de 3 fournisseurs
cloud pour en arriver à 8 sur demande une fois la forme validée ; même
chemin de croissance ouvert ici plutôt que d'en deviner d'autres à
l'avance. Fournisseur réseau local générique (`LocalSTT`/`LocalTTS`,
`pi/rover_audio/local.py`) pour tout serveur auto-hébergé exposant une
API compatible OpenAI (`/audio/transcriptions`, `/audio/speech`) --- même
raisonnement que `LocalAIProvider`, aucune clé requise.

Identifiants stockés dans `pi/audio_credentials.json` (git-ignoré,
permissions `0600`, jamais dans `config.json`) --- même logique que
`pi/ai_credentials.json`. STT et TTS sont configurés indépendamment
(fournisseur/vendeur différents possibles pour chacun) mais partagent ce
même fichier, comme un futur panneau "Audio" unique le suggère déjà
(`rover_control/audio_panel.py` + `/audio`).

**Panneau web** : `/audio` permet de configurer STT et TTS séparément,
de tester une transcription (fichier audio uploadé depuis le navigateur)
et une synthèse (texte tapé, audio joué directement dans la page) --- le
tout utilisable dès maintenant depuis n'importe quel appareil, sans
attendre le micro/haut-parleur du robot.

------------------------------------------------------------------------

## 17.3 Conséquence du Pi déporté : l'audio doit traverser le réseau

Décision du 2026-09-15 (§6.2). C'est **le seul sous-système réellement
pénalisé** par le déport du Pi, et il mérite d'être traité à part.

Le problème : l'ampli MAX98357A est câblé sur l'**ESP32** (I2S,
`AIDE_CABLAGE.md`), le micro y sera également (voir ci-dessous), mais le
STT/TTS vit sur le **Pi**, désormais à l'autre bout du WiFi. L'audio doit
donc traverser le réseau dans les deux sens --- et le Rover Protocol,
orienté ligne ASCII avec checksum, n'est pas fait pour ça. Il faut un
**canal séparé**, distinct du lien de commande.

### Contrainte dimensionnante : streamer, pas bufferiser

À 16 kHz mono 16 bits, l'audio pèse 32 Ko/s. Sur un WROOM (~274 Ko de
RAM libre au link, moins ~40-60 Ko pris par la pile WiFi) :

| Approche | RAM nécessaire | Verdict WROOM |
|---|---|---|
| Bufferiser un énoncé complet de 5 s | ~160 Ko (+33 % si base64) | Passe, mais fragile |
| Streamer au fil de la capture (DMA I2S → TCP) | ~8-16 Ko | Confortable |

**Le streaming n'est pas une optimisation, c'est la condition qui rend
le WROOM viable.** En mode flux, la RAM cesse d'être un sujet.

### Ce que ça impose côté Pi

`POST /audio/converse` (§17.2, `pi/rover_control/voice_panel.py`) fait
déjà conceptuellement le bon travail --- « audio en entrée → audio en
sortie en un appel » --- mais il renvoie l'audio en **base64**. Ce choix
était délibéré et reste le bon pour le navigateur (robustesse des
accents français, voir `PROGRESS.md` 2026-09-11) ; il est en revanche
inadapté à un client ESP32, puisqu'il oblige à bufferiser toute la
réponse avant de pouvoir en jouer la moindre milliseconde.

Il faudra donc une **variante binaire/streaming** de cette route pour le
client embarqué, sans toucher à la route existante que le panneau web
utilise. C'est du Python, sans difficulté particulière --- mais c'est du
travail à prévoir, pas un détail d'implémentation.

### Micro : la broche est enfin disponible

Le micro (`BOM.md`, resté « non décidé » jusqu'ici) n'avait nulle part
où aller : plus aucun GPIO libre sur le WROOM. Le déport du Pi libère
`GPIO1`/`GPIO3` (ex-UART0 vers le Pi) et résout le problème.

Un micro I2S type INMP441 peut **partager BCLK (`GPIO16`) et WS
(`GPIO17`) avec l'ampli** en mode full-duplex sur I2S0 : il ne réclame
donc qu'**une seule broche supplémentaire** pour sa sortie data.
`GPIO3` (RX) est le bon candidat --- c'est une entrée, et contrairement à
`GPIO1`/TX elle n'émet pas le log de boot.

⚠ **Coût à assumer** : occuper `GPIO3` revient à perdre la console série
USB. Le scénario qui pique est identifié --- *si le WiFi tombe, on perd
le pilotage **et** le moyen de déboguer en même temps*. Mitigation
retenue avec l'utilisateur le 2026-09-15 : un **cavalier (jumper)** sur
la piste du micro, à retirer pour flasher ou déboguer en filaire, avec
la CAO adaptée pour le rendre accessible sans démonter le robot. Détail
dans `esp32/WIRING.md`.

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

**Révision du budget (2026-09-15)** : le Pi déporté (§6.2) sort du
budget de puissance embarqué. Le pic passe de ~32 W à **~24,5 W**
(~3,3 A côté pack à 7,4 V au lieu de ~4,3 A). Surtout, le gain réel est
bien supérieur à ces 23 % : le pic correspond aux moteurs à fond, alors
qu'**au repos ou en déplacement lent le Pi était le premier
consommateur**. Conséquences matérielles :

-   le **convertisseur buck 5V/3A dédié au Pi** (`BOM.md`) n'est plus
    nécessaire --- une carte, du volume et du câblage en moins ;
-   le risque de **brownout/corruption de carte SD** qui avait imposé de
    séparer les rails disparaît complètement, puisqu'il n'y a plus ni
    Pi ni carte SD à bord ;
-   le rail 5V restant (servos + ESP32 + ampli audio) reste dimensionné
    à l'identique.

**État (2026-08-31)** : `STATE battery=...` et `EVENT name=low_battery`
sont implémentés côté ESP32 (`esp32/lib/power/BatteryMonitor.h`), mais
**désactivés par défaut** (`ROVER_BATTERY_MONITORING_ENABLED = false`,
`power_config.h`) tant que le diviseur de tension n'est pas câblé et
calibré --- contrairement aux capteurs I2C (Phase 4), une broche ADC ne
peut pas détecter "rien n'est branché ici", donc mieux vaut ne rien
publier que publier du bruit comme si c'était un vrai niveau de
batterie. Un bouton d'arrêt d'urgence physique existe aussi maintenant
(`esp32/lib/safety/EStop.h`, `EVENT name=estop_pressed`) --- c'est
exactement la "protection électrique matérielle indépendante du
logiciel" que cette section demande, mais implémentée en dur dans le
firmware plutôt qu'un circuit séparé : elle fonctionne dès le boot,
avec ou sans bouton physique câblé.

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

### Lien WiFi perdu (depuis le Pi déporté, 2026-09-15)

Distinct du cas ci-dessus : le Pi va parfaitement bien, c'est le
**réseau** qui lâche. Depuis §6.2 c'est devenu le mode de défaillance le
plus probable, alors qu'il était négligeable à l'époque du câble USB.

``` text
WiFi ❌
 ↓
ralentissement (~30 %)     -- palier DEGRADED, §9
 ↓  (si ça persiste)
STOP -> SAFE
 ↓
l'ESP32 continue seul : réflexes d'obstacle, E-stop, écran
```

L'ESP32 reste pleinement fonctionnel sans Pi --- c'est déjà la règle
(§4.2), elle devient simplement beaucoup plus sollicitée. Il tente sa
reconnexion en continu et reprend au vol dès que le Pi est de nouveau
joignable, sans redémarrage.

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
départ arbitraires). **Calibration des gains PID désormais possible sans
reflash** : `SYSTEM action=set_pid/get_pid/reset_pid`, persisté en NVS
(`esp32/lib/calibration/CalibrationStore.h`), validé en conditions
réelles y compris la survie à un redémarrage (PROGRESS.md 2026-08-31)
--- ne remplace pas le calibrage lui-même (toujours à faire une fois les
moteurs/encodeurs réels disponibles), juste l'outillage pour le faire
vite. La géométrie roue (`ROVER_WHEEL_DIAMETER_M` etc.) reste en dur
dans `motion_config.h` pour l'instant, pas encore dans ce mécanisme.

Simulation Wokwi d'un encodeur moteur (boucle fermée en simulation)
étudiée mais **pas implémentée** : Wokwi n'a pas de chip officiel pour
ça, seulement des projets tiers avec un chip personnalisé (API Custom
Chips C de Wokwi) --- écrire un tel chip depuis zéro n'a pas été fait
cette session, faute de pouvoir le valider (pas d'accès `wokwi-cli`
avec jeton dans cet environnement). La boucle PID continue donc de
tourner en boucle ouverte en simulation (`left_speed`/`right_speed`
restent à 0), sujet à reprendre avec l'accès Wokwi complet.

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
-   [x] Animations --- mécanisme générique (`ANIMATION name=...`), trois
      animations concrètes : `GLITCH` (glitch RGB-split intensifié),
      `LOOK_AROUND` (balayage du regard gauche-droite-centre) et
      `WAKE_UP` (ouverture grand écarquillée qui redescend) --- toujours
      pas une bibliothèque complète, mais plus la preuve d'un seul
      mécanisme isolé. Voir PROGRESS.md 2026-08-31.
-   [x] Glitch engine --- l'effet RGB-split (utilisé par `ALERT` et par
      l'animation `GLITCH`).
-   [x] Commandes FACE.
-   [x] Commandes ANIMATION.

Complet au niveau code et compile sur les deux cibles ; validé en
simulation Wokwi au niveau protocole (dispatch sans crash) et
**désormais validé visuellement sur écran ST7789 physique réel**
(240x280, paysage) --- voir PROGRESS.md 2026-08-31. Yeux, contour
glitch cyan/rose et animation GLITCH confirmés à l'œil. Servos toujours
non testés sur matériel réel.

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

-   [x] MPU6050.
-   [x] VL53L0X gauche.
-   [x] VL53L0X droite.
-   [x] BME280 (remplace le BME688 initialement prévu ici --- voir
      PROGRESS.md 2026-09-02, le module reçu est en fait un BME280,
      chip-id 0x60 confirmé au firmware puis par le lien d'achat, pas de
      capteur de gaz sur cette pièce ; `EnvironmentSensor` adapté à
      `Adafruit_BME280`, `gas_kohm` reste toujours à 0 en télémétrie).
-   [x] Lecture périodique.
-   [x] Gestion des erreurs.
-   [x] Transmission STATE.
-   [x] Transmission EVENT.

Complet au niveau code, compile sur les deux cibles, **validé sur ESP32
WROOM physique réel** (voir PROGRESS.md 2026-08-31) mais sans aucun
capteur encore câblé : le test confirme que l'absence des quatre
capteurs est gérée proprement (`ERROR code=sensor_timeout sensor=...`
une fois par capteur au boot, aucun crash, télémétrie `STATE` avec
valeurs "indisponible"), pas que les capteurs eux-mêmes lisent des
valeurs correctes. Deux bugs réels trouvés et corrigés pendant ce
premier test (driver VL53L0X bloquant sur capteur absent ; buffers de
trame trop petits tronquant `ERROR`/`STATE` en silence) -- voir
PROGRESS.md pour le détail. **Mise à jour 2026-09-02** : MPU6050 et
BME280 câblés et validés sur matériel réel avec des valeurs cohérentes
(voir PROGRESS.md) ; VL53L0X gauche/droite restent à câbler/valider.

Résultat :

Le Pi possède une représentation de l'état physique de Rover.

------------------------------------------------------------------------

# PHASE 5 --- Raspberry Pi

Objectif : créer le cerveau.

-   [ ] OS.
-   [ ] Services Rover.
-   [x] Rover Core (`pi/rover_core/`) --- gère la connexion ESP32, le
      heartbeat/resume, et désormais une première machine à états de
      haut niveau (voir ci-dessous).
-   [x] Rover ESP32 Interface (`pi/rover_esp32/`) --- pont Rover
      Protocol complet (encode/decode conforme à
      `RoverProtocol.cpp`, testé trame par trame), voir PROGRESS.md.
-   [x] Configuration --- fichier `pi/config.json` optionnel
      (`pi/config.example.json` pour le gabarit), un argument CLI
      explicite garde priorité. Voir `pi/rover_core/config.py`.
-   [x] Logs --- fichier journal tournant optionnel (`--log-dir`,
      `RotatingFileHandler`), console par défaut. Voir
      `pi/rover_core/main.py`.
-   [ ] Gestion des événements.
-   [x] Machine à états --- `RoverBehaviorState` (`pi/rover_core/core.py`)
      reprend exactement l'énumération de la section 20
      (IDLE/INTERACTING/MOVING/EXPLORING/PATROLLING/FOLLOWING/CHARGING/
      SLEEPING/ERROR), mais seules les transitions pilotables par ce qui
      existe déjà sont câblées : connexion/déconnexion d'un client de
      contrôle (IDLE ↔ INTERACTING), commande `MOVE` non nulle (→
      MOVING), et une `ERROR` ESP32 (→ ERROR, avec retour automatique
      après quelques secondes). EXPLORING/PATROLLING/FOLLOWING/CHARGING/
      SLEEPING n'ont aucun déclencheur pour l'instant (dépendent de
      phases pas encore commencées : navigation, vision, énergie).
      Validé en conditions réelles contre l'ESP32 physique (voir
      PROGRESS.md 2026-08-31).

Résultat :

Le Pi devient l'orchestrateur central. **Tranche minimale seulement**
--- suffisante pour piloter Rover (Phase 6 minimale), pas
l'orchestrateur complet décrit ici.

------------------------------------------------------------------------

# PHASE 6 --- Caméra et contrôle distant

Objectif : voir et piloter Rover.

-   [x] Caméra --- **décidée (2026-09-06)** : module **ESP32-CAM**
      déporté et indépendant plutôt qu'une caméra Raspberry Pi (pas de
      place en CAO dans la tête), voir §4.3. Matériel disponible, pas
      encore câblé/flashé.
-   [x] Flux vidéo --- `pi/rover_control/camera.py` réécrit (2026-09-06,
      voir §4.3) : relaie (reverse proxy `aiohttp`) le flux MJPEG de
      l'ESP32-CAM à travers `/video`, authentifié comme le reste.
      Configurable via `--camera-url`/`camera_url`. Logique validée
      avec un faux serveur MJPEG (upstream/dead-upstream/pas-configuré,
      voir PROGRESS.md) --- **pas encore testé contre le vrai
      ESP32-CAM**, pas encore flashé/câblé. Répond `503 unavailable`
      tant que `camera_url` n'est pas configuré ou que l'ESP32-CAM est
      injoignable.
-   [x] Interface de contrôle (`pi/rover_control/`, page web
      autonome, pas de build).
-   [x] Commandes MOVE --- validé de bout en bout **sur le robot réel**
      (2026-09-06) : téléphone → Pi (WiFi) → Rover Protocol (série USB) →
      ESP32 → moteurs. Joystick tactile confirmé fonctionnel, les roues
      tournent. Voir PROGRESS.md.
-   [x] Commandes HEAD --- la Phase 3 (servos tête) est terminée côté
      ESP32 depuis une session précédente, ce point n'avait juste
      jamais été branché côté interface : deuxième pad tactile (regard)
      + stick droit de manette (convention gauche=déplacement,
      droite=regard), `RoverCore.look()` → `HEAD pitch=... yaw=...`.
      Validé de bout en bout en conditions réelles (WebSocket → Rover
      Protocol → ESP32, voir PROGRESS.md 2026-08-31) ; pas testé au
      toucher sur un vrai téléphone/manette (même limite que MOVE).
-   [x] Retour d'état -- les trames `STATE`/`EVENT`/`ERROR` sont
      affichées dans l'interface (commit `ebc2aa3`) ; désormais
      améliorées avec un état fusionné (les différentes lignes `STATE`
      --- distance/IMU/environnement/vitesse --- s'accumulent au lieu de
      s'écraser), un badge d'état comportemental (IDLE/INTERACTING/
      MOVING/ERROR/...) et un bandeau d'alerte obstacle basé sur
      `distance_left`/`distance_right` (même seuil que l'`EVENT
      obstacle_detected` de l'ESP32). Voir PROGRESS.md 2026-08-31.
-   [x] Contrôle smartphone (joystick tactile, Pointer Events) ---
      **validé au toucher sur un vrai téléphone (2026-09-06)** : les
      roues répondent bien au joystick, sur le robot réel.
-   [x] Support manette / gamepad (Gamepad API) --- code écrit, reste à
      valider avec une manette physique branchée (le joystick tactile,
      lui, est validé).
-   [x] Accès distant sécurisé (VPN) --- guide complet WireGuard
      auto-hébergé (`pi/VPN.md`), templates de config commités
      (`pi/wireguard/*.example.conf`, jamais de vraie clé committée),
      un seul port UDP à exposer sur la box (jamais le port HTTP/HTTPS
      de `rover_control` directement). Le token d'accès reste
      obligatoire une fois connecté au VPN --- défense en profondeur,
      pas un remplacement. **Toujours pas testé en conditions réelles**
      --- Pi désormais disponible (voir PROGRESS.md 2026-09-06), mais
      pas encore essayé à travers la box/routeur.
-   [x] Authentification --- token d'accès obligatoire sur toutes les
      routes (page, WebSocket, vidéo), généré aléatoirement à chaque
      démarrage si `ROVER_CONTROL_TOKEN` n'est pas fixé par
      l'utilisateur, jamais de valeur par défaut codée en dur (important
      pour un projet open source, voir `pi/README.md` "Sécurité").
      Chiffrement TLS/WSS désormais disponible en option
      (`--tls-cert`/`--tls-key`, certificat auto-signé à générer
      soi-même, voir `pi/README.md` "HTTPS/WSS") --- toujours à ne pas
      exposer hors d'un réseau local de confiance sans le VPN ci-dessus.

Résultat :

Rover peut être piloté à distance (smartphone ou manette) avec retour
vidéo, y compris hors du réseau local via VPN. **Pilotage validé sur le
robot réel (2026-09-06)** : joystick tactile → Pi (WiFi) → ESP32 →
moteurs, de bout en bout, token d'authentification vérifié (`403` sans,
`200` avec) ; **caméra ESP32-CAM (§4.3) : firmware écrit et compile,
relais vidéo Pi réécrit et validé en isolation (faux serveur MJPEG),
mais rien encore testé sur le module physique** (pas flashé/câblé) ; VPN
toujours pas testé en conditions réelles (voir PROGRESS.md pour l'état
détaillé).

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

-   [ ] Micro --- aucune capture audio réelle : dépend du matériel
      (INMP441, BOM) pas encore câblé.
-   [x] Audio pipeline --- orchestration écrite et testée
      (`pi/rover_control/voice_panel.py`, route `POST /audio/converse`) :
      un audio → STT → `AIPanel.ask()` (persona/historique §17.1) →
      TTS → audio réponse, chaque étage identifié en cas d'échec
      (`VoiceTurnError.stage` = `stt`/`ai`/`tts`). **Seul maillon
      manquant : aucun vrai micro/haut-parleur physique** (Micro
      ci-dessus, pas câblé) --- utilisable dès maintenant depuis le
      panneau `/audio` avec un fichier audio uploadé à la place.
-   [x] Speech-to-Text (`pi/rover_audio/`, voir §17.2) --- écrit, testé
      (fakes, aucun vrai réseau), panneau web `/audio` pour tester une
      transcription dès maintenant (fichier audio uploadé) ---
      **jamais testé avec une vraie clé API**, et rien n'alimente
      encore ce provider avec de l'audio réel (pas de micro).
-   [x] IA conversationnelle (`rover-ai`, voir §17.1) --- fournisseur API
      cloud (Gemini/ChatGPT/Claude/...) ou LLM réseau local (ex. Qwen
      2.5 sur un second Raspberry Pi via Ollama), interchangeable à tout
      moment depuis le portail de pilotage (`pi/rover_control`). Écrit,
      testé (fakes, aucun vrai réseau), branché dans le panneau web ---
      **jamais testé avec une vraie clé API sur le Pi réel** (pas de
      Pi disponible pour l'instant, voir PROGRESS.md).
-   [x] Text-to-Speech (`pi/rover_audio/`, voir §17.2) --- écrit, testé
      (fakes), panneau web `/audio` pour tester une synthèse (texte
      tapé, audio joué dans le navigateur) --- **jamais testé avec une
      vraie clé API**, et rien ne relie encore sa sortie à un
      haut-parleur physique (pas câblé).
-   [x] Personality Engine (`pi/rover_ai/personality.py`) --- persona +
      historique de conversation (borné) injectés dans chaque appel
      IA, réaction émotionnelle (`RoverCore.set_emotion`) sur les
      échanges (`curious`/`happy`/`confused`). Ne couvre encore que la
      réaction "je discute avec toi" --- pas le reste de "machine à
      émotions" ci-dessous (contexte hors conversation : batterie,
      heure, présence).
-   [ ] Machine à émotions --- au-delà de la réaction liée à une
      conversation IA (ci-dessus, déjà en place), reste à faire :
      humeur influencée par le contexte hors conversation (niveau de
      batterie, heure/inactivité, interactions récentes -- voir
      README "Personnalité Dynamique").
-   [ ] Connexion aux commandes Rover --- l'IA ne peut aujourd'hui que
      parler (texte), pas encore déclencher un `MOVE`/`HEAD` réel ;
      volontairement pas fait sans validation explicite du schéma de
      sécurité (bornes, confirmation) puisqu'il s'agit de faire bouger
      le robot physique.

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
-   [x] Visualisation AR (casque Meta Quest 3 / WebXR, voir §13.1) ---
      **premier incrément seulement** : HUD télémétrie temps réel
      (`pi/rover_control/static/ar-hud.html`) superposé au passthrough
      caméra. La superposition de la carte réelle attend Localisation/
      Cartographie ci-dessus --- rien à afficher tant que ces cases ne
      sont pas cochées.

ROS 2 peut être introduit à ce stade si sa complexité apporte une vraie
valeur.

**ROS 2 n'est pas une obligation pour Rover V1.**

------------------------------------------------------------------------

# PHASE 10 --- Home Assistant

Objectif : intégrer Rover à la maison.

-   [x] MQTT --- **premier pas minimal seulement**, pas l'intégration
      complète décrite par cette phase : `pi/rover_mqtt/publisher.py`
      publie périodiquement un instantané (état comportemental + tous
      les champs `STATE` connus) sur `<prefix>/state`, dépendance
      optionnelle (`paho-mqtt`, `requirements-mqtt.txt`, pas dans les
      dépendances de base) et broker optionnel (rien ne se passe si non
      configuré). Voir PROGRESS.md 2026-08-31.
-   [x] États Rover --- inclus dans l'instantané ci-dessus
      (`behavior_state`).
-   [x] Capteurs --- inclus dans l'instantané ci-dessus (distance/IMU/
      environnement/vitesse, tout ce que `RoverCore.last_state`
      connaît).
-   [ ] Événements --- pas encore publiés séparément (seul l'état
      fusionné l'est, pas les `EVENT`/`ERROR` individuels).
-   [ ] Commandes --- aucune commande entrante depuis Home Assistant
      (MQTT sortant uniquement pour l'instant).
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
   ESP32-CAM (filme)               ┌─────────────────────┐
        │                          │    RASPBERRY PI     │
        │ WiFi, flux MJPEG,        │                     │
        │ indépendant du reste     │    ROVER CORE       │
        └────────────────────────▶ │         │           │
                                    │  ┌──────┼───────┐   │
                                    │  │      │       │   │
                                    │ AI    Vision   Nav  │
                                    │  │      │       │   │
                                    │ Audio            │   │
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
