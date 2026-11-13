# -*- coding: utf-8 -*-
"""
Élargit les pistes de puissance après le routage.

Freerouting trace chaque net à une seule largeur, limitée par la broche la plus
fine qu'il dessert (0,25 mm autour du chargeur par exemple). Ici, on reprend
chaque segment des nets de puissance et on lui donne la plus grande largeur qui
respecte encore l'isolement avec tout ce qui l'entoure : la piste reste fine
juste à l'arrivée sur une broche serrée, et s'élargit dès qu'elle en sort.
Les vias de ces nets sont agrandis de la même façon quand la place existe.

Usage autonome (sur une carte déjà routée) :
    python widen.py puissance
gen_pcb.py l'appelle aussi automatiquement après chaque routage.
"""
import os
import pcbnew

mm = pcbnew.FromMM
CLEARANCE = 0.17        # un peu plus que la règle de la carte (0,15 mm)
STEP = 0.05
BIG_VIA = (0.8, 0.4)    # diamètre, perçage
CU = [pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu]


def _obstacles(board):
    """[(netcode, couche, forme, boîte englobante, objet)] pour pastilles, pistes et vias."""
    out = []
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            for layer in CU:
                if pad.IsOnLayer(layer) or pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                    out.append((pad.GetNetCode(), layer, pad.GetEffectiveShape(layer), pad.GetBoundingBox(), pad))
    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            for layer in CU:
                out.append((t.GetNetCode(), layer, t.GetEffectiveShape(layer), t.GetBoundingBox(), t))
        else:
            out.append((t.GetNetCode(), t.GetLayer(), None, None, t))   # forme relue à chaque essai
    return out


def _free(item, layer, net, obstacles, reach):
    shape = item.GetEffectiveShape(layer)
    box = item.GetBoundingBox()
    box.Inflate(reach)
    for onet, olayer, oshape, obox, obj in obstacles:
        if olayer != layer or obj is item or (onet == net and onet != 0):
            continue
        if obox is None:
            obox, oshape = obj.GetBoundingBox(), obj.GetEffectiveShape(layer)
        if not box.Intersects(obox):
            continue
        if shape.Collide(oshape, mm(CLEARANCE)):
            return False
    return True


def widen(board, targets):
    """targets : {nom de net: largeur visée en mm}. Renvoie (pistes élargies, vias agrandis)."""
    obstacles = _obstacles(board)
    reach = mm(2.0)
    n_tracks = n_vias = 0
    for t in list(board.GetTracks()):
        target = targets.get(str(t.GetNetname()))
        if not target:
            continue
        if t.GetClass() == "PCB_VIA":
            if pcbnew.ToMM(t.GetWidth(pcbnew.F_Cu)) >= BIG_VIA[0] or target < 0.5:
                continue
            old = (t.GetWidth(pcbnew.F_Cu), t.GetDrill())
            t.SetWidth(pcbnew.F_Cu, mm(BIG_VIA[0]))
            t.SetDrill(mm(BIG_VIA[1]))
            pos = t.GetPosition()
            ok = all(_free(t, layer, t.GetNetCode(), obstacles, reach) for layer in CU)
            if ok:   # distance aux autres perçages
                for onet, olayer, oshape, obox, obj in obstacles:
                    if obj is t or olayer != pcbnew.F_Cu or obj.GetClass() not in ("PCB_VIA", "PAD"):
                        continue
                    drill = obj.GetDrill() if obj.GetClass() == "PCB_VIA" else obj.GetDrillSize().x
                    if drill and (obj.GetPosition() - pos).EuclideanNorm() < drill / 2 + mm(BIG_VIA[1] / 2 + 0.3):
                        ok = False
                        break
            if ok:
                n_vias += 1
            else:
                t.SetWidth(pcbnew.F_Cu, old[0])
                t.SetDrill(old[1])
            continue
        start = pcbnew.ToMM(t.GetWidth())
        best = start
        w = target
        while w > start + 1e-6:
            t.SetWidth(mm(w))
            if _free(t, t.GetLayer(), t.GetNetCode(), obstacles, reach):
                best = w
                break
            w = round(w - STEP, 3)
        t.SetWidth(mm(best))
        if best > start + 1e-6:
            n_tracks += 1
    return n_tracks, n_vias


if __name__ == "__main__":
    import board_select  # noqa: F401
    import design as D
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", D.NAME, D.NAME + ".kicad_pcb")
    b = pcbnew.LoadBoard(path)
    print("%s : %d pistes elargies, %d vias agrandis" % ((D.NAME,) + widen(b, D.WIDEN)), flush=True)
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())
    pcbnew.SaveBoard(path, b)
