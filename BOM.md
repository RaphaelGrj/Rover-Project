# ROVER --- Liste des composants (BOM)

> Bill of Materials du projet Rover. Complémentaire à
> `ARCHITECTURE_AND_ROADMAP.md` (propriétaire matériel de chaque
> périphérique, §5) et `esp32/WIRING.md`/`AIDE_CABLAGE.md` (câblage
> détaillé). Mis à jour au fil des commandes/réceptions --- voir
> `PROGRESS.md` pour l'historique.
>
> Statuts : ✅ reçu --- 🛒 à commander --- ❓ à définir (dépend d'une
> décision ou d'une info pas encore fixée).

------------------------------------------------------------------------

## Calcul / contrôle

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| ESP32 WROOM (devkit 30 broches) | 1 | ✅ | Contrôleur temps réel (moteurs, servos, écran, capteurs, sécurité) | Firmware compatible ESP32-S3 pour évolution future, voir `CLAUDE.md` |
| Raspberry Pi 3B+ (ou plus récent) | 1 | ❓ | "Cerveau" --- IA, réseau, contrôle distant, futur : caméra/vision/HA | Cible architecturale (§4.1) ; pas confirmé reçu à ce jour, le code Pi n'a été testé que sur machine de dev (voir `pi/README.md`) |
| Carte microSD (16-32 Go, classe 10+) | 1 | 🛒 | Stockage OS + code du Raspberry Pi | Nécessaire dès que le Pi est disponible |
| Câble USB (ESP32 ↔ Pi ou PC) | 1 | ✅ (via PC dev) | Flash + alimentation ESP32 en test | Pour le robot final, liaison UART directe Pi↔ESP32 envisagée (voir `WIRING.md` §UART2), pas de composant supplémentaire nécessaire (les deux cartes sont en logique 3,3V) |

------------------------------------------------------------------------

## Motorisation (Phase 2)

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| Moteur N20 6V à encodeur quadrature (6 fils) | 2 | ✅ | Un par côté (gauche/droite), chacun entraîne 2 roues/pignons via un axe commun --- confirmé par l'utilisateur, correspond au firmware actuel (2 moteurs, pas 4) | Pignon/sprocket de sortie 32mm de diamètre (confirmé 2026-08-29), voir `motion_config.h` |
| Driver moteur DRV8833 (double pont en H) | 1 | ✅ | Pilote les 2 moteurs (PWM direct sur IN1-4) | Pins réelles confirmées (`IN1-4`/`GND`/`VCC`/`SLEEP`/`OUT1-4`/`FAULT`), voir `AIDE_CABLAGE.md` |
| Pignons/roues motrices 32mm | 4 | ❓ | Entraînent les chenilles | Probablement issus de l'impression 3D du châssis (CAO proto terminée) --- à confirmer si fournis avec le châssis ou à acheter séparément |
| Chenilles (courroie/tread) | 2 | ❓ | Une par côté, autour des pignons | Type non déterminé (chenille caoutchouc dédiée, courroie crantée détournée, ou impression 3D) --- à définir selon la CAO |
| Roues folles / galets tendeurs (si présents sur la CAO) | ❓ selon CAO | ❓ | Guident/tendent la chenille sans être motorisés | Dépend entièrement du design du châssis, pas encore précisé |

------------------------------------------------------------------------

## Tête (Phase 3)

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| Servomoteur standard 5V (PWM) | 2 | 🛒 | Pitch + Yaw de la tête | Type/couple pas encore fixé (ex. SG90 pour léger, MG90S métal si plus de charge) --- dépend du poids de la tête (écran + support) une fois la CAO connue. GPIO déjà réservés (`head_config.h` : GPIO13/19) |

------------------------------------------------------------------------

## Écran (Phase 3)

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| Écran ST7789 (SPI) | 1 | 🛒 | Affichage des yeux/émotions | Résolution à confirmer à l'achat --- le firmware suppose 240×240 (calé sur le chip simulé Wokwi), Lumi utilisait du 240×280 ; si le module acheté diffère, `display_config.h` devra être ajusté (une seule constante à changer) |

------------------------------------------------------------------------

## Capteurs (Phase 4)

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| VL53L0X (ToF, distance) | 2 | 🛒 (en cours) | Détection d'obstacle gauche/droite | **Doit avoir une broche XSHUT séparée et accessible** (indispensable pour l'adressage I2C séquentiel des 2 capteurs) --- voir liste de course détaillée envoyée précédemment dans la conversation |
| MPU6050 (IMU 6 axes) | 1 | 🛒 | Accéléromètre + gyroscope | Breakout standard type GY-521, I2C, rien de spécial (pas de XSHUT) |
| BME688 (env. : temp/humidité/pression/gaz) | 1 | 🛒 | Capteur environnemental | Bien vérifier que c'est un BME688 (ou BME680, registre-compatible) et pas un BME280 qui ressemble mais n'a pas le capteur de gaz. Adresse I2C par défaut attendue : 0x77 --- si le module utilisé en a une autre (cavalier SDO → 0x76), le signaler pour ajuster le firmware |

------------------------------------------------------------------------

## Alimentation

> Non conçue à ce stade (`ARCHITECTURE_AND_ROADMAP.md` §19, "Gestion de
> l'énergie" --- aucune case cochée). Tout ce qui suit est utilisé pour
> le développement/test, pas encore pour un robot autonome sur
> batterie.

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| Alimentation de laboratoire 30V/10A | 1 | ✅ | Alimente les moteurs pendant les tests (réglée à 6V) | Outil de banc, ne sera pas embarqué sur le robot final |
| Batterie embarquée (LiPo/Li-ion) | 1 pack | ❓ | Alimentation autonome du robot fini | Chimie/capacité/tension pas encore choisies --- dépend de l'autonomie voulue et du budget de poids du châssis |
| Régulateur(s) de tension (vers 5V Pi, vers 6V moteurs) | ❓ | ❓ | Adapter la tension batterie aux besoins de chaque sous-système | Dépend de la tension de la batterie choisie ci-dessus |
| Interrupteur d'alimentation général | 1 | ❓ | Coupure physique de l'alimentation embarquée | Pas encore spécifié |
| Bouton poussoir E-stop (NO, à GND) | 1 | 🛒 | Arrêt d'urgence matériel | Code déjà en place côté firmware (`esp32/lib/safety/EStop.h`, GPIO25) et fonctionne sans lui pour l'instant --- pas bloquant, à câbler dès réception |
| Pont diviseur de tension (2 résistances) | 1 | 🛒 | Mesure batterie via ADC | Valeurs exactes à choisir selon la tension de la batterie retenue ci-dessus ; monitoring désactivé dans le firmware tant que ce n'est pas câblé/calibré (`power_config.h`) |

------------------------------------------------------------------------

## Câblage / connectique générale

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| Fils Dupont (M-M, M-F, F-F), assortiment | 1 kit | ❓ | Câblage général ESP32 ↔ modules | Quantité selon ce qui est déjà dans le tiroir --- à vérifier avant de commander |
| Bornier/domino ou connecteurs pour l'alim moteur | quelques | ❓ | Point de jonction masse commune DRV8833/ESP32/alim (voir `AIDE_CABLAGE.md`) | Optionnel si soudure directe |
| Résistances pull-up I2C 4,7 kΩ | 2 (si besoin) | ❓ | Bus I2C partagé (IMU/ToF×2/BME688) | À vérifier seulement si les 4 modules combinés n'ont pas déjà assez de pull-up embarqués (la plupart des breakouts en ont) |

------------------------------------------------------------------------

## Hors périmètre pour l'instant (phases ultérieures)

Ne pas commander maintenant --- listé pour mémoire, phases pas encore
commencées :

- Caméra (Phase 6) --- module Pi Camera ou webcam USB, non décidé.
  Piste envisagée : mini-module type Arducam "spy camera" (capteur
  OV5647, ~6mm de large, câble plat déporté) pour le contraste de place
  dans la tête (voir PROGRESS.md 2026-08-31). Logiciel déjà prêt côté
  Pi (`pi/rover_control/camera.py`, via `picamera2`) --- s'active tout
  seul dès qu'un module est branché, aucun changement de code attendu.
- Micro/haut-parleur (Phase 7, audio) --- non décidé.
- Tout composant Home Assistant/domotique (Phase 10).

------------------------------------------------------------------------

## Notes de fin

Ce fichier est une checklist d'achat, pas une nomenclature figée ---
mettre à jour les statuts (✅/🛒/❓) et ajouter des lignes au fur et à
mesure des décisions et réceptions, comme pour `AIDE_CABLAGE.md`.
