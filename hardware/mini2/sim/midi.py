# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's four MIDI outputs as true 5 V current loops (2026-11-15, the user's; parts.py "MIDI OUT 1 and MIDI
OUT 2"), in ngspice. Writes sim/midi.txt.

The circuit: V5M (the 5 V aux rail through R122) -> the source (2 x 470 ohm 0805 in parallel, 235 ohm, the jacks and
the DINs alike since the Standard study's experiment A, 2026-11-15; the jacks were 3 x 680) -> ring / DIN 4 -> the
receiver (MIDI 1.0: 220 ohm, the optocoupler's LED, a reverse diode; some add a series diode) -> tip / DIN 5 -> the sink
(2 x 470 ohm 0805 in parallel, 235 ohm; was one 220) -> a 2N7002 (gate on the inverted UART TX at 3.3 V) -> GND. Models (GENERIC, not the vendors'):
 - the receiver's LED: a diode fitted to 1.2 V at 5 mA (6N138 / H11L1 / PC900 class), 1.5 V and 1.7 V (slow or old
   optos); its 220 ohm +-5 %; "+ diode": a 1N4148 in series.
 - the 2N7002: LTspice's standard.mos entry (Fairchild), copied by hand, at VGS 3.3 V (the GPIO high, 17 ohm).
 - V5M: 4.72 / 4.85 / 4.97 V (the TPS61023's 4.85 V with R136 20 k, its VREF's +-2.5 %); the resistors +-1 %.
  python3 hardware/mini2/sim/midi.py        (ngspice; seconds)
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
LEDS = {"1.2 V": "D(IS=3.1e-14 N=1.8 RS=2)", "1.5 V": "D(IS=1.3e-15 N=2.0 RS=2)", "1.7 V": "D(IS=2.6e-17 N=2.0 RS=2)"}
QN = ".model QN VDMOS(Rg=3 Vto=1.6 Rd=0 Rs=.75 Rb=.14 Kp=.17 mtriode=1.25 Cgdmax=80p Cgdmin=12p Cgs=50p Cjo=50p Is=.04p)"
SRC = {"jack": 470 / 2, "DIN": 470 / 2}
SINK = 470 / 2


def run(net):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(net)
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=120).stdout
    return {m[1]: float(m[2]) for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M)}


def loop(v5, rsrc, led, rrx=220.0, rsink=SINK, series=False, gate=3.3):
    d = "Dser rx2 rx3 D1N\n" if series else "Rser rx2 rx3 0.001\n"
    net = (f"* midi\n{QN}\n.model DL {LEDS[led]}\n.model D1N D(IS=2.5n N=1.75 RS=0.6)\n"
           f"V5 v5 0 {v5}\nRs v5 ring {rsrc}\nRrx ring rx1 {rrx}\nDled rx1 rx2 DL\nDrev rx2 rx1 D1N\n{d}"
           f"Vm rx3 tip 0\nRk tip k {rsink}\nM1 k g 0 0 QN\nVg gs 0 {gate}\nRg gs g 17\n"
           ".control\nop\nlet i = i(Vm)\nlet vk = v(k)\necho \"i = $&i\"\necho \"vk = $&vk\"\n.endc\n.end\n")
    return run(net)


def main():
    out = ["jank mini 2.2, the four MIDI outputs as 5 V current loops: ngspice results (midi.py; generic models: its docstring)", ""]
    lo, hi = 1e9, 0.0
    out.append("the receiver's current with the FET on (a 0 bit), per connector:")
    for v5, tol in ((4.72, -1), (4.85, 0), (4.97, 1)):
        for led in LEDS:
            for series in (False, True):
                row = []
                for conn, rs in SRC.items():
                    k = 1 + 0.01 * tol
                    m = loop(v5, rs / k, led, rrx=220 * (1 - 0.05 * tol), rsink=SINK / k, series=series)
                    i = abs(m["i"]) * 1000
                    lo, hi = min(lo, i), max(hi, i)
                    row.append(f"{conn} {i:.2f} mA")
                out.append(f"  V5M {v5} V{' (resistors -1 %, receiver +5 %)' if tol < 0 else ' (resistors +1 %, receiver -5 %)' if tol > 0 else ''}, "
                           f"LED {led}{' + a series diode' if series else ''}: " + ", ".join(row))
    m = loop(4.97, SRC["jack"], "1.2 V", gate=0.0)
    out.append(f"  the FET off (idle, a 1 bit): {abs(m['i']) * 1e6:.2f} uA")
    out.append("")
    out.append("faults (V5M 4.97 V, the resistors -1 %):")
    for conn, rs in SRC.items():
        i = 4.97 / (rs * 0.99)
        out.append(f"  {conn}: ring / DIN 4 shorted to GND (a mono plug, a broken cable): {i * 1000:.1f} mA, "
                   f"{i * i * rs * 0.99 / 2 * 1000:.0f} mW in each of its 2 0805s (125 mW parts)")
    i = 4.97 / (SRC["jack"] * 0.99 + SINK * 0.99)
    out.append(f"  jack: tip shorted to ring with the FET on: {i * 1000:.1f} mA ({i * i * SINK * 0.99 / 2 * 1000:.0f} mW in "
               "each of the sink's 2 0805s)")
    out.append("  tip shorted to GND: no current (the sink side is already at GND when the FET is on; off, nothing drives it)")
    out.append("")
    out.append(f"SUMMARY: a receiver gets {lo:.2f} .. {hi:.2f} mA over the corners (MIDI 1.0's 5 V circuit aims at ~5 mA); "
               "the GPIOs drive only the FETs' gates")
    open(os.path.join(HERE, "midi.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
