# Outil de diagnostic : affiche l'emprise (courtyard) et les pastilles d'une empreinte KiCad.
import pcbnew, sys
FP = r"C:\Program Files\KiCad\10.0\share\kicad\footprints"
for a in sys.argv[1:]:
    lib, name = a.split(":")
    f = pcbnew.FootprintLoad(FP + "\\" + lib + ".pretty", name)
    if f is None:
        print("MISSING", a); continue
    bb = f.GetCourtyard(pcbnew.F_CrtYd).BBox()
    mm = pcbnew.ToMM
    print("==", a, "crtyd x[%.2f,%.2f] y[%.2f,%.2f]" % (mm(bb.GetLeft()), mm(bb.GetRight()), mm(bb.GetTop()), mm(bb.GetBottom())))
    out = []
    for p in f.Pads():
        pos = p.GetPosition(); s = p.GetSize()
        out.append("%s(%.2f,%.2f %.2fx%.2f%s)" % (p.GetNumber(), mm(pos.x), mm(pos.y), mm(s.x), mm(s.y), " d%.2f" % mm(p.GetDrillSize().x) if p.GetDrillSize().x else ""))
    print(" ".join(out))
