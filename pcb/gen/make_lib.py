# -*- coding: utf-8 -*-
"""
Crée la bibliothèque locale rover.pretty avec une variante de l'empreinte
ESP32-S3-WROOM-1 adaptée à un montage "antenne au bord de la carte".

L'empreinte standard embarque des zones interdites et un courtyard de
48 x 41 mm (dégagement d'antenne idéal d'Espressif), impossibles à respecter
sur une carte de 50 mm de large. On les retire et on dessine un courtyard égal
au boîtier ; l'absence de cuivre sous l'antenne est garantie par le routeur.
"""
import io
import os
import board_select  # noqa: F401
import design as D

SRC = r"C:\Program Files\KiCad\10.0\share\kicad\footprints\RF_Module.pretty\ESP32-S3-WROOM-1.kicad_mod"
HERE = os.path.dirname(os.path.abspath(__file__))
LIB = os.path.join(HERE, "..", D.NAME, "rover.pretty")
NAME = "ESP32-S3-WROOM-1_bord"


def children(text):
    """Découpe le contenu d'une S-expression en (début, fin) de chaque enfant de 1er niveau."""
    out, depth, start, in_str, i = [], 0, None, False, 0
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
            if depth == 2:
                start = i
        elif c == ")":
            if depth == 2:
                out.append((start, i + 1))
            depth -= 1
        i += 1
    return out


def main():
    text = io.open(SRC, encoding="utf8").read()
    keep, last = [], 0
    for a, b in children(text):
        block = text[a:b]
        drop = block.startswith("(zone") or (block.startswith("(fp_") and '(layer "F.CrtYd")' in block)
        if drop:
            keep.append(text[last:a].rstrip(" \t"))
            last = b
    keep.append(text[last:])
    body = "".join(keep)
    body = body.replace('(footprint "ESP32-S3-WROOM-1"', '(footprint "%s"' % NAME, 1)
    crt = ('\t(fp_rect\n\t\t(start -9.25 -13.25)\n\t\t(end 9.25 12.75)\n'
           '\t\t(stroke\n\t\t\t(width 0.05)\n\t\t\t(type solid)\n\t\t)\n\t\t(fill no)\n'
           '\t\t(layer "F.CrtYd")\n\t)\n')
    end = body.rstrip().rfind(")")
    body = body[:end] + crt + ")\n"
    os.makedirs(LIB, exist_ok=True)
    io.open(os.path.join(LIB, NAME + ".kicad_mod"), "w", encoding="utf8", newline="\n").write(body)
    print("empreinte locale ecrite :", NAME)


if __name__ == "__main__":
    main()
