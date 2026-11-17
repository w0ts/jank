# python3 measure.py PLACE.json [...]: crossings, KEEP, CONG, hard pulls over, wire length, the hottest cells, the KEEP edges
import json, math, sys, time
import model as M, anneal as A, render as Rn
units, fb, c = M.load()
for f in sys.argv[1:]:
    st = A.State(units, fb, c); st.access = True
    Rn.apply(st, json.load(open(f)))
    st.hard_ok("U1", (st.u["U1"].x, st.u["U1"].y, st.u["U1"].a))
    pos = {p[0].rstrip("'"): (p[2], p[3]) for ps in st.pg.values() for p in ps}
    over = [(a, b, m, round(math.dist(pos[a], pos[b]), 2)) for a, b, m in M.HARD if a in pos and b in pos and math.dist(pos[a], pos[b]) > m + 1e-6]
    ill = [r for r in st.mov if not st.legal(r, (st.u[r].x, st.u[r].y, st.u[r].a))]
    wl = sum(math.dist(a, b) for es in st.edges.values() for a, b, _ in es)
    print(f"{f}: X {st.X} KEEP {st.K:.1f} mm CONG {st.C:.1f} hard-over {len(over)} {over} wire {wl:.0f} mm (weighted L {st.L:.0f}) P {st.P:.0f} B {st.B:.0f} illegal {ill}")
    print("  LINE IN keep-out boxes:", [tuple(round(v, 1) for v in b) for b in st.RC.boxes])
    print("  KEEP edges:", st.RC.keep_edges())
    for h in st.RC.hot(10):
        print("  hot", h)
