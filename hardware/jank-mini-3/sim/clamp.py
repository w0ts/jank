# SPDX-License-Identifier: GPL-3.0-only
"""The 5 V aux rail against its clamp (U19 CJ431 behind R145 10 ohm, R138 24k / R139 18k), Monte Carlo, Python.
Fixes REVIEW3 R1 / R2 of mini 2.2: the clamp moved from 5.0 V to 5.83 V (no WS2812 on the rail any more).
 - boost regulating: V5A = VREF x (1 + R134 / (R135 + R154 + R136)); VREF 580..610 mV (TPS61023 SLVSF14B, PWM),
   PFM up to +16 mV over typical (ASSUMED, as REVIEW3); 1 % resistors (uniform).
 - pass-through on USB: VIN_B = VBUS - VF(D3); the TPS61023 passes VIN through (SLVSF14B 7.3.5) less its PMOS + L5's
   DCR (~0.1 ohm x I); VBUS 4.75..5.5 V (USB-C vSafe5V), VF(SS34) at the quiescent current 0.06..0.20 V (25..85 C,
   MDD SS34 Fig. 1 read at 0.1..1 mA; ASSUMED range).
 - the clamp: VK = Vref x (1 + R138 / R139) + Iref x R138; Vref 2.475..2.525 V, Iref 1.5..4 uA (CJ431 sheet).
 - faults: +10 V into CLOCK and RESET at once (each through 2 x 1k and a BAV99 into V5C = V5A).
Writes clamp.txt.   python3 sim/clamp.py"""
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
N = 200_000
U = random.uniform


def r(nom, tol=0.01):
    return nom * U(1 - tol, 1 + tol)


def clamp_v():
    return U(2.475, 2.525) * (1 + r(24e3) / r(18e3)) + U(1.5e-6, 4e-6) * 24e3


def boost_v(pfm=False):
    vref = U(0.580, 0.610) + (U(0, 0.016) if pfm else 0)
    return vref * (1 + r(1e6) / (r(100e3) + r(20e3) + r(15e3)))


def main():
    random.seed(1)
    out = ["# clamp.py: the 5 V aux rail against its 5.83 V clamp (Monte Carlo, %d boards)" % N, ""]
    worst = {}
    for label, f in (("boost PWM", lambda: boost_v()), ("boost PFM (+16 mV, ASSUMED)", lambda: boost_v(True)),
                     ("USB pass-through, no load", lambda: U(4.75, 5.5) - U(0.06, 0.20)),
                     ("USB 5.5 V pass-through, hot (VF 0.06 V)", lambda: 5.5 - 0.06)):
        over, imax, margin = 0, 0.0, 9.9
        for _ in range(N):
            v, k = f(), clamp_v()
            margin = min(margin, k - v)
            if v > k:
                over += 1
                imax = max(imax, (v - k) / 10.0)   # through R145 (the boost / USB stiff)
        worst[label] = (over, imax, margin)
        out.append(f"- {label}: {over / N * 100:.3f} % of boards conduct, worst {imax * 1000:.1f} mA,"
                   f" smallest margin {margin * 1000:+.0f} mV")
    vk = [clamp_v() for _ in range(N)]
    vb = [boost_v(True) for _ in range(N)]
    out.append(f"\nclamp threshold {min(vk):.3f} .. {max(vk):.3f} V; boost set point {min(vb):.3f} .. {max(vb):.3f} V")
    # the fault: +10 V into both outputs, the sources 0 ohm (worst), 1k + the BAV99 (0.75 V at 5 mA) into V5C; the rail's
    # own load ~0 (worst: MIDI idle, U21 idle); the boost cannot sink. V5A = VK + 10 ohm x I.
    i = 2 * (10 - 5.6 - 0.75) / 2000   # (2 x 1k in series since review B R1)
    vmax = max(vk) + 10 * i
    out.append(f"\nfault, +10 V into CLOCK and RESET: {i * 1000:.1f} mA into the rail, V5A <= {vmax:.2f} V"
               f" (U21 74AHCT04 VCC abs max 7 V, recommended 5.5 V; TPS61023 VOUT abs max 7 V);"
               f" U19 {max(vk) * i * 1000:.0f} mW (300 mW)")
    i12 = 2 * (12 - 5.6 - 0.75) / 2000
    out.append(f"fault, +12 V: {i12 * 1000:.1f} mA, V5A <= {max(vk) + 10 * i12:.2f} V, U19 {max(vk) * i12 * 1000:.0f} mW")
    txt = "\n".join(out) + "\n"
    open(os.path.join(HERE, "clamp.txt"), "w").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
