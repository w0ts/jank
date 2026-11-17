# SPDX-License-Identifier: GPL-3.0-only
"""two routability terms for the placer (2026-11-16, after the buf-try route left 26 open pairs and joined BTN_UP and
+3V3 through LINE IN's keep-out):

KEEP: for every airwire (MST edge) of a net that is not a LINE IN net (LINE_IN, LINE_SENSE, LIN_*), its length inside
  LINE IN's keep-out region. The region: make_board.inner_planes()'s rule areas ("no power under LINE IN"), i.e. the
  LINE IN nets' pads (not U1's) grouped 5 mm apart, each group's bounding box + 1.5 mm, recomputed as the parts move.
  An airwire's path: the mean of its two L-shaped paths (axis-aligned, as the router mostly runs).
CONG: a 2 mm grid; each airwire adds demand along its two L paths (0.5 each, power nets 2x: wider tracks), counted
  separately for horizontal and vertical runs. Capacity per cell and direction: 2 layers x 5 tracks (0.2 track + 0.2
  clearance pitch in 2 mm), each layer's share reduced by the pads in the cell (area + 0.2 mm clearance round them,
  top SMD pads the top layer, bottom parts / twins the bottom, through-hole both; floor 1 track per layer). Cost: sum of max(0, demand - cap)^2.
"""
import collections, math
import model as M

CELL = 2.0
NX = NY = int(M.W / CELL)
PITCH = 0.4                       # track 0.2 + clearance 0.2
UTIL = 0.5                        # (half the room: the GND pour on both layers, vias, corners; a judgement, unmeasured)
TRK = UTIL * CELL / PITCH         # 2.5 tracks per layer through a cell
POWER = {"+3V3", "VSYS", "VREG", "V5A", "V5M", "V5C", "VBUS", "BAT_P", "VUSB", "V5H", "VLED", "V3_L1", "V3_L2", "V5_SW"}
LIN_MARGIN, LIN_JOIN = 1.5, 5.0
MIN_FREE = 0.2                    # (a layer's floor: a cell full of pads still lets their own tracks in, 1 per layer)


def is_lin(n):
    return n.startswith("LIN_") or n in ("LINE_IN", "LINE_SENSE")


def cell(x, y):
    return min(NX - 1, max(0, int(x / CELL))), min(NY - 1, max(0, int(y / CELL)))


def lpaths(a, b):
    """the two L paths' runs: [(kind 'h'/'v', fixed coord, lo, hi)] for each"""
    (x0, y0), (x1, y1) = a, b
    l1 = [("h", y0, min(x0, x1), max(x0, x1)), ("v", x1, min(y0, y1), max(y0, y1))]
    l2 = [("v", x0, min(y0, y1), max(y0, y1)), ("h", y1, min(x0, x1), max(x0, x1))]
    return l1, l2


def edge_demand(a, b, w):
    """{(dir, ix, iy): demand} of one airwire"""
    out = collections.defaultdict(float)
    for path in lpaths(a, b):
        for k, c, lo, hi in path:
            if k == "h":
                iy = cell(0, c)[1]
                for ix in range(cell(lo, 0)[0], cell(hi, 0)[0] + 1):
                    out[(0, ix, iy)] += 0.5 * w
            else:
                ix = cell(c, 0)[0]
                for iy in range(cell(0, lo)[1], cell(0, hi)[1] + 1):
                    out[(1, ix, iy)] += 0.5 * w
    return out


def pad_block(p, unit_side, ref):
    """{(layer, ix, iy): blocked fraction of that layer's tracks} for one pad (its area + clearance, at its centre cell)"""
    nm, _net, x, y, w, h, attr = p
    frac = min(1.0, (w + 0.4) * (h + 0.4) / (CELL * CELL))
    ix, iy = cell(x, y)
    if attr != "smd":
        return {(0, ix, iy): frac, (1, ix, iy): frac}
    lay = 1 if (unit_side == "B" or nm.split(".")[0].startswith(("RT", "CT"))) else 0
    return {(lay, ix, iy): frac}


def keep_len(a, b, boxes):
    """mean inside-length of the two L paths in the boxes"""
    tot = 0.0
    for path in lpaths(a, b):
        for k, c, lo, hi in path:
            for bx in boxes:
                if k == "h":
                    if bx[1] <= c <= bx[3]:
                        tot += max(0.0, min(hi, bx[2]) - max(lo, bx[0]))
                else:
                    if bx[0] <= c <= bx[2]:
                        tot += max(0.0, min(hi, bx[3]) - max(lo, bx[1]))
    return tot / 2


class Rout:
    """the two terms' state over an anneal.State: full build, and a delta for a move"""

    def __init__(self, st):
        self.st = st
        self.build()

    # ---- LINE IN region
    def lin_pts(self, pg):
        return [(p[2], p[3]) for r, ps in pg.items() if r != "U1" for p in ps if p[1] and is_lin(p[1]) and p[1] != "LIN_DET"]

    @staticmethod
    def regions(pts):
        groups = []
        for q in pts:
            near = [g for g in groups if any(math.dist(q, r) <= LIN_JOIN for r in g)]
            groups = [g for g in groups if not any(g is n for n in near)] + [[q] + [r for g in near for r in g]]
        out = []
        for g in groups:
            xs, ys = [x for x, _ in g], [y for _, y in g]
            out.append((min(xs) - LIN_MARGIN, min(ys) - LIN_MARGIN, max(xs) + LIN_MARGIN, max(ys) + LIN_MARGIN))
        return out

    def lin_units(self):
        return {r for r, ps in self.st.pg.items() if r != "U1" and any(p[1] and is_lin(p[1]) for p in ps)}

    def build(self):
        st = self.st
        self.linu = self.lin_units()
        self.boxes = self.regions(self.lin_pts(st.pg))
        self.K = sum(keep_len(a, b, self.boxes) for n, es in st.edges.items() if not is_lin(n) for a, b, _ in es)
        self.dem = collections.defaultdict(float)
        for n, es in st.edges.items():
            w = 2.0 if n in POWER else 1.0
            for a, b, _ in es:
                for k, v in edge_demand(a, b, w).items():
                    self.dem[k] += v
        self.blk = collections.defaultdict(float)
        for r, ps in st.pg.items():
            u = st.u[r]
            for p in ps:
                for k, v in pad_block(p, u.side, r).items():
                    self.blk[k] += v
        self.C = sum(self.cell_cost(d, ix, iy) for d in (0, 1) for ix in range(NX) for iy in range(NY))

    def cap(self, ix, iy, blk=None):
        b = self.blk if blk is None else blk
        return TRK * (max(MIN_FREE, 1 - b.get((0, ix, iy), 0.0)) + max(MIN_FREE, 1 - b.get((1, ix, iy), 0.0)))

    def cell_cost(self, d, ix, iy, dem=None, blk=None):
        dm = (self.dem if dem is None else dem).get((d, ix, iy), 0.0)
        return max(0.0, dm - self.cap(ix, iy, blk)) ** 2

    # ---- a move's delta: (dK, dC, apply)
    def delta(self, moves, nets, new_e, new_pg):
        st = self.st
        # KEEP
        if self.linu & set(moves):
            pg = dict(st.pg)
            pg.update(new_pg)
            boxes = self.regions(self.lin_pts(pg))
            K = 0.0
            for n, es in st.edges.items():
                if is_lin(n):
                    continue
                for a, b, _ in (new_e[n] if n in new_e else es):
                    K += keep_len(a, b, boxes)
            dK = K - self.K
        else:
            boxes = self.boxes
            dK = 0.0
            for n in nets:
                if is_lin(n):
                    continue
                dK += sum(keep_len(a, b, boxes) for a, b, _ in new_e[n]) - sum(keep_len(a, b, boxes) for a, b, _ in st.edges[n])
        # CONG
        ddem = collections.defaultdict(float)
        for n in nets:
            w = 2.0 if n in POWER else 1.0
            for a, b, _ in st.edges[n]:
                for k, v in edge_demand(a, b, w).items():
                    ddem[k] -= v
            for a, b, _ in new_e[n]:
                for k, v in edge_demand(a, b, w).items():
                    ddem[k] += v
        dblk = collections.defaultdict(float)
        for r in moves:
            u = st.u[r]
            for p in st.pg[r]:
                for k, v in pad_block(p, u.side, r).items():
                    dblk[k] -= v
            for p in new_pg[r]:
                for k, v in pad_block(p, u.side, r).items():
                    dblk[k] += v
        cells = {(k[1], k[2]) for k in ddem} | {(k[1], k[2]) for k in dblk}
        dem2 = _Overlay(self.dem, ddem)
        blk2 = _Overlay(self.blk, dblk)
        dC = 0.0
        for ix, iy in cells:
            for d in (0, 1):
                dC += self.cell_cost(d, ix, iy, dem2, blk2) - self.cell_cost(d, ix, iy)

        def apply():
            self.K += dK
            self.boxes = boxes
            for k, v in ddem.items():
                self.dem[k] += v
            for k, v in dblk.items():
                self.blk[k] += v
            self.C += dC
        return dK, dC, apply

    # ---- the report: hottest cells and the nets through them
    def hot(self, top=10):
        st = self.st
        nets_at = collections.defaultdict(set)
        for n, es in st.edges.items():
            for a, b, _ in es:
                for k in edge_demand(a, b, 1):
                    nets_at[(k[1], k[2])].add(n)
        rows = []
        for ix in range(NX):
            for iy in range(NY):
                for d in (0, 1):
                    over = self.dem.get((d, ix, iy), 0.0) - self.cap(ix, iy)
                    if over > 0:
                        rows.append((over, d, ix, iy))
        rows.sort(reverse=True)
        return [(round(o, 1), "HV"[d], (ix * CELL, iy * CELL), round(self.dem[(d, ix, iy)], 1), round(self.cap(ix, iy), 1),
                 sorted(nets_at[(ix, iy)])) for o, d, ix, iy in rows[:top]]

    def keep_edges(self, top=15):
        st = self.st
        out = []
        for n, es in st.edges.items():
            if is_lin(n):
                continue
            for a, b, _ in es:
                k = keep_len(a, b, self.boxes)
                if k > 0:
                    out.append((round(k, 1), n, tuple(round(v, 1) for v in a), tuple(round(v, 1) for v in b)))
        return sorted(out, reverse=True)[:top]


class _Overlay:
    def __init__(self, base, d):
        self.base, self.d = base, d

    def get(self, k, default=0.0):
        return self.base.get(k, default) + self.d.get(k, 0.0)
