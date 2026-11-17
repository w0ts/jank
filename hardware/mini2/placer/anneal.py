# SPDX-License-Identifier: GPL-3.0-only
"""the fresh floorplan's placer: a state over model.py's units with an incremental cost, and simulated annealing.
python3 anneal.py START.json OUT.json [moves] [seed]   (START: {ref: [x, y, a]} for the movable units)"""
import collections, json, math, random, sys, time
import os
import model as M
import rules as R
import rcost
K_W = float(os.environ.get('K_W', '0'))   # (2026-11-16: per mm of a non-LINE-IN airwire inside LINE IN's keep-out)
C_W = float(os.environ.get('C_W', '0'))   # (per unit of squared over-demand in a 2 mm cell)


class State:
    def __init__(self, units, fixed_boxes, circles, pin_swap=None):
        self.u = units
        self.fixed_boxes, self.circles = fixed_boxes, circles
        self.mov = [r for r, u in units.items() if not u.fixed]
        self.pin_swap = pin_swap            # (ref, {pad name: allowed class}) or None
        self.pull_idx = collections.defaultdict(list)
        for i, (a, b, w) in enumerate(M.PULLS):
            for q in (a, b):
                r0 = q.split(".")[0]
                self.pull_idx[M.OWNER.get(r0, r0)].append(i)
        self.rebuild()

    # ---- the full evaluation
    def rebuild(self):
        self.gp = {r: u.gpads() for r, u in self.u.items()}
        self.gb = {r: u.gboxes() for r, u in self.u.items()}
        self.pg = {r: R.padgeo(u, u.x, u.y, u.a) for r, u in self.u.items()}
        self.net_units = collections.defaultdict(set)
        for r, ps in self.gp.items():
            for _nm, n, _x, _y in ps:
                if n != "GND":
                    self.net_units[n].add(r)
        self.edges = {n: self.net_mst(n) for n in self.net_units}
        self.X = self.count_all()
        self.L = sum(M.NET_W.get(n, 1) * sum(math.dist(a, b) for a, b, _ in e) for n, e in self.edges.items())
        self.P = sum(self.pull(i) for i in range(len(M.PULLS)))
        self.B = sum(self.cohesion(b) for b in M.BLOCKS)
        self.RC = rcost.Rout(self)
        self.K, self.C = self.RC.K, self.RC.C

    def pads_of_net(self, n, over=None):
        pts = []
        for r in sorted(self.net_units[n]):
            ps = over[r] if over and r in over else self.gp[r]
            pts += [(x, y) for _nm, nn, x, y in ps if nn == n]
        return pts

    def net_mst(self, n, over=None):
        e = M.mst(self.pads_of_net(n, over))
        return [(a, b, (min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1]))) for a, b in e]

    def count_all(self):
        allx = [(n, e) for n, es in self.edges.items() for e in es]
        c = 0
        for i in range(len(allx)):
            n1, (a, b, bb1) = allx[i]
            for j in range(i + 1, len(allx)):
                n2, (c2, d2, bb2) = allx[j]
                if n1 != n2 and bb1[0] <= bb2[2] and bb2[0] <= bb1[2] and bb1[1] <= bb2[3] and bb2[1] <= bb1[3] \
                        and M.cross(a, b, c2, d2):
                    c += 1
        return c

    def pad_pos(self, name, over=None):
        r = name.split(".")[0]
        r = M.OWNER.get(r, r)
        ps = over[r] if over and r in over else self.gp[r]
        for nm, _n, x, y in ps:
            if nm == name:
                return (x, y)
        raise KeyError(name)

    def pull(self, i, over=None):
        a, b, w = M.PULLS[i]
        return w * math.dist(self.pad_pos(a, over), self.pad_pos(b, over))

    def centroid(self, blk, over=None):
        xs = [(over[r] if over and r in over else (self.u[r].x, self.u[r].y)) for r in M.BLOCKS[blk] if r in self.u]
        return (sum(p[0] for p in xs) / len(xs), sum(p[1] for p in xs) / len(xs)), xs

    def cohesion(self, blk, over=None):
        (cx, cy), xs = self.centroid(blk, over)
        return sum((x - cx) ** 2 + (y - cy) ** 2 for x, y in xs) / 10.0

    def cost(self):
        return M.X_W * self.X + M.L_W * self.L + M.L_W * self.P + M.B_W * self.B + K_W * self.K + C_W * self.C

    # ---- legality of a unit's boxes
    def ring_ok(self, r, pose, skip=()):
        """the converters' ground (2026-11-15: their pours came out cut into islands): no other part within GND_RING of
        a converter template's boxes, but the parts pulled to that converter's pins"""
        if not hasattr(self, "_ring_ok"):
            self.hard_ok(r, pose)
            self._ring_ok = {c: {self._own[a] for a, b, _m in M.HARD if b.startswith(c + ".")} |
                                {self._own[b] for a, b, _m in M.HARD if a.startswith(c + ".")} for c in M.TEMPLATES}
        boxes = self.u[r].gboxes(*pose)
        for c in M.TEMPLATES:
            if c not in self.u or c in skip:
                continue
            if r == c:
                for o, obs in self.gb.items():
                    if o != c and o not in self._ring_ok[c] and not self.u[o].fixed and o not in skip and \
                            any(M.box_dist(b, ob) < M.GND_RING for b in boxes for ob in obs):
                        return False
            elif r not in self._ring_ok[c] and any(M.box_dist(b, cb) < M.GND_RING for b in boxes for cb in self.gb[c]):
                return False
        return True

    def hard_ok(self, r, pose):
        """the hard pulls (model.HARD) touching r: pad to pad within their distance"""
        if not hasattr(self, "_hard"):
            own = {}
            for rr, u in self.u.items():
                for nm in u.allpads:
                    own[nm.rstrip("'")] = rr
            self._own = own
            self._hard = collections.defaultdict(list)
            for a, b, m in M.HARD:
                if a in own and b in own:
                    self._hard[own[a]].append((a, b, m))
                    self._hard[own[b]].append((a, b, m))
        if r not in self._hard:
            return True
        mine = {p[0].rstrip("'"): (p[2], p[3]) for p in R.padgeo(self.u[r], *pose)}

        def pos(nm):
            if nm in mine:
                return mine[nm]
            q = next(p for p in self.pg[self._own[nm]] if p[0].rstrip("'") == nm)
            return (q[2], q[3])
        parts_only = getattr(self, "hard_parts_only", False)   # (legalising: only the pulled part answers for its pull)
        def responsible(a, b, m):                     # (an IC pin's pull: the part; a chain's: its far end)
            return self._own[b] if m == M.CHAIN_MM else self._own[a]
        return all(math.dist(pos(a), pos(b)) <= m for a, b, m in self._hard[r]
                   if not parts_only or responsible(a, b, m) == r)

    def ep_zone(self, pose=None):
        u = self.u["U1"]
        x, y, a = pose or (u.x, u.y, u.a)
        q = M.rbox(u.ep_local, a)
        return (q[0] + x, q[1] + y, q[2] + x, q[3] + y)

    def access_ok(self, r, pose, skip=()):
        """the pin-access rules (rules.py) with r at pose: r's own pads, and its neighbours' (no worse than needed)"""
        u = self.u[r]
        new = R.padgeo(u, *pose)
        ep = self.ep_zone(pose if r == "U1" else None)
        nb = self.gb_at(r, pose)
        cx = sum((b[0] + b[2]) / 2 for b in nb) / len(nb) if nb else pose[0]
        cy = sum((b[1] + b[3]) / 2 for b in nb) / len(nb) if nb else pose[1]
        span = max([max(b[2] - b[0], b[3] - b[1]) for b in nb] + [2.0]) / 2 + R.NEAR + 2.5
        near = {o for o, ps in self.pg.items() if o != r and o not in skip
                and any(abs(p[2] - cx) < span and abs(p[3] - cy) < span for p in ps)}
        others = [p for o in near for p in self.pg[o]]
        boxes_o = [b for o in near if not self.u[o].fixed for b in self.gb[o]]
        for p in new:                           # (r's own pads)
            nd = R.need(u, p)
            if nd and not R.ok(nd, R.free_count(p, others + [q for q in new if q is not p], boxes_o, ep, nd == "conn")):
                return False
        old = self.pg[r]
        for o in near:                          # (the neighbours' pads near r)
            uo = self.u[o]
            for p in self.pg[o]:
                nd = R.need(uo, p)
                if not nd or abs(p[2] - cx) > span - 2.0 or abs(p[3] - cy) > span - 2.0:
                    continue
                rest = [q for oo in near if oo != o for q in self.pg[oo]] + [q for q in self.pg[o] if q is not p]
                rboxes = [b for oo in near if oo != o and not self.u[oo].fixed for b in self.gb[oo]]
                n_new = R.free_count(p, rest + new, rboxes + nb, ep, nd == "conn")
                if R.ok(nd, n_new):
                    continue
                n_old = R.free_count(p, rest + old, rboxes + self.gb[r], ep, nd == "conn")
                if n_new < n_old:
                    return False
        # (the fine-pitch ICs: a free 1 mm ring round their own courtyard)
        for o in M.FINE_PITCH:
            if o not in self.u or o in skip:
                continue
            if o == r:
                ring = nb[0]
                if any(M.box_dist(ring, b) < 1.0 for oo in near if not self.u[oo].fixed or True for b in self.gb[oo]):
                    return False
            else:
                ring = self.gb[o][0]
                if any(M.box_dist(ring, b) < 1.0 for b in nb):
                    return False
        return True

    def gb_at(self, r, pose):
        return self.u[r].gboxes(*pose)

    def corridors(self, x, y, a):
        out = []
        for b in M.MOD_CORR:
            q = M.rbox(b, a)
            out.append((q[0] + x, q[1] + y, q[2] + x, q[3] + y))
        return out

    def legal(self, r, pose, skip=()):
        u = self.u[r]
        if not self.hard_ok(r, pose):            # (first: the cheapest test, prunes most candidates)
            return False
        boxes = u.gboxes(*pose)
        reg = M.REGION.get(r)
        if r == "U1" and pose[0] < M.MOD_X_MIN:
            return False
        if r == "U1":
            for cb in self.corridors(*pose):
                for o, obs in self.gb.items():
                    if o != "U1" and o not in skip and o not in M.MOD_EXEMPT and not self.u[o].fixed and \
                            any(M.overlap(cb, ob) for ob in obs):
                        return False
        elif r not in M.MOD_EXEMPT and "U1" not in skip:
            cbs = self.corridors(self.u["U1"].x, self.u["U1"].y, self.u["U1"].a)
            if any(M.overlap(b, cb) for b in boxes for cb in cbs):
                return False
        for b in boxes:
            if b[0] < M.EDGE or b[1] < M.EDGE or b[2] > M.W - M.EDGE or b[3] > (M.H if r == "U1" else M.H - M.EDGE):
                return False
            if reg and not (reg[0] <= b[0] and reg[1] <= b[1] and b[2] <= reg[2] and b[3] <= reg[3]):
                return False
            if any(M.box_dist(b, f) < M.GAP for f in self.fixed_boxes):
                return False
            if any(M.box_circle(b, c) for c in self.circles):
                return False
            for o, obs in self.gb.items():
                if o == r or o in skip or self.u[o].fixed:
                    continue
                if any(M.box_dist(b, ob) < M.GAP for ob in obs):
                    return False
        if r in M.TALL and any(M.box_dist(b, M.SCREEN) <= 0 for b in boxes):
            return False
        if getattr(self, "access", True) and not self.access_ok(r, pose, skip):
            return False
        if not self.ring_ok(r, pose, skip):
            return False
        if r in M.LINE_IN or r in M.CONVERTERS:
            other = M.CONVERTERS if r in M.LINE_IN else M.LINE_IN
            for o in other:
                if o in self.gb and any(M.box_dist(b, ob) < M.LIN_GAP for b in boxes for ob in self.gb[o]):
                    return False
        return True

    # ---- a move: new (x, y, a) for one or two units; returns (delta, commit)
    def try_move(self, moves, pads=None):
        over_p = {r: self.u[r].gpads(*m, pads=(pads if r == "U1" else None)) for r, m in moves.items()}
        nets = {n for r in moves for _nm, n, _x, _y in over_p[r] if n != "GND"}
        new_e = {n: self.net_mst(n, over_p) for n in nets}
        others = [e for n, es in self.edges.items() if n not in nets for e in es]

        def xc(eset):
            c = 0
            lst = [(n, e) for n, es in eset.items() for e in es]
            for n1, (a, b, bb1) in lst:
                for (c2, d2, bb2) in others:
                    if bb1[0] <= bb2[2] and bb2[0] <= bb1[2] and bb1[1] <= bb2[3] and bb2[1] <= bb1[3] \
                            and M.cross(a, b, c2, d2):
                        c += 1
            for i in range(len(lst)):
                n1, (a, b, bb1) = lst[i]
                for j in range(i + 1, len(lst)):
                    n2, (c2, d2, bb2) = lst[j]
                    if n1 != n2 and bb1[0] <= bb2[2] and bb2[0] <= bb1[2] and bb1[1] <= bb2[3] and bb2[1] <= bb1[3] \
                            and M.cross(a, b, c2, d2):
                        c += 1
            return c
        dX = xc(new_e) - xc({n: self.edges[n] for n in nets})
        dL = sum(M.NET_W.get(n, 1) * (sum(math.dist(a, b) for a, b, _ in new_e[n]) -
                                      sum(math.dist(a, b) for a, b, _ in self.edges[n])) for n in nets)
        pis = {i for r in moves for i in self.pull_idx.get(r, ())}
        dP = sum(self.pull(i, over_p) - self.pull(i) for i in pis)
        blks = {self.u[r].block for r in moves if self.u[r].block}
        over_c = {r: (m[0], m[1]) for r, m in moves.items()}
        dB = sum(self.cohesion(b, over_c) - self.cohesion(b) for b in blks)
        new_pg = {r: R.padgeo(self.u[r], *m) for r, m in moves.items()}
        dK, dC, rc_apply = self.RC.delta(moves, nets, new_e, new_pg)
        delta = M.X_W * dX + M.L_W * dL + M.L_W * dP + M.B_W * dB + K_W * dK + C_W * dC

        def commit():
            if pads is not None:
                self.u["U1"].pads = pads
            for r, m in moves.items():
                u = self.u[r]
                u.x, u.y, u.a = m
                self.gp[r] = over_p[r]
                self.gb[r] = u.gboxes()
                self.pg[r] = R.padgeo(u, u.x, u.y, u.a)
            self.edges.update(new_e)
            self.X += dX
            self.L += dL
            self.P += dP
            self.B += dB
            rc_apply()
            self.K, self.C = self.RC.K, self.RC.C
        return delta, commit


def swap_move(st, rnd):
    """a module pin swap: one signal to another allowed pin (free, or swapped with that pin's signal)"""
    u = st.u["U1"]
    ents = [i for i, p in enumerate(u.pads) if p[1] in M.ADC_NETS | M.DIG_NETS]
    i = rnd.choice(ents)
    name, net, _dx, _dy = u.pads[i]
    allowed = M.ADC_PINS if net in M.ADC_NETS else M.DIG_PINS
    if net in M.OUT_NETS:
        allowed = allowed & M.OUT_PINS
    if net in M.NO_PULL_NETS:
        allowed = allowed - {32}
    t = rnd.choice(sorted(allowed - {u.pin_of[name]}))
    occ = {pin: nm for nm, pin in u.pin_of.items()}
    pads = list(u.pads)
    new_of = dict(u.pin_of)
    if t in occ:
        j = next(k for k, p in enumerate(u.pads) if p[0] == occ[t])
        n2 = u.pads[j][1]
        if n2 not in M.ADC_NETS | M.DIG_NETS:
            return None
        if u.pin_of[name] not in (M.ADC_PINS if n2 in M.ADC_NETS else M.DIG_PINS):
            return None
        if n2 in M.OUT_NETS and u.pin_of[name] not in M.OUT_PINS:
            return None
        if n2 in M.NO_PULL_NETS and u.pin_of[name] == 32:
            return None
        pads[j] = (pads[j][0], n2, *u.pins[u.pin_of[name]])
        new_of[pads[j][0]] = u.pin_of[name]
    pads[i] = (name, net, *u.pins[t])
    new_of[name] = t
    return pads, new_of


def anneal(st, moves=60000, seed=1, t0=6.0, t1=0.05, log=None, only=None, swaps=0.0):
    rnd = random.Random(seed)
    mov = [r for r in st.mov if (only is None or r in only)]
    sizes = {r: tuple(round(v, 1) for v in (st.u[r].boxes[0][2] - st.u[r].boxes[0][0],
                                             st.u[r].boxes[0][3] - st.u[r].boxes[0][1])) for r in mov}
    by_size = collections.defaultdict(list)
    for r in mov:
        if st.u[r].twin is None and r != "U1":
            by_size[tuple(sorted(sizes[r]))].append(r)
    cur = st.cost()
    acc = 0
    t_start = time.time()
    for k in range(moves):
        T = t0 * (t1 / t0) ** (k / moves)
        sig = max(0.25, 10.0 * T / t0)
        if swaps and rnd.random() < swaps:
            sm = swap_move(st, rnd)
            if sm is None:
                continue
            pads, new_of = sm
            uu = st.u["U1"]
            d, commit = st.try_move({"U1": (uu.x, uu.y, uu.a)}, pads=pads)
            if d <= 0 or rnd.random() < math.exp(-d / T):
                commit()
                uu.pin_of = new_of
                cur += d
                acc += 1
            continue
        r = rnd.choice(mov)
        u = st.u[r]
        q = rnd.random()
        if r == "U1":
            if q < 0.9:
                mv = {r: (u.x + rnd.gauss(0, sig), M.MOD_Y, u.a)}
            else:
                mv = {r: (u.x, M.MOD_Y, (u.a + 180) % 360)}
        elif q < 0.65:
            mv = {r: (u.x + rnd.gauss(0, sig), u.y + rnd.gauss(0, sig), u.a)}
        elif q < 0.85:
            mv = {r: (u.x, u.y, (u.a + rnd.choice((90, 180, 270))) % 360)}
        else:
            peers = [p for p in by_size.get(tuple(sorted(sizes[r])), []) if p != r]
            if not peers:
                continue
            p = rnd.choice(peers)
            v = st.u[p]
            mv = {r: (v.x, v.y, u.a if sizes[r] == sizes[p] else (u.a + 90) % 360),
                  p: (u.x, u.y, v.a if sizes[r] == sizes[p] else (v.a + 90) % 360)}
        ok = True
        for rr, m in mv.items():
            if not st.legal(rr, m, skip=set(mv)):
                ok = False
                break
        if ok and len(mv) == 2:
            a, b = list(mv)
            ba, bb = st.u[a].gboxes(*mv[a]), st.u[b].gboxes(*mv[b])
            ok = not any(M.box_dist(x, y) < M.GAP for x in ba for y in bb)
        if ok and len(mv) == 2 and getattr(st, "access", False):   # (a swap: each checked with the other moved)
            for p_, q_ in (list(mv), list(mv)[::-1]):
                up = st.u[p_]
                keep = (up.x, up.y, up.a, st.gb[p_], st.pg[p_])
                up.x, up.y, up.a = mv[p_]
                st.gb[p_], st.pg[p_] = up.gboxes(), R.padgeo(up, up.x, up.y, up.a)
                good = st.legal(q_, mv[q_])
                up.x, up.y, up.a, st.gb[p_], st.pg[p_] = keep
                if not good:
                    ok = False
                    break
        if not ok:
            continue
        d, commit = st.try_move(mv)
        if d <= 0 or rnd.random() < math.exp(-d / T):
            commit()
            cur += d
            acc += 1
        if log and k % 5000 == 0:
            print(f"{k:6d} T {T:5.2f} cost {cur:8.1f} X {st.X} L {st.L:7.0f} P {st.P:6.0f} B {st.B:6.0f} K {st.K:5.1f} C {st.C:6.1f} "
                  f"acc {acc} {time.time() - t_start:5.0f}s", flush=True)
    return st


def save(st, path):
    d = {r: [round(u.x, 3), round(u.y, 3), u.a] for r, u in st.u.items() if not u.fixed}
    u = st.u["U1"]
    d["_pins"] = {nm: pin for nm, pin in u.pin_of.items()}
    json.dump(d, open(path, "w"), indent=0)
