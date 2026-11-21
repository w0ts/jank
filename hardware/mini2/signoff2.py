# SPDX-License-Identifier: GPL-3.0-only
"""Sign-off stage 2 (2026-11-17, worktrees/mini22-review-scratch/REVIEW.md): the approved fixes applied to the ROUTED
board in place, not by a new Freerouting run (the routed board carries review fixes no generator holds):
  R1  C72 / C92 (2 x 100 uF) from VSYS to VREG, after the power switch, fitted in both orders (parts.py), behind
      R155 1 ohm (VREG_B: the usb order's inrush, mots's pick);
  R6  C104 (10 uF 0402) at U18's VIN / GND; the bottom under U2's loops (U2, C3, C5, L4) kept free of tracks (VBUS);
  C2  C97 / C103 (100 nF) at LED2 / LED4: one per LED;
  R5  CC2 and the USB pair's / CC1's vias out from under J1's shell: TRIED, NOT DONE. With the shell kept free (the
      J1_* keep-outs) the grid router finds no way: CC2's pad (B5) is boxed in by VUSB's track (B4A9) and UDP's
      via below it, and the pair's A6 / B6 crossover needs a via between 0.5 mm-pitch escapes (a 0.6 mm via does not
      fit). Two layers and this receptacle leave them where they are (tented, as review 2 accepted).
Signoff3 (round 3, worktrees/mini22-round3-scratch/REVIEW3.md), the same way: C3 0603 -> 0805 (R4) replaced in place,
R155 moved 1.6 mm in from the edge (R10) and R156 beside it (R155 // R156 = 0.5 ohm, mots's), up to 3 vias at each layer change of BAT_P / BAT_C / BAT_N (R3: stitch_vias;
fatten.py widens their copper); R136, Q10..Q13 and R10 / R11 are value / part changes only (parts.py).
Run it ONCE on the board it starts from (signoff3: tag mini2.2-signoff2's mini2.kicad_pcb), then fatten.py: a second
pass on its own output re-trims and adds stitching vias where fatten.py's wider copper changed the room.
What it does: every part of parts.py on the board with its nets (a new part from lib/chip.pretty at its floorplan.json
place); every track or via that KiCad's DRC then finds in a short or a clearance error ripped, and the VBUS run under
U2; the open pairs joined by route.py's grid router (repair.build: every pad, track and via an obstacle), with the
keep-outs below; then make_fab.sh's own repair.py (stubs) and fatten.py, and ROUTE=0 for the checks.
  .../python3.9 signoff2.py mini2.kicad_pcb
"""
import json
import math
import os
import re
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import parts as P
import repair
import route

KC = "/opt/homebrew/bin/kicad-cli"
LIB = os.path.join(HERE, "lib", "chip.pretty")
OX = OY = 100.0
MM, TO = pcbnew.FromMM, pcbnew.ToMM
FLOOR = json.load(open(os.path.join(HERE, "floorplan.json")))
U2_LOOPS = (15.8, 42.6, 21.0, 50.1)        # (U2, C3, C5, L4: no track on the bottom under them, review 2's A1)
J1_SHELL = (31.3, 0.0, 40.7, 6.75)         # (J1's body above its pad row: no new track on top under it)
J1_VIAS = (31.3, 0.0, 40.7, 8.2)           # (no new via centre here: its ring stays past the pad row's end, y 7.9)
WIDTH = dict(repair.WIDTH, GND=(0.3, "mid"), VLED=(0.3, "mid"), VREG_B=(0.5, "pwr"))
RIP_VIAS_NEAR = ("C97", "C103", "C104", "R155", "R156", "C3")    # (no via in or within 0.2 mm of a new part's pads, its own net's too)
DRAWN = [   # (net, layer, x0, y0, x1, y1, width): LED2's GND pad -> C97's -> where the GND via was (ripped: C97's pad);
    #  the pour joins it to the via at (14.41, 53.86)
    ("GND", pcbnew.F_Cu, 11.36, 54.05, 13.0, 54.37, 0.3), ("GND", pcbnew.F_Cu, 13.0, 54.37, 13.71, 54.26, 0.3),
    # (C104's ground pad straight to U18's pin 4, and a via there to the bottom: its two vias sat where C104 is)
    ("GND", pcbnew.F_Cu, 16.15, 61.12, 17.16, 61.45, 0.3)]
DRAWN_VIAS = [("GND", 16.7, 61.95)]
PAIRS = []  # (net, a, b): pieces to join that DRC reports only as pour islands
GRAVE = []                                 # (removed items kept referenced: a removed item's wrapper freed by Python
                                           #  breaks the board's SWIG iterators, "SwigPyObject is not iterable")


def remove(board, item):
    board.Remove(item)
    GRAVE.append(item)


def xy(v):
    return TO(v.x) - OX, TO(v.y) - OY


def netinfo(board, name):
    n = board.FindNet(name)
    if n is None:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
    return n


def sync_parts(board):
    """parts.py's nets on every pad; a part missing from the board added at its floorplan.json place; a part whose
    footprint changed (signoff3: C3 0603 -> 0805) replaced there; a part whose floorplan.json place differs from the
    board's moved to it (signoff3: R155); the values as parts.py's"""
    fps = {f.GetReference(): f for f in board.GetFootprints()}
    added, renet = [], []
    for ref, _k, value, fp, _lcsc, pins in P.PARTS:
        f = fps.get(ref)
        if f is not None and f.GetFPID().GetLibItemName().wx_str() != P.FP[fp]:
            remove(board, f)
            added.append(f"{ref} (new footprint {P.FP[fp]})")
            f = None
        if f is not None and ref in FLOOR and not f.IsFlipped():
            x, y, rot = FLOOR[ref]
            bx, by = xy(f.GetPosition())
            if abs(bx - x) > 0.005 or abs(by - y) > 0.005 or abs((f.GetOrientationDegrees() - rot) % 360) > 0.1:
                f.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
                f.SetOrientationDegrees(rot)
                added.append(f"{ref} (moved to {x}, {y}, {rot})")
        if f is not None and f.GetValue() != value:
            f.SetValue(value)
        if f is None:
            f = pcbnew.FootprintLoad(LIB, P.FP[fp])
            f.SetFPID(pcbnew.LIB_ID("chip", P.FP[fp]))
            f.SetReference(ref)
            f.SetValue(value)
            x, y, rot = FLOOR[ref]
            f.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
            f.Reference().SetVisible(False)
            f.Value().SetVisible(False)
            board.Add(f)
            f.SetOrientationDegrees(rot)
            added.append(ref)
        for p in f.Pads():
            n = pins.get(p.GetNumber())
            if n and p.GetNetname() != n:
                renet.append(f"{ref}.{p.GetNumber()} {p.GetNetname()} -> {n}")
                p.SetNet(netinfo(board, n))
    gone = [r for r in fps if r not in {p[0] for p in P.PARTS}]
    for r in gone:
        remove(board, fps[r])
    return added, renet, gone


def drc(path, name):
    rpt = os.path.join(HERE, "fab", name)
    subprocess.run(["nice", "-n", "19", KC, "pcb", "drc", "--severity-all", "-o", rpt, path], capture_output=True)
    return open(rpt).read()


def items_in(text, kinds):
    """(kind, x, y, net) of every track / via named in the DRC report's blocks of these kinds"""
    out = []
    for block in re.split(r"\n(?=\[)", text):
        m = re.match(r"\[(\w+)\]", block)
        if not m or m.group(1) not in kinds:
            continue
        for x, y, what, net, length in re.findall(
                r"@\(([-\d.]+) mm, ([-\d.]+) mm\): (Track|Via)[^\[]*\[([^\]]*)\][^\n]*?(?:length ([\d.]+) mm)?\n", block + "\n"):
            out.append((what, float(x) - OX, float(y) - OY, net, float(length) if length else None))
    return out


def find(board, what, x, y, net, length):
    """the track (of that length) or via KiCad's report names at (x, y): at a junction the first hit is not it"""
    at = pcbnew.VECTOR2I(MM(OX + x), MM(OY + y))
    hits = [t for t in board.GetTracks() if t.GetNetname() == net and (t.GetClass() == "PCB_VIA") == (what == "Via")
            and t.HitTest(at, MM(0.01))]
    if what == "Track" and length is not None:
        hits.sort(key=lambda t: abs(TO(t.GetLength()) - length))
    return hits[0] if hits else None


def rip_errors(board, path):
    """tracks and vias in a short or a clearance error after the sync: removed (their nets joined again below)"""
    board.Save(path)
    text = drc(path, "signoff2.rpt")
    hits = items_in(text, ("shorting_items", "clearance", "hole_clearance", "tracks_crossing", "copper_edge_clearance"))
    n = 0
    for item in hits:
        t = find(board, *item)
        if t is not None:
            remove(board, t)
            n += 1
    return n, len(hits)


def rip_vias_near(board, refs, gap=0.2):
    n = 0
    pads = [p for f in board.GetFootprints() if f.GetReference() in refs for p in f.Pads()]
    for v in [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]:
        vx, vy = xy(v.GetPosition())
        r = TO(v.GetWidth(pcbnew.F_Cu)) / 2 + gap
        for p in pads:
            b = p.GetBoundingBox()
            x0, y0, x1, y1 = TO(b.GetLeft()) - OX, TO(b.GetTop()) - OY, TO(b.GetRight()) - OX, TO(b.GetBottom()) - OY
            if math.hypot(max(x0 - vx, 0, vx - x1), max(y0 - vy, 0, vy - y1)) < r:
                remove(board, v)
                n += 1
                break
    return n


def seg_in(t, box, margin=0.0):
    (x0, y0), (x1, y1) = xy(t.GetStart()), xy(t.GetEnd())
    bx0, by0, bx1, by1 = box[0] - margin, box[1] - margin, box[2] + margin, box[3] + margin
    for k in range(21):
        x, y = x0 + (x1 - x0) * k / 20, y0 + (y1 - y0) * k / 20
        if bx0 <= x <= bx1 and by0 <= y <= by1:
            return True
    return False


def rip_bottom_under_u2(board):
    """the bottom tracks crossing U2's loops' box (+ 1 mm, so no stub ends inside it); nothing when none crosses the box
    itself (a second run keeps the first run's routes)"""
    if not any(t.GetClass() == "PCB_TRACK" and t.GetLayer() == pcbnew.B_Cu and t.GetNetname() != "GND"
               and seg_in(t, U2_LOOPS) for t in board.GetTracks()):
        return [], 0
    gone = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetLayer() == pcbnew.B_Cu
            and t.GetNetname() != "GND" and seg_in(t, U2_LOOPS, 1.0)]
    for t in gone:
        remove(board, t)
    return sorted({t.GetNetname() for t in gone}), len(gone)


def open_pairs(path):
    """every open pair KiCad's DRC lists, GND too (the board had none before this script)"""
    text = drc(path, "signoff2.rpt")
    out = []
    for block in text.split("[unconnected_items]")[1:]:
        pts = re.findall(r"@\(([-\d.]+) mm, ([-\d.]+) mm\): ((?:Track|Via|Zone|\w* ?[Pp]ad)[^\[]*)\[([^\]]+)\]", block.split("\n[")[0])
        if len(pts) == 2 and pts[0][3] == pts[1][3] and not pts[0][2].startswith("Zone"):   # ("PTH pad" too; pour
                                                                                            #  islands: tie_groups.py)
            out.append((pts[0][3], [(float(x) - OX, float(y) - OY) for x, y, _k, _n in pts]))
    return out


def keepouts(R):
    R.block((1,), U2_LOOPS, ("sig", "mid", "pwr"))
    R.block((0, 1), U2_LOOPS, ("via",))
    R.block((0,), J1_SHELL, ("sig", "mid", "pwr"))
    R.block((0, 1), J1_VIAS, ("via",))


def near_new_pads(R, board, gap=0.2):
    """no via centre where its ring would come within gap of a new / moved part's pad (signoff3 review F2: the join
    router put a via 0.08 mm from R156's pad)"""
    rr = route.VIA_D / 2 + gap
    for f in board.GetFootprints():
        if f.GetReference() in RIP_VIAS_NEAR:
            for p in f.Pads():
                b = p.GetBoundingBox()
                R.block((0, 1), (TO(b.GetLeft()) - OX - rr, TO(b.GetTop()) - OY - rr, TO(b.GetRight()) - OX + rr,
                                 TO(b.GetBottom()) - OY + rr), classes=("via",))


def join(board, path):
    pairs = open_pairs(path) + [(n, [a, b]) for n, a, b in PAIRS]
    print(f"signoff2: {len(pairs)} open pairs: {', '.join(n for n, _ in pairs)}")
    routers = {}
    for k, f in repair.KINDS.items():
        routers[k] = repair.build(board, f)
        keepouts(routers[k])
        near_new_pads(routers[k], board)
    left = []
    for net, (a, b) in pairs:
        width, cls = WIDTH.get(net, (0.2, "sig"))
        starts, goals = repair.cells_at(routers["plain"], board, net, *a), repair.cells_at(routers["plain"], board, net, *b)
        if not starts or not goals:
            left.append(net)
            print(f"  {net}: its two pieces not found at {a} / {b}")
            continue
        xs = [c[1] for c in starts + goals]
        ys = [c[2] for c in starts + goals]
        res = None
        for k in (repair.kind(net), "plain"):
            R = routers[k]
            for mg in (3.0, 8.0, 20.0):
                m = int(mg / route.GRID)
                win = (max(0, min(xs) - m), max(0, min(ys) - m), min(R.nx - 1, max(xs) + m), min(R.ny - 1, max(ys) + m))
                res = R.path(net, cls, starts, goals, win)
                if res:
                    break
            if res:
                break
        if not res:
            left.append(net)
            print(f"  {net}: no path {a} -> {b}")
            continue
        segs, vias = routers["plain"].to_items(res)
        repair.add(board, net, segs, vias, width)
        for k, Rk in routers.items():
            for L, x0, y0, x1, y1 in segs:
                Rk.seg(L, net, (x0, y0, x1, y1), width)
            for x, y in vias:
                Rk.circle((0, 1), net, x, y, route.VIA_D / 2 + route.VIA_EXTRA)
                Rk.drill(x, y, route.VIA_DRILL)
        print(f"  {net}: joined, {sum(math.hypot(s[3] - s[1], s[4] - s[2]) for s in segs):.1f} mm, {len(vias)} vias")
    return left


def dangling(path):
    """(kind, x, y, net) of every dangling track and via KiCad's DRC lists"""
    return {(w, round(x, 3), round(y, 3), n, L) for w, x, y, n, L in
            items_in(drc(path, "signoff2.rpt"), ("track_dangling", "via_dangling"))}


def prune(board, path, before):
    """the stubs and vias the rips left hanging (not the board's own: before), removed until none is new"""
    n = 0
    for _ in range(20):
        board.Save(path)
        new = dangling(path) - before
        if not new:
            break
        for item in new:
            t = find(board, *item)
            if t is not None:
                if item[0] == "Via" or not trim(board, t):
                    remove(board, t)
                n += 1
    return n


def touches(board, t, pt):
    """other copper of t's net at the point pt (a track's end or body, a via, a pad)"""
    me = t.m_Uuid.AsString()
    return any(o.GetNetCode() == t.GetNetCode() and o.m_Uuid.AsString() != me and o.HitTest(pt) for o in board.GetTracks()) \
        or any(p.GetNetCode() == t.GetNetCode() and p.HitTest(pt) for f in board.GetFootprints() for p in f.Pads())


def trim(board, t):
    """a track with a free end that a new route joins along its length: its free end cut back to the join (True);
    False when nothing joins it there (a whole stub: removed)"""
    s, e = t.GetStart(), t.GetEnd()
    if touches(board, t, e) and not touches(board, t, s):
        s, e = e, s
    elif touches(board, t, s) == touches(board, t, e):
        return False
    me = t.m_Uuid.AsString()
    pts = []
    for o in board.GetTracks():
        if o.GetNetCode() != t.GetNetCode() or o.m_Uuid.AsString() == me:
            continue
        if o.GetClass() == "PCB_VIA":            # (a via whose ring meets the track: the track's point under its centre)
            c = o.GetPosition()
            if t.HitTest(c, o.GetWidth(pcbnew.F_Cu) // 2):
                d = e - s
                L2 = float(d.x) ** 2 + float(d.y) ** 2
                k = 0.0 if L2 == 0 else max(0.0, min(1.0, (float(c.x - s.x) * d.x + float(c.y - s.y) * d.y) / L2))
                pts.append(pcbnew.VECTOR2I(int(s.x + k * d.x), int(s.y + k * d.y)))
            continue
        pts += [q for q in (o.GetStart(), o.GetEnd()) if t.HitTest(q, 0)]
    pts = [q for q in pts if (q - s).EuclideanNorm() > MM(0.01)]
    if not pts:
        return False
    keep = min(pts, key=lambda q: (q - e).EuclideanNorm())     # (the join nearest the free end)
    if t.GetStart() == s:
        t.SetEnd(keep)
    else:
        t.SetStart(keep)
    return True


HAND = ("J2", "J3", "J5", "J6", "J7", "J8", "J9", "J10B", "J11", "J13", "J14")   # (check_board.py's list)
STITCH = ("BAT_P", "BAT_C", "BAT_N")      # (signoff3, round 3 R3: 2 .. 3 vias at each layer change of the cell's path)


def free_for(R, cls, net, x, y):
    ix, iy = int(round(x / route.GRID)), int(round(y / route.GRID))
    if not (0 <= ix < R.nx and 0 <= iy < R.ny):
        return False
    n, i = R.nid(net), iy * R.nx + ix
    return all(R.mask[cls][L][i] in (0, n) for L in (0, 1))


def stitch_vias(board, nets=STITCH, want=3, width=0.4):
    """beside each via of these nets, more vias of its net (0.9 .. 1.1 mm away, joined to it by a short track on both
    layers) until it has want within 1.6 mm; only where the grid (repair.build: every pad, track, via, iron room, drill
    spacing) is free on both layers for the via and the joins"""
    R = repair.build(board)
    keepouts(R)
    near_new_pads(R, board)
    for f in board.GetFootprints():            # (check_board's hand-soldered parts: no via in their iron's room, SMD lands too)
        if f.GetReference() in HAND:
            for p in f.Pads():
                px, py = xy(p.GetPosition())
                rr = max(TO(p.GetSize().x), TO(p.GetSize().y)) / 2 + 0.8 + route.VIA_D / 2
                R.block((0, 1), (px - rr, py - rr, px + rr, py + rr), classes=("via",))
    have = [(v.GetNetname(), xy(v.GetPosition())) for v in board.GetTracks() if v.GetClass() == "PCB_VIA" and v.GetNetname() in nets]
    added = {}
    for net, (vx, vy) in list(have):
        need = want - sum(1 for n, (x, y) in have if n == net and math.hypot(x - vx, y - vy) < 1.6)
        for r in (0.9, 1.0, 1.1):
            for k in range(16):
                if need <= 0:
                    break
                a = 2 * math.pi * k / 16
                cx, cy = round(vx + r * math.cos(a), 3), round(vy + r * math.sin(a), 3)
                pts = [(vx + (cx - vx) * j / 20, vy + (cy - vy) * j / 20) for j in range(21)]
                if not free_for(R, "via", net, cx, cy) or not all(free_for(R, "mid", net, x, y) for x, y in pts):
                    continue
                segs = [(L, vx, vy, cx, cy) for L in (0, 1)]
                repair.add(board, net, segs, [(cx, cy)], width)
                for L, x0, y0, x1, y1 in segs:
                    R.seg(L, net, (x0, y0, x1, y1), width)
                R.circle((0, 1), net, cx, cy, route.VIA_D / 2 + route.VIA_EXTRA)
                R.drill(cx, cy, route.VIA_DRILL)
                have.append((net, (cx, cy)))
                added[net] = added.get(net, 0) + 1
                need -= 1
    return added


def main():
    path = sys.argv[1]
    before = dangling(path)
    board = pcbnew.LoadBoard(path)
    added, renet, gone = sync_parts(board)
    print(f"signoff2: added {added}; nets {renet}; removed {gone}")
    board.Save(path)                           # (reloaded: a footprint from FootprintLoad leaves the SWIG wrappers of
    board = pcbnew.LoadBoard(path)             #  this board unusable, GetTracks() fails)
    print(f"signoff2: {rip_vias_near(board, RIP_VIAS_NEAR)} vias by the new parts' pads removed")
    nets, n = rip_bottom_under_u2(board)
    print(f"signoff2: {n} bottom tracks under U2's loops removed ({', '.join(nets)})")
    for rnd in range(3):                       # (a rip can uncover the next error behind it)
        n, hits = rip_errors(board, path)
        print(f"signoff2: round {rnd}: {hits} DRC items, {n} tracks / vias removed")
        if not n:
            break
    have = {(t.GetNetname(), t.GetClass(), xy(t.GetStart()), xy(t.GetEnd())) for t in board.GetTracks()}
    near = lambda a, b: abs(a[0] - b[0]) < 0.005 and abs(a[1] - b[1]) < 0.005
    for net, layer, x0, y0, x1, y1, w in DRAWN:                 # (each once: a second run adds nothing)
        if not any(n == net and c == "PCB_TRACK" and near(s0, (x0, y0)) and near(e0, (x1, y1)) for n, c, s0, e0 in have):
            repair.add(board, net, [(0 if layer == pcbnew.F_Cu else 1, x0, y0, x1, y1)], [], w)
    for net, x, y in DRAWN_VIAS:
        if not any(n == net and c == "PCB_VIA" and near(s0, (x, y)) for n, c, s0, _e in have):
            repair.add(board, net, [], [(x, y)], 0.0)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(path)
    board = pcbnew.LoadBoard(path)
    left = join(board, path)
    print(f"signoff2: {prune(board, path, before)} stubs / vias left hanging by the rips removed")
    print(f"signoff2: stitching vias added on the cell's path: {stitch_vias(board)}")
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(path)
    print(f"signoff2: not joined: {left}; {repair.unconnected(board)} connections open (KiCad's count)")
    os.remove(os.path.join(HERE, "fab", "signoff2.rpt"))


if __name__ == "__main__":
    main()
