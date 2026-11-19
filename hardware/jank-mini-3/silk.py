# SPDX-License-Identifier: GPL-3.0-only
"""The silkscreen of lcd2.kicad_pcb, after routing: footprint silk that leaves the board or lies on another part's
pads is trimmed (the USB-A's outline overhangs the left edge by design); then the labels a user needs, each placed
only where it clears every pad by 0.2 mm (else it is moved down in 0.5 mm steps, up to 4 mm, or left out and named).
KiCad's python: silk.py lcd2.kicad_pcb"""
import sys

import pcbnew

MM = pcbnew.FromMM
OX = OY = 100.0
# (text, x, y, size, layer) in board mm; the LCD-2's rows: pin 1 marks
LABELS = [   # (connector names only: warnings live in the README, mots 2026-11-18)
    ("OUT", 9.5, 13.6, 0.9, "F"), ("IN", 9.5, 49.6, 0.9, "F"),
    ("LINE IN", 16.0, 15.0, 0.9, "F"), ("MIDI 1", 30.0, 15.0, 0.9, "F"), ("MIDI 2", 44.0, 15.0, 0.9, "F"),
    ("CLK", 55.5, 15.0, 0.9, "F"), ("RST", 64.0, 15.0, 0.9, "F"),
    ("UP", 66.5, 27.0, 0.9, "F"), ("DOWN", 66.5, 45.0, 0.9, "F"), ("NO", 57.5, 36.0, 0.9, "F"), ("YES", 75.5, 36.0, 0.9, "F"),
    ("MIDI 1 DIN", 78.5, 9.0, 0.9, "F"), ("MIDI 2 DIN", 78.5, 56.0, 0.9, "F"),
    ("LCD-2 P1 (pin 1 >)", 30.0, 50.0, 0.8, "F"), ("LCD-2 P2 (pin 1 >)", 30.0, 19.4, 0.8, "F"),
    ("cell lead", 28.5, 27.0, 0.8, "F"),
    ("jank LCD-2 add-on  2026-11-18", 50.0, 32.0, 1.2, "B"),
]

import os  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_board as B  # noqa: E402

if B.MINI3:   # (jank-mini-3: the labels follow the layout; the cut-out has no silk)
    _h, _d, _ly = B.H, B.DPX, B.LY
    LABELS = [
        # (the two USB-C, mots 2026-11-18: named for what plugs in, not IN / OUT; behind their data fan-out)
        ("TO ESP", B.PLACE["J1"][0], 12.4, 1.0, "F"), ("TO DEVICE", B.PLACE["J4"][0], 12.4, 1.0, "F"),   # (below the drawn data pair's vias, review 7B N5)
        ("MIDI 1", B.PLACE["J2"][0], 15.0, 1.0, "F"), ("MIDI 2", B.PLACE["J3"][0], 15.0, 1.0, "F"),
        ("MIDI 1 DIN", B.W - 33.0, 17.6, 1.0, "F"), ("MIDI 2 DIN", B.W - 10.9, 17.6, 1.0, "F"),
        ("CLK", B.W - 8.0, _h / 2 - 14.0, 1.0, "F"), ("RST", B.W - 8.0, _h / 2 + 14.0, 1.0, "F"),
        ("LINE IN", 30.0, _h - 16.2, 1.0, "F"),
        ("UP", _d + 4.6, _h / 2 - 8.0, 1.0, "F"), ("DOWN", _d + 5.4, _h / 2 + 8.0, 1.0, "F"),
        ("NO", _d - 8.0, _h / 2 + 3.8, 1.0, "F"), ("YES", _d + 8.0, _h / 2 + 3.8, 1.0, "F"),
        # (pin 1 lies LEFT of these labels: "<" on top; seen from below it is on the right, so the bottom fallback reads
        #  "... >", review 5B N6)
        ("< P2 pin 1", 45.5, _ly - 3.4, 1.0, "F"), ("< P1 pin 1", 45.5, _ly + 38.4, 1.0, "F"),
        ("jank-mini-3  2026-11-19", B.W - 13.0, _h - 3.0, 1.2, "B"),
    ]


def trim(board):
    edge = board.GetBoardEdgesBoundingBox()
    pads = [(f, p) for f in board.GetFootprints() for p in f.Pads()]
    items = [(f, g) for f in board.GetFootprints() for g in list(f.GraphicalItems())]   # (all first: removing while
    dead = []                                                                       #  iterating broke SWIG's list)
    for f, g in items:
        if True:
            if g.GetLayer() not in (pcbnew.F_SilkS, pcbnew.B_SilkS) or g.Type() == pcbnew.PCB_TEXT_T:
                continue
            bb = g.GetBoundingBox()
            out = not edge.Contains(bb.GetOrigin()) or not edge.Contains(bb.GetEnd())
            if B.NOTCHES:   # (jank-mini-3: silk reaching into a notch, or within 0.2 mm of its edge)
                x0, y0 = pcbnew.ToMM(bb.GetLeft()) - OX, pcbnew.ToMM(bb.GetTop()) - OY
                x1, y1 = pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY
                out = out or B.in_notch(x0, y0, x1, y1, 0.2)
            sh = g.GetEffectiveShape()
            hit = any(o is not f and p.GetEffectiveShape(pcbnew.F_Cu if g.GetLayer() == pcbnew.F_SilkS else pcbnew.B_Cu)
                      .Collide(sh, MM(0.15)) for o, p in pads)
            if B.MINI3 and f.GetReference() in ("J15", "J16"):
                out = True   # (jank-mini-3: the male rows' silk is under the board, hidden by their own plastic; all
                             #  of it goes, so both rows stay identical and match the one library copy)
            if out or hit:
                dead.append((f, g))
    # a passive's (R / C) outline gives way where it touches another part's silk (pin-1 marks, chip outlines stay):
    # DRC silk_overlap, e.g. R162's outline against U22's pin-1 dot (jank-mini-3, 2026-11-18)
    taken = {id(g) for _f, g in dead}
    silk = [(f, g) for f, g in items if id(g) not in taken and g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS)
            and g.Type() != pcbnew.PCB_TEXT_T]
    for f, g in silk:
        if f.GetReference()[:1] not in ("R", "C"):
            continue
        sh = g.GetEffectiveShape()
        if any(o is not f and h.GetLayer() == g.GetLayer() and h.GetEffectiveShape().Collide(sh, MM(0.15))
               for o, h in silk):
            dead.append((f, g))
    # (then that footprint's outline goes on EVERY copy, so all copies match the one library copy: lib_footprint_mismatch)
    def fpname(f):
        n = f.GetFPID().GetLibItemName()
        return n.wx_str() if hasattr(n, "wx_str") else str(n)
    strip = {fpname(f) for f, g in dead if f.GetReference()[:1] in ("R", "C") and id(g) not in taken}
    have = {id(g) for _f, g in dead}
    dead += [(f, g) for f, g in silk if fpname(f) in strip and id(g) not in have]
    for f, g in dead:
        f.Remove(g)
    # the trimmed footprints back into the project's library (else DRC: lib_footprint_mismatch)
    import os
    lib = os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib", "chip.pretty")
    for f in {id(f): f for f, _g in dead}.values():
        name = f.GetFPID().GetLibItemName().wx_str() if hasattr(f.GetFPID().GetLibItemName(), "wx_str") else str(f.GetFPID().GetLibItemName())
        c = pcbnew.FOOTPRINT(f)
        c.SetPosition(pcbnew.VECTOR2I(0, 0))
        c.SetOrientationDegrees(0)
        pcbnew.FootprintSave(lib, c)
        print(f"  {name}: its trimmed silk saved to lib/chip.pretty")
    return len(dead)


PLACED = []   # (layer, inflated box) of the labels placed so far: no label on another


def label(board, text, x, y, size, side):
    layer = pcbnew.F_SilkS if side == "F" else pcbnew.B_SilkS
    cu = pcbnew.F_Cu if side == "F" else pcbnew.B_Cu
    pads = [p for f in board.GetFootprints() for p in f.Pads() if p.IsOnLayer(cu) or p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    # (nor over a via: the CLK_M via printed through "UP" on the bottom, read "UR", review 7B N5)
    # (signal vias only: the GND stitching grid covers the board, avoiding it left "TO DEVICE" and the date out)
    vias = [v for v in board.GetTracks() if v.Type() == pcbnew.PCB_VIA_T and v.GetNetname().lstrip("/") != "GND"]
    # (the spots round the wanted one, nearest first: +/-3 mm in 0.5 steps; was: only downward)
    spots = sorted(((dx * 0.5, dy * 0.5) for dx in range(-6, 7) for dy in range(-6, 9)), key=lambda d: abs(d[0]) + abs(d[1]))
    for dx, dy in spots:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(text)
        t.SetLayer(layer)
        t.SetTextSize(pcbnew.VECTOR2I(MM(size), MM(size)))
        t.SetTextThickness(MM(0.15 if size >= 1 else 0.12))
        t.SetPosition(pcbnew.VECTOR2I(MM(OX + x + dx), MM(OY + y + dy)))
        if side == "B":
            t.SetMirrored(True)
        bb = t.GetBoundingBox()
        bb.Inflate(MM(0.2))
        x0, y0 = pcbnew.ToMM(bb.GetLeft()) - OX, pcbnew.ToMM(bb.GetTop()) - OY
        x1, y1 = pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY
        in_cut = B.in_notch(x0, y0, x1, y1)   # (never in a notch)
        if side == "F":   # (nor under the D-pad cap, nor on a top part's body: "DOWN" sat on SW4, read "OWN", review 3B)
            in_cut = in_cut or any(x0 < k[2] and x1 > k[0] and y0 < k[3] and y1 > k[1] for k in B.DPAD_KEEP) \
                or any((lambda c: x0 < c[2] and x1 > c[0] and y0 < c[3] and y1 > c[1])(B.courtyard(f, pads=False))
                       for f in board.GetFootprints() if not f.IsFlipped() and f.GetReference()[:1] in "SUJ")
        if board.GetBoardEdgesBoundingBox().Contains(bb.GetOrigin()) and board.GetBoardEdgesBoundingBox().Contains(bb.GetEnd()) \
                and not in_cut and not any(p.GetBoundingBox().Intersects(bb) for p in pads) \
                and not any(v.GetBoundingBox().Intersects(bb) for v in vias) \
                and not any(lay == layer and o.Intersects(bb) for lay, o in PLACED):
            board.Add(t)
            PLACED.append((layer, bb))
            return True
    return False


def main():
    """two processes: silk.py BOARD --trim, then silk.py BOARD --labels (SWIG's lists broke after a Remove())"""
    path = sys.argv[1]
    board = pcbnew.LoadBoard(path)
    if "--unlabel" in sys.argv:   # (its own process: the labels off before a re-run of --labels)
        dead = [d for d in board.GetDrawings() if d.GetClass() == "PCB_TEXT"]
        for d in dead:
            board.Remove(d)
        print(f"unlabel: {len(dead)} labels removed")
    elif "--trim" in sys.argv:
        print(f"silk: {trim(board)} footprint silk items trimmed")
    else:
        left, moved = [], []
        for txt, x, y, s, side in LABELS:
            if label(board, txt, x, y, s, side):
                continue
            under = txt[2:] + " >" if txt.startswith("< ") else txt   # (the arrow flips seen from below)
            if side == "F" and label(board, under, x, y, s, "B"):   # (no room on top: the bottom, said)
                moved.append(under)
            else:
                left.append(txt)
        # (the cell sockets' "+" marks went with J18, mots 2026-11-19: the cell is on the LCD-2's own MX1.25)
        print(f"labels: {len(LABELS) - len(left) - len(moved)} placed; on the bottom instead: {moved or 'none'}; "
              f"left out (no room): {left or 'none'}")
    board.Save(path)


if __name__ == "__main__":
    main()
