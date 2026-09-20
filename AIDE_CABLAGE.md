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

✅ **Câblé et validé sur le vrai robot le 2026-09-20** --- les deux
capteurs répondent et mesurent.

Les deux capteurs partagent le même bus I2C (2 fils communs à tout le
bus). **Un seul fil `XSHUT` est piloté**, celui du capteur droit.

| Broche du module VL53L0X | Où la brancher |
|---|---|
| `VIN`/`VCC` (les deux capteurs) | 3V3 |
| `GND` (les deux capteurs) | GND commun |
| `SDA` (les deux capteurs, même fil) | GPIO21 |
| `SCL` (les deux capteurs, même fil) | GPIO22 |
| `XSHUT` du capteur **gauche** | **3V3** (en permanence, comme `VIN`) |
| `XSHUT` du capteur **droit** | GPIO4 |
| `GPIO1` du module (sortie interruption) | ne rien brancher |

**Placement physique** : les deux **côte à côte en façade avant**,
tournés vers l'avant --- pas un devant et un derrière. Le code
`distance_left`/`distance_right` et le réflexe d'arrêt sur obstacle
supposent les deux orientés dans la même direction.

### ⚠ Pourquoi le `XSHUT` gauche va au 3V3 et pas sur une broche

Ce guide demandait `GPIO0` jusqu'au 2026-09-20. **C'était impossible** :
le devkit ESP32 WROOM 30 broches **ne sort pas GPIO0 du tout**. La
rangée réelle enchaîne `GND, 15, 2, 4, RX2, TX2` --- il n'y a aucun `0`
nulle part, parce que cette broche reste interne à la carte (elle est
reliée au bouton `BOOT` et au circuit qui redémarre l'ESP32 tout seul
au moment du flash).

C'est la raison pour laquelle ces deux capteurs sont restés non câblés
du 2026-09-02 au 2026-09-20 : le plan était irréalisable, sans que
personne s'en aperçoive.

Rien n'est perdu. Les deux capteurs sortent d'usine avec **la même
adresse** (0x29) et ne peuvent donc pas parler sur le bus en même temps
au démarrage. Pour les départager, il suffit d'en **éteindre un seul**
le temps de renommer l'autre :

1. le firmware éteint le capteur **droit** (`GPIO4` au niveau bas) ;
2. le capteur **gauche**, seul à répondre, est renommé en `0x30` ;
3. le firmware rallume le droit, qui prend l'adresse `0x29` laissée
   libre.

Le `XSHUT` du gauche n'a donc jamais besoin d'être piloté : il reste
branché au 3V3, c'est-à-dire « allumé en permanence ». En prime, ça
libère une broche sur une carte où il n'en reste aucune, et ça évite
une broche sensible au démarrage.

💡 **Un détail qui peut surprendre** : un VL53L0X garde son nouveau nom
(`0x30`) tant qu'il n'est pas **débranché du courant**. Redémarrer
l'ESP32 (bouton, reflash) ne suffit pas. Le firmware le remet donc
lui-même à son adresse d'usine avant chaque initialisation --- sans
quoi le capteur gauche serait porté disparu à partir du deuxième
démarrage. Rien à faire de ton côté, mais ça explique le
`SYSTEM action=tof_status` ci-dessous.

### Vérifier que le câblage est bon

- `SYSTEM action=i2c_scan` → doit lister **`0x29` et `0x30`** (en plus
  de `0x68` = capteur de mouvement et `0x76` = capteur
  température/pression).
- `SYSTEM action=tof_status` → détaille ce que la séquence de
  démarrage a vu, étape par étape (utile si un capteur manque à
  l'appel) : `tof_post29=1` et `tof_lbegin=1` signifient que le gauche
  a bien été trouvé et initialisé, `tof_rbegin=1` pareil pour le droit.
- `SYSTEM action=tof_rescan` → rejoue toute la séquence sans
  redémarrer, pratique après avoir rebranché un fil.
- Dans la télémétrie, `distance_left=`/`distance_right=` en
  millimètres. **`9999` = capteur absent**, **`8190` = capteur présent
  mais ne voit rien à portée**. Les deux se ressemblent à l'oeil et ne
  veulent pas du tout dire la même chose.

------------------------------------------------------------------------

## Servomoteur de tête (un seul, MG90S)

✅ **Câblé et validé sur le robot le 2026-09-20.**

| Fil du servo | Où le brancher |
|---|---|
| **Marron** (ou noir) | GND --- **la même masse que l'ESP32**, sinon le signal n'a aucune référence et le servo part n'importe où |
| **Rouge** | +5 à 6 V (jamais le 3V3 de l'ESP32 : un MG90S tire jusqu'à 700 mA) |
| **Orange** (ou jaune) | **GPIO13** |

### ⚠ Pourquoi un seul servo, et pas deux

Le projet prévoyait deux servos face à face entraînant la même pièce
(montage « tandem »). **Abandonné le 2026-09-20** : deux servos sur un
axe commun ne sont jamais parfaitement d'accord (leurs points neutres et
leurs courses diffèrent toujours un peu), donc ils passent leur temps à
se combattre --- ça chauffe, ça use, et ça ne tient pas la position.
Remplacé par **un servo qui entraîne + un roulement qui guide** de
l'autre côté.

### ⚠ Le piège du palonnier : à lire avant de (re)monter le bras

Le bras (« palonnier ») se fixe sur l'axe par des cannelures : il ne
peut être posé que tous les ~1,4°, et **rien n'indique où est le milieu
de la course du servo**. Le 2026-09-20, il était monté environ **90°
hors du neutre** : la position de repos de la tête tombait tout au bout
de la course du servo, si bien que **n'importe quel angle commandé
enfonçait la tête dans sa butée**, dans les deux sens. Ça a coûté une
bonne partie de la session avant d'être compris.

**La bonne méthode, à suivre à chaque remontage :**

1. **Retirer le bras** de l'axe du servo (pas seulement le dévisser de
   la pièce : l'enlever complètement).
2. Demander le centrage : le firmware place le servo au **milieu exact**
   de sa course.
3. **Alors seulement**, refixer le bras, tête en position neutre.

On dispose ainsi de ~90° de marge de chaque côté, pour une tête qui n'a
besoin que de ~20°.

💡 Le firmware enregistre cette position de référence en mémoire
(`SYSTEM action=head_origin`), et elle **survit aux redémarrages et aux
reflash**. Si le bras est remonté légèrement de travers, il suffit de
réenregistrer l'origine --- pas besoin de recompiler quoi que ce soit.

### ⚠ Ne jamais utiliser GPIO19

Cette broche était prévue pour un second servo : **elle n'émet rien sur
cette carte** (vérifié le 2026-09-20 en croisant les deux servos et deux
canaux PWM différents). C'est la deuxième broche défectueuse de ce
devkit, après GPIO0 qui n'est carrément pas sortie sur le connecteur.

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
- GPIO19 : **ne fonctionne pas sur cette carte** (constaté le
  2026-09-20 en croisant deux servos et deux canaux PWM). Ne rien y
  brancher.
- GPIO0 : **n'existe pas sur le connecteur** de ce devkit 30 broches
  (constaté sur la carte le 2026-09-20). Inutile de la chercher : si un
  plan de câblage la mentionne, c'est le plan qui est faux.
