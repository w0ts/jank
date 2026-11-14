# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's CLOCK / RESET outputs with their charge pump (parts.py "_cv", "PUMP"; README "CLOCK / RESET"), in
ngspice. Writes sim/cv.txt. The user's ruling: a real 5 V+ output or none; PASS = 5.0 V or more into 10 k from a 3.4 V
cell, a rise under 10 us, 0 V while the pump is off.

The circuit. The pump: GPIO35 (PUMP_IO) drives a square wave (LEDC, 416,667 Hz = 80 MHz / 2 / 96) through R83 22 ohm into C84
1 uF (the flying cap); D12 (B5819W) from VREG to the flying node, D13 (B5819W) from it to CV_RAIL; C85 10 uF 25 V on
the rail, R84 100 k bleed. Per output: the GPIO (CLK_IO / RST_IO) on Q3 2N7002's gate (R71 100 k to GND), its drain
pulls Q4 AO3401A's gate (R73 10 k up to CV_RAIL: off), Q4's source on CV_RAIL, its drain the node N (R74 10 k to GND;
D10 BAV99: GND -> N, N -> CV_RAIL), two 2 k 1206 in parallel (1 k, 0.5 W) from N to the jack's tip. The values were
chosen with this script (CV_RS, CV_RB, CV_RPU, CV_RPD in the environment try others): without R74 the switch-on put
0.77 V on the outputs (the P-FET's drain capacitance); R83 33 ohm / R84 10 k left 4.93 V into 10 k from a 3.4 V cell.

Models (each one's source; none is the part's maker's own model unless said):
 - Q3: a 2N7002 VDMOS with the parameters of LTspice's standard.mos entry (Fairchild): Vto 1.6 V, Kp 0.17, Rs 0.75 ohm,
   Rg 3 ohm, Cgs 50 pF, Cgd 12..80 pF (copied by hand: check against the file before relying on edge times).
 - Q4: a GENERIC p-channel VDMOS, ASSUMED: Vto -0.9 V, Kp 12, Rd 20 mohm, Cgs 600 pF, Cgd 60..180 pF (chosen for the
   AO3401A's typical threshold and ~50 mohm at -4.5 V; not its maker's model).
 - D12 / D13 (B5819W): a 1N5819-class Schottky, IS 31.7 uA, N 1.373, RS 51 mohm, CJO 110 pF (the common 1N5819 SPICE
   card), ASSUMED for the SOD-123 part.
 - D10 / D11 (BAV99): a 1N4148-class small-signal diode, ASSUMED close to the BAV99.
 - The GPIOs: high = 3.3 V through 17 ohm, low = 0 V through 12 ohm (midi_faults.py's fit to the datasheet's IOH / IOL,
   drive strength 3); floating (boot, reset) = the datasheet's IIH 50 nA pushed into the pin's net (Table 5-4); no pull
   at reset on GPIO35 / 2 / 1 (the datasheet's pin table, p. 16).
 - VREG: an ideal source through 0.1 ohm: 4.6 V (USB: power.txt), 3.4 V (a nearly flat cell). The load: 10 k or
   100 k, with 100 pF (a patch cable and a module's input, ASSUMED).
  python3 hardware/mini2/sim/cv.py        (ngspice 47; a few minutes)
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
F_PUMP = 416667.0                # (LEDC 80 MHz / 2 / 96: 10 x the ADC rate per channel, TIMING.md)
P1206 = 0.25                     # each 2 k 1206: 1/4 W
RPU = float(os.environ.get("CV_RPU", "10e3"))     # R73 / R79, the P-FET's gate pull-up to CV_RAIL (the board: 10 k)
RPD = float(os.environ.get("CV_RPD", "10e3"))      # R74 / R80, the node's pull-down (the board: 10 k; 0: none)
RS = float(os.environ.get("CV_RS", "22"))          # R83, the pump pin's series resistor (the board: 22 ohm)
RB = float(os.environ.get("CV_RB", "100e3"))       # R84, the rail's bleed (the board: 100 k)
MODELS = """
.model QN VDMOS(Rg=3 Vto=1.6 Rd=0 Rs=.75 Rb=.14 Kp=.17 mtriode=1.25 Cgdmax=80p Cgdmin=12p Cgs=50p Cjo=50p Is=.04p)
.model QP VDMOS(pchan Rg=6 Vto=-0.9 Rd=.02 Rs=.01 Kp=12 Cgdmax=180p Cgdmin=60p Cgs=600p Cjo=150p Is=1e-12)
.model DS D(IS=2.52n RS=.568 N=1.752 CJO=4p M=.4 TT=20n BV=100)
.model DSK D(IS=31.7u RS=.051 N=1.373 CJO=110p M=.35 VJ=.3 BV=40)
"""


def stage(k, g):
    return f"""R71{k} {g} 0 100k
M3{k} pg{k} {g} 0 0 QN
R73{k} rail pg{k} {RPU:g}
M4{k} n{k} pg{k} rail rail QP
Dlo{k} 0 n{k} DS
Dhi{k} n{k} rail DS
R75{k} n{k} o{k}x 1k
Vm{k} o{k}x o{k} 0""" + (f"\nR74{k} n{k} 0 {RPD:g}" if RPD else "")


def netlist(vreg, pump, gate_clk, gate_rst, load_clk, load_rst):
    """vreg / pump / gate_*: element lines on nodes vreg, pio, gc, gr; load_*: on nodes oc, or"""
    return f"""* cv
{MODELS}
{vreg}
{pump}
R83 pio p {RS:g}
C84 p x 1u
D12 vreg x DSK
D13 x rail DSK
C85 rail 0 10u
R84 rail 0 {RB:g}
{gate_clk}
{gate_rst}
{stage("c", "gc")}
{stage("r", "gr")}
{load_clk}
{load_rst}
"""


def run(net, ctl, timeout=900):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(net + f".control\n{ctl}\n.endc\n.end\n")
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=timeout).stdout
    return {m[1]: float(m[2]) for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M)}


def echo(*names):
    return "\n".join(f'echo "{n} = $&{n}"' for n in names)


def vreg(v):
    return f"Vreg vs 0 {v}\nRreg vs vreg 0.1"


HALF = 0.5 / F_PUMP
PUMP_ON = f"Vp ps 0 PULSE(0 3.3 0 5n 5n {HALF - 5e-9} {1 / F_PUMP})\nRgp ps pio 17"
PUMP_OFF = "Vp ps 0 0\nRgp ps pio 12"
HIGH = {"c": "Vgc gcs 0 3.3\nRgc gcs gc 17", "r": "Vgr grs 0 3.3\nRgr grs gr 17"}
LOW = {"c": "Vgc gcs 0 0\nRgc gcs gc 12", "r": "Vgr grs 0 0\nRgr grs gr 12"}
FLOAT = {"c": "Igc 0 gc 50n", "r": "Igr 0 gr 50n"}


def load(k, r):
    return f"Rl{k} o{k} 0 {r}\nCl{k} o{k} 0 100p"


def steady():
    """both outputs high into 10 k or 100 k, the pump running: the rail (mean, ripple), the outputs, the pump pin's peak"""
    out, res = [], {}
    for v in (4.6, 3.4):
        for rl in ("10k", "100k"):
            net = netlist(vreg(v), PUMP_ON, HIGH["c"], HIGH["r"], load("c", rl), load("r", rl))
            m = run(net, "tran 50n 6m 5m\n"
                    "meas tran rmean avg v(rail) from=5.5m to=6m\nmeas tran rmax max v(rail) from=5.5m to=6m\n"
                    "meas tran rmin min v(rail) from=5.5m to=6m\nmeas tran oc avg v(oc) from=5.5m to=6m\n"
                    "meas tran ipk max i(Vp) from=5.5m to=6m\nmeas tran imin min i(Vp) from=5.5m to=6m\n"
                    + echo("rmean", "rmax", "rmin", "oc", "ipk", "imin"))
            ipk = max(abs(m["ipk"]), abs(m["imin"])) * 1000
            res[(v, rl)] = m["oc"]
            out.append(f"VREG {v:.1f} V, both outputs high into {rl}: CV_RAIL {m['rmean']:.2f} V (ripple "
                       f"{(m['rmax'] - m['rmin']) * 1000:.0f} mV pp), the outputs {m['oc']:.2f} V; the pump pin's peak "
                       f"{ipk:.0f} mA (steady)")
    return out, res


def edges():
    out, res = [], {}
    for v in (4.6, 3.4):
        g = "Vgc gcs 0 PULSE(0 3.3 5m 2n 2n 100u 200u)\nRgc gcs gc 15"
        net = netlist(vreg(v), PUMP_ON, g, LOW["r"], load("c", "10k"), load("r", "10k"))
        m = run(net, "tran 20n 5.3m 4.95m\nmeas tran hi avg v(oc) from=5.06m to=5.09m\n" + echo("hi"))
        hi = m["hi"]
        m = run(net, f"tran 20n 5.3m 4.95m\nmeas tran tr trig v(oc) val={0.1 * hi} rise=1 targ v(oc) val={0.9 * hi} rise=1\n"
                f"meas tran tf trig v(oc) val={0.9 * hi} fall=1 targ v(oc) val={0.1 * hi} fall=1\n"
                f"meas tran td trig v(gc) val=1.65 rise=1 targ v(oc) val={0.5 * hi} rise=1\n" + echo("tr", "tf", "td"))
        res[v] = m["tr"]
        out.append(f"VREG {v:.1f} V, a 100 us pulse into 10 k // 100 pF: high {hi:.2f} V, rise (10-90 %) "
                   f"{m['tr'] * 1e9:.0f} ns, fall {m['tf'] * 1e6:.2f} us, {m['td'] * 1e9:.0f} ns after the GPIO")
    return out, res


def startup():
    """the toggle switched on (VREG 0 -> v at 1 ms) with the GPIOs floating (boot), the pump started at 20 ms: the
    outputs (any glitch), the rail, the pump pin's peak current at the pump's start"""
    out, worst = [], 0.0
    for v in (4.6, 3.4):
        vr = f"Vreg vs 0 PWL(0 0 1m 0 1.1m {v})\nRreg vs vreg 0.1"
        pump = f"Vp ps 0 PULSE(0 3.3 20m 5n 5n {HALF - 5e-9} {1 / F_PUMP})\nRgp ps pio 17"
        net = netlist(vr, pump, FLOAT["c"], FLOAT["r"], load("c", "100k"), load("r", "100k"))
        m = run(net, "tran 100n 28m\nmeas tran omax max v(oc) from=0 to=28m\nmeas tran r19 avg v(rail) from=18m to=19.9m\n"
                "meas tran r28 avg v(rail) from=27m to=28m\nmeas tran ipk max i(Vp) from=20m to=21m\n"
                "meas tran imin min i(Vp) from=20m to=21m\n" + echo("omax", "r19", "r28", "ipk", "imin"))
        ipk = max(abs(m["ipk"]), abs(m["imin"])) * 1000
        worst = max(worst, m["omax"])
        out.append(f"VREG 0 -> {v:.1f} V at 1 ms, the GPIOs floating, the pump started at 20 ms: the outputs never above "
                   f"{m['omax'] * 1000:.2f} mV; CV_RAIL {m['r19']:.2f} V with the pump off (VREG through D12 / D13), "
                   f"{m['r28']:.2f} V with it running; the pump pin's peak at its start {ipk:.0f} mA")
    return out, worst


def pump_off():
    out, worst = [], 0.0
    for v in (4.6, 3.4):
        for gates, what in ((FLOAT, "the GPIOs floating"), (LOW, "the GPIOs low")):
            net = netlist(vreg(v), PUMP_OFF, gates["c"], gates["r"], load("c", "100k"), load("r", "100k"))
            m = run(net, "op\nlet o = v(oc)\nlet r = v(rail)\n" + echo("o", "r"))
            worst = max(worst, abs(m["o"]))
            out.append(f"the pump off, VREG {v:.1f} V, {what}: the outputs {m['o'] * 1e6:.1f} uV (100 k), CV_RAIL {m['r']:.2f} V")
    net = netlist(vreg(4.6), PUMP_OFF, HIGH["c"], LOW["r"], load("c", "10k"), load("r", "10k"))
    m = run(net, "op\nlet o = v(oc)\n" + echo("o"))
    out.append(f"the pump off but CLK driven high (a firmware error): CLK {m['o']:.2f} V into 10 k (CV_RAIL = VREG through "
               "the diodes): the firmware keeps the outputs low until the pump runs")
    return out, worst


def faults():
    out = []
    for v in (4.6, 3.4):
        for vx, gate in ((12.0, LOW), (12.0, HIGH), (-12.0, LOW), (-12.0, HIGH)):
            net = netlist(vreg(v), PUMP_ON, gate["c"], LOW["r"], f"Vx oc 0 {vx}", load("r", "100k"))
            m = run(net, "tran 50n 4m 3m\nmeas tran i avg i(Vmc) from=3.5m to=4m\nmeas tran r avg v(rail) from=3.5m to=4m\n"
                    "meas tran vg avg v(pgc) from=3.5m to=4m\n" + echo("i", "r", "vg"))
            p_each = (m["i"] / 2) ** 2 * 2000             # (each 2 k of the pair carries half the current)
            out.append(f"{vx:+.0f} V into CLK, VREG {v:.1f} V, CLK {'high' if gate is HIGH else 'low'}: {abs(m['i']) * 1000:.2f} mA, "
                       f"{p_each * 1000:.0f} mW in each 2 k (of {P1206 * 1000:.0f}); CV_RAIL {m['r']:.2f} V; Q4's VGS "
                       f"{m['vg'] - m['r']:.2f} V (AO3401A: +-12 V)")
    net = netlist(vreg(4.6), PUMP_ON, HIGH["c"], HIGH["r"], "Vxc oc 0 12", "Vxr or 0 12")
    m = run(net, "tran 50n 4m 3m\nmeas tran r avg v(rail) from=3.5m to=4m\nmeas tran vg avg v(pgc) from=3.5m to=4m\n"
            + echo("r", "vg"))
    out.append(f"+12 V into BOTH outputs, both high, VREG 4.6 V: CV_RAIL {m['r']:.2f} V, Q4's VGS {m['vg'] - m['r']:.2f} V "
               "(C85 25 V; AO3401A 30 V / 12 V; D13 40 V)")
    return out


def main():
    out = ["jank mini 2.2, the CLOCK / RESET outputs and their charge pump: ngspice results (cv.py; the models in its "
           f"docstring); the pump at {F_PUMP / 1e3:g} kHz", ""]
    s, lv = steady()
    e, tr = edges()
    u, glitch = startup()
    o, off = pump_off()
    f = faults()
    for name, lines in (("steady (the pump running)", s), ("edges", e), ("switch-on and boot", u), ("the pump off", o),
                        ("a wrong voltage into the output (sustained)", f)):
        out.append(name + ":")
        out += ["  " + line for line in lines]
        out.append("")
    ok = lv[(3.4, "10k")] >= 5.0 and max(tr.values()) < 10e-6 and off < 0.01
    out.append(f"PASS (>= 5.0 V into 10 k from a 3.4 V cell: {lv[(3.4, '10k')]:.2f} V; rise < 10 us: "
               f"{max(tr.values()) * 1e6:.2f} us; 0 V while the pump is off: {off * 1000:.2f} mV at most): "
               f"{'YES' if ok else 'NO'}; the switch-on blip (the GPIOs floating): {glitch * 1000:.0f} mV at most")
    open(os.path.join(HERE, "cv.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
