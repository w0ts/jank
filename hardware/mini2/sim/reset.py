# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's reset: CHIP_PU (R8 10 k from +3V3, C7 1 uF to GND; no RST button since stage 1f) against the +3V3
ramp, in ngspice (README "stage 2", "Simulations"). Writes sim/reset.txt.

Espressif's rules (ESP32-S3 Series Datasheet v2.2, Table 2-13 p. 30 and Table 5-4 p. 65, read 2026-11-14): CHIP_PU goes
high only after the supplies are stable (tSTBL >= 50 us, the figure's rail level 2.8 V); a reset needs CHIP_PU under
VIL_nRST = 0.25 x VDD for tRST >= 50 us; VIH_nRST = 0.75 x VDD.
Models: +3V3 a ramp (the LDO's start: 0.1 ms with a cell plugged in, 1 ms and 5 ms with USB hosts, ASSUMED); the pin's
ESD diode from CHIP_PU to the chip's supply (ASSUMED: every ESP32 pin has one; it empties C7 when +3V3 collapses), the
chip's input otherwise open. The cases: power-up (USB order, battery order with a cell), a power cut of 1 .. 100 ms (a
cell's contact bounce, a quick re-plug), and with a cell fitted, USB unplugged (+3V3 dips to 2.9 V: power.py) - no reset.
  python3 hardware/mini2/sim/reset.py        (ngspice 47; a second)
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
NET = """* reset
.model DESD D(IS=1e-12 N=1.0)
V3 v3 0 {wave}
R8 v3 pu 10k
C7 pu 0 1u
Desd pu v3 DESD
Rchip v3 0 50
.control
tran 5u {stop}
{meas}
.endc
.end
"""


def run(wave, stop, meas):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(NET.format(wave=wave, stop=stop, meas=meas))
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=120).stdout
    return {m[1]: float(m[2]) for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M)}


def power_up(ramp):
    m = ("meas tran tvdd when v(v3)=3.0 rise=1\nmeas tran tpu when v(pu)=2.475 rise=1\n"
         "echo \"tvdd = $&tvdd\"\necho \"tpu = $&tpu\"\n")
    return run(f"PWL(0 0 1m 0 {1e-3 + ramp} 3.3)", "0.1", m)


def cut(off):
    """+3V3 falls to 0 in 0.2 ms (the chip's ~60 mA on 34 uF), stays off `off` seconds, back in 0.1 ms"""
    t1 = 0.01
    wave = f"PWL(0 3.3 {t1} 3.3 {t1 + 2e-4} 0 {t1 + 2e-4 + off} 0 {t1 + 3e-4 + off} 3.3)"
    m = (f"meas tran pumin min v(pu) from={t1} to={t1 + 3e-4 + off}\n"
         f"meas tran tlow when v(pu)=0.825 fall=1\nmeas tran tup when v(pu)=0.825 rise=1\n"
         "echo \"pumin = $&pumin\"\necho \"tlow = $&tlow\"\necho \"tup = $&tup\"\n")
    return run(wave, f"{t1 + off + 0.05}", m)


def dip():
    wave = "PWL(0 3.3 10m 3.3 10.1m 2.9 30m 2.9 30.1m 3.3)"
    m = "meas tran pumin min v(pu) from=10m to=40m\necho \"pumin = $&pumin\"\n"
    return run(wave, "0.05", m)


def main():
    out = ["jank mini 2.2, CHIP_PU (R8 10 k / C7 1 uF): ngspice results (reset.py; its docstring)", ""]
    for ramp, what in ((1e-4, "a cell plugged in (0.1 ms)"), (1e-3, "a USB host (1 ms)"), (5e-3, "a slow host (5 ms)")):
        r = power_up(ramp)
        out.append(f"power-up, +3V3 rising in {ramp * 1000:g} ms ({what}): +3V3 at 3.0 V at {r['tvdd'] * 1000:.2f} ms, "
                   f"CHIP_PU at VIH (2.475 V) at {r['tpu'] * 1000:.2f} ms: {(r['tpu'] - r['tvdd']) * 1000:.1f} ms after "
                   "the supply (Espressif: >= 0.05 ms)")
    out.append("")
    for off in (1e-3, 5e-3, 20e-3, 100e-3):
        r = cut(off)
        low = (r["tup"] - r["tlow"]) * 1e3 if "tlow" in r and "tup" in r else 0.0
        out.append(f"+3V3 cut for {off * 1000:g} ms: CHIP_PU down to {r['pumin']:.2f} V; under VIL (0.825 V) for "
                   f"{low:.2f} ms (a reset needs >= 0.05 ms)" if low else
                   f"+3V3 cut for {off * 1000:g} ms: CHIP_PU down to {r['pumin']:.2f} V: never under VIL")
    r = dip()
    out.append("")
    out.append(f"USB unplugged with a cell fitted (+3V3 dips to 2.9 V for 20 ms, power.py): CHIP_PU stays at "
               f"{r['pumin']:.2f} V or more: no reset (the brown-out detector decides)")
    open(os.path.join(HERE, "reset.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
