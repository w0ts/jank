# SPDX-License-Identifier: GPL-3.0-only
"""jank-mini-3's bridge strip (mots, 2026-11-19, after review 8): a small 2-layer PCB with TWO 1 x 14 female sockets
(C2905420, 8.5 mm), 3.81 mm apart: one takes the LCD-2's pins, the other OUR male row (J15 / J16: standard Kinghelm
C2905490, 6 mm mating pin), each pin joined straight across. Both sides are the same stack (2.4-2.5 mm plastic, ~6 mm
of pin into an 8.5 mm socket), so the strip plugs onto both boards and hangs 11 mm below them; a printed support in
the case holds it up from below. Nothing is soldered to our board, the LCD-2 stays removable. (Rounds 4-8: one socket
and plated holes for 11.5 mm long pins soldered from below; review 4B W1 had ruled out two sockets only because our
side then used those long pins.) 2 per unit, identical: turned 180 degrees, the same strip serves the other row.
Writes strip.kicad_pcb.   KiCad's python: make_strip.py"""
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
LIB = os.path.join(HERE, "..", "lib", "chip.pretty")
FP = "HDR-TH_14P-P2.54-V-F"
MM = pcbnew.FromMM
OX = OY = 100.0
PITCH, N, D = 2.54, 14, 3.81            # pin pitch, pins per row, row to row (make_board.D_ROW)
L = N * PITCH + 1.5                     # 37.06: the sockets' 35.56 plus 0.75 a side
W = 10.0                                # (10 mm: JLC assembles nothing under 10 x 10 mm; was D + 4.6 = 8.41)
TRACK = 0.5


def seg(board, x0, y0, x1, y1, layer, width):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(pcbnew.VECTOR2I(MM(OX + x0), MM(OY + y0)))
    s.SetEnd(pcbnew.VECTOR2I(MM(OX + x1), MM(OY + y1)))
    s.SetLayer(layer)
    s.SetWidth(MM(width))
    board.Add(s)


def socket(board, ref, y):
    fp = pcbnew.FootprintLoad(LIB, FP)
    if fp is None:
        sys.exit(f"no footprint {FP}")
    fp.SetReference(ref)
    fp.SetValue("1x14 F 8.5 (C2905420)")
    fp.SetFPID(pcbnew.LIB_ID("chip", FP))
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    board.Add(fp)
    fp.SetOrientationDegrees(0)
    fp.SetPosition(pcbnew.VECTOR2I(MM(OX + L / 2), MM(OY + y)))
    return fp


def main() -> None:
    board = pcbnew.BOARD()
    board.SetFileName(os.path.join(HERE, "strip.kicad_pcb"))
    board.GetDesignSettings().SetCopperLayerCount(2)
    for a, b in (((0, 0), (L, 0)), ((L, 0), (L, W)), ((L, W), (0, W)), ((0, W), (0, 0))):
        seg(board, *a, *b, pcbnew.Edge_Cuts, 0.1)
    y_a, y_b = W / 2 - D / 2, W / 2 + D / 2
    fa = socket(board, "J1", y_a)             # (the LCD-2's row)
    fb = socket(board, "J2", y_b)             # (our row: the same socket, our standard male header plugs in)
    for k in range(1, N + 1):
        n = pcbnew.NETINFO_ITEM(board, f"P{k}")
        board.Add(n)
        pa = next(p for p in fa.Pads() if p.GetNumber() == str(k))
        pb = next(p for p in fb.Pads() if p.GetNumber() == str(k))
        pa.SetNet(n)
        pb.SetNet(n)
        if abs(pa.GetPosition().x - pb.GetPosition().x) > MM(0.01):
            sys.exit(f"pin {k}: the two rows are not straight across")
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):          # (both layers: two parallel paths per pin)
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(pa.GetPosition())
            t.SetEnd(pb.GetPosition())
            t.SetWidth(MM(TRACK))
            t.SetLayer(layer)
            t.SetNet(n)
            board.Add(t)
    # (review mini3-3B R5: fab files need an origin and a legible part: the pin-1 end marked on both sides, a name)
    board.GetDesignSettings().SetAuxOrigin(pcbnew.VECTOR2I(MM(OX), MM(OY + W)))
    p1 = next(p for p in fa.Pads() if p.GetNumber() == "1").GetPosition()
    x1 = pcbnew.ToMM(p1.x) - OX
    for layer, mirror in ((pcbnew.F_SilkS, False), (pcbnew.B_SilkS, True)):
        for text, x in (("1", 0.7 if x1 < L / 2 else L - 0.7), ("14", L - 0.7 if x1 < L / 2 else 0.7)):
            t = pcbnew.PCB_TEXT(board)
            t.SetText(text)
            t.SetLayer(layer)
            t.SetTextSize(pcbnew.VECTOR2I(MM(1.0), MM(1.0)))   # (JLC minimum 1.0, review 5B)
            t.SetTextThickness(MM(0.15))
            t.SetTextAngleDegrees(90)
            t.SetMirrored(mirror)
            t.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + W / 2)))
            board.Add(t)
    board.Save(board.GetFileName())
    print(f"strip {L:.2f} x {W:.2f} mm: two 1x14 sockets (the LCD-2's pins, ours) {D} mm apart, {N} pins joined straight across")


if __name__ == "__main__":
    main()
