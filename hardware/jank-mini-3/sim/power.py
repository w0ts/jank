# SPDX-License-Identifier: GPL-3.0-only
"""The 5 V aux rail's input: VIN_B fed from the LCD-2's 5V pin (VBUS_B) through D3 when USB is in, from VBAT through
Q1 when it is not (parts.py). ngspice; the boost is a behavioural load on VIN_B (P_out / (eta x VIN_B), eta 0.85,
running down to 0.8 V), its output ideal. The LCD-2's side from its schematic: C26 1u on 5V, D1 MBR230 5V -> VCC,
its Q1 AO3401 VBAT -> VCC (gate on 5V, R25 10k down), C33 10u on VCC, the charger as a constant-current sink on 5V.
Checks: (1) on USB our draw from the cell is ~0 (the charger can terminate); (2) on the cell, VIN_B and Q1's drop;
(3) the USB unplug: VIN_B's dip until Q1 takes over. Writes power.txt.   python3 sim/power.py"""
import os

from spice import MODELS, run

HERE = os.path.dirname(os.path.abspath(__file__))
ETA, VMIN = 0.85, 0.8


C27_PATH = {"drawn": "D17 vbd1 vbc SS34\nC27 vbc 0 12u\nR158 vbc 0 10k", "round 1 (C27 on VBUS_B)": "C27 vbd1 0 12u"}


def deck(vusb, vbat, iload, unplug=None, ichg=0.5, ilcd=0.15, c27="drawn"):
    usb = (f"VUSB usbsrc 0 DC {vusb}\nRCAB usbsrc usbsw 0.2\n"
           + (f"S1 usbsw VBUS_B ctl 0 SW\n.model SW SW(Ron=1m Roff=1e9 Vt=0.5)\nVCTL ctl 0 PWL(0 1 {unplug} 1 {unplug + 1e-6} 0)\n"
              if unplug is not None else "RSW usbsw VBUS_B 1m\n"))
    tran = f".tran 2u {unplug + 0.4} 0 5u uic" if unplug is not None else ".op"
    meas = ("""
.measure tran vinb_min MIN v(VIN_B) from={u} to={e}
.measure tran v5_ok WHEN v(VIN_B)=0.8 FALL=1
.measure tran iq1_end FIND i(VQ1) AT={e}
.measure tran vbusb_end FIND v(VBUS_B) AT={e}
.measure tran vcc_min MIN v(VCC) from={u} to={e}
.measure tran vcc_low TRIG v(VCC) VAL={lo} FALL=1 TD={u} TARG v(VCC) VAL={lo} RISE=1 TD={u}
""".format(u=unplug, e=unplug + 0.39, lo=vbat - 0.3)) if unplug is not None else ""
    return f"""* power {vusb} {vbat} {iload}
{MODELS}
{usb}
* the LCD-2: C26, its charger (CC sink while 5V > 4.3 V), D1 + its VCC load, its Q1 cell -> VCC
C26 VBUS_B 0 1u
BCHG VBUS_B 0 I={ichg if vusb > 0 else 0}*0.5*(1+tanh((v(VBUS_B)-4.3)/0.05))
D1L VBUS_B VCC SS34
M1L VBATL VBUS_B VCC AO3401
R25L VBUS_B 0 10k
C33L VCC 0 10u
ILCD VCC 0 {ilcd}
VCELL cell 0 {vbat}
RCELL cell VBATL 0.15
VQ1 VBATL VBATQ 0
* ours: R50, R88 / R89 / C29 (VBUS_SENSE on io10), C27 / R151, D3 + R167, Q1 (drain VBAT, source VIN_B, gate VBUS_B), C99 + C104 (derated), the boost
R50 VBUS_B 0 100k
R88 VBUS_B io10 10k
R89 io10 0 15k
C29 io10 0 10n
R151 VBUS_B vbd1 1
{C27_PATH[c27]}
R157 VBUS_B vbd3 1
D3 vbd3 VIN_B SS34
R167 vbd3 0 4.7k
MQ1 VBATQ VBUS_B VIN_B AO3401
C99 VIN_B 0 7u
C104 VIN_B 0 3u
BBOOST VIN_B 0 I={5.0 * iload / ETA}/max(v(VIN_B),{VMIN})*0.5*(1+tanh((v(VIN_B)-{VMIN})/0.05))
{tran}
.control
run
{"print v(VIN_B) i(VQ1) v(VBUS_B)" if unplug is None else ""}
.endc
{meas}
.end
"""


def op(vusb, vbat, iload):
    d = deck(vusb, vbat, iload)
    import subprocess, tempfile, re
    with tempfile.TemporaryDirectory(dir=HERE, prefix=".run-") as t:
        p = os.path.join(t, "op.cir")
        open(p, "w").write(d)
        r = subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, cwd=t)
    vals = dict(re.findall(r"^(v\(vin_b\)|i\(vq1\)|v\(vbus_b\))\s*=\s*([-+0-9.eE]+)", r.stdout + r.stderr, re.M))
    return float(vals["v(vin_b)"]), float(vals["i(vq1)"]), float(vals["v(vbus_b)"])


def main():
    lines = ["# power.py: VIN_B, the 5 V aux rail's input (ngspice; behavioural boost, eta 0.85; generic FET / diode models)", ""]
    lines.append("## 1-2. steady state (I_VBAT = our draw from the cell through Q1; + = out of the cell)")
    lines.append("| USB | cell | V5A load | VIN_B | our I from the cell | VBUS_B |")
    lines.append("|---|---|---|---|---|---|")
    worst_usb = 0.0
    for vusb in (0.0, 4.4, 4.75, 5.0, 5.5):
        for vbat in (3.0, 3.7, 4.2):
            for il in (0.0, 0.05, 0.12):
                vin, iq, vb = op(vusb, vbat, il)
                if vusb > 0:
                    worst_usb = max(worst_usb, iq)
                lines.append(f"| {vusb:.2f} | {vbat:.1f} | {il * 1000:.0f} mA | {vin:.3f} | {iq * 1000:.3f} mA | {vb:.3f} |")
    lines.append(f"\nworst draw from the cell with USB in: {worst_usb * 1e3:.3f} mA")
    lines.append("\n## 3b. the LCD-2's own VCC at the unplug (review C R1): how long VCC sits > 0.3 V under the cell (its FET's body diode)")
    lines.append("| cell | C27 | VCC min | time under cell - 0.3 V |")
    lines.append("|---|---|---|---|")
    for vbat in (3.3, 3.7):
        for c27 in C27_PATH:
            m = run(deck(5.0, vbat, 0.12, unplug=0.01, ilcd=0.18, c27=c27), "vcc")
            t = f"{m['vcc_low'] * 1000:.1f} ms" if "vcc_low" in m else "never"
            lines.append(f"| {vbat} | {c27} | {m['vcc_min']:.2f} V | {t} |")
    lines.append("\n## 3. USB unplugged at 10 ms (V5A load 120 mA, the LCD-2's 150 mA, the charger 0.5 A until then)")
    lines.append("| cell | VIN_B min after unplug | VIN_B fell below 0.8 V | Q1 current at +390 ms | VBUS_B at +390 ms |")
    lines.append("|---|---|---|---|---|")
    for vbat in (3.0, 3.7, 4.2):
        m = run(deck(5.0, vbat, 0.12, unplug=0.01), "unplug")
        below = "yes" if "v5_ok" in m else "no"
        lines.append(f"| {vbat:.1f} | {m['vinb_min']:.3f} V | {below} | {m['iq1_end'] * 1000:.1f} mA | {m['vbusb_end']:.3f} V |")
    txt = "\n".join(lines) + "\n"
    open(os.path.join(HERE, "power.txt"), "w").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
