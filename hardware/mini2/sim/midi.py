# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's four MIDI outputs in ngspice (README "stage 2", "Simulations"): the current in each receiver, a mono
plug in a jack, a shorted DIN pin 4, a shorted tip, and the GPIO's current. Writes sim/midi.txt.

The circuit (parts.py, stage 2's values): GPIO21 sinks MIDI OUT 1's jack (R15 68 ohm, source R14 // R26 // R48 = 33.3
ohm) and DIN (R55 68 ohm, source R54 // R58 = 50 ohm); GPIO17 the same for MIDI OUT 2 (R20 / R21 // R27 // R49, R57 /
R56 // R59). Each receiver: the MIDI 1.0 input
(220 ohm, the optocoupler's LED, a reverse diode across it).
Models (GENERIC, not the vendors'):
 - the receiver's LED: a diode fitted to 1.2 V at 5 mA (6N138 / H11L1 / PC900 class, "typical") and a high corner of
   1.7 V at 5 mA (an older or a slow opto); the reverse diode a 1N4148 (ngspice default diode).
 - the GPIO: low = 0 V through RON_L, high = +3V3 through RON_H, fitted to the ESP32-S3 datasheet v2.2, Table 5-4
   (p. 65): IOL 28 mA typ at VOL 0.495 V and IOH 40 mA typ at VOH >= 2.64 V, both at PAD_DRIVER = 3: 17.7 / 16.5 ohm;
   RON_L taken at 12 ohm (the worst for the sink current), RON_H 17 ohm. Typical figures, not limits.
 - +3V3: 3.3 V, and 3.47 V (the LDO's +5 % worst high).
  python3 hardware/mini2/sim/midi.py        (ngspice 47; a second)
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RON_L, RON_H = 12.0, 17.0
LEDS = {"1.2 V": "D(IS=3.1e-14 N=1.8 RS=2)", "1.7 V": "D(IS=2.6e-17 N=2.0 RS=2)"}
# connector: (source ohms from +3V3, sink ohms to the GPIO, the GPIO)
CONN = {"J2 jack OUT 1": (100 / 3, 68, "g21"), "J5 DIN OUT 1": (50, 68, "g21"),
        "J3 jack OUT 2": (100 / 3, 68, "g17"), "J6 DIN OUT 2": (50, 68, "g17")}


def run(net):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SIM_TMP")) as d:
        p = os.path.join(d, "x.cir")
        open(p, "w").write(net)
        out = subprocess.run(["nice", "-n", "19", "ngspice", "-b", p], capture_output=True, text=True, timeout=120).stdout
    vals = {m[1]: float(m[2]) for m in re.finditer(r"^(\w+)\s*=\s*([-+\d.eE]+)", out, re.M)}
    if not vals:
        raise SystemExit(out[-2000:])
    return vals


def circuit(v33, led, gpio_level, fault=None):
    """gpio_level 0 = both GPIOs low (every receiver on), 1 = high; fault: (connector, 'ring' | 'tip') shorted to ground,
    the receiver of that connector unplugged"""
    lines = [f"V33 v33 0 {v33}", f".model LED {LEDS[led]}", ".model D4148 D(IS=2.5n N=1.75 RS=0.6)"]
    for g in ("g21", "g17"):
        lines.append(f"Vg{g} {g}x 0 {v33 if gpio_level else 0}")
        lines.append(f"Rg{g} {g}x {g} {RON_H if gpio_level else RON_L}")
    for k, (name, (rsrc, rsnk, g)) in enumerate(CONN.items()):
        lines += [f"Rsrc{k} v33 r{k} {rsrc}", f"Rsnk{k} t{k} {g} {rsnk}"]
        if fault and fault[0] == name:
            lines.append(f"Rf{k} {'r' if fault[1] == 'ring' else 't'}{k} 0 0.05")
            continue
        lines += [f"R220_{k} r{k} a{k}x 220", f"Vm{k} a{k}x a{k} 0",    # (Vm: the receiver's current)
                  f"Dled{k} a{k} t{k} LED", f"Drev{k} t{k} a{k} D4148"]
    lines.append(".op")
    lines.append(".control\nop\n" + "".join(
        f"let i{k} = i(Vm{k})\necho \"i{k} = $&i{k}\"\n" for k, n in enumerate(CONN) if not (fault and fault[0] == n)) +
        "let ig21 = i(Vgg21)\necho \"ig21 = $&ig21\"\nlet ig17 = i(Vgg17)\necho \"ig17 = $&ig17\"\n" +
        "".join(f"let p{k} = (v(v33) - v(r{k})) * (v(v33) - v(r{k})) / {CONN[n][0]}\necho \"p{k} = $&p{k}\"\n"
                for k, n in enumerate(CONN)) + ".endc")
    return run("* midi\n" + "\n".join(lines) + "\n.end\n")


def main():
    names = list(CONN)
    out = ["jank mini 2.2, the four MIDI outputs: ngspice results (midi.py; generic models: its docstring)", ""]
    for v33 in (3.3, 3.47):
        for led in LEDS:
            r = circuit(v33, led, 0)
            cur = ", ".join(f"{names[k].split()[0]} {abs(r[f'i{k}']) * 1000:.2f}" for k in range(4))
            out.append(f"+3V3 {v33} V, LED {led} at 5 mA, both GPIOs low: receivers {cur} mA; GPIO21 sinks "
                       f"{abs(r['ig21']) * 1000:.1f} mA, GPIO17 {abs(r['ig17']) * 1000:.1f} mA")
    r = circuit(3.3, "1.2 V", 1)
    out.append(f"both GPIOs high (idle): receivers {max(abs(r[f'i{k}']) for k in range(4)) * 1e6:.2f} uA at most")
    out.append("")
    r = circuit(3.47, "1.2 V", 0, ("J2 jack OUT 1", "ring"))
    out.append(f"a mono plug in J2 (its ring on the sleeve), +3V3 3.47 V: J2's source trio {r['p0'] * 1000:.0f} mW "
               f"({r['p0'] / 3 * 1000:.0f} mW each, 250 mW parts); the other receivers "
               + ", ".join(f"{names[k].split()[0]} {abs(r[f'i{k}']) * 1000:.2f}" for k in (1, 2, 3)) + " mA")
    r = circuit(3.47, "1.2 V", 0, ("J5 DIN OUT 1", "ring"))
    out.append(f"a broken DIN cable shorting J5's pin 4, 3.47 V: R54 // R58 {r['p1'] * 1000:.0f} mW ({r['p1'] / 2 * 1000:.0f} mW "
               f"each, 250 mW parts); the others " + ", ".join(
                   f"{names[k].split()[0]} {abs(r[f'i{k}']) * 1000:.2f}" for k in (0, 2, 3)) + " mA")
    r = circuit(3.47, "1.2 V", 1, ("J2 jack OUT 1", "tip"))
    out.append(f"J2's tip shorted to ground with GPIO21 high (a damaged cable): GPIO21 sources {abs(r['ig21']) * 1000:.0f} "
               "mA through R15 68 ohm (the datasheet's IOH figure is 40 mA typ, Table 5-4; sim/midi_faults.md)")
    out.append("")
    out.append("GPIO17's default drive is 10 mA (the datasheet p. 18, note 5): the firmware sets GPIO17 and GPIO21 to "
               "drive strength 2 (20 mA) or more; the sink current a pin is in the table above")
    open(os.path.join(HERE, "midi.txt"), "w").write("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
