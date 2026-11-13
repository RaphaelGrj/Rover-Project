# -*- coding: utf-8 -*-
"""À importer en premier dans chaque script : choisit la carte ("puissance" ou
"logique") d'après la ligne de commande, AVANT que `design` ne soit importé."""
import os
import sys

BOARDS = ("puissance", "logique")


def select():
    for a in list(sys.argv[1:]):
        if a in BOARDS:
            os.environ["ROVER_BOARD"] = a
            sys.argv.remove(a)
            return a
    if os.environ.get("ROVER_BOARD") in BOARDS:
        return os.environ["ROVER_BOARD"]
    raise SystemExit("Préciser la carte : " + " ou ".join(BOARDS))


BOARD = select()
