# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's power path in ngspice (README "stage 2", "Simulations"): USB plugged and unplugged with a cell fitted
(Q1's switch-over, the VSYS dip, +3V3; stage 1f's circuit and the board's: R50 10 k, D9), the +3V3 droop at the Wi-Fi peak with the HT7833 (battery order) and with the
AMS1117 (usb order), a reversed cell (Q2), and the charger's current (TP4054). Writes sim/power.txt.

The circuit (parts.py): VBUS -> D3 (B5819W) -> VSYS; the cell -> J8 / J9 (BAT_C) -> Q2 (AO3401A, gate to GND through R53
100 k: reverse polarity) -> BAT_P -> Q1 (AO3401A, gate = VBUS, R50 10 k to GND: on without USB) and D9 (B5819W) -> VSYS; VSYS -> U10 (HT7833) or
U2 (AMS1117) -> +3V3; TP4054 (U8) from VBUS into BAT_P; C40 10 uF on VBUS, C41 10 uF on BAT_P, C3 10 uF on VSYS, C5 22 uF +
the chip's ~12 uF on +3V3.
Models (GENERIC; each figure's source):
 - AO3401A: a level-1 PMOS, Vth -0.9 V, 60 mohm at Vgs -3.7 V, its body diode 0.7 V at 0.1 A (AOS datasheet's typical
   figures as recalled, not re-read this session).
 - B5819W: a diode fitted to ~0.3 V at 0.1 A and ~0.4 V at 0.5 A (CJ's sheet gives 0.45 V max at 1 A; the fit ASSUMED).
 - HT7833: Vout = min(3.3, Vin - 20 mV - 0.68 ohm x I) (Holtek: 360 mV dropout at 500 mA), 0.1 ohm output, a 10 us lag,
   its current limited at 0.6 A (ASSUMED: the sheet's limit not read).
 - AMS1117-3.3: Vout = min(3.3, Vin - 0.95 V - 0.19 ohm x I) (its sheet: 1.1 V typical dropout at 800 mA), likewise,
   limited at 1.0 A (ASSUMED).
 - the power toggle (an MTS-102 on J11's two wires): 30 mohm closed (ASSUMED: the old SW6's figure; the toggle's sheet
   not read); the XB5353A's FET in the minus lead 50 mohm (ASSUMED).
 - the cell: 3.0 .. 4.2 V behind 0.15 ohm (ASSUMED; small LiPos are 0.05 .. 0.3 ohm).
 - USB: the host 5.0 V (and 4.75 V, USB 2.0's minimum at the plug) through 0.25 ohm (cable and contacts, ASSUMED).
 - the chip: 60 mA, plus 280 mA Wi-Fi bursts (340 mA peak, Espressif's TX figure as recalled) 2 ms every 10 ms.
 - TP4054: a current source VBUS -> BAT_P of 1000 V / RPROG while VBUS > 4.0 V and BAT_P < 4.2 V (its datasheet's
   formula, as parts.py), else 0.
  python3 hardware/mini2/sim/power.py        (ngspice 47; a few seconds)
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

MODELS = """
.options method=gear reltol=1e-3
.model PFET PMOS(LEVEL=1 VTO=-0.9 KP=5.95)
.model DBODY D(IS=1e-9 N=1.4 RS=0.05)
.model DSCH D(IS=3e-6 N=1.1 RS=0.1)
.subckt LDO vin out gnd vdo0=0.02 rdo=0.68 ilim=0.6
Bh h gnd V = V(vin) - {vdo0} - {rdo} * V(io)
Btgt t gnd V = (3.3 + V(h) - sqrt((3.3 - V(h)) * (3.3 - V(h)) + 1e-4)) / 2
Rlag t t2 1
Clag t2 gnd 10u
Bio io gnd V = min({ilim}, max(0, (V(t2) - V(out)) / 0.1))
Bo gnd out I = V(io)
Bin vin gnd I = V(io)
.ends
"""
LOAD = "Iload v3v3 0 PULSE(0.06 0.34 {t0} 10u 10u 2m 10m)\nC5 v3v3 0 34u\n"


def run(net):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(net)
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=300).stdout
    vals = {m[1]: float(m[2]) for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M)}
    if not vals:
        raise SystemExit(out[-3000:])
    return vals


def battery_net(vhost, vcell, usb_on, usb_off, r50="10k", c40="10u", reverse=False, rprog=5100, bypass=True, psw="PWL(0 1)"):
    """the battery order: USB from usb_on to usb_off (seconds; usb_off None = stays)"""
    on = "PWL(0 1" if usb_on == 0 else f"PWL(0 0 {usb_on} 0 {usb_on + 1e-6} 1"
    sw = on + (f" {usb_off} 1 {usb_off + 1e-6} 0)" if usb_off else ")")
    cell = (f"Vcell cm bat_c {vcell}\nRcell cm bat_n 0.15\n" if reverse else
            f"Vcell cp cn {vcell}\nRcell cp bat_c 0.15\nRxb cn 0 0.054\n")   # (Rxb: the XB5353A's FET in the minus lead, 54 mohm typ: its datasheet)
    if bypass:                                   # (the stage-2 change: a B5819W from BAT_P to VSYS, beside Q1's body diode)
        cell += "D9 bat_p vsys DSCH\n"
    return (MODELS + f"Vhost host 0 {vhost}\nRcab host hostr 0.25\nVsw swc 0 {sw}\n"
            "S1 hostr vbus swc 0 SWU\n.model SWU SW(VT=0.5 RON=0.01 ROFF=1e9)\n"
            f"C40 vbus 0 {c40}\nR50 vbus 0 {r50}\nD3 vbus vsys DSCH\n"
            f"Vswc swk 0 {psw}\nRt1 vsys t1 0.1\nLt1 t1 sa 50n\nSsw sa vsw swk 0 SWP\n.model SWP SW(VT=0.5 RON=0.03 ROFF=1e9)\n"
            "Vsm vsw t2 0\nRt2 t2 t3 0.1\nLt2 t3 vreg 50n\n"
            "C3 vreg 0 10u\nC34 vreg 0 1u\n" + cell +
            "Mq2 bat_c q2g bat_p bat_p PFET\nDq2 bat_c bat_p DBODY\nR53 q2g 0 100k\n"
            "Mq1 bat_p vbus vsys vsys PFET\nDq1 bat_p vsys DBODY\nC41 bat_p 0 10u\nR10 bat_p adc 1Meg\nR11 adc 0 1Meg\n"
            f"Bchg vbus bat_p I = 1000 / {rprog} * 0.25 * (1 + tanh((V(vbus) - 4.0) / 0.05)) * (1 + tanh((4.2 - V(bat_p)) / 0.02))\n"
            "XU10 vreg v3v3 0 LDO vdo0=0.02 rdo=0.68 ilim=0.6\n")


def unplug(vcell, r50="10k", c40="10u", bypass=True):
    net = battery_net(5.0, vcell, 0, 0.05, r50, c40, bypass=bypass) + LOAD.format(t0="0.049") + (
        ".control\ntran 10u 0.6 uic\n"
        "meas tran vsysmin min v(vsys) from=0.05 to=0.6\nmeas tran v3min min v(v3v3) from=0.05 to=0.6\n"
        "meas tran v3pre min v(v3v3) from=0.02 to=0.05\n"
        "meas tran tbrown when v(v3v3)=3.0 fall=1 from=0.05\nmeas tran tback when v(vsys)='" + f"{vcell - 0.3}" +
        "' rise=1 from=0.051\n"
        "echo \"vsysmin = $&vsysmin\"\necho \"v3min = $&v3min\"\necho \"v3pre = $&v3pre\"\n"
        "echo \"tback = $&tback\"\n.endc\n.end\n")
    return run("* unplug\n" + net)


def plug(vcell):
    net = battery_net(5.0, vcell, 0.05, None) + LOAD.format(t0="0.051") + (
        ".control\ntran 10u 0.12 uic\n"
        "meas tran vsysmin min v(vsys) from=0.03 to=0.12\nmeas tran v3min min v(v3v3) from=0.03 to=0.12\n"
        "meas tran icell avg i(Vcell) from=0.1 to=0.12\nmeas tran vsysend avg v(vsys) from=0.1 to=0.12\n"
        "echo \"vsysmin = $&vsysmin\"\necho \"v3min = $&v3min\"\necho \"icell = $&icell\"\necho \"vsysend = $&vsysend\"\n"
        ".endc\n.end\n")
    return run("* plug\n" + net)


def battery_only(vcell):
    net = battery_net(5.0, vcell, 1.0, None) + LOAD.format(t0="0.02") + (
        ".control\ntran 10u 0.06 uic\nmeas tran v3min min v(v3v3) from=0.03 to=0.06\n"
        "meas tran vsysmin min v(vsys) from=0.03 to=0.06\necho \"v3min = $&v3min\"\necho \"vsysmin = $&vsysmin\"\n"
        ".endc\n.end\n")
    return run("* battery only\n" + net)


def switch_on(vcell):
    """the power toggle (J11) closed at 10 ms with the cell alone: the inrush into C3 / C34 (the regulators' input caps,
    after the switch) and on into +3V3: its peak through the switch (0.5 A rated, its specification 1.1) and through the
    cell's minus lead (the XB5353A: its over-current / short-circuit detection: README, unverified). The switch sits ~55
    mm from the power path by track, each way: 0.1 ohm and 50 nH each (ESTIMATES: 0.3 mm track, ~1 nH / mm); C41 (10 uF
    on BAT_P) gives the first charge"""
    net = battery_net(5.0, vcell, 1.0, None, psw="PWL(0 0 10m 0 10.001m 1)") + "Rload v3v3 0 55\nC5 v3v3 0 34u\n" + (
        ".control\ntran 0.05u 0.03 uic\nlet isw = i(Vsm)\nmeas tran ipk max isw from=0.01 to=0.012\n"
        "let icell = -i(Vcell)\nmeas tran icpk max icell from=0.01 to=0.012\necho \"icpk = $&icpk\"\n"
        "meas tran toc when icell=2.1 fall=last from=0.01 to=0.03\necho \"toc = $&toc\"\n"
        "meas tran t50 when isw=0.5 fall=1 from=0.01\nmeas tran v3 avg v(v3v3) from=0.025 to=0.03\n"
        "meas tran vsysmin min v(vsys) from=0.0099 to=0.012\n"
        "echo \"ipk = $&ipk\"\necho \"t50 = $&t50\"\necho \"v3 = $&v3\"\necho \"vsysmin = $&vsysmin\"\n.endc\n.end\n")
    return run("* switch on\n" + net)


def off_drain(vcell):
    """the switch open, no USB: what the cell still feeds: R10 / R11 (2 M), D3's reverse leakage (the model's), the
    XB5353A's own supply (IOPE 2.8 uA typ, 6 max: its datasheet) and TP4054's battery-side drain (1 uA ASSUMED: not
    read from its sheet)"""
    net = battery_net(5.0, vcell, 1.0, None, psw="PWL(0 0)") + "Iu9 bat_p 0 2.8u\nIu8 bat_p 0 1u\nC5 v3v3 0 34u\n" + (
        ".control\ntran 10u 0.5 uic\nmeas tran ic avg i(Vcell) from=0.4 to=0.5\necho \"ic = $&ic\"\n.endc\n.end\n")
    return run("* off\n" + net)


def reversed_cell():
    net = battery_net(5.0, 3.7, 1.0, None, reverse=True, bypass=True) + "Rload v3v3 0 55\nC5 v3v3 0 34u\n" + (
        "* the XB5353A (U9) in the minus lead: a body diode BAT_N -> GND (ASSUMED, the worst: a dual FET blocks it), cell + on BAT_N\n"
        "Du9 bat_n 0 DBODY\n"
        ".control\ntran 10u 0.05 uic\nmeas tran ic avg i(Vcell) from=0.03 to=0.05\n"
        "let dq2 = v(bat_p) - v(bat_c)\nmeas tran vq2 avg dq2 from=0.03 to=0.05\necho \"ic = $&ic\"\necho \"vq2 = $&vq2\"\n.endc\n.end\n")
    return run("* reversed\n" + net)


def usb_order(vhost):
    net = (MODELS + f"Vhost host 0 {vhost}\nRcab host vbus 0.25\nD3 vbus vsys DSCH\nRtr vsys vreg 0.2\n"
           "C3 vreg 0 10u\nC34 vreg 0 1u\nXU2 vreg v3v3 0 LDO vdo0=0.95 rdo=0.19 ilim=1.0\n" + LOAD.format(t0="0.02") +
           ".control\ntran 10u 0.06\nmeas tran v3min min v(v3v3) from=0.03 to=0.06\n"
           "meas tran vsysmin min v(vsys) from=0.03 to=0.06\necho \"v3min = $&v3min\"\necho \"vsysmin = $&vsysmin\"\n"
           ".endc\n.end\n")
    return run("* usb order\n" + net)


def main():
    out = ["jank mini 2.2, the power path: ngspice results (power.py; generic models: its docstring)", ""]
    out.append("battery order, USB unplugged at 50 ms during a Wi-Fi burst; stage 1f's circuit (R50 100 k, no D9):")
    for vc in (4.2, 3.7, 3.4):
        r = unplug(vc, r50="100k", bypass=False)
        out.append(f"  cell {vc} V: VSYS min {r['vsysmin']:.2f} V, +3V3 min {r['v3min']:.2f} V (before: "
                   f"{r['v3pre']:.2f}); VSYS back on Q1's channel {r['tback'] - 0.05:.3f} s after the unplug"
                   if "tback" in r else f"  cell {vc} V: VSYS min {r['vsysmin']:.2f} V, +3V3 min {r['v3min']:.2f} V; "
                   "Q1 not on within 0.55 s")
    out.append("the same with R50 10 k (VBUS drains 10 x faster, Q1 turns on sooner):")
    for vc in (4.2, 3.7, 3.4):
        r = unplug(vc, r50="10k", bypass=False)
        out.append(f"  cell {vc} V: VSYS min {r['vsysmin']:.2f} V, +3V3 min {r['v3min']:.2f} V; VSYS back "
                   f"{r['tback'] - 0.05:.3f} s after the unplug" if "tback" in r else f"  cell {vc} V: VSYS min "
                   f"{r['vsysmin']:.2f} V, +3V3 min {r['v3min']:.2f} V; Q1 not on within 0.55 s")
    out.append("the board (stage 2): R50 10 k and D9 (a second B5819W, BAT_P -> VSYS, beside Q1's body diode):")
    for vc in (4.2, 3.7, 3.4):
        r = unplug(vc, r50="10k", bypass=True)
        out.append(f"  cell {vc} V: VSYS min {r['vsysmin']:.2f} V, +3V3 min {r['v3min']:.2f} V; VSYS back "
                   f"{r['tback'] - 0.05:.3f} s after the unplug" if "tback" in r else f"  cell {vc} V: VSYS min "
                   f"{r['vsysmin']:.2f} V, +3V3 min {r['v3min']:.2f} V; Q1 not on within 0.55 s")
    out.append("")
    out.append("battery order, USB plugged in at 50 ms (a burst at 51 ms):")
    for vc in (3.4, 4.2):
        r = plug(vc)
        out.append(f"  cell {vc} V: VSYS min {r['vsysmin']:.2f} V, +3V3 min {r['v3min']:.2f} V, then VSYS "
                   f"{r['vsysend']:.2f} V; the cell's current after it {r['icell'] * 1000:+.0f} mA (+ = charging: ngspice's current into the cell's + end)")
    out.append("")
    out.append("battery order, the cell alone, 340 mA Wi-Fi peak:")
    for vc in (4.2, 3.7, 3.5, 3.4):
        r = battery_only(vc)
        out.append(f"  cell {vc} V: VSYS min {r['vsysmin']:.3f} V, +3V3 min {r['v3min']:.3f} V")
    out.append("usb order (AMS1117), 340 mA Wi-Fi peak (VSYS to VREG through R62 and the copper: 0.2 ohm, an upper"
               " bound: all of VSYS and VREG's tracks in series are ~0.14 ohm at their routed widths):")
    for vh in (5.0, 4.75, 4.5):
        r = usb_order(vh)
        out.append(f"  host {vh} V: VSYS min {r['vsysmin']:.3f} V, +3V3 min {r['v3min']:.3f} V")
    out.append("")
    out.append("the power toggle (J11, stage 2) closed with the cell alone (C3 10 uF + C34 1 uF on VREG, C5 + the chip's"
               " 34 uF on +3V3):")
    for vc in (4.2, 3.7):
        r = switch_on(vc)
        oc = f"above 2.1 A for {(r['toc'] - 0.01) * 1e6:.1f} us" if "toc" in r else "never above 2.1 A"
        out.append(f"  cell {vc} V: {r['ipk']:.2f} A peak through the switch, {r['icpk']:.2f} A from the cell ({oc}); "
                   f"under 0.5 A after {(r['t50'] - 0.01) * 1e6:.0f} us; VSYS min {r['vsysmin']:.2f} V; +3V3 then {r['v3']:.3f} V")
    out.append("  the XB5353A (its datasheet): overcurrent 1 at 2.1 A min / 3 A typ after 10 ms typ, load short at 10 A min "
               "/ 20 A typ after 75 us typ: the inrush trips neither. The switch's 0.5 A is its continuous rating; the "
               "~11 A peak lasts microseconds (C41 into C3 / C34 through the tracks: the model's 0.1 ohm / 50 nH estimates)")
    r = off_drain(4.2)
    out.append(f"switched off, no USB, a 4.2 V cell: {-r['ic'] * 1e6:.1f} uA from the cell (R10 / R11 2.1 uA, D3's leakage,"
               " the XB5353A 2.8 uA typ, the TP4054 1 uA ASSUMED): a 550 mAh cell lasts years on it (self-discharge first)")
    out.append("")
    r = reversed_cell()
    out.append(f"a reversed cell (+ on BAT_N, - on BAT_C), no USB: the cell's current {abs(r['ic']) * 1e6:.2f} uA, "
               f"{abs(r['vq2']):.2f} V across Q2 (blocked; Q2's 30 V rating)")
    out.append("")
    out.append("the charger (TP4054, I = 1000 V / RPROG, its datasheet's formula): R51 5.1 k -> 196 mA; with a twin on "
               "RT51: 10 k -> 296 mA, 5.1 k -> 392 mA (TP4054's 500 mA maximum; its own thermal regulation cuts the "
               "current when hot: at 5 V in and a 3.6 V cell, 196 mA = 0.27 W in the SOT-23-5)")
    open(os.path.join(HERE, "power.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
