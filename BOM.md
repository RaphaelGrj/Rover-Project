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

## Son

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| Buzzer (piezo passif ou actif) | 1 | ✅ (déjà en stock) | Bip sonore (boot, obstacle, batterie faible, E-stop) | `esp32/lib/sound/Buzzer.h`, GPIO12 --- broche de strapping normalement évitée, réutilisée faute de GPIO libre restant (voir `WIRING.md`), à surveiller au premier boot avec le buzzer câblé |

------------------------------------------------------------------------

## Capteurs (Phase 4)

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| VL53L0X (ToF, distance) | 2 | ✅ reçu, 🛒 câblage à faire | Détection d'obstacle gauche/droite | **Doit avoir une broche XSHUT séparée et accessible** (indispensable pour l'adressage I2C séquentiel des 2 capteurs) --- pas encore câblés sur le robot physique, plan de câblage détaillé dans `PROGRESS.md` "Prochaines étapes" |
| MPU6050 (IMU 6 axes) | 1 | ✅ | Accéléromètre + gyroscope | Câblé et validé sur matériel réel (2026-09-02) --- breakout standard type GY-521, adresse `0x68`, valeurs cohérentes |
| BME280 (env. : temp/humidité/pression) | 1 | ✅ | Capteur environnemental | Le BME688 était initialement prévu (capteur de gaz en plus) mais le module reçu est en réalité un **BME280** (chip-id `0x60`, pas de capteur de gaz) --- confirmé par le lien d'achat. Câblé et validé (2026-09-02), adresse `0x76`. Le firmware détecte automatiquement BME280 vs BME680/688 par chip-id (`EnvironmentSensor.cpp`) --- un futur BME688 sera plug-and-play, aucun changement de code nécessaire |

------------------------------------------------------------------------

## Alimentation

> Solution retenue (2026-09-02) : **pack 2S1P 18650 (7.4V nominal) avec
> module BMS/charge USB-C tout-en-un**, alimente moteurs/Pi/ESP32/
> servos/capteurs sur une seule batterie, recharge directe en USB-C.
> Budget de puissance estimé : ~32W pic réaliste (Pi 3B+ ~7.5W, ESP32+
> capteurs+écran+buzzer ~2.5W, 2 servos tête ~7W, 2 moteurs N20+DRV8833
> ~15W) --- soit ~4.3A côté pack à 7.4V, dimensionné avec une marge ×2
> (BMS/cellules calibrés ~10A). Deux autres options évaluées et
> écartées pour l'instant (détail dans la conversation, à rouvrir si
> besoin) : variante cellules LiFePO4 (chimie plus sûre, un peu plus
> cher/moins dense) et powerbank scellé + boost moteur (le moins de
> travail d'assemblage, mais moins de contrôle sur le form factor).
> Architecture : rail moteurs directement sur le pack (comme
> aujourd'hui) ; **rail 5V du Pi sur un buck dédié, séparé** du rail
> servos/ESP32 --- à ne pas mutualiser, pour que les à-coups moteurs ne
> fassent pas chuter la tension du Pi (risque de brownout/corruption
> carte SD).

| Composant | Qté | Statut | Utilité | Notes |
|---|---|---|---|---|
| Alimentation de laboratoire 30V/10A | 1 | ✅ | Alimente les moteurs pendant les tests (réglée à 6-9V) | Outil de banc, ne sera pas embarqué sur le robot final |
| Cellule 18650 haut-drain (Samsung 25R/30Q ou Molicel P26A/P28A) | 2 | 🛒 | Pack batterie 2S1P (7.4V nominal) | Marque reconnue uniquement, acheté chez un vendeur fiable --- jamais de cellule "9900mAh" bon marché (capacité fantaisiste, mauvaise QC, risque incendie). ~9-13€/pièce |
| Module BMS + charge USB-C 2S (protection + boost interne 5V→8.4V) | 1 | 🛒 | Protection (surcharge/décharge/surintensité/court-circuit) + recharge directe en USB-C | Ex. "2S 5A/8A 8.4V BMS USB-C", voir sources ci-dessous pour des références précises. ~5-8€ |
| Support 2×18650 avec fils | 1 | 🛒 | Interface mécanique cellules ↔ BMS, évite la soudure directe sur les bornes des cellules | ~2-3€ |
| Fusible réarmable (PTC) 6-8A | 1 | 🛒 | Protection en ligne indépendante du BMS, dernier rempart si l'électronique de protection tombe en panne | En série sur le + du pack. ~1-2€ |
| Convertisseur buck 5V/3A dédié Pi | 1 | 🛒 | Rail 5V isolé/propre pour le Raspberry Pi (voir note ci-dessus) | Type MP1584/XL4015 réglable. ~3-5€ |
| Convertisseur buck 5V/3A servos + ESP32 | 1 | 🛒 | Rail 5V partagé pour le reste de l'électronique | Séparé du rail Pi. ~3-5€ |
| Gaine/sac ignifuge pour les cellules | 1 | 🛒 | Confinement en cas de défaillance cellule, protection mécanique contre les chocs | "LiPo safe bag" ou support imprimé en matière ignifuge. ~5-10€ |
| Câblage silicone 18-20AWG + connecteurs XT30 (détrompés) | --- | 🛒 | Câblage du rail batterie, polarité impossible à inverser | ~5€ |
| Interrupteur d'alimentation général | 1 | 🛒 | Coupure physique de l'alimentation embarquée, en amont de tout | Pas encore spécifié (calibre à choisir selon le courant total, ~10A+) |
| Bouton poussoir E-stop (NO, à GND) | 1 | 🛒 | Arrêt d'urgence matériel | Code déjà en place côté firmware (`esp32/lib/safety/EStop.h`, GPIO25) et fonctionne sans lui pour l'instant --- pas bloquant, à câbler dès réception |
| Pont diviseur de tension (2 résistances) | 1 | 🛒 | Mesure batterie via ADC | Valeurs à choisir pour la plage 6-8.4V du pack 2S ; monitoring désactivé dans le firmware tant que ce n'est pas câblé/calibré (`power_config.h`) |

**Total estimé pack + BMS + régulation : ~35-55€** (hors interrupteur/E-stop/pont diviseur déjà listés séparément).

Sources consultées pour les modules BMS/charge USB-C 2S :
- [2S 5A 8.4V BMS USB-C --- RoboComp](https://robocomp.in/product/2s-5a-8-4v-18650-lithium-battery-charger-board-protection-module/)
- [2S 20A BMS avec équilibrage --- Amazon](https://www.amazon.com/AITRIP-Lithium-Battery-Charger-Balance/dp/B08NSTF8CT)
- [Module BMS USB-C boost 2S/3S/4S --- Amazon](https://www.amazon.com/clp/B0C89Z3HFJ)
- [Type-C BMS 2S boost vers 8.4V --- Adeept](https://www.adeept.com/type-c-bms-2s-2a-18650-21700-37v-lithium-battery-charge-board-step-up-boost-li-po-polymer-usb-c-to-84v_p0374.html)
- [Discussion charge d'un pack 2S 18650 --- All About Circuits](https://forum.allaboutcircuits.com/threads/charging-circuit-for-2s-8-4v-battery-pack-2-x-18650-in-series.203337/)

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
