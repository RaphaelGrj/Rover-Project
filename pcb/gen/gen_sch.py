# -*- coding: utf-8 -*-
"""
Génère le schéma KiCad (rover_pcb_v1.kicad_sch) à partir de design.py.

Style volontairement simple : chaque composant est un rectangle avec ses
broches, et chaque broche porte une étiquette globale au nom de son net. Deux
broches qui portent la même étiquette sont reliées (pas de fils à suivre). Les
broches volontairement inutilisées portent une croix "non connecté".

Les identifiants (UUID) sont dérivés de la référence du composant : ils sont
donc stables d'une génération à l'autre, et gen_pcb.py réutilise les mêmes pour
lier chaque empreinte à son symbole.
"""
import io
import os
import uuid
import board_select  # noqa: F401  (doit précéder l'import de design)
import design as D

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", D.NAME, D.NAME + ".kicad_sch")
P = 2.54           # pas des broches
NS = uuid.UUID("6f1d2c1e-52d0-4c7e-9a57-0d0a52564f32")


def uid(key):
    return str(uuid.uuid5(NS, key))


ROOT = uid("root")
PREFIX = {"R": "R", "C": "C", "CP": "C", "L": "L", "LED": "D", "D": "D", "F": "F", "CONN": "J",
          "HOLE": "H", "NFET": "Q", "PFET8": "Q", "BTN": "SW", "SLIDE": "SW", "TP": "TP", "JP": "JP",
          "FPC24": "J", "USBC": "J"}


def pad_sort(k):
    """Tri naturel des numéros de pastilles (1, 2, 10, A1, A12, SH...)."""
    head = k.rstrip("0123456789")
    tail = k[len(head):]
    return (head, int(tail) if tail else -1)


def symbol_for(part):
    """Renvoie (nom_de_symbole, [(numéro, nom_de_broche), ...]) pour un composant."""
    kind = part["kind"]
    names = D.PIN_NAMES.get(kind)
    if names:
        pins = [(str(k), v) for k, v in names.items()]
        return kind, pins
    nums = sorted(part["pins"], key=pad_sort)
    if kind == "CONN":
        return "CONN_%d" % len(nums), [(n, n) for n in nums]
    if kind == "HOLE":
        return "HOLE", []
    return kind, [(n, n) for n in nums]


def geometry(pins):
    """Répartit les broches : moitié gauche, moitié droite. Renvoie (largeur, hauteur, positions)."""
    n = len(pins)
    left = (n + 1) // 2
    rows = max(left, n - left, 1)
    longest = max([len(nm) for _, nm in pins] + [1])
    w = max(5.08, round((longest * 2 * 1.3 + 6) / P) * P) if n > 2 else 5.08
    h = rows * P
    pos = {}
    for i, (num, _) in enumerate(pins):
        if i < left:
            pos[num] = (-w / 2 - P, h / 2 - P / 2 - i * P, 0)
        else:
            j = i - left
            pos[num] = (w / 2 + P, h / 2 - P / 2 - j * P, 180)
    return w, h, pos


def lib_symbol(name, pins, prefix):
    w, h, pos = geometry(pins)
    hide_names = len(pins) <= 2 or name.startswith("CONN")
    out = ['\t\t(symbol "rover:%s"' % name]
    if hide_names:
        out.append('\t\t\t(pin_names (offset 1.016) (hide yes))')
    else:
        out.append('\t\t\t(pin_names (offset 1.016))')
    out.append('\t\t\t(exclude_from_sim no) (in_bom yes) (on_board yes)')
    out.append('\t\t\t(property "Reference" "%s" (at 0 %.2f 0) (effects (font (size 1.27 1.27))))' % (prefix, h / 2 + 1.5))
    out.append('\t\t\t(property "Value" "%s" (at 0 %.2f 0) (effects (font (size 1.27 1.27))))' % (name, -h / 2 - 1.5))
    out.append('\t\t\t(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))')
    out.append('\t\t\t(property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))')
    out.append('\t\t\t(symbol "%s_0_1" (rectangle (start %.2f %.2f) (end %.2f %.2f) '
               '(stroke (width 0.254) (type default)) (fill (type background))))' % (name, -w / 2, h / 2, w / 2, -h / 2))
    out.append('\t\t\t(symbol "%s_1_1"' % name)
    for num, nm in pins:
        x, y, a = pos[num]
        out.append('\t\t\t\t(pin passive line (at %.2f %.2f %d) (length 2.54) '
                   '(name "%s" (effects (font (size 1.27 1.27)))) (number "%s" (effects (font (size 1.0 1.0)))))'
                   % (x, y, a, nm, num))
    out.append('\t\t\t)')
    out.append('\t\t)')
    return "\n".join(out), (w, h, pos)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    libs = {}
    geo = {}
    placed = []
    # --- mise en page : on remplit des lignes de gauche à droite -------------
    x, y, row_h = 30.0, 30.0, 0.0
    PAGE_W = 800.0
    for part in D.PARTS:
        name, pins = symbol_for(part)
        if name not in libs:
            prefix = PREFIX.get(part["kind"], "U")
            libs[name], geo[name] = lib_symbol(name, pins, prefix)
        w, h, pos = geo[name]
        longest = max([len(part["pins"].get(n, "")) for n, _ in pins] + [2])
        cell_w = w + 2 * P + 2 * (longest * 1.5 + 6)
        cell_h = h + 12
        if x + cell_w > PAGE_W:
            x, y, row_h = 30.0, y + row_h, 0.0
        cx = round((x + cell_w / 2) / 1.27) * 1.27
        cy = round((y + cell_h / 2) / 1.27) * 1.27
        placed.append((part, name, pins, cx, cy))
        x += cell_w
        row_h = max(row_h, cell_h)

    out = ['(kicad_sch', '\t(version 20260306)', '\t(generator "eeschema")', '\t(generator_version "10.0")',
           '\t(uuid "%s")' % ROOT, '\t(paper "A0")',
           '\t(title_block (title "%s")' % D.TITLE + ' (company "Rover-Project") '
           '(comment 1 "Genere par gen/gen_sch.py depuis gen/design.py - ne pas editer a la main"))',
           '\t(lib_symbols']
    out.extend(libs[n] for n in libs)
    out.append('\t)')

    for part, name, pins, cx, cy in placed:
        w, h, pos = geo[name]
        ref = part["ref"]
        su = uid("sym:" + ref)
        out.append('\t(symbol (lib_id "rover:%s") (at %.2f %.2f 0) (unit 1) (exclude_from_sim no) (in_bom %s) '
                   '(on_board yes) (dnp %s) (uuid "%s")'
                   % (name, cx, cy, "no" if (part["kind"] in ("HOLE", "TP", "JP") or part["dnp"]) else "yes",
                      "yes" if part["dnp"] else "no", su))
        val = part["value"].replace('"', "'")
        out.append('\t\t(property "Reference" "%s" (at %.2f %.2f 0) (effects (font (size 1.27 1.27))))' % (ref, cx, cy - h / 2 - 3.0))
        out.append('\t\t(property "Value" "%s" (at %.2f %.2f 0) (effects (font (size 1.27 1.27))))' % (val, cx, cy - h / 2 - 1.2))
        out.append('\t\t(property "Footprint" "%s" (at %.2f %.2f 0) (effects (font (size 1.27 1.27)) (hide yes)))' % (part["fp"], cx, cy))
        out.append('\t\t(property "Datasheet" "" (at %.2f %.2f 0) (effects (font (size 1.27 1.27)) (hide yes)))' % (cx, cy))
        for num, _ in pins:
            out.append('\t\t(pin "%s" (uuid "%s"))' % (num, uid("pin:%s:%s" % (ref, num))))
        out.append('\t\t(instances (project "%s" (path "/%s" (reference "%s") (unit 1))))' % (D.NAME, ROOT, ref))
        out.append('\t)')
        for num, _ in pins:
            px, py, ang = pos[num]
            gx, gy = cx + px, cy - py          # le repère du schéma a l'axe Y vers le bas
            net = part["pins"].get(num)
            if net is None:
                out.append('\t(no_connect (at %.2f %.2f) (uuid "%s"))' % (gx, gy, uid("nc:%s:%s" % (ref, num))))
            else:
                la, just = (180, "right") if ang == 0 else (0, "left")
                out.append('\t(global_label "%s" (shape passive) (at %.2f %.2f %d) '
                           '(effects (font (size 1.27 1.27)) (justify %s)) (uuid "%s"))'
                           % (net, gx, gy, la, just, uid("lbl:%s:%s" % (ref, num))))
    out.append('\t(sheet_instances (path "/" (page "1")))')
    out.append('\t(embedded_fonts no)')
    out.append(')')
    io.open(OUT, "w", encoding="utf8", newline="\n").write("\n".join(out) + "\n")
    # Bibliothèque de symboles du projet (mêmes symboles que ceux embarqués dans le schéma),
    # déclarée dans sym-lib-table pour que KiCad puisse les retrouver et les éditer.
    lib = ['(kicad_symbol_lib', '\t(version 20251024)', '\t(generator "rover_gen_sch")', '\t(generator_version "10.0")']
    lib.extend(libs[n].replace('(symbol "rover:', '(symbol "', 1) for n in libs)
    lib.append(')')
    d = os.path.dirname(OUT)
    io.open(os.path.join(d, "rover.kicad_sym"), "w", encoding="utf8", newline="\n").write("\n".join(lib) + "\n")
    io.open(os.path.join(d, "sym-lib-table"), "w", encoding="utf8", newline="\n").write(
        '(sym_lib_table\n  (version 7)\n  (lib (name "rover")(type "KiCad")(uri "${KIPRJMOD}/rover.kicad_sym")'
        '(options "")(descr "Symboles du projet ROVER"))\n)\n')
    print("schema ecrit :", os.path.normpath(OUT), "-", len(placed), "symboles")


if __name__ == "__main__":
    main()
