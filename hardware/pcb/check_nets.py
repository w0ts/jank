# SPDX-License-Identifier: GPL-3.0-only
"""The schematic says what make_schematic.py meant, and the design rules hold. The netlist comes from kicad-cli
(make_fab.sh); the intent from fab/intended-nets.json. No board yet: when the .kicad_pcb comes, its pads get the same
check as optimist's SD board. Exit 1 on any difference or broken rule.

  python3 check_nets.py fab/jank-pcb.net.xml fab/intended-nets.json
"""
import json
import re
import sys
import xml.etree.ElementTree as ET

STRAPPING = {0, 3, 45, 46}          # ESP32-S3 datasheet v2.2, section 2.3.5 and 3
AVOID = {43: "U0TXD, 499 ohm in series on the XIAO (v1.2 schematic, R6)", 44: "U0RXD, ROM download-mode receive"}

sch = {}
for n in ET.parse(sys.argv[1]).getroot().iter("net"):
    name = n.get("name").lstrip("/")
    for node in n.iter("node"):
        if not node.get("ref").startswith("#"):
            sch[(node.get("ref"), node.get("pin"))] = None if name.startswith(("unconnected-", "Net-(")) else name
intent = json.load(open(sys.argv[2]))
want = {tuple(p.split(".")): net for net, pins in intent["nets"].items() for p in pins}

errors = []
for k in sorted(set(sch) | set(want)):
    if sch.get(k) != want.get(k):
        errors.append(f"{k[0]}.{k[1]}: schematic {sch.get(k)}, intended {want.get(k)}")
nets = {}
for k, net in sch.items():
    if net:
        nets.setdefault(net, set()).add(k)
for net, pins in sorted(nets.items()):
    if len({r for r, _ in pins}) < 2:
        errors.append(f"net {net} reaches one part only: {sorted(pins)}")


def on(net, *pins):
    have = nets.get(net, set())
    for ref, pin in pins:
        if (ref, pin) not in have:
            errors.append(f"{ref}.{pin} is not on {net}")


xiao = intent["xiao"]
tx_gpio = []
for k in range(1, 5):
    on(f"TIP{k}", (f"R1{k}", "2"), (f"J{5 + k}", "5"))
    on(f"RING{k}", (f"R2{k}", "2"), (f"J{5 + k}", "4"))
    on("GND", (f"J{1 + k}", "S"), (f"J{5 + k}", "2"))
    sw = f"SW{1 + k}"
    if (sw, "1") in sch:                        # (the long board: the A/B switch between the resistors and the jack)
        on(f"TIP{k}", (sw, "3"), (sw, "4"))
        on(f"RING{k}", (sw, "1"), (sw, "6"))
        on(f"JT{k}", (sw, "5"), (f"J{1 + k}", "T"))
        on(f"JR{k}", (sw, "2"), (f"J{1 + k}", "R"))
    else:                                        # (the square board: the jack straight on)
        on(f"TIP{k}", (f"J{1 + k}", "T"))
        on(f"RING{k}", (f"J{1 + k}", "R"))
    src = f"SRC{k}" if f"SRC{k}" in nets else "+3V3"   # (the MIDI IN channel: its source on a GPIO)
    on(src, (f"R2{k}", "1"), (f"R4{k}", "1"), (f"R8{k}", "1"))   # (v5c: R8k, the second 68 ohm, in JP2k's place)
    tx = f"MIDI_TX{k}" if f"MIDI_TX{k}" in nets or "MIDI_TXB" not in nets or k == 1 else "MIDI_TXB"   # (v5: OUT2..4 share TXB)
    on(tx, (f"R1{k}", "1"), (f"R3{k}", "1"), (f"JP1{k}", "1"))
    pads = [p for (r, p) in nets.get(tx, set()) if r == "U1"]
    if len(pads) != 1:
        errors.append(f"{tx} on XIAO pads {pads}")
        continue
    if tx == "MIDI_TXB" and k > 2:
        continue                                 # (one pad for three outputs: counted once)
    gpio = int(re.search(r"GPIO(\d+)", xiao[pads[0]]).group(1))
    tx_gpio.append(gpio)
    if gpio in STRAPPING or gpio in AVOID:
        errors.append(f"MIDI_TX{k} on GPIO{gpio}: strapping or {AVOID.get(gpio, 'avoided')}")
if len(set(tx_gpio)) != len(tx_gpio):
    errors.append(f"MIDI TX pads not distinct: {tx_gpio}")
on("+5V", ("U1", "14"), ("U2", "6"), ("U2", "3"), ("LED1", "3"), ("U3", "5"), *([("LED2", "3")] if ("LED2", "3") in sch else []))
on("+3V3", ("U1", "12"))
on("VBUS_IN", ("J1", "A9"), ("J1", "B9"), ("Q1", "2"))
on("VIN_SW", ("Q1", "3"), ("U2", "1"))
on("CC1", ("J1", "A5"), ("R1", "1"))
on("CC2", ("J1", "B5"), ("R2", "1"))
if ("LED2", "2") in sch:                         # (v4: two LEDs in a chain; v5: one, its DOUT open)
    on("LED_DIN2", ("LED1", "4"), ("LED2", "2"))
else:
    on("LED_DIN1", ("LED1", "2"), ("R8", "2"))
    on("GND", ("LED1", "1"))
    if sch.get(("LED1", "4")):
        errors.append("LED1.4 (DOUT) should be open: one LED")
if "SDA" in nets:                                # (v5: the OLED on I2C, pulled up on the board)
    on("SDA", ("J10", "4"), ("R71", "2"))
    on("SCL", ("J10", "3"), ("R70", "2"))
    on("+3V3", ("J10", "2"), ("R70", "1"), ("R71", "1"))
    on("GND", ("J10", "1"))
    for name in ("SDA", "SCL"):
        pads = [p for (r, p) in nets.get(name, set()) if r == "U1"]
        if len(pads) != 1:
            errors.append(f"{name} on XIAO pads {pads}")
            continue
        gpio = int(re.search(r"GPIO(\d+)", xiao[pads[0]]).group(1))
        if gpio in STRAPPING or gpio in AVOID:
            errors.append(f"{name} on GPIO{gpio}: strapping or {AVOID.get(gpio, 'avoided')}")
if "MIDI_RX" in nets:                           # (the long board: one output also a MIDI IN)
    k_in = [k for k in range(1, 5) if f"SRC{k}" in nets]
    if len(k_in) != 1:
        errors.append(f"MIDI IN: SRC nets on channels {k_in}")
    else:
        k = k_in[0]
        on(f"RING{k}", ("R50", "1"))
        on("OPTO_A", ("R50", "2"), ("U4", "1"), ("D2", "1"))
        on("OPTO_K", ("U4", "2"), ("D2", "2"), ("Q3", "3"))
        on(f"TIP{k}", ("Q3", "2"))
        on("IN_G", ("Q3", "1"), ("R52", "2"))
        on("IN_EN", ("R52", "1"), ("R53", "1"))
        on("MIDI_RX", ("U4", "4"), ("R51", "2"))
        on("+3V3", ("U4", "6"), ("R51", "1"))
        on("GND", ("U4", "5"), ("R53", "2"))
        if ("Q4", "3") in sch:                     # (round 7: the source through Q4, off while IN_EN is high)
            on(f"SRC{k}", ("Q4", "3"))
            on("+3V3", ("Q4", "2"))
            on("IN_EN", ("Q4", "1"))
        for name in ("IN_EN", "MIDI_RX") if ("Q4", "3") in sch else (f"SRC{k}", "IN_EN", "MIDI_RX"):
            pads = [p for (r, p) in nets.get(name, set()) if r == "U1"]
            if len(pads) != 1:
                errors.append(f"{name} on XIAO pads {pads}")
                continue
            gpio = int(re.search(r"GPIO(\d+)", xiao[pads[0]]).group(1))
            if gpio in STRAPPING or (gpio in AVOID and name != f"SRC{k}"):   # (SRC on U0RXD: an output, said in README)
                errors.append(f"{name} on GPIO{gpio}: strapping or {AVOID.get(gpio, 'avoided')}")

if "CLK_L" in nets:                              # (the modular variant: CLOCK / RESET on both side walls, README "Modular variant")
    on("SDA", ("U5", "15"))
    on("SCL", ("U5", "14"))
    on("+3V3", ("U5", "16"), ("C12", "1"))
    on("GND", ("U5", "1"), ("U5", "2"), ("U5", "3"), ("U5", "8"))   # (A0..A2 low: address 0x20)
    for out, inp, buf, u_, r_, d_, j_, c_, pin in (("CLK_L", "CLKL_IN", "CLKL_B", "U6", "R72", "D3", "J11", "C13", "4"),
                                                   ("RST_L", "RSTL_IN", "RSTL_B", "U7", "R73", "D4", "J12", "C14", "5"),
                                                   ("CLK_R", "CLKR_IN", "CLKR_B", "U8", "R74", "D5", "J13", "C15", "7"),   # (P3; v5c: the south row, see make_board.py),
                                                   ("RST_R", "RSTR_IN", "RSTR_B", "U9", "R75", "D6", "J14", "C16", "6")):
        on(inp, ("U5", pin), (u_, "2"))
        on(buf, (u_, "4"), (r_, "1"))
        on(out, (r_, "2"), (d_, "3"), (j_, "T"))
        on("+5V", (u_, "5"), (d_, "2"), (c_, "1"))
        on("GND", (u_, "1"), (u_, "3"), (d_, "1"), (j_, "S"), (c_, "2"))
        if sch.get((j_, "R")):
            errors.append(f"{j_}.R should be open (mono)")

if "LADDER" in nets:                             # (round 7: the controls: one ADC ladder, the encoder's A / B)
    on("LADDER", ("U1", "3"), ("SW1", "1"), ("SW6", "1"), ("SW7", "S2"), ("R60", "2"), ("C9", "1"))
    on("+3V3", ("R60", "1"), ("R64", "1"), ("R65", "1"))
    ok, back = ("OK_R", "BACK_R") if "OK_R" in nets else ("MODE_R", "PLAY_R")   # (v5: the buttons are OK and BACK)
    on(ok, ("SW1", "2"), ("R61", "1"))
    on(back, ("SW6", "2"), ("R62", "1"))
    on("PUSH_R", ("SW7", "S1"), ("R63", "1"))
    on("ENC_A", ("U1", "7"), ("SW7", "A"), ("R64", "2"), ("C10", "1"))
    on("ENC_B", ("U1", "8"), ("SW7", "B"), ("R65", "2"), ("C11", "1"))
    on("GND", ("SW7", "C"), ("R61", "2"), ("R62", "2"), ("R63", "2"))

if len(sys.argv) > 3:                            # the board: every pad's net = its schematic pin's (needs KiCad's Python)
    import pcbnew
    on_board = set()
    nboard = 0
    for fp in pcbnew.LoadBoard(sys.argv[3]).GetFootprints():
        ref = fp.GetReference()
        on_board.add(ref)
        for p in fp.Pads():
            num, got = p.GetNumber(), p.GetNetname() or None
            if not num:                          # (NPTH: the DIN's pegs, the mounting holes)
                continue
            nboard += 1
            exp = "GND" if (ref.startswith("J") and num == "E") else sch.get((ref, num))   # (the DIN's shield pins)
            if num in ("T", "R", "S") and ref in ("J6", "J7", "J8", "J9"):   # (the jack inside: J2..J5's pins)
                exp = sch.get((f"J{int(ref[1:]) - 4}", num))
            if got != exp:
                errors.append(f"board {ref}.{num}: {got}, schematic {exp}")
    for ref, num in sch:
        if ref not in on_board and ref not in ("J2", "J3", "J4", "J5"):   # (the TRS lines: inside J6..J9's footprint)
            errors.append(f"{ref} is in the schematic but not on the board")
    print(f"board: {nboard} pads checked against the schematic")

print("XIAO pad -> net:")
for pad in sorted(xiao, key=int):
    print(f"  pad {pad:>2} {xiao[pad]:10} -> {sch.get(('U1', pad)) or '(open)'}")
for e in errors:
    print("ERROR", e)
print(f"nets: {len(sch)} pins in the schematic, {len(want)} intended, {len(nets)} nets, {len(errors)} problems")
sys.exit(1 if errors else 0)
