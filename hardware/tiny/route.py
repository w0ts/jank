# SPDX-License-Identifier: GPL-3.0-only
"""A small two-layer grid router for make_board.py (no autorouter is installed here; hardware/pcb routes by hand).

The board is a grid (GRID mm) on two layers. Each net is routed in turn (A*, 8 directions, vias between the layers),
from one of its pads to the copper already joined, until all its pads are joined. Clearances: a cell is free for net N
when no copper of another net is nearer than clearance + its own half width; the masks are kept per width class
(signal, power, via) and grow as nets are routed. The result is checked by KiCad's DRC (make_fab.sh), not trusted.

Coordinates: board millimetres, origin top left, y down. Layers: 0 = top (F.Cu), 1 = bottom (B.Cu).
"""
import heapq
import math
from array import array

GRID = 0.05
CLEAR = 0.17            # (the rules say 0.15; 0.02 for the grid's corners)
EDGE = 0.3              # copper to the board edge
VIA_D, VIA_DRILL = 0.6, 0.3
VIA_EXTRA = 0.12        # (a via's drill keeps 0.5 mm from the next drill: vias at least 0.8 apart)
CLASSES = {"sig": 0.2, "pwr": 0.5, "via": VIA_D}   # (pwr: the widest power track, 0.5)
DIRS = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
        (1, 1, 1.4142), (1, -1, 1.4142), (-1, 1, 1.4142), (-1, -1, 1.4142)]
BOTTOM_COST = 1.6       # (keep the bottom a ground plane where possible)
VIA_COST = 1.2 / GRID
TURN_COST = 0.3


def _dist_rect(x, y, r):
    x0, y0, x1, y1 = r
    dx = max(x0 - x, 0.0, x - x1)
    dy = max(y0 - y, 0.0, y - y1)
    return math.hypot(dx, dy)


def _dist_seg(x, y, s):
    ax, ay, bx, by = s
    dx, dy = bx - ax, by - ay
    L = dx * dx + dy * dy
    t = 0.0 if L == 0 else max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / L))
    return math.hypot(x - ax - t * dx, y - ay - t * dy)


def _via_behind(prev, key, layer, steps=18):
    """a via within the last steps cells of this path (two vias nearer than 0.8 mm break the drill spacing)"""
    for _ in range(steps):
        key = prev.get(key)
        if key is None:
            return False
        if key[0][0] != layer:
            return True
    return False


class Router:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.nx, self.ny = int(math.ceil(w / GRID)) + 1, int(math.ceil(h / GRID)) + 1
        n = self.nx * self.ny
        self.mask = {c: [array("i", bytes(4 * n)), array("i", bytes(4 * n))] for c in CLASSES}
        self.netid = {}
        for c, wc in CLASSES.items():
            m = EDGE + wc / 2
            for layer in (0, 1):
                a = self.mask[c][layer]
                for iy in range(self.ny):
                    y = iy * GRID
                    for ix in range(self.nx):
                        x = ix * GRID
                        if x < m or y < m or x > w - m or y > h - m:
                            a[iy * self.nx + ix] = -1

    def nid(self, net):
        if net is None:
            return -1
        return self.netid.setdefault(net, len(self.netid) + 1)

    def _mark(self, layers, net, bbox, dist, extra):
        """mark every cell within clearance + the class's half width (+ extra) of a shape (dist(x, y) to it)"""
        n = self.nid(net)
        for c, wc in CLASSES.items():
            r = CLEAR + wc / 2 + extra
            x0, y0, x1, y1 = bbox
            ix0, ix1 = max(0, int((x0 - r) / GRID)), min(self.nx - 1, int((x1 + r) / GRID) + 1)
            iy0, iy1 = max(0, int((y0 - r) / GRID)), min(self.ny - 1, int((y1 + r) / GRID) + 1)
            for layer in layers:
                a = self.mask[c][layer]
                for iy in range(iy0, iy1 + 1):
                    y = iy * GRID
                    row = iy * self.nx
                    for ix in range(ix0, ix1 + 1):
                        if dist(ix * GRID, y) < r:
                            v = a[row + ix]
                            if v == 0:
                                a[row + ix] = n
                            elif v != n:
                                a[row + ix] = -1

    def rect(self, layers, net, r, clear_extra=0.0):
        self._mark(layers, net, r, lambda x, y: _dist_rect(x, y, r), clear_extra)

    def seg(self, layer, net, s, width):
        bb = (min(s[0], s[2]) - width / 2, min(s[1], s[3]) - width / 2, max(s[0], s[2]) + width / 2, max(s[1], s[3]) + width / 2)
        self._mark((layer,), net, bb, lambda x, y: max(0.0, _dist_seg(x, y, s) - width / 2), 0.0)

    def circle(self, layers, net, cx, cy, rad):
        self._mark(layers, net, (cx - rad, cy - rad, cx + rad, cy + rad), lambda x, y: max(0.0, math.hypot(x - cx, y - cy) - rad), 0.0)

    def drill(self, x, y, d):
        """no via within 0.5 mm (drill edge to drill edge) of a hole of diameter d"""
        r = d / 2 + 0.5 + VIA_DRILL / 2 + 0.03
        for layer in (0, 1):
            a = self.mask["via"][layer]
            for iy in range(max(0, int((y - r) / GRID)), min(self.ny, int((y + r) / GRID) + 2)):
                for ix in range(max(0, int((x - r) / GRID)), min(self.nx, int((x + r) / GRID) + 2)):
                    if math.hypot(ix * GRID - x, iy * GRID - y) < r:
                        a[iy * self.nx + ix] = -1

    def block(self, layers, r):
        """a keep-out rectangle: nothing on these layers (no copper, no via)"""
        x0, y0, x1, y1 = r
        for c in CLASSES:
            for layer in layers:
                a = self.mask[c][layer]
                for iy in range(max(0, int(y0 / GRID)), min(self.ny, int(y1 / GRID) + 1)):
                    for ix in range(max(0, int(x0 / GRID)), min(self.nx, int(x1 / GRID) + 1)):
                        a[iy * self.nx + ix] = -1

    # ---- the search
    def _cells_in(self, rect, layers):
        x0, y0, x1, y1 = rect
        out = []
        for layer in layers:
            for iy in range(int(math.ceil(y0 / GRID)), int(y1 / GRID) + 1):
                for ix in range(int(math.ceil(x0 / GRID)), int(x1 / GRID) + 1):
                    out.append((layer, ix, iy))
        return out

    def path(self, net, cls, starts, goals, window):
        """A* from the start cells to any goal cell, inside window (ix0, iy0, ix1, iy1); returns [(layer, ix, iy)]"""
        n = self.nid(net)
        nx = self.nx
        m, mv = self.mask[cls], self.mask["via"]
        goal = set(goals)
        gx = [g[1] for g in goals]
        gy = [g[2] for g in goals]
        cx, cy = sum(gx) / len(gx), sum(gy) / len(gy)
        spread = max(max(gx) - min(gx), max(gy) - min(gy)) / 2

        def hfun(ix, iy):
            return max(0.0, math.hypot(ix - cx, iy - cy) - spread)

        wx0, wy0, wx1, wy1 = window
        best = {}
        prev = {}
        heap = []
        for s in starts:
            best[(s, -1)] = 0.0
            heapq.heappush(heap, (hfun(s[1], s[2]), 0.0, s, -1))
        while heap:
            f, g, s, d = heapq.heappop(heap)
            if best.get((s, d), 1e18) < g:
                continue
            if s in goal:
                out = [s]
                key = (s, d)
                while key in prev:
                    key = prev[key]
                    if out[-1] != key[0]:
                        out.append(key[0])
                return out[::-1]
            layer, ix, iy = s
            lc = BOTTOM_COST if layer else 1.0
            a = m[layer]
            for k, (dx, dy, c) in enumerate(DIRS):
                jx, jy = ix + dx, iy + dy
                if jx < wx0 or jy < wy0 or jx > wx1 or jy > wy1:
                    continue
                v = a[jy * nx + jx]
                if v != 0 and v != n:
                    continue
                if dx and dy:                      # (no corner cutting past a blocked neighbour)
                    v1, v2 = a[iy * nx + jx], a[jy * nx + ix]
                    if (v1 != 0 and v1 != n) or (v2 != 0 and v2 != n):
                        continue
                ng = g + c * lc + (TURN_COST if d not in (-1, k) else 0.0)
                t = (layer, jx, jy)
                if ng < best.get((t, k), 1e18):
                    best[(t, k)] = ng
                    prev[(t, k)] = (s, d)
                    heapq.heappush(heap, (ng + hfun(jx, jy), ng, t, k))
            i = iy * nx + ix
            v0, v1 = mv[0][i], mv[1][i]
            if (v0 == 0 or v0 == n) and (v1 == 0 or v1 == n) and not _via_behind(prev, (s, d), layer):
                t = (1 - layer, ix, iy)
                ng = g + VIA_COST
                if ng < best.get((t, -1), 1e18):
                    best[(t, -1)] = ng
                    prev[(t, -1)] = (s, d)
                    heapq.heappush(heap, (ng + hfun(ix, iy), ng, t, -1))
        return None

    def to_items(self, cells):
        """cells -> ([(layer, x0, y0, x1, y1)], [(x, y)] vias), straight runs merged"""
        segs, vias = [], []
        i = 0
        while i < len(cells) - 1:
            l0, x0, y0 = cells[i]
            if cells[i + 1][0] != l0:
                vias.append((x0 * GRID, y0 * GRID))
                i += 1
                continue
            dx, dy = cells[i + 1][1] - x0, cells[i + 1][2] - y0
            j = i + 1
            while (j + 1 < len(cells) and cells[j + 1][0] == l0 and cells[j + 1][1] - cells[j][1] == dx
                   and cells[j + 1][2] - cells[j][2] == dy):
                j += 1
            segs.append((l0, x0 * GRID, y0 * GRID, cells[j][1] * GRID, cells[j][2] * GRID))
            i = j
        return segs, vias


def route_net(R, net, cls, width, pads, margin=6.0):
    """pads: [(rect, layers)] of the net; joins them all. Returns (segments, vias, [what failed])"""
    segs_all, vias_all, failed = [], [], []
    tree = set(R._cells_in(*pads[0]))
    left = list(pads[1:])
    xs = [p[0][0] for p in pads] + [p[0][2] for p in pads]
    ys = [p[0][1] for p in pads] + [p[0][3] for p in pads]
    while left:
        left.sort(key=lambda p: min(math.hypot((p[0][0] + p[0][2]) / 2 / GRID - t[1], (p[0][1] + p[0][3]) / 2 / GRID - t[2])
                                    for t in list(tree)[::7] or tree))
        pad = left.pop(0)
        starts = R._cells_in(*pad)
        res = None
        for mg in (margin, margin * 3, 100.0):
            win = (max(0, int((min(xs) - mg) / GRID)), max(0, int((min(ys) - mg) / GRID)),
                   min(R.nx - 1, int((max(xs) + mg) / GRID)), min(R.ny - 1, int((max(ys) + mg) / GRID)))
            res = R.path(net, cls, starts, list(tree), win)
            if res:
                break
        if not res:
            failed.append(f"{net}: no path to the pad at ({pad[0][0]:.2f}, {pad[0][1]:.2f})")
            continue
        segs, vias = R.to_items(res)
        for layer, x0, y0, x1, y1 in segs:
            R.seg(layer, net, (x0, y0, x1, y1), width)
        for x, y in vias:
            R.circle((0, 1), net, x, y, VIA_D / 2 + VIA_EXTRA)
            R.drill(x, y, VIA_DRILL)
        segs_all += segs
        vias_all += vias
        tree |= set(res) | set(starts)
        for layer, x0, y0, x1, y1 in segs:            # (the whole new run joins the tree, not only its cells)
            n = int(max(abs(x1 - x0), abs(y1 - y0)) / GRID)
            for k in range(n + 1):
                tree.add((layer, int(round((x0 + (x1 - x0) * k / max(n, 1)) / GRID)), int(round((y0 + (y1 - y0) * k / max(n, 1)) / GRID))))
    return segs_all, vias_all, failed


def via_near(R, net, rect, layer, reach=2.5):
    """a stub from a pad to the nearest place a via of the net fits: (segments, via) or None"""
    n = R.nid(net)
    mv = R.mask["via"]
    nx = R.nx
    goals = []
    x0, y0, x1, y1 = rect
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    for iy in range(int((cy - reach) / GRID), int((cy + reach) / GRID) + 1):
        for ix in range(int((cx - reach) / GRID), int((cx + reach) / GRID) + 1):
            if not (0 <= ix < nx and 0 <= iy < R.ny):
                continue
            i = iy * nx + ix
            if mv[0][i] in (0, n) and mv[1][i] in (0, n):
                goals.append((layer, ix, iy))
    if not goals:
        return None
    win = (max(0, int((cx - reach - 1) / GRID)), max(0, int((cy - reach - 1) / GRID)),
           min(nx - 1, int((cx + reach + 1) / GRID)), min(R.ny - 1, int((cy + reach + 1) / GRID)))
    res = R.path(net, "sig", R._cells_in(rect, (layer,)), goals, win)
    if not res:
        return None
    segs, _v = R.to_items([c for c in res if c[0] == layer])
    end = res[-1]
    return segs, (end[1] * GRID, end[2] * GRID)
