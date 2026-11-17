# SPDX-License-Identifier: GPL-3.0-only
"""pin access (2026-11-15: round 1 left 13 links open, 10 of them at pads with no way out): every top signal pad
keeps a free lane out of it, a 1.2 mm run from its edge as wide as the pad (0.6 at most) that no other net's copper
(+0.15, through-hole +0.2), the board's edge (0.5) or the module's EPAD keep-out crosses; the connectors' signal pins
a 2.5 x 0.8 mm lane that no other part's courtyard crosses either. Needed: 1 lane, 2 for the converter loops' pads."""
import model as M

LANE, LANE_W = 1.2, 0.6
CONN_LANE, CONN_W = 2.5, 0.8
NEAR = 4.0


def padgeo(u, x, y, a):
    """the unit's pads at a pose: (name, net, cx, cy, w, h, attr)"""
    out = []
    for nm, (w, h, attr, net, dx, dy) in u.allpads.items():
        if u.side == "B" or u.fixed:
            px, py = dx, dy
            ww, hh = w, h
        else:
            px, py = M.rot(dx, dy, a)
            ww, hh = (h, w) if round(a) % 180 == 90 else (w, h)
        out.append((nm, net, x + px, y + py, ww, hh, attr))
    return out


def need(u, p):
    """the lanes a pad needs: 0, 1, 2 or "conn" """
    nm, net, _x, _y, _w, _h, attr = p
    ref = nm.split(".")[0]
    if not net or net == "GND" or u.ref in M.NO_ACCESS or ref.startswith(("RT", "CT")):
        return 0
    if u.ref in M.CONN_LANES:
        return "conn" if attr == "pth" else 0
    if attr != "smd" or u.side == "B":
        return 0
    return 2 if nm in M.LOOP_PADS else 1


def lanes(p, conn):
    _nm, _net, cx, cy, w, h, _a = p
    L, wd = (CONN_LANE, CONN_W) if conn else (LANE, None)
    wx = min(LANE_W, h) if wd is None else wd
    wy = min(LANE_W, w) if wd is None else wd
    return [(cx + w / 2, cy - wx / 2, cx + w / 2 + L, cy + wx / 2), (cx - w / 2 - L, cy - wx / 2, cx - w / 2, cy + wx / 2),
            (cx - wy / 2, cy + h / 2, cx + wy / 2, cy + h / 2 + L), (cx - wy / 2, cy - h / 2 - L, cx + wy / 2, cy - h / 2)]


def free_count(p, blockers, boxes, ep, conn):
    """blockers: pads (name, net, cx, cy, w, h, attr); boxes: other parts' courtyards (counted for conn only)"""
    net = p[1]
    n = 0
    for ln in lanes(p, conn):
        if ln[0] < 0.5 or ln[1] < 0.5 or ln[2] > M.W - 0.5 or ln[3] > M.H - 0.5 or M.overlap(ln, ep):
            continue
        hit = False
        for q in blockers:
            if q[1] == net and net:
                continue
            c = 0.2 if q[6] != "smd" else 0.15
            if M.overlap(ln, (q[2] - q[4] / 2 - c, q[3] - q[5] / 2 - c, q[2] + q[4] / 2 + c, q[3] + q[5] / 2 + c)):
                hit = True
                break
        if not hit and conn:
            hit = any(M.overlap(ln, b) for b in boxes)
        if not hit:
            n += 1
    return n


def ok(need_, n):
    return n >= (1 if need_ == "conn" else need_)
