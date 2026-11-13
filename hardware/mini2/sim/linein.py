# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2's LINE IN front end in ngspice (linein.lib): gain, band, bias, the swing at +4 dBu, clipping, a mono plug,
USB plug-in with a signal present, and the ADC's sampling-capacitor kick. Writes sim/results.txt.

  python3 hardware/mini2/sim/linein.py        (ngspice 47 on the PATH; a few seconds)
"""
import math
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DBU0 = math.sqrt(0.6)                      # 0 dBu = 0.7746 V rms
LSB = 2.9 / 4096                           # ATTEN3 (12 dB), the datasheet's 0..2900 mV, 12 bits


def peak(dbu):
    return DBU0 * 10 ** (dbu / 20) * math.sqrt(2)


def run(body, name):
    """one netlist: body holds sources, analyses and a .control block that prints `name_key = value` lines"""
    net = f"* {name}\n.include {os.path.join(HERE, 'linein.lib')}\n{body}\n.end\n"
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(net)
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=600).stdout
    vals = {}
    for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M):
        vals[m[1]] = float(m[2])
    if not vals:
        raise SystemExit(f"{name}: no results\n{out[-2000:]}")
    return vals


def src(dbu, rs=600, ring=True, f=1000):
    a = peak(dbu)
    r = f"Vr sr 0 SIN(0 {a} {f})\nRsr sr ring {rs}" if ring else "Rring ring 0 0.01"
    return f"Vt st 0 SIN(0 {a} {f})\nRst st tip {rs}\n{r}\n"


AVDD = "Vavdd avdd 0 3.3\n"
FE = "X1 tip ring ain avdd 0 FRONTEND\n"


def ac():
    body = ("Vt st 0 DC 0 AC 1\nRst st tip 100\nRsr st ring 100\n" + AVDD + FE +
            ".control\nac dec 200 1 100k\nlet g = vdb(ain)\n"
            "meas ac g1k find g at=1000\nmeas ac g22k find g at=22050\nmeas ac g40k find g at=40000\n"
            "let ref = g1k - 3\nmeas ac fhi when g=ref fall=last\nmeas ac flo when g=ref rise=1\n"
            "echo \"g1k = $&g1k\"\necho \"g22k = $&g22k\"\necho \"g40k = $&g40k\"\n"
            "echo \"fhi = $&fhi\"\necho \"flo = $&flo\"\n.endc\n")
    return run(body, "ac")


def swing(dbu, ring=True, rs=600):
    body = (src(dbu, rs, ring) + AVDD + FE + ".control\ntran 5u 60m 20m\n"
            "meas tran vmax max v(ain)\nmeas tran vmin min v(ain)\nmeas tran vb avg v(x1.vb)\n"
            "echo \"vmax = $&vmax\"\necho \"vmin = $&vmin\"\necho \"vb = $&vb\"\n.endc\n")
    return run(body, f"swing {dbu}")


def plug_in():
    """USB plugged at t = 0 with +4 dBu already on the jack: AVDD (the LDO from VBUS) is up in ~0.1 ms; +3V3 follows the
    bank charging through R5 (4.4 mF x 4.7 ohm, README "Schematic"); the pin must not sit above +3V3 by a diode drop"""
    body = (src(4) + "Vbus vbus 0 PWL(0 0 50u 5)\nBavdd avdd 0 V = min(V(vbus), 3.3)\n"
            "B3v3 v3v3 0 V = min(4.65 * (1 - exp(-time / 20.7m)), 3.3)\n" + FE +
            ".control\ntran 10u 200m uic\nlet over = v(ain) - v(v3v3)\nmeas tran omax max over\n"
            "echo \"omax = $&omax\"\n.endc\n")
    return run(body, "plug-in")


def kick():
    """the ADC's sampling capacitor (ASSUMED 10 pF, reset to 0 V before each sample) switched onto the pin for 1 us every
    44.1 kHz period, with a +4 dBu 5 kHz tone: the sampled value against the same front end with no ADC load"""
    t = 1 / 44100
    body = (src(4, f=5000) + AVDD + FE + "X2 tip ring ain2 avdd 0 FRONTEND\n"
            f"Vclk clk 0 PULSE(0 1 0 1n 1n 1u {t})\nVrst rst 0 PULSE(0 1 1.5u 1n 1n {t - 2.2e-6} {t})\n"
            "S1 ain cs clk 0 SW1\nS2 cs 0 rst 0 SW1\nCs cs 0 10p\n.model SW1 SW(VT=0.5 RON=200 ROFF=1G)\n"
            f".control\ntran 20n 3m 2m\nlet e = v(cs) - v(ain2)\nlet ae = abs(e)\n"
            "meas tran emax max ae when clk=0.5 fall=1\n"
            + "".join(f"meas tran e{k} find e when clk=0.5 fall={k}\n" for k in range(1, 41)) +
            "".join(f"echo \"e{k} = $&e{k}\"\n" for k in range(1, 41)) + ".endc\n")
    v = run(body, "kick")
    return max(abs(v[f"e{k}"]) for k in range(1, 41) if f"e{k}" in v)


def main():
    lines = ["jank mini 2, LINE IN front end: ngspice results (linein.py; generic op-amp model, linein.lib)", ""]
    a = ac()
    lines += [f"mono gain at 1 kHz (both channels the same signal, 100 ohm source): {a['g1k']:.2f} dB "
              f"({10 ** (a['g1k'] / 20):.3f} x)",
              f"band (-3 dB from 1 kHz): {a['flo']:.1f} Hz .. {a['fhi'] / 1000:.2f} kHz",
              f"at 22.05 kHz (Nyquist of 44.1 kHz): {a['g22k'] - a['g1k']:.1f} dB; at 40 kHz: {a['g40k'] - a['g1k']:.1f} dB", ""]
    for dbu, ring, rs in ((4, True, 600), (4, True, 100), (4, False, 600), (-10 + 2.2, True, 600), (10, True, 600),
                          (20, True, 600)):
        s = swing(dbu, ring, rs)
        what = "mono plug (ring grounded)" if not ring else "stereo, same signal both channels"
        lines.append(f"{dbu:+5.1f} dBu, {what}, {rs} ohm source: pin {s['vmin']:.3f} .. {s['vmax']:.3f} V "
                     f"({s['vmax'] - s['vmin']:.3f} Vpp), bias {s['vb']:.3f} V")
    lines.append("  (-7.8 dBu = -10 dBV, consumer line level)")
    p = plug_in()
    lines += ["", f"USB plug-in with +4 dBu on the jack: the pin at most {p['omax']:.3f} V above +3V3 "
              "(a diode drop, ~0.5 V, would inject current)"]
    k = kick()
    lines += [f"ADC sampling capacitor (10 pF ASSUMED, reset to 0 V each sample), +4 dBu 5 kHz: sampled value off by at "
              f"most {k * 1000:.3f} mV = {k / LSB:.2f} LSB (ATTEN3, 0.71 mV)", ""]
    open(os.path.join(HERE, "results.txt"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
