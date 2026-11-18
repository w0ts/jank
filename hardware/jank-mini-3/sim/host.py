# SPDX-License-Identifier: GPL-3.0-only
"""USB: the hot plug on the LCD-2's own USB-C (VBUS_B), the host signal on J4 (OUT, USB-A), ESD on J4's VBUS.
ngspice, generic models (spice.py).
 A. a stiff computer / charger (5.25 / 5.5 V, cable 0.7 / 1 uH, 50 mohm) hot-plugged into the LCD-2's USB-C: its trace
    to the 5V pin (ASSUMED 20 nH) -> VBUS_B with the LCD-2's C26 1u, D1 -> VCC (C33 10u, its 150 mA), our C27 22u (12u
    derated) + R151 1, R50 / R88+R89 / its R25, R157 1 + D3 -> VIN_B (10u derated). VIN_B against the TPS61023's
    5.5 V recommended / 7 V abs max (SLVSF14B 6.1 / 6.3); "before the fix": C27 10u, no R157.
 B. host mode: HOST5 = V5C 4.79 V (74AHCT04, ~50 ohm), R93 2.2k, D16 onto VUSB (C28 1u): VUSB for device loads.
 C. ESD, IEC 61000-4-2 contact +8 kV (150 pF / 330 ohm) on J4's VBUS: with C28 1u and without.
Writes host.txt.   python3 sim/host.py"""
import os
import re
import subprocess
import tempfile

from spice import MODELS, run

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD_T = """
LTR lcdusb VBUS_B 20n
C26 VBUS_B 0 1u
D1L VBUS_B VCC SS34
C33L VCC 0 10u IC=0
RVCC VCC 0 33
R151 VBUS_B vbd1 1
{c27path}
R50 VBUS_B 0 100k
R8889 VBUS_B 0 200k
R25L VBUS_B 0 10k
RD3 VBUS_B vd3 {rd3}
D3 vd3 VIN_B SS34
CVIN VIN_B 0 10u
"""
_D = "D17 vbd1 vbc SS34\nC27 vbc 0 12u\nR158 vbc 0 10k"
FIX = {"as drawn (C27 22u behind D17, R157 1 ohm)": dict(c27path=_D, rd3=1),
       "C27 straight on VBUS_B (round 1)": dict(c27path="C27 vbd1 0 12u", rd3=1),
       "before the fix (C27 10u, no R157)": dict(c27path="C27 vbd1 0 6u", rd3="1m"),
       "the LCD-2 alone (no add-on)": dict(c27path="C27 vbd1 0 1p", rd3="1e9")}


def hotplug(vsrc, lcab, fix):
    return f"""* hot plug
{MODELS}
VS src 0 PWL(0 0 1u 0 1.1u {vsrc})
RC src l1 0.05
LC l1 lcdusb {lcab}u
{BOARD_T.format(**FIX[fix])}
.tran 5n 200u 0 20n uic
.measure tran vbusb_pk MAX v(VBUS_B)
.measure tran vinb_pk MAX v(VIN_B)
.measure tran ipk MAX i(LC)
.end
"""


def host(rload):
    load = f"RLD VUSB 0 {rload}\n" if rload else ""
    return f"""* host mode
{MODELS}
VH5 h5src 0 5.0
RH h5src HOST5 50
R93 HOST5 HOST_R 680
D16 HOST_R VUSB BAT54
C28 VUSB 0 0.6u
{load}.op
.control
run
print v(VUSB) i(VH5)
.endc
.end
"""


def esd(cap):
    c28 = "C28 VUSB 0 0.6u\n" if cap else ""
    return f"""* esd
{MODELS}
CE gun 0 150p IC=8000
RE gun VUSB 330
{c28}D16 HOST_R VUSB BAT54
R93 HOST5 HOST_R 680
RH HOST5 h5 50
VH5 h5 0 0
CPAR VUSB 0 20p
.tran 0.1n 2u 0 0.2n uic
.measure tran vusb_pk MAX v(VUSB)
.measure tran vusb_min MIN v(VUSB)
.end
"""


def out_hotplug(vsrc, lcab):
    """a legacy A-to-C cable (its own 56k Rp) from a computer / charger into J4 (OUT): VBUS arrives with no CC
    handshake; only C28, D16 (reversed) and Q-less VUSB see it"""
    return f"""* out hot plug
{MODELS}
VS src 0 PWL(0 0 1u 0 1.1u {vsrc})
RC src l1 0.05
LC l1 VUSB {lcab}u
C28 VUSB 0 0.6u
D16 HOST_R VUSB BAT54
R93 HOST5 HOST_R 680
RH HOST5 h5 50
VH5 h5 0 0
.tran 5n 200u 0 20n uic
.measure tran vusb_pk MAX v(VUSB)
.measure tran ih5 MAX i(VH5)
.end
"""


def cc_esd():
    """+8 kV contact on J4's CC1: U5's steering diodes to +3V3 (the LCD-2's rail, 22u) / GND, R160 56k to HOST5"""
    return f"""* cc esd
{MODELS}
CE gun 0 150p IC=8000
RE gun cc 330
CCC cc 0 10p
DS1 0 cc DST
DS2 cc v33 DST
.model DST D(IS=1e-12 N=1.1 RS=0.3 CJO=1p)
VR3 r33 0 3.3
RR3 r33 v33 0.05
C33 v33 0 22u
R160 cc h5 56k
RH h5 0 50
.tran 0.1n 1u 0 0.2n uic
.measure tran vcc_pk MAX v(cc)
.measure tran v33_pk MAX v(v33)
.end
"""


def op_print(deck):
    with tempfile.TemporaryDirectory(dir=HERE, prefix=".run-") as t:
        p = os.path.join(t, "op.cir")
        open(p, "w").write(deck)
        r = subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, cwd=t)
    return {k: float(v) for k, v in re.findall(r"^([a-z0-9_()#]+)\s*=\s*([-+0-9.eE]+)", r.stdout + r.stderr, re.M)}


def main():
    out = ["# host.py: USB hot plug (the LCD-2's USB-C), the host signal (J4), ESD on J4's VBUS (ngspice, generic models)", ""]
    out.append("## A. hot plug into the LCD-2's own USB-C")
    out.append("| board | source | cable L | VBUS_B peak | VIN_B peak (TPS61023: 5.5 rec., 7 abs) | peak current |")
    out.append("|---|---|---|---|---|---|")
    for fix in FIX:
        for v in (5.25, 5.5):
            for l in (0.7, 1.0):
                m = run(hotplug(v, l, fix), "hot")
                out.append(f"| {fix} | {v} V | {l} uH | {m['vbusb_pk']:.2f} V | {m['vinb_pk']:.2f} V | {m['ipk']:.1f} A |")
    out.append("\n## B. host mode (HOST5 = 5.0 V): the signal on J4's VBUS")
    out.append("| device's VBUS load | VUSB | from U21 |")
    out.append("|---|---|---|")
    for rl in (None, "100k", "10k", "1k", "100"):
        m = op_print(host(rl))
        out.append(f"| {rl or 'none'} | {m['v(vusb)']:.2f} V | {-m['i(vh5)'] * 1000:.2f} mA |")
    out.append("\n## C. ESD +8 kV contact on J4's VBUS (U21 output low)")
    out.append("| C28 | VUSB peak | VUSB min |")
    out.append("|---|---|---|")
    for cap in (True, False):
        m = run(esd(cap), "esd")
        out.append(f"| {'1u (as drawn)' if cap else 'none'} | {m['vusb_pk']:.1f} V | {m['vusb_min']:.1f} V |")
    out.append("\n## D. J4 (OUT, USB-C) hot-plugged by a legacy A-to-C source (VBUS with no CC handshake)")
    out.append("| source | cable L | VUSB peak (D16 30 V, C28 50 V) | current into U21's output |")
    out.append("|---|---|---|---|")
    for v in (5.25, 5.5):
        for l in (0.7, 1.0):
            m = run(out_hotplug(v, l), "outhp")
            out.append(f"| {v} V | {l} uH | {m['vusb_pk']:.2f} V | {m['ih5'] * 1000:.2f} mA |")
    m = run(cc_esd(), "ccesd")
    out.append(f"\n## E. ESD +8 kV contact on J4's CC1: CC peak {m['vcc_pk']:.1f} V, +3V3 peak {m['v33_pk']:.2f} V (U5 steers it)")
    txt = "\n".join(out) + "\n"
    open(os.path.join(HERE, "host.txt"), "w").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
