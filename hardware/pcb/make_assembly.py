# SPDX-License-Identifier: GPL-3.0-only
"""JLCPCB's assembly files for the jank board (README "Assembly"): fab/bom.csv (Comment, Designator, Footprint, LCSC
Part #) and fab/cpl.csv (Designator, Mid X, Mid Y, Layer, Rotation), from jank-pcb.kicad_pcb and the parts table of
make_schematic.py (its BOM codes), as optimist's hardware/long-both-sides does it.

  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 make_assembly.py

What JLCPCB places (Economic, the TOP side only): every top part with an LCSC code, SMD and through hole, but NOT the
jacks and the DINs
(J6..J9: the user solders them), NOT the XIAO module (U1: Seeed, not at LCSC; the user solders it on the two headers
JLCPCB fits), NOT C4 and the 1206 pads in parallel (not fitted), and no jumper, test point or hole. Three parts have no
footprint of their own and are added at their place: the XIAO's two 1 x 7 headers (U1H1, U1H2, on its pin rows) and
the OLED's 1 x 4 header (J10H, J10's pins; J10 itself is the module, placed on it).

Mid X / Mid Y: mm from the board's lower left corner (the Gerbers' and the drill files' origin), Y up. An SMD part's
middle is its footprint's origin (KiCad's SMD footprints are drawn round their middle); a through-hole part's is the
middle of its pads (KiCad's origin is its pin 1). Rotation: KiCad's, plus the correction of the community table that
Bouni's kicad-jlcpcb-tools downloads (matthewlai/JLCKicadTools cpl_rotations_db.csv, read 2026-11-08) where a
footprint matches it (SOT-23*: -90, SOT-363: +180, SOIC-*: +270); every other part as KiCad has it. Nothing on the bottom is placed
(the DINs and the 9 standing resistors R8, R11..R14, R21..R24: by hand). All of it is UNVERIFIED against JLCPCB's
placement preview.
"""
import csv
import os
import re
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import make_footprints as F
import make_schematic as S                      # (the parts table and the BOM codes; nothing is written on import)

MOD = os.environ.get("MODULAR") == "1"         # (the modular variant: jank-pcb-mod.kicad_pcb, its files in fab/mod/)
FAB = os.path.join(HERE, "fab", *(["mod"] if MOD else []))
SKIP = re.compile(r"^(H\d|JP|TP|J[6-9]$|J1[1-4]$|U1$|C4$|R8$|R1[1-4]$|R2[1-4]$)")   # (the standing resistors: by hand, under the board)
ROT_FIX = [(r"^SOT-23", -90), (r"^SOT-363", 180), (r"^SOIC-", 270)]   # (the community table's rows that match a footprint here)

board = pcbnew.LoadBoard(os.path.join(HERE, "jank-pcb-mod.kicad_pcb" if MOD else "jank-pcb.kicad_pcb"))
ox, oy = (pcbnew.ToMM(v) for v in (board.GetDesignSettings().GetAuxOrigin().x, board.GetDesignSettings().GetAuxOrigin().y))
key = {p[0]: p[4] for p in S.PARTS}
dnp = {p[0] for p in S.PARTS if "DNP" in p[2]}


def mid(fp):
    """an SMD part's origin; a through-hole part's middle of its pads (board mm, KiCad's page frame)"""
    if not fp.GetAttributes() & pcbnew.FP_THROUGH_HOLE:
        return pcbnew.ToMM(fp.GetPosition().x), pcbnew.ToMM(fp.GetPosition().y)
    xs = [pcbnew.ToMM(p.GetPosition().x) for p in fp.Pads() if p.GetNumber()]
    ys = [pcbnew.ToMM(p.GetPosition().y) for p in fp.Pads() if p.GetNumber()]
    return (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2


def rotation(fp, name):
    r = fp.GetOrientationDegrees()
    if fp.GetLayer() != pcbnew.B_Cu:
        for pat, fix in ROT_FIX:
            if re.match(pat, name):
                r += fix
    return round(r) % 360


rows, places = [], []
for fp in board.GetFootprints():
    ref = fp.GetReference()
    if SKIP.match(ref) or ref in dnp:
        continue
    info = S.BOM.get(key.get(ref, ""), {})
    code = info.get("code", "")
    if not code.startswith("C"):
        raise SystemExit(f"{ref}: no LCSC code")
    name = fp.GetFPID().GetLibItemName().wx_str()
    x, y = mid(fp)
    side = "Bottom" if fp.GetLayer() == pcbnew.B_Cu else "Top"
    smd = not any(q.GetAttribute() == pcbnew.PAD_ATTRIB_PTH for q in fp.Pads())   # (J1: SMD pins but through-hole shell legs)
    rows.append((fp.GetValue(), ref, name, code, smd))
    places.append((ref, x, y, side, rotation(fp, name)))
    if ref == "J10":                             # (the module on its header: the header at the pins, the module at its middle)
        hdr = S.BOM["hdr4"]["code"]
        rows.append(("Pin header 1x4", "J10H", "PinHeader_1x04_P2.54mm_Vertical", hdr, False))
        places.append(("J10H", x, y, side, 90))
        pos = (pcbnew.ToMM(fp.GetPosition().x), pcbnew.ToMM(fp.GetPosition().y))
        mx = pos[0] + F.OLED_PIN_X - F.OLED_L / 2       # (J10 turned 180: the module along -x from its pins)
        my = pos[1] + F.OLED_PIN_Y - F.OLED_W / 2
        places[-2] = (ref, mx, my, side, round(fp.GetOrientationDegrees()) % 360)
u1 = next(fp for fp in board.GetFootprints() if fp.GetReference() == "U1")
for n, pads in (("U1H1", range(1, 8)), ("U1H2", range(8, 15))):   # (the XIAO's two pin rows: its headers)
    pts = [(pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y)) for p in u1.Pads()
           if p.GetNumber() in {str(i) for i in pads} and p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    rows.append(("Pin header 1x7", n, "PinHeader_1x07_P2.54mm_Vertical", S.BOM["hdr"]["code"], False))
    places.append((n, sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts), "Top", 90))

assert all(p[3] == "Top" for p in places), [p[0] for p in places if p[3] != "Top"]   # (Economic: one side)
# the checks JLCPCB's upload makes: one row per designator, every row with a code, the two files on the same set
refs = [p[0] for p in places]
assert len(refs) == len(set(refs)), "designators not unique"
assert {r[1] for r in rows} == set(refs)
assert all(re.match(r"^C\d+$", r[3]) for r in rows)
W = pcbnew.ToMM(board.GetBoardEdgesBoundingBox().GetWidth())
for ref, x, y, side, rot in places:
    assert ox <= x <= ox + W and y <= oy, (ref, x, y)

nat = lambda r: [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", r)]


def write(folder, keep):
    """bom.csv and cpl.csv of the parts `keep` accepts, in `folder`"""
    os.makedirs(folder, exist_ok=True)
    chosen = [r for r in rows if keep(r)]
    names = {r[1] for r in chosen}
    groups = {}
    for value, ref, name, code, _smd in chosen:
        groups.setdefault((value, name, code), []).append(ref)
    with open(os.path.join(folder, "bom.csv"), "w", newline="") as f:
        w = csv.writer(f, quoting=csv.QUOTE_ALL, lineterminator="\n")
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (value, name, code), refs_ in sorted(groups.items(), key=lambda g: nat(sorted(g[1], key=nat)[0])):
            w.writerow([value, ",".join(sorted(refs_, key=nat)), name, code])
    with open(os.path.join(folder, "cpl.csv"), "w") as f:
        f.write("Designator,Mid X,Mid Y,Layer,Rotation\n")
        for ref, x, y, side, rot in sorted((p for p in places if p[0] in names), key=lambda p: nat(p[0])):
            f.write(f"{ref},{x - ox:.3f}mm,{oy - y:.3f}mm,{side},{rot}\n")
    print(f"wrote {os.path.relpath(folder, HERE)}/bom.csv ({len(groups)} lines), cpl.csv ({len(names)} parts, all top)")


# the default set: the SMD parts only (the user's ruling: JLCPCB's through-hole lines are hand work at a fee, and four
# of those parts had no stock); the full set, every part JLCPCB could place, for later
write(FAB, lambda r: r[4])
write(os.path.join(FAB, "full"), lambda r: True)
