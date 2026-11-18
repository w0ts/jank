# SPDX-License-Identifier: GPL-3.0-only
"""LINE IN (response, levels, rejection, faults, plug detect), the buttons, and the ESD paths of every connector.
LINE IN by nodal analysis (numpy, exact for the linear network; the ADC's sampling cap left out), parts.py's values,
the ADC's range at 12 dB attenuation 0..2.9 V (ESP32-S3 datasheet v2.2, Table 5-5 class figure: ASSUMED 2.9 V).
ESD: IEC 61000-4-2 contact +8 kV (150 pF, 330 ohm) in ngspice, generic models: the SRV05-4 as steering diodes into
its REF rail with a 6.5 V clamp to GND (its sheet: VC ~ 12 V at 1 A pulse class, ASSUMED), the PESD15VL2BT as two
back-to-back 18 V breakdowns, the S3 pin as diodes to its 3V3 (the LCD-2's rail, 22 uF + its decoupling).
Writes linein_esd.txt.   python3 sim/linein_esd.py"""
import os

import numpy as np

from spice import MODELS, run

HERE = os.path.dirname(os.path.abspath(__file__))
R30 = R31 = R65 = R66 = 20e3
C30 = 1e-6
R32 = R67 = 12e3
C32 = 2.2e-9
R33, R34 = 150e3, 120e3
C33 = 10e-6
V33 = 3.3
VB = V33 * R34 / (R33 + R34)
ADC_HI = 2.9


def solve(f, vt, vr, vs):
    """node voltages A, B (AC) for tip / ring / sleeve sources (stiff), at f Hz"""
    w = 2 * np.pi * f
    zc = 1 / (1j * w * C30)
    # unknowns: A, B, VB(ac)
    yA = 2 / (R30 + zc) + 1 / R32 + 1j * w * C32
    yB = 2 / (R65 + zc) + 1 / R67 + 1j * w * C32
    yV = 1 / R32 + 1 / R67 + 1 / R33 + 1 / R34 + 1j * w * C33
    M = np.array([[yA, 0, -1 / R32], [0, yB, -1 / R67], [-1 / R32, -1 / R67, yV]], complex)
    rhs = np.array([(vt + vr) / (R30 + zc), 2 * vs / (R65 + zc), 0], complex)
    a, b, _v = np.linalg.solve(M, rhs)
    return a, b


def linein(out):
    out.append(f"## LINE IN (bias {VB:.3f} V, ADC 0..{ADC_HI} V: headroom {min(VB, ADC_HI - VB):.2f} V at each node)")
    out.append("| f | mono (tip = ring = 1 V): A - B | stereo L only: A - B | ground offset (tip = ring = sleeve = 1 V): A - B | rejection |")
    out.append("|---|---|---|---|---|")
    mid = None
    for f in (5, 10, 20, 50, 100, 1e3, 5e3, 10e3, 13e3, 20e3, 40e3):
        a, b = solve(f, 1, 1, 0)
        m = abs(a - b)
        a2, b2 = solve(f, 1, 0, 0)
        a3, b3 = solve(f, 1, 1, 1)
        g = abs(a3 - b3)
        if f == 1e3:
            mid = m
        out.append(f"| {f:g} Hz | {m:.3f} | {abs(a2 - b2):.3f} | {g:.2e} | {20 * np.log10(m / max(g, 1e-12)):.0f} dB |")
    # corners
    fs = np.logspace(0, 5, 2000)
    g = np.array([abs(np.subtract(*solve(f, 1, 1, 0))) for f in fs])
    band = fs[g >= mid / np.sqrt(2)]
    out.append(f"\n-3 dB band (mono): {band[0]:.1f} Hz .. {band[-1] / 1e3:.1f} kHz; mid-band gain A-B / v = {mid:.3f}"
               f" (node A alone {abs(solve(1e3, 1, 1, 0)[0]):.3f})")
    clip = min(VB, ADC_HI - VB) / abs(solve(1e3, 1, 1, 0)[0])
    out.append(f"clipping at 1 kHz: mono {clip:.2f} V peak ({20 * np.log10(clip / np.sqrt(2) / 0.775):+.1f} dBu);"
               f" one channel only {min(VB, ADC_HI - VB) / abs(solve(1e3, 1, 0, 0)[0]):.2f} V peak."
               " Line level (+4 dBu = 1.74 V peak) fits mono with "
               f"{20 * np.log10(clip / 1.736):+.1f} dB to spare")
    # matching: 1 % R, 5 % C0G -> rejection with tolerances (Monte Carlo on the AC network at 50 Hz and 1 kHz)
    rng = np.random.default_rng(3)
    worst = {50: 1e9, 1000: 1e9}
    for _ in range(4000):
        r = rng.uniform(0.99, 1.01, 6)
        c = rng.uniform(0.95, 1.05, 6)   # (the 1 uF X5R: 10 % class, ASSUMED +-5 % matched by lot; worst case +-10 % below)
        for f in worst:
            w = 2 * np.pi * f
            zA1, zA2 = R30 * r[0] + 1 / (1j * w * C30 * c[0]), R31 * r[1] + 1 / (1j * w * C30 * c[1])
            zB1, zB2 = R65 * r[2] + 1 / (1j * w * C30 * c[2]), R66 * r[3] + 1 / (1j * w * C30 * c[3])
            yA = 1 / zA1 + 1 / zA2 + 1 / (R32 * r[4]) + 1j * w * C32 * c[4]
            yB = 1 / zB1 + 1 / zB2 + 1 / (R67 * r[5]) + 1j * w * C32 * c[5]
            # sleeve offset 1 V on all three, VB(ac) ~ 0 (C33): A = (1/zA1 + 1/zA2) / yA, B = (1/zB1 + 1/zB2) / yB
            cm = abs((1 / zA1 + 1 / zA2) / yA - (1 / zB1 + 1 / zB2) / yB)
            dm = abs((1 / zA1 + 1 / zA2) / yA)
            worst[f] = min(worst[f], 20 * np.log10(dm / cm))
    out.append(f"ground-offset rejection with 1 % R, 5 % C (Monte Carlo 4000): >= {worst[50]:.0f} dB at 50 Hz,"
               f" >= {worst[1000]:.0f} dB at 1 kHz")
    # faults: a +-12 V step on the tip (a modular CV patched into LINE IN): the coupling cap passes the step, the node
    # is held by the pin's diodes (3.3 + 0.5 V / -0.5 V); the current through the 20k decays with 20k x 1u = 20 ms
    for v in (12, -12):
        node = VB + v * abs(solve(1e3, 1, 0, 0)[0])
        held = min(max(node, -0.5), V33 + 0.5)
        i = abs(node - held) / (R30 * 0 + 1 / (1 / R30 + 1 / R32))   # (the excess through the node's own impedance)
        out.append(f"fault {v:+d} V step on the tip: node would reach {node:+.2f} V, held at {held:+.2f} V by the pin's"
                   f" diodes, ~{i * 1e6:.0f} uA into the pin for ~20 ms (PESD15VL2BT off below 17 V)")
    out.append("plug detect: no plug -> J7's ring switch (3) on the ring (2), R37 100k down vs R38 1M up:"
               f" LIN_DET = {V33 * 100 / 1100:.2f} V (low); a plug opens it: 3.3 V through 1M (high)")


def esd_deck(path):
    rail = "VR3 r33 0 3.3\nRR3 r33 v33 0.05\nC33V v33 0 22u\n"
    pin = ".model DPIN D(IS=1e-14 N=1 RS=2 BV=12 IBV=1m)\n"
    gun = "CE gun 0 150p IC=8000\nRE gun x 330\n"
    if path == "MIDI ring (no array: through R14 into V5M)":
        body = (f"{gun}R14 x v5m 220\nCRX x 0 10p\nC5M v5m 0 20u\nDZ 0 v5m DZ65\n"
                ".model DZ65 D(IS=1e-12 N=1 RS=0.5 BV=6.5 IBV=1m)\n")
        probe = "v5m"
    elif path == "MIDI tip -> R15 -> the array on the FET drain":
        body = (f"{gun}CTX x 0 10p\nR15 x k 220\nDS1 0 k DSTEER\nDS2 k v5m DSTEER\nDZ 0 v5m DZ65\nC5M v5m 0 20u\n"
                "MQ k 0 0 AO3400\n.model DSTEER D(IS=1e-12 N=1.1 RS=0.3 CJO=1p)\n.model DZ65 D(IS=1e-12 N=1 RS=0.5 BV=6.5 IBV=1m)\n")
        probe = "k"
    elif path == "CLOCK tip -> U21 output":
        body = (f"{gun}C89 x 0 2.2n\nR75 x n 1k\nDA 0 n BAV99\nDB n v5c BAV99\nC5C v5c 0 20u\nRO n d 50\nVD d 0 0\n")
        probe = "n"
    elif path == "LINE IN tip -> GPIO8":
        body = (f"{gun}D7a x z 0 DZ18\nD7b 0 z DZ18\nR30 x a1 20k\nC30 a1 a 1u\nR32 a vb 12k\nC32 a 0 2.2n\nVB vb 0 1.47\n"
                "DP1 a v33 DPIN\nDP2 0 a DPIN\n" + rail + pin + ".model DZ18 D(IS=1e-12 N=1 RS=1 BV=18 IBV=1m)\n")
        body = body.replace("D7a x z 0 DZ18\nD7b 0 z DZ18\n", "D7a x z DZ18\nD7b 0 z DZ18\n")
        probe = "a"
    else:   # USB D+ through U5 (REF on +3V3) to the S3 (through the LCD-2's cable / its 22 ohm)
        body = (f"{gun}DS1 0 x DSTEER\nDS2 x v33 DSTEER\nRL x a 22\nDP1 a v33 DPIN\nDP2 0 a DPIN\n" + rail + pin
                + ".model DSTEER D(IS=1e-12 N=1.1 RS=0.3 CJO=1p)\n")
        probe = "a"
    return f"""* esd {path}
{MODELS}
{body}
.tran 0.1n 1u 0 0.2n uic
.measure tran vpk MAX v({probe})
.measure tran vmin MIN v({probe})
.end
""", probe


def main():
    out = ["# linein_esd.py: LINE IN, buttons, ESD paths", ""]
    linein(out)
    out.append("\n## Buttons")
    out.append("- UP (IO16) / DOWN (IO21): the LCD-2's 4.7k pull-ups (R4 / R5): pressed 0.70 mA, released 3.3 V;"
               " NO (IO6) / YES (IO18): the S3's internal pull-up (~45 k, ESP32-S3 datasheet): pressed ~73 uA."
               " No RC fitted (C80..C83 DNP pads): the firmware debounces. A button shorts only its GPIO to GND: a GPIO"
               " driven high by a firmware fault into a pressed button is limited by the S3's drive (~40 mA, drive"
               " strength 2), a fault, not a damage case at the default drive.")
    out.append("- ESD: the TS-1187A's actuator is plastic and its contacts sit inside; the case covers the frame. No"
               " exposed metal on the button nets, so no ESD part (the connectors below are the exposed paths).")
    out.append("\n## ESD, +8 kV contact (150 pF / 330 ohm) on each connector pin (peak at the protected node)")
    out.append("| path | node | peak | min |")
    out.append("|---|---|---|---|")
    for path in ("MIDI ring (no array: through R14 into V5M)", "MIDI tip -> R15 -> the array on the FET drain",
                 "CLOCK tip -> U21 output", "LINE IN tip -> GPIO8", "USB D+ -> the S3"):
        deck, probe = esd_deck(path)
        m = run(deck, "esd")
        out.append(f"| {path} | {probe} | {m['vpk']:.1f} V | {m['vmin']:.1f} V |")
    # MIDI jack mis-patches (reviews C R2 / D R1): a modular output on the tip / ring, stiff or through its own R
    out.append("\n## MIDI jacks mis-patched with a modular output (V5A held by the clamp at <= 6.05 V + 10 ohm x I)")
    out.append("| where | source | current | in the 220 ohm 1206 (0.25 W) | into the rail / out of GND |")
    out.append("|---|---|---|---|---|")
    for where in ("tip", "ring"):
        for v, rs in ((12, 1000), (12, 100), (12, 0), (-12, 1000), (-12, 100), (-12, 0)):
            if where == "tip":   # tip -> R15 220 -> MO_K: the array's diode to V5M (+) or to GND (-), or the FET body diode
                i = (abs(v) - 0.75 - (6.05 if v > 0 else 0)) / (rs + 220)
            else:                # ring -> R14 220 -> V5M (the rail sits at ~5 V, held by the clamp above, the boost below)
                i = (v - 5.0) / (rs + 220) if v > 0 else (5.0 - v) / (rs + 220)
            i = max(i, 0)
            out.append(f"| {where} | {v:+d} V through {rs} ohm | {i * 1000:.1f} mA | {i * i * 220 * 1000:.0f} mW |"
                       f" {'rail' if v > 0 else 'GND / rail'} |")
    txt = "\n".join(out) + "\n"
    open(os.path.join(HERE, "linein_esd.txt"), "w").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
