# Rover-Project
Coming soon

🤖 PROJECT ROVER | by R-BotL'évolution mobile de LUMI. Un compagnon robotique autonome, intelligent, et intégré à l'écosystème domotique, arborant la signature esthétique "Glitch" de R-Bot.

📖 Présentation du Projet 

Project Rover est le successeur spirituel de LUMI. Là où LUMI observait depuis son bureau, Rover explore. Fortement inspiré par la mécanique et l'expressivité du robot Vector (Anki), Rover va plus loin en devenant un véritable assistant domotique mobile.

Il ne s'agit pas d'un simple jouet télécommandé, mais d'une machine à émotions capable d'interagir naturellement avec son environnement et son humain. Conçu autour d'une architecture hybride (ESP32-S3 + Raspberry Pi), Rover surveille le logement, analyse l'air, interagit via une IA générative, et patrouille tout en gardant une direction artistique unique : un design "Vector Glitché".

🎯 Cahier des Charges & Fonctionnalités

  🧠 Machine à Émotions & IA
Personnalité Dynamique : Humeurs variables selon le contexte (météo, niveau de batterie, interactions récentes).
Expressivité Physique : Affichage d'animations sur écran IPS (yeux type Vector) combiné à des mouvements de tête fluides (Pitch/Yaw) gérés par servomoteurs.
Cerveau IA : Intégration d'une IA conversationnelle pour répondre aux requêtes de manière naturelle (évolution du système embarqué sur LUMI).

  🛞 Mobilité & NavigationDéplacement Autonome : 
  Exploration libre du logement avec évitement d'obstacles.
  Pilotage Manuel : Prise de contrôle à distance via smartphone ou télécommande (avec retour vidéo direct).
  
  -[À l'étude] Cartographie & Patrouille : Algorithme SLAM pour générer une carte de la maison, avec des rondes de sécurité programmables.-
  
  👁️ Perception & VisionReconnaissance Visuelle : 
  Capacité à identifier son humain (reconnaissance faciale), à détecter des objets, des présences inconnues, ou même des animaux (idéal pour surveiller si votre compagnon fait des bêtises en votre absence).
  Surveillance Distante : Flux caméra accessible hors du domicile pour lever le doute lors d'une alerte.
  
  🏠 Hub Domotique & Capteurs (Home Assistant)
  Lien Home Assistant : Intégration native via MQTT/API. Rover peut déclencher des scènes ou annoncer des notifications de la maison.Bilan Météo Local : Station météo sur chenilles. 
  Analyse en temps réel de la température, de l'humidité et de la pression.
  -[À l'étude] Sécurité Environnementale : Détection de COV (Composés Organiques Volatils), de fumée et de qualité de l'air. Rover alerte en cas de risque d'incendie ou de dégradation de l'air intérieur.-
  
  🎨 Direction Artistique (DA)Design R-Bot "Glitch" : 
  Esthétique asymétrique, textures corrompues ou détails cyberpunk. Modélisation complète sous SolidWorks, optimisée pour un assemblage mêlant l'endurance du FDM (châssis) et la précision de la résine (coque/visage).
  
  ⚙️ Architecture Matérielle (BOM - Bill of Materials)
L'architecture est divisée en deux couches pour garantir fluidité et puissance de calcul.

Cerveaux & LogiqueComposantRôleESP32-S3Microcontrôleur temps réel : gestion des moteurs, servos, capteurs I2C et écran SPI.Raspberry Pi Zero 2 WOrdinateur de bord : Serveur vidéo, traitement IA lourde, SLAM, lien Home Assistant.Locomotion & MouvementComposantRôle2x Moteurs DC N20 (avec encodeurs)Propulsion précise des chenilles ou roues.1x Driver Moteur (TB6612FNG)Contrôle de puissance des moteurs.2x Micro Servomoteurs (SG90 ou MG90S)Mouvement de la tête (Axe X et Y) pour l'expressivité.Perception & CapteursComposantRôleModule Caméra (Pi Camera V2 ou OV2640)Vision, reconnaissance faciale, retour vidéo.MPU6050 (IMU)Accéléromètre & Gyroscope (équilibre, détection de chute ou si on le soulève).Capteur ToF (VL53L0X) x2Mesure de distance laser précise (évitement d'obstacles sans contact).BME688 (Bosch)Capteur 4-en-1 : Température, Humidité, Pression barométrique et Gaz/COV (idéal pour le bilan météo et risque incendie).Interface Humain-Machine (IHM)ComposantRôleÉcran IPS (ex: ST7789 1.54")Affichage des yeux, des animations de glitch et des humeurs.Micro I2S (INMP441)Écoute de l'environnement et des commandes vocales.Ampli Audio (MAX98357A) + Haut-parleurSynthèse vocale et bruitages émotionnels.ÉnergieComposantRôle2x Accus 18650 (Li-ion)Autonomie énergétique.Module BMS / TP4056 + Step-down (LM2596)Charge sécurisée et régulation 5V/3.3V pour alimenter l'ESP et le Pi.


🚀 Roadmap du Développement (Prévisions)

[ ] Phase 1 : Preuve de concept (Châssis & Écran)

Conception SolidWorks de la base roulante.

Test de motorisation avec l'ESP32.

Portage des animations d'yeux depuis le projet LUMI.

[ ] Phase 2 : Architecture Hybride

Communication série/I2C entre le Pi Zero et l'ESP32-S3.

Intégration du flux vidéo et pilotage distant.

[ ] Phase 3 : Écosystème & IA

Connexion à Home Assistant.

Câblage et programmation des capteurs environnementaux (BME688).

Implémentation du modèle IA conversationnel.

[ ] Phase 4 : Les fonctions "À l'étude"

Déploiement d'un nœud ROS ou OpenCV pour le mode patrouille et le mapping.

Reconnaissance faciale et animale en local.

Projet développé par R-Bot. "Glitch the system, but make it cute."
