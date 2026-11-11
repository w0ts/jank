# SPDX-License-Identifier: GPL-3.0-only
"""The schematic says what parts.py meant, the board carries the schematic's nets, and the pin rules hold. The netlist
comes from kicad-cli (make_fab.sh), the intent from fab/intended-nets.json (make_schematic.py). Exit 1 on any difference.

  .../python3.9 check_nets.py fab/chip.net.xml fab/intended-nets.json chip.kicad_pcb
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

# the pin rules (README "Pin map"): no strapping pin for an output, ADC1 for the senses, the UART TX an RTC GPIO
for name, (gpio, _pin) in P.GPIO.items():
    if gpio in P.STRAPPING and name != "BOOT":
        errors.append(f"{name} on strapping GPIO{gpio}")
for name in ("VBANK_ADC", "VBUS_SENSE"):
    if not 1 <= P.GPIO[name][0] <= 10:
        errors.append(f"{name} on GPIO{P.GPIO[name][0]}: not ADC1 (ADC2 is not usable with Wi-Fi on)")
for name in ("MIDI_TX", "PWR_HOLD"):
    if P.GPIO[name][0] > 21:
        errors.append(f"{name} on GPIO{P.GPIO[name][0]}: not an RTC GPIO (the ULP and deep-sleep hold need one)")
if P.GPIO["USB_DN"][0] != 19 or P.GPIO["USB_DP"][0] != 20:
    errors.append("USB D- / D+ must be GPIO19 / GPIO20")

for path in sys.argv[3:]:
    import pcbnew
    nb = 0
    seen = set()
    for fp in pcbnew.LoadBoard(path).GetFootprints():
        ref = fp.GetReference()
        seen.add(ref)
        for p in fp.Pads():
            num = p.GetNumber()
            if not num:
                continue
            nb += 1
            got = p.GetNetname() or None
            exp = want.get((ref, num))
            if got != exp:
                errors.append(f"{os.path.basename(path)} {ref}.{num}: {got}, intended {exp}")
    for ref in {r for r, _ in want} - seen:
        errors.append(f"{os.path.basename(path)}: {ref} not on the board")
    print(f"{os.path.basename(path)}: {nb} pads checked")
for e in errors:
    print("ERROR", e)
print(f"nets: {len(sch)} schematic pins, {len(want)} intended, {len(errors)} problems")
sys.exit(1 if errors else 0)
