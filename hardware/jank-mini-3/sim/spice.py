# SPDX-License-Identifier: GPL-3.0-only
"""Shared ngspice runner and GENERIC device models for the LCD-2 add-on's simulations (not the vendors' models: each
is fitted to two or three datasheet points, named below). Temp decks go in a temporary folder under this folder."""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

MODELS = """
* SS34 (MDD sheet: VF 0.50 V at 3 A, ~0.30 V at 0.1 A, ~0.2 V at 10 mA, 25 C)
.model SS34 D(IS=1e-5 N=1.15 RS=0.04 CJO=250p BV=40 IBV=1m)
* BAT54 (Nexperia: VF 0.24 V at 0.1 mA, 0.32 V at 1 mA, 0.4 V at 10 mA)
.model BAT54 D(IS=1.5e-7 N=1.05 RS=2 CJO=10p BV=30 IBV=1u)
* BAV99 (Nexperia: VF 0.715 V at 1 mA, 0.855 V at 10 mA)
.model BAV99 D(IS=2.5e-9 N=1.75 RS=0.8 CJO=1.5p BV=100 IBV=1u)
* AO3401A P-FET (AOS: VGS(th) -0.5..-1.3 V, RDS(on) 44 / 60 / 85 mohm at -10 / -4.5 / -2.5 V, BVDSS -30 V)
.model AO3401 VDMOS(pchan Vto=-0.9 Kp=25 Rd=0.03 Rs=0.01 Rg=6 Cgdmax=150p Cgdmin=40p Cgs=600p Cjo=120p Is=1e-12 Rb=0.02 BV=32)
* AO3400A N-FET (AOS: VGS(th) 0.65..1.45 V, RDS(on) 26 / 32 / 48 mohm at 10 / 4.5 / 2.5 V)
.model AO3400 VDMOS(Vto=1.05 Kp=40 Rd=0.015 Rs=0.008 Rg=3 Cgdmax=120p Cgdmin=30p Cgs=500p Cjo=100p Is=1e-12 Rb=0.02 BV=32)
"""


def run(deck, name="x"):
    """run an ngspice batch deck; returns {measure name: float} from 'name = value' lines"""
    with tempfile.TemporaryDirectory(dir=HERE, prefix=".run-") as d:
        p = os.path.join(d, name + ".cir")
        open(p, "w").write(deck)
        r = subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, timeout=300, cwd=d)
    out = {}
    for line in (r.stdout + r.stderr).splitlines():
        m = re.match(r"^\s*([a-z_0-9]+)\s*=\s*([-+0-9.eE]+)", line)
        if m:
            out[m.group(1)] = float(m.group(2))
    if not out:
        raise RuntimeError(f"ngspice gave no measures:\n{r.stdout[-3000:]}\n{r.stderr[-3000:]}")
    return out
