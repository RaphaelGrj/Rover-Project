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

## Prochaines étapes

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

------------------------------------------------------------------------

## Journal court (une ligne par session --- détail complet dans PROGRESS_ARCHIVE.md)

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
