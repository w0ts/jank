# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2: U19, the CJ431 clamp on the 5 V aux rail, with a series resistor (review pass 3, 2026-11-15), in
ngspice. Writes sim/clamp.txt.

The finding: the TPS61023 passes VIN through when VIN > VOUT (SLVSF14B, 7.3.5): on USB, VREG (VBUS - D3) reaches V5A,
and a 5.4 .. 5.5 V charger puts it over the clamp's lowest threshold (5.11 V). Nothing limited U19's current (150 mA /
300 mW). The fix: R145 in series with U19's cathode. This script, with cv.py's models and stages (its docstring):
 1. pass-through: V5A from VREG (an ideal source through 0.1 ohm and a diode: the boost's rectifier, ASSUMED), VREG
    5.0 .. 5.3 V (5.5 V VBUS minus D3's ~0.25 V at a light load: B5819W, its sheet's curve, read by eye), the clamp at its
    lowest threshold 5.11 V: U19's current and power, with R145 0 / 4.7 / 10 ohm;
 2. the faults of cv.py (+-12 V into CLOCK and RESET, the rail unloaded, the boost at 5.03 V, the clamp at 5.11 / 5.32 V):
    the highest V5A, against the WS2812's 5.3 V maximum.
  python3 hardware/mini2/sim/clamp.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cv

HERE = os.path.dirname(os.path.abspath(__file__))


def rail_r(rs):
    def rail(vb, vth):
        return f"""Vb bs 0 {vb}
Rb bs bs2 0.1
Db bs2 v5a DID
C100 v5a 0 10u
Rdiv v5a 0 98k
Rcl v5a cl {max(rs, 1e-3)}
Bclamp cl 0 I = max(0, (V(cl) - {vth}) / 0.2) + 1u
Vcl v5a v5c 0
"""
    return rail


FAULTS = [("+12 V into BOTH, both low", 12, 12, "LOW"), ("+12 V into BOTH, both high", 12, 12, "HIGH")]


def faults(lo, hi):
    """cv.py's double fault (+12 V into CLOCK and RESET at once, the rail unloaded, the boost at 5.03 V): V5A and U19's
    own current (at its cathode, after R145)"""
    out, worst = [], 0.0
    for vth in (lo, hi):
        for name, vc, vr, g in FAULTS:
            gate = getattr(cv, g)
            net = cv.netlist(5.03, vth, gate["c"], gate["r"], f"Vxc oc 0 {vc}", f"Vxr or 0 {vr}")
            m = cv.run(net, "tran 1u 3m 2m\nmeas tran r avg v(v5a) from=2.5m to=3m\nmeas tran c avg v(cl) from=2.5m to=3m\n"
                       + cv.echo("r", "c"))
            i = max(0.0, (m["c"] - vth) / 0.2)
            worst = max(worst, m["r"])
            out.append(f"clamp {vth} V, {name}: V5A {m['r']:.3f} V, U19 {i * 1000:.1f} mA ({i * m['c'] * 1000:.0f} mW)")
    return out, worst


def case(rs, lo, hi, label):
    cv.rail = rail_r(rs)
    out = [f"{label}: R145 {rs} ohm, the clamp's threshold {lo} .. {hi} V:"]
    worst_p = 0.0
    for vreg in (5.03, 5.1, 5.2, 5.3):
        net = cv.netlist(vreg, lo, cv.LOW["c"], cv.LOW["r"], cv.load("c", "100k"), cv.load("r", "100k"))
        m = cv.run(net, "tran 1u 3m 2m\nmeas tran r avg v(v5a) from=2.5m to=3m\nmeas tran c avg v(cl) from=2.5m to=3m\n"
                   + cv.echo("r", "c"))
        i = max(0.0, (m["c"] - lo) / 0.2)
        p = i * m["c"]
        worst_p = max(worst_p, p)
        what = "the boost at its highest (5.03 V)" if vreg == 5.03 else f"pass-through, VREG {vreg} V"
        out.append(f"  {what}, clamp at its lowest {lo} V: V5A {m['r']:.3f} V, U19 {i * 1000:.1f} mA, {p * 1000:.0f} mW "
                   f"(its 300), R145 {i * i * rs * 1000:.1f} mW")
    f, worst = faults(lo, hi)
    out += ["  " + x for x in f]
    out.append(f"  worst fault V5A {worst:.3f} V (the WS2812's 5.3 V)")
    return out, worst_p, worst


def main():
    out = ["jank mini 2.2, U19 (the 5 V clamp) with a series resistor R145: ngspice (clamp.py; models: cv.py)", ""]
    summ = []
    for rs, lo, hi, label in ((0.0, 5.11, 5.32, "as it was (R138 51 k / R139 47 k)"),
                              (10.0, 5.11, 5.32, "R145 alone"),
                              (10.0, 4.95, 5.05, "R145 + R138 47 k / R139 47 k (5.0 V +-1 %)")):
        o, p, w = case(rs, lo, hi, label)
        out += o + [""]
        summ.append(f"{label}: U19 up to {p * 1000:.0f} mW, worst-fault V5A {w:.3f} V")
    out.append("SUMMARY: " + "; ".join(summ))
    open(os.path.join(HERE, "clamp.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
