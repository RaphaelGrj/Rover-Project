# -*- coding: utf-8 -*-
"""Lance le DRC KiCad sur la carte générée et résume les violations.

Usage : python check.py [nombre_max_de_lignes]
Les avertissements de sérigraphie (purement esthétiques) sont comptés mais pas
détaillés.
"""
import board_select  # noqa: F401
import collections
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
import design as D  # noqa: E402
OUT = os.path.join(HERE, "..", D.NAME)
CLI = r"C:\Program Files\KiCad\10.0\bin\kicad-cli.exe"


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    rep = os.path.join(OUT, "drc.json")
    r = subprocess.run([CLI, "pcb", "drc", "--severity-all", "--schematic-parity", "--format", "json", "-o", rep,
                        os.path.join(OUT, D.NAME + ".kicad_pcb")], capture_output=True, text=True)
    if not os.path.exists(rep):
        print("DRC impossible :", r.stdout, r.stderr)
        return 1
    d = json.load(open(rep, encoding="utf8"))
    c = collections.Counter(v["type"] for v in d["violations"])
    unconnected = d.get("unconnected_items", [])
    print(D.NAME, dict(c), "| non connectes :", len(unconnected), "| ecarts schema :", len(d.get("schematic_parity", [])))
    n = 0
    for v in d["violations"]:
        if v["type"].startswith("silk"):
            continue
        n += 1
        if n <= limit:
            print(v["type"], [(i["description"][:55], round(i["pos"]["x"], 2), round(i["pos"]["y"], 2))
                              for i in v["items"]])
    if len(sys.argv) > 2:
        for u in unconnected[:limit]:
            print("NC", [(i["description"][:55], round(i["pos"]["x"], 2), round(i["pos"]["y"], 2))
                         for i in u["items"]])
    return 0


if __name__ == "__main__":
    sys.exit(main())
