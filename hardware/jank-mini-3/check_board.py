# SPDX-License-Identifier: GPL-3.0-only
"""lcd2.kicad_pcb against parts.py and the layout rules DRC does not know. Exit 1 on any problem.
 1. every pad's net = parts.py's (the netlist the schematic was checked against);
 2. LINE IN: its analogue nets' copper >= QUIET_MM from the MIDI / CLOCK / RESET / switch-node copper, and its parts
    >= BOOST_MM from the boost (U18, L5);
 3. under the LCD-2 (its outline + 0.5 mm): only parts lower than LOW_MM (the 11 mm gap less its ~3.5 mm back parts);
 4. the M2 spacers: no part's courtyard within 2.0 mm of a hole's centre; the cell slot: no part within 2 mm;
 5. the connectors' mouths at their walls (front edge within 1 mm of the edge).
KiCad's python: check_board.py lcd2.kicad_pcb"""
import math
import os
import sys

import pcbnew

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import parts as P
import make_board as B

OX = OY = 100.0
QUIET = {"LINE_IN", "LINE_SENSE", "LIN_A1", "LIN_A2", "LIN_B1", "LIN_B2", "LIN_VB", "LIN_VBL"}
LOUD = {"MIDI_TX", "MIDI_TX2", "MO_R", "MO_T", "MO_K", "MD_R", "MD_T", "MD_K", "MO2_R", "MO2_T", "MO2_K", "MD2_R",
        "MD2_T", "MD2_K", "CLK_IO", "CLK_B", "CLK_N", "CLK_M", "CLK_OUT", "RST_IO", "RST_B", "RST_N", "RST_M", "RST_OUT",
        "V5_SW", "HOST5", "HOST5G", "HOST_M", "UDP", "UDN"}
QUIET_MM, BOOST_MM, LOW_MM = 1.0, 8.0, 6.0
HEIGHT = {"SMA_L4.3-W2.6-LS5.2-RD": 2.3, "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5": 1.5, "IND-SMD_L2.0-W1.6-1": 1.0,
          "HDR-TH_14P-P2.54-V-F": 8.5}                 # (others: chip parts and SOT-23 / SOT-563 / TSSOP, <= 1.3 mm)
SOCKETS = {"J15", "J16"}


def mm(v):
    return pcbnew.ToMM(v)


def main():
    board = pcbnew.LoadBoard(sys.argv[1])
    errors, notes = [], []
    want = {(r, pad): net for r, _k, _v, _f, _c, pins in P.PARTS for pad, net in pins.items()}
    fps = {f.GetReference(): f for f in board.GetFootprints()}
    # 1
    for f in board.GetFootprints():
        for p in f.Pads():
            got = p.GetNetname().lstrip("/") or None
            if got and got.startswith("unconnected-"):
                got = None
            exp = want.get((f.GetReference(), p.GetNumber()))
            if p.GetNumber() and got != exp:
                errors.append(f"{f.GetReference()}.{p.GetNumber()}: board {got}, parts.py {exp}")
    for r in {r for r, _ in want} - set(fps):
        errors.append(f"{r} not on the board")
    # 2
    copper = [t for t in board.GetTracks()] + [p for f in board.GetFootprints() for p in f.Pads()]
    q = [c for c in copper if c.GetNetname().lstrip("/") in QUIET]
    l = [c for c in copper if c.GetNetname().lstrip("/") in LOUD]
    worst = (1e9, None, None)
    for a in q:
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
            if not a.IsOnLayer(layer):
                continue
            sa = a.GetEffectiveShape(layer)
            for b in l:
                if not b.IsOnLayer(layer):
                    continue
                d = mm(b.GetEffectiveShape(layer).Collide(sa, pcbnew.FromMM(QUIET_MM)) and 0 or 0)
                if b.GetEffectiveShape(layer).Collide(sa, pcbnew.FromMM(QUIET_MM)):
                    worst = (0, a.GetNetname(), b.GetNetname())
                    errors.append(f"LINE IN {a.GetNetname()} within {QUIET_MM} mm of {b.GetNetname()} on {board.GetLayerName(layer)}")
    if worst[0] > 0:
        notes.append(f"LINE IN copper >= {QUIET_MM} mm from every MIDI / CLOCK / RESET / switch-node / USB net")
    boost = [fps[r].GetPosition() for r in ("U18", "L5")]
    lin_parts = [r for r, _k, _v, _f, _c, pins in P.PARTS if set(pins.values()) & QUIET]
    dmin = min((fps[r].GetPosition() - b).EuclideanNorm() for r in lin_parts for b in boost)
    (notes if mm(dmin) >= BOOST_MM else errors).append(f"LINE IN parts to the boost (U18 / L5): {mm(dmin):.1f} mm (>= {BOOST_MM})")
    # 3 (jank-mini-3: the LCD-2 sits in a cut-out, nothing of ours is under it; make_board keeps parts out of it)
    x0, y0 = B.LX - 0.5, B.LY - 0.5
    x1, y1 = B.LX + 48.21 + 0.5, B.LY + 35.01 + 0.5
    for r, f in fps.items():
        if B.MINI3 or r in SOCKETS or r.startswith("H") and r[1:].isdigit():
            continue
        c = f.GetPosition()
        cx, cy = mm(c.x) - OX, mm(c.y) - OY
        if x0 <= cx <= x1 and y0 <= cy <= y1:
            h = HEIGHT.get(f.GetFPID().GetLibItemName().wx_str() if hasattr(f.GetFPID().GetLibItemName(), "wx_str") else str(f.GetFPID().GetLibItemName()), 1.3)
            if h > LOW_MM:
                errors.append(f"{r} ({h} mm) under the LCD-2")
    notes.append(f"under the LCD-2: every part <= {LOW_MM} mm (the tallest: SS34, 2.3 mm)")
    # 4
    for hx, hy in B.HOLES + B.MOUNT:
        for r, f in fps.items():
            if r.startswith("H") and r[1:].isdigit():
                continue
            bx0, by0, bx1, by1 = B.courtyard(f, pads=False)
            dx = max(bx0 - hx, 0, hx - bx1)
            dy = max(by0 - hy, 0, hy - by1)
            if math.hypot(dx, dy) < 2.0:
                errors.append(f"{r} within 2 mm of the M2 hole at ({hx:.2f}, {hy:.2f}) (the spacer's 4 mm)")
    sx0, sy0, sx1, sy1 = getattr(B, "SLOT", (-99, -99, -99, -99))   # (the cell-lead slot of an older layout)
    for r, f in fps.items():
        bx0, by0, bx1, by1 = B.courtyard(f, pads=False)
        if bx0 < sx1 + 2 and bx1 > sx0 - 2 and by0 < sy1 + 2 and by1 > sy0 - 2:
            errors.append(f"{r} within 2 mm of the cell lead's slot")
    notes.append("M2 holes: 2 mm free round each (4 mm spacers); the cell slot: 2 mm free")
    # 5
    walls = {r: f for r, (_x, _y, f) in B.PLACE.items() if isinstance(f, str)}   # (the connectors' walls, as placed)
    for r, wall in sorted(walls.items()):
        bx0, by0, bx1, by1 = B.courtyard(fps[r], pads=False)
        d = {"-x": bx0, "-y": by0, "+y": B.H - by1, "+x": B.W - bx1}[wall]
        (notes if abs(d) <= 1.0 else errors).append(f"{r} front {d:+.2f} mm from its wall ({wall})")
    # 6 (jank-mini-3, mots: no part where the D-pad cap rests): only the 4 switches in the cross, on top; no
    #   through-hole pad of a bottom part there either
    for k in B.DPAD_KEEP:
        for r, f in fps.items():
            if r in ("SW1", "SW3", "SW4", "SW5"):
                continue
            if not f.IsFlipped():
                bx0, by0, bx1, by1 = B.courtyard(f, pads=False)
                if bx0 < k[2] and bx1 > k[0] and by0 < k[3] and by1 > k[1]:
                    errors.append(f"{r} under the D-pad cap")
            for p in f.Pads():
                if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                    px, py = mm(p.GetPosition().x) - OX, mm(p.GetPosition().y) - OY
                    if k[0] - 0.9 < px < k[2] + 0.9 and k[1] - 0.9 < py < k[3] + 0.9:
                        errors.append(f"{r} pad {p.GetNumber()} (through hole) under the D-pad cap")
    if B.DPAD_KEEP:
        notes.append("D-pad cap: only SW1/3/4/5 in the 24 mm cross (+1 mm)")
    for n in notes:
        print("ok  ", n)
    for e in errors:
        print("ERROR", e)
    print(f"check_board: {len(errors)} problems")
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
