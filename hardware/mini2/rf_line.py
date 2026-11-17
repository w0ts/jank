# SPDX-License-Identifier: GPL-3.0-only
"""The RF line's width and gap (README "RF"): a grounded coplanar waveguide on the top layer, the bottom layer's pour
under it, for JLC's 2-layer 1.6 mm FR-4. Wadell's quasi-static formula (Transmission Line Design Handbook, 1991, 3.4.3),
zero copper thickness. make_board.py uses RF_W / RF_GAP; this prints what they give and the 50 ohm width per gap.

  python3 rf_line.py
"""
import math

H = 1.6 - 2 * 0.035          # dielectric between the copper layers, mm (1.6 mm board, 1 oz copper both sides)
ER = 4.5                     # FR-4 at 2.4 GHz: ASSUMED (a typical figure; JLC's own value for this core not read)
RF_W, RF_GAP = 1.2, 0.2      # what make_board.py draws


def K(k):
    """complete elliptic integral of the first kind, modulus k (arithmetic-geometric mean)"""
    a, b = 1.0, math.sqrt(1 - k * k)
    for _ in range(40):
        a, b = (a + b) / 2, math.sqrt(a * b)
    return math.pi / (2 * a)


def gcpw(w, g, h=H, er=ER):
    """(Z0 ohm, effective permittivity) of a grounded coplanar line: track w, gaps g, dielectric h (mm)"""
    k = w / (w + 2 * g)
    k1 = math.tanh(math.pi * w / (4 * h)) / math.tanh(math.pi * (w + 2 * g) / (4 * h))
    kp, k1p = math.sqrt(1 - k * k), math.sqrt(1 - k1 * k1)
    r, r1 = K(kp) / K(k), K(k1) / K(k1p)
    eeff = (1 + er * r * r1) / (1 + r * r1)
    return 60 * math.pi / math.sqrt(eeff) / (K(k) / K(kp) + K(k1) / K(k1p)), eeff


def main():
    z, e = gcpw(RF_W, RF_GAP)
    lam = 299.792458 / 2.45 / math.sqrt(e)
    print(f"board: h {H:.2f} mm, er {ER}; line {RF_W} mm, gaps {RF_GAP} mm: Z0 {z:.1f} ohm, e_eff {e:.2f}, "
          f"wavelength at 2.45 GHz {lam:.0f} mm")
    for er in (4.3, 4.5, 4.6):
        print(f"  er {er}: Z0 {gcpw(RF_W, RF_GAP, er=er)[0]:.1f} ohm")
    for g in (0.15, 0.2, 0.25):
        w = min((abs(gcpw(x / 1000, g)[0] - 50), x / 1000) for x in range(300, 3000, 5))[1]
        print(f"  50 ohm with gaps {g} mm: track {w:.3f} mm")


if __name__ == "__main__":
    main()
