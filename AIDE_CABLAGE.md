# Aide au câblage --- niveau débutant

> Ce fichier donne, composant par composant, "quelle broche va où" en
> langage simple. Pour le détail technique complet (pourquoi ce choix de
> GPIO, pièges connus, historique des révisions), voir `esp32/WIRING.md`
> et `esp32-cam/WIRING.md`.
>
> Toutes les broches ci-dessous ciblent la carte **ESP32 WROOM** (devkit
> 30 broches), la carte physique actuelle du robot.

------------------------------------------------------------------------

## Ampli audio I2S (MAX98357A)

Confirmé avec l'utilisateur le 2026-09-13. Cet ampli reçoit un signal
audio numérique (I2S) de l'ESP32 principal et pilote un petit
haut-parleur.

| Broche du module MAX98357A | Où la brancher | Remarque |
|---|---|---|
| `VIN` | 5V (pas 3.3V) | L'ampli a besoin de 5V pour driver le haut-parleur correctement |
| `GND` | GND commun (ESP32 + alim) | |
| `BCLK` | GPIO16 | |
| `LRC` (parfois noté `WS`) | GPIO17 | |
| `DIN` | GPIO14 | |
| `GAIN` | Selon le volume voulu --- relier au 3V3, au GND ou laisser flottant change le gain (voir la sérigraphie/datasheet du module précis reçu) |
| `SD` (shutdown, si présent) | 3V3 direct si pas de contrôle logiciel prévu (ampli toujours actif) |
| `+` / `−` haut-parleur | Directement les 2 fils du haut-parleur | Pas de polarité stricte à respecter en général, mais garder la même orientation sur les deux canaux si tu en câbles deux |

⚠ Ces 3 broches (GPIO16/17/14) étaient réservées à autre chose dans les
plans précédents (une liaison série vers le Raspberry Pi jamais
utilisée, et un monitoring de batterie jamais câblé) --- elles ont été
libérées spécifiquement pour cet ampli, c'est normal si tu vois une
trace de ces anciens usages ailleurs dans le dépôt.

------------------------------------------------------------------------

## Capteurs de distance VL53L0X --- gauche et droite

Les deux capteurs partagent le même bus I2C (2 fils communs à tout le
bus) plus une broche individuelle chacun pour pouvoir les distinguer au
démarrage.

| Broche du module VL53L0X | Où la brancher |
|---|---|
| `VIN`/`VCC` (les deux capteurs) | 3V3 |
| `GND` (les deux capteurs) | GND commun |
| `SDA` (les deux capteurs, même fil) | GPIO21 |
| `SCL` (les deux capteurs, même fil) | GPIO22 |
| `XSHUT` du capteur **gauche** | GPIO0 |
| `XSHUT` du capteur **droit** | GPIO4 |

⚠ GPIO0 est une broche sensible au démarrage de l'ESP32 (elle doit
rester haute/flottante pendant le boot) --- ne rien brancher dessus qui
tire fort vers le GND en permanence.

------------------------------------------------------------------------

## ESP32-CAM (module caméra séparé)

Ce module est **indépendant** de l'ESP32 principal --- il ne se
connecte à rien d'autre en filaire, seulement en WiFi une fois en
service. Le câblage ci-dessous ne sert qu'à le programmer une première
fois (et pour l'alimentation).

### Pour flasher le firmware (adaptateur FTDI/USB-série requis, réglé sur 3.3V)

| Broche de l'adaptateur FTDI | Broche de l'ESP32-CAM | Remarque |
|---|---|---|
| 5V (ou VCC) | `5V` | |
| GND | `GND` | |
| TX | `U0R` | Croisé : TX de l'adaptateur va sur RX de la carte |
| RX | `U0T` | Croisé : RX de l'adaptateur va sur TX de la carte |
| (fil volant) | `GPIO0` → `GND` | **Seulement pendant le flash**, à retirer juste après |

Étapes : relier GPIO0 au GND → appuyer sur RESET → lancer l'upload →
une fois terminé, **débrancher le fil GPIO0-GND** puis reset à nouveau.

### En usage normal (une fois flashé)

Seulement `5V` et `GND` sont nécessaires --- tout le reste (flux vidéo)
passe par le WiFi vers le Raspberry Pi.

------------------------------------------------------------------------

## Raspberry Pi ↔ ESP32 principal

**Changement important du 2026-09-15 : il n'y a plus aucun câble ici.**

Avant, le Raspberry Pi était posé sur le robot et relié à l'ESP32 par un
câble USB. Maintenant, **le Raspberry Pi reste à la maison**, branché sur
le secteur, et parle à l'ESP32 **par le WiFi** --- comme le fait déjà
l'ESP32-CAM.

Ce qui reste sur le robot : l'ESP32, l'ESP32-CAM, la batterie. C'est
tout.

Pourquoi ce choix :

- le robot devient **plus petit** (plus de Raspberry Pi ni de sa carte
  SD à caser dans le châssis) ;
- la **batterie dure nettement plus longtemps** --- le Raspberry Pi était
  le plus gros consommateur quand le robot ne roule pas ;
- un **convertisseur 5V en moins** à câbler (celui qui était dédié au
  Raspberry Pi).

⚠ Contrepartie à connaître : **sans WiFi, le robot n'a plus de cerveau**
(plus d'IA, plus de voix, plus de caméra analysée). Il continue de se
protéger tout seul : il s'arrête, évite les obstacles.

**Mais il reste pilotable** --- voir juste en dessous.

------------------------------------------------------------------------

## Piloter Rover sans Pi et sans WiFi (mode « pilotage direct »)

Si le Raspberry Pi est éteint, ou s'il n'y a aucun réseau (tu emmènes
Rover dans un parc, chez quelqu'un, en panne de box...), **le robot
fabrique lui-même son propre réseau WiFi** et tu le pilotes depuis ton
téléphone.

### Ce qu'il faut faire une seule fois, avant

Choisir un mot de passe pour ce mode, sinon il refusera de démarrer
(c'est volontaire : sans mot de passe, n'importe qui à portée pourrait
faire rouler ton robot).

1. Envoyer `SYSTEM action=wifi_setup` au robot (ou le faire depuis le
   portail de configuration habituel).
2. Se connecter au réseau `Rover-Setup-XXXXXX` qui apparaît.
3. Remplir le champ **« Mot de passe pilotage direct »** --- minimum
   **8 caractères**.
4. Enregistrer : le robot redémarre.

### Comment l'utiliser, le jour où tu en as besoin

1. Allumer le robot. **Ne rien faire pendant 30 secondes** --- au bout de
   ce délai sans nouvelle du Raspberry Pi, il ouvre tout seul un réseau
   nommé `Rover-Pilot-XXXXXX`.
2. Connecter ton téléphone à ce réseau (avec le mot de passe choisi
   plus haut).
3. Ouvrir n'importe quelle page dans le navigateur (par exemple
   `192.168.4.1`) : la page de pilotage s'affiche.
4. Appuyer sur **« Activer »** pour sortir de la sécurité, puis piloter
   avec le rond tactile. Le bouton **STOP** arrête tout.

### Bon à savoir

- **Si tu fermes la page ou t'éloignes trop, le robot s'arrête seul.**
  C'est la même sécurité que d'habitude, pas une sécurité à part.
- Le bouton d'arrêt d'urgence physique reste prioritaire sur tout.
- Le robot refuse toujours d'avancer vers un obstacle détecté ---
  reculer et tourner restent possibles.
- Ce mot de passe est **différent** de celui de l'OTA : conduire le
  robot et reprogrammer le robot ne sont pas la même chose, tu peux
  confier l'un sans l'autre.

------------------------------------------------------------------------

## Micro (prévu, pas encore câblé)

Le micro n'est pas encore acheté/câblé, mais sa place est maintenant
réservée --- c'est justement le départ du Raspberry Pi qui a libéré la
broche dont il avait besoin.

Il se branchera en partageant deux fils avec l'ampli audio :

| Broche du micro (INMP441) | Où la brancher | Remarque |
|---|---|---|
| `VDD` | 3V3 | |
| `GND` | GND commun | |
| `SCK` (horloge) | GPIO16 | **le même fil que le `BCLK` de l'ampli** |
| `WS` | GPIO17 | **le même fil que le `LRC` de l'ampli** |
| `SD` (sortie audio du micro) | GPIO3 | ⚠ à câbler avec un cavalier, voir ci-dessous |
| `L/R` | GND | choisit le canal gauche, pas de broche ESP32 nécessaire |

### ⚠ Pourquoi un cavalier (jumper) sur le fil `SD`

`GPIO3` servait à la prise USB de l'ESP32. En y branchant le micro, on
perd la possibilité de brancher un câble USB pour voir les messages de
l'ESP32 ou le reprogrammer en filaire.

Ce n'est gênant que si le WiFi ne marche plus --- mais ce jour-là, on
perdrait le pilotage **et** le dépannage en même temps. D'où la
solution : **un cavalier sur ce fil, qu'on retire pour rebrancher l'USB**
(la pièce imprimée en 3D est prévue pour y accéder sans démonter le
robot). Ne surtout pas souder ce fil en dur.

Au quotidien, la mise à jour du firmware se fait par le WiFi (OTA), donc
le cavalier reste en place la plupart du temps.

------------------------------------------------------------------------

## Broches à ne surtout pas utiliser pour un nouveau composant

Si un jour tu ajoutes encore un module, évite ces broches (déjà prises
ou dangereuses au boot) --- voir `esp32/WIRING.md` pour le tableau
complet et à jour :

- GPIO1 : prise USB (sortie), ne pas toucher.
- GPIO3 : **réservée au micro** depuis le 2026-09-15 (voir plus haut) ---
  c'était la liaison USB vers le Raspberry Pi, libérée par son départ.
- GPIO6 à GPIO11 : réservées en interne à la mémoire flash, jamais
  disponibles.
- GPIO2, GPIO12, GPIO15 : sensibles au démarrage, déjà utilisées
  (écran / buzzer) mais à éviter pour tout nouveau signal qui tirerait
  fort au boot.
