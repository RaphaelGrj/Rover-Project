# Connexion WiFi et mise à jour du firmware (OTA)

> Complémentaire à `WIRING.md`. Fonctionnalité **optionnelle**, inactive
> tant qu'elle n'est pas configurée --- aucun impact sur le
> fonctionnement normal du robot (voir `esp32/lib/ota/RoverOTA.h` et
> `esp32/lib/network/`).

------------------------------------------------------------------------

## Pourquoi

Une fois le robot assemblé, rouvrir la tête pour brancher un câble USB
à chaque flash de firmware devient pénible. L'OTA permet de flasher par
WiFi à la place, une fois le robot déjà sur le réseau local.

Ce n'est **pas** une entorse à la règle "l'ESP32 ne doit jamais dépendre
d'Internet" (`ARCHITECTURE_AND_ROADMAP.md` §22) : c'est un canal de
maintenance **réseau local uniquement**, jamais utilisé par le
fonctionnement normal du robot (le Rover Protocol reste 100% UART), et
entièrement inactif tant qu'un développeur ne l'active pas
explicitement.

------------------------------------------------------------------------

## Deux façons de fournir les identifiants WiFi

### 1. Portail de configuration (recommandé --- PC ou smartphone, aucun reflash)

Le firmware peut ouvrir son propre point d'accès WiFi temporaire pour
recevoir les identifiants du vrai réseau, sans jamais les écrire dans
le code ni dans un fichier du dépôt :

1. Connecte-toi à l'ESP32 en série (USB, `pio device monitor`, ou tout
   terminal série) et envoie une trame `SYSTEM action=wifi_setup`.
   Le Raspberry Pi pourra plus tard envoyer la même trame via le Rover
   Protocol UART habituel.
2. Le robot ouvre un point d'accès WiFi nommé `Rover-Setup-XXXX` (les 3
   derniers octets de l'adresse MAC, pour distinguer plusieurs Rovers).
3. Depuis **un PC ou un smartphone**, connecte-toi à ce réseau WiFi
   (ouvert, pas de mot de passe --- voir "Sécurité" ci-dessous), puis
   ouvre un navigateur sur `http://192.168.4.1/` (ou laisse le
   téléphone/PC ouvrir automatiquement la page de connexion captive).
4. Renseigne le SSID/mot de passe du vrai réseau WiFi et, si ce n'est
   pas déjà fait, un mot de passe OTA. Valide : le robot enregistre en
   NVS (flash interne, survit aux redémarrages et aux reflash) et
   redémarre automatiquement. Laisser un champ mot de passe vide garde
   la valeur déjà enregistrée (utile pour revenir plus tard ajouter
   juste le mot de passe OTA sans retaper le mot de passe WiFi).
5. Au démarrage suivant, l'ESP32 rejoint ce réseau **même sans mot de
   passe OTA renseigné** (`STATE wifi_mode=wifi ip=...`) --- la
   connexion WiFi et l'activation de l'OTA sont deux choses
   indépendantes, seule l'OTA (`ArduinoOTA`) refuse de démarrer sans son
   propre mot de passe (`STATE wifi_mode=ota ip=...` une fois les deux
   configurés).

Le portail se ferme tout seul au bout de 10 minutes d'inactivité s'il
n'est pas utilisé, pour ne jamais laisser un point d'accès de
configuration ouvert en permanence.

Autres actions disponibles en `SYSTEM action=...` :

- `wifi_status` --- renvoie `STATE wifi_mode=...` (`setup` si le portail
  est ouvert, `ota` avec l'IP si connecté et l'OTA active, `off` sinon).
- `wifi_forget` --- efface le SSID/mot de passe WiFi enregistrés (garde
  le mot de passe OTA) ; le prochain `wifi_setup` repart d'un formulaire
  vierge.

### 2. Variables d'environnement au moment de la compilation (avancé/CI)

Toujours disponible, utilisée uniquement si aucun identifiant n'a été
enregistré via le portail ci-dessus (la NVS a priorité dès qu'elle
contient un SSID) :

```
ROVER_WIFI_SSID
ROVER_WIFI_PASSWORD
ROVER_OTA_PASSWORD
```

------------------------------------------------------------------------

## ⚠️ Sécurité --- important pour un projet open source

Ce dépôt est public. **Aucun identifiant WiFi ni mot de passe OTA ne
doit jamais être écrit en dur dans un fichier commité** --- ni ici, ni
dans `platformio.ini`, ni dans le code. Un mot de passe par défaut
partagé dans un projet open source serait une vraie faille : tout le
monde qui télécharge le projet aurait le même, jusqu'à ce que quelqu'un
pense à le changer (et personne ne le fera systématiquement).

Si les variables d'environnement ne sont pas définies **et** qu'aucun
identifiant n'a été enregistré via le portail, l'OTA reste désactivée
--- c'est le cas par défaut pour tout le monde. Le firmware **refuse
aussi de démarrer l'OTA sans mot de passe OTA** (vide), pour ne jamais
exposer un canal de flash sans authentification à n'importe qui sur le
réseau local.

Le point d'accès de configuration (`Rover-Setup-XXXX`) est volontairement
**ouvert, sans mot de passe** : c'est un réseau temporaire, déclenché
explicitement, qui se ferme après 10 minutes --- pas un identifiant
permanent à protéger comme le WiFi/OTA. Deux limites à connaître :

- Pendant la fenêtre de 10 minutes, n'importe qui à portée radio peut
  rejoindre ce point d'accès et soumettre le formulaire --- ne déclenche
  `wifi_setup` que quand tu es prêt à configurer immédiatement.
- Le formulaire est servi en HTTP simple (pas HTTPS) sur ce point
  d'accès local : acceptable puisque la liaison est directe
  téléphone/PC ↔ robot (rien ne transite par un routeur tiers), mais à
  garder en tête.

------------------------------------------------------------------------

## Configuration par variables d'environnement (Linux Mint / bash)

```bash
export ROVER_WIFI_SSID="TonReseauWifi"
export ROVER_WIFI_PASSWORD="TonMotDePasseWifi"
export ROVER_OTA_PASSWORD="un-mot-de-passe-different-et-solide"

cd esp32
pio run -e esp32_wroom -t upload --upload-port /dev/ttyUSB0   # premier flash, USB obligatoire
```

Ces `export` ne persistent que pour le terminal courant --- ajoute-les à
`~/.bashrc` (ou un fichier chargé par ton shell, **jamais commité**) si
tu veux les garder d'une session à l'autre.

## Flasher par WiFi une fois l'OTA active

Une fois le firmware avec OTA activée tourne sur le robot et rejoint le
réseau configuré, PlatformIO peut cibler son adresse IP au lieu d'un
port USB :

```bash
pio run -e esp32_wroom -t upload --upload-port <IP_DU_ROVER>
```

(trouver l'IP via ton routeur/`arp -a`, ou --- une fois le point "mDNS"
de `PROGRESS.md` en place côté Pi --- via `rover.local`).

## Windows (PowerShell)

```powershell
$env:ROVER_WIFI_SSID = "TonReseauWifi"
$env:ROVER_WIFI_PASSWORD = "TonMotDePasseWifi"
$env:ROVER_OTA_PASSWORD = "un-mot-de-passe-different-et-solide"
```

------------------------------------------------------------------------

## Limites connues

- **Testé et validé de bout en bout sur matériel réel** (2026-09-05,
  ESP32 WROOM, COM10) : portail de configuration, connexion WiFi,
  activation OTA, puis un vrai flash OTA réussi
  (`pio run -t upload --upload-port <IP>`).
- Un flash OTA réel a d'abord échoué à chaque tentative vers ~15% du
  transfert : `ArduinoOTA.handle()` bloque en interne pendant l'écriture
  flash tant que le transfert est en cours, sans jamais rendre la main à
  `loop()` entre deux morceaux --- ça finissait par déclencher le
  watchdog matériel 3s (`esp32/lib/safety/Watchdog.h`) et redémarrer la
  carte en plein transfert. Corrigé en nourrissant le watchdog depuis le
  callback `ArduinoOTA.onProgress()` (voir `esp32/lib/ota/RoverOTA.h`).
- Si tu testes un flash OTA depuis un PC : un **VPN actif côté PC** (kill
  switch bloquant le trafic LAN) et le **pare-feu Windows** (réseau
  classé "Public" par défaut, bloque la connexion TCP entrante que
  l'ESP32 initie vers l'outil de flash) peuvent tous les deux faire
  échouer le transfert silencieusement --- vérifier `ping <IP_du_rover>`
  depuis le PC avant de soupçonner le firmware.
- Ouvrir le portail (`wifi_setup`) coupe toute connexion WiFi/OTA en
  cours, puisque les deux partagent la même radio --- normal, pas un
  bug : une éventuelle mise à jour OTA en cours serait interrompue.
- GPIO14 (ADC batterie, voir `WIRING.md`) est en ADC2 et devient
  illisible pendant que le WiFi est actif --- pas un problème tant que
  le monitoring batterie reste désactivé par défaut, à revoir si les
  deux fonctionnalités sont activées ensemble un jour.
- Le mot de passe OTA protège contre un flash non autorisé, mais le
  trafic OTA lui-même n'est pas chiffré (limite d'`ArduinoOTA` telle
  qu'utilisée ici) --- acceptable pour un usage réseau local de
  confiance, pas pour un réseau partagé/non fiable.
