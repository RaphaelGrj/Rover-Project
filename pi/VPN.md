# Accès distant sécurisé (VPN)

> Complémentaire à `pi/README.md` "Sécurité". Dernière pièce ouverte de
> la Phase 6 (`ARCHITECTURE_AND_ROADMAP.md`) --- piloter Rover depuis
> l'extérieur du réseau local, sans exposer directement le serveur de
> contrôle sur Internet.

------------------------------------------------------------------------

## Pourquoi WireGuard

- Léger, dans le noyau Linux (Raspberry Pi OS), pas de dépendance
  lourde --- cohérent avec `ARCHITECTURE_AND_ROADMAP.md` §27.10.
- Auto-hébergé : contrairement à un service tiers, aucune donnée ne
  transite par un serveur externe. Cohérent avec l'exigence "pas de
  fuite de données" de ce projet open source.
- Un seul port UDP à ouvrir sur la box, jamais le port HTTP/HTTPS de
  `rover_control` lui-même.

**Alternative plus simple à mettre en place** si la gestion manuelle du
port-forwarding ne te tente pas : [Tailscale](https://tailscale.com/)
fait la même chose (WireGuard en dessous) sans configuration réseau
manuelle, au prix d'un compte chez un tiers pour la coordination
initiale (le trafic lui-même reste chiffré de bout en bout). Non
retenu ici par défaut pour rester 100% auto-hébergé, mais une option
raisonnable si tu préfères la simplicité.

## Comment ça s'intègre à ce qui existe déjà

**Aucun changement dans `rover_core`/`rover_control`.** Une fois un
appareil connecté au VPN, il obtient une adresse à l'intérieur du
tunnel (ex. `10.66.0.2`) et peut joindre le Pi à son adresse de tunnel
(`10.66.0.1`) exactement comme s'il était sur le réseau local. Le token
d'accès (`ROVER_CONTROL_TOKEN`, `pi/README.md`) reste **obligatoire**
--- le VPN règle l'accessibilité réseau, pas l'authentification
applicative ; les deux se cumulent (défense en profondeur), l'un ne
remplace pas l'autre.

------------------------------------------------------------------------

## Installation (sur le Raspberry Pi)

```bash
sudo apt update && sudo apt install wireguard
```

### 1. Générer les clés

Une paire de clés par appareil (le Pi, et un jeu séparé par client
distant) :

```bash
wg genkey | tee privatekey | wg pubkey > publickey
```

Ne partage jamais une clé privée. Garde `privatekey` uniquement sur
l'appareil qui l'utilise.

### 2. Configurer le serveur (Pi)

```bash
cp pi/wireguard/wg0.example.conf pi/wireguard/wg0.conf
```

Éditer `pi/wireguard/wg0.conf` (git-ignoré, jamais commité) :
`PrivateKey` = la clé privée générée pour le Pi, `PublicKey` (dans le
bloc `[Peer]`) = la clé publique du client. Puis installer et démarrer :

```bash
sudo cp pi/wireguard/wg0.conf /etc/wireguard/wg0.conf
sudo systemctl enable --now wg-quick@wg0
```

Pas besoin d'activer l'IP forwarding (`net.ipv4.ip_forward`) --- ce
tunnel sert uniquement à joindre le Pi lui-même (`rover_control`), pas
à router vers le reste du réseau local ou vers Internet à travers lui.
Une surface d'exposition plus petite si un appareil client est un jour
perdu ou compromis.

### 3. Configurer le client (téléphone/ordinateur distant)

```bash
cp pi/wireguard/client.example.conf pi/wireguard/client.conf
```

Éditer `client.conf` : `PrivateKey` = clé privée du client, `PublicKey`
= clé publique du Pi, `Endpoint` = l'IP ou le nom de domaine public de
ta box. Importer ce fichier dans l'app WireGuard officielle (Android/
iOS/Windows/macOS/Linux) et se connecter.

### 4. Router / box

Rediriger le port **UDP 51820** (ou celui choisi dans `ListenPort`) vers
l'IP locale du Pi. **Ne jamais** rediriger le port HTTP/HTTPS de
`rover_control` (8080 par défaut) directement --- lui doit rester
inaccessible depuis Internet, seul le tunnel VPN y donne accès.

Si ta box n'a pas d'IP publique fixe, un service de DNS dynamique
(DuckDNS, No-IP, ou celui fourni par ta box) résout le problème pour
`Endpoint` dans `client.conf`.

### 5. Utiliser

Une fois connecté au VPN, ouvrir depuis l'appareil distant :

```
http://10.66.0.1:8080/?token=...
```

(ou `https://` si HTTPS/WSS est configuré, voir `pi/README.md`) --- le
même token que d'habitude, la même page de contrôle.

------------------------------------------------------------------------

## Sécurité

- Une paire de clés par appareil, jamais partagée entre plusieurs
  appareils --- révoquer l'accès d'un appareil perdu/compromis, c'est
  juste supprimer son bloc `[Peer]` dans `wg0.conf` et relancer
  `sudo systemctl restart wg-quick@wg0`.
- `pi/wireguard/wg0.conf` et `client.conf` contiennent des clés privées
  --- ignorés par git (`.gitignore`), ne jamais les committer une fois
  remplis avec de vraies valeurs. Seuls les `.example.conf` sont
  suivis.
- `AllowedIPs` volontairement restreint au sous-réseau du tunnel
  (`10.66.0.0/24`), jamais `0.0.0.0/0` --- un client distant n'a besoin
  de joindre que le Pi, pas d'être routé à travers lui vers autre chose.
