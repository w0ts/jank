# SPDX-License-Identifier: GPL-3.0-only
"""(Superseded 2026-11-15: MIDI OUT is now a 5 V current loop, sim/midi.py; this script models the 3.3 V GPIO-driven
design, kept as history.)
jank mini 2.2's MIDI outputs under single faults, in ngspice (README "stage 2", "MIDI shorts"). Writes
sim/midi_faults.txt: for a resistor set (source R_s from +3V3 to the ring / DIN 4, sink R_k from the GPIO to the tip /
DIN 5), each fault's GPIO current and each resistor's dissipation, and the current into a standard receiver.

The datasheet (Espressif ESP32-S3 Series Datasheet v2.2, worktrees/hw-tiny-scratch/ref/esp32-s3_datasheet_en.pdf):
 - Table 5-1 "Absolute Maximum Ratings" (p. 64): only "Ioutput, cumulative IO output current, max 1500 mA"; its note 2:
   "The product proved to be fully functional after all its IO pins were pulled high while being connected to ground
   for 24 consecutive hours at ambient temperature of 25 C" (the drive strength used is not stated). No per-pin maximum.
 - Table 5-4 "DC Characteristics (3.3 V, 25 C)" (p. 65): IOH typ 40 mA (VDD 3.3 V, VOH >= 2.64 V, PAD_DRIVER = 3); IOL
   typ 28 mA (VOL = 0.495 V, PAD_DRIVER = 3). No statement that the pins are current-limited.
 - p. 18, note 5: the default drive strengths: GPIO17 / GPIO18 10 mA, GPIO19 / GPIO20 40 mA, all others 20 mA.
 So a 127 mA short is NOT verified safe (no figure says so); the limit used below is 40 mA (the IOH figure).
Models: the GPIO high = +3V3 through 17 ohm, low = 0 V through 12 ohm (fitted to the IOH / IOL rows above: (3.3 - 2.64)
/ 40 mA = 16.5 ohm, 0.495 / 28 mA = 17.7 ohm; 12 ohm taken low for the worst sink case); a receiver = 220 ohm + an opto
LED (1.2 V or 1.5 V at 5 mA) + optionally a series diode (0.7 V, some receivers have one); 0402 rating 1/16 W.
  python3 hardware/mini2/sim/midi_faults.py
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RON_H, RON_L = 17.0, 12.0
P0402 = 1 / 16
LED = {1.2: "D(IS=3.1e-14 N=1.8 RS=2)", 1.5: "D(IS=1.1e-15 N=1.9 RS=2)"}


def op(net):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(net + "\n.end\n")
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=60).stdout
    return {m[1]: float(m[2]) for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M)}


def receiver_ma(rs, rk, v33, vf, series):
    net = (f"* rx\nV33 v 0 {v33}\nRs v r {rs}\nR220 r a 220\nVm a a2 0\n.model L {LED[vf]}\n.model D1 D(IS=2.5n N=1.75)\n"
           + ("Dl a2 b L\nDs b t D1\n" if series else "Dl a2 t L\n") +
           f"Rk t g {rk}\nRon g 0 {RON_L}\n.control\nop\nlet i = i(Vm)\necho \"i = $&i\"\n.endc")
    return op(net)["i"] * 1000


def faults(rs, rk, v33=3.47):
    """sustained single faults on one output (the other output of the same GPIO driving its receiver), +3V3 at its
    +5 % high (the worst dissipation)"""
    out = {}
    i = v33 / (rk + RON_H)                              # tip (DIN 5) to ground, the GPIO high (the UART's idle)
    out["tip-GND, GPIO high"] = (i * 1000, i * i * rk, None)
    i = v33 / rs                                        # ring (DIN 4) to ground
    out["ring-GND"] = (None, None, i * i * rs)
    i = v33 / (rs + rk + RON_L)                         # tip to ring, the GPIO low (sending)
    out["tip-ring, GPIO low"] = (i * 1000, i * i * rk, i * i * rs)
    # the other output of the same GPIO while this tip is shorted and the GPIO is high: the pin's high sags
    ig = v33 / (rk + RON_H)
    vhigh = v33 - ig * RON_H
    out["the twin output, tip short, GPIO high: its receiver's 'off' current"] = (None, None, None, vhigh)
    return out


def twin_leak(rs, rk, v33, vhigh):
    net = (f"* leak\nV33 v 0 {v33}\nRs v r {rs}\nR220 r a 220\nVm a a2 0\n.model L {LED[1.2]}\nDl a2 t L\n"
           f"Rk t g {rk}\nVg g 0 {vhigh}\n.control\nop\nlet i = i(Vm)\necho \"i = $&i\"\n.endc")
    return op(net)["i"] * 1000


def table(name, rs, rk, out):
    out.append(f"### {name}: source {rs:g} ohm, sink {rk:g} ohm")
    out.append("")
    out.append("| fault (sustained, +3V3 3.47 V) | GPIO mA | P sink W | P source W |")
    out.append("|---|---|---|---|")
    f = faults(rs, rk)
    for k, v in f.items():
        if len(v) == 4:
            leak = twin_leak(rs, rk, 3.47, v[3])
            out.append(f"| {k} | pin high sags to {v[3]:.2f} V | the twin's receiver gets {leak:.2f} mA while it "
                       f"should be off | |")
            continue
        g, pk, ps = v
        out.append(f"| {k} | {'' if g is None else f'{g:.0f}'} | {'' if pk is None else f'{pk:.3f}'} | "
                   f"{'' if ps is None else f'{ps:.3f}'} |")
    out.append("")
    out.append("| receiver | 3.0 V | 3.3 V |")
    out.append("|---|---|---|")
    for vf, ser in ((1.2, False), (1.5, False), (1.5, True)):
        a, b = receiver_ma(rs, rk, 3.0, vf, ser), receiver_ma(rs, rk, 3.3, vf, ser)
        out.append(f"| 220 ohm + {vf} V opto{' + series diode' if ser else ''} | {a:.2f} mA | {b:.2f} mA |")
    out.append("")


def main():
    out = ["# jank mini 2.2: the MIDI outputs under single faults (midi_faults.py; the datasheet's figures and the models"
           " in its docstring)", "",
           "Limits asked: the GPIO within 40 mA (Table 5-4's IOH; no per-pin maximum is stated), each 0402 within 1/16 W"
           " in a sustained short, +3V3 up, and >= 5 mA into a standard receiver at 3.0 V and 3.3 V.", ""]
    table("THE BOARD, the jacks (stage 2, the coordinator's decision: source three 100 ohm 1206 = 33.3 ohm, 0.75 W; sink "
          "68 ohm 0805, 0.125 W)", 100 / 3, 68, out)
    table("THE BOARD, the DINs (source two 100 ohm 1206 = 50 ohm, 0.5 W; sink 68 ohm 0805, 0.125 W)", 50, 68, out)
    out += ["Every sustained single fault stays inside each part's rating on the board's values (a tip short 0.113 W in a "
            "0.125 W sink; a ring or DIN pin-4 short 0.36 / 0.24 W shared by three / two 0.25 W 1206s); the GPIO's tip-short "
            "current is at the datasheet's IOH figure (40 mA typ, PAD_DRIVER 3: Table 5-4). The receivers get 5.5 mA "
            "(jacks) / 5.1 mA (DINs) at 3.3 V and 4.7 / 4.1 mA at 3.0 V into a 220 ohm + 1.5 V opto: accepted (the "
            "coordinator). **Firmware:** the MIDI pins at drive strength 2 (20 mA nominal): the pin's own driver then "
            "limits a short further; NOT modelled (the datasheet gives the drive per strength only as nominal currents, "
            "no output resistance): unverified.", ""]
    table("stage 1f (CA-033's values: source 33.3 / 33 ohm, sink 10 ohm 0603)", 33.3, 10, out)
    table("protection by resistors alone, 0402 (the smallest that keep every fault inside: source >= 174 ohm for a ring "
          "short, sink >= 138 ohm for a tip short at 1/16 W and <= 40 mA)", 180, 150, out)
    out += ["Resistors alone into 0402s: **no set meets every limit.** Keeping 0402s inside 1/16 W in a ring or tip short needs a source"
            " >= 174 ohm and a sink >= 138 ohm; 5 mA into a 220 ohm + 1.5 V receiver at 3.0 V needs source + sink <= 68"
            " ohm (1.5 V / 5 mA - 220 - 12): off by about 4.6 x. With a receiver that has a series diode (2.2 V in all),"
            " even 0 ohm gives only 3.6 mA at 3.0 V.", ""]
    open(os.path.join(HERE, "midi_faults.md"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
