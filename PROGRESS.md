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

- **DÉCISION D'ARCHITECTURE --- Raspberry Pi déporté (2026-09-15)** :
  le Pi quitte le châssis et devient une machine du réseau local ; le
  Rover Protocol passe du câble USB à une **socket TCP WiFi**. Le robot
  n'embarque plus qu'ESP32 + ESP32-CAM + batterie. **Carte confirmée :
  WROOM** (un S3 est disponible mais gardé pour plus tard, objectif =
  finir le projet sur le WROOM). Raisonnement complet dans
  `ARCHITECTURE_AND_ROADMAP.md` §6.2, rédigé selon les 7 questions de la
  §28. Points saillants :
  - **Coût en code quasi nul** : les deux extrémités étaient déjà
    abstraites sans que ç'ait été prévu --- `serial_for_url()` côté Pi
    accepte `socket://`, `RoverProtocol` côté ESP32 prend un `Stream&`
    (donc un `WiFiClient`). Aucun changement du protocole V1.
  - **Mesures réelles** (compilation WROOM ce jour) : RAM 16.2 %
    (53 144/327 680), Flash **71.0 %** (930 353/1 310 720, déjà en
    partition OTA double). La ressource tendue est la flash, pas la RAM.
  - **Vraie difficulté = le heartbeat** : les 500 ms étaient calibrées
    pour un câble. Remplacées par 3 paliers (NOMINAL / DEGRADED ~30 % de
    vitesse à 500 ms / TIMEOUT→SAFE à ~1500 ms), §9. ⚠ **Valeurs non
    mesurées**, à confirmer en conditions réelles.
  - **Seul sous-système pénalisé = l'audio** (§17.3) : ampli/micro sur
    l'ESP32, STT/TTS sur le Pi → l'audio doit traverser le WiFi. Viable
    sur WROOM **à condition de streamer** (~8-16 Ko) et non de
    bufferiser (~160 Ko). Impose une variante binaire/streaming de
    `POST /audio/converse`, qui renvoie du base64 aujourd'hui.
  - **Effet de bord heureux** : le déport libère `GPIO1`/`GPIO3`
    (ex-UART0 vers le Pi) --- or `esp32/WIRING.md` constatait qu'il ne
    restait *aucune* broche libre, ce qui bloquait le micro depuis la
    Phase 4. Micro INMP441 prévu sur `GPIO3` en partageant BCLK/WS avec
    l'ampli. Contrepartie assumée : perte de la console série USB,
    **mitigée par un cavalier** que la CAO rendra accessible.
  - Budget de puissance revu (~32W → ~24.5W pic), **buck 5V/3A dédié au
    Pi abandonné** (`BOM.md`), risque de brownout/corruption SD éliminé.
  - Documentation mise à jour : `ARCHITECTURE_AND_ROADMAP.md` (§4.1,
    §6.2, §9, §17.3, §19, §21), `ROVER_PROTOCOL.md` (§2, §6, §10),
    `esp32/WIRING.md`, `AIDE_CABLAGE.md`, `BOM.md`, `README.md`,
    `pi/README.md`, `esp32/OTA.md`.
- **`RoverLink` rendu résistant aux coupures réseau (2026-09-15)**,
  même session que la décision ci-dessus --- **seul code modifié à ce
  jour pour le Pi déporté**, et ce n'est pas du développement neuf mais
  la correction d'un défaut que le WiFi rend critique.
  Constat : `pi/rover_esp32/link.py` ouvrait la connexion dans
  `__init__` et écrivait dans `self._serial` sans aucune gestion
  d'erreur. Acceptable sur un câble USB (le port existe ou pas),
  **cassé sur une socket WiFi** où une coupure est un événement normal :
  (a) le process Pi refusait de démarrer si l'ESP32 n'écoutait pas
  encore, (b) `send()` levait `SerialException` → `_heartbeat_loop`
  (`core.py`) ne rattrape que `CancelledError`, donc **la tâche
  heartbeat mourait définitivement** et ne repartait jamais même réseau
  revenu, (c) le thread lecteur mourait en silence. Précédent réel de
  cette classe de panne : le bug non-ASCII du 2026-09-10 qui *« killed
  the pyserial reader thread entirely »* (`tests/test_protocol.py`).
  Violait aussi la règle `CLAUDE.md` « gère systématiquement les erreurs
  de communication ».
  Corrections : connexion supervisée avec reconnexion et backoff
  (0.5s → 5s, remis au minimum après succès), `start()` non bloquant
  (le serveur de contrôle démarre même robot éteint), `send()` renvoie
  `False` au lieu de lever, exceptions du consommateur et
  `connection_lost` neutralisées pour ne plus tuer le lien, et une
  course `stop()`-pendant-connexion fermée sous verrou (elle laissait
  fuir un thread lecteur --- **trouvée grâce à un test flaky**, pas par
  relecture). API publique inchangée : `RoverCore` et `main.py` n'ont eu
  besoin d'aucune adaptation.
  **9 nouveaux tests (`pi/tests/test_link.py`, entièrement nouveau)** ---
  contre un vrai serveur TCP en mémoire via `socket://`, donc la
  reconnexion est réellement exercée. **145 tests `pi/`**, suite lancée
  4× d'affilée pour vérifier l'absence de flakiness.
  ⚠ Restent 2 échecs **pré-existants et propres à Windows**
  (`test_rover_ai`/`test_rover_audio`, permissions `0600` des fichiers
  d'identifiants : `os.chmod` ne pose pas de bits POSIX sous Windows,
  mode 438 = `0o666`). Sans rapport avec cette session, devraient passer
  sous Linux Mint --- **à reconfirmer sur le vrai Pi**, ne pas
  « corriger » en assouplissant l'assertion, c'est une vraie garantie de
  sécurité côté Linux.
  **Toujours rien côté ESP32** : le firmware parle encore uniquement sur
  `Serial`, pas de `WiFiServer`, pas de paliers de heartbeat.
- **Pilotage autonome sans Pi ni réseau (2026-09-15)** --- exigence de
  l'utilisateur, qui **revient sur un compromis que §6.2 présentait comme
  accepté** (« plus de WiFi = plus de cerveau »). La perte de
  l'*intelligence* hors réseau reste assumée ; la perte du *pilotage*
  non. Nouveau `esp32/lib/network/StandaloneControl.h` : l'ESP32 ouvre
  son propre AP WPA2 et sert une page joystick (`PROGMEM`, pas de CDN --
  il n'y a pas d'Internet sur cet AP par définition). Conception en
  §6.3.
  - **Principe : réutiliser, jamais dupliquer.** Le module ne contient
    aucune logique de sécurité propre --- la page alimente le *même*
    `HeartbeatMonitor` que le Pi (fermer l'onglet ou sortir de portée
    arrête donc les moteurs par le timeout existant), l'E-stop prime
    toujours, le réflexe d'obstacle s'applique, et entrer dans ce mode
    ne réarme pas les moteurs.
  - **WPA2 obligatoire, refus de démarrer sans mot de passe** --- calqué
    sur `RoverOTA`. Un AP ouvert serait pire que dans le cas OTA : il ne
    flashe pas un firmware, il déplace un robot. Mot de passe distinct
    de celui de l'OTA (`solo_pass` en NVS), collecté par le portail
    existant, refusé si < 8 caractères (sans quoi `softAP()` bascule
    silencieusement en réseau **ouvert**).
  - **Déclenchement** : automatique après 30 s sans trame du Pi ---
    couvre « le lien est mort » *et* « il n'y a pas de Pi du tout »
    (avant la première trame, le compteur mesure depuis le boot, ce qui
    est voulu : c'est le cas d'usage principal). Manuel via
    `SYSTEM action=standalone`.
  - **Deux pièges attrapés en cours d'écriture**, tous deux notés dans
    le code : (1) la page alimentant le heartbeat, s'en servir pour
    détecter « le Pi est revenu » était **circulaire** et coupait l'AP
    sous les pieds du pilote → suivi séparé de `lastPiFrameMs` ;
    (2) une première version ne se déclenchait jamais sur un robot
    démarré sans Pi --- exactement le cas visé.
  - **Coût : +8 Ko de flash** (71,0 % → 71,6 %), +400 octets de RAM ---
    `WebServer` était déjà lié par le portail de provisioning.
  - ⚠ **Jamais exécuté sur matériel** : WROOM et S3 compilent, 18/18
    tests natifs, relecture --- rien de plus.
- **Revue de code complète (2026-09-15)** --- passe sur l'ensemble du
  dépôt (~9 300 lignes : `pi/`, `esp32/`, `esp32-cam/`) à la recherche
  d'incohérences, de failles et de points bloquants pour le Pi déporté.
  **Le constat le plus important m'a fait corriger ma propre
  documentation** : §6.2 affirmait que les réflexes d'obstacle étaient
  déjà locaux à l'ESP32 « puisque les VL53L0X sont de son côté ». Faux :
  les *capteurs* l'étaient, le *réflexe* non --- le firmware n'émettait
  qu'un `EVENT`, le seul blocage réel vivait côté Pi. L'argument de
  sécurité central de la décision reposait donc sur du vide.
  **8 corrections appliquées** (détail et raisonnement dans le diff) :
  - **Réflexe d'obstacle local** (`DriveController::setForwardBlocked`,
    appliqué dans `update()` donc valable aussi pour une commande
    périmée ; marche avant seule, reculer/tourner restent possibles ;
    inerte si aucun ToF sain). Le clamp du Pi est **conservé** : deux
    couches indépendantes. Nouveau champ `forward_blocked=` en
    télémétrie --- sans lui, « le robot n'avance plus » serait
    indiagnosticable depuis un Pi déporté.
  - **Troncature silencieuse des trames sortantes**
    (`RoverProtocol::send`) : une trame coupée repartait avec un
    checksum valide *calculé sur le texte tronqué*, donc indétectable
    côté réception. Ce dépôt s'est fait avoir 3 fois par des buffers
    trop petits (commentaires « 48, not 32 » / « 96, not 64 » dans
    `main.cpp`) --- la garde couvre la classe entière au seul endroit
    par où tout passe. 2 tests natifs.
  - **Verrouillage WiFi** : `RoverWifiProvisioning::stop()` finissait
    sur `WIFI_OFF` sans jamais rallumer --- un portail qui expirait tuait
    l'OTA jusqu'au reboot. Après le déport, ç'aurait été le lien de
    pilotage, sans câble pour se rattraper. Retour en `WIFI_STA` +
    `reconnect()`.
  - **Injection HTML dans le portail WiFi** : le SSID stocké était
    réinjecté brut dans `value='...'` --- une apostrophe suffit à sortir
    de l'attribut. Échappement ajouté.
  - **Boucle infinie du firmware caméra** : `stream_handler` retentait
    `esp_camera_fb_get()` sans condition de sortie ; une caméra HS
    confisquait définitivement un worker httpd (et finissait par figer
    le serveur). Abandon après 10 échecs + 503, et `esp_camera_deinit()`
    avant chaque réinitialisation (sans quoi la relance renvoie
    `ESP_ERR_INVALID_STATE` à vie).
  - **Panneaux IA/audio** : le fournisseur était fermé *avant* une
    sauvegarde qui peut échouer → panneau mort jusqu'au redémarrage.
    Ordre inversé.
  - **WebSocket** : une tâche par trame, sans référence conservée
    (asyncio ne garde qu'une référence faible → collecte possible en vol)
    et sans ordre garanti. Remplacé par une file bornée + une tâche de
    drainage unique.
  - **Docstring de `camera.py`** qui surestimait la protection : le
    proxy authentifie ceux qui passent par le Pi, il n'empêche pas
    l'accès direct à l'ESP32-CAM, qui reste ouvert sur le LAN.
  **2 points structurels documentés, non corrigés** (ils demandent du
  firmware et deviennent bloquants pour l'étape 1, voir §6.2 « 5 bis » /
  « 5 ter ») : le lien déporté n'aurait **aucune authentification**
  (alors que le serveur web exige un token et l'OTA un mot de passe), et
  la connexion WiFi vit dans `RoverOTA`, un module explicitement
  optionnel dont le lien de commande ne peut pas dépendre.
  **Vérifié** : WROOM *et* S3 compilent (portabilité `CLAUDE.md`
  respectée), 18/18 tests natifs, 143/145 tests `pi/` (2 échecs
  pré-existants propres à Windows, voir ci-dessous).
- **Audio pipeline --- STT/IA/TTS enchaînés (2026-09-11)**, même
  session que `rover_audio` ci-dessous : nouveau
  `pi/rover_control/voice_panel.py` (`VoicePanel.converse`) enchaîne
  transcription → `AIPanel.ask()` (persona/historique déjà en place,
  §17.1) → synthèse en un seul appel, chaque étage (`stt`/`ai`/`tts`)
  identifié séparément en cas d'échec (`VoiceTurnError.stage`) plutôt
  qu'une erreur indifférenciée. Nouvelle route `POST /audio/converse`
  (réponse JSON : texte entendu, texte de la réponse, audio en base64
  --- choix délibéré plutôt qu'une réponse binaire avec le texte dans des
  en-têtes HTTP, plus sûr avec de l'accentuation française) + section
  "Pipeline complet" sur `/audio` (upload d'un fichier audio, affiche
  l'échange, joue la réponse). **136/136 tests `pi/` (5 nouveaux,
  `test_voice_panel.py`)**, testé de bout en bout en isolation avec des
  fakes (STT/IA/TTS simulés) via un serveur aiohttp en mémoire.
  Referme la boucle logicielle complète de la Phase 7 "Audio pipeline"
  --- **seul maillon manquant : aucun vrai micro/haut-parleur** (pas
  câblés, pas de Pi disponible cette session), donc rien de tout ça
  essayé avec du son réel.
- **Speech-to-Text / Text-to-Speech --- module `rover_audio` écrit
  (2026-09-11)**, suite logique du Personality Engine du même jour :
  `pi/rover_audio/` (interfaces `SpeechToTextProvider`/
  `TextToSpeechProvider`, fournisseur cloud `OpenAIWhisperSTT`/
  `OpenAITTS`, fournisseur réseau local générique `LocalSTT`/`LocalTTS`,
  stockage d'identifiants dans `pi/audio_credentials.json` --- même
  forme que `rover_ai` §17.1, voir `ARCHITECTURE_AND_ROADMAP.md` §17.2).
  Panneau web `pi/rover_control/audio_panel.py` + page `/audio` +
  routes `/audio/config` (GET/POST), `/audio/transcribe` (POST, upload
  audio brut), `/audio/speak` (POST texte → audio) --- toutes protégées
  par le token existant, câblées dans `rover_core/main.py`. Bug de fuite
  de clé API entre fournisseurs (celui déjà trouvé et corrigé sur
  `ai_panel.py` le 2026-09-09) anticipé cette fois dès l'écriture : STT
  et TTS ont chacun leur propre garde "champ vide garde la clé, sauf en
  changeant de fournisseur" (`rover_control/audio_panel.py`
  `_merge_api_key`, testé explicitement --- la clé STT ne doit jamais
  fuiter dans le champ TTS). **131/131 tests `pi/` (38 nouveaux :
  `test_rover_audio.py` et `test_audio_panel.py` entièrement nouveaux)**,
  routes `/audio/*` testées en isolation avec des fakes (200/400/503
  vérifiés). **Rien de tout ça essayé avec une vraie clé API ni un vrai
  micro/haut-parleur** --- pas de Pi disponible cette session, et le
  micro (Phase 7 "Micro") n'est de toute façon pas câblé. Pas encore
  relié à `PersonalityEngine.converse()` (§17.1) --- c'est l'"Audio
  pipeline" de la Phase 7, prochaine étape une fois le micro I2S en main.
- **HUD en réalité augmentée (Meta Quest 3 / WebXR) --- premier
  incrément écrit (2026-09-11)**, suite à une idée de l'utilisateur
  (coupler Rover à son casque Meta Quest 3, argument fort pour l'aspect
  open source du projet). Conçu d'abord (`ARCHITECTURE_AND_ROADMAP.md`
  §13.1, nouvelle section) : WebXR plutôt qu'une app Quest native
  (zéro installation, zéro compte développeur Meta, réutilise le
  serveur/token/`/ws` déjà en place), deux incréments séparés
  volontairement --- HUD télémétrie tout de suite (aucune nouvelle
  donnée requise), superposition de carte plus tard (dépend de la
  Phase 9, Cartographie/Localisation, pas commencée : rien à afficher
  pour l'instant).
  **Code écrit** : nouvelle page `pi/rover_control/static/ar-hud.html`
  (session WebXR `immersive-ar` + feature `dom-overlay`, superpose le
  panneau de télémétrie existant --- distance/IMU/environnement/
  batterie/état --- sur le passthrough caméra ; fallback 2D propre si
  `immersive-ar` n'est pas supporté), route `GET /ar`
  (`rover_control/server.py`, protégée par le même token que le reste),
  lien "RA" ajouté à côté de "IA" sur `index.html`.
  **Testé** : syntaxe JS vérifiée (`node --check`), route `/ar` testée
  en isolation (200 avec token valide, 403 sans --- même middleware
  d'auth que le reste), 93/93 tests `pi/` toujours au vert (aucun test
  dédié à cette page, même convention que `index.html`/`ai.html` : "la
  logique métier est testée, la page web brute ne l'est pas"). **Jamais
  testé sur un vrai casque** (pas de Meta Quest 3 disponible cette
  session, écrit au plus près de la spec WebXR) --- seul le chemin de
  repli 2D (qui partage le même code de rendu du HUD) a pu être vérifié
  ici. Voir `pi/README.md` "Réalité augmentée" pour l'usage.
- **Personality Engine --- écrit et testé (2026-09-11)**, session
  100% code (utilisateur sans accès au robot physique). Nouveau
  `pi/rover_ai/personality.py` (`PersonalityEngine`) branché dans
  `AIPanel.ask()` (`pi/rover_control/ai_panel.py`) : chaque appel IA
  construit maintenant un vrai `AIContext` (persona Rover + historique
  de conversation borné à 10 échanges) au lieu d'un message "sec" sans
  mémoire ni personnalité --- tous les fournisseurs (`cloud.py`,
  `_openai_compatible.py`) savaient déjà lire `context.system`/
  `context.history`, rien ne les alimentait jusqu'ici. Réaction
  émotionnelle ajoutée sur l'échange : `RoverCore.set_emotion()`
  (nouvelle méthode publique, remplace les deux envois `FACE` en dur
  déjà présents dans `core.py`) envoie `curious` pendant l'appel,
  `happy` sur une réponse, `confused` sur une `AIProviderError` --- sans
  jamais faire planter la conversation si l'envoi échoue (câble série
  down, etc.). Câblé dans `rover_core/main.py`
  (`PersonalityEngine(emotion_sink=core.set_emotion)`). Nouvelle route
  `POST /ai/reset` (bouton "Nouvelle conversation" sur `/ai`) pour
  repartir sans historique ; changer de fournisseur/vendeur via
  `/ai/config` réinitialise aussi l'historique automatiquement (éviter
  qu'une conversation construite contre un backend survive au
  changement vers un autre). Voir `ARCHITECTURE_AND_ROADMAP.md` §17.1 et
  Phase 7 pour le détail, `pi/README.md` "rover-ai" pour l'usage.
  **93/93 tests `pi/` (16 nouveaux : `pi/tests/test_personality.py`
  entièrement nouveau, plus des ajouts dans `test_ai_panel.py` et
  `test_core.py`)** --- testé uniquement avec des fakes, comme le reste
  de `rover-ai` jusqu'ici : **rien de tout ça essayé sur le Pi réel**
  (pas de matériel disponible cette session).
  Volontairement pas fait dans la foulée, pour rester dans le
  périmètre "Personality Engine" et ne pas prendre de décision de
  sécurité sans l'utilisateur : "Connexion aux commandes Rover" (l'IA
  ne peut aujourd'hui que répondre en texte, pas déclencher un
  `MOVE`/`HEAD` réel) et la partie "Machine à émotions" hors
  conversation (humeur influencée par la batterie/l'heure/l'inactivité,
  cf. README "Personnalité Dynamique") --- les deux restent à faire,
  voir Phase 7 dans `ARCHITECTURE_AND_ROADMAP.md`.
- **`rover-ai` --- backend écrit et testé (2026-09-09)**, pendant que
  l'utilisateur alimentait le Pi pour la suite du câblage (VL53L0X +
  ESP32-CAM). Voir `ARCHITECTURE_AND_ROADMAP.md` §17.1 pour le détail
  architectural, `pi/README.md` "rover-ai" pour l'usage. Nouveau paquet
  `pi/rover_ai/` :
  - `provider.py` --- interface `AIProvider.ask(message, context) -> str`
    + `AIContext` (system/history) + `AIProviderError`.
  - `cloud.py` --- trois fournisseurs choisis avec l'utilisateur
    (Anthropic, OpenAI, Gemini), chacun son format de requête/réponse.
  - `local.py` --- `LocalAIProvider`, un seul fournisseur générique pour
    n'importe quel serveur compatible API OpenAI sur le réseau local
    (Ollama en référence, §17.1). Qwen 2.5 et un modèle non censuré
    demandés par l'utilisateur passent tous les deux par cette même
    classe --- juste un nom de modèle différent, aucun code par modèle.
  - `credentials.py` --- stockage `pi/ai_credentials.json` (git-ignoré,
    `pi/ai_credentials.example.json` tracké comme gabarit), écriture
    atomique, permissions `0600` posées dès la création (pas de fenêtre
    où le fichier existe en lecture large). Jamais dans `config.json`
    (même règle que `ROVER_CONTROL_TOKEN`).
  - `factory.py` --- `create_provider(credentials)` choisit le
    fournisseur actif ; type inconnu/absent → provider "non configuré"
    (`available=False`, `ask()` lève `AIProviderError` plutôt que de
    planter ou de renvoyer une réponse vide).
  **Testé en isolation** (`pi/tests/test_rover_ai.py`, 6 nouveaux tests,
  58/58 au total sur `pi/`) : session HTTP simulée pour chaque
  fournisseur (requête bien formée, réponse bien parsée, erreur HTTP et
  réponse malformée bien transformées en `AIProviderError`), aller-retour
  du stockage des identifiants, permissions du fichier, clé inconnue
  rejetée, JSON corrompu géré sans crash, sélection par la factory.
  **Rien encore branché dans `RoverCore`/`rover_control`** (pas de
  panneau web, pas d'appel réel avec une vraie clé API) --- prochaine
  étape avec le reste de la Phase 7 (micro/STT/TTS).
  **Suite (même session)** : cinq fournisseurs cloud supplémentaires
  demandés par l'utilisateur --- `QwenCloudProvider` (Qwen officiel via
  DashScope), `OpenRouterProvider`, `TogetherProvider`,
  `FireworksProvider`, `DeepInfraProvider`. Les quatre derniers sont des
  agrégateurs qui hébergent aussi bien Qwen que des modèles
  communautaires non censurés (Dolphin, "abliterated", ...) --- comme
  tous parlent l'API compatible OpenAI, chacun n'est qu'une URL de base
  + un modèle par défaut réutilisant `_OpenAICompatibleProvider`, aucune
  logique de requête nouvelle. Catalogues de modèles notés comme
  mouvants (à vérifier chez chaque fournisseur avant prod) et précision
  ajoutée : même un modèle "non censuré" chez un agrégateur reste sous
  la propre modération de ce service, seule la voie locale
  (`LocalAIProvider`) garantit une absence totale de filtre tiers.
  Confirmé à l'utilisateur : `LocalAIProvider` est déjà générique --- vaut
  pour n'importe quel serveur compatible API OpenAI et n'importe quel nom
  de modèle, aucun code par modèle. 61/61 tests `pi/` (3 nouveaux).
  **Suite (même session) : panneau web branché** --- `rover_control/ai_panel.py`
  (logique) + `rover_control/static/ai.html` (page), routes `/ai`,
  `/ai/config` (GET/POST), `/ai/ask` (POST) ajoutées à `server.py`,
  toutes protégées par le token existant (`auth_middleware` s'applique
  automatiquement à toute nouvelle route). `AIPanel` instancié dans
  `rover_core/main.py`, fermé proprement à l'arrêt. Lien "IA" ajouté en
  haut à droite de la page de pilotage (`index.html`). Un champ clé API
  vide au ré-enregistrement garde la clé déjà stockée (même convention
  que le mot de passe OTA du portail WiFi ESP32) --- **sauf en changeant
  de type de fournisseur** : bug trouvé en écrivant les tests (passer de
  "cloud" à "local" laissait la clé cloud trainer dans le fichier stocké
  alors qu'elle n'est plus utilisée), corrigé avant que ça parte plus
  loin. Testé manuellement de bout en bout avec un serveur aiohttp en
  mémoire (page/config/ask répondent, token invalide → `403`,
  fournisseur injoignable → `503` propre) --- **pas encore essayé sur le
  Pi réel**. 72/72 tests `pi/` (11 nouveaux dans
  `pi/tests/test_ai_panel.py`).
  **Suite (même session) : revue de code sur tout ce qui précède**, 3
  bugs réels trouvés et corrigés :
  1. **Fuite de clé API entre fournisseurs cloud**
     (`rover_control/ai_panel.py`) --- en changeant de fournisseur cloud
     (ex. Anthropic → OpenAI) tout en laissant le champ clé vide, la
     logique "champ vide = garder la clé actuelle" gardait l'ancienne
     clé (Anthropic) et l'enregistrait comme clé du nouveau fournisseur
     (OpenAI) --- le prochain `/ai/ask` aurait envoyé le secret Anthropic
     comme jeton Bearer à `api.openai.com`. Corrigé : la clé n'est
     conservée que si le fournisseur **et** le vendeur cloud restent
     identiques ; sinon le champ vide efface vraiment la clé.
  2. **Erreurs réseau pendant la lecture du corps de la réponse non
     rattrapées** (`rover_ai/_http.py`) --- seule l'ouverture de la
     requête était protégée ; une connexion qui tombe pendant la
     lecture du corps (timeout `sock_read`) ou un JSON malformé sur un
     `200` remontaient une exception `aiohttp`/`json` brute au lieu
     d'`AIProviderError`, cassant le contrat `503` propre attendu par
     `ai_ask_post`. Corrigé : la lecture du corps est protégée aussi.
  3. **`TypeError` non rattrapé sur un `cloud_vendor` non-string**
     (`rover_ai/factory.py`) --- un corps de requête `POST /ai/config`
     malicieux/malformé avec `cloud_vendor` en liste/dict faisait
     planter `dict.get()` (type non hashable) en `500` au lieu du
     comportement "fournisseur inconnu → non configuré" prévu. Corrigé
     avec une garde `isinstance(vendor, str)`.
  6 tests de régression ajoutés (3 dans `test_rover_ai.py`, 1 dans
  `test_ai_panel.py` + 1 test de non-régression pour vérifier qu'une
  vraie nouvelle clé tapée n'est pas perdue au passage). **77/77 tests
  `pi/`.**
- **Caméra --- décision ESP32-CAM (2026-09-06)** : la caméra ne sera
  **pas** rattachée au Raspberry Pi --- pas la place en CAO dans la tête
  pour un module Pi Camera, même déporté par nappe. Un module
  **ESP32-CAM** (déjà en main, avec sa propre caméra intégrée) filme et
  sert le flux en WiFi, indépendamment de l'ESP32 principal ; le Pi
  récupère ce flux sur le réseau et l'analyse (« l'ESP32-CAM filme, le
  Pi analyse »). Voir `ARCHITECTURE_AND_ROADMAP.md` §4.3 (nouvelle
  section) et `BOM.md`.
  **Code écrit (2026-09-06, jour suivant)** --- pas de câblage ce
  jour-là (prévu pour la session d'après), session 100% code :
  - **Firmware `esp32-cam/`** (nouveau projet PlatformIO, séparé de
    `esp32/`, voir §4.3) : board AI-Thinker ESP32-CAM confirmé avec
    l'utilisateur. `src/main.cpp` --- connexion WiFi (mêmes
    `ROVER_WIFI_SSID`/`ROVER_WIFI_PASSWORD` que le firmware principal,
    voir `esp32/OTA.md`), init caméra (`esp_camera_init`, VGA/qualité
    12, PSRAM si détectée sinon repli QVGA une seule frame buffer),
    mDNS (`rovercam.local`), serveur MJPEG (`esp_http_server`, port 81,
    `/stream`, boundary `roverframe`). Aucune authentification sur ce
    flux --- volontaire, réseau local de confiance uniquement (voir
    `esp32-cam/WIRING.md` "Sécurité"), c'est le Pi qui authentifie vers
    l'extérieur. **Compile proprement** (`pio run`, testé sur ce PC de
    dev : RAM 15.2%, Flash 27.5%, avec et sans
    `ROVER_WIFI_SSID`/`PASSWORD` définies comme en CI) --- **jamais
    flashé sur le module physique**, câblage vers un adaptateur
    FTDI/USB-TTL prévu pour la prochaine session (procédure détaillée
    dans `esp32-cam/WIRING.md`, y compris l'avertissement
    sous-alimentation classique de cette carte).
  - **`pi/rover_control/camera.py` réécrit** : ne dépend plus de
    `picamera2`, relaie maintenant (reverse proxy `aiohttp.ClientSession`)
    le flux MJPEG de l'ESP32-CAM à travers `/video`, en reprenant tel
    quel le `Content-Type`/boundary envoyé par l'ESP32-CAM plutôt que de
    ré-encoder les frames. Nouveau réglage `camera_url`
    (`--camera-url` CLI, ou clé `camera.json`/`config.example.json`),
    `None` par défaut (vidéo désactivée, `503`). `CameraStream.close()`
    ajouté et appelé à l'arrêt (`main.py`).
  - **Validé avec un faux serveur MJPEG** (script de fumée, pas de
    matériel ESP32-CAM disponible ce jour) : les trois cas --- pas
    configuré → indisponible, upstream valide → `200` avec le bon
    `Content-Type` et les bons octets relayés, upstream injoignable →
    `503` sans crash --- passent tous. **Toujours pas testé contre le
    vrai module** (pas encore flashé). Suite de tests `pi/` toujours au
    vert (37/37) --- aucune régression, pas de nouveau fichier de test
    committé pour `camera.py` (même choix que l'ancienne version
    `picamera2`, jamais testée non plus --- cohérent avec le reste du
    projet qui ne teste pas la couche serveur web elle-même).
  - CI (`.github/workflows/esp32-build.yml`) étendue : nouveau job
    `build-esp32-cam` (déclenché sur `esp32-cam/**`), en plus de la
    matrice `esp32_wroom`/`esp32_s3` existante.
- **Raspberry Pi --- bring-up bloqué sur le WiFi (2026-09-05)** : début du
  travail Phase 5 (`ARCHITECTURE_AND_ROADMAP.md`, case "OS" toujours
  décochée) --- carte SD 8 Go, Raspberry Pi 3B+ physique en main.
  Architecture d'abord posée avant le code : nouvelle section **§17.1
  "Fournisseur IA (module `rover-ai`)"** ajoutée à
  `ARCHITECTURE_AND_ROADMAP.md` --- conversation IA avec deux
  fournisseurs interchangeables à tout moment (API cloud type
  Gemini/ChatGPT/Claude avec clé utilisateur, ou LLM tournant sur un
  second Raspberry Pi du même réseau local, ex. Qwen 2.5 via Ollama),
  configurable depuis le même portail web que le pilotage
  (`pi/rover_control`), identifiants jamais dans `config.json` (comme
  `ROVER_CONTROL_TOKEN`) --- **conçu, pas encore implémenté**, Phase 7
  mise à jour en conséquence. **Rien de tout ça codé encore** : le
  blocage WiFi ci-dessous a pris toute la session.
  Flashage Raspberry Pi Imager (Raspberry Pi OS Lite 64-bit, hostname
  `rover`, SSH+utilisateur+WiFi configurés via l'écran de
  personnalisation) : le Pi démarre, SSH tourne bien
  (`systemctl status ssh` → active), mais **`wlan0` n'existe jamais**
  (absent de `ip a`, `/sys/class/net/`, `nmcli device status` --- pas
  juste "non connecté", l'interface n'est pas créée par le noyau) et
  `dmesg | grep -i brcm` ne montre **aucune trace** du pilote WiFi
  (`brcmfmac`), pas même une erreur. `rfkill` montrait le WiFi
  soft-blocked (débloqué avec `rfkill unblock wifi` +
  `raspi-config nonint do_wifi_country FR`) --- n'a rien changé, symptôme
  identique après. Hypothèses écartées dans l'ordre :
  1. Pays WiFi non configuré --- redébloqué/reconfiguré, aucun effet.
  2. Carte Pi défectueuse --- testé sur une **deuxième carte 3B+
     physique différente**, exactement le même symptôme.
  3. Cache d'image Raspberry Pi Imager corrompu
     (`%LOCALAPPDATA%\Raspberry Pi\Raspberry Pi Imager\cache\lastdownload.cache`,
     ~525 Mo, supprimé pour forcer un retéléchargement) --- reflash
     complet avec image fraîche, même symptôme.
  4. Image/OS en cause plutôt que le matériel --- testé avec **DietPi**
     (OS totalement différent) à la place de Raspberry Pi OS --- même
     symptôme exact (`wlan0` absent, rien dans `dmesg`).
  **Résolu (2026-09-06)** : confirmé matériel --- puce WiFi onboard
  endommagée sur la première carte 3B+. DietPi installé sur une carte
  physique différente : WiFi fonctionnel directement, aucun contournement
  (dongle USB) nécessaire. Pi retrouvé sur le réseau local via
  `nmap -sn 192.168.1.0/24` --- **`rover.lan` / `192.168.1.187`**
  (hostname `rover` déjà annoncé au routeur), port 22 (SSH) ouvert.
  Résolution `.local` (mDNS) pas testée avec succès directement depuis ce
  PC dev (`avahi-resolve` présent mais pas essayé, `ping rover.local` a
  timeout --- probablement juste `nss-mdns` non configuré côté client,
  pas un problème du Pi). **Configuration logicielle faite (2026-09-06,
  même session)** : connecté en SSH (login système `dietpi`, pas
  `rover` --- `rover` est le mot de passe/hostname, pas l'utilisateur),
  `python3`/`git` installés (absents par défaut sur cette image DietPi
  minimale), dépôt cloné (`~/Rover-Project`, depuis GitHub), venv créé et
  `requirements-dev.txt` installé --- **37/37 tests passent sur le
  matériel réel** (aarch64, Debian 13 trixie, DietPi 10.6.2). Utilisateur
  `dietpi` ajouté au groupe `dialout` par anticipation (nécessaire pour
  `/dev/ttyUSB0` une fois l'ESP32 branché --- pas encore le cas, aucun
  port série présent pour l'instant). `pi/rover.env` créé avec un token
  `ROVER_CONTROL_TOKEN` généré aléatoirement (fichier git-ignored, pas
  dans le dépôt).
  **ESP32 branché en USB au Pi (même session)** : détecté en
  `/dev/ttyUSB0` (CP2102), `dietpi` avait déjà été ajouté au groupe
  `dialout` par anticipation --- accès immédiat sans reconfiguration.
  `rover_core.main` lancé manuellement contre le port réel : trames
  `STATE` bien décodées (IMU/environnement cohérents, distance à `9999`
  --- VL53L0X toujours pas câblés, attendu). **Pilotage validé de bout en
  bout sur le robot réel, en conditions WiFi normales** : page de
  contrôle ouverte depuis un téléphone sur le même réseau
  (`http://192.168.1.187:8080/?token=...`), joystick tactile testé, les
  moteurs répondent. Token d'accès vérifié (`403` sans, `200` avec).
  Petite anomalie cosmétique observée et pas encore creusée : à chaque
  connexion série fraîche, une salve d'environ 1s de warnings `checksum
  mismatch` pour un fragment tronqué identique (`...as_kohm=0.0 *XX`)
  avant que le flux `STATE` ne se stabilise normalement --- reproductible
  à chaque lancement, mais sans impact fonctionnel observé une fois
  stabilisé (le pilotage n'a pas été perturbé). Piste pas creusée :
  `pi/rover_esp32/link.py` (`_LineHandler`/`ReaderThread` de pyserial)
  ou un reset ESP32 déclenché par l'ouverture du port (DTR/RTS).
  **Service systemd installé et activé (même session)** : copie de
  `pi/rover-core.service` adaptée (`User=dietpi`,
  `/home/dietpi/Rover-Project/...` au lieu des placeholders `pi`) dans
  `/etc/systemd/system/`, `enable --now` --- démarre au boot, et
  redémarrage automatique confirmé après un `SIGKILL` forcé (repart en
  moins de 7s, même token, page de contrôle de nouveau joignable).
  Rover tourne maintenant en service permanent sur le Pi.
- **WiFi/OTA (2026-09-05)** : le canal OTA existant (`esp32/lib/ota/RoverOTA.h`,
  identifiants fixés à la compilation via variables d'environnement,
  voir historique 2026-08-31) est complété par un **portail de
  configuration accessible depuis un PC ou un smartphone**, sans
  reflash --- `esp32/lib/network/RoverWifiProvisioning.h` +
  `WifiCredentialsStore.h` (nouveau dossier `esp32/lib/network/`).
  Déclenché à la demande via `SYSTEM action=wifi_setup` (jamais
  automatique au boot, pour ne pas laisser un point d'accès ouvert en
  permanence) : l'ESP32 ouvre son propre point d'accès temporaire
  (`Rover-Setup-XXXX`), sert une page web (formulaire SSID/mot de
  passe/mot de passe OTA, `WebServer`+`DNSServer` du core Arduino ESP32,
  aucune dépendance ajoutée) accessible depuis n'importe quel appareil
  WiFi, enregistre en NVS puis redémarre. `RoverOTA` lit maintenant ces
  identifiants NVS en priorité (fallback sur les variables
  d'environnement existantes si NVS vide --- rien ne casse pour l'usage
  précédent). Actions `wifi_status`/`wifi_forget` ajoutées en
  complément. Voir `esp32/OTA.md` (réécrit pour couvrir les deux
  méthodes) et `ROVER_PROTOCOL.md` §5.1. **Compile sur `esp32_wroom` et
  `esp32_s3`** (929KB/889KB flash, marge suffisante), tests natifs
  toujours au vert (16/16) --- **non testé sur matériel réel** (pas de
  réseau WiFi disponible dans cette session de développement), à
  valider au prochain accès au robot physique : portail effectivement
  joignable depuis un téléphone, formulaire fonctionnel, reconnexion
  après redémarrage, flash OTA réel une fois connecté.
  **Mise à jour (même jour, testé en conditions réelles, COM10)** :
  portail testé de bout en bout avec un vrai téléphone --- le point
  d'accès `Rover-Setup-XXXXXX` n'apparaissait d'abord dans aucun scan
  (ni téléphone ni PC), un cache de scan WiFi périmé (pas un bug
  firmware : `SYSTEM action=wifi_status` confirmait déjà `ap_started=1`
  pendant que rien n'était détecté) --- réapparu après un scan forcé.
  Deux bugs réels trouvés et corrigés pendant ce test :
  1. Buffer `wifi_status` trop court (64 octets) tronquait l'IP en
     silence (`esp32/src/main.cpp`, même classe de bug que les buffers
     ERROR/STATE de la Phase 4) --- passé à 96.
  2. **Connexion WiFi et activation OTA étaient couplées à tort** :
     `RoverOTA::begin()` refusait de rejoindre le réseau du tout tant
     qu'aucun mot de passe OTA n'était enregistré, alors que l'objectif
     explicite était de pouvoir faire les deux étapes séparément.
     Découvert en soumettant le formulaire sans mot de passe OTA
     (`wifi_mode=off` après redémarrage alors que le SSID/mot de passe
     WiFi étaient bien enregistrés). Corrigé : la connexion WiFi ne
     dépend plus que du SSID, seul `ArduinoOTA.begin()` reste
     conditionné au mot de passe OTA (`esp32/lib/ota/RoverOTA.h`,
     nouveaux modes `wifi_mode=wifi` vs `wifi_mode=ota`). Un mot de
     passe WiFi laissé vide dans le formulaire de reconfiguration garde
     désormais la valeur déjà enregistrée (même logique que le mot de
     passe OTA), pour éviter d'écraser le vrai mot de passe en ne
     voulant modifier que l'un des deux champs.
  **Validé sur matériel réel** : `STATE wifi_mode=wifi ip=192.168.1.109`
  obtenu après reflash, robot bien connecté au réseau domestique sans
  mot de passe OTA. **Complété (même session)** : second passage par le
  portail (SSID/mot de passe WiFi laissés vides, seul le mot de passe
  OTA renseigné --- confirme que les champs vides gardent bien la valeur
  déjà enregistrée) → `STATE wifi_mode=ota ip=192.168.1.109` après
  redémarrage, `ArduinoOTA` active.
  **Flash OTA réel testé (même session)** : a d'abord échoué trois fois
  de suite. Deux causes côté PC (pas firmware) : un VPN actif bloquant
  le trafic LAN (kill switch), puis le pare-feu Windows (réseau "Public"
  par défaut) bloquant la connexion TCP entrante initiée par l'ESP32
  vers l'outil de flash --- résolu en désactivant le VPN et en ajoutant
  une règle pare-feu ciblée (IP source = celle du rover uniquement).
  Une fois ça réglé, le transfert démarrait mais échouait systématiquement
  vers ~15% : `ArduinoOTA.handle()` bloque en interne pendant l'écriture
  flash sans jamais rendre la main à `loop()`, ce qui déclenchait le
  watchdog matériel 3s (`Watchdog.h`) en plein transfert --- **vrai bug
  firmware**, corrigé en nourrissant le watchdog depuis
  `ArduinoOTA.onProgress()` (`esp32/lib/ota/RoverOTA.h`). **Flash OTA
  réussi ensuite** (`Result: OK`), redémarrage propre confirmé après
  coup. **Portail de configuration WiFi/OTA entièrement validé de bout
  en bout sur matériel réel**, du premier contact (portail introuvable
  au premier scan WiFi périmé, réapparu après rescan) jusqu'à un vrai
  flash de firmware par WiFi.
- **Capteurs Phase 4 (2026-09-02)** : premiers capteurs réels câblés et
  validés. **MPU6050** : répond à `0x68`, valeurs cohérentes
  (`accel_z≈-10.5`, proche de la gravité ; gyro quasi nul à l'arrêt).
  **BME688 prévu → en réalité un BME280** reçu (chip-id lu `0x60` au lieu
  de `0x61` attendu, confirmé par le lien d'achat) --- pas de capteur de
  gaz sur cette pièce. `EnvironmentSensor` adapté pour utiliser
  `Adafruit_BME280` au lieu de `Adafruit_BME680` (voir
  `esp32/lib/sensors/EnvironmentSensor.{h,cpp}`, `platformio.ini`) ---
  valeurs cohérentes obtenues (25.7°C, 45.4% humidité, 1019.7 hPa),
  `gas_kohm` reste à 0 en permanence (pas de hardware pour ça). Adresse
  renommée `ROVER_ENV_SENSOR_ADDRESS` (était `ROVER_BME688_ADDRESS`,
  trompeur maintenant) dans `sensors_config.h`, fixée à `0x76`. **VL53L0X
  gauche/droite pas encore câblés.**
  Outillage bring-up ajouté au passage, réutilisable pour la suite :
  `SYSTEM action=i2c_scan` (liste toutes les adresses qui répondent sur
  le bus) et `action=bme_chip_id` (lit le registre chip-id brut 0xD0) ---
  utile pour diagnostiquer n'importe quel futur capteur I2C récalcitrant
  sans deviner à l'aveugle.
- **Moteurs** : **mouvement stable et symétrique obtenu (2026-09-02)** ---
  `left_speed`/`right_speed` convergent tous les deux proprement sur la
  cible (0.15) et y restent, testé sur 12s sans oscillation ni dérive.
  Chemin parcouru ce soir (résumé, détail dans `PROGRESS_ARCHIVE.md`) :
  asymétrie initiale entre canaux (`right_speed` très en retard, cf.
  historique canal B DRV8833 faible 2026-09-01, confirmé au multimètre :
  `OUT3`/`OUT4` livre ~1V de moins que `OUT1`/`OUT2` pour un `VCC` stable
  à 6V --- faiblesse électrique réelle mais pas la cause principale du
  comportement erratique) ; fils jaune/vert (`C1`/`C2`) trouvés inversés
  entre les deux moteurs, corrigés ; puis les deux roues sont parties en
  saturation (feedback positif au lieu de négatif --- mesure encodeur qui
  s'aggrave au lieu de converger). **Cause retenue** : sens de comptage
  encodeur opposé au sens réel d'entraînement moteur sur ce matériel, sur
  les deux roues. **Corrigé en firmware** (pas en recâblant encore une
  fois) : `DriveController.cpp`, `tickSign = -1.0f` passé aux deux appels
  `updateWheel()` --- flashé et vérifié sur matériel réel (`esp32_wroom`,
  COM10). À revalider si le câblage encodeur est retouché plus tard (le
  fix logiciel suppose la config actuelle ; repasser `tickSign` à `1.0f`
  si une future correction matérielle du sens rend le flip logiciel
  redondant). À revalider aussi vers 6-7V une fois les connexions soudées
  (actuellement 9V), pour ne pas faire tourner les N20 en surrégime en
  continu --- la faiblesse ~1V du canal B reste présente et à surveiller
  même si elle n'empêche plus un mouvement stable.
- **Encodeurs** : **résolu (2026-09-02)** --- la vraie cause n'était pas
  le rail 3V3 de la breadboard mais un mauvais mapping fil→fonction :
  le silkscreen du PCB encodeur donne Blanc=`M1`/Rouge=`M2` (moteur) et
  Noir=`VCC`/Bleu=`GND` (encodeur), alors qu'on avait câblé en supposant
  Rouge/Noir = fils moteur. Résultat : Blanc (en fait une borne moteur)
  était posé en direct sur le rail 3V3 ESP32, et Noir (en fait le VCC
  encodeur) recevait le PWM moteur pulsé jusqu'à 9V au lieu d'un 3.3V
  stable --- ça perturbait le rail dès que les deux encodeurs étaient
  branchés ensemble. Recâblé selon le marquage PCB (voir `WIRING.md`) :
  **les deux LED s'allument ensemble**. Pas encore vérifié : dommage
  éventuel sur l'encodeur qui a reçu le 9V pulsé, ou sur le régulateur
  3V3 ESP32 --- à surveiller en test. Prochaine étape : confirmer par
  télémétrie (`move_diagnostic.py --move`) que `left_speed`/`right_speed`
  sont comparables une fois en mouvement.
- Breadboard peu fiable sur les manipulations longues (plusieurs faux
  positifs aujourd'hui à cause de contacts qui bougent) --- souder les
  points qui ont posé problème (jumpers GPIO↔driver, VCC/GND/signal
  encodeur) est recommandé avant la prochaine session de bring-up.
- **Roues définitives montées sur ROVER (2026-09-10)** : diamètre réel
  mesuré à **31.83mm**, `ROVER_WHEEL_DIAMETER_M` mis à jour dans
  `esp32/include/motion_config.h` (remplace le placeholder 0.065m posé
  quand seul l'axe moteur nu tournait sans roue). **Flashé sur le
  matériel réel (2026-09-10, COM10, `esp32_wroom`)** : build local OK
  (RAM 16.2%, Flash 71.0%), upload `SUCCESS` en 34s, reset matériel
  automatique. La cible `MOVE` en "m/s" correspond maintenant à la vraie
  géométrie de roue. Test qualitatif au joystick prévu ensuite, robot
  surélevé (roues dans le vide) --- vérifie que les deux roues tournent
  bien, même sens, sans frottement mécanique nouveau ; la calibration
  PID sous charge réelle (item 4 "Prochaines étapes" ci-dessous) reste à
  refaire séparément une fois le robot posé au sol.
- **Premier test roues définitives, robot surélevé (2026-09-10)** :
  session interrompue en cours de diagnostic, à reprendre.
  - **Faux problème trouvé et résolu en cours de route** : à la mise sous
    tension, bip continu dès l'alimentation (avant toute commande
    joystick) + moteur droit ne répondant pas du tout. Diagnostic par
    `sudo journalctl -u rover-core -n 80` : trames `STATE` parfaitement
    normales en continu (`left_speed`/`right_speed` à `0.00`, pas de
    reset, aucun `EVENT estop_pressed`) --- écarte un bug firmware
    (reset en boucle par brownout, buzzer déclenché par le code,
    E-stop électriquement bruité). Pointait donc vers un problème
    physique côté câblage plutôt que logiciel. **Résolu en rebranchant**
    (mauvais contact --- cohérent avec l'avertissement du 2026-09-01/02
    jamais traité : jumpers GPIO↔driver et connexions moteur/encodeur
    toujours pas soudés, voir "Prochaines étapes" ci-dessous). Pas de
    cause précise isolée (quel connecteur exactement) --- si ça revient,
    regarder en premier le canal B du DRV8833 (moteur droit, déjà connu
    plus faible électriquement) et son câblage vers l'ESP32.
  - **Nouveau point bloquant, plus important** : une fois le contact
    retrouvé, **les deux moteurs tournent mais ne sont pas synchronisés**
    (pas de mesure chiffrée prise avant l'arrêt de session). Testé roues
    dans le vide (surélevé), donc ce n'est pas un problème de charge
    sol/friction asymétrique --- pointe vers PID/calibration par roue,
    ou un résidu du canal B faible, ou un effet du nouveau diamètre de
    roue (31.83mm, flashé cette session, jamais testé en mouvement réel
    avant cette coupure).
  - **Suite (même jour, reprise de session) : bug logiciel réel trouvé et
    corrigé en cours de diagnostic** --- `pi/rover_esp32/protocol.py`,
    `checksum()` faisait `content.encode("ascii")` sans filet ; une trame
    bruitée (bruit série connu au boot, cf. anomalie cosmétique du
    2026-09-06) contenant des octets non-ASCII levait `UnicodeEncodeError`
    au lieu de `FrameError`, ce qui échappait au `except FrameError` de
    `link.py` et **tuait tout le thread de lecture pyserial en silence**
    --- explique pourquoi le premier essai de diagnostic isolé
    (`move_diagnostic.py`) ne recevait plus aucune trame ensuite. Corrigé
    (converti en `FrameError`, donc juste droppé avec un warning comme
    n'importe quelle autre trame malformée) ; 2 tests de régression
    ajoutés (`pi/tests/test_protocol.py`).
  - **Cause réelle de la désynchronisation identifiée sur matériel réel**
    (ESP32 débranché du Pi, branché en direct sur COM10 pour un accès
    exclusif et rapide, `move_diagnostic.py --move`) : un emballement en
    boucle positive (mesure encodeur de signe opposé à la direction
    moteur réelle, PWM qui sature au lieu de converger --- même famille de
    bug que le `tickSign` du 2026-09-02), mais cette fois **seulement sur
    la roue droite**, apparu après un rebranchement en cours de session.
    `ROVER_TICK_SIGN_LEFT`/`ROVER_TICK_SIGN_RIGHT` séparés en deux
    constantes (`esp32/include/motion_config.h`, étaient une seule valeur
    partagée) pour pouvoir corriger un côté sans toucher à l'autre.
    Diagnostic non-trivial, deux fausses pistes en cours de route (les
    deux vérifiées sur matériel réel, pas juste en théorie) :
    1. Hypothèse initiale (signe droit inversé tout seul) testée par OTA
       --- emballement toujours présent, juste dans l'autre sens : pas la
       bonne piste, signe droit reremis à `-1.0f` (valeur d'origine).
    2. **L'utilisateur a ressoudé les fils de puissance moteur (rouge/
       blanc) au driver en cours de session**, ce qui a inversé la
       polarité du moteur **gauche** cette fois (jusque-là correct). Une
       première tentative de correction a inversé à la fois le mapping
       GPIO IN1/IN2 gauche **et** `ROVER_TICK_SIGN_LEFT` --- erreur de
       raisonnement corrigée en cours de route : inverser les deux en même
       temps s'annule mathématiquement (confirmé par une mesure
       identique au bit près à ne rien changer). Un seul des deux doit
       être inversé à la fois. Correctif final : `ROVER_PIN_MOTOR_L_IN1`/
       `IN2` laissés inchangés, seul `ROVER_TICK_SIGN_LEFT` passé à
       `1.0f`.
    **Validé à la fois par télémétrie et visuellement par l'utilisateur**
    (sens de rotation confirmé correct à l'œil sur les deux roues,
    `move_diagnostic.py --velocity 0.15 --duration 15` : `left_speed`/
    `right_speed` convergent tous les deux vers ~0.15-0.16, écart résiduel
    qui se résorbe avec le temps grâce au terme intégral --- pas un vrai
    désaccord). **Leçon retenue pour la suite** (documentée dans le
    commentaire de `motion_config.h`) : ne pas faire confiance aux valeurs
    de tickSign d'une session à l'autre dès que le câblage moteur/encodeur
    est retouché --- revérifier avec `move_diagnostic.py` (convergence
    propre = bon signe ; saturation à la vitesse plafond = mauvais signe)
    plutôt que de supposer que la dernière valeur connue tient encore.
  - **Effet de bord découvert au rebranchement ESP32↔Pi** : `rover-core`
    plantait en boucle (`SerialException: write failed: [Errno 5] Input/
    output error`) --- le débranchement/rebranchement du câble USB a fait
    réapparaître l'ESP32 en `/dev/ttyUSB1` au lieu de `/dev/ttyUSB0`
    (renumérotation classique côté noyau Linux), alors que le service
    systemd déployé pointait encore sur l'ancien chemin. Corrigé
    durablement plutôt que juste renseigner `ttyUSB1` en dur (qui aurait
    pu re-changer au prochain branchement) : `ExecStart` du service pointe
    maintenant sur le lien stable `/dev/serial/by-id/usb-Silicon_Labs_
    CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0`, qui ne bouge
    pas d'un branchement à l'autre sur le même port physique. Gabarit
    `pi/rover-core.service` mis à jour avec un commentaire expliquant
    pourquoi préférer `by-id` à `/dev/ttyUSBn`, pour que ça ne piège pas
    une future réinstallation.
  - **Point restant, pour la prochaine session** : au pilotage joystick
    réel (une fois tout rebranché), la roue droite démarre avec **~1s de
    retard** par rapport à la gauche et les vitesses ne sont pas
    identiques tout de suite. Cohérent avec une limitation déjà connue et
    caractérisée (2026-09-02) plutôt qu'un nouveau bug de câblage : le
    canal B du DRV8833 (droit) est électriquement plus faible depuis le
    tout début du bring-up, et le terme intégral du PID met du temps à
    vaincre le frottement statique à basse vitesse sur ce canal --- voir
    "Calibration PID" ci-dessous, qui documente déjà ce comportement et
    propose un terme feedforward comme piste. **Décision prise avec
    l'utilisateur : ne pas retoucher le PID ce soir** (câblage pas encore
    soudé partout, tension pas encore validée à 6-7V --- retoucher les
    gains maintenant risquerait de re-régler sur une config pas
    définitive). À reprendre en priorité la prochaine session, avec le
    reste de la calibration PID/géométrie déjà prévue au point 4 de
    "Prochaines étapes".
- **Calibration PID (2026-09-02)** : géométrie roue mesurée en partie ---
  `ROVER_ENCODER_TICKS_PER_REV` mis à jour à 1073 (mesuré via le nouveau
  `SYSTEM action=raw_ticks`/`reset_ticks`, 1 tour compté à l'œil pendant
  une rotation lente motorisée --- pas de rotation à la main possible,
  le réducteur N20 est trop dur). `ROVER_WHEEL_DIAMETER_M` **reste un
  placeholder** : pas de roue montée, la CAO n'est pas finalisée, seul
  l'axe moteur nu (6.82mm, mesuré) tourne pour l'instant --- donc les
  cibles `MOVE` en "m/s" ne correspondent pas encore à une vitesse
  réelle, et tout ce qui suit a été testé **sans charge mécanique**
  (pas de frottement sol, pas d'inertie de roue réelle).
  Caractérisation des gains par défaut (180/300/0) sur la plage de
  cibles : **0.05 → ne bouge jamais** (frottement statique jamais vaincu,
  le terme P seul est trop faible et l'intégral met ~20s à saturer à
  cette erreur) ; **0.15 → converge en ~2-3s avec un léger dépassement à
  0.20 puis stable** ; **0.25 → montée douce en ~5-6s sans dépassement,
  stable ~0.24-0.25**. Gains gardés tels quels (pas de changement dans
  `motion_config.h`) --- bonne base sur la plage utile, mais **la vraie
  calibration (géométrie + gains) sera à refaire une fois une roue
  réelle montée** (dynamique moteur différente sous charge, le point de
  décrochage en basse vitesse va changer). Correctif possible pour la
  limite basse vitesse si besoin plus tard : terme feedforward (PWM
  minimum ajouté quand la cible n'est pas nulle) plutôt que remonter
  `Ki` (risque de dépassement ailleurs sur la plage).

- **Nouveaux moteurs N20 définitifs reçus, suite : PWM télémétrie,
  intermittence toujours pas résolue (2026-09-11)** : plusieurs essais
  identiques (même commande, même firmware) donnant tantôt
  `left_speed=0.00` tantôt `0.04`/`0.05` sans changement physique
  rapporté entre les essais --- preuve d'un contact intermittent (le
  firmware ne peut pas produire cette variance tout seul avec une
  commande strictement identique), pas un moteur/canal mort. **Correction
  : aucune soudure n'a été faite** --- l'essai qui a donné `0.05` stable
  était juste une coïncidence de contact favorable ce lancement-là, pas
  une réparation. Le résidu peut revenir à `0.00` à tout moment tant que
  rien n'a été physiquement changé côté câblage gauche. Ajout (permanent,
  utile au-delà de cette session) d'une télémétrie `left_pwm=`/
  `right_pwm=` dans `STATE` (`DriveController.{h,cpp}`, buffer `main.cpp`
  élargi à 96 octets) confirmant que le canal gauche pousse déjà à
  `255/255` (pleine puissance) dès qu'il produit du mouvement, contre
  `~220-229/255` côté droit pour une vitesse proche de la cible --- donc
  même dans son meilleur cas observé, la gauche reste ~3x plus faible que
  la droite à duty égal ou supérieur. Compile sur `esp32_wroom`/
  `esp32_s3`, flashé et validé sur matériel réel (COM10). **Toujours pas
  résolu** : souder les connexions côté gauche (jumpers GPIO↔driver,
  moteur↔driver, jusque-là sur breadboard) reste à faire pour de vrai.
  Une fois fait, tester si le résidu de faiblesse (~0.05 vs 0.15 à pleine
  puissance) persiste malgré un contact fiable --- si oui, tester le
  moteur gauche seul, débranché de la roue, à vide sous 6V, pour
  distinguer moteur intrinsèquement faible/défectueux d'un frottement
  mécanique côté accouplement roue/axe.
- **Nouveaux moteurs N20 définitifs reçus, premier test (2026-09-11)** :
  ESP32 branché en direct sur COM10 (comme le 2026-09-10), test
  `move_diagnostic.py --move --velocity 0.15 --duration 12` roues en
  l'air. **Roue droite saine** : `right_speed` converge normalement
  (~0.14-0.17), `raw_ticks_right=-2726` sur 5s. **Roue gauche muette** :
  `left_speed` reste à `0.00` tout du long malgré le PID qui pousse à
  fond. Écarté un encodeur totalement mort par un test `raw_ticks`
  dédié : `raw_ticks_left=117` sur les mêmes 5s (donc pas zéro) mais
  ~23x moins que la droite. Observation physique utilisateur pendant le
  test : moteur gauche **immobile et silencieux** (pas de bruit de
  blocage/effort) --- les 117 ticks résiduels sont donc probablement du
  bruit électrique/vibration parasite plutôt qu'un vrai mouvement, pas
  contradictoire avec un moteur qui ne reçoit aucun courant utile.
  **Diagnostic retenu : mauvais contact électrique côté moteur gauche
  (canal A DRV8833, `ROVER_PIN_MOTOR_L_IN1`=GPIO27/`IN2`=GPIO26)**, pas
  un bug logiciel --- le firmware n'a pas changé, seuls les moteurs ont
  été remplacés, et ce pattern (une roue muette, l'autre saine) est déjà
  celui vu le 2026-09-10 (moteur droit muet ce jour-là, résolu par un
  simple rebranchement) --- cohérent avec les connexions moteur/driver
  toujours pas soudées (voir "Prochaines étapes" point 2, non fait).
  **Pas encore vérifié physiquement** (continuité des fils du nouveau
  moteur gauche jusqu'au canal A, ou test du moteur isolé hors driver) ---
  prochaine étape avant de relancer un test de convergence complet.
  **Retesté deux fois de plus (même session, sans intervention physique
  entre les essais)** : symptôme identique à chaque fois, avec une
  nuance --- `left_speed` produit un très bref sursaut (0.01-0.04) au
  tout début de la commande `MOVE` avant de retomber à `0.00` et d'y
  rester pour le reste du test (12s). Cohérent avec un **contact
  intermittent** plutôt qu'un circuit totalement coupé (un peu de
  courant passe à la montée du PWM puis le contact se perd) --- pas
  contradictoire avec le diagnostic "mauvais contact électrique"
  ci-dessus, précise juste la nature (intermittent, pas franc).
  Test décisif toujours en attente : échanger les fils moteur gauche/
  droite sur le driver pour savoir si le défaut suit le moteur ou reste
  sur le canal A --- pas encore fait à ce stade.
  **Rebond (même session) : alimentation moteurs mesurée à 6V en sortie
  côté gauche, confirmée présente par l'utilisateur.** Écarte
  l'hypothèse "aucun courant n'arrive" (fil coupé) retenue plus haut ---
  symptôme identique reconfirmé une 5e fois avec la tension présente
  (`left_speed` sursaute à 0.01-0.02 puis retombe à `0.00` et y reste,
  `right_speed` toujours sain à ~0.16-0.17). **Nouvelles hypothèses
  principales** : (a) moteur gauche mécaniquement grippé/bloqué
  (réducteur), le sursaut correspondant au tout petit mouvement avant
  blocage --- à vérifier en tournant la roue à la main hors tension ; (b)
  protection de surcourant du DRV8833 qui verrouille le canal A dès le
  premier pic de courant au démarrage moteur, cohérent avec un plateau
  identique à chaque essai (chaque test relance l'ESP32 par reset série,
  donc redonne "une seule chance" avant verrouillage). **Test décisif
  toujours en attente** : échange physique des deux moteurs entre les
  canaux du driver, jamais réellement effectué jusqu'ici (seuls des
  rebranchements au même endroit ont été tentés).
- **Suite (même session) : nouveau driver DRV8833 installé, panne totale
  puis résidu inversé (2026-09-11)** : après remplacement du driver
  (l'utilisateur affirme qu'aucune soudure n'avait encore été faite au
  moment du point précédent), premier retest --- **plus aucune roue ne
  bouge**, `left_speed`/`right_speed` à `0.00` et `left_pwm`/`right_pwm`
  saturés à `255/255` des deux côtés. Diagnostiqué via `WIRING.md` ligne
  49 (point bloquant déjà documenté depuis 2026-09-01 sur ce type de
  breakout DRV8833) : SLEEP non câblé au 3V3 désactive le driver en
  permanence, 0A quoi qu'envoient IN1-4. **Confirmé par l'utilisateur :
  SLEEP bien sur le 3V3**, donc pas la cause ici. Point suivant vérifié
  (VM moteur + masse commune ESP32↔nouveau driver) --- après vérification,
  **les deux roues bougent de nouveau**, mais `left_speed` ressort
  **négatif et stable** (~-0.04 à -0.10) alors que la cible est positive
  (+0.15) et que `left_pwm` reste saturé à `255` sans jamais converger ---
  signature d'un problème de sens (moteur ou encodeur), pas d'absence de
  courant. **Deux tests à une seule variable tentés à tour de rôle**
  (jamais les deux en même temps, cf. leçon 2026-09-10 ci-dessus) :
  1. Échange `ROVER_PIN_MOTOR_L_IN1`/`IN2` (26↔27, compile+flash sur
     matériel réel COM10) --- signe resté négatif, écarte une inversion
     de polarité moteur comme seule explication.
  2. Reverti, puis `ROVER_TICK_SIGN_LEFT` passé à `-1.0f` seul (compile+
     flash) --- signe resté négatif également, résultat quasi identique
     au test précédent (`-0.04` dans les deux cas). Écarte aussi un
     simple problème de signe d'encodeur isolé.
  **Aucun des deux ne corrige le signe** --- conclusion retenue : le
  résidu négatif n'est probablement pas un vrai problème de convention de
  signe mais plutôt du bruit mécanique (vibration du châssis pendant que
  la droite tourne fort, jeu de réducteur) lu comme un mouvement, cohérent
  avec le fait que `left_pwm` ne cesse jamais de saturer (la boucle ne
  voit jamais de convergence réelle). **Firmware remis à l'état d'origine**
  (`ROVER_PIN_MOTOR_L_IN1=27`/`IN2=26`, `ROVER_TICK_SIGN_LEFT=1.0f`,
  aucune des deux valeurs n'étant confirmée meilleure), recompilé et
  reflashé sur COM10 avant la fin de session. **Session arrêtée ici** ---
  le test qui reste le plus informant, jamais fait malgré plusieurs
  relances : moteur gauche débranché de la roue, testé seul à vide sous
  6V, pour savoir si le moteur lui-même est capable de tourner
  normalement une fois isolé de tout ce bruit/confusion de signe.

## Prochaines étapes

**PRIORITÉ ABSOLUE pour la prochaine session** : moteur gauche toujours
problématique avec les nouveaux moteurs + nouveau driver (2026-09-11, voir
"État actuel" ci-dessus) --- saga de la session : muet, puis intermittent,
puis (après remplacement du driver) totalement mort (SLEEP/VM/masse
vérifiés OK), puis de nouveau mobile mais avec un `left_speed` négatif et
un PWM qui sature en permanence sans jamais converger. Deux tentatives de
correction de signe (IN1/IN2 puis tick sign, une seule variable à la fois)
n'ont rien changé --- le firmware a été remis à son état d'origine, aucune
des deux hypothèses de signe n'étant confirmée. **Premier test à faire,
jamais réalisé malgré plusieurs relances** : moteur gauche débranché de la
roue, testé seul à vide sous 6V --- tourne-t-il librement et vite comme le
droit ? Coupe court à toute la confusion de signe/bruit en isolant le
moteur de l'encodeur et du reste du bruit mécanique. Une fois ça clarifié,
vérifier la continuité électrique jusqu'au canal A du nouveau driver, puis
relancer `move_diagnostic.py --move --velocity 0.15` pour reconfirmer la
convergence des deux côtés avant de reprendre le reste ci-dessous.

**Point suivant, déjà en attente depuis le 2026-09-10** : la désynchronisation
dangereuse (emballement/sens inversé) trouvée le 2026-09-10 est
**résolue et validée sur matériel réel** (voir "État actuel" ci-dessus)
--- mais il reste un résidu de réglage fin, volontairement pas traité ce
soir-là :
- Au pilotage joystick réel, la roue droite (canal B DRV8833, déjà connu
  plus faible électriquement) démarre avec **~1s de retard** et les
  vitesses ne s'égalisent pas tout de suite. Cohérent avec la limitation
  PID basse-vitesse déjà caractérisée le 2026-09-02 (voir "Calibration
  PID" ci-dessous), pas un nouveau bug de câblage.
- Ne pas retoucher les gains PID avant d'avoir fait les points 2 et 3
  ci-dessous (souder les connexions encore volantes, revalider la tension
  à 6-7V) --- retoucher maintenant risquerait de re-régler sur une config
  pas définitive. Une fois ça fait, reprendre la calibration complète
  (point 4), en envisageant le terme feedforward déjà proposé pour la
  limite basse vitesse.
- Rappel méthode (leçon de la session du 2026-09-10) : après tout
  rebranchement/ressoudage d'un moteur ou d'un encodeur, revérifier le
  sens avec `move_diagnostic.py` avant de faire confiance aux constantes
  `ROVER_TICK_SIGN_LEFT`/`RIGHT` de la session précédente --- une
  convergence propre vers la cible = bon signe, une saturation à la
  vitesse plafond = mauvais signe.

0. **Bring-up Pi terminé** (2026-09-06) : ESP32 branché, pilotage validé
   de bout en bout sur le robot réel, service systemd installé/activé
   (démarrage auto + redémarrage sur crash confirmé). Voir ci-dessus.
   Tout le travail `rover-ai` (voir §17.1) est débloqué mais pas
   commencé --- prochaine grosse brique côté Pi.
1. Vérifier que rien n'a été endommagé par l'ancien mauvais câblage
   (encodeur exposé au PWM 9V sur son VCC, régulateur 3V3 ESP32 avec une
   borne moteur en direct dessus) --- pas de symptôme attendu si tout est
   sain, mais à garder en tête si un comportement bizarre réapparaît.
2. Souder les connexions instables identifiées le 2026-09-01 (jumpers
   GPIO↔driver, VCC/GND/signal encodeur) --- d'autant plus important
   maintenant que le comportement correct dépend d'une config de câblage
   précise (voir note `tickSign` ci-dessus, à ne pas perturber en soudant
   sans y repenser).
3. Revalider à une tension proche du nominal (6-7V) une fois soudé.
4. Calibrer PID + géométrie roue --- **première passe faite (2026-09-02,
   voir ci-dessus)**, ticks/tour mesuré, gains caractérisés sur la plage
   utile. À refaire une fois une roue réelle montée (diamètre + retuning
   sous charge).
5. Commande AliExpress en cours (nouveaux moteurs N20 --- pas strictement
   nécessaire, les actuels ne sont pas défectueux --- + système
   d'alimentation : 2×18650 en série (7.4V), BMS 2S avec charge USB-C
   intégrée, buck converters servos/logique). Reste à trancher : le
   Raspberry Pi tournera-t-il sur ce même pack ou une alim séparée ---
   ça dimensionne le BMS/buck à choisir.
6. Servos tête : câblage signal documenté (`WIRING.md`/`head_config.h`),
   mais pas encore testés sur matériel réel, et leur alimentation
   (partager le rail moteurs, pas l'ESP32) reste à ajouter à `WIRING.md`.
7. Capteurs Phase 4 : MPU6050 et BME280 câblés et validés (2026-09-02,
   voir ci-dessus) --- **VL53L0X gauche/droite restent à câbler, prévu
   pour la prochaine session.** Câblage à faire (les deux **ensemble**,
   pas un par un --- le firmware réadresse le gauche via `XSHUT` au boot,
   il attend les deux) :
   - `VCC`/`GND` des deux → 3V3/GND commun.
   - `SDA`/`SCL` des deux → GPIO21/GPIO22 (bus I2C partagé avec
     MPU6050/BME280, déjà câblés).
   - `XSHUT` gauche → GPIO0 ; `XSHUT` droite → GPIO4.
   - Placement physique décidé : **côte à côte en façade avant** (pas
     un devant/un derrière --- le code `distance_left`/`distance_right`
     et le réflexe d'arrêt obstacle du Pi supposent les deux tournés vers
     l'avant).
   - Une fois câblés : `SYSTEM action=i2c_scan` pour vérifier qu'ils
     répondent (`0x29` avant réadressage), puis `move_diagnostic.py`
     pour voir `distance_left`/`distance_right` en tandem avec
     `SYSTEM action=bme_chip_id`-style outillage déjà en place si un
     capteur pose souci (voir outillage ajouté ce soir : `i2c_scan`
     réutilisable directement).
8. Reste identique par ailleurs (voir `ARCHITECTURE_AND_ROADMAP.md`) :
   Raspberry Pi physique, caméra, MQTT contre un vrai broker, buzzer
   audible, VPN en conditions réelles.
9. Portail WiFi/OTA (2026-09-05, voir ci-dessus) : **entièrement validé
   de bout en bout sur matériel réel, flash OTA réel inclus**. Rien de
   plus à faire ici pour l'instant.

**Chantier « Pi déporté » (décidé le 2026-09-15, rien d'implémenté).**
Ne pas démarrer avant d'avoir réglé le moteur gauche ci-dessus --- mais
l'ordre interne de ce chantier compte, parce qu'il est conçu pour
échouer tôt et pas cher :

0. **Deux prérequis bloquants, trouvés à la revue de code du
   2026-09-15** (détail : `ARCHITECTURE_AND_ROADMAP.md` §6.2, questions
   « 5 bis » et « 5 ter »). À traiter **avant** de débrancher l'USB,
   parce qu'aujourd'hui le câble *est* la sécurité :
   - **Authentifier le lien.** Le Rover Protocol n'a aucune notion
     d'identité. Une socket ouverte = n'importe qui sur le WiFi pilote
     les moteurs. Le serveur web exige un token et l'OTA un mot de
     passe ; le lien de commande serait le seul canal ouvert, et le plus
     dangereux. Secret en NVS (`WifiCredentialsStore`), trame d'auth
     obligatoire avant de quitter `READY`, une seule connexion à la fois.
   - **Sortir le WiFi de `RoverOTA`.** C'est aujourd'hui le seul endroit
     qui connecte la radio, dans un module explicitement optionnel --- le
     lien de commande ne peut pas en dépendre.

1. **Transport socket d'abord, mécanique ensuite.** Côté ESP32,
   `WiFiServer` → `WiFiClient` passé à `RoverProtocol` (qui prend déjà
   un `Stream&`), avec repli sur `Serial` tant que le câble existe
   encore. Côté Pi, **plus rien à écrire** : `RoverLink` supervise déjà
   la connexion et reconnecte (fait le 2026-09-15), il ne reste qu'à
   mettre `socket://host:port` dans `config.json`. Garder le Pi
   physiquement sur le robot à ce stade --- on ne teste qu'une chose à la
   fois. ⚠ Penser à `WiFi.setSleep(false)` dès ce premier jet : sans ça
   la mesure de l'étape 2 mesurerait l'économie d'énergie radio, pas le
   lien.
2. **Mesurer avant de régler.** Gigue et taux de perte du lien WiFi,
   robot en mouvement et à distance du point d'accès. C'est cette mesure
   qui fixe les seuils de §9 --- les 500 ms/1500 ms écrits aujourd'hui
   sont une hypothèse, pas un résultat. Tant que ce n'est pas mesuré, ne
   pas toucher à `ROVER_HEARTBEAT_TIMEOUT_MS`
   (`esp32/include/board_config.h`).
3. **Durcir** : paliers de heartbeat, `WiFi.setSleep(false)`, échéance
   propre à chaque `MOVE` (« vitesse X pendant au plus N ms »).
4. **Seulement là**, retirer physiquement le Pi et adapter la CAO
   (emplacement du cavalier `GPIO3` accessible sans démontage).
5. **Audio en dernier**, une fois le lien éprouvé : micro INMP441 sur
   `GPIO3`, variante binaire/streaming de `POST /audio/converse` côté
   Pi. Ne pas tenter de bufferiser un énoncé complet sur le WROOM.

------------------------------------------------------------------------

## Journal court (une ligne par session --- détail complet dans PROGRESS_ARCHIVE.md)

- **2026-09-15 (3)** --- **Pilotage autonome sans Pi ni réseau**
  (`StandaloneControl.h`, §6.3) : l'ESP32 ouvre son propre AP WPA2 et
  sert une page joystick. Rattrape le seul vrai point faible du Pi
  déporté. Aucune logique de sécurité dupliquée : la page bat le même
  heartbeat que le Pi. +8 Ko de flash. Jamais testé sur matériel.
- **2026-09-15 (2)** --- Revue de code complète du dépôt. Constat
  principal : §6.2 affirmait à tort que le réflexe d'obstacle était déjà
  local à l'ESP32 (les capteurs l'étaient, pas le réflexe) --- corrigé
  en firmware *et* dans la doc. 8 corrections au total (réflexe local,
  troncature de trame indétectable, verrouillage WiFi, injection HTML du
  portail, boucle infinie caméra, ordre fermeture/sauvegarde des
  panneaux, tâches WebSocket, docstring trompeuse). 2 prérequis
  bloquants documentés pour le Pi déporté : **aucune authentification**
  sur le lien, et WiFi enfermé dans `RoverOTA`.

- **2026-09-15** --- Décision d'architecture : **Raspberry Pi déporté sur
  le réseau** (socket TCP WiFi au lieu du câble USB), **carte WROOM
  confirmée** (S3 gardé pour plus tard). Motivée par l'encombrement et
  l'autonomie, validée par compilation réelle (Flash 71 %, RAM 16 %).
  Trois conséquences traitées : heartbeat à paliers (§9), audio en
  streaming obligatoire (§17.3), et déblocage inattendu de `GPIO3` pour
  le micro (cavalier prévu). Côté code, une seule correction mais réelle :
  `RoverLink` ne survivait pas à une coupure réseau (reconnexion, `send()`
  qui ne lève plus, course `stop()` fermée) --- 9 nouveaux tests, 145 au
  total. Firmware ESP32 non modifié.

- **2026-09-11** --- Session bring-up des moteurs N20 définitifs, non
  résolue. Résumé : muet (contact intermittent, jamais soudé malgré une
  fausse annonce en cours de session) → nouveau driver DRV8833 installé →
  panne totale (SLEEP/VM/masse vérifiés OK, cause non identifiée) → de
  nouveau mobile mais `left_speed` négatif avec PWM saturé en permanence.
  Deux corrections de signe testées séparément (IN1/IN2, puis tick sign)
  sans effet sur le résultat --- probablement du bruit mécanique plutôt
  qu'un vrai problème de signe, vu que le PWM ne converge jamais. Firmware
  remis à l'état d'origine. Ajout permanent utile : télémétrie
  `left_pwm=`/`right_pwm=` dans `STATE` (`DriveController`, `main.cpp`) ---
  a permis de prouver que le firmware pousse bien à pleine puissance sans
  bug logiciel, isolant le problème côté matériel. Session arrêtée avant
  le test le plus informant (moteur gauche seul, à vide, hors roue) ---
  voir "Prochaines étapes".
- **2026-09-11 (premier test, détail)** --- Nouveaux moteurs N20 définitifs reçus, premier test
  (`move_diagnostic.py --move`, ESP32 en direct sur COM10, roues en
  l'air) : roue droite saine (convergence PID normale), **roue gauche
  muette** (immobile et silencieuse à l'observation, PID poussant à
  fond sans effet) --- écarté un encodeur mort via `raw_ticks` (117
  ticks non-nuls mais ~23x moins que la droite, probablement du bruit
  parasite). Diagnostic retenu : mauvais contact électrique côté moteur
  gauche plutôt qu'un bug logiciel (firmware inchangé, même pattern
  qu'un incident similaire déjà vu le 2026-09-10 sur l'autre roue,
  résolu par rebranchement). Vérification physique du câblage à faire
  en priorité la prochaine session.
- **2026-09-10** --- Roues définitives montées sur ROVER, diamètre mesuré
  (31.83mm), `ROVER_WHEEL_DIAMETER_M` mis à jour et flashé sur matériel
  réel (COM10). Premier test au joystick robot surélevé : faux problème
  (bip continu + moteur droit muet dès l'alimentation) diagnostiqué via
  les logs `rover-core` (télémétrie parfaitement normale, donc pas un bug
  firmware) puis résolu par un simple rebranchement --- mauvais contact,
  cohérent avec les connexions jamais soudées. **Point bloquant restant,
  prioritaire pour la suite : les deux moteurs tournent mais ne sont pas
  synchronisés**, même roues dans le vide. Session interrompue ici avant
  d'avoir pu mesurer/diagnostiquer plus loin --- voir "Prochaines étapes"
  en tête de fichier.
- **2026-09-10 (reprise même jour)** --- Désynchronisation diagnostiquée
  et corrigée. Bug logiciel réel trouvé en route : `checksum()`
  (`pi/rover_esp32/protocol.py`) plantait tout le thread de lecture
  série sur une trame bruitée non-ASCII au lieu de la dropper --- corrigé
  (`FrameError`), 2 tests de régression ajoutés. Cause de la
  désynchronisation isolée sur matériel réel (ESP32 débranché du Pi,
  branché en direct sur COM10) : emballement en boucle positive sur la
  roue droite seule (signe encodeur, même famille que le bug `tickSign`
  du 2026-09-02), compliqué par un ressoudage des fils moteur en cours de
  session qui a inversé la polarité de la roue gauche entre-temps.
  `ROVER_TICK_SIGN_LEFT`/`RIGHT` séparés en deux constantes
  (`motion_config.h`) et calibrés empiriquement par mesure réelle
  (`move_diagnostic.py`) + confirmation visuelle utilisateur du sens de
  rotation --- une fausse piste en route (inverser broche moteur ET signe
  encodeur ensemble s'annule mathématiquement, leçon notée dans le code).
  Effet de bord trouvé au rebranchement ESP32↔Pi : `rover-core` plantait
  en boucle sur `/dev/ttyUSB0` devenu `/dev/ttyUSB1` après le
  débranchement --- corrigé durablement via le lien stable
  `/dev/serial/by-id/...` (gabarit `pi/rover-core.service` mis à jour).
  **Point bloquant dangereux résolu et validé.** Résidu restant (roue
  droite ~1s de retard au démarrage, vitesses pas identiques tout de
  suite) identifié comme la limitation PID basse-vitesse déjà connue
  (2026-09-02, canal B faible) --- laissé pour la prochaine session
  plutôt que retouché sur un câblage pas encore définitif (jumpers pas
  soudés, tension pas encore à 6-7V). Voir "Prochaines étapes".
- **2026-09-09** --- Backend `rover-ai` écrit et testé pendant que
  l'ESP32-CAM/VL53L0X étaient préparés pour câblage : interface
  `AIProvider` commune, trois fournisseurs cloud (Anthropic/OpenAI/
  Gemini), un fournisseur local générique compatible OpenAI (Qwen 2.5 ou
  modèle non censuré = juste un nom de modèle, même classe), stockage
  des identifiants git-ignoré en `0600`, factory de sélection. 58/58
  tests `pi/` passent (6 nouveaux). Rien encore branché dans
  `RoverCore` ni de panneau web --- backend seul, voir §17.1.
- **2026-09-06** --- Blocage WiFi résolu : puce onboard endommagée sur la
  1ère carte 3B+, DietPi sur une carte différente fonctionne directement.
  Pi retrouvé sur le réseau (`rover.lan`, 192.168.1.187, SSH ouvert),
  environnement logiciel mis en place (python3/git installés, dépôt
  cloné, venv, 37/37 tests passent sur le matériel réel, token de
  contrôle généré). ESP32 principal branché en USB au Pi (même
  session) : **pilotage complet validé de bout en bout sur le robot
  réel** --- joystick tactile depuis un téléphone → Pi (WiFi) → ESP32 →
  moteurs, token d'authentification vérifié. Service systemd installé et
  activé (`rover-core`, démarrage auto au boot, redémarrage confirmé
  après un `SIGKILL` forcé). Décision d'architecture
  caméra : ESP32-CAM déporté et indépendant à la place d'une caméra
  Raspberry Pi (contrainte CAO tête), voir `ARCHITECTURE_AND_ROADMAP.md`
  §4.3 --- pas encore implémenté.
- **2026-09-06 (suite, code uniquement, pas de câblage)** --- Firmware
  `esp32-cam/` écrit (nouveau projet PlatformIO, AI-Thinker ESP32-CAM
  confirmé) : WiFi, init caméra, mDNS `rovercam.local`, serveur MJPEG
  `esp_http_server` port 81 `/stream`. Compile proprement (RAM 15.2%,
  Flash 27.5%) --- pas encore flashé (câblage FTDI prévu la prochaine
  session, voir `esp32-cam/WIRING.md`). `pi/rover_control/camera.py`
  réécrit en reverse proxy `aiohttp` vers ce flux (`camera_url`
  configurable, `503` si absent/injoignable) --- validé avec un faux
  serveur MJPEG (pas le vrai module), 37/37 tests `pi/` toujours au
  vert. CI étendue pour compiler `esp32-cam/` en plus de
  `esp32_wroom`/`esp32_s3`.
- **2026-09-05 (suite, Raspberry Pi)** --- Architecture `rover-ai`
  conçue et documentée (§17.1, deux fournisseurs IA interchangeables :
  API cloud ou LLM réseau local) ; début bring-up Raspberry Pi 3B+
  (carte SD 8 Go) bloqué sur le WiFi --- `wlan0` n'existe jamais côté
  noyau, testé sur deux cartes physiques, deux OS différents
  (Raspberry Pi OS + DietPi) et après reflash complet (cache Imager
  purgé), symptôme identique à chaque fois. Non résolu, session
  arrêtée ici (voir "Prochaines étapes" point 0).
- **2026-09-05** --- Portail de configuration WiFi accessible PC/smartphone
  ajouté (`esp32/lib/network/`, `SYSTEM action=wifi_setup`), sans
  reflash, identifiants persistés en NVS ; `RoverOTA` branché dessus
  (fallback sur les variables d'environnement existantes). **Testé et
  validé sur matériel réel (COM10)** : bug de buffer tronquant l'IP
  corrigé, bug plus sérieux de couplage WiFi/OTA trouvé et corrigé
  (connexion WiFi dépendait à tort d'un mot de passe OTA déjà défini) ;
  connexion au réseau domestique confirmée (`wifi_mode=wifi`), puis OTA
  activée en un second passage par le portail (`wifi_mode=ota`). **Flash
  OTA réel ensuite testé et réussi** après avoir écarté un VPN/pare-feu
  PC bloquant le trafic, puis corrigé un vrai bug firmware (watchdog 3s
  qui se déclenchait en plein transfert, `ArduinoOTA.onProgress` ajouté
  pour le nourrir) --- portail WiFi/OTA entièrement validé de bout en
  bout.
- **2026-09-02** --- Erreur de mapping fil→fonction encodeur trouvée et
  corrigée (silkscreen PCB) ; asymétrie moteur/canal B diagnostiquée
  (multimètre) puis vraie cause trouvée : sens de comptage encodeur
  opposé au sens moteur sur les deux roues, corrigé en firmware
  (`tickSign`) ; PID + géométrie roue première passe (ticks/tour mesuré
  à 1073, gains 180/300/0 caractérisés 0.05-0.25, pas de roue montée
  donc à refaire plus tard) ; MPU6050 et BME280 câblés et validés (le
  BME688 prévu s'est avéré être un BME280, détection auto par chip-id
  ajoutée pour un futur remplacement plug-and-play) ; outillage bring-up
  I2C ajouté (`i2c_scan`, `bme_chip_id`, `raw_ticks`/`reset_ticks`) ;
  VL53L0X pas encore câblés, plan de câblage documenté pour la prochaine
  session (voir "Prochaines étapes" ci-dessus).
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
