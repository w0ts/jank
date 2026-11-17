# SPDX-License-Identifier: GPL-3.0-only
"""The schematic says what parts.py meant, the board carries the schematic's nets, and the pin rules hold. The netlist
comes from kicad-cli (make_fab.sh), the intent from fab/intended-nets.json (make_schematic.py). Exit 1 on any difference.

  .../python3.9 check_nets.py fab/mini2.net.xml fab/intended-nets.json mini2.kicad_pcb
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
for name in ("LINE_IN", "LINE_SENSE", "VBAT_ADC"):   # (2026-11-15: VBUS_SENSE is a digital input now)
    if name in P.GPIO and not 1 <= P.GPIO[name][0] <= 10:
        errors.append(f"{name} on GPIO{P.GPIO[name][0]}: not ADC1 (ADC2 is not usable with Wi-Fi on)")
# 2026-11-15 (the module, the GPIO re-map): the outputs that must stay idle through power-up and reset only on pins with
# no power-up glitch (ESP32-S3 datasheet v2.2 Table 2-2: GPIO1 .. 20) and no reset pull (GPIO39, Table 2-1 note 7),
# not a strapping, flash or USB pin. (Was: MIDI on an RTC GPIO for the ULP / deep-sleep hold: no ULP here, and each
# MIDI gate has its 100 k pull-down, R124 / R125, that holds the loop off while the pin floats in sleep)
for name in ("MIDI_TX", "MIDI_TX2", "CLK_IO", "RST_IO", "HOST_EN", "LED_DATA", "V5_EN_IO"):
    if name in P.GPIO and P.GPIO[name][0] not in (21, 35, 36, 37, 38, 40, 41, 42, 47, 48):
        errors.append(f"{name} on GPIO{P.GPIO[name][0]}: a power-up glitch or a reset pull (datasheet Tables 2-1 / 2-2)")
# GPIO0's strap (the user's rule, 2026-11-14): on the BOOT net only the chip's pin, one pull-up and switches; nothing
# that delays or weakens the level at reset (no capacitor, no other driver)
boot = sorted({p.split(".")[0] for p in P.nets()["BOOT"]})
if [r for r in boot if not (r == "U1" or r.startswith(("SW", "J12", "C82")))] != ["R16"]:   # (stage 2: the recovery pad, the DNP debounce pad)
    errors.append(f"BOOT (GPIO0's strap) carries {boot}: only U1, R16 (the pull-up) and switches allowed")
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
