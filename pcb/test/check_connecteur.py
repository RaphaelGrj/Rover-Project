# -*- coding: utf-8 -*-
"""
Contrôle géométrique du connecteur inter-cartes, sur les fichiers KiCad réels.

Les deux cartes se superposent : chaque broche de l'embase (carte puissance)
doit se trouver exactement sous la broche de la barrette (carte logique) qui
porte le même net. Ce test lit les deux .kicad_pcb et compare, broche par
broche, position X/Y et nom de net. Il vérifie aussi que les 4 trous de
fixation sont aux mêmes endroits.

Usage (Python fourni avec KiCad, car il faut le module pcbnew) :
    "C:\Program Files\KiCad\10.0\bin\python.exe" pcb\test\check_connecteur.py
"""
import os
import sys
import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))


def load(name):
    board = pcbnew.LoadBoard(os.path.join(HERE, "..", name, name + ".kicad_pcb"))
    conn, holes = {}, set()
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        for pad in fp.Pads():
            xy = (round(pcbnew.ToMM(pad.GetPosition().x), 2), round(pcbnew.ToMM(pad.GetPosition().y), 2))
            if ref == "J20":
                conn[xy] = str(pad.GetNetname())
            elif ref.startswith("H") and ref[1:].isdigit():
                holes.add(xy)
    return conn, holes


def main():
    (ca, ha), (cb, hb) = load("rover_puissance"), load("rover_logique")
    errors = []
    if set(ca) != set(cb):
        errors.append("positions differentes : %s" % sorted(set(ca) ^ set(cb))[:6])
    for xy in sorted(set(ca) & set(cb)):
        if ca[xy] != cb[xy]:
            errors.append("broche en %s : puissance '%s', logique '%s'" % (xy, ca[xy], cb[xy]))
    if ha != hb:
        errors.append("trous de fixation differents : %s / %s" % (sorted(ha), sorted(hb)))
    print("%d broches comparees, %d trous, %d ecart(s)" % (len(set(ca) & set(cb)), len(ha), len(errors)), flush=True)
    for e in errors:
        print("  ECART :", e, flush=True)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
