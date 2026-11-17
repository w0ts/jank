# SPDX-License-Identifier: GPL-3.0-only
"""python3 run.py START.json OUT.json MOVES SEED [T0] [SWAPS]: anneal (SWAPS: the share of moves that are module pin
swaps, 0 = the GPIO map kept), then a cold polish"""
import json, sys
import model as M, anneal as A, render as R
units, fb, c = M.load()
st = A.State(units, fb, c)
st.access = True
R.apply(st, json.load(open(sys.argv[1])))
u1 = st.u["U1"]
used = set(u1.pin_of.values())
for i, (nm, n, dx, dy) in enumerate(list(u1.pads)):      # (an output on a forbidden pin: to a free allowed one first)
    if n in M.OUT_NETS and u1.pin_of[nm] not in M.OUT_PINS:
        t = min(M.OUT_PINS - used)
        used.add(t)
        used.discard(u1.pin_of[nm])
        u1.pads[i] = (nm, n, *u1.pins[t])
        u1.pin_of[nm] = t
st.rebuild()
print("start X", st.X, "cost", round(st.cost()), flush=True)
A.anneal(st, moves=int(sys.argv[3]), seed=int(sys.argv[4]), t0=float(sys.argv[5]) if len(sys.argv) > 5 else 6.0, log=True,
         swaps=float(sys.argv[6]) if len(sys.argv) > 6 else 0.0)
A.anneal(st, moves=60000, seed=99, t0=0.05, t1=0.001, swaps=float(sys.argv[6]) if len(sys.argv) > 6 else 0.0)   # (the polish)
st.rebuild()
print("end X", st.X, "L", round(st.L), "cost", round(st.cost()), flush=True)
for nm, n, _x, _y in u1.pads:
    p = u1.pin_of.get(nm)
    assert not (n in M.OUT_NETS and p not in M.OUT_PINS) and not (n in M.ADC_NETS and p not in M.ADC_PINS), (nm, n, p)
A.save(st, sys.argv[2])
