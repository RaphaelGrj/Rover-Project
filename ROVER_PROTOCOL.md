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
| `resume`   | sortie de l'état `SAFE` vers `ACTIVE` (voir §9) --- refusé tant que l'E-stop physique est enfoncé (`estop_pressed`, §7.2) |
| `diag`     | diagnostic série : l'ESP32 répond `STATE uptime_ms=... free_heap=... state=... board=... protocol=...` |
| `set_pid`  | calibre les gains PID moteur à chaud, persistés en NVS (survit au reboot) : `SYSTEM action=set_pid kp=180 ki=300 kd=0` --- un champ omis garde sa valeur actuelle. Répond `STATE pid_kp=... pid_ki=... pid_kd=...`, ou `ERROR code=invalid_pid_gains` si une valeur est négative/NaN/infinie |
| `get_pid`  | répond `STATE pid_kp=... pid_ki=... pid_kd=...` avec les gains actuellement actifs |
| `reset_pid` | revient aux gains compilés par défaut (`motion_config.h`) et oublie la valeur sauvegardée en NVS |
| `wifi_setup` | ouvre le portail de configuration WiFi/OTA (point d'accès temporaire `Rover-Setup-XXXX`, voir `esp32/OTA.md`) --- accessible depuis un PC ou un smartphone, se ferme seul après 10 min d'inactivité |
| `wifi_status` | répond `STATE wifi_mode=...` : `setup` (portail ouvert), `wifi` avec `ip=...` (connecté au réseau, OTA pas encore configurée), `ota` avec `ip=...` (connecté, OTA active), ou `off` |
| `wifi_forget` | efface le SSID/mot de passe WiFi enregistrés en NVS (garde le mot de passe OTA) |

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
```

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
READY → ACTIVE (dès réception d'un HEARTBEAT ou d'une COMMAND valide)
```

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
