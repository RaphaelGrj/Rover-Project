# Rover Protocol --- Spécification V1

> Document technique complémentaire à `ARCHITECTURE_AND_ROADMAP.md`.
> Ce document définit le format exact des messages échangés entre le
> Raspberry Pi et l'ESP32 (section 7 du document d'architecture).

------------------------------------------------------------------------

## 1. Version

```
ROVER_PROTOCOL_V1
```

Le numéro de version doit être annoncé par l'ESP32 au démarrage (voir
§8) et vérifié par le Raspberry Pi. Un changement incompatible du
format impose une nouvelle version (`ROVER_PROTOCOL_V2`, ...), jamais
une modification silencieuse de la V1.

------------------------------------------------------------------------

## 2. Couche physique

- Liaison : **socket TCP sur le WiFi** depuis le 2026-09-15 (Pi
  déporté, voir §6.2 de l'architecture). Transport historique : UART
  série sur câble USB (`/dev/ttyUSB0`).
- Vitesse : `115200` bauds, 8N1 --- pertinent uniquement pour le
  transport UART.
- **Le format des trames est identique quel que soit le transport.**
  Aucun message, aucun checksum, aucune séquence de démarrage ne change
  entre USB et WiFi : c'est un changement de couche physique, pas de
  protocole. Le protocole V1 n'est donc pas versionné à nouveau
  (voir §29 de l'architecture).
- Cette portabilité n'est pas un heureux hasard, elle était déjà
  garantie aux deux extrémités :
  - côté ESP32, `RoverProtocol` est conçu autour d'un `Stream`
    générique --- un `WiFiClient` *est* un `Stream`, au même titre qu'un
    `HardwareSerial` ;
  - côté Pi, `pi/rover_esp32/link.py` utilise
    `serial.serial_for_url()`, qui accepte indifféremment
    `/dev/ttyUSB0`, `socket://host:port` ou `rfc2217://...` (ce dernier
    servant déjà à piloter une simulation Wokwi).
- L'implémentation ne doit dépendre d'aucun port ni d'aucun transport
  particulier.

------------------------------------------------------------------------

## 3. Format général d'un message

Chaque message est une ligne ASCII terminée par `\n` :

```
TYPE [clé=valeur ...] *CS\n
```

- `TYPE` : type du message (`MOVE`, `HEAD`, `STATE`, `EVENT`, ...).
- `clé=valeur` : zéro ou plusieurs champs séparés par un espace.
- `*CS` : marqueur de checksum, précédé d'un espace.
- `CS` : 2 caractères hexadécimaux majuscules.

### 3.1 Calcul du checksum

Le checksum est le **XOR** de tous les octets du contenu, c'est-à-dire
`TYPE` et les champs `clé=valeur` séparés par un simple espace, **sans**
l'espace final ni le `*CS` lui-même.

Exemple :

```
MOVE velocity=0.25 rotation=-0.10 *39
```

Contenu utilisé pour le checksum : `MOVE velocity=0.25 rotation=-0.10`.

### 3.2 Contraintes de trame

- Longueur maximale d'une ligne : **128 octets** (`\n` exclu).
- Encodage : ASCII imprimable uniquement.
- Une ligne trop longue ou dont le checksum est invalide est
  **rejetée silencieusement au niveau transport** ; l'ESP32 répond
  `ERROR frame_too_long` ou `ERROR checksum_invalid` (voir §9).
- Le récepteur doit tolérer une terminaison `\r\n` (le `\r` est ignoré).

------------------------------------------------------------------------

## 4. Catégories de messages

```
COMMAND        Raspberry Pi → ESP32
STATE          ESP32 → Raspberry Pi
EVENT          ESP32 → Raspberry Pi
ERROR          ESP32 → Raspberry Pi
ACK            ESP32 → Raspberry Pi
HEARTBEAT      Raspberry Pi → ESP32
AUTH           Raspberry Pi → ESP32
```

------------------------------------------------------------------------

## 5. COMMAND (Pi → ESP32)

| Type        | Champs                          | Exemple                                  |
|-------------|----------------------------------|-------------------------------------------|
| `MOVE`      | `velocity` (m/s), `rotation` (rad/s) | `MOVE velocity=0.25 rotation=-0.10 *39` |
| `HEAD`      | `pitch` (deg), `yaw` (deg)       | `HEAD pitch=15 yaw=-20 *3A`               |
| `FACE`      | `emotion`                        | `FACE emotion=happy *2E`                  |
| `ANIMATION` | `name`                           | `ANIMATION name=GLITCH_03 *11`            |
| `AUDIO`     | `action`, ...                    | `AUDIO action=play id=1 *0C`              |
| `LIGHT`     | `state`, ...                     | `LIGHT state=on *0F`                      |
| `SYSTEM`    | `action` (`ping`, `reset`, ...)  | `SYSTEM action=ping *2A`                  |

Tout champ `seq=<entier>` est optionnel et sert à corréler la commande
avec un futur `ACK` (voir §7). Les commandes qui ne le fournissent pas
ne reçoivent pas d'ACK.

### 5.1 SYSTEM --- actions réservées

| `action=`  | Effet                                                        |
|------------|---------------------------------------------------------------|
| `ping`     | l'ESP32 répond `SYSTEM action=pong`                           |
| `resume`   | sortie de l'état `SAFE` vers `ACTIVE` (voir §9) --- refusé tant que l'E-stop physique est enfoncé, et ce refus est **rapporté** (`ERROR code=estop_held`, §7.3) au lieu d'être ignoré en silence. Sans objet (donc sans erreur) si l'ESP32 est déjà `ACTIVE`. C'est la **seule** façon de ré-armer les moteurs : un `MOVE` reçu en `SAFE` est jeté, un `HEARTBEAT` ne ré-arme rien |
| `diag`     | diagnostic série : l'ESP32 répond `STATE uptime_ms=... free_heap=... state=... board=... protocol=...` |
| `set_pid`  | calibre les gains PID moteur à chaud, persistés en NVS (survit au reboot) : `SYSTEM action=set_pid kp=180 ki=300 kd=0` --- un champ omis garde sa valeur actuelle. Répond `STATE pid_kp=... pid_ki=... pid_kd=...`, ou `ERROR code=invalid_pid_gains` si une valeur est négative/NaN/infinie |
| `get_pid`  | répond `STATE pid_kp=... pid_ki=... pid_kd=...` avec les gains actuellement actifs |
| `reset_pid` | revient aux gains compilés par défaut (`motion_config.h`) et oublie la valeur sauvegardée en NVS |
| `wifi_setup` | ouvre le portail de configuration WiFi/OTA (point d'accès temporaire `Rover-Setup-XXXX`, voir `esp32/OTA.md`) --- accessible depuis un PC ou un smartphone, se ferme seul après 10 min d'inactivité |
| `wifi_status` | répond `STATE wifi_mode=...` : `setup` (portail ouvert), `wifi` avec `ip=...` (connecté au réseau, OTA pas encore configurée), `ota` avec `ip=...` (connecté, OTA active), ou `off` |
| `wifi_forget` | efface le SSID/mot de passe WiFi enregistrés en NVS (garde le mot de passe OTA) |
| `standalone` / `standalone_off` | entre/sort du pilotage autonome (point d'accès WPA2 + page joystick, §6.3 de l'architecture) |
| `motor_raw` | **bring-up uniquement** : `SYSTEM action=motor_raw left=200 right=200 ms=2000` applique un rapport cyclique fixe directement au pont en H, **sans PID ni encodeur**, et s'arrête tout seul (15 s max). Indispensable pour diagnostiquer une roue immobile : la sortie du PID sature à 255 *parce que* rien ne tourne, donc un blocage mécanique, un driver mort et un encodeur inversé y sont indiscernables. Refusé hors de l'état `ACTIVE` |
| `pintest` | **bring-up uniquement** : `SYSTEM action=pintest pin=13 level=1` force une broche (servos ou entrées moteur seulement) à un niveau logique continu, bien plus lisible au multimètre qu'un PWM. Réarme le PWM moteur après coup, `digitalWrite` détachant la broche du LEDC |
| `head_origin` / `head_status` | `head_origin` fige les angles servo courants comme position de référence de la tête et les persiste en NVS ; tout angle logique est ensuite mesuré depuis là, ce qui rend un remontage de palonnier rattrapable sans reflasher. `head_status` renvoie `head_enabled=`, `head_a_att=`, `head_b_att=`, `head_a_deg=`, `head_b_deg=`, `head_org_a=`, `head_org_b=` |
| `servo` | **bring-up uniquement** : `which=a\|b` pilote un servo en relâchant l'autre, `which=ab a= b=` les deux indépendamment, `which=pair angle=` les deux depuis l'origine, `which=off` relâche tout (aucune impulsion, aucun couple) |
| `tof_status` / `tof_rescan` | diagnostic des deux VL53L0X : `tof_pre29=`/`tof_pre30=` (qui répondait avant la séquence), `tof_areset=` (remise à l'adresse d'usine), `tof_post29=`, `tof_lbegin=`/`tof_rbegin=` (init de chaque côté). `tof_rescan` rejoue la séquence d'abord --- utile après avoir rebranché un capteur, sans redémarrer. Les quatre causes possibles d'un `distance_left=9999` sont indiscernables autrement |
| `net_status` | répond `STATE net=...` : santé du lien WiFi vue du robot --- `up` avec `ip=`, `rssi=` et `drops=` (nombre de coupures depuis le boot), `down` avec `drops=`, `suspended` (radio prêtée au portail ou au pilotage autonome) ou `unconfigured`. C'est l'instrument de mesure du lien avant le réglage des seuils de heartbeat (§6.2, étape 2) |

------------------------------------------------------------------------

## 5.2 AUTH --- authentification du lien (Pi → ESP32)

```
AUTH secret=<secret partagé> *xx
```

**Obligatoire sur un transport réseau, inutile sur le câble USB.**

Tant que le lien était un câble, l'authentification était physique :
pour envoyer un `MOVE`, il fallait être dans la pièce, une main sur le
robot. Une socket TCP supprime cette protection sans rien mettre à la
place --- n'importe qui sur le WiFi pilote alors les moteurs, sans mot
de passe et sans trace. Voir `ARCHITECTURE_AND_ROADMAP.md` §6.2,
question « 5 bis ».

Règles :

- L'ESP32 **rejette toute trame** reçue avant un `AUTH` valide, y
  compris un `HEARTBEAT` --- sans quoi un pair non authentifié pourrait
  maintenir le robot en `ACTIVE` sans jamais s'identifier.
- Réponse en cas de succès : `STATE link=authenticated`. La trame
  `AUTH` est consommée, jamais traitée comme une commande.
- Réponse en cas d'échec : `ERROR code=unauthenticated` (secret faux ou
  absent) ou `ERROR code=link_secret_not_set` (aucun secret enregistré
  côté robot). Les deux codes sont distincts parce que le premier se
  corrige côté Pi et le second côté robot.
- **L'authentification vaut pour une connexion, jamais au-delà** : le
  robot l'oublie à chaque coupure, donc le Pi la represente à chaque
  reconnexion (`pi/rover_esp32/link.py`).
- Comparaison à temps constant, longueur du secret comprise.
- Secret stocké en NVS côté robot (saisi par le portail
  `SYSTEM action=wifi_setup`), 8 à 47 caractères. Côté Pi, il vient de
  la variable d'environnement `ROVER_LINK_SECRET`, **jamais de
  `config.json`**.
- **Pas de secret enregistré = tout est refusé** (jamais l'inverse),
  comme l'OTA refuse de flasher sans mot de passe. Le robot reste
  pilotable par le mode autonome (§6.3) et par USB.

⚠ **Limite assumée** : le secret circule en clair dans la trame, protégé
seulement par le chiffrement WPA2 du réseau. Cela protège de « qui
peut joindre l'IP du robot », pas de « qui a déjà la clé WiFi et
capture les paquets » --- même niveau que le mot de passe ArduinoOTA et
que le token du serveur de contrôle. Un défi/réponse (nonce + HMAC)
serait l'incrément suivant.

------------------------------------------------------------------------

## 6. HEARTBEAT (Pi → ESP32)

```
HEARTBEAT *00\n
```

- Envoyé périodiquement par le Pi, recommandé toutes les **150 ms**
  (soit environ 3 fois par fenêtre de timeout).
- Timeout de sécurité, **révisé le 2026-09-15** pour le transport WiFi
  (Pi déporté) --- trois paliers au lieu d'un seuil unique, voir §9 de
  l'architecture pour le raisonnement complet :

  | Sans message valide depuis | État | Effet |
  |---|---|---|
  | < 500 ms | `NOMINAL` | --- |
  | 500 ms | `DEGRADED` | vitesse plafonnée (~30 %), `EVENT name=link_degraded` |
  | ~1500 ms | `TIMEOUT` | `STOP MOTORS` → état `SAFE` |

  ⚠ Ces valeurs sont un point de départ raisonné, **pas un résultat
  mesuré** : à confirmer par une mesure de gigue/perte en conditions
  réelles avant d'être considérées comme définitives.
- N'importe quel message valide (`COMMAND` ou `HEARTBEAT`) réinitialise
  le compteur de timeout ; `HEARTBEAT` n'est qu'un minimum garanti
  quand aucune commande n'est envoyée.

------------------------------------------------------------------------

## 7. STATE / EVENT / ERROR / ACK (ESP32 → Pi)

### 7.1 STATE

Émis périodiquement (fréquence à définir en Phase 2/4, un champ par
message ou groupés) :

```
STATE battery=82 *1F
STATE left_speed=0.24 right_speed=0.26 *0A
STATE distance_left=420 distance_right=380 *2C
STATE temperature=24.3 humidity=45.2 pressure=1013.2 gas_kohm=120.5 *19
STATE accel_x=-0.12 accel_y=0.03 accel_z=9.81 gyro_x=0.01 gyro_y=-0.02 gyro_z=0.00 *2A
STATE state=SAFE estop=0 *33
```

`state=` / `estop=` : l'état de la machine à états de l'ESP32 (§9) et
l'état du bouton d'arrêt d'urgence. Émis **au changement uniquement**
(plus une fois au boot), donc coût nul en régime établi --- le Pi
fusionne les champs `STATE` et les rejoue aux clients qui se connectent
plus tard, une trame par transition suffit. À ne pas confondre avec
l'état comportemental du Pi (`ROVER_STATE`, §20 de l'architecture) : ce
sont deux machines distinctes.

Pourquoi c'est diffusé et pas seulement disponible sur demande
(`action=diag`) : un robot en `SAFE` ignore tous les `MOVE` par
conception. Sans ce champ, un timeout heartbeat après une micro-coupure
WiFi est indiscernable d'un moteur mort --- exactement la confusion qui
a coûté plusieurs sessions de diagnostic moteur (`PROGRESS.md`).

`distance_left`/`distance_right` sont en millimètres ; `9999` signifie
"capteur indisponible" (échec `begin()` ou perte depuis), `8190`
signifie "rien détecté dans la portée" (le capteur répond mais ne voit
pas d'obstacle) -- voir `esp32/lib/sensors/DistanceSensor.cpp`.

### 7.2 EVENT

Émis ponctuellement lors d'un changement d'état matériel :

```
EVENT name=obstacle_detected *0B
EVENT name=robot_lifted *07
EVENT name=head_limit *05
EVENT name=low_battery *0D
EVENT name=estop_pressed *12
```

`estop_pressed` : bouton d'arrêt d'urgence physique enfoncé (voir
`esp32/lib/safety/EStop.h`) -- force l'état `SAFE` indépendamment du
heartbeat, et bloque `SYSTEM action=resume` tant que le bouton reste
enfoncé (relâcher le bouton ne suffit pas à lui seul, un `resume`
explicite reste nécessaire ensuite, comme pour un timeout heartbeat).

### 7.3 ERROR

| Code                  | Origine                              |
|------------------------|---------------------------------------|
| `motor_overcurrent`    | protection moteur                     |
| `sensor_timeout`       | capteur I2C ne répond plus             |
| `display_failure`      | écran ST7789 en erreur                |
| `checksum_invalid`     | trame reçue corrompue                 |
| `frame_too_long`       | trame reçue > 128 octets              |
| `unknown_command`      | `TYPE` non reconnu                    |
| `invalid_pid_gains`    | `SYSTEM action=set_pid` avec une valeur négative, NaN ou infinie |
| `unauthenticated`      | trame reçue avant un `AUTH` valide sur un lien réseau, ou secret faux (§5.2) |
| `link_secret_not_set`  | `AUTH` reçue mais aucun secret enregistré côté robot (§5.2) |
| `tx_truncated`         | trame sortante trop longue, abandonnée plutôt qu'émise tronquée |
| `estop_held`           | `SYSTEM action=resume` reçue alors que l'E-stop physique est enfoncé (§5.1) |

```
ERROR code=motor_overcurrent *3D
ERROR code=sensor_timeout sensor=tof_left *48
```

`sensor=` est un champ optionnel supplémentaire sur `sensor_timeout`
(même esprit que `reason=` sur un `ACK` d'échec, §7.4) pour indiquer
lequel des capteurs Phase 4 est en cause : `tof_left`, `tof_right`,
`imu` ou `bme688`. Émis une seule fois par transition OK→échec (y
compris un échec dès le boot, pas seulement une perte en cours de
route), jamais en boucle à chaque nouvelle tentative de reconnexion.

### 7.4 ACK

Réponse à une commande porteuse d'un `seq` :

```
ACK seq=42 status=ok *18
ACK seq=42 status=error reason=head_limit *2F
```

------------------------------------------------------------------------

## 8. Séquence de démarrage

```
ESP32 boot
  ↓
SYSTEM protocol=ROVER_PROTOCOL_V1 board=<WROOM|S3> state=BOOT *xx
  ↓
état interne : BOOT → READY
  ↓
en attente du premier HEARTBEAT du Pi
  ↓
(lien réseau uniquement : AUTH secret=... → STATE link=authenticated)
  ↓
READY → ACTIVE (dès réception d'un HEARTBEAT ou d'une COMMAND valide)
```

Sur un transport réseau, l'étape `AUTH` est un préalable strict : aucune
trame reçue avant elle ne fait quoi que ce soit, et le passage
`READY → ACTIVE` ne peut donc pas avoir lieu (§5.2).

Le Pi doit vérifier le champ `protocol=` avant d'envoyer des commandes
et refuser de continuer si la version ne correspond pas à celle qu'il
attend.

------------------------------------------------------------------------

## 9. Machine d'état ESP32 (rappel §11 de l'architecture)

```
BOOT → READY → ACTIVE → SAFE → ERROR
```

- `SAFE` est atteint automatiquement en cas de timeout heartbeat, et
  **jamais** contourné par une commande logicielle du Pi.
- Le retour de `SAFE` vers `ACTIVE` nécessite la reprise d'un
  heartbeat valide **et** une commande explicite (`SYSTEM
  action=resume`), pour éviter un redémarrage moteur inattendu.

------------------------------------------------------------------------

## 10. Compatibilité matérielle

Le protocole est indépendant du **transport** (UART sur câble USB ou
socket TCP WiFi, voir §2) et du modèle exact de la carte (WROOM ou S3,
voir `CLAUDE.md`). Le champ `board=` du message `SYSTEM` de boot permet
au Pi de savoir sur quelle variante il communique, sans que cela change
le format des messages.

Carte de référence actuelle : **WROOM**, confirmée le 2026-09-15 (un S3
est disponible mais gardé pour une évolution ultérieure).

------------------------------------------------------------------------

## Statut du document

**Protocole cible : V1 --- en cours d'implémentation (Phase 0/1).**
