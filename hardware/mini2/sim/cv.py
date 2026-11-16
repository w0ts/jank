# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's CLOCK / RESET outputs from the 5 V aux rail (parts.py "AUX5", "_cv"; README "CLOCK / RESET"), in
ngspice. Writes sim/cv.txt. 2026-11-15: the charge pump is gone (the user's); the outputs run from V5A (U18, a TPS61023
boost, 4.91 V) through R132 (V5C); the user accepts ~4.5 V into 10 k (~4.95 V into 100 k, the usual Eurorack input).

The circuit. Per output: the GPIO (CLK_IO / RST_IO) on Q3 2N7002's gate (R71 100 k to GND), its drain pulls Q4 AO3401A's
gate (R73 10 k up to V5C: off), Q4's source on V5C, its drain the node N (R74 10 k and C89 2.2 nF to GND; D10 BAV99:
GND -> N, N -> V5C), two 2 k 1206 in parallel (1 k) from N to the jack's tip; the sleeve to GND through R120 / R121
100 ohm (the ground study; 47 until 2026-11-15). The rail: U19 (CJ431) with R138 47 k / R139 47 k behind R145 10 ohm clamps V5A at ~5.0 V: a wrong +12 V on the
outputs pushes current into the rail through the BAV99s and the P-FETs' body diodes, which the boost cannot sink.

Models (each one's source; none is the part's maker's own model unless said):
 - Q3: a 2N7002 VDMOS with the parameters of LTspice's standard.mos entry (Fairchild), copied by hand.
 - Q4: a GENERIC p-channel VDMOS, ASSUMED: Vto -0.9 V, Kp 12, Rd 20 mohm (the AO3401A's typical threshold and ~50 mohm).
 - D10 / D11 (BAV99): a 1N4148-class small-signal diode, ASSUMED close to the BAV99.
 - U18 (TPS61023): an ideal source of VB (4.72 / 4.85 / 4.97 V: R134 1 M / 140 k and its VREF's +-2.5 %) through 50 mohm and a diode, so it
   sources but cannot sink (a boost's rectifier), 10 uF of output cap (C100 + C101 at 5 V: ASSUMED from DC bias).
 - U19 (CJ431) behind R145 10 ohm: above VTH (4.95 / 5.0 / 5.05 V: its Vref +-1 %, R138 47 k / R139 47 k at 1 %) it sinks (V - VTH) / 0.2 ohm (its
   sheet's 0.2 ohm typical dynamic impedance), 1 uA below; the divider's 53 uA.
 - The GPIOs: high = 3.3 V through 17 ohm, low = 0 V through 12 ohm (midi_faults.py's fit to Table 5-4).
  python3 hardware/mini2/sim/cv.py        (ngspice; under a minute)
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
P1206 = 0.25
MODELS = """
.model QN VDMOS(Rg=3 Vto=1.6 Rd=0 Rs=.75 Rb=.14 Kp=.17 mtriode=1.25 Cgdmax=80p Cgdmin=12p Cgs=50p Cjo=50p Is=.04p)
.model QP VDMOS(pchan Rg=6 Vto=-0.9 Rd=.02 Rs=.01 Kp=12 Cgdmax=180p Cgdmin=60p Cgs=600p Cjo=150p Is=1e-12)
.model DS D(IS=2.52n RS=.568 N=1.752 CJO=4p M=.4 TT=20n BV=100)
.model DID D(IS=1e-12 N=0.01 RS=0.05)
"""
TYP, WORST_LOW, WORST_HIGH = (4.845, 5.0), (4.724, 5.05), (4.966, 4.95)   # (the boost, the clamp's threshold)
# (2026-11-15, the Standard study's experiment A + review pass 3: the boost 4.85 V (R136 20 k: 0.595 x (1 + 1000 / 140)),
#  its VREF +-2.5 %; the clamp 5.0 V +-1 % (R138 47 k / R139 47 k) behind R145 10 ohm; C89 / C90 2.2 nF; the sleeves'
#  R120 / R121 100 ohm)


def rail(vb, vth):
    return f"""Vb bs 0 {vb}
Db bs v5a DID
C100 v5a 0 10u
Rdiv v5a 0 98k
Rcl v5a cl 10
Bclamp cl 0 I = max(0, (V(cl) - {vth}) / 0.2) + 1u
Vcl v5a v5c 0
"""


def stage(k, g):
    return f"""R71{k} {g} 0 100k
M3{k} pg{k} {g} 0 0 QN
R73{k} v5c pg{k} 10k
M4{k} n{k} pg{k} v5c v5c QP
Dlo{k} 0 n{k} DS
Dhi{k} n{k} v5c DS
R74{k} n{k} 0 10k
Cn{k} n{k} 0 2.2n
R75{k} n{k} o{k}x 1k
Vm{k} o{k}x o{k} 0"""


def netlist(vb, vth, gate_c, gate_r, load_c, load_r):
    return f"""* cv
{MODELS}
{rail(vb, vth)}
{gate_c}
{gate_r}
{stage("c", "gc")}
{stage("r", "gr")}
{load_c}
{load_r}
"""


def run(net, ctl, timeout=900):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(net + f".control\n{ctl}\n.endc\n.end\n")
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=timeout).stdout
    return {m[1]: float(m[2]) for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M)}


def echo(*names):
    return "\n".join(f'echo "{n} = $&{n}"' for n in names)


HIGH = {"c": "Vgc gcs 0 3.3\nRgc gcs gc 17", "r": "Vgr grs 0 3.3\nRgr grs gr 17"}
LOW = {"c": "Vgc gcs 0 0\nRgc gcs gc 12", "r": "Vgr grs 0 0\nRgr grs gr 12"}


def load(k, r):
    return f"Rl{k} o{k} sl{k} {r}\nCl{k} o{k} sl{k} 100p\nRs{k} sl{k} 0 100"   # (the jack's sleeve through R120 / R121)


def levels():
    out, res = [], {}
    for (vb, vth), corner in ((TYP, "typ"), (WORST_LOW, "low (boost -2.5 %)")):
        for rl in ("10k", "100k"):
            m = run(netlist(vb, vth, HIGH["c"], HIGH["r"], load("c", rl), load("r", rl)),
                    "tran 1u 3m 2m\nlet d = v(oc) - v(slc)\nmeas tran oc avg d from=2.5m to=3m\nmeas tran r avg v(v5a) from=2.5m to=3m\n"
                    + echo("oc", "r"))
            res[(corner, rl)] = m["oc"]
            out.append(f"V5A {m['r']:.3f} V ({corner}), both outputs high into {rl}: the outputs {m['oc']:.3f} V (tip to sleeve)")
    return out, res


def edges():
    out = []
    for (vb, vth), corner in ((TYP, "typ"), (WORST_LOW, "low")):
        g = "Vgc gcs 0 PULSE(0 3.3 1m 2n 2n 100u 200u)\nRgc gcs gc 15"
        net = netlist(vb, vth, g, LOW["r"], load("c", "10k"), load("r", "10k"))
        m = run(net, "tran 20n 1.3m 0.95m\nmeas tran hi avg v(oc) from=1.06m to=1.09m\n" + echo("hi"))
        hi = m["hi"]
        m = run(net, f"tran 20n 1.3m 0.95m\nmeas tran tr trig v(oc) val={0.1 * hi} rise=1 targ v(oc) val={0.9 * hi} rise=1\n"
                f"meas tran tf trig v(oc) val={0.9 * hi} fall=1 targ v(oc) val={0.1 * hi} fall=1\n" + echo("tr", "tf"))
        out.append(f"{corner}: a 100 us pulse into 10 k // 100 pF: high {hi:.2f} V, rise (10-90 %) {m['tr'] * 1e9:.0f} ns, "
                   f"fall {m['tf'] * 1e6:.1f} us")
    return out


def blip():
    """the switch-on blip (review pass 1's sim pass 5): the rail rising from 0 with the GPIOs floating (the gates held
    by R71 / R77), the P-FET's drain-source capacitance into the node's 10 k // C89; the outputs' peak into 100 k"""
    out = []
    for tr in (1e-6, 13.8e-6, 100e-6, 700e-6):
        net = f"""* blip
{MODELS}
Vb bs 0 PWL(0 0 10u 0 {10e-6 + tr} 4.966)
Db bs v5a DID
C100 v5a 0 10u
Rdiv v5a 0 98k
Vcl v5a v5c 0
Rgfc gc 0 1e9
Rgfr gr 0 1e9
{stage("c", "gc")}
{stage("r", "gr")}
{load("c", "100k")}
{load("r", "100k")}
"""
        m = run(net, f"tran 20n {10e-6 + tr + 300e-6}\nmeas tran pk max v(oc)\n" + echo("pk"))
        out.append(f"the rail rising in {tr * 1e6:g} us, the GPIOs floating: CLOCK's peak {m['pk'] * 1000:.0f} mV into 100 k")
    return out


def faults():
    """a wrong voltage into the outputs (another module's output), the rail at its lightest load (nothing else on V5A),
    the boost at its highest (4.97 V) and the clamp at its lowest / highest threshold"""
    out, worst = [], 0.0
    cases = [("+12 V into CLK, CLK low", 12, None, LOW), ("+12 V into CLK, CLK high", 12, None, HIGH),
             ("+12 V into BOTH, both low", 12, 12, LOW), ("+12 V into BOTH, both high", 12, 12, HIGH),
             ("-12 V into CLK, CLK high", -12, None, HIGH), ("-12 V into BOTH, both high", -12, -12, HIGH),
             ("-12 V into BOTH, both low", -12, -12, LOW)]
    for vth in (4.95, 5.05):
        for name, vc, vr, gate in cases:
            lr = f"Vxr or 0 {vr}" if vr is not None else load("r", "100k")
            net = netlist(4.966, vth, gate["c"], gate["r"], f"Vxc oc 0 {vc}", lr)
            m = run(net, "tran 1u 3m 2m\nmeas tran r avg v(v5a) from=2.5m to=3m\nmeas tran ic avg i(Vmc) from=2.5m to=3m\n"
                    "meas tran icl avg i(Vcl) from=2.5m to=3m\nmeas tran vg avg v(pgc) from=2.5m to=3m\n"
                    f"let pk = {vth}\n" + echo("r", "ic", "icl", "vg"))
            icl = max(0.0, (m["r"] - vth) / 10.2)
            worst = max(worst, m["r"])
            p_each = (m["ic"] / 2) ** 2 * 2000
            out.append(f"clamp at {vth} V, {name}: V5A {m['r']:.3f} V; CLK's current {abs(m['ic']) * 1000:.2f} mA "
                       f"({p_each * 1000:.0f} mW in each 2 k of {P1206 * 1000:.0f}); U19 sinks {icl * 1000:.1f} mA "
                       f"({icl * m['r'] * 1000:.0f} mW of its 300); Q4's VGS {m['vg'] - m['r']:.2f} V")
    return out, worst


def main():
    out = ["jank mini 2.2, the CLOCK / RESET outputs from the 5 V aux rail: ngspice results (cv.py; models in its docstring)", ""]
    lv, res = levels()
    e = edges()
    f, worst = faults()
    bl = blip()
    for name, lines in (("levels", lv), ("edges", e), ("switch-on blip (C89 / C90 2.2 nF)", bl), ("a wrong voltage into the outputs (sustained), the rail unloaded", f)):
        out.append(name + ":")
        out += ["  " + x for x in lines]
        out.append("")
    out.append(f"SUMMARY: into 10 k {res[('typ', '10k')]:.2f} V typ / {res[('low (boost -2.5 %)', '10k')]:.2f} V low; into "
               f"100 k {res[('typ', '100k')]:.2f} / {res[('low (boost -2.5 %)', '100k')]:.2f} V; V5A in the worst fault "
               f"{worst:.3f} V (the WS2812B-2020's 5.3 V, the TPS61023's 7 V)")
    open(os.path.join(HERE, "cv.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
