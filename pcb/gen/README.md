# Génération des cartes ROVER

Le schéma et le PCB des deux cartes sont **générés par script**. Pour
modifier une carte, on modifie sa description puis on régénère ; une
retouche faite à la main dans KiCad est perdue à la génération suivante.

| Fichier | Rôle |
|---|---|
| `parts.py` | Empreintes, noms de broches, connecteur inter-cartes (commun aux deux cartes) |
| `design_puissance.py` | Carte du bas : composants, positions, connexions, largeurs de pistes |
| `design_logique.py` | Carte du haut |
| `make_lib.py` | Variante locale de l'empreinte du module ESP32 (antenne au bord) |
| `gen_sch.py` | Écrit le schéma KiCad |
| `gen_pcb.py` | Place les composants, lance le routage, élargit les pistes, remplit les plans |
| `freeroute.py` | Appel du routeur automatique Freerouting |
| `widen.py` | Élargit les pistes de puissance là où la place existe |
| `check.py` | Contrôle KiCad (DRC + cohérence schéma ↔ carte) |
| `export_fab.py` | Fichiers JLCPCB (Gerber, nomenclature, placement) |

## Outils à installer une fois

Les scripts utilisent le Python fourni avec **KiCad 10** et deux outils
à télécharger dans `pcb/gen/.tools/` (dossier ignoré par git) :

| Outil | Où le prendre | Où le mettre |
|---|---|---|
| Freerouting 2.5.0 (`freerouting-2.5.0.jar`) | <https://github.com/freerouting/freerouting/releases> | `.tools/freerouting/` |
| Java 25 portable (Temurin JRE, zip Windows x64) | <https://adoptium.net> | décompressé dans `.tools/jre/` (donne `.tools/jre/jdk-25…/bin/java.exe`) |

Les chemins de KiCad et de ces outils sont ceux d'une installation
Windows ; sous Linux, adapter `FP_ROOT` (`gen_pcb.py`), `CLI`
(`check.py`, `export_fab.py`) et `find_tools()` (`freeroute.py`).

## Régénérer une carte

```
cd pcb/gen
set PY="C:\Program Files\KiCad\10.0\bin\python.exe"
%PY% make_lib.py   puissance
%PY% gen_sch.py    puissance
%PY% gen_pcb.py    puissance route
%PY% check.py      puissance
%PY% export_fab.py puissance
```

Même chose avec `logique`. Puis les contrôles croisés :

```
%PY% ..\test\check_pins.py
%PY% ..\test\check_connecteur.py
```

Résultat attendu de `check.py` : `non connectes : 0`, `ecarts schema : 0`,
et aucune ligne `clearance` ni `shorting_items`.

⚠ **Le routage n'est pas reproductible à l'identique** : deux lancements
peuvent donner des tracés différents, et il arrive qu'un lancement
laisse une liaison non routée. Toujours relancer `check.py` après
`gen_pcb.py … route`, et ne régénérer les fichiers de fabrication que si
le contrôle est propre.
