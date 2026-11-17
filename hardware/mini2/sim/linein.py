# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's digital LINE IN (2026-11-15; parts.py "LINE IN, digital"): two matched passive networks into two ADC1
pins, the firmware takes the difference. Writes sim/results.txt.

The circuit: tip and ring each through 20 k and 1 uF into node A (LINE_IN, GPIO9); the sleeve through two identical
branches (20 k + 1 uF each) into node B (LINE_SENSE, GPIO8); each node 12 k to the bias LIN_VB (R33 150 k from +3V3 /
R34 120 k (100 k + R153 20 k since 2026-11-15) = 1.467 V, C33 10 uF) and 2.2 nF C0G to GND; R36 / R37 100 k from tip / ring to GND, R39 100 ohm from the
sleeve to GND; the PESD15VL2BT (D7 on tip + ring, D14 on the sleeve; VRWM 15 V, VBR 17.1 .. 20.3 V, VCL 26 V at 8/20 us, 350 W). A - B = 0.273 x ((tip - sleeve) + (ring - sleeve)).
Models / assumptions: the ADC range ATTEN3 0 .. 2.9 V (the datasheet's 12 dB range); the ADC pins as their ESD diodes to
+3V3 and GND (a 1N4148-class card: ASSUMED); the source 100 .. 600 ohm; the TVS as two 18.7 V zeners back to back per line (+ 0.7 V diodes);
tolerances 1 % (resistors), 5 % (the 2.2 nF C0G), 10 % (the 1 uF X5R coupling caps: ASSUMED their grade).
  python3 hardware/mini2/sim/linein.py        (ngspice; seconds)
"""
import math
import os
import random
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
VB = 3.3 * 120 / 270
FS_LO, FS_HI = 0.0, 2.9
R_IN, C_IN, R_B, C_N, R_PD = 20e3, 1e-6, 12e3, 2.2e-9, 100e3


def node(f, vins, rs, cs, rb=R_B, cn=C_N):
    """a node's voltage (AC) from its branches (vin_i through rs_i + cs_i), rb to the bias (AC ground), cn to GND"""
    w = 2j * math.pi * f
    ys = [1 / (r + 1 / (w * c)) for r, c in zip(rs, cs)]
    return sum(v * y for v, y in zip(vins, ys)) / (sum(ys) + 1 / rb + w * cn)


def jack(vsig_t, vsig_r, vcm, rsrc, rpd=R_PD):
    """tip / ring at the jack: the source (its ground = the sleeve at vcm) through rsrc, loaded by the 100 k pull-downs
    to the board's GND (the network's own ~20 k load ignored here: it is the same on every side to first order)"""
    k = rpd / (rpd + rsrc)
    return (vcm + vsig_t) * k, (vcm + vsig_r) * k


def gains():
    out = []
    for f in (20, 50, 1000, 10000, 13300, 20800):
        a = node(f, [1, 0], [R_IN, R_IN], [C_IN, C_IN])
        both = node(f, [1, 1], [R_IN, R_IN], [C_IN, C_IN])
        out.append(f"  {f:>5} Hz: one channel {20 * math.log10(abs(a)):.2f} dB ({abs(a):.3f}), the same on both "
                   f"{20 * math.log10(abs(both)):.2f} dB ({abs(both):.3f})")
    # the input impedance at the tip (1 kHz): R36 // (20 k + 1 uF + (the node's other paths to AC ground))
    f, w = 1000, 2j * math.pi * 1000
    zrest = 1 / (1 / (R_IN + 1 / (w * C_IN)) + 1 / R_B + w * C_N)
    zin = 1 / (1 / R_PD + 1 / (R_IN + 1 / (w * C_IN) + zrest))
    out.append(f"  the input impedance at the tip, 1 kHz: {abs(zin) / 1e3:.1f} k (100 k // ~27.5 k)")
    return out


def levels():
    g1 = abs(node(1000, [1, 0], [R_IN, R_IN], [C_IN, C_IN]))
    g2 = abs(node(1000, [1, 1], [R_IN, R_IN], [C_IN, C_IN]))
    head = min(VB - FS_LO, FS_HI - VB)
    out = [f"  node A's centre {VB:.3f} V, {head:.3f} V to the nearer end of ATTEN3's 0 .. 2.9 V"]
    for what, g in (("the same signal on tip and ring", g2), ("one channel (a mono plug: the ring on the sleeve)", g1)):
        pk = head / g
        out.append(f"  {what}: the ADC clips at {pk:.2f} V peak = {20 * math.log10(pk / math.sqrt(2) / 0.7746):+.1f} dBu")
    for vcm in (0.1, 1.0):
        pk_cm = vcm * math.sqrt(2) * g2          # (an earth difference moves tip, ring and sleeve: both nodes by g2)
        left = head - pk_cm
        out.append(f"  with {vcm * 1000:.0f} mV rms of 50 Hz earth difference: both nodes swing +-{pk_cm:.3f} V; the "
                   f"same-signal input then clips at {left / g2:.2f} V peak ({20 * math.log10(left / g2 / math.sqrt(2) / 0.7746):+.1f} dBu)")
    return out


def monte_carlo(n=5000, seed=1):
    """the analog rejection of a 50 Hz / 1 kHz / 10 kHz earth difference: (A - B) from vcm, against the differential gain,
    with the parts' tolerances (uniform: 1 % R, 5 % C0G, 10 % the 1 uF), a 600 ohm source"""
    rng = random.Random(seed)
    out = []
    for f in (50, 1000, 10000):
        w = 2j * math.pi * f
        rej = []
        for _ in range(n):
            u = lambda t: 1 + rng.uniform(-t, t)
            rs_a = [R_IN * u(0.01), R_IN * u(0.01)]
            cs_a = [C_IN * u(0.10), C_IN * u(0.10)]
            rs_b = [R_IN * u(0.01), R_IN * u(0.01)]
            cs_b = [C_IN * u(0.10), C_IN * u(0.10)]
            rba, rbb, cna, cnb = R_B * u(0.01), R_B * u(0.01), C_N * u(0.05), C_N * u(0.05)
            vt, vr = jack(0, 0, 1.0, 600, R_PD * u(0.01))
            a = node(f, [vt, vr], rs_a, cs_a, rba, cna)
            b = node(f, [1.0, 1.0], rs_b, cs_b, rbb, cnb)
            vt2, vr2 = jack(1.0, 1.0, 0.0, 600)
            gd = node(f, [vt2, vr2], rs_a, cs_a, rba, cna)
            rej.append(20 * math.log10(abs(gd) / max(abs(a - b), 1e-12)))
        rej.sort()
        nom_a = node(f, list(jack(0, 0, 1.0, 600)), [R_IN] * 2, [C_IN] * 2)
        nom_b = node(f, [1.0, 1.0], [R_IN] * 2, [C_IN] * 2)
        nom_g = node(f, list(jack(1, 1, 0, 600)), [R_IN] * 2, [C_IN] * 2)
        out.append(f"  {f:>5} Hz: nominal {20 * math.log10(abs(nom_g) / abs(nom_a - nom_b)):.1f} dB (the 100 k pull-downs "
                   f"against a 600 ohm source); over {n} boards: median {rej[n // 2]:.1f} dB, 1 % worst "
                   f"{rej[n // 100]:.1f} dB, worst {rej[0]:.1f} dB")
    return out


def protection_case(amp, bv):
    """a +-amp square wave (100 Hz) on the tip through 100 ohm; the PESD15VL2BT (D7, tip + ring to pin 3) as two zeners
    back to back (VBR bv, RS 0.54 ohm: VCL 26 V at its 13.5 A, 8/20 us, from the 350 W) plus a 0.7 V forward diode in
    each; node A through its coupling cap. ngspice transient: the tip's and node A's extremes, the pin diodes' current"""
    net = f"""* prot
.model DZ D(BV={bv} IBV=1m CJO=10p RS=0.54)
.model DPIN D(IS=2.5n N=1.75 RS=0.6)
Vs s 0 PULSE({-amp} {amp} 0 1u 1u 5m 10m)
Rs s tip 100
D1 tip m DZ
D2 0 m DZ
R36 tip 0 100k
R30 tip a1 20k
C30 a1 a 1u
R31 r 0 1
R32 a vb 12k
C32 a 0 2.2n
Vb vb 0 {VB:.4f}
Rr r a1r 20k
C31 a1r a 1u
V33 v33 0 3.3
Dhi a vh DPIN
Vih vh v33 0
Dlo vl a DPIN
Vil 0 vl 0
.control
tran 2u 60m
meas tran vmax max v(a) from=20m to=60m
meas tran vmin min v(a) from=20m to=60m
meas tran tmax max v(tip) from=20m to=60m
meas tran ihi max i(Vih) from=20m to=60m
meas tran ilo max i(Vil) from=20m to=60m
meas tran itvs max i(Vs) from=20m to=60m
.endc
.end
"""
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(net)
        o = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=300).stdout
    m = {k: float(v) for k, v in re.findall(r"^(\w+)\s*=\s*([-+\d.eE]+)", o, re.M)}
    return (f"  +-{amp:g} V square, VBR {bv:g} V: the tip reaches +-{m['tmax']:.2f} V, {m['itvs'] * 1000:.1f} mA from the source; "
            f"node A {m['vmin']:.2f} .. {m['vmax']:.2f} V (ADC 0 .. 2.9 V, pin diodes conduct beyond -0.4 / 3.7 V); "
            f"pin-diode current: to +3V3 {max(m['ihi'], 0) * 1000:.3f} mA peak, to GND {max(m["ilo"], 0) * 1000:.3f} mA peak")


def protection():
    """+-10 V and +-12 V (modular outputs) on the tip; D7's PESD15VL2BT: VRWM 15 V, VBR 17.1 .. 20.3 V (nominal 18.7), so it
    does not conduct below 15 V: the 20 k / 1 uF network and the pin's diodes carry the event. Also VBR min (17.1 V) and a
    +-25 V case (the clamp working)"""
    out = [protection_case(a, bv) for a, bv in ((10, 18.7), (12, 18.7), (12, 17.1), (12, 20.3), (25, 17.1))]
    out.append("  the 100 k pull-down (R36) is all a +-12 V DC sees at the jack (VRWM 15 V: no clamp, ~nA leakage); node A"
               " is AC coupled: it sits at the bias for DC")
    return out


def main():
    out = ["jank mini 2.2, the digital LINE IN (two passive networks, the difference in firmware): results (linein.py)", ""]
    for name, lines in (("gain into a node (one branch of 20 k, the node's 12 k and 2.2 nF)", gains()),
                        ("levels (ATTEN3 0 .. 2.9 V)", levels()),
                        ("the analog rejection of an earth difference (A - B against the differential gain)", monte_carlo()),
                        ("protection", protection())):
        out.append(name + ":")
        out += lines
        out.append("")
    out.append("the anti-alias pole: 20 k // 20 k // 12 k = 5.45 k with 2.2 nF: 13.3 kHz; -5.4 dB at 20.8 kHz (the tip's"
               " Nyquist at 41.7 kHz per channel); a second matched RC was not added (the ADC's source impedance stays"
               " 5.45 k, two more parts per node)")
    open(os.path.join(HERE, "results.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
