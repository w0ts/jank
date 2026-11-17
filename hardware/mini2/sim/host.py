# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's USB HOST signal on VBUS (battery order only; parts.py "HOST"; README "USB host mode"), in ngspice.
Writes sim/host.txt.

The user's option A: jank hosts SELF-POWERED gear only, with a current-limited 5 V "signal" on the connector's VBUS so
an attached device sees a host; it never powers the device. Since 2026-11-15 the signal comes from the 5 V aux rail
(U18, a TPS61023 boost from VREG; parts.py "AUX5"): V5A -> R133 (0 ohm) -> V5H; no HT7550, no charge pump.
 - Q7 (AO3401A) V5H -> HOST5, its gate pulled up to V5H (R90 100 k: off) and pulled down by Q8 (2N7002) from GPIO42
   (HOST_EN, R91 100 k to GND). R92 20 k (47 k until 2026-11-15) pulls HOST5 to GND when Q7 is off.
 - R93 2.2 k HOST5 -> HOST_R, D16 (BAT54C, one half) HOST_R -> VUSB (the connector's VBUS).
 - Q9 (AO3401A): source VUSB, drain VBUS (the board's), gate on HOST5: on in device mode, off in host mode.
 - U17 TUSB320LI (the CC logic) is not simulated: the roles are its datasheet's.
Models (each ASSUMED unless said):
 - the aux rail: 4.85 V (4.72 .. 4.97: the TPS61023's VREF +-2.5 %, R136 20 k since 2026-11-15) through 50 mohm, sourcing only, 10 uF; the CJ431
   clamp at 5.11 .. 5.32 V (cv.py); its 3.7 A valley limit makes it stiff for these currents.
 - Q7 / Q9: cv.py's generic P-channel VDMOS; Q8 its 2N7002. D16: BAT54 fitted to Nexperia's maximum VF, plus 250 k across
   it (20 uA at 5 V: a hot board). The device: 100 k (a VBUS-detect divider) // 10 uF; 10 k; a short (0.1 ohm).
 - the charger (a legacy always-on 5 V): 5.25 / 5.5 V through 0.25 ohm. The board's VBUS side: C27 / C91 (10 uF, 3 ohm),
   C40, R88 + R89, D3 into VSYS (0.3 A as 12 ohm, the cell behind a Schottky), the TP4054 as power.py's current source.
  python3 hardware/mini2/sim/host.py        (ngspice; a minute)
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RLIM = int(os.environ.get("HOST_RLIM", "2200"))      # R93 on the board
MODELS = """
.model QN VDMOS(Rg=3 Vto=1.6 Rd=0 Rs=.75 Rb=.14 Kp=.17 mtriode=1.25 Cgdmax=80p Cgdmin=12p Cgs=50p Cjo=50p Is=.04p)
.model QP VDMOS(pchan Rg=6 Vto=-0.9 Rd=.02 Rs=.01 Kp=12 Cgdmax=180p Cgdmin=60p Cgs=600p Cjo=150p Is=1e-12)
.model DSK D(IS=31.7u RS=.051 N=1.373 CJO=110p M=.35 VJ=.3 BV=40)
.model DID D(IS=1e-12 N=0.01 RS=0.05)
.model DBAT D(IS=9.9e-8 N=1.34 RS=2 CJO=10p)
"""
DEVICES = {"detect": "Rdev vusb 0 100k\nCdev vusb 0 10u", "10k": "Rdev vusb 0 10k\nCdev vusb 0 10u",
           "short": "Rdev vusb 0 0.1", "none": "Rdev vusb 0 100meg"}
ON, OFF = "Vhen hens 0 3.3", "Vhen hens 0 0"


def net(aux, en, rlim, device, charger=""):
    return f"""* host
{MODELS}
Vb bs 0 {aux}
Db bs v5a DID
Caux v5a 0 10u
Bclamp v5a 0 I = max(0, (V(v5a) - 5.11) / 0.2)
Vmaux v5a v5h 0
R90 v5h hg 100k
M7 host5 hg v5h v5h QP
M8 hg hen 0 0 QN
{en}
Rhen hens hen 17
R91 hen 0 100k
R92 host5 0 20k
R93 host5 hr {rlim}
Vmd hr hrd 0
D16 hrd vusb DBAT
Rleak vusb hrd 250k
M9 vq9 host5 vusb vusb QP
Vm9 vq9 vbus 0
R94 vusb vdet 940k
Rdet vdet 0 95k
{DEVICES[device]}
{charger}
C27 vbus c27 10u
Rc27 c27 0 3
C91 vbus c91 10u
Rc91 c91 0 3
C40 vbus 0 10u
R88 vbus 0 55k
Vmb vbus vbd 0
D3 vbd vsys DSK
Vcell vc 0 3.6
Dcell vc vsys DSK
Rsys vsys 0 12
Bchg vbus vsys I = 1000 / 12000 * 0.25 * (1 + tanh((V(vbus) - 4.0) / 0.05)) * (1 + tanh((4.2 - V(vsys)) / 0.02))
"""


def run(n, ctl):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(n + f".control\n{ctl}\n.endc\n.end\n")
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=600).stdout
    return {m[1]: float(m[2]) for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M)}


def echo(*names):
    return "\n".join(f'echo "{n} = $&{n}"' for n in names)


MEAS = ("meas tran v5 avg v(v5a) from=58m to=60m\nmeas tran vu avg v(vusb) from=58m to=60m\n"
        "meas tran ih avg i(Vmd) from=58m to=60m\nmeas tran iq avg i(Vm9) from=58m to=60m\n"
        "meas tran g avg v(host5) from=58m to=60m\nmeas tran ia avg i(Vmaux) from=58m to=60m\n")
NAMES = ("v5", "vu", "ih", "iq", "g", "ia")


def steady(aux, en, rlim, device, charger=""):
    return run(net(aux, en, rlim, device, charger), "tran 10u 60m 55m\n" + MEAS + echo(*NAMES))


def line(m):
    return (f"V5A {m['v5']:.3f} V, VBUS at the connector {m['vu']:.2f} V, the signal's current {m['ih'] * 1000:.2f} mA "
            f"(from the aux rail {m['ia'] * 1000:.2f} mA); through Q9 into the board {m['iq'] * 1e6:.2f} uA (Q9's gate "
            f"{m['g']:.2f} V)")


def levels():
    out, res = [], {}
    for aux, corner in ((4.845, "typ"), (4.724, "low")):
        for dev in ("detect", "10k", "short"):
            m = steady(aux, ON, RLIM, dev)
            res[(corner, dev)] = m
            out.append(f"aux {aux} V ({corner}), host ON, device {dev}: " + line(m))
    return out, res


def charger():
    out, worst = [], 0.0
    for vchg, aux in ((5.25, 4.845), (5.5, 4.724)):
        chg = f"Vchg cs 0 {vchg}\nVsw sw 0 PWL(0 0 50m 0 50.001m 1)\nS1 cs cr sw 0 SWC\n.model SWC SW(VT=0.5 RON=0.25 ROFF=1e9)\nRcr cr vusb 1m"
        en = "Vhen hens 0 PWL(0 3.3 55m 3.3 55.001m 0)"
        m = run(net(aux, en, RLIM, "detect", chg), "tran 10u 60m 45m\n"
                "meas tran ihmin min i(Vmd) from=50m to=60m\nmeas tran vu7 avg v(vusb) from=53m to=54.9m\n"
                "meas tran iq7 avg i(Vm9) from=53m to=54.9m\nmeas tran vb10 avg v(vbus) from=59m to=60m\n"
                "meas tran v5max max v(v5a) from=50m to=60m\nmeas tran g7 avg v(host5) from=53m to=54.9m\n"
                + echo("ihmin", "vu7", "iq7", "vb10", "v5max", "g7"))
        back = max(0.0, -m["ihmin"])
        worst = max(worst, back)
        out.append(f"a {vchg} V charger plugged in host mode (aux {aux} V): back into the signal path {back * 1e6:.2f} uA at "
                   f"most (the 250 k leakage the model assumes), V5A never above {m['v5max']:.3f} V; before the firmware "
                   f"drops HOST_EN: Q9's gate {m['g7']:.2f} V vs VBUS {m['vu7']:.2f} V ({m['iq7'] * 1000:.2f} mA through Q9); "
                   f"after: the board's VBUS {m['vb10']:.2f} V (the charger runs)")
    return out, worst


def device_mode():
    out = []
    m = steady(4.724, OFF, RLIM, "none", "Vchg cs 0 5.0\nRcs cs vusb 0.25")
    out.append("device mode, a host's 5.0 V on VBUS: " + line(m))
    m = steady(0.0, OFF, RLIM, "none", "Vchg cs 0 5.0\nRcs cs vusb 0.25")
    out.append("the aux rail dead (0 V: the boost off, dead or lifted), a charger's 5.0 V: " + line(m))
    return out


def main():
    out = ["jank mini 2.2, the USB host signal on VBUS from the 5 V aux rail (battery order): ngspice results (host.py; "
           f"models in its docstring); R93 = {RLIM} ohm", ""]
    lv, res = levels()
    ch, back = charger()
    for name, lines in (("levels", lv), ("a charger plugged in host mode", ch), ("device mode", device_mode())):
        out.append(name + ":")
        out += ["  " + x for x in lines]
        out.append("")
    d, t, s = res[("low", "detect")], res[("low", "10k")], res[("low", "short")]
    out.append(f"SUMMARY (aux at its low 4.72 V): host VBUS {d['vu']:.2f} V into 100 k, {t['vu']:.2f} V into 10 k; a short "
               f"takes {s['ih'] * 1000:.2f} mA and leaves V5A at {s['v5']:.3f} V; back-feed with a charger {back * 1e6:.2f} uA")
    open(os.path.join(HERE, "host.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
