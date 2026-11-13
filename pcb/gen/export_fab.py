# -*- coding: utf-8 -*-
"""
Prépare le dossier ../<carte>/fabrication pour une commande JLCPCB :

  <carte>-gerber.zip   Gerber + perçages (à déposer sur jlcpcb.com)
  <carte>-bom.csv      nomenclature au format JLCPCB (Comment, Designator, Footprint, LCSC Part #)
  <carte>-cpl.csv      placement au format JLCPCB (Designator, Mid X, Mid Y, Layer, Rotation)

Usage :  python export_fab.py puissance     puis     python export_fab.py logique

Les références LCSC viennent de AIDE_CABLAGE.md (relevé du 2026-10-13). Les
passifs courants (résistances, condensateurs, LED...) n'en ont volontairement
pas : JLCPCB propose une pièce "Basic" à partir de la valeur et du boîtier.
"""
import csv
import io
import os
import shutil
import subprocess
import zipfile
import board_select  # noqa: F401  (doit précéder l'import de design)
import design as D

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", D.NAME)
FAB = os.path.join(OUT, "fabrication")
PCB = os.path.join(OUT, D.NAME + ".kicad_pcb")
CLI = r"C:\Program Files\KiCad\10.0\bin\kicad-cli.exe"
LAYERS = "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts"
# Empreintes sans composant à poser (trous, pastilles de test, cavalier de soudure).
NOT_ASSEMBLED = ("HOLE", "TP", "JP")


def run(*args):
    r = subprocess.run([CLI] + list(args), capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("echec kicad-cli %s : %s %s" % (args[:3], r.stdout, r.stderr))


def main():
    gerber = os.path.join(FAB, "gerber")
    if os.path.isdir(FAB):
        shutil.rmtree(FAB)
    os.makedirs(gerber)
    run("pcb", "export", "gerbers", "--layers", LAYERS, "--no-protel-ext", "-o", gerber + os.sep, PCB)
    run("pcb", "export", "drill", "--format", "excellon", "--excellon-separate-th", "-o", gerber + os.sep, PCB)
    with zipfile.ZipFile(os.path.join(FAB, D.NAME + "-gerber.zip"), "w", zipfile.ZIP_DEFLATED) as z:
        for name in sorted(os.listdir(gerber)):
            z.write(os.path.join(gerber, name), name)
    n_files = len(os.listdir(gerber))
    shutil.rmtree(gerber)   # seul le zip sert à la commande

    # Placement : export KiCad puis mise au format JLCPCB.
    tmp = os.path.join(FAB, "_pos.csv")
    run("pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both", "--exclude-dnp", "-o", tmp, PCB)
    rows = list(csv.DictReader(io.open(tmp, encoding="utf8")))
    os.remove(tmp)
    assembled = {p["ref"] for p in D.PARTS if p["kind"] not in NOT_ASSEMBLED and not p["dnp"]}
    n_cpl = 0
    with io.open(os.path.join(FAB, D.NAME + "-cpl.csv"), "w", encoding="utf8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for r in rows:
            if r["Ref"] in assembled:
                w.writerow([r["Ref"], r["PosX"] + "mm", r["PosY"] + "mm",
                            "Top" if r["Side"].lower().startswith("top") else "Bottom", r["Rot"]])
                n_cpl += 1

    groups = {}
    for p in D.PARTS:
        if p["ref"] in assembled:
            groups.setdefault((p["value"], p["fp"].split(":")[1], p["lcsc"]), []).append(p["ref"])
    with io.open(os.path.join(FAB, D.NAME + "-bom.csv"), "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (value, fp, lcsc), refs in sorted(groups.items(), key=lambda kv: kv[1][0]):
            w.writerow([value, ",".join(refs), fp, lcsc])
    missing = sorted(assembled - {r["Ref"] for r in rows})
    print("%s : %d fichiers dans le zip, %d lignes de nomenclature, %d composants a poser (%d attendus)"
          % (D.NAME, n_files, len(groups), n_cpl, len(assembled)))
    if missing:
        print("ATTENTION, absents du fichier de placement :", missing)


if __name__ == "__main__":
    main()
