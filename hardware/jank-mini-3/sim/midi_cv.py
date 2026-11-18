# SPDX-License-Identifier: GPL-3.0-only
"""MIDI OUT current loops (AO3400A sinks) and CLOCK / RESET into 10k // 100 nF. Writes midi_cv.txt.
MIDI (ngspice, then a Monte Carlo on the DC loop): V5M -> 220 (1206) -> ring / DIN 4 -> the receiver (MIDI 1.0: 220
ohm +-5 %, an opto LED fitted to 1.2 / 1.5 / 1.7 V at 5 mA, optionally a series 1N4148) -> tip / DIN 5 -> 220 ->
AO3400A (gate 3.3 V from the GPIO) -> GND. V5M 4.79..5.32 V (the 5.0 V boost's corners, clamp.py) and 5.37 V (USB pass-through).
Faults: ring to GND (the source's 1206 power), tip to V5M of another box.
CLOCK / RESET (ngspice): the GPIO -> 2 x 74AHCT04 (on V5C) modelled as V5C behind 50 ohm (TI SCLS228: VOH 4.4 V at
-8 mA at VCC 4.5 V) -> BAV99 -> 2 x 1k 1206 in series -> 2.2 nF -> the jack -> the module's input: 10k // 100 nF (the brief's
worst), 100k, 10k. Pulse 5 ms. Faults: +-10 / +-12 V into the output high and low; the power in each 1k (0.25 W).
  python3 sim/midi_cv.py"""
import math
import os
import random

from spice import MODELS, run

HERE = os.path.dirname(os.path.abspath(__file__))
LED = {"1.2 V": "D(IS=3.1e-14 N=1.8 RS=2)", "1.5 V": "D(IS=1.3e-15 N=2.0 RS=2)", "1.7 V": "D(IS=2.6e-17 N=2.0 RS=2)"}


def midi_deck(v5, led, series_diode):
    sd = "DS2 r3 r4 D1N4148\n.model D1N4148 D(IS=2.5n N=1.75 RS=0.6)\n" if series_diode else "R0 r3 r4 1m\n"
    return f"""* midi
{MODELS}
.model DL {LED[led]}
V5 V5M 0 {v5}
RSRC V5M ring 220
RRX ring r2 220
DL r2 r3 DL
{sd}RTIP r4 tip 0
RSNK tip k 220
VG g 0 3.3
MQ k g 0 AO3400
.op
.control
run
print i(V5)
.endc
.end
"""


def op_i(deck):
    import re
    import subprocess
    import tempfile
    with tempfile.TemporaryDirectory(dir=HERE, prefix=".run-") as t:
        p = os.path.join(t, "m.cir")
        open(p, "w").write(deck)
        r = subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, cwd=t)
    return -float(re.search(r"^i\(v5\)\s*=\s*([-+0-9.eE]+)", r.stdout + r.stderr, re.M).group(1))


def cv_deck(load, fault=None, high=True):
    loads = {"10k // 100n": "RL jk 0 10k\nCL jk 0 100n\n", "100k": "RL jk 0 100k\n", "10k": "RL jk 0 10k\n"}
    if fault is None:
        src = "VIN in 0 PULSE(0 1 1m 1u 1u 5m 20m)\n"
        tail = (".tran 2u 15m 0 5u\n.measure tran vhi FIND v(jk) AT=5.9m\n.measure tran trise TRIG v(jk) VAL=0.4 RISE=1 "
                "TARG v(jk) VAL=3.6 RISE=1\n.measure tran ipk MAX i(VDRV)\n")
        drv = "BDRV d0 0 V=v(in)*4.79\n"
    else:
        src = ""
        drv = f"BDRV d0 0 V={4.79 if high else 0}\n"
        tail = (f"VF jk 0 {fault}\n.op\n.control\nrun\nprint i(VF) i(VDRV) i(V5C)\n.endc\n")
        loads = {load: ""}
    return f"""* cv
{MODELS}
{src}{drv}VDRV d0 d1 0
RO d1 n 50
V5C v5c 0 4.79
DA 0 n BAV99
DB n v5c BAV99
R75 n m 1k
R78 m out 1k
C89 out 0 2.2n
RSL out jk 0.01
{loads[load]}{tail}.end
"""


def main():
    out = ["# midi_cv.py: MIDI OUT loops and CLOCK / RESET (ngspice, generic models)", ""]
    out.append("## MIDI: receiver current (MIDI 1.0 asks ~5 mA; optos switch from ~1.5..2 mA)")
    out.append("| V5M | opto LED | + 1N4148 | loop current |")
    out.append("|---|---|---|---|")
    pts = {}
    for v5 in (4.79, 5.0, 5.32, 5.37):
        for led in LED:
            for sd in (False, True):
                i = op_i(midi_deck(v5, led, sd))
                pts[(v5, led, sd)] = i
                out.append(f"| {v5} V | {led} | {'yes' if sd else 'no'} | {i * 1000:.2f} mA |")
    # Monte Carlo on the DC loop: V5M uniform 4.58..5.11, R 1 % (ours) / 5 % (receiver), LED drop at ~4-6 mA 1.1..1.75 V,
    # optional diode 0.6..0.75 V, the FET 0.03..0.06 ohm
    random.seed(2)
    lo, hi = 1, 0
    for _ in range(100_000):
        v = random.uniform(4.79, 5.32)
        drop = random.uniform(1.1, 1.75) + (random.uniform(0.6, 0.75) if random.random() < 0.5 else 0)
        r = 220 * random.uniform(0.99, 1.01) * 2 + 220 * random.uniform(0.95, 1.05) + 0.05
        i = (v - drop) / r
        lo, hi = min(lo, i), max(hi, i)
    out.append(f"\nMonte Carlo (100k): {lo * 1000:.2f} .. {hi * 1000:.2f} mA")
    p_short = 5.37 ** 2 / 220
    out.append(f"fault, ring / DIN 4 shorted to GND: {5.37 / 220 * 1000:.1f} mA, {p_short * 1000:.0f} mW in the 220 ohm"
               f" 1206 (250 mW) at the 5.37 V pass-through corner")
    out.append("fault, tip / DIN 5 to another box's +5 V: the FET off = no path (open drain); FET on: "
               f"{5.37 / 220 * 1000:.1f} mA through the sink, AO3400A {(5.37 / 220) ** 2 * 0.05 * 1000:.2f} mW")
    out.append("\n## CLOCK / RESET into a module input (5 ms pulse)")
    out.append("| load | high level | rise 0.4 -> 3.6 V | peak drive current (74AHCT04: 8 mA rec., 25 mA abs) |")
    out.append("|---|---|---|---|")
    for load in ("10k // 100n", "100k", "10k"):
        m = run(cv_deck(load), "cv")
        tr = f"{m['trise'] * 1e6:.0f} us" if "trise" in m else "never reaches 3.6 V"
        out.append(f"| {load} | {m['vhi']:.2f} V | {tr} | {m['ipk'] * 1000:.1f} mA |")
    out.append("\n## CLOCK / RESET faults: a stiff +-10 / +-12 V source on the jack")
    out.append("| fault | output state | into the jack | from the 74AHCT04 (8 mA rec., 25 abs) | into V5C (BAV99) | each 1k 1206 (0.25 W) |")
    out.append("|---|---|---|---|---|---|")
    import re
    import subprocess
    import tempfile
    for v in (10, -10, 12, -12):
        for high in (True, False):
            with tempfile.TemporaryDirectory(dir=HERE, prefix=".run-") as t:
                p = os.path.join(t, "f.cir")
                open(p, "w").write(cv_deck("10k", v, high))
                r = subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, cwd=t)
            vals = {k: float(x) for k, x in re.findall(r"^(i\([a-z0-9]+\))\s*=\s*([-+0-9.eE]+)", r.stdout + r.stderr, re.M)}
            out.append(f"| {v:+d} V | {'high' if high else 'low'} | {-vals['i(vf)'] * 1000:+.1f} mA |"
                       f" {vals['i(vdrv)'] * 1000:+.1f} mA | {-vals['i(v5c)'] * 1000:+.1f} mA | {vals['i(vf)'] ** 2 * 1000 * 1000:.0f} mW |")
    txt = "\n".join(out) + "\n"
    open(os.path.join(HERE, "midi_cv.txt"), "w").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
