# SPDX-License-Identifier: GPL-3.0-only
"""The schematic says what parts.py meant, and the pin rules hold for the LCD-2 add-on. The netlist from kicad-cli,
the intent from fab/intended-nets.json (make_schematic.py), the LCD-2's rows from parts.LCD2_P1 / LCD2_P2 (Waveshare's
schematic). Exit 1 on any difference.

  python3 check_nets.py fab/lcd2.net.xml fab/intended-nets.json
"""
import json
import os
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import parts as P

errors = []
sch = {}
for n in ET.parse(sys.argv[1]).getroot().iter("net"):
    name = n.get("name").lstrip("/")
    for node in n.iter("node"):
        if not node.get("ref").startswith("#"):
            sch[(node.get("ref"), node.get("pin"))] = None if name.startswith(("unconnected-", "Net-(")) else name
want = {tuple(p.split(".")): net for net, pins in json.load(open(sys.argv[2]))["nets"].items() for p in pins}
for k in sorted(set(sch) | set(want)):
    if sch.get(k) != want.get(k):
        errors.append(f"{k[0]}.{k[1]}: schematic {sch.get(k)}, intended {want.get(k)}")

# 1. every jank function's connector pin carries the GPIO parts.GPIO names (the LCD-2's rows, Waveshare's PinOut)
rows = {"J15": P.LCD2_P1, "J16": P.LCD2_P2}
for name, (gpio, at) in P.GPIO.items():
    ref, pin = at.split(".")
    if rows[ref][int(pin)] != f"IO{gpio}":
        errors.append(f"{name}: {at} is the LCD-2's {rows[ref][int(pin)]}, not IO{gpio}")
    if want.get((ref, pin)) != name:
        errors.append(f"{name}: {at} carries {want.get((ref, pin))}")
# 2. power pins: the LCD-2's 3V3 / GND / 5V / VBAT on ours
POWER = {"3V3": "+3V3", "GND": "GND", "5V": "VBUS_B", "VBAT": "VBAT"}
for ref, row in rows.items():
    for pin, what in row.items():
        net = want.get((ref, str(pin)))
        if what in POWER and net != POWER[what]:
            errors.append(f"{ref}.{pin} (the LCD-2's {what}) on {net}, not {POWER[what]}")
        if what in ("IO43", "IO44") and net:
            errors.append(f"{ref}.{pin}: UART0 ({what}) must stay free, carries {net}")
        if what in ("IO19", "IO20") and net:
            errors.append(f"{ref}.{pin}: USB {what} carries {net} (the S3's USB goes through its own USB-C)")
# 2b. nothing of ours may drive the LCD-2's VBUS: J1's VBUS / CC pins open, VBUS_B only on loads, D3's anode side, Q1's gate
for p_ in ("A4B9", "B4A9", "A5", "B5"):
    if want.get(("J1", p_)):
        errors.append(f"J1.{p_} carries {want[('J1', p_)]}: the loop cable would join it to the LCD-2's USB-C")
# 3. pin rules (ESP32-S3 datasheet v2.2 Tables 2-1 / 2-2; the LCD-2's own pulls, parts.LCD2_PULL)
for name, (gpio, _at) in P.GPIO.items():
    if gpio in P.STRAPPING:
        errors.append(f"{name} on strapping GPIO{gpio}")
for name in P.ADC1:
    if not 1 <= P.GPIO[name][0] <= 10:
        errors.append(f"{name} on GPIO{P.GPIO[name][0]}: not ADC1")
for name in P.OUTPUTS:
    g = P.GPIO[name][0]
    if g == 18 or not 1 <= g <= 17:      # GPIO1..17: only a LOW 60 us glitch at power-up; GPIO18: a HIGH glitch
        errors.append(f"{name} on GPIO{g}: not a low-glitch pin")
    if "up" in P.LCD2_PULL.get(g, ""):
        errors.append(f"{name} on GPIO{g}: the LCD-2 pulls it up ({P.LCD2_PULL[g]}): high through reset")
# 4. every output that must idle low has a pull-down on our side or the LCD-2's (HOST_EN: R91 and the LCD-2's R6)
for name in ("MIDI_TX", "MIDI_TX2", "CLK_IO", "RST_IO", "HOST_EN"):
    downs = [ref for ref, kind, _v, _f, _c, pins in P.PARTS
             if kind == "R" and P.fitted(ref) and sorted(pins.values()) == sorted([name, "GND"])]
    if not downs:
        errors.append(f"{name}: no fitted pull-down to GND")
# 5. the 5 V rail's set point under the clamp on every board (1 % parts, VREF 580..610 mV, CJ431 2.475..2.525 V)
r = {p[0]: p[2] for p in P.PARTS}
val = lambda s: float(s.replace("k", "e3").replace("M", "e6").split()[0])
lo = val(r["R135"]) + val(r["R154"]) + val(r["R136"])
v5_max = 0.610 * (1 + val(r["R134"]) * 1.01 / (lo * 0.99))
clamp_min = 2.475 * (1 + val(r["R138"]) * 0.99 / (val(r["R139"]) * 1.01))
if v5_max >= clamp_min:
    errors.append(f"boost max {v5_max:.3f} V reaches the clamp's min {clamp_min:.3f} V")
print(f"boost set point max {v5_max:.3f} V, clamp min {clamp_min:.3f} V")
for e in errors:
    print("ERROR", e)
print(f"nets: {len(sch)} schematic pins, {len(want)} intended, {len(errors)} problems")
sys.exit(1 if errors else 0)
