# SPDX-License-Identifier: GPL-3.0-only
"""JLCPCB's assembly files for the jank board (README "Assembly"): fab/bom.csv (Comment, Designator, Footprint, LCSC
Part #) and fab/cpl.csv (Designator, Mid X, Mid Y, Layer, Rotation), from jank-pcb.kicad_pcb and the parts table of
make_schematic.py (its BOM codes), as optimist's hardware/long-both-sides does it.

  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 make_assembly.py

What JLCPCB places (Economic, the TOP side only; the user, 2026-11-09: only the DINs by hand): every top part with an
LCSC code, SMD and through hole, the MIDI jacks J2..J5 (surface mount, inside J6..J9's footprint) and the side jacks,
but NOT the DINs (J6..J9: the user solders them), NOT the XIAO module (U1: Seeed, not at LCSC; the user solders it on the two headers
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
SKIP = re.compile(r"^(H\d|JP|TP|J[6-9]$|J10$|SW1$|SW6$|U1$|C4$|R8$|R1[1-4]$|R2[1-4]$)")   # (J10 the OLED module, SW1/SW6 the buttons: the user (2026-11-09; JLC refused the buttons))   # (J6..J9: the DINs, by hand; the axial pads: empty)
# JLC rotation = KiCad's + the correction for the footprint. Measured 2026-11-09 (the review) by placing JLC's own EasyEDA
# footprint of each LCSC code at the CPL position and matching pads by number: SOT-23 / SOT-23-5 +180 (the community
# table's -90 is wrong for C47546, C15127, C20917, C7484), SOT-363 +270, SOIC +270, DIP-6 +270 (the wrong way round
# reverses the opto), the EC11 encoder +270, the PJ-320A side jacks +90; the slide switches and 0805/1206 as KiCad has
# them. STILL check JLC's placement preview before paying.
JLC_ORIGIN = {"USB_C_SHOUHAN_TYPE-C-6P"}           # (footprints taken from JLC / EasyEDA: origin and rotation are JLC's)
ROT_FIX = [(r"^SOT-23", 180), (r"^SOT-363", 270), (r"^SOIC-", 270), (r"^DIP-6", 270), (r"^RotaryEncoder_Alps_EC11E", 270),
           (r"^Jack_3.5mm_XKB_PJ-320A", 90)]

board = pcbnew.LoadBoard(os.path.join(HERE, "jank-pcb-mod.kicad_pcb" if MOD else "jank-pcb.kicad_pcb"))
ox, oy = (pcbnew.ToMM(v) for v in (board.GetDesignSettings().GetAuxOrigin().x, board.GetDesignSettings().GetAuxOrigin().y))
key = {p[0]: p[4] for p in S.PARTS}
dnp = {p[0] for p in S.PARTS if "DNP" in p[2]}


def mid(fp):
    """an SMD part's origin; a through-hole part's middle of its pads (board mm, KiCad's page frame); a part with SMD
    pins and through-hole shell legs (the USB-C): the middle of its SMD pads only (the legs shifted it 1.14 mm)"""
    if fp.GetFPID().GetLibItemName().wx_str() in JLC_ORIGIN:     # (JLC's own footprint: its origin is JLC's)
        return pcbnew.ToMM(fp.GetPosition().x), pcbnew.ToMM(fp.GetPosition().y)
    smd = [p for p in fp.Pads() if p.GetNumber() and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD]
    mixed = smd and any(p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH for p in fp.Pads())
    if not mixed and not fp.GetAttributes() & pcbnew.FP_THROUGH_HOLE:
        return pcbnew.ToMM(fp.GetPosition().x), pcbnew.ToMM(fp.GetPosition().y)
    if mixed:
        xs = [pcbnew.ToMM(p.GetPosition().x) for p in smd]
        ys = [pcbnew.ToMM(p.GetPosition().y) for p in smd]
        return (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
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
        places.append(("J10H", x, y, side, 0))       # (the 0.96in module's pins along x)
        pos = (pcbnew.ToMM(fp.GetPosition().x), pcbnew.ToMM(fp.GetPosition().y))
        mx = pos[0] + F.O96_X0 + F.O96_W / 2            # (the module along +y from its pin row)
        my = pos[1] - F.O96_ROW + F.O96_L / 2
        places[-2] = (ref, mx, my, side, round(fp.GetOrientationDegrees()) % 360)
u1 = next(fp for fp in board.GetFootprints() if fp.GetReference() == "U1")
for n, pads in (("U1H1", range(1, 8)), ("U1H2", range(8, 15))):   # (the XIAO's two pin rows: its headers)
    pts = [(pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y)) for p in u1.Pads()
           if p.GetNumber() in {str(i) for i in pads} and p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    rows.append(("Pin header 1x7", n, "PinHeader_1x07_P2.54mm_Vertical", S.BOM["hdr"]["code"], False))
    places.append((n, sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts), "Top", 90))

# the MIDI outputs' surface-mount jacks J2..J5: no footprint of their own (their pads are in J6..J9's, beside the DIN):
# placed at the middle of those pads, turned as the footprint (v5c)
for k, fp in enumerate(sorted((f for f in board.GetFootprints() if re.match(r"^J[6-9]$", f.GetReference())),
                              key=lambda f: f.GetReference())):
    pts = [(pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y)) for p in fp.Pads() if p.GetNumber() in ("T", "R", "SW", "S")]
    ref = f"J{2 + k}"
    rows.append((S.BOM["trs_smd"]["mpn"].split()[-1], ref, "PJ-320D-B-SMT", S.BOM["trs_smd"]["code"], True))
    # JLC's origin for C2884940: the middle of its pads' bounding box (on the jack's axis); rotation KiCad + 90
    xs, ys = [q[0] for q in pts], [q[1] for q in pts]
    places.append((ref, (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, "Top", (round(fp.GetOrientationDegrees()) + 90) % 360))

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


# the default set (the user, 2026-11-09: "only keep the DINs hand soldered"): every part JLCPCB can place, SMD and
# through hole (the jacks, switches, encoder, buttons, opto, headers, the screen); the user solders the DINs and the XIAO.
# smd-only/: the SMD parts alone (the earlier choice), for comparison
write(FAB, lambda r: True)
write(os.path.join(FAB, "smd-only"), lambda r: r[4])
