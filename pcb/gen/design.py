# -*- coding: utf-8 -*-
"""
Aiguillage : `import design as D` donne la description de la carte choisie.

La carte ("puissance" ou "logique") est lue sur la ligne de commande par
board_select.py, que chaque script importe en premier.
"""
import os

_board = os.environ.get("ROVER_BOARD")
if _board == "puissance":
    from design_puissance import *      # noqa: F401,F403
elif _board == "logique":
    from design_logique import *        # noqa: F401,F403
else:
    raise SystemExit("Carte non choisie : importer board_select avant design")
