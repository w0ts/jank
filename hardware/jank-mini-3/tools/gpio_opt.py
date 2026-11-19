"""jank-mini-3 study (KiCad's python): assign the jank functions to the LCD-2's GPIO pins from the LAYOUT, within the
electrical rules check_nets.py enforces. Cost = estimated track length from the pin to the nearest pad of the function's
net on the placed board, plus the detour round the right of the cut-out when the pad is in the other band. Optimal
assignment (Hungarian). Usage: python3 gpio_opt.py <placed board>"""
import math
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import make_board as M  # noqa: E402
import parts as P  # noqa: E402

OX = OY = 100.0
BIG = 1e6
LCD = {"J15", "J16"}
# pins: (row, pin) -> GPIO, with its board position (the male rows J15 = P1 bottom, J16 = P2 top)
PINS = {("J15", p): int(v[2:]) for p, v in P.LCD2_P1.items() if v.startswith("IO") and int(v[2:]) not in (19, 20)}
PINS.update({("J16", p): int(v[2:]) for p, v in P.LCD2_P2.items() if v.startswith("IO") and int(v[2:]) not in (43, 44)})
# the pins no function takes: two on the top row (J20's pads: in practice IO47 / IO48, the IMU bus, spares only) and two
# anywhere at no cost ("*": the functions get the best pins first; a free one on the bottom row then goes to J17's pads)
SPARES = {"SPARE_T1": "J16", "SPARE_T2": "J16", "SPARE_X1": "*", "SPARE_X2": "*"}


IMU = {47, 48}   # (the LCD-2's own IMU I2C, pulled up: spares only, a button there would short its bus)


def legal(func, gpio, row):
    if func in SPARES:
        return SPARES[func] in ("*", row)
    if gpio in IMU:
        return False
    if func in P.WEAK and gpio in P.LCD2_PULL:   # (review mini3-A W1: a weak divider loses to the LCD-2's 4.7k)
        return False
    if gpio in P.STRAPPING:
        return False
    if func in P.OUTPUTS:
        return 1 <= gpio <= 17 and gpio != 18 and "up" not in P.LCD2_PULL.get(gpio, "")
    if func in P.ADC1:
        return 1 <= gpio <= 10
    return True


def band(x, y):
    if x > M.CUT[2]:
        return "right"
    return "top" if y < (M.CUT[1] + M.CUT[3]) / 2 else "bottom"


def path(a, b):
    """track length estimate: straight, or round the cut-out's right edge when a and b are in opposite bands"""
    ba, bb = band(*a), band(*b)
    if {ba, bb} == {"top", "bottom"}:
        c1 = (M.CUT[2] + 1.0, M.CUT[1] - 1.0)
        c2 = (M.CUT[2] + 1.0, M.CUT[3] + 1.0)
        if ba == "bottom":
            c1, c2 = c2, c1
        return math.dist(a, c1) + math.dist(c1, c2) + math.dist(c2, b)
    return math.dist(a, b)


LAMBDA = float(os.environ.get("GPIO_CROSS_MM", 8.0))   # (one crossing ~ a via pair and its detour, in mm of track)
BEND = float(os.environ.get("GPIO_BEND_MM", 5.0))      # (straightness, mots: each bend of the estimated route, in mm)


def route_of(a, b):
    """the estimated path as segments: straight, or round the cut-out's right edge (as path())"""
    ba, bb = band(*a), band(*b)
    if {ba, bb} == {"top", "bottom"}:
        c1 = (M.CUT[2] + 1.0, M.CUT[1] - 1.0)
        c2 = (M.CUT[2] + 1.0, M.CUT[3] + 1.0)
        if ba == "bottom":
            c1, c2 = c2, c1
        return [(a, c1), (c1, c2), (c2, b)]
    return [(a, b)]


def _cross(p, q):
    (a, b), (c, d) = p, q
    def o(u, v, w):
        return (v[0] - u[0]) * (w[1] - u[1]) - (v[1] - u[1]) * (w[0] - u[0])
    if min(math.dist(a, c), math.dist(a, d), math.dist(b, c), math.dist(b, d)) < 0.05:
        return False   # (a shared end is not a crossing)
    return (o(a, b, c) > 0) != (o(a, b, d) > 0) and (o(c, d, a) > 0) != (o(c, d, b) > 0)


def crossings(r1, r2):
    return sum(_cross(p, q) for p in r1 for q in r2)


def mst_edges(points):
    if len(points) < 2:
        return []
    left, tree, out = list(points[1:]), [points[0]], []
    while left:
        d, i, t = min((math.dist(t, p), i, t) for i, p in enumerate(left) for t in tree)
        out.append((t, left[i]))
        tree.append(left.pop(i))
    return out


def hungarian(cost):
    """min-cost perfect assignment, square matrix; returns col for each row (O(n^3), Kuhn-Munkres with potentials)"""
    n = len(cost)
    u, v, p, way = [0.0] * (n + 1), [0.0] * (n + 1), [0] * (n + 1), [0] * (n + 1)
    for i in range(1, n + 1):
        p[0], j0 = i, 0
        minv, used = [math.inf] * (n + 1), [False] * (n + 1)
        while True:
            used[j0] = True
            i0, delta, j1 = p[j0], math.inf, 0
            for j in range(1, n + 1):
                if not used[j]:
                    cur = cost[i0 - 1][j - 1] - u[i0] - v[j]
                    if cur < minv[j]:
                        minv[j], way[j] = cur, j0
                    if minv[j] < delta:
                        delta, j1 = minv[j], j
            for j in range(n + 1):
                if used[j]:
                    u[p[j]] += delta
                    v[j] -= delta
                else:
                    minv[j] -= delta
            j0 = j1
            if p[j0] == 0:
                break
        while j0:
            j1 = way[j0]
            p[j0] = p[j1]
            j0 = j1
    res = [0] * n
    for j in range(1, n + 1):
        res[p[j] - 1] = j - 1
    return res


def main(path_pcb: str) -> None:
    board = pcbnew.LoadBoard(path_pcb)
    pos, dest = {}, {}
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        for pad in fp.Pads():
            xy = (pcbnew.ToMM(pad.GetPosition().x) - OX, pcbnew.ToMM(pad.GetPosition().y) - OY)
            if ref in LCD:
                pos[(ref, int(pad.GetNumber()))] = xy
            else:
                dest.setdefault(pad.GetNetname().lstrip("/"), []).append(xy)
    spare_pads = {"J15": [], "J16": []}
    for fp in board.GetFootprints():
        if fp.GetReference() in ("J17", "J20"):
            for pad in fp.Pads():
                xy = (pcbnew.ToMM(pad.GetPosition().x) - OX, pcbnew.ToMM(pad.GetPosition().y) - OY)
                spare_pads["J15" if fp.GetReference() == "J17" else "J16"].append(xy)
    funcs = [f for f in P.GPIO if not f.startswith("SPARE_")] + list(SPARES)
    pins = sorted(PINS)
    assert len(funcs) == len(pins), (len(funcs), len(pins))

    def cost_of(f, pin):
        if not legal(f, PINS[pin], pin[0]):
            return BIG
        if f in SPARES:   # (the distance to its row's spare pad set as placed; a "*" spare costs nothing)
            return 0.0 if SPARES[f] == "*" else min(path(pos[pin], d) for d in spare_pads[SPARES[f]])
        return min(path(pos[pin], d) for d in dest[f])

    def dest_of(f, pin):   # (the function's nearest destination pad from that pin)
        if f in SPARES:
            return None if SPARES[f] == "*" else min(spare_pads[SPARES[f]], key=lambda d: path(pos[pin], d))
        return min(dest[f], key=lambda d: path(pos[pin], d))

    def rat(f, pin):
        d = dest_of(f, pin)
        return [] if d is None else route_of(pos[pin], d)
    # (every other signal net's own rat lines stay put: a function's line crossing them is a linear cost)
    fixed = [e for n, pts in dest.items() if n not in P.GPIO and n not in ("GND", "") and not n.startswith("unconnected")
             for e in mst_edges(sorted(set(pts)))]
    lin = [[cost_of(f, pin) + (LAMBDA * crossings(rat(f, pin), fixed) + BEND * max(len(rat(f, pin)) - 1, 0)
                               if cost_of(f, pin) < BIG else 0) for pin in pins] for f in funcs]
    cost = [[cost_of(f, pin) for pin in pins] for f in funcs]
    pick = hungarian(lin)
    rats = {}

    def pair_x(i, pi, j, pj):
        k = (i, pi, j, pj)
        if k not in rats:
            rats[k] = crossings(rat(funcs[i], pins[pi]), rat(funcs[j], pins[pj]))
        return rats[k]

    def total(pk):
        t = sum(lin[i][pk[i]] for i in range(len(funcs)))
        return t + LAMBDA * sum(pair_x(i, pk[i], j, pk[j]) for i in range(len(funcs)) for j in range(i + 1, len(funcs)))
    best = total(pick)
    improved = True
    while improved:   # (pairwise swaps: the crossings between functions make it quadratic; local search from the
        improved = False   #  linear optimum)
        for i in range(len(funcs)):
            for j in range(i + 1, len(funcs)):
                if lin[i][pick[j]] >= BIG or lin[j][pick[i]] >= BIG:
                    continue
                pick[i], pick[j] = pick[j], pick[i]
                t = total(pick)
                if t < best - 1e-6:
                    best, improved = t, True
                else:
                    pick[i], pick[j] = pick[j], pick[i]

    def xcount(pk):
        own = sum(crossings(rat(funcs[i], pins[pk[i]]), fixed) for i in range(len(funcs)))
        mut = sum(pair_x(i, pk[i], j, pk[j]) for i in range(len(funcs)) for j in range(i + 1, len(funcs)))
        return own + mut
    now = {f: next(pin for pin in pins if f"{pin[0]}.{pin[1]}" == P.GPIO[f][1]) for f in P.GPIO if not f.startswith("SPARE_")}
    old_total = sum(cost_of(f, now[f]) for f in now)
    new_total = sum(cost[i][pick[i]] for i, f in enumerate(funcs) if f not in SPARES)
    now_pk = [pins.index(now[f]) if f in now else pick[i] for i, f in enumerate(funcs)]
    def bends(pk):
        return sum(max(len(rat(funcs[i], pins[pk[i]])) - 1, 0) for i in range(len(funcs)))
    print(f"estimated track length: now {old_total:.0f} mm, chosen map {new_total:.0f} mm; crossings: now "
          f"{xcount(now_pk)}, chosen {xcount(pick)} (lambda {LAMBDA:g} mm); bends: now {bends(now_pk)}, chosen "
          f"{bends(pick)} ({BEND:g} mm each)")
    for i, f in enumerate(funcs):
        pin = pins[pick[i]]
        was = now.get(f)
        mark = "" if pin == was else f"   (was {was[0]}.{was[1]} GPIO{PINS[was]})" if was else ""
        print(f"{f:12s} -> {pin[0]}.{pin[1]:<2d} GPIO{PINS[pin]:<3d} {cost[i][pick[i]]:6.1f} mm{mark}")


if __name__ == "__main__":
    main(sys.argv[1])
