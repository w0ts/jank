# SPDX-License-Identifier: GPL-3.0-only
"""the placement fix (2026-11-15): take a placement, make every unit legal under the pin-access rules and the converter
templates (each illegal unit to the nearest legal spot, the templates first), then a cool anneal that keeps the layout.
python3 fix.py IN.json OUT.json [MOVES] [SEED] [T0]"""
import itertools, json, math, os, random, sys, time
import model as M, anneal as A, render as Rn, init as I

units, fb, c = M.load()
st = A.State(units, fb, c)
Rn.apply(st, json.load(open(sys.argv[1])))
st.access = True
st.hard_parts_only = True                   # (legalising: the pulled parts move to their pins, the ICs stay)


def illegal():
    return [r for r in st.mov if not st.legal(r, (st.u[r].x, st.u[r].y, st.u[r].a))]


t0 = time.time()
def target(r, x0, y0):
    """where a part with hard pulls should look first: its pins' middle (else where it is)"""
    st.hard_ok(r, (st.u[r].x, st.u[r].y, st.u[r].a))
    pins = [b for a, b, m in st._hard.get(r, []) if st._own.get(a) == r and m != M.CHAIN_MM]
    pins += [a for a, b, m in st._hard.get(r, []) if st._own.get(b) == r and m == M.CHAIN_MM]
    pts = []
    for b in pins:
        q = next((p for p in st.pg[st._own[b]] if p[0].rstrip("'") == b), None)
        if q:
            pts.append((q[2], q[3]))
    if not pts:
        return x0, y0
    return sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)


bad = illegal()
print("illegal at the start:", len(bad), sorted(bad), flush=True)
# (2026-11-15, the hard pulls: every pulled part, and every other part sitting within 4.5 mm of a pulled pin, off the
#  board first; then the pulled parts at their pins, then the others back near where they were)
st.hard_ok("U1", (st.u["U1"].x, st.u["U1"].y, st.u["U1"].a))
pulled = {st._own[a] for a, b, m in M.HARD if a in st._own and not st.u[st._own[a]].fixed and m != M.CHAIN_MM}
chained = {st._own[b] for a, b, m in M.HARD if m == M.CHAIN_MM and b in st._own and not st.u[st._own[b]].fixed} - pulled
pins = []
for a, b, _m in M.HARD:
    if b in st._own:
        q = next((p for p in st.pg[st._own[b]] if p[0].rstrip("'") == b), None)
        if q:
            pins.append((q[2], q[3]))
crowd = {r for r in st.mov if r not in pulled and r not in chained and r != "U1" and not r.startswith("U") and
         any(M.box_dist(bx, (x - 4.5, y - 4.5, x + 4.5, y + 4.5)) == 0 for bx in st.gb[r] for x, y in pins)}
old = {r: (st.u[r].x, st.u[r].y, st.u[r].a) for r in pulled | chained | crowd}
TOUCH = os.environ.get("FIX_TOUCH") == "1"   # (a touch-up of a finished placement: only its illegal parts move)
if not TOUCH:
    for r in pulled | chained | crowd:
        st.u[r].x, st.u[r].y = 300.0, 300.0
        st.gb[r] = st.u[r].gboxes(); st.pg[r] = A.R.padgeo(st.u[r], 300.0, 300.0, st.u[r].a)
    print(f"evicted: {len(pulled)} pulled, {len(chained)} chained, {len(crowd)} in the way", flush=True)
    for r in sorted(pulled, key=lambda r: -len(st._hard.get(r, []))) + sorted(chained):
        x0, y0, a0 = old[r]
        tx, ty = target(r, x0, y0)
        if not I.place_near(st, r, tx, ty, rots=(a0, (a0 + 90) % 360, (a0 + 180) % 360, (a0 + 270) % 360)):
            print("pulled part without room:", r, flush=True)
    for r in sorted(crowd):
        x0, y0, a0 = old[r]
        if not I.place_near(st, r, x0, y0, rots=(a0, (a0 + 90) % 360, (a0 + 180) % 360, (a0 + 270) % 360)):
            print("evicted part without room:", r, flush=True)
st.rebuild()
bad = illegal()
print("after the pulled placement:", len(bad), sorted(bad), flush=True)


def put(r, x, y, a):
    u = st.u[r]
    u.x, u.y, u.a = x, y, a
    st.gb[r] = u.gboxes(); st.pg[r] = A.R.padgeo(u, x, y, a)


BIG = ("U", "Q", "J", "SW", "L", "Y", "BT")


def make_room(r):
    """a pulled part still without room: the small parts round its pins (not pulled ones) aside, the part at its
    pins, then those parts back to the nearest legal spot; all or nothing"""
    tx, ty = target(r, st.u[r].x, st.u[r].y)
    m = max(mm for a, b, mm in st._hard[r]) + 1.5
    zone = (tx - m, ty - m, tx + m, ty + m)
    blk = [o for o in st.mov if o != r and o not in pulled and o not in chained and not o.startswith(BIG)
           and any(M.box_dist(bx, zone) == 0 for bx in st.gb[o])]
    save = {o: (st.u[o].x, st.u[o].y, st.u[o].a) for o in blk + [r]}
    for o in blk:
        put(o, 300.0, 300.0, st.u[o].a)
    a0 = st.u[r].a
    ok = I.place_near(st, r, tx, ty, rots=(a0, (a0 + 90) % 360, (a0 + 180) % 360, (a0 + 270) % 360))
    if ok:
        st.pg[r] = A.R.padgeo(st.u[r], st.u[r].x, st.u[r].y, st.u[r].a)
        for o in sorted(blk, key=lambda o: -sum((b[2] - b[0]) * (b[3] - b[1]) for b in st.u[o].boxes)):
            x0, y0, a1 = save[o]
            if not I.place_near(st, o, x0, y0, rots=(a1, (a1 + 90) % 360, (a1 + 180) % 360, (a1 + 270) % 360)):
                ok = False
                break
            st.pg[o] = A.R.padgeo(st.u[o], st.u[o].x, st.u[o].y, st.u[o].a)
    if not ok:
        for o, p in save.items():
            put(o, *p)
    print("make room for", r, "moved", len(blk), "ok" if ok else "FAILED", flush=True)
    return ok


def near_lim(r, tx, ty, kmax=2500):
    """init.place_near within about 12 mm"""
    u = st.u[r]
    a0 = u.a
    for k in range(kmax):
        d = 0.25 * math.sqrt(k)
        t = k * 2.39996
        x, y = tx + d * math.cos(t), ty + d * math.sin(t)
        for a in (a0, (a0 + 90) % 360, (a0 + 180) % 360, (a0 + 270) % 360):
            if st.legal(r, (x, y, a)):
                put(r, x, y, a)
                return True
    return False


def solve_cluster(r):
    """make_room failed: every pulled part whose pins lie within 4 mm of r's pins, and the small parts round them,
    aside; then the pulled parts back in up to 40 orders until all of them fit; all or nothing"""
    tx, ty = target(r, st.u[r].x, st.u[r].y)
    mates = [o for o in pulled | chained if o != r and math.dist(target(o, st.u[o].x, st.u[o].y), (tx, ty)) <= 4.0]
    group = [r] + mates
    m = max(mm for g in group for a, b, mm in st._hard[g]) + 1.5
    zone = (tx - m - 2, ty - m - 2, tx + m + 2, ty + m + 2)
    blk = [o for o in st.mov if o not in group and o not in pulled and o not in chained and not o.startswith(BIG)
           and any(M.box_dist(bx, zone) == 0 for bx in st.gb[o])]
    save = {o: (st.u[o].x, st.u[o].y, st.u[o].a) for o in group + blk}
    tg = {g: target(g, *save[g][:2]) for g in group}
    area = lambda o: sum((b[2] - b[0]) * (b[3] - b[1]) for b in st.u[o].boxes)
    orders = [tuple(sorted(group, key=lambda o: -area(o))), tuple(sorted(group, key=lambda o: -len(st._hard[o])))]
    if len(group) <= 4:
        orders += list(itertools.permutations(group))
    rng = random.Random(len(group))
    while len(orders) < 40:
        g = list(group)
        rng.shuffle(g)
        orders.append(tuple(g))
    for order in orders[:40]:
        for o in group + blk:
            put(o, 300.0, 300.0, save[o][2])
        ok = all(near_lim(g, *tg[g]) for g in order)
        if ok:
            for o in sorted(blk, key=lambda o: -sum((b[2] - b[0]) * (b[3] - b[1]) for b in st.u[o].boxes)):
                if not I.place_near(st, o, save[o][0], save[o][1], rots=tuple((save[o][2] + k) % 360 for k in (0, 90, 180, 270))):
                    ok = False
                    break
                st.pg[o] = A.R.padgeo(st.u[o], st.u[o].x, st.u[o].y, st.u[o].a)
        if ok:
            print("cluster for", r, group, "moved", len(blk), "ok", flush=True)
            return True
    for o, p in save.items():
        put(o, *p)
    print("cluster for", r, group, "moved", len(blk), "FAILED", flush=True)
    return False


for r in sorted(r for r in bad if r in pulled or r in chained):
    if not st.legal(r, (st.u[r].x, st.u[r].y, st.u[r].a)):
        make_room(r) or solve_cluster(r)
st.rebuild()
bad = illegal()
print("after making room:", len(bad), sorted(bad), flush=True)
order = [r for r in ("U2", "U18", "U17", "U11", "U12", "U8") if r in bad] + [r for r in bad if r not in ("U2", "U18", "U17", "U11", "U12", "U8")]
for rnd in range(3):
    for r in order:
        u = st.u[r]
        if st.legal(r, (u.x, u.y, u.a)):
            continue
        px, py = u.x, u.y
        x0, y0 = (u.x, u.y) if u.x < 200 else old.get(r, (M.W / 2, M.H / 2, 0))[:2]   # (off the board: from where it was)
        u.x, u.y = 300.0, 300.0                    # (off the board while it looks: its old place blocks nothing)
        st.gb[r] = u.gboxes(); st.pg[r] = A.R.padgeo(u, u.x, u.y, u.a)
        tx, ty = target(r, x0, y0)
        if not I.place_near(st, r, tx, ty, rots=(u.a, (u.a + 90) % 360, (u.a + 180) % 360, (u.a + 270) % 360)):
            u.x, u.y = px, py
            st.gb[r] = u.gboxes()
        st.pg[r] = A.R.padgeo(u, u.x, u.y, u.a)
    st.rebuild()
    order = illegal()
    print(f"pass {rnd}: illegal {len(order)} {sorted(order)}  X {st.X}  {time.time() - t0:.0f}s", flush=True)
    if not order:
        break
st.hard_parts_only = False
moves = int(sys.argv[3]) if len(sys.argv) > 3 else 120000
A.anneal(st, moves=moves, seed=int(sys.argv[4]) if len(sys.argv) > 4 else 1, t0=float(sys.argv[5]) if len(sys.argv) > 5 else 1.0,
         t1=0.02, log=True, swaps=0.0)
st.rebuild()
for rnd in range(3):                               # (whatever the anneal left illegal: to the nearest legal spot)
    bad = illegal()
    if not bad:
        break
    for r in bad:
        u = st.u[r]
        px, py = u.x, u.y
        x0, y0 = (u.x, u.y) if u.x < 200 else old.get(r, (M.W / 2, M.H / 2, 0))[:2]   # (off the board: from where it was)
        u.x, u.y = 300.0, 300.0
        st.gb[r] = u.gboxes(); st.pg[r] = A.R.padgeo(u, u.x, u.y, u.a)
        if not I.place_near(st, r, x0, y0, rots=(u.a, (u.a + 90) % 360, (u.a + 180) % 360, (u.a + 270) % 360)):
            u.x, u.y = px, py
            st.gb[r] = u.gboxes()
        st.pg[r] = A.R.padgeo(u, u.x, u.y, u.a)
    st.rebuild()
def clear_for(r):
    """an IC or template illegal where it stands (a neighbour in its pin lanes or its ground ring): the small parts
    within 2.5 mm aside; if that makes it legal, they go back one by one to the nearest legal spot; all or nothing"""
    u = st.u[r]
    pose = (u.x, u.y, u.a)
    near = [o for o in st.mov if o != r and not o.startswith(BIG)
            and any(M.box_dist(b, ob) < 2.5 for b in st.gb[r] for ob in st.gb[o])]
    save = {o: (st.u[o].x, st.u[o].y, st.u[o].a) for o in near}
    for o in near:
        put(o, 300.0, 300.0, save[o][2])
    ok = st.legal(r, pose)
    if ok:
        for o in sorted(near, key=lambda o: -sum((b[2] - b[0]) * (b[3] - b[1]) for b in st.u[o].boxes)):
            if not near_lim(o, *target(o, *save[o][:2]), kmax=6000):
                ok = False
                break
        ok = ok and st.legal(r, pose)
    if not ok:
        for o, p in save.items():
            put(o, *p)
    print("clear for", r, "moved", len(near), "ok" if ok else "FAILED", flush=True)
    return ok


st.hard_parts_only = True                          # (a pulled part still out: make room / the cluster once more)
for r in sorted(r for r in illegal() if r in pulled or r in chained):
    make_room(r) or solve_cluster(r)
for r in sorted(illegal()):
    clear_for(r)
st.hard_parts_only = False
st.rebuild()
print("end X", st.X, "L", round(st.L), "illegal", illegal(), f"{time.time() - t0:.0f}s", flush=True)
A.save(st, sys.argv[2])
