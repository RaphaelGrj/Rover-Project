# -*- coding: utf-8 -*-
"""
Construit le PCB KiCad d'une des deux cartes à partir de sa description
(design_puissance.py / design_logique.py), puis le route avec Freerouting.

Usage (Python fourni avec KiCad) :
    python gen_pcb.py puissance place     -> carte placée, non routée
    python gen_pcb.py puissance route     -> carte placée + routée + plans remplis
    python gen_pcb.py logique route
La carte est écrite dans ../<nom>/<nom>.kicad_pcb (ex. ../rover_logique/).
"""
import os
import sys
import board_select  # noqa: F401  (doit précéder l'import de design)
import pcbnew
import design as D
import gen_sch
import freeroute
import widen

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, "..", D.NAME)
OUT_PCB = os.path.join(OUT_DIR, D.NAME + ".kicad_pcb")
FP_ROOT = r"C:\Program Files\KiCad\10.0\share\kicad\footprints"
LAYER = {"In1": pcbnew.In1_Cu, "In2": pcbnew.In2_Cu}

mm = pcbnew.FromMM


def pt(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


def build_board():
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(4)
    ds = board.GetDesignSettings()
    # Règles compatibles JLCPCB 4 couches (capacité réelle : 0,09 mm), avec marge.
    # 0,09 mm : Freerouting rétrécit la piste à l'arrivée sur les pastilles les plus fines.
    ds.m_TrackMinWidth = mm(0.09)
    ds.m_MinClearance = mm(0.15)
    ds.m_ViasMinSize = mm(0.45)
    ds.m_MinThroughDrill = mm(0.2)
    ds.m_CopperEdgeClearance = mm(0.25)
    ds.m_HoleToHoleMin = mm(0.25)
    ds.m_HoleClearance = mm(0.2)

    # Contour de la carte (polygone fermé, voir OUTLINE dans la description de la carte).
    pts = D.OUTLINE + [D.OUTLINE[0]]
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(pt(ax, ay))
        seg.SetEnd(pt(bx, by))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(mm(0.1))
        board.Add(seg)

    nets = {}

    def net(name):
        if name not in nets:
            ni = pcbnew.NETINFO_ITEM(board, name)
            board.Add(ni)
            nets[name] = ni
        return nets[name]

    for p in D.PARTS:
        lib, name = p["fp"].split(":")
        # "rover" = bibliothèque locale du projet (voir make_lib.py), sinon bibliothèques KiCad.
        libdir = os.path.join(OUT_DIR, "rover.pretty") if lib == "rover" else os.path.join(FP_ROOT, lib + ".pretty")
        fp = pcbnew.FootprintLoad(libdir, name)
        if fp is None:
            raise SystemExit("Empreinte introuvable : " + p["fp"])
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        fp.SetReference(p["ref"])
        # Certaines empreintes KiCad imposent leur propre isolement (0,2 mm sur le HTSSOP) :
        # on revient à la règle de la carte, la même pour tout le monde.
        fp.SetLocalClearance(None)
        # Lien empreinte <-> symbole du schéma (même UUID que dans gen_sch.py).
        fp.SetPath(pcbnew.KIID_PATH("/" + gen_sch.uid("sym:" + p["ref"])))
        fp.SetValue(p["value"])
        fp.SetPosition(pt(p["x"], p["y"]))
        # L'empreinte doit appartenir à la carte avant d'être retournée (sinon
        # pcbnew plante), d'où l'ajout ici et non en fin de boucle.
        board.Add(fp)
        if p["side"] == "B":
            fp.SetLayerAndFlip(pcbnew.B_Cu)
        fp.SetOrientationDegrees(p["rot"])
        if p["dnp"]:
            # Empreinte gardée en réserve, composant non monté.
            fp.SetDNP(True)
            fp.SetExcludedFromBOM(True)
            fp.SetExcludedFromPosFiles(True)
        if p["kind"] in ("TP", "HOLE", "JP"):
            fp.SetExcludedFromBOM(True)
            fp.SetExcludedFromPosFiles(True)
        used = set()
        # Broches du symbole laissées libres : KiCad leur donne un net "unconnected-(...)".
        # On reprend exactement le même nom pour que schéma et carte soient identiques.
        sym_pins = dict(gen_sch.symbol_for(p)[1])
        for pad in fp.Pads():
            num = pad.GetNumber()
            if num in p["pins"]:
                pad.SetNet(net(p["pins"][num]))
                used.add(num)
            elif num in sym_pins:
                pad.SetNet(net("unconnected-(%s-%s-Pad%s)" % (p["ref"], sym_pins[num], num)))
        missing = set(p["pins"]) - used
        if missing:
            raise SystemExit("%s : pastilles inconnues %s" % (p["ref"], sorted(missing)))
        # Cartes denses : les repères (R12, U3...) restent sur la couche de fabrication ;
        # la sérigraphie ne porte que le nom des connecteurs (SILK).
        fp.Value().SetVisible(False)
        ref_txt = fp.Reference()
        ref_txt.SetLayer(pcbnew.B_Fab if p["side"] == "B" else pcbnew.F_Fab)
        ref_txt.SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8)))
        ref_txt.SetTextThickness(mm(0.12))
    return board, nets


def add_silk(board):
    for text, x, y, angle, h, side in D.SILK:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(text)
        t.SetPosition(pt(x, y))
        t.SetLayer(pcbnew.B_SilkS if side == "B" else pcbnew.F_SilkS)
        t.SetTextSize(pcbnew.VECTOR2I(mm(h), mm(h)))
        t.SetTextThickness(mm(0.15))
        t.SetTextAngleDegrees(angle)
        if side == "B":
            t.SetMirrored(True)   # lisible quand on retourne la carte
        board.Add(t)


def add_zone(board, netinfo, layer, pts, clearance=0.15):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(netinfo)
    o = z.Outline()
    o.NewOutline()
    for x, y in pts:
        o.Append(mm(x), mm(y))
    z.SetLocalClearance(mm(clearance))
    z.SetMinThickness(mm(clearance))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefGap(mm(0.2))
    z.SetThermalReliefSpokeWidth(mm(0.25))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    board.Add(z)


def add_antenna_keepout(board):
    """Zone interdite (ni cuivre, ni piste, ni via) sous l'antenne du module ESP32.
    Présente sur les deux cartes : la carte puissance est juste sous l'antenne."""
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    z.SetLayerSet(pcbnew.LSET.AllCuMask())
    z.SetDoNotAllowTracks(True)
    z.SetDoNotAllowVias(True)
    z.SetDoNotAllowPads(False)
    z.SetDoNotAllowFootprints(False)
    try:
        z.SetDoNotAllowZoneFills(True)
    except AttributeError:
        z.SetDoNotAllowCopperPour(True)
    o = z.Outline()
    o.NewOutline()
    x0, y0, x1, y1 = D.ANTENNA
    for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        o.Append(mm(x), mm(y))
    board.Add(z)


def write_project():
    """Fichier projet KiCad : c'est lui (et non le .kicad_pcb) qui porte les règles de
    conception et les classes de nets utilisées par le contrôle DRC."""
    import io
    import json
    pro = json.load(io.open(os.path.join(HERE, "modele.kicad_pro"), encoding="utf8"))
    pro["meta"]["filename"] = D.NAME + ".kicad_pro"
    pro["board"]["design_settings"]["rules"].update(
        min_clearance=0.15, min_track_width=0.09, min_via_diameter=0.45, min_through_hole_diameter=0.2,
        min_hole_to_hole=0.25, min_copper_edge_clearance=0.25, min_hole_clearance=0.2, min_via_annular_width=0.1)
    base = pro["net_settings"]["classes"][0]
    classes = []
    for name, (w, clr, vd, vdr) in freeroute.CLASSES.items():
        c = dict(base)
        c.update(name=name, track_width=w, clearance=0.15, via_diameter=vd, via_drill=vdr)
        classes.append(c)
    classes.sort(key=lambda c: c["name"] != "Default")
    pro["net_settings"]["classes"] = classes
    pro["net_settings"]["netclass_patterns"] = [{"netclass": cls, "pattern": net}
                                                for net, cls in sorted(D.NETCLASS.items())]
    with io.open(os.path.join(OUT_DIR, D.NAME + ".kicad_pro"), "w", encoding="utf8", newline="\n") as f:
        json.dump(pro, f, indent=2)
    table = ["(fp_lib_table", "  (version 7)",
             '  (lib (name "rover")(type "KiCad")(uri "${KIPRJMOD}/rover.pretty")(options "")'
             '(descr "Empreintes locales du projet ROVER"))', ")", ""]
    with io.open(os.path.join(OUT_DIR, "fp-lib-table"), "w", encoding="utf8", newline="\n") as f:
        f.write("\n".join(table))


def main():
    stage = sys.argv[1] if len(sys.argv) > 1 else "place"
    os.makedirs(OUT_DIR, exist_ok=True)
    write_project()
    board, nets = build_board()
    add_silk(board)
    add_antenna_keepout(board)
    full = [(0, 0), (D.BOARD_W, 0), (D.BOARD_W, D.BOARD_H), (0, D.BOARD_H)]
    for layer, netname in D.PLANES.items():
        add_zone(board, nets[netname], LAYER[layer], full)
    freeroute.apply_netclasses(board, D.NETCLASS)
    pcbnew.SaveBoard(OUT_PCB, board)
    if stage == "route":
        passes = int(sys.argv[2]) if len(sys.argv) > 2 else 60
        log = freeroute.route(board, OUT_PCB, passes=passes)
        print("elargissement : %d pistes, %d vias" % widen.widen(board, D.WIDEN))
        # Remplissage de masse sur les deux faces, ajouté après le routage : il relie
        # entre eux les morceaux du plan de masse interne que les vias ont découpés.
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
            add_zone(board, nets["GND"], layer, full, clearance=0.2)
        # Le routeur a travaillé avec 0,2 mm d'isolement ; la règle de la carte est 0,15 mm.
        freeroute.apply_netclasses(board, D.NETCLASS, clearance=0.15)
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        pcbnew.SaveBoard(OUT_PCB, board)
        write_project()   # SaveBoard réécrit le fichier projet : on y remet nos règles
        print("journal Freerouting :", os.path.normpath(log))
    print("ecrit :", os.path.normpath(OUT_PCB))


if __name__ == "__main__":
    main()
