# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2: a USB cable plugged in hot, VBUS's ringing, in ngspice (the Standard study's experiment A, 2026-11-15:
C27 / C91, the 10 uF tantalums, to 10 uF 25 V 0805 ceramics with a series resistor). Writes sim/vbus.txt.

The circuit (the board's VBUS node, the worst case: D3 not conducting, VSYS already up): the charger's 5.25 V (USB's
maximum; 5.5 V for a loose charger) through the cable: 0.7 uH (about 1 m, ASSUMED) and 0.15 ohm (cable and contacts, the
low end: less damping), switched in within 100 ns; on VBUS: C40 (10 uF 10 V X5R 0603, ~5 uF at 5 V: DC bias, ASSUMED from
Samsung's curves for the CL10A106 class) and the bulk under test: the tantalums (2 x 10 uF, ESR 2 ohm each: TAJA106K016's
sheet gives 3.0 ohm max at 100 kHz, 2 ASSUMED typical) or 2 x CL21A106KAYNNNE (10 uF 25 V X5R 0805, ~7 uF at 5 V, ESR
5 mohm) each through R = 0 / 1 / 4.7 ohm. Limits: C40 10 V, TP4054's VCC 10 V absolute maximum (its sheet), so the peak
should stay well under ~8 V.
  python3 hardware/mini2/sim/vbus.py
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def peak(vsrc, bulk):
    net = f"""* hot plug
Vs src 0 PWL(0 0 1u 0 1.1u {vsrc})
Rc src a 0.15
Lc a vbus 0.7u
C40 vbus c40n 5u
Rc40 c40n 0 5m
Rload vbus 0 1k
{bulk}
.control
tran 5n 60u
meas tran pk max v(vbus)
meas tran vend avg v(vbus) from=50u to=60u
echo "pk = $&pk"
echo "vend = $&vend"
.endc
.end
"""
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(net)
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=120).stdout
    return {m[1]: float(m[2]) for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M)}


def main():
    cases = [("the tantalums (2 x 10 uF, ESR 2 ohm)", "Ct1 vbus t1 10u\nRt1 t1 0 2\nCt2 vbus t2 10u\nRt2 t2 0 2")]
    for r in (0.005, 1.0, 4.7):
        cases.append((f"2 x 10 uF 25 V 0805 ceramic, each through {r:g} ohm",
                       f"Ct1 vbus t1 7u\nRt1 t1 0 {r}\nCt2 vbus t2 7u\nRt2 t2 0 {r}"))
    out = ["jank mini 2.2, VBUS when a cable is plugged in hot (vbus.py: the circuit and the assumptions in its docstring)", ""]
    for v in (5.25, 5.5):
        for name, bulk in cases:
            m = peak(v, bulk)
            out.append(f"{v} V supply, {name}: VBUS peak {m['pk']:.2f} V (settles {m['vend']:.2f} V)")
        out.append("")
    open(os.path.join(HERE, "vbus.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
