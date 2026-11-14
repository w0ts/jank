# SPDX-License-Identifier: GPL-3.0-only
"""After Freerouting (72 x 72, stage 2): the few connections it leaves open, joined with the grid router of route.py on
the routed board (every pad, track and via of another net an obstacle; the RF parts' bottom and the hand-soldered
parts' iron rooms kept free of vias), then the board's leftovers tidied: a GND via on a twin's pad, a GND via within
0.5 mm (drill to drill) of another via, a dangling stub: each removed when the board's open count stays the same.
KiCad's DRC finds the open pairs (fab/repair.rpt); the result is checked by make_fab.sh's DRC, not trusted.
  .../python3.9 repair.py mini2.kicad_pcb
"""
import math
import os
import re
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import route
from parts import TWINS

KC = "/opt/homebrew/bin/kicad-cli"
OX = OY = 100.0
MM, TO = pcbnew.FromMM, pcbnew.ToMM
WIDTH = {"VBUS": (0.3, "mid"), "VUSB": (0.3, "mid"), "+3V3": (0.25, "mid"), "VSYS": (0.5, "pwr"), "VREG": (0.5, "pwr")}
RF = ("LNA_IN", "RF", "UFL")
IRON = 0.8


def xy(v):
    return TO(v.x) - OX, TO(v.y) - OY


def open_pairs(path):
    rpt = os.path.join(HERE, "fab", "repair.rpt")
    subprocess.run(["nice", "-n", "19", KC, "pcb", "drc", "--severity-all", "-o", rpt, path], capture_output=True)
    text = open(rpt).read()
    out = []
    for block in text.split("[unconnected_items]")[1:]:
        pts = re.findall(r"@\(([-\d.]+) mm, ([-\d.]+) mm\): (Track|Pad[^\[]*|Via[^\[]*) \[([^\]]+)\]", block.split("\n[")[0])
        if len(pts) == 2 and pts[0][3] == pts[1][3] and pts[0][3] != "GND":
            out.append((pts[0][3], [(float(x) - OX, float(y) - OY) for x, y, _k, _n in pts]))
    return out


def build(board):
    bb = board.GetBoardEdgesBoundingBox()
    R = route.Router(TO(bb.GetWidth()), TO(bb.GetHeight()))
    for f in board.GetFootprints():
        for p in f.Pads():
            b = p.GetBoundingBox()
            r = (TO(b.GetLeft()) - OX, TO(b.GetTop()) - OY, TO(b.GetRight()) - OX, TO(b.GetBottom()) - OY)
            layers = [L for L, cu in ((0, pcbnew.F_Cu), (1, pcbnew.B_Cu)) if p.IsOnLayer(cu)]
            n = p.GetNetname() or None
            if layers:
                R.rect(layers, n, r)
            if p.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                cx, cy = xy(p.GetPosition())
                rr = max(TO(p.GetSize().x), TO(p.GetSize().y)) / 2 + IRON
                R.block((0, 1), (cx - rr, cy - rr, cx + rr, cy + rr), classes=("via",))
                R.drill(cx, cy, TO(p.GetDrillSize().x))
            if f.GetReference() in TWINS:            # (no via on a twin's pad)
                R.block((0, 1), r, classes=("via",))
            g = route.VIA_D / 2 + 0.05                # (no via in or touching any pad, its own net's too)
            R.block((0, 1), (r[0] - g, r[1] - g, r[2] + g, r[3] + g), classes=("via",))
            if p.GetNetname() in RF:                 # (the RF parts' bottom stays ground)
                R.block((1,), (r[0] - 1.0, r[1] - 1.0, r[2] + 1.0, r[3] + 1.0))
    for t in board.GetTracks():
        n = t.GetNetname() or None
        if t.GetClass() == "PCB_VIA":
            x, y = xy(t.GetPosition())
            R.circle((0, 1), n, x, y, TO(t.GetWidth(pcbnew.F_Cu)) / 2)
            R.drill(x, y, TO(t.GetDrillValue()))
        else:
            (x0, y0), (x1, y1) = xy(t.GetStart()), xy(t.GetEnd())
            R.seg(0 if t.GetLayer() == pcbnew.F_Cu else 1, n, (x0, y0, x1, y1), TO(t.GetWidth()))
    return R


def cells_at(R, board, net, x, y):
    """the grid cells of the copper of net under the point (x, y): the whole item it belongs to"""
    at = pcbnew.VECTOR2I(MM(OX + x), MM(OY + y))
    out = set()
    for f in board.GetFootprints():
        for p in f.Pads():
            if p.GetNetname() == net and p.HitTest(at):
                b = p.GetBoundingBox()
                r = (TO(b.GetLeft()) - OX, TO(b.GetTop()) - OY, TO(b.GetRight()) - OX, TO(b.GetBottom()) - OY)
                layers = [L for L, cu in ((0, pcbnew.F_Cu), (1, pcbnew.B_Cu)) if p.IsOnLayer(cu)]
                out |= set(R._cells_in(r, layers))
    for t in board.GetTracks():
        if t.GetNetname() == net and t.HitTest(at):
            if t.GetClass() == "PCB_VIA":
                cx, cy = xy(t.GetPosition())
                out |= {(L, int(round(cx / route.GRID)), int(round(cy / route.GRID))) for L in (0, 1)}
                continue
            (x0, y0), (x1, y1) = xy(t.GetStart()), xy(t.GetEnd())
            L = 0 if t.GetLayer() == pcbnew.F_Cu else 1
            n = max(1, int(math.hypot(x1 - x0, y1 - y0) / route.GRID))
            out |= {(L, int(round((x0 + (x1 - x0) * k / n) / route.GRID)), int(round((y0 + (y1 - y0) * k / n) / route.GRID)))
                    for k in range(n + 1)}
    return sorted(out)


def add(board, net, segs, vias, width):
    ni = board.FindNet(net)
    for L, x0, y0, x1, y1 in segs:
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(pcbnew.VECTOR2I(MM(OX + x0), MM(OY + y0)))
        t.SetEnd(pcbnew.VECTOR2I(MM(OX + x1), MM(OY + y1)))
        t.SetWidth(MM(width))
        t.SetLayer(pcbnew.F_Cu if L == 0 else pcbnew.B_Cu)
        t.SetNet(ni)
        board.Add(t)
    for x, y in vias:
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
        v.SetDrill(MM(route.VIA_DRILL))
        v.SetWidth(MM(route.VIA_D))
        v.SetNet(ni)
        board.Add(v)


def unconnected(board):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.BuildConnectivity()
    return board.GetConnectivity().GetUnconnectedCount(True)


def try_remove(board, item):
    before = unconnected(board)
    board.Remove(item)
    if unconnected(board) > before:
        board.Add(item)
        return False
    return True


def tidy(board):
    twin_pads = [p for f in board.GetFootprints() if f.GetReference() in TWINS for p in f.Pads()]
    vias = [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    on_twin = [v for v in vias if v.GetNetname() == "GND" and any(p.HitTest(v.GetPosition()) or
               (p.GetPosition() - v.GetPosition()).EuclideanNorm() < MM(0.3) + max(p.GetSize().x, p.GetSize().y) // 2
               for p in twin_pads)]
    n1 = sum(try_remove(board, v) for v in on_twin)
    close = [v for v in vias if v.GetNetname() == "GND" and any(o is not v and 0 < (v.GetPosition() - o.GetPosition()).EuclideanNorm()
             < MM(0.5) + (v.GetDrillValue() + o.GetDrillValue()) // 2 for o in vias)]
    n2 = sum(try_remove(board, v) for v in close)
    return n1, n2, len(on_twin) - n1


def free_end(board, t):
    """True when one end of the track t touches no other copper of its net (a pad, a via, another track): a stub; a
    short piece between two others (a 45-degree corner) is not one"""
    for end in (t.GetStart(), t.GetEnd()):
        hit = any(o is not t and o.GetNetCode() == t.GetNetCode() and o.HitTest(end) for o in board.GetTracks())
        hit = hit or any(p.GetNetCode() == t.GetNetCode() and p.HitTest(end) for f in board.GetFootprints() for p in f.Pads())
        if not hit:
            return True
    return False


def twin_courtyards(board):
    """a twin is two bare lands beside its own part, not an assembled part: its courtyard (pads + 0.5 mm, used to place it
    1.0 mm clear of the other parts: make_board.place_twins, check_board.twin_copper) cut back to its pads + 0.05 mm,
    so KiCad's courtyard check does not flag it against its own part, which it abuts by design"""
    n = 0
    for f in board.GetFootprints():
        if f.GetReference() not in TWINS:
            continue
        boxes = [p.GetBoundingBox() for p in f.Pads()]                    # (board-aligned: twins sit at 0 / 90 / ...)
        m = MM(0.05)
        a = pcbnew.VECTOR2I(min(b.GetLeft() for b in boxes) - m, min(b.GetTop() for b in boxes) - m)
        c = pcbnew.VECTOR2I(max(b.GetRight() for b in boxes) + m, max(b.GetBottom() for b in boxes) + m)
        for g in f.GraphicalItems():
            if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd) and g.GetShape() == pcbnew.SHAPE_T_RECT:
                g.SetStart(a)
                g.SetEnd(c)
                n += 1
    return n


def main():
    path = sys.argv[1]
    board = pcbnew.LoadBoard(path)
    pairs = open_pairs(path)
    print(f"repair: {len(pairs)} open pairs: {', '.join(n for n, _ in pairs)}")
    R = build(board)
    for net, (a, b) in pairs:
        width, cls = WIDTH.get(net, (0.2, "sig"))
        starts, goals = cells_at(R, board, net, *a), cells_at(R, board, net, *b)
        if not starts or not goals:
            print(f"  {net}: its two pieces not found at {a} / {b}")
            continue
        xs = [c[1] for c in starts + goals]
        ys = [c[2] for c in starts + goals]
        res = None
        for mg in (4.0, 10.0, 25.0):
            m = int(mg / route.GRID)
            win = (max(0, min(xs) - m), max(0, min(ys) - m), min(R.nx - 1, max(xs) + m), min(R.ny - 1, max(ys) + m))
            res = R.path(net, cls, starts, goals, win)
            if res:
                break
        if not res:
            print(f"  {net}: no path")
            continue
        segs, vias = R.to_items(res)
        add(board, net, segs, vias, width)
        for L, x0, y0, x1, y1 in segs:
            R.seg(L, net, (x0, y0, x1, y1), width)
        for x, y in vias:
            R.circle((0, 1), net, x, y, route.VIA_D / 2 + route.VIA_EXTRA)
            R.drill(x, y, route.VIA_DRILL)
        print(f"  {net}: joined, {sum(math.hypot(s[3] - s[1], s[4] - s[2]) for s in segs):.1f} mm, {len(vias)} vias")
    print(f"repair: {twin_courtyards(board)} twins' courtyards cut back to their pads")
    n1, n2, kept = tidy(board)
    text = open(os.path.join(HERE, "fab", "repair.rpt")).read()
    stubs = 0
    for block in text.split("[track_dangling]")[1:]:
        m = re.search(r"@\(([-\d.]+) mm, ([-\d.]+) mm\): Track \[([^\]]+)\]", block)
        if m:
            at = pcbnew.VECTOR2I(MM(float(m.group(1))), MM(float(m.group(2))))
            for t in [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetNetname() == m.group(3)
                      and t.HitTest(at) and t.GetLength() < MM(5.0) and free_end(board, t)]:
                stubs += try_remove(board, t)
    print(f"repair: {stubs} dangling stubs removed")
    print(f"repair: {n1} GND vias off twins' pads, {n2} GND vias too close to another removed ({kept} kept: needed)")
    print(f"repair: {unconnected(board)} connections open (KiCad's count)")
    board.Save(path)


if __name__ == "__main__":
    main()
