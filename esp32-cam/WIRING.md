# Câblage --- ESP32-CAM (AI-Thinker)

> Référence technique. Voir aussi `AIDE_CABLAGE.md` (racine du dépôt)
> pour un tableau niveau débutant une fois ce câblage confirmé avec
> l'utilisateur (règle `CLAUDE.md`).

Ce module est **indépendant** du reste du robot (voir
`ARCHITECTURE_AND_ROADMAP.md` §4.3) : pas de lien avec l'ESP32
principal, uniquement WiFi vers le Raspberry Pi une fois en service. Le
câblage ci-dessous ne concerne que la programmation/l'alimentation de
ce module lui-même.

## Programmation (flash) --- adaptateur FTDI/USB-TTL requis

La carte AI-Thinker ESP32-CAM n'a **pas** d'USB intégré (contrairement
au devkit ESP32 WROOM principal) --- un adaptateur USB-série externe
(FTDI, CP2102, CH340...) réglé sur **3.3V logique** est nécessaire pour
flasher et pour lire les logs `Serial`.

| Adaptateur USB-série | ESP32-CAM | Note |
|---|---|---|
| 5V (ou VCC selon l'adaptateur) | `5V` | Alimentation --- voir section suivante, ne pas alimenter uniquement par l'adaptateur si celui-ci ne fournit pas assez de courant |
| GND | `GND` | Masse commune, obligatoire |
| TX | `U0R` (RX de l'ESP32) | Croisé : TX↔RX |
| RX | `U0T` (TX de l'ESP32) | Croisé : RX↔TX |
| --- | `GPIO0` → `GND` | **Uniquement pendant le flash** (voir ci-dessous) |

Procédure de flash (comme tout ESP32 sans auto-reset câblé) :

1. Relier `GPIO0` à `GND` (bouton/jumper, ou fil volant) --- place la
   puce en mode téléchargement au prochain démarrage.
2. Appuyer sur le bouton `RESET` de la carte (ou débrancher/rebrancher
   l'alimentation).
3. Lancer `pio run -t upload` (ou l'upload PlatformIO habituel).
4. Une fois le flash terminé, **retirer le fil `GPIO0`→`GND`** et
   reset à nouveau --- sinon la carte redémarre en mode téléchargement
   au lieu de lancer le firmware.

## Alimentation

- **5V requis**, pas 3.3V direct sur la broche `5V` (il y a un
  régulateur onboard) --- mais le pic de courant à l'allumage de la
  caméra/WiFi peut dépasser ce qu'un adaptateur FTDI basique fournit
  sur sa broche 5V. Symptôme classique si sous-alimenté : reboot en
  boucle ou échec `camera_init_failed` intermittent. Alimentation 5V
  externe (même source que le reste du robot, ou un simple
  step-down/USB) recommandée dès que le module tourne en continu (pas
  seulement pendant le flash).
- Masse commune obligatoire entre l'adaptateur de flash, l'alimentation
  5V externe (si séparée) et l'ESP32-CAM.

## Après le premier flash réussi

Une fois le firmware `esp32-cam/` qui tourne (voir `PROGRESS.md` pour
l'état exact --- écrit et compile au 2026-09-06, **pas encore flashé**),
retirer l'adaptateur FTDI pour l'usage normal : le module n'a besoin que
du 5V/GND une fois en service, tout le reste se fait en WiFi vers le
Raspberry Pi (`ROVER_CAM_STREAM_PORT`/`ROVER_CAM_STREAM_PATH`, voir
`include/cam_config.h`).

## Sécurité

Le flux MJPEG servi par ce module (port 81, `/stream`) n'est **pas**
authentifié --- volontairement minimal, ce module ne fait que filmer
(voir `ARCHITECTURE_AND_ROADMAP.md` §4.3). Il ne doit jamais être
exposé au-delà du réseau local de confiance : c'est le Raspberry Pi
(`pi/rover_control/camera.py`, token obligatoire) qui sert de
passerelle authentifiée vers l'extérieur, jamais ce module directement.
