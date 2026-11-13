# -*- coding: utf-8 -*-
"""
Routage par Freerouting (routeur automatique libre, https://github.com/freerouting/freerouting).

Chaîne : carte placée (pcbnew) -> export Specctra .dsn -> Freerouting -> import
de la session .ses -> remplissage des plans. Les largeurs de pistes viennent des
classes de nets définies ici (Freerouting les lit dans le .dsn).

Outils attendus dans pcb/gen/.tools/ (non commités, voir pcb/README.md) :
    freerouting/freerouting-*.jar      jre/jdk-25*/bin/java.exe
"""
import glob
import os
import subprocess
import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, ".tools")
mm = pcbnew.FromMM

# Classes de nets : (largeur de piste, isolement, diamètre de via, perçage), en mm.
CLASSES = {
    # Freerouting garde la largeur de piste jusqu'à la pastille : chaque largeur est donc
    # la plus grande qui passe encore sur les broches que ces nets desservent.
    "Batterie": (0.8, 0.15, 0.8, 0.4),    # chemin du courant batterie (pics ~3 A, pistes courtes)
    "Rail": (0.5, 0.15, 0.8, 0.4),        # rails 5 V / USB (~2 A)
    "Puissance": (0.3, 0.15, 0.6, 0.3),   # ~1 A ; passe entre des broches au pas de 0,65 mm
    "Moyen": (0.25, 0.15, 0.6, 0.3),      # tronçons courts sur broches au pas de 0,5 mm
    "Default": (0.15, 0.15, 0.6, 0.3),
}


def find_tools():
    jars = sorted(glob.glob(os.path.join(TOOLS, "freerouting", "freerouting-*.jar")))
    javas = sorted(glob.glob(os.path.join(TOOLS, "jre", "jdk-25*", "bin", "java.exe")))
    if not jars or not javas:
        raise SystemExit("Freerouting ou Java introuvable dans %s (voir pcb/README.md)" % TOOLS)
    return javas[-1], jars[-1]


def apply_netclasses(board, assignment, clearance=None):
    """assignment : {nom de net: nom de classe}. Les autres nets restent en 'Default'.
    `clearance` force l'isolement de toutes les classes (sinon celui de CLASSES)."""
    ds = board.GetDesignSettings()
    ns = ds.m_NetSettings
    for name, (w, clr, vd, vdr) in CLASSES.items():
        nc = ns.GetDefaultNetclass() if name == "Default" else pcbnew.NETCLASS(name)
        nc.SetTrackWidth(mm(w))
        nc.SetClearance(mm(clearance if clearance is not None else clr))
        nc.SetViaDiameter(mm(vd))
        nc.SetViaDrill(mm(vdr))
        if name != "Default":
            ns.SetNetclass(name, nc)
    for net, cls in assignment.items():
        ns.SetNetclassPatternAssignment(net, cls)
    board.SynchronizeNetsAndNetClasses(True)


def route(board, pcb_path, passes=40, timeout_s=3000):
    """Route `board` (déjà sauvegardée dans pcb_path) et importe le résultat dedans."""
    java, jar = find_tools()
    base = os.path.splitext(pcb_path)[0]
    dsn, ses = base + ".dsn", base + ".ses"
    for f in (dsn, ses):
        if os.path.exists(f):
            os.remove(f)
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        raise SystemExit("export DSN impossible")
    cmd = [java, "-jar", jar, "-de", dsn, "-do", ses, "-mp", str(passes), "-l", "en"]
    log = base + ".freerouting.log"
    with open(log, "w", encoding="utf8", errors="replace") as f:
        r = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, timeout=timeout_s)
    if not os.path.exists(ses):
        raise SystemExit("Freerouting n'a pas produit de session (code %s), voir %s" % (r.returncode, log))
    if not pcbnew.ImportSpecctraSES(board, ses):
        raise SystemExit("import de la session Freerouting impossible")
    return log
