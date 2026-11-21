# SPDX-License-Identifier: GPL-3.0-only
"""mots's cells in parallel behind one protector: 2 or 3 x 1S LiPo (450 / 550 mAh, 4.2 V full), each on its own
PH2.0 socket through R_S = 0.33 ohm into BAT+ (= VBAT, the LCD-2's header pin P2.14); the cells' minus pins joined into
B-; DW01A (PUOLOP, LCSC C351410) + FS8205A (TECH PUBLIC, C2830320) between B- and the board's GND (P-).
Monte Carlo, Python (the paths are resistive; the time constants are minutes).
Sources:
 - DW01A C351410 datasheet (Rev B 2016-4-12): VOC 4.25..4.35 V, VOCR 4.05..4.15, VOD 2.40..2.60, VODR 2.80..3.00,
   overcurrent VEDI 130..170 mV (7..20 ms), short 0.82..1.75 V (400..600 us), no charge-overcurrent level listed;
   R1 100 ohm / C1 0.1 uF on VCC, R2 1k on VM.
 - FS8205A C2830320 datasheet: RDS(on) per FET 19.5 typ / 25 max mohm at VGS 4.5 V, 25 / 31.5 at 2.5 V (25 C, 3 A);
   ID 6 A (4.8 A at 70 C). The two FETs in series. ASSUMED: hot (70..85 C) x1.0..1.35; linear in VGS between the points.
 - ETA6096 (the LCD-2's ETA6098's pin-identical sibling): CV 4.17..4.25 V, fast charge 2 A at ISET 82k (the LCD-2's R15).
 - ASSUMED: cell + lead + PH2.0 contacts 60..150 mohm (a 450 mAh LiPo ~40..80 + lead ~20 + two contacts ~2 x 10..20).
 - ESTIMATE of the peak load from the cells (no USB): S3 Wi-Fi TX ~0.35 A at 3.3 V through the LCD-2's buck (eta 0.85)
   = 0.45 A at a 3.0 V cell, backlight ~0.06 A, LCD-2 rest ~0.03 A, our 5 V rail ~40 mA at 5 V = ~0.08 A from the cell:
   ~0.62 A; checked up to 1.2 A (x2 margin).
Writes cells.txt.   python3 sim/cells.py"""
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
N = 100_000
U = random.uniform
R_S = 0.33


def rds(vgs, hot=True):
    """one FET, worst-to-best spread: typ..max at the two datasheet points, interpolated in VGS"""
    lo = 19.5e-3 + (25e-3 - 19.5e-3) * (4.5 - vgs) / 2.0
    hi = 25e-3 + (31.5e-3 - 25e-3) * (4.5 - vgs) / 2.0
    return U(lo, hi) * (U(1.0, 1.35) if hot else 1.0)


def r_cell():
    return U(0.060, 0.150)


def discharge(lines):
    lines.append("DISCHARGE (no USB): the overcurrent trip current = VEDI / (2 x RDS), VGS = the cell voltage")
    for vc in (4.2, 3.6, 3.0, 2.6):
        trips = []
        for _ in range(N):
            trips.append(U(0.130, 0.170) / (rds(vc) + rds(vc)))
        trips.sort()
        lines.append(f"  cells at {vc:.1f} V: trip {trips[0]:.2f} A (lowest of {N}) .. {trips[-1]:.2f} A; "
                     f"0.1 % point {trips[N // 1000]:.2f} A; load estimate 0.62 A, checked to 1.2 A")
    lines.append("  short circuit (VM > 0.82 V): trips within 0.6 ms; the cells' current is limited by R_S anyway "
                 f"(4.2 V / (0.33 + 0.06) = {4.2 / 0.39:.1f} A per cell max, before the FETs)")


def charge(lines):
    lines.append("CHARGE (USB in, the charger's 2 A into BAT+; the protector carries it backwards)")
    for n in (3, 2):
        worst_share = 0
        worst_drop = 0
        vm_min = 0
        for _ in range(N):
            r = [R_S * U(0.99, 1.01) + r_cell() for _ in range(n)]
            g = [1 / x for x in r]
            share = [gi / sum(g) * 2.0 for gi in g]
            worst_share = max(worst_share, max(share))
            worst_drop = max(worst_drop, max(s * R_S for s in share))
            vm_min = min(vm_min, -2.0 * (rds(3.6) + rds(3.6)))
        lines.append(f"  {n} cells: the most-loaded cell takes {worst_share:.2f} A ({worst_share / 0.45:.1f} C of a 450, "
                     f"{worst_share / 0.55:.1f} C of a 550); R_S drops <= {worst_drop * 1000:.0f} mV and burns "
                     f"<= {worst_share ** 2 * R_S:.2f} W during the 2 A phase; VM down to {vm_min * 1000:.0f} mV "
                     f"(the DW01A lists no charge-overcurrent level: no trip)")
    lines.append(f"  1 cell: 2 A = 4.4 C of a 450 (too much: use 2 or 3 cells)")
    over = sum(1 for _ in range(N) if U(4.17, 4.25) >= U(4.25, 4.35)) / N
    lines.append(f"  end of charge: the charger's CV 4.17..4.25 V against the DW01A's overcharge 4.25..4.35 V: "
                 f"overlap in {over * 100:.3f} % of pairs (then it only stops charging at the very top, releases at "
                 f"4.05..4.15 V; harmless)")


def plug_mismatch(lines):
    lines.append("PLUGGING IN A CELL AT ANOTHER VOLTAGE (the current between cells does not pass the protector)")
    for dv in (0.3, 0.6, 1.2):
        worst_i = 0
        for _ in range(N):
            loop = 2 * R_S * U(0.99, 1.01) + r_cell() + r_cell()
            worst_i = max(worst_i, dv / loop)
        lines.append(f"  {dv:.1f} V apart: up to {worst_i:.2f} A at the start ({worst_i / 0.45:.1f} C into a 450), "
                     f"{worst_i ** 2 * R_S:.2f} W in each R_S, falling as they even out "
                     f"(tau ~ R x C_eq ~ 0.8 ohm x 675 F ~ 9 min, from a 450's ~1.2 V over its capacity)")
    lines.append("  (1.2 V = a full 4.2 V cell beside one cut off near 3.0 V: the README rule; the 2 W part keeps "
                 "the worst case under half its rating)")


def main():
    random.seed(1)
    lines = ["cells.py: 2-3 x 1S LiPo, 0.33 ohm each, DW01A + FS8205A (seed 1, %d draws)" % N, ""]
    discharge(lines)
    lines.append("")
    charge(lines)
    lines.append("")
    plug_mismatch(lines)
    lines.append("")
    lines.append("OVER-DISCHARGE: VOD 2.40..2.60 V at BAT+ (the joined node, after R_S: at 0.6 A over 3 cells it reads "
                 f"~{0.2 * R_S * 1000:.0f} mV under the cells), released at 2.80..3.00 V or by plugging in USB. "
                 "Sleep current through the DW01A ~3 uA.")
    text = "\n".join(lines) + "\n"
    open(os.path.join(HERE, "cells.txt"), "w").write(text)
    print(text)


if __name__ == "__main__":
    main()
