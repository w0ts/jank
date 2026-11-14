# SPDX-License-Identifier: GPL-3.0-only
"""The checks KiCad's DRC does not make (README "Checks"), on the placed or the routed board:
- courtyards as boxes, top and bottom: no two parts' boxes overlap on one side (KiCad's DRC lets 0.01 .. 0.05 mm touches
  through); the PH2.0 sockets (J8 / J9) counted either way round (the user may solder one turned 180 degrees);
- the iron's room: no top part's box within 0.8 mm of a through-hole pad's copper (the DINs, the jacks, the sockets, the
  screen's header: the user solders them from the top);
- the screen module's outline (make_board.MODULE): no button, LED or tall part under it;
- vias: none in or touching an SMD pad (0.05 mm), none within the iron's room of a DIN or jack pad;
- the twins (parts.TWINS): each pad joined to its resistor's pad of the same net by a top track, and no via on a twin's
  pads;
- the ground: every piece of a GND pour reaches another layer's GND through a via or a through-hole pad (no island);
- the RF keep-outs: under the RF tracks (0.4 mm round) no bottom-layer track but GND; beside them on top no other
  net's track within 0.5 mm (the coplanar line's gap 0.2 + the fence), away from the chip's own pins.
  .../python3.9 check_board.py mini2.kicad_pcb        (KiCad's Python); exit 1 on any problem
"""
import math
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True
import parts as P

MM = pcbnew.ToMM
OX = OY = 100.0
IRON = 0.8
MODULE = (38.0 - 27.3 / 2, 15.9 - 1.5, 38.0 + 27.3 / 2, 15.9 - 1.5 + 27.8)   # (make_board.MODULE: HDR (38.0, 15.9) on 72 x 72)
TALL = ("J1", "J4", "U2", "D7", "D8")


def xy(v):
    return MM(v.x) - OX, MM(v.y) - OY


def box(f):
    b = f.GetCourtyard(pcbnew.B_CrtYd if f.IsFlipped() else pcbnew.F_CrtYd).BBox()
    return [MM(b.GetLeft()) - OX, MM(b.GetTop()) - OY, MM(b.GetRight()) - OX, MM(b.GetBottom()) - OY]


def hit(a, c):
    return a[0] < c[2] and c[0] < a[2] and a[1] < c[3] and c[1] < a[3]


def dist_box(b, x, y):
    return math.hypot(max(b[0] - x, 0, x - b[2]), max(b[1] - y, 0, y - b[3]))


def courtyards(fps, problems):
    boxes = {r: box(f) for r, f in fps.items()}
    for r in ("J8", "J9"):
        if r in fps:
            cx, cy = xy(fps[r].GetPosition())
            x0, y0, x1, y1 = boxes[r]
            boxes[r] = [min(x0, 2 * cx - x1), min(y0, 2 * cy - y1), max(x1, 2 * cx - x0), max(y1, 2 * cy - y0)]
    refs = sorted(boxes)
    for i, a in enumerate(refs):
        for c in refs[i + 1:]:
            if P.TWINS.get(a, (None,))[0] == c or P.TWINS.get(c, (None,))[0] == a:
                continue                         # (a twin and its own resistor: twin_copper() checks them)
            if fps[a].IsFlipped() == fps[c].IsFlipped() and hit(boxes[a], boxes[c]):
                problems.append(f"courtyards overlap ({'bottom' if fps[a].IsFlipped() else 'top'}): {a} {c}")
    rooms = [(r, *xy(p.GetPosition()), max(MM(p.GetSize().x), MM(p.GetSize().y)) / 2)
             for r, f in fps.items() for p in f.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    for r in refs:
        if fps[r].IsFlipped():
            continue
        for o, x, y, rad in rooms:
            if o != r and dist_box(boxes[r], x, y) < rad + IRON - 1e-6:
                problems.append(f"{r} in {o}'s iron room ({dist_box(boxes[r], x, y) - rad:.2f} mm from its pad)")
                break
    for r in refs:
        if not fps[r].IsFlipped() and (r.startswith(("SW", "LED")) or r in TALL) and hit(boxes[r], MODULE):
            problems.append(f"{r} under the screen module's outline")
    return boxes


def twin_copper(fps, problems):
    """stage 2: a twin is soldered by hand: its pads 1.0 mm or more from every other part's copper (its own resistor:
    0.3 mm from the other net's pad)"""
    def pb(p):
        b = p.GetBoundingBox()
        return [MM(b.GetLeft()) - OX, MM(b.GetTop()) - OY, MM(b.GetRight()) - OX, MM(b.GetBottom()) - OY]

    def d(a, b):
        return math.hypot(max(a[0] - b[2], b[0] - a[2], 0), max(a[1] - b[3], b[1] - a[3], 0))
    for t, (m, _why) in P.TWINS.items():
        if t not in fps:
            continue
        cu = pcbnew.B_Cu if fps[t].IsFlipped() else pcbnew.F_Cu   # (72 x 72: some twins on the bottom)
        for q in fps[t].Pads():
            qb = pb(q)
            for r, f in fps.items():
                if r == t:
                    continue
                for o in f.Pads():
                    if not o.IsOnLayer(cu):
                        continue
                    need = (0.3 if o.GetNetname() != q.GetNetname() else 0.0) if r == m else 1.0
                    if d(qb, pb(o)) < need - 1e-6:
                        problems.append(f"twin {t}.{q.GetNumber()} {d(qb, pb(o)):.2f} mm from {r}.{o.GetNumber()} (needs {need})")


PIVOT = ((61.5, 62.2), 1.5)                      # (make_board.BTN_C / PIVOT_R: the D-pad's pivot)


def pivot(board, fps, boxes, problems):
    (px, py), pr = PIVOT
    for r, b in boxes.items():
        if not fps[r].IsFlipped() and dist_box(b, px, py) < pr:
            problems.append(f"{r} in the D-pad's pivot circle")
    for v in board.GetTracks():
        if v.GetClass() == "PCB_VIA":
            x, y = xy(v.GetPosition())
            if math.hypot(x - px, y - py) < pr + MM(v.GetWidth(pcbnew.F_Cu)) / 2:
                problems.append(f"via in the D-pad's pivot circle ({x:.2f}, {y:.2f})")


def vias(board, fps, problems):
    smd = [(r, p) for r, f in fps.items() for p in f.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD]
    tht = [(r, p) for r, f in fps.items() for p in f.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH
           and r in ("J2", "J3", "J5", "J6", "J7", "J11", "J13", "J14")]
    n = 0
    for v in board.GetTracks():
        if v.GetClass() != "PCB_VIA":
            continue
        n += 1
        x, y = xy(v.GetPosition())
        rv = MM(v.GetWidth(pcbnew.F_Cu)) / 2
        for r, p in smd:
            b = p.GetBoundingBox()
            bx = [MM(b.GetLeft()) - OX, MM(b.GetTop()) - OY, MM(b.GetRight()) - OX, MM(b.GetBottom()) - OY]
            if dist_box(bx, x, y) < rv + 0.05:
                if r in P.TWINS:
                    problems.append(f"via on twin {r}'s pad {p.GetNumber()} ({x:.2f}, {y:.2f})")
                elif r not in ("C23", "C24", "C28", "J4", "U5", "U1"):   # (freeroute.PAD_VIA_PARTS and the chip's pad)
                    problems.append(f"via in or touching {r}.{p.GetNumber()} ({x:.2f}, {y:.2f})")
        for r, p in tht:
            px, py = xy(p.GetPosition())
            if math.hypot(x - px, y - py) < max(MM(p.GetSize().x), MM(p.GetSize().y)) / 2 + IRON + rv - 1e-6:
                problems.append(f"via in {r}'s iron room ({x:.2f}, {y:.2f})")
    return n


def twins(board, fps, problems):
    tracks = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetLayer() == pcbnew.F_Cu]
    bottom = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetLayer() == pcbnew.B_Cu]
    for t, (m, _why) in P.TWINS.items():
        if t not in fps:
            problems.append(f"twin {t} not on the board")
            continue
        if fps[t].IsFlipped():                    # (a bottom twin: its non-GND pads each reached by a bottom track)
            for q in fps[t].Pads():
                if q.GetNetname() != "GND" and not any(s.GetNetname() == q.GetNetname() and (
                        (s.GetStart() - q.GetPosition()).EuclideanNorm() < pcbnew.FromMM(0.05) or
                        (s.GetEnd() - q.GetPosition()).EuclideanNorm() < pcbnew.FromMM(0.05)) for s in bottom):
                    problems.append(f"bottom twin {t}.{q.GetNumber()} not joined by its bottom track")
            continue
        for q in fps[t].Pads():
            mp = next(z for z in fps[m].Pads() if z.GetNumber() == q.GetNumber())
            a, b = q.GetPosition(), mp.GetPosition()
            tol = pcbnew.FromMM(0.01)

            def near(u, v):
                return abs(u.x - v.x) <= tol and abs(u.y - v.y) <= tol
            if not any((near(s.GetStart(), a) and near(s.GetEnd(), b)) or (near(s.GetStart(), b) and near(s.GetEnd(), a))
                       for s in tracks):
                problems.append(f"twin {t}.{q.GetNumber()} not joined to {m}.{q.GetNumber()} by its own top track")


def twin_tracks_cross(board, fps, problems):
    """a twin's two tracks must not cross each other (pad 1 to pad 1, pad 2 to pad 2 drawn straight)"""
    def ccw(a, b, c):
        return (c[1] - a[1]) * (b[0] - a[0]) > (b[1] - a[1]) * (c[0] - a[0])
    for t, (m, _why) in P.TWINS.items():
        if t not in fps or fps[t].IsFlipped():
            continue
        segs = []
        for q in fps[t].Pads():
            mp = next(z for z in fps[m].Pads() if z.GetNumber() == q.GetNumber())
            segs.append((xy(q.GetPosition()), xy(mp.GetPosition())))
        (a, b), (c, d) = segs
        if ccw(a, c, d) != ccw(b, c, d) and ccw(a, b, c) != ccw(a, b, d):
            problems.append(f"twin {t}'s two tracks cross (a short between its nets)")


def islands(board, problems):
    """each filled GND polygon must hold a via or a through-hole pad of GND (its tie to the other layer)"""
    ties = [(v.GetPosition(), v.GetWidth(pcbnew.F_Cu) // 2) for v in board.GetTracks()
            if v.GetClass() == "PCB_VIA" and v.GetNetname() == "GND"]
    ties += [(p.GetPosition(), max(p.GetSize().x, p.GetSize().y) // 2 + pcbnew.FromMM(0.6)) for f in board.GetFootprints()
             for p in f.Pads() if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]   # (spokes)
    n = 0
    for z in board.Zones():
        if z.GetNetname() != "GND" or z.GetIsRuleArea():
            continue
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
            if not z.IsOnLayer(layer):
                continue
            poly = z.GetFilledPolysList(layer)
            for i in range(poly.OutlineCount()):
                n += 1
                one = pcbnew.SHAPE_POLY_SET()
                one.AddOutline(poly.Outline(i))
                o = poly.Outline(i)
                if not any(one.Collide(t, r) for t, r in ties):
                    c = o.BBox().Centre()
                    area = abs(o.Area()) / 1e12
                    problems.append(f"GND island on {'top' if layer == pcbnew.F_Cu else 'bottom'} at "
                                    f"({MM(c.x) - OX:.2f}, {MM(c.y) - OY:.2f}), {area:.2f} mm2: no via")
    return n


RF_NETS = ("LNA_IN", "RF", "UFL")


def seg_dist(t, x, y):
    ax, ay = xy(t.GetStart())
    bx, by = xy(t.GetEnd())
    dx, dy = bx - ax, by - ay
    L = dx * dx + dy * dy
    u = 0.0 if L == 0 else max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / L))
    return math.hypot(ax + u * dx - x, ay + u * dy - y)


def seg_points(t, n=10):
    (ax, ay), (bx, by) = xy(t.GetStart()), xy(t.GetEnd())
    return [(ax + (bx - ax) * k / n, ay + (by - ay) * k / n, MM(t.GetWidth()) / 2) for k in range(n + 1)]


def rf(board, fps, problems):
    tracks = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK"]
    rft = [t for t in tracks if t.GetNetname() in RF_NETS]
    pts = [xy(p.GetPosition()) for f in fps.values() if f.GetReference() != "U1" for p in f.Pads() if p.GetNetname() in RF_NETS]
    pts += [xy(t.GetStart()) for t in rft] + [xy(t.GetEnd()) for t in rft]
    xs, ys = [q[0] for q in pts], [q[1] for q in pts]
    b = [min(xs) - 0.4, min(ys) - 0.4, max(xs) + 0.4, max(ys) + 0.4]
    pin1 = xy(next(p for p in fps["U1"].Pads() if p.GetNumber() == "1").GetPosition())
    under = [(x, y, w) for t in rft for x, y, w in seg_points(t)    # (the RF tracks, sampled: the bottom under them,
             if math.hypot(x - pin1[0], y - pin1[1]) > 2.0]           #  away from the chip's pins 1 .. 3)
    for t in tracks:
        if t.GetLayer() == pcbnew.B_Cu and t.GetNetname() not in RF_NETS + ("GND",):
            for x, y, w in under:
                if seg_dist(t, x, y) - MM(t.GetWidth()) / 2 - w < 0.4:
                    problems.append(f"{t.GetNetname()} on the bottom within 0.4 mm of the RF path near ({x:.2f}, {y:.2f})")
                    break
    samples = [(xy(t.GetStart())[0] + (xy(t.GetEnd())[0] - xy(t.GetStart())[0]) * k / 10,
                xy(t.GetStart())[1] + (xy(t.GetEnd())[1] - xy(t.GetStart())[1]) * k / 10, MM(t.GetWidth()) / 2)
               for t in rft if t.GetLayer() == pcbnew.F_Cu for k in range(11)]
    samples = [s for s in samples if math.hypot(s[0] - pin1[0], s[1] - pin1[1]) > 2.0]   # (at the chip: its pins 1 .. 3)
    for t in tracks:
        if t.GetLayer() != pcbnew.F_Cu or t.GetNetname() in RF_NETS + ("GND",):
            continue
        for x, y, w in samples:
            if seg_dist(t, x, y) - MM(t.GetWidth()) / 2 - w < 0.5:
                problems.append(f"{t.GetNetname()} within 0.5 mm of the RF line on top near ({x:.2f}, {y:.2f})")
                break
    return b


CV_NETS = ("CLK_", "RST_", "PUMP_")
LIN_NETS = ("LIN_OUT", "LINE_IN")
CV_MIN = 0.5                                       # mm: CLK / RST copper to LINE IN's ADC path (the coordinator's ask)


def cv_vs_linein(board, problems):
    """the CLOCK / RESET nets against LINE IN's ADC path (LIN_OUT, LINE_IN): the nearest approach of their copper on the
    same layer (sampled every 1/20 of a track), and how long a CLK / RST track runs within 1 mm of it; prints both"""
    tr = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK"]
    cv = [t for t in tr if t.GetNetname().startswith(CV_NETS)]
    lin = [t for t in tr if t.GetNetname() in LIN_NETS]
    best, near = None, 0.0
    for a in cv:
        close = False
        for b in lin:
            if b.GetLayer() != a.GetLayer():
                continue
            d = min(seg_dist(b, q[0], q[1]) for q in seg_points(a, 20)) - pcbnew.ToMM(a.GetWidth() + b.GetWidth()) / 2
            if best is None or d < best[0]:
                best = (d, a.GetNetname(), b.GetNetname())
            close = close or d < 1.0
        if close:
            near += pcbnew.ToMM(a.GetLength())
    if best is None:
        print("CLK / RST vs LINE IN: no tracks to compare")
        return
    print(f"CLK / RST vs LINE IN's ADC path: nearest copper {best[0]:.2f} mm ({best[1]} to {best[2]}, same layer); "
          f"CLK / RST track within 1 mm of it: {near:.1f} mm")
    if best[0] < CV_MIN:
        problems.append(f"{best[1]} within {best[0]:.2f} mm of {best[2]} (limit {CV_MIN})")


AGGRESSORS = {"CLK / RST": ("CLK_", "RST_"), "the pump": ("PUMP_", "CV_RAIL"), "RF / u.FL": ("LNA_IN", "RF", "UFL")}
VICTIMS = ("LIN_", "LINE_IN", "AVDD")


def _items(board, prefixes):
    """(tracks, pads) whose net starts with one of prefixes (exact for the bare names)"""
    ok = lambda n: any(n == p or (p.endswith("_") and n.startswith(p)) for p in prefixes)
    tr = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and ok(t.GetNetname())]
    pads = [p for f in board.GetFootprints() for p in f.Pads() if ok(p.GetNetname())]
    return tr, pads


def _pad_d(p, x, y):
    b = p.GetBoundingBox()
    x0, y0 = pcbnew.ToMM(b.GetLeft()) - OX, pcbnew.ToMM(b.GetTop()) - OY
    x1, y1 = pcbnew.ToMM(b.GetRight()) - OX, pcbnew.ToMM(b.GetBottom()) - OY
    return math.hypot(max(x0 - x, 0, x - x1), max(y0 - y, 0, y - y1))


def separation(board):
    """the timing study's self-lock check: each aggressor group's copper (tracks and pads, any layer) against LINE IN's
    front end (LIN_*, LINE_IN, AVDD): the nearest approach, edge to edge (tracks sampled every 1/20)"""
    vt, vp = _items(board, VICTIMS)
    for name, pre in AGGRESSORS.items():
        at, ap = _items(board, pre)
        pts = [(q[0], q[1], q[2]) for t in at for q in seg_points(t, 20)]
        for p in ap:
            b = p.GetBoundingBox()
            pts.append(((pcbnew.ToMM(b.GetLeft() + b.GetRight()) / 2) - OX, (pcbnew.ToMM(b.GetTop() + b.GetBottom()) / 2) - OY,
                        min(pcbnew.ToMM(b.GetWidth()), pcbnew.ToMM(b.GetHeight())) / 2))
        best = None
        for x, y, r in pts:
            for t in vt:
                d = seg_dist(t, x, y) - pcbnew.ToMM(t.GetWidth()) / 2 - r
                if best is None or d < best[0]:
                    best = (d, t.GetNetname())
            for p in vp:
                d = _pad_d(p, x, y) - r
                if best is None or d < best[0]:
                    best = (d, p.GetNetname())
        if best:
            print(f"{name} vs LINE IN's front end (LIN_*, LINE_IN, AVDD): nearest copper {max(best[0], 0):.2f} mm (to {best[1]}, any layer)")


def main():
    board = pcbnew.LoadBoard(sys.argv[1])
    fps = {f.GetReference(): f for f in board.GetFootprints()}
    problems = []
    boxes = courtyards(fps, problems)
    pivot(board, fps, boxes, problems)
    twin_copper(fps, problems)
    nv = vias(board, fps, problems)
    twins(board, fps, problems)
    twin_tracks_cross(board, fps, problems)
    ni = islands(board, problems)
    rf(board, fps, problems)
    cv_vs_linein(board, problems)
    separation(board)
    for p in problems:
        print(p)
    print(f"check_board: {len(fps)} parts, {nv} vias, {ni} GND pour pieces, {len(P.TWINS)} twins: {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
