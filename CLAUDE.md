# Contexte du projet
Ce dépôt contient le code du projet ROVER. 
L'environnement de développement est sous Linux Mint. Le robot actuel utilise un ESP32 WROOM (le Raspberry Pi assurant les tâches lourdes, l'ESP32 reste peu chargé), mais le firmware doit rester compatible avec un ESP32-S3 pour une évolution future.

## Architecture & Technologies
- **Langages :** C/C++ pour l'embarqué.
- **Framework :** PlatformIO / Arduino.
- **Portabilité matérielle :** le code ESP32 doit être écrit pour cibler à la fois WROOM et S3 (environnements PlatformIO séparés, pas de dépendance à des périphériques spécifiques au S3 comme USB natif/PSRAM sauf si isolée derrière une abstraction/#ifdef).

## Instructions strictes pour la génération de code
- Ne génère jamais de code impliquant des bibliothèques exclusives à Windows.
- Garde une indentation à 4 espaces et nomme les variables en anglais.
- Gère systématiquement les erreurs de communication (I2C, SPI, Serial).
- Avant d'écrire ou de modifier massivement un fichier, explique brièvement ta logique.
- Tout code écrit pour le projet doit être commenté : pas besoin de commenter chaque ligne, mais il faut suffisamment d'informations (pourquoi, pas juste quoi) pour s'y retrouver facilement sans relire tout l'historique.