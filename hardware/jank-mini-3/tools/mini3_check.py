"""jank-mini-3 placement check (study helper, KiCad's python): bottom bodies vs each other, the bridge strips and the
cells' volume; every courtyard inside the board and out of the cut-out; top courtyards vs each other.
Usage: python3 mini3_check.py <board.kicad_pcb>"""
import itertools
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import make_board as M  # noqa: E402  (the same geometry constants)

OX = OY = 100.0
STRIP_DEPTH, CELL_TOP_GAP = 12.6, 21.4 - 6.5      # under the board: strips 12.6 deep; above the cells 14.9 free
HANG = {"J5": 20.9, "J6": 20.9}                    # (everything else under the board hangs <= 9 mm)


def box(fp, layer):
    c = fp.GetCourtyard(layer)
    b = c.BBox() if c.OutlineCount() else fp.GetBoundingBox(False)
    return (pcbnew.ToMM(b.GetLeft()) - OX, pcbnew.ToMM(b.GetTop()) - OY,
            pcbnew.ToMM(b.GetRight()) - OX, pcbnew.ToMM(b.GetBottom()) - OY)


def hit(a, b, gap=0.0):
    return a[0] < b[2] + gap and a[2] > b[0] - gap and a[1] < b[3] + gap and a[3] > b[1] - gap


def strips():
    lo = M.ROW_X - 18.5, M.ROW_X + 18.5
    p2 = (lo[0], M.LY + 2.265 - M.D_ROW - 2.3, lo[1], M.LY + 2.265 + 2.3)
    p1 = (lo[0], M.LY + 32.745 - 2.3, lo[1], M.LY + 32.745 + M.D_ROW + 2.3)
    return {"strip P2": p2, "strip P1": p1}


def main(path: str) -> int:
    board = pcbnew.LoadBoard(path)
    fps = {f.GetReference(): f for f in board.GetFootprints()}
    bad = []
    bottom = {r: box(f, pcbnew.B_CrtYd) for r, f in fps.items() if f.IsFlipped()}
    top = {r: box(f, pcbnew.F_CrtYd) for r, f in fps.items() if not f.IsFlipped()}
    cells = (1.0, M.LY + 0.5, 61.0, M.LY + 34.5)
    for (a, ba), (b, bb) in itertools.combinations(bottom.items(), 2):
        if hit(ba, bb):
            bad.append(f"bottom {a} x {b}")
    for r, b in bottom.items():
        if r in ("J15", "J16"):
            continue          # (the male rows are part of the bridge)
        for n, s in strips().items():
            if hit(b, s, 0.2):
                bad.append(f"bottom {r} x {n}")
        if HANG.get(r, 9.0) > CELL_TOP_GAP and hit(b, cells):
            bad.append(f"bottom {r} (hangs {HANG[r]}) x the cells")
    for r, b in {**top, **bottom}.items():
        if b[0] < -0.31 or b[1] < -0.31 or b[2] > M.W + 0.31 or b[3] > M.H + 0.31:
            bad.append(f"{r} outside the board {tuple(round(v, 2) for v in b)}")
        if hit(b, M.CUT) and r not in ("J15", "J16"):
            bad.append(f"{r} in the cut-out")
    for (a, ba), (b, bb) in itertools.combinations(top.items(), 2):
        if hit(ba, bb):
            bad.append(f"top {a} x {b}")
    # through-hole pads of bottom parts vs top courtyards
    for r, f in fps.items():
        if not f.IsFlipped():
            continue
        for p in f.Pads():
            if p.GetAttribute() not in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                continue
            pb = p.GetBoundingBox()
            pbox = (pcbnew.ToMM(pb.GetLeft()) - OX, pcbnew.ToMM(pb.GetTop()) - OY,
                    pcbnew.ToMM(pb.GetRight()) - OX, pcbnew.ToMM(pb.GetBottom()) - OY)
            for t, tb in top.items():
                if hit(pbox, tb):
                    bad.append(f"{r} pad {p.GetNumber()} under top {t}")
    for r in ("J15", "J16"):
        p1 = next(p for p in fps[r].Pads() if p.GetNumber() == "1").GetPosition()
        print(f"{r}: pin 1 at x {pcbnew.ToMM(p1.x) - OX:.2f}, y {pcbnew.ToMM(p1.y) - OY:.2f} (bottom: {fps[r].IsFlipped()})")
    for r in sorted(bottom):
        print(f"bottom {r}: " + " ".join(f"{v:.1f}" for v in bottom[r]))
    print("\n".join(sorted(set(bad))) or "no clashes")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
