# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's LINE IN front end in ngspice (linein.lib): gain, band, bias, the swing at +4 dBu and at consumer
level, clipping, a mono plug, the stage-2 pull-downs (the level with and without them, by source impedance), the plug
detect (LIN_DET with and without a plug), the supply's ripple (U7 on VSYS, Wi-Fi bursts) at the ADC, USB plug-in with a
signal present, and the ADC's sampling-capacitor kick. Writes sim/results.txt.

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
FE = "Vslv slv 0 0\nX1 tip ring ain avdd 0 slv FRONTEND\n"


def fe(pd=1):
    return f"Vslv slv 0 0\nX1 tip ring ain avdd 0 slv FRONTEND rpd={100e3 if pd else 1e12:g}\n"


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


def ground_loop(cb=0):
    """stage 2: 100 mV (rms) between the source's ground (the plug's sleeve) and the board's GND, no signal: what reaches
    the ADC pin with the ground-sensing input (gs=1: the sleeve through R39 22 ohm, sensed by R65 / R66) and with stage
    1f's (gs=0: the sleeve on GND, the source's ground offset by the loop's voltage). AC analysis, both channels driven
    from the source's ground through 600 ohm"""
    out = {}
    for gs in (1, 0):
        slv = "sref" if gs else "0"
        body = ("Vgl sref 0 DC 0 AC 0.1\nRst sref tip 600\nRsr sref ring 600\n" + AVDD +
                f"X1 tip ring ain avdd 0 {slv} FRONTEND gs={gs} cb={cb if gs and cb else 1e-18:g}\n"
                ".control\nac dec 50 10 100k\nlet g = mag(v(ain))\n"
                "meas ac g50 find g at=50\nmeas ac g1k find g at=1000\nmeas ac g10k find g at=10000\n"
                "echo \"g50 = $&g50\"\necho \"g1k = $&g1k\"\necho \"g10k = $&g10k\"\n.endc\n")
        out[gs] = run(body, f"ground loop {gs}")
    return out


def level(rs, pd):
    """the 1 kHz level at the pin (rms over 20 ms) from a 0 dBu source of rs ohms on both channels, pull-downs in or out"""
    body = (src(0, rs) + AVDD + fe(pd) + ".control\ntran 5u 60m 20m\nlet ac = v(ain) - 1.4667\n"
            "meas tran vr rms ac from=20m to=60m\necho \"vr = $&vr\"\n.endc\n")
    return run(body, f"level {rs} {pd}")["vr"]


def detect():
    """LIN_DET (GPIO16, R38 1 M to +3V3): no plug = the ring switch closed onto the ring (R37 100 k to GND, D8); a plug
    with a line source on the ring (the switch open); the GPIO's input leakage ASSUMED 50 nA (Espressif gives none)"""
    out = {}
    for name, closed in (("open", True), ("plugged", False)):
        sw = "Rsw det ring 0.03\n" if closed else "Rsw det ring 1e12\nVr sr 0 SIN(0 1.55 1000)\nRsr sr ring 600\n"
        body = ("V33 v33 0 3.3\nR38 v33 det 1Meg\nIleak det 0 50n\nVt tip 0 0\n" + sw + AVDD + fe(1) +
                ".control\ntran 10u 40m 20m\nmeas tran dmax max v(det) from=20m\nmeas tran dmin min v(det) from=20m\n"
                "echo \"dmax = $&dmax\"\necho \"dmin = $&dmin\"\n.endc\n")
        out[name] = run(body, f"detect {name}")
    return out


def ripple(vsys, dip, period, width):
    """U7 on VSYS: VSYS dips by `dip` volts (the Wi-Fi bursts through the USB cable and D3, or the cell's resistance)
    for `width` every `period`; the input silent (tip and ring grounded through 600 ohm): the pin's ripple"""
    body = (f"Vsys vsys 0 PULSE({vsys} {vsys - dip} 5m 20u 20u {width} {period})\nXU7 vsys avdd 0 LDO\n"
            "Rst tip 0 600\nRsr ring 0 600\n" + fe(1) +
            ".control\ntran 5u 200m 100m\nmeas tran amax max v(ain) from=100m\nmeas tran amin min v(ain) from=100m\n"
            "meas tran vb avg v(avdd) from=100m\necho \"amax = $&amax\"\necho \"amin = $&amin\"\necho \"vb = $&vb\"\n.endc\n")
    return run(body, f"ripple {vsys} {dip}")


def plug_in():
    """USB plugged at t = 0 with +4 dBu already on the jack: VSYS (VBUS through D3) up in 50 us, AVDD (U7) and +3V3 (the
    3.3 V LDO, ~22 uF + the chip, ASSUMED 1 ms) follow; the pin must not sit above +3V3 by a diode drop"""
    body = (src(4) + "Vsys vsys 0 PWL(0 0 50u 4.6)\nXU7 vsys avdd 0 LDO\n"
            "B3v3 v3v3 0 V = min(3.3 * time / 1m, 3.3)\n" + FE +
            ".control\ntran 10u 200m uic\nlet over = v(ain) - v(v3v3)\nmeas tran omax max over\n"
            "echo \"omax = $&omax\"\n.endc\n")
    return run(body, "plug-in")


def kick():
    """the ADC's sampling capacitor (ASSUMED 10 pF, reset to 0 V before each sample) switched onto the pin for 1 us every
    44.1 kHz period, with a +4 dBu 5 kHz tone: the sampled value against the same front end with no ADC load"""
    t = 1 / 44100
    body = (src(4, f=5000) + AVDD + FE + "X2 tip ring ain2 avdd 0 slv FRONTEND\n"
            f"Vclk clk 0 PULSE(0 1 0 1n 1n 1u {t})\nVrst rst 0 PULSE(0 1 1.5u 1n 1n {t - 2.2e-6} {t})\n"
            "S1 ain cs clk 0 SW1\nS2 cs 0 rst 0 SW1\nCs cs 0 10p\n.model SW1 SW(VT=0.5 RON=200 ROFF=1G)\n"
            f".control\ntran 20n 3m 2m\nlet e = v(cs) - v(ain2)\nlet ae = abs(e)\n"
            "meas tran emax max ae when clk=0.5 fall=1\n"
            + "".join(f"meas tran e{k} find e when clk=0.5 fall={k}\n" for k in range(1, 41)) +
            "".join(f"echo \"e{k} = $&e{k}\"\n" for k in range(1, 41)) + ".endc\n")
    v = run(body, "kick")
    return max(abs(v[f"e{k}"]) for k in range(1, 41) if f"e{k}" in v)


def main():
    lines = ["jank mini 2.2, LINE IN front end: ngspice results (linein.py; generic models, linein.lib)", ""]
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
    lines += ["", "the 100 k pull-downs (R36 / R37), 0 dBu 1 kHz on both channels: the level at the pin with them / without"]
    for rs in (100, 600, 1000, 10000):
        a, b = level(rs, 1), level(rs, 0)
        lines.append(f"  {rs:>5} ohm source: {a * 1000:.1f} / {b * 1000:.1f} mV rms: {20 * math.log10(a / b):+.3f} dB")
    gl = ground_loop()
    lines += ["", "a ground loop: 100 mV rms between the plug's sleeve and the board's GND, no signal: at the ADC pin "
              "(stage 2's ground-sensing input / stage 1f's, the sleeve on GND)"]
    for f in ("50", "1k", "10k"):
        a, b = gl[1]["g" + f], gl[0]["g" + f]
        lines.append(f"  {f:>3} Hz: {a * 1000:.2f} / {b * 1000:.2f} mV rms: {20 * math.log10(b / a):.1f} dB rejected")
    gl2 = ground_loop(2.2e-9)
    lines.append("  with 2.2 nF on C67 (a DNP pad across R66, matching C32): " + ", ".join(
        f"{f} Hz {20 * math.log10(gl2[0]['g' + f] / gl2[1]['g' + f]):.1f} dB" for f in ("50", "1k", "10k")))
    d = detect()
    lines += ["", f"plug detect (LIN_DET, R38 1 M, 50 nA leakage ASSUMED): no plug {d['open']['dmin']:.3f} .. "
              f"{d['open']['dmax']:.3f} V (low: the S3's VIL is 0.25 x 3.3 = 0.825 V); a plug with +4 dBu on the ring "
              f"{d['plugged']['dmin']:.3f} .. {d['plugged']['dmax']:.3f} V (high: VIH 0.75 x 3.3 = 2.475 V)"]
    lines += ["", "the supply (U7, PSRR 50 dB ASSUMED) under Wi-Fi bursts, the input silent: the pin's ripple"]
    for vsys, dip, per, wid, what in ((4.6, 0.15, 10e-3, 2e-3, "USB: VSYS 4.6 V, 150 mV dips 2 ms every 10 ms"),
                                      (4.6, 0.15, 1e-3, 0.3e-3, "USB: 150 mV dips 0.3 ms every 1 ms"),
                                      (3.7, 0.08, 10e-3, 2e-3, "cell: VSYS 3.7 V, 80 mV dips 2 ms every 10 ms"),
                                      (3.4, 0.08, 10e-3, 2e-3, "cell low: VSYS 3.4 V (AVDD 3.2 V), 80 mV dips")):
        r = ripple(vsys, dip, per, wid)
        pp = r["amax"] - r["amin"]
        lines.append(f"  {what}: {pp * 1e6:.1f} uV pp = {pp / LSB:.3f} LSB; AVDD {r['vb']:.3f} V")
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
