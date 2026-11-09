# SPDX-License-Identifier: GPL-3.0-only
"""The jank board's schematic (jank-pcb.kicad_sch), its project file, the XIAO symbol (lib/jank.kicad_sym), the intended
nets (fab/intended-nets.json, read by check_nets.py) and BOM.md. Every pin carries a net label (no wires to draw), so
the drawing reads as the netlist and KiCad's ERC checks it, as in optimist's hardware/archive-v6-tested.

  python3 make_schematic.py        (then sh make_fab.sh runs it, the ERC, the netlist check and the PDF)
"""
import json
import os
import re
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
SYMS = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols"
NAME = "jank-pcb"
NS = uuid.UUID("3b8e5c1a-7d2f-4a6b-9e0c-5f4d3a2b1c09")   # (fixed: the files are the same each run)
ROOT = str(uuid.uuid5(NS, "root"))
DATE = "2026-11-08"


def uid(*key):
    return str(uuid.uuid5(NS, "/".join(map(str, key))))


def sexpr(text, i):
    """the balanced s-expression starting at text[i] == '('"""
    depth = 0
    for j in range(i, len(text)):
        depth += text[j] == "("
        depth -= text[j] == ")"
        if depth == 0:
            return text[i:j + 1]
    raise ValueError(text[i:i + 40])


def block(text, name):
    """the top-level (symbol "name" ...) of a .kicad_sym"""
    return sexpr(text, text.index(f'(symbol "{name}"'))


def properties(sym):
    """{name: full (property ...) text} of a symbol's own top-level properties"""
    out = {}
    for m in re.finditer(r'\(property "([^"]+)"', sym):
        out.setdefault(m.group(1), sexpr(sym, m.start()))
    return out


# ---- the XIAO ESP32S3 symbol: pads numbered as Seeed's footprint (1 = D0 at the USB end, 8 = D7 at the far end) ----
XIAO_LEFT = ["D0/GPIO1", "D1/GPIO2", "D2/GPIO3", "D3/GPIO4", "D4/GPIO5", "D5/GPIO6", "D6/GPIO43"]   # pads 1..7
XIAO_RIGHT = {14: "5V", 13: "GND", 12: "3V3", 11: "D10/GPIO9", 10: "D9/GPIO8", 9: "D8/GPIO7", 8: "D7/GPIO44"}
XIAO_TYPE = {14: "passive", 13: "power_in", 12: "power_out"}


def xiao_symbol(name):
    f = '(effects (font (size 1.27 1.27)))'
    pins = []
    for i, n in enumerate(XIAO_LEFT):
        pins.append(f'(pin bidirectional line (at -20.32 {7.62 - 2.54 * i:.2f} 0) (length 5.08) (name "{n}" {f}) (number "{i + 1}" {f}))')
    for k, (num, n) in enumerate(XIAO_RIGHT.items()):
        pins.append(f'(pin {XIAO_TYPE.get(num, "bidirectional")} line (at 20.32 {7.62 - 2.54 * k:.2f} 180) (length 5.08)'
                    f' (name "{n}" {f}) (number "{num}" {f}))')
    short = name.split(":")[-1]
    return (f'(symbol "{name}" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)'
            f' (property "Reference" "U" (at 0 12.7 0) {f})'
            f' (property "Value" "XIAO_ESP32S3" (at 0 -12.7 0) {f})'
            f' (property "Footprint" "jank:XIAO-ESP32-S3-DIP" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'
            f' (property "Datasheet" "https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32S3/res/XIAO_ESP32S3_SCH_v1.2.pdf"'
            f' (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'
            f' (property "Description" "Seeed XIAO ESP32S3 (plain), 14 edge pads; 5V = the USB-C socket\'s VBUS (v1.2 schematic)"'
            f' (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'
            f' (symbol "{short}_0_1" (rectangle (start -15.24 10.16) (end 15.24 -10.16) (stroke (width 0.254) (type default))'
            f' (fill (type background))))'
            f' (symbol "{short}_1_1" {" ".join(pins)}))')


def dpdt_symbol(name):
    """a DPDT slide switch, one unit: pins 1-2-3 (common 2) and 4-5-6 (common 5); 1-2 / 4-5 in one position"""
    f = '(effects (font (size 1.27 1.27)))'
    pins = [f'(pin passive line (at -7.62 {y} 0) (length 2.54) (name "{n}" {f}) (number "{n}" {f}))'
            for n, y in (("1", 5.08), ("2", 2.54), ("3", 0.0))]
    pins += [f'(pin passive line (at 7.62 {y} 180) (length 2.54) (name "{n}" {f}) (number "{n}" {f}))'
             for n, y in (("4", 5.08), ("5", 2.54), ("6", 0.0))]
    short = name.split(":")[-1]
    return (f'(symbol "{name}" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)'
            f' (property "Reference" "SW" (at 0 8.89 0) {f})'
            f' (property "Value" "DPDT" (at 0 -3.81 0) {f})'
            f' (property "Footprint" "jank:SW_Slide_DPDT_MSS22D18" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'
            f' (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'
            f' (property "Description" "DPDT slide switch: 2 = common of 1/3, 5 = common of 4/6; 1-2 and 4-5 closed in one position"'
            f' (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'
            f' (symbol "{short}_0_1" (rectangle (start -5.08 6.35) (end 5.08 -1.27) (stroke (width 0.254) (type default))'
            f' (fill (type background))))'
            f' (symbol "{short}_1_1" {" ".join(pins)}))')


def lib_symbol(lib_id):
    """the symbol for the schematic's cache, a derived one flattened onto its parent"""
    lib, name = lib_id.split(":")
    if lib == "jank":
        return dpdt_symbol(lib_id) if name.startswith("SW_DPDT") else xiao_symbol(lib_id)
    text = open(os.path.join(SYMS, f"{lib}.kicad_sym")).read()
    b = block(text, name)
    ext = re.search(r'\(extends "([^"]+)"\)', b)
    if not ext:
        return b.replace(f'(symbol "{name}"', f'(symbol "{lib_id}"', 1)
    parent = ext.group(1)
    out = block(text, parent).replace(f'(symbol "{parent}"', f'(symbol "{lib_id}"', 1)
    out = out.replace(f'(symbol "{parent}_', f'(symbol "{name}_')
    mine = properties(out)
    for prop, text_ in properties(b).items():   # (the derived symbol's own fields win, missing ones are added)
        if prop in mine:
            out = out.replace(mine[prop], text_, 1)
        else:
            k = out.index('(symbol "', 1)
            out = out[:k] + text_ + "\n" + out[k:]
    return out


def pins_of(sym):
    """{number: (x, y, angle, type, hidden)} of a cached symbol"""
    pins = {}
    for m in re.finditer(r'\(pin (\w+) \w+\s*\(at ([-\d.]+) ([-\d.]+) (\d+)\)(.*?)\(number "([^"]*)"', sym, re.S):
        pins.setdefault(m.group(6), (float(m.group(2)), float(m.group(3)), int(m.group(4)), m.group(1),
                                     "(hide yes)" in m.group(5)))
    return pins


# ---- footprints ----
R0805 = "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder"
R1206 = "Resistor_SMD:R_1206_3216Metric_Pad1.30x1.75mm_HandSolder"
R2010 = "Resistor_SMD:R_2010_5025Metric_Pad1.40x2.65mm_HandSolder"
C0805 = "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder"
SOT23 = "Package_TO_SOT_SMD:SOT-23"
AX0207 = "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical"   # (axial, standing: hand-swappable)
AX0309 = "Resistor_THT:R_Axial_DIN0309_L9.0mm_D3.2mm_P2.54mm_Vertical"
JP = "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm"

# ---- the parts: (ref, lib_id, value, footprint, bom key, x, y, {pin: net}) on an A3 sheet, mm ----
PARTS = [
    # power in: a power-only USB-C, its TVS, the cut switch (Q1, Q2) and the ideal diode (U2) onto the XIAO's 5V
    ("J1", "Connector:USB_C_Receptacle_PowerOnly_6P", "USB-C power only",
     "Connector_USB:USB_C_Receptacle_GCT_USB4125-xx-x-0190_6P_TopMnt_Horizontal", "usbc", 35.56, 76.2,
     {"A9": "VBUS_IN", "B9": "VBUS_IN", "A5": "CC1", "B5": "CC2", "A12": "GND", "B12": "GND", "SH": "GND"}),
    ("R1", "Device:R", "5.1k", R0805, "r5k1", 76.2, 96.52, {"1": "CC1", "2": "GND"}),
    ("R2", "Device:R", "5.1k", R0805, "r5k1", 86.36, 96.52, {"1": "CC2", "2": "GND"}),
    ("D1", "Device:D_Zener", "SMF5.0A", "Diode_SMD:D_SOD-123F", "tvs", 76.2, 63.5, {"1": "VBUS_IN", "2": "GND"}),
    ("C1", "Device:C", "10uF", C0805, "c10u", 96.52, 63.5, {"1": "VBUS_IN", "2": "GND"}),
    ("Q1", "Transistor_FET:AO3401A", "AO3401A", SOT23, "pfet", 119.38, 63.5, {"1": "PG", "2": "VBUS_IN", "3": "VIN_SW"}),
    ("R3", "Device:R", "100k", R0805, "r100k", 134.62, 63.5, {"1": "VBUS_IN", "2": "PG"}),
    ("Q2", "Transistor_FET:AO3400A", "AO3400A", SOT23, "nfet", 119.38, 96.52, {"1": "NG", "2": "GND", "3": "PG"}),
    ("R4", "Device:R", "100k", R0805, "r100k", 134.62, 96.52, {"1": "VBUS_IN", "2": "NG"}),
    ("R5", "Device:R", "100k", R0805, "r100k", 144.78, 96.52, {"1": "NG", "2": "GND"}),
    ("C2", "Device:C", "100nF", C0805, "c100n", 154.94, 96.52, {"1": "NG", "2": "GND"}),
    ("R6", "Device:R", "1k", R0805, "r1k", 165.1, 96.52, {"1": "PWR_CUT", "2": "NG"}),
    ("U2", "Power_Management:LM66100DCK", "LM66100DCK", "Package_TO_SOT_SMD:SOT-363_SC-70-6", "idiode", 160.02, 63.5,
     {"1": "VIN_SW", "2": "GND", "3": "+5V", "6": "+5V"}),
    ("C3", "Device:C", "10uF", C0805, "c10u", 182.88, 63.5, {"1": "+5V", "2": "GND"}),
    ("C4", "Device:C_Polarized", "100uF DNP", "Capacitor_SMD:CP_Elec_5x5.4", "cbulk", 193.04, 63.5, {"1": "+5V", "2": "GND"}),
    # the module, the button, the test points
    ("U1", "jank:XIAO_ESP32S3", "XIAO ESP32S3", "jank:XIAO-ESP32-S3-DIP", "xiao", 289.56, 63.5,
     {"1": "MIDI_TX1", "2": "MIDI_TXB", "4": "LED_DATA", "5": "IN_EN", "6": "MIDI_RX", "3": "LADDER", "7": "ENC_A", "8": "ENC_B", "9": "SDA", "10": "SCL", "11": "PWR_CUT",
      "12": "+3V3", "13": "GND", "14": "+5V"}),
    ("SW1", "Switch:SW_Push", "OK 9.5 mm", "Button_Switch_THT:SW_PUSH_6mm", "sw", 360.68, 60.96, {"1": "LADDER", "2": "OK_R"}),
    ("TP1", "Connector:TestPoint", "5V", "TestPoint:TestPoint_Pad_D1.5mm", "", 337.82, 88.9, {"1": "+5V"}),
    ("TP2", "Connector:TestPoint", "3V3", "TestPoint:TestPoint_Pad_D1.5mm", "", 350.52, 88.9, {"1": "+3V3"}),
    ("TP3", "Connector:TestPoint", "GND", "TestPoint:TestPoint_Pad_D1.5mm", "", 363.22, 88.9, {"1": "GND"}),
    ("TP4", "Connector:TestPoint", "TX", "TestPoint:TestPoint_Pad_D1.5mm", "", 375.92, 88.9, {"1": "MIDI_TX1"}),
    # the two RGB LEDs: a 5 V buffer for the 3.3 V data, 330 ohm, then the chain
    ("R7", "Device:R", "100k", R0805, "r100k", 33.02, 154.94, {"1": "LED_DATA", "2": "GND"}),
    ("U3", "74xGxx:74AHCT1G125", "74AHCT1G125", "Package_TO_SOT_SMD:SOT-23-5_HandSoldering", "buf", 66.04, 154.94,
     {"1": "GND", "2": "LED_DATA", "3": "GND", "4": "LED_BUF", "5": "+5V"}),
    ("C5", "Device:C", "100nF", C0805, "c100n", 91.44, 154.94, {"1": "+5V", "2": "GND"}),
    ("R8", "Device:R", "330", AX0207, "r330", 104.14, 154.94, {"1": "LED_BUF", "2": "LED_DIN1"}),
    ("JP8", "Jumper:SolderJumper_2_Open", "bypass", JP, "", 106.68, 175.26, {"1": "LED_BUF", "2": "LED_DIN1"}),
    ("R9", "Device:R", "DNP parallel", R1206, "rpar", 116.84, 154.94, {"1": "LED_BUF", "2": "LED_DIN1"}),
    ("LED1", "LED:SK6812", "SK6812MINI-HS", "jank:LED_SK6812MINI-HS_3.7x3.5mm_HandSolder", "led", 132.08, 154.94,
     {"1": "GND", "2": "LED_DIN1", "3": "+5V"}),
    ("C6", "Device:C", "100nF", C0805, "c100n", 147.32, 154.94, {"1": "+5V", "2": "GND"}),
    # the screen (v5c, the user 2026-11-09): a 0.96 inch 128 x 64 OLED module (SSD1315, SSD1306 commands, I2C 0x3C) on a
    # 4-pin header, 3V3; SDA / SCL pulled up here. SDA on the XIAO's D8 (GPIO7), SCL on D9 (GPIO8): swapped from v5 for the
    # layout (the S3's I2C goes on any pin)
    ("J10", "Connector_Generic:Conn_01x04", "OLED 0.96in 128x64", "jank:OLED_0.96in_128x64_I2C_HS96L03W2C03", "oled",
     175.26, 154.94, {"1": "GND", "2": "+3V3", "3": "SCL", "4": "SDA"}),
    ("R70", "Device:R", "10k", R0805, "r10k", 195.58, 147.32, {"1": "+3V3", "2": "SCL"}),
    ("R71", "Device:R", "10k", R0805, "r10k", 205.74, 147.32, {"1": "+3V3", "2": "SDA"}),
    ("#FLG01", "power:PWR_FLAG", "PWR_FLAG", "", "", 220.98, 132.08, {"1": "GND"}),
    # OUT1 as MIDI IN (the user's ruling, 2026-11-08): an H11L1 across DIN pins 4/5, gated by Q3 (on only in IN mode)
    ("R50", "Device:R", "220", R0805, "r220", 30.48, 320.04, {"1": "RING1", "2": "OPTO_A"}),
    ("D2", "Diode:1N4148W", "1N4148W", "Diode_SMD:D_SOD-123", "d4148", 50.8, 337.82, {"1": "OPTO_A", "2": "OPTO_K"}),
    ("U4", "Isolator:H11L1", "H11L1", "Package_DIP:DIP-6_W7.62mm", "opto", 76.2, 330.2,
     {"1": "OPTO_A", "2": "OPTO_K", "4": "MIDI_RX", "5": "GND", "6": "+3V3"}),
    ("Q3", "Transistor_FET:AO3400A", "AO3400A", SOT23, "nfet", 50.8, 360.68, {"1": "IN_G", "2": "TIP1", "3": "OPTO_K"}),
    ("R52", "Device:R", "1k", R0805, "r1k", 30.48, 360.68, {"1": "IN_EN", "2": "IN_G"}),
    ("R53", "Device:R", "100k", R0805, "r100k", 17.78, 360.68, {"1": "IN_EN", "2": "GND"}),
    ("R51", "Device:R", "10k", R0805, "r10k", 101.6, 320.04, {"1": "+3V3", "2": "MIDI_RX"}),
    ("C8", "Device:C", "100nF", C0805, "c100n", 114.3, 320.04, {"1": "+3V3", "2": "GND"}),
    # OUT1's source (the user's ruling, round 7): 3V3 through Q4, off while IN_EN is high (MIDI IN); R53 holds IN_EN low
    ("Q4", "Transistor_FET:AO3401A", "AO3401A", SOT23, "pfet", 139.7, 360.68, {"1": "IN_EN", "2": "+3V3", "3": "SRC1"}),
    # the controls (face side): MODE (SW1), PLAY/STOP (SW6) and the encoder's push on one ADC ladder (GPIO3); the encoder's
    # A / B on GPIO43 / GPIO44 with 10k pull-ups and 100 nF
    ("R60", "Device:R", "10k", R0805, "r10k", 160.02, 320.04, {"1": "+3V3", "2": "LADDER"}),
    ("C9", "Device:C", "100nF", C0805, "c100n", 172.72, 320.04, {"1": "LADDER", "2": "GND"}),
    ("R61", "Device:R", "2.2k", R0805, "r2k2", 185.42, 342.9, {"1": "OK_R", "2": "GND"}),
    ("SW6", "Switch:SW_Push", "BACK 9.5 mm", "Button_Switch_THT:SW_PUSH_6mm", "sw", 203.2, 320.04,
     {"1": "LADDER", "2": "BACK_R"}),
    ("R62", "Device:R", "6.8k", R0805, "r6k8", 215.9, 342.9, {"1": "BACK_R", "2": "GND"}),
    ("SW7", "Device:RotaryEncoder_Switch", "EC11E09244BS", "Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm",
     "enc", 254.0, 325.12, {"A": "ENC_A", "B": "ENC_B", "C": "GND", "S1": "PUSH_R", "S2": "LADDER"}),
    ("R63", "Device:R", "22k", R0805, "r22k", 279.4, 342.9, {"1": "PUSH_R", "2": "GND"}),
    ("R64", "Device:R", "10k", R0805, "r10k", 228.6, 365.76, {"1": "+3V3", "2": "ENC_A"}),
    ("R65", "Device:R", "10k", R0805, "r10k", 241.3, 365.76, {"1": "+3V3", "2": "ENC_B"}),
    ("C10", "Device:C", "100nF", C0805, "c100n", 254.0, 365.76, {"1": "ENC_A", "2": "GND"}),
    ("C11", "Device:C", "100nF", C0805, "c100n", 266.7, 365.76, {"1": "ENC_B", "2": "GND"}),
    ("#FLG02", "power:PWR_FLAG", "PWR_FLAG", "", "", 236.22, 132.08, {"1": "VBUS_IN"}),
    ("#FLG03", "power:PWR_FLAG", "PWR_FLAG", "", "", 251.46, 132.08, {"1": "VIN_SW"}),
]
# four MIDI outputs, each with both footprints: a 3.5 mm TRS type A (CUI SJ1-3523N, right angle) and a 180 degree DIN-5
TX = {1: "MIDI_TX1", 2: "MIDI_TXB", 3: "MIDI_TXB", 4: "MIDI_TXB"}   # (v5: OUT2..OUT4 share one TX pad)
for k in range(1, 5):
    x0 = 25.4 + 96.52 * (k - 1)
    PARTS += [
        (f"R1{k}", "Device:R", "10", AX0207, "r10", x0, 231.14, {"1": TX[k], "2": f"TIP{k}"}),
        (f"R3{k}", "Device:R", "DNP parallel", R1206, "rpar", x0 + 5.08, 231.14, {"1": TX[k], "2": f"TIP{k}"}),
        (f"R2{k}", "Device:R", "33 0.5W", AX0309, "r33", x0 + 12.7, 231.14, {"1": "+3V3" if k > 1 else "SRC1", "2": f"RING{k}"}),
        (f"R4{k}", "Device:R", "DNP parallel", R1206, "rpar", x0 + 17.78, 231.14, {"1": "+3V3" if k > 1 else "SRC1", "2": f"RING{k}"}),
        (f"JP1{k}", "Jumper:SolderJumper_2_Open", "bypass", JP, "", x0 + 2.54, 241.3, {"1": TX[k], "2": f"TIP{k}"}),
        (f"JP2{k}", "Jumper:SolderJumper_2_Open", "bypass: 3V3 direct" if k > 1 else "bypass: SRC1 direct", JP, "", x0 + 25.4, 241.3, {"1": "+3V3" if k > 1 else "SRC1", "2": f"RING{k}"}),
        (f"J{1 + k}", "Connector_Audio:AudioJack3", "TRS PJ-320D-B-SMT", "", "trs_smd",
         x0 + 35.56, 231.14, {"T": f"JT{k}", "R": f"JR{k}", "S": "GND"}),
        (f"SW{1 + k}", "jank:SW_DPDT_Slide", "TRS A/B", "jank:SW_Slide_DPDT_SS3235S_SMD" if k == 1 else "jank:SW_Slide_DPDT_MSS22D18",
         "swab_smd" if k == 1 else "swab", x0 + 45.72, 271.78,
         {"1": f"RING{k}", "2": f"JR{k}", "3": f"TIP{k}", "4": f"TIP{k}", "5": f"JT{k}", "6": f"RING{k}"}),
        (f"J{5 + k}", "Connector:DIN-5_180degree", "DIN-5 DIN-504B-M10", "jank:MIDI_OUT_PJ-320A_top_DIN-504B-M10_bottom", "din",
         x0 + 66.04, 231.14, {"5": f"TIP{k}", "4": f"RING{k}", "2": "GND"}),
    ]

# ---- the modular variant (MODULAR=1: jank-pcb-mod.*, the user's ruling 2026-11-08): CLOCK and RESET 3.5 mm mono outputs on
# both side walls, from a PCF8574 I2C expander (0x20, on the OLED's bus) through 5 V buffers, 1 k and a BAT54S clamp each
MODULAR = os.environ.get("MODULAR") == "1"
MOD_OUT = (("CLK_L", "CLKL_IN", "CLKL_B", "U6", "R72", "D3", "J11", "C13"), ("RST_L", "RSTL_IN", "RSTL_B", "U7", "R73", "D4", "J12", "C14"),
           ("CLK_R", "CLKR_IN", "CLKR_B", "U8", "R74", "D5", "J13", "C15"), ("RST_R", "RSTR_IN", "RSTR_B", "U9", "R75", "D6", "J14", "C16"))
if MODULAR:
    NAME = "jank-pcb-mod"
    PARTS += [
        ("U5", "Interface_Expansion:PCF8574T", "PCF8574T", "Package_SO:SOIC-16W_7.5x10.3mm_P1.27mm", "pcf", 30.48, 396.24,
         {"1": "GND", "2": "GND", "3": "GND", "4": "CLKL_IN", "5": "RSTL_IN", "6": "RSTR_IN", "7": "CLKR_IN", "8": "GND",
          "9": "MOD_P4", "10": "MOD_P5", "11": "MOD_P6", "12": "MOD_P7", "14": "SCL", "15": "SDA", "16": "+3V3"}),
        ("C12", "Device:C", "100nF", C0805, "c100n", 60.96, 396.24, {"1": "+3V3", "2": "GND"}),
    ]
    for i, (out, inp, buf, u, r, d, j, c) in enumerate(MOD_OUT):
        x0 = 88.9 + 76.2 * i
        PARTS += [
            (u, "74xGxx:74AHCT1G125", "74AHCT1G125", "Package_TO_SOT_SMD:SOT-23-5_HandSoldering", "buf", x0, 396.24,
             {"1": "GND", "2": inp, "3": "GND", "4": buf, "5": "+5V"}),
            (c, "Device:C", "100nF", C0805, "c100n", x0, 411.48, {"1": "+5V", "2": "GND"}),
            (r, "Device:R", "1k", R0805, "r1k", x0 + 17.78, 396.24, {"1": buf, "2": out}),
            (d, "Diode:BAT54S", "BAT54S", "Package_TO_SOT_SMD:SOT-23", "bat54s", x0 + 30.48, 396.24, {"1": "GND", "2": "+5V", "3": out}),
            (j, "Connector_Audio:AudioJack3", "3.5 mm mono " + out, "jank:Jack_3.5mm_XKB_PJ-320A_Horizontal", "trs", x0 + 45.72, 396.24,
             {"T": out, "S": "GND"}),
        ]
    for k, n in enumerate(("MOD_P4", "MOD_P5", "MOD_P6", "MOD_P7")):
        PARTS.append((f"TP{5 + k}", "Connector:TestPoint", n[4:], "TestPoint:TestPoint_Pad_D1.0mm", "", 406.4 + 12.7 * k, 411.48,
                      {"1": n}))

NOTES = [
    (20.32, 30.48, 2.0, "POWER IN: J1 power only (D+/D- not fitted). Q1 cut (on by default), U2 ideal diode onto the XIAO's 5V"),
    (20.32, 35.56, 1.27, "U2 ~CE tied to VOUT: the datasheet's ideal-diode mode (blocks current back into J1). The XIAO's 5V pin IS its USB-C VBUS (no part between,"),
    (20.32, 38.1, 1.27, "Seeed v1.2 schematic): a computer on the XIAO port and a charger on J1 share +5V. Firmware pulls PWR_CUT low once a computer has"),
    (20.32, 40.64, 1.27, "enumerated it (device mode): Q2 off, Q1 off, J1 cut. At reset PWR_CUT floats: R4/R5 hold Q2 on, so a charger alone always starts it."),
    (20.32, 43.18, 1.27, "C4 (100 uF) not fitted: host-mode hot-plug reserve, against USB's 10 uF limit for a device."),
    (238.76, 30.48, 2.0, "XIAO ESP32S3 (Seeed footprint, 2 x 7 headers or castellations)"),
    (238.76, 35.56, 1.27, "D6 = GPIO43 has 499 ohm on the module (U0TXD). D2 = GPIO3 strapping, ignored by default eFuses. ENC_A = D6 = GPIO43, ENC_B = D7 = GPIO44 (U0TXD / U0RXD), IN_EN = D4 = GPIO5, MIDI_RX = D5 = GPIO6 (v5)."),
    (238.76, 38.1, 1.27, "v5: MIDI_TX1 = D0 = GPIO1 (OUT1 alone: Hi-Z in IN mode); MIDI_TXB = D1 = GPIO2 drives OUT2, OUT3 and OUT4 together (3 loops on one pad)."),
    (238.76, 40.64, 1.27, "LADDER = D2 = GPIO3 (ADC1): OK / BACK / PUSH on one resistor ladder. LED_DATA = D3 = GPIO4. SCL = D8 = GPIO7, SDA = D9 = GPIO8 (the OLED, I2C by the GPIO matrix). PWR_CUT = D10 = GPIO9, low = J1 cut."),
    (20.32, 129.54, 2.0, "LED: one SK6812MINI-HS on +5V (top); U3 lifts the 3.3 V data to 5 V (VIH = 0.7 x VDD = 3.5 V at 5 V). SCREEN: J10, a 0.96 in 128 x 64 OLED (SSD1315, I2C 0x3C, 3V3)"),
    (20.32, 200.66, 2.0, "MIDI OUT 1-4 (CA-033 at 3.3 V), v5: BOTH fitted on every output, the TRS (type A, top) AND the DIN-5 (bottom), in parallel; plug one at a time"),
    (20.32, 205.74, 1.27, "TRS type A: tip = data (10 ohm to the TX pad), ring = source (33 ohm 0.5 W to 3V3), sleeve = GND."
                          "  DIN-5: pin 5 = tip net, pin 4 = ring net, pin 2 = GND, pins 1 and 3 open."),
]
for k in range(1, 5):
    NOTES.append((25.4 + 96.52 * (k - 1), 210.82, 1.5, f"OUT{k}: J{1 + k} TRS  and  J{5 + k} DIN"))
NOTES.append((20.32, 289.56, 1.27, "SW2..SW5: TRS type A / B per output (top side): they swap the jack's T and R between TIPk (data, sink) and RINGk"
                                   " (source) after the resistors; the DIN keeps pin 5 = TIPk, pin 4 = RINGk. 1-2 / 4-5 closed = type A."))
NOTES.append((20.32, 304.8, 2.0, "OUT1 AS MIDI IN (software): TX1 to input (Hi-Z), IN_EN high (Q4 off: no source; Q3 on), MIDI_RX as UART RX"))
NOTES.append((20.32, 309.88, 1.27, "OUT mode: IN_EN low (R53 at reset): Q4 on (3V3 to the 33 ohm), Q3 off, the opto's loop open (its body diode blocks), OUT1 drives as before."
                                   " D2 across the opto's LED (reverse protection). OUT1 sits on the BACK edge (long board)."))
NOTES.append((140.0, 304.8, 2.0, "CONTROLS (top side, by the screen): one ADC ladder on GPIO3: R60 10k to 3V3; OK 2.2k, BACK 6.8k, PUSH 22k to GND"))
NOTES.append((140.0, 309.88, 1.27, "Idle 3.30 V, OK 0.60 V, BACK 1.34 V, PUSH 2.27 V (one switch at a time). Encoder A / B: 10k to 3V3, 100 nF to GND each."))
NOTES.append((20.32, 264.16, 1.27, "Each MIDI resistor (and R8): axial, hand-swappable; JP across it = bypass (0 ohm, open by default); R3k / R4k / R9 in"
                                   " parallel, not fitted (lower the value). JP2k bridged = RING on 3V3 directly (JP21: on SRC1): a mono plug then shorts the rail."))
NOTES.append((20.32, 194.31, 1.27, "On the board each pair is ONE footprint (J6..J9, jank:MIDI_OUT_PJ-320A_top_DIN-504B-M10_bottom: the jack's pads T, R, S"
                                   " carry J2..J5's nets, its switch pin SW open; the DIN's pins on the bottom). J2..J5 are BOM lines only."))

if MODULAR:
    NOTES.append((20.32, 383.54, 2.0, "MODULAR: CLOCK / RESET out, 3.5 mm mono, left and right side walls: PCF8574T (I2C 0x20, A0..A2 low) P0 CLK_L,"
                                      " P1 RST_L, P5 CLK_R, P4 RST_R; P2 P3 P6 P7 spare on TP5..TP8"))
    NOTES.append((20.32, 388.62, 1.27, "Each: 74AHCT1G125 on +5V (TTL input: the expander's 3.3 V high is enough), 1 k in series, BAT54S from the jack's"
                                       " tip to GND and +5V; tip = signal, sleeve = GND, ring and switch open"))

LABEL_ANGLE = {0: 180, 180: 0, 90: 270, 270: 90}   # (the label away from the body)
LIB_IDS = sorted({p[1] for p in PARTS})
CACHE = {lid: lib_symbol(lid) for lid in LIB_IDS}
PINS = {lid: pins_of(sym) for lid, sym in CACHE.items()}


def prop(name, value, x, y, hide=False):
    h = " (hide yes)" if hide else ""
    return f'(property "{name}" "{value}" (at {x} {y} 0) (effects (font (size 1.27 1.27)){h}))'


def check_parts():
    seen = set()
    for ref, lib, _v, _fp, _b, _x, _y, pins in PARTS:
        assert ref not in seen, ref
        seen.add(ref)
        bad = set(pins) - set(PINS[lib])
        assert not bad, (ref, bad)


def schematic():
    out = [f'(kicad_sch (version 20250114) (generator "make_schematic.py") (generator_version "1.0")',
           f'(uuid "{ROOT}") (paper "A2")',
           f'(title_block (title "jank: XIAO ESP32S3 MIDI clock stick") (date "{DATE}") (rev "0.1 schematic for review")'
           ' (comment 1 "hardware/pcb/README.md: the power path, the pin table and every unverified item")'
           ' (comment 2 "generated by make_schematic.py; do not edit by hand"))',
           "(lib_symbols", *CACHE.values(), ")"]
    for ref, lib, value, fp, bom, x, y, pins in PARTS:
        x, y = round(round(x / 2.54) * 2.54, 2), round(round(y / 2.54) * 2.54, 2)   # (on KiCad's connection grid)
        flag = ref.startswith("#")
        dnp = "DNP" in value
        pins_txt = " ".join(f'(pin "{p}" (uuid "{uid(ref, "pin", p)}"))' for p in PINS[lib])
        info = BOM.get(bom, {})
        out.append(f'(symbol (lib_id "{lib}") (at {x} {y} 0) (unit 1) (exclude_from_sim no)'
                   f' (in_bom {"no" if flag or ref.startswith(("TP", "JP")) else "yes"}) (on_board {"no" if flag or not fp else "yes"})'
                   f' (dnp {"yes" if dnp else "no"}) (uuid "{uid(ref)}")'
                   f' {prop("Reference", ref, x, y - 12.7, flag)} {prop("Value", value, x, y + 12.7, flag)}'
                   f' {prop("Footprint", fp, x, y, True)} {prop("Datasheet", info.get("datasheet", ""), x, y, True)}'
                   f' {prop("MPN", info.get("mpn", ""), x, y, True)} {prop("Shop", info.get("code", ""), x, y, True)}'
                   f' {pins_txt} (instances (project "{NAME}" (path "/{ROOT}" (reference "{ref}") (unit 1)))))')
        for p, net in pins.items():
            px, py, ang, _t, _h = PINS[lib][p]
            lx, ly = round(x + px, 2), round(y - py, 2)
            if flag:
                out.append(f'(label "{net}" (at {lx} {ly} 0) (effects (font (size 1.27 1.27)) (justify left bottom))'
                           f' (uuid "{uid(ref, "lbl", p)}"))')
                continue
            a = LABEL_ANGLE[ang]
            just = "right" if a in (180, 270) else "left"
            out.append(f'(label "{net}" (at {lx} {ly} {a}) (effects (font (size 1.27 1.27)) (justify {just} bottom))'
                       f' (uuid "{uid(ref, "lbl", p)}"))')
        placed = {(PINS[lib][p][0], PINS[lib][p][1]) for p in pins}
        for p, (px, py, _a, typ, hidden) in PINS[lib].items():   # (the pins on no net: marked, so the ERC does not ask)
            if p not in pins and not flag and typ != "no_connect" and (px, py) not in placed:
                out.append(f'(no_connect (at {round(x + px, 2)} {round(y - py, 2)}) (uuid "{uid(ref, "nc", p)}"))')
    for i, (x, y, size, text) in enumerate(NOTES):
        bold = " (bold yes)" if size >= 1.5 else ""
        out.append(f'(text "{text}" (exclude_from_sim no) (at {x} {y} 0) (effects (font (size {size} {size}){bold})'
                   f' (justify left bottom)) (uuid "{uid("note", i)}"))')
    out.append('(sheet_instances (path "/" (page "1")))')
    out.append(")")
    return "\n".join(out) + "\n"


def symbol_lib():
    return ('(kicad_symbol_lib (version 20251024) (generator "make_schematic.py") (generator_version "1.0")\n'
            + xiao_symbol("XIAO_ESP32S3") + "\n" + dpdt_symbol("SW_DPDT_Slide") + "\n)\n")


PROJECT = {"meta": {"filename": f"{NAME}.kicad_pro", "version": 3},
           "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
           "sheets": [[ROOT, "Root"]], "text_variables": {},
           # (the board's rules, JLCPCB 2 layers: 0.15 mm track/space is fine, 0.3 mm drill; KiCad reads them from here)
           "board": {"design_settings": {"rules": {
               "min_clearance": 0.15, "min_copper_edge_clearance": 0.2, "min_hole_clearance": 0.2, "min_hole_to_hole": 0.5,
               "min_through_hole_diameter": 0.3, "min_track_width": 0.15, "min_via_annular_width": 0.1, "min_via_diameter": 0.5,
               "min_text_height": 0.6, "min_text_thickness": 0.08, "min_silk_clearance": 0.0}}},
           "net_settings": {"classes": [{"name": "Default", "clearance": 0.2, "track_width": 0.25, "via_diameter": 0.6,
                                         "via_drill": 0.3, "priority": 2147483647}], "meta": {"version": 5}}}
SYM_LIB_TABLE = '(sym_lib_table\n  (version 7)\n  (lib (name "jank")(type "KiCad")(uri "${KIPRJMOD}/lib/jank.kicad_sym")(options "")(descr "the XIAO ESP32S3, drawn for this board"))\n)\n'
FP_LIB_TABLE = '(fp_lib_table\n  (version 7)\n  (lib (name "jank")(type "KiCad")(uri "${KIPRJMOD}/lib/jank.pretty")(options "")(descr "XIAO-ESP32-S3-DIP from Seeed (CC BY-SA 4.0); the jacks come with the layout"))\n)\n'

# ---- the BOM: one price each, as read on DATE (LCSC: its own product API, the first price break, USD excl. VAT) ----
# Prices not read on 2026-11-09 (LCSC refused the page): 0.0 here, the BOM says so. Fill in from LCSC before trusting the total.
SWS_PRICE = 0.0        # XKB SS-3235S-L2 (C500056), the surface-mount A/B switch
PJD_PRICE = 0.0        # XKB PJ-320D-B-SMT (C2884940), the surface-mount output jack
OLED96_PRICE = 0.0     # Hansheng HS96L03W2C03 (C5248080), the 0.96" screen
LCSC = "LCSC"
BOM = {
    "xiao": dict(part="Seeed XIAO ESP32S3 (plain, not Sense), no headers included", mpn="113991114", shop="Seeed Studio",
                 code="seeedstudio.com XIAO-ESP32S3-p-5627", price=7.49, cur="USD", min=1,
                 datasheet="https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32S3/res/XIAO_ESP32S3_SCH_v1.2.pdf"),
    "hdr": dict(part="Pin header 1x7, 2.54 mm (the XIAO's legs: parts sit under the module)", mpn="PZ254V-11-07P",
                shop=LCSC, code="C492406", price=0.0467, cur="USD", min=10, qty=2),
    "usbc": dict(part="USB-C receptacle, power only, 6 pins + 4 through-hole shell legs", mpn="GCT USB4125-GF-A-0190",
                 shop=LCSC, code="C5246813", price=0.9178, cur="USD", min=1, datasheet="https://gct.co/files/drawings/usb4125.pdf"),
    "r5k1": dict(part="Resistor 5.1k 1% 0805", mpn="UNI-ROYAL 0805W8F5101T5E", shop=LCSC, code="C27834", price=0.0055, cur="USD", min=100),
    "tvs": dict(part="TVS 5.0 V unidirectional, SOD-123F", mpn="MDD SMF5.0A", shop=LCSC, code="C193402", price=0.0285, cur="USD", min=20),
    "c10u": dict(part="Capacitor 10uF 25V X5R 0805", mpn="Samsung CL21A106KAYNNNE", shop=LCSC, code="C15850", price=0.0362, cur="USD", min=20),
    "pfet": dict(part="P-MOSFET 30 V 4 A, SOT-23", mpn="AOS AO3401A", shop=LCSC, code="C15127", price=0.0835, cur="USD", min=5),
    "nfet": dict(part="N-MOSFET 30 V, Vgs(th) max 1.45 V (listing), SOT-23", mpn="AOS AO3400A", shop=LCSC, code="C20917", price=0.0899, cur="USD", min=5),
    "r100k": dict(part="Resistor 100k 1% 0805", mpn="UNI-ROYAL 0805W8F1003T5E", shop=LCSC, code="C149504", price=0.0060, cur="USD", min=100),
    "r1k": dict(part="Resistor 1k 1% 0805", mpn="UNI-ROYAL 0805W8F1001T5E", shop=LCSC, code="C17513", price=0.0035, cur="USD", min=100),
    "c100n": dict(part="Capacitor 100nF 50V X7R 0805 (JLCPCB basic part; price: JLCPCB's parts list)", mpn="YAGEO CC0805KRX7R9BB104", shop=LCSC, code="C49678", price=0.0205, cur="USD", min=20),
    "idiode": dict(part="Ideal diode 1.5 A, SC-70-6 (0.65 mm pitch)", mpn="TI LM66100DCKR", shop=LCSC, code="C2869734",
                   price=0.2089, cur="USD", min=5, datasheet="https://www.ti.com/lit/ds/symlink/lm66100.pdf"),
    "r10k": dict(part="Resistor 10k 1% 0805 (pull-ups: the ladder, the encoder, I2C, the opto output)", mpn="UNI-ROYAL 0805W8F1002T5E", shop=LCSC, code="C17414", price=0.0034, cur="USD", min=100),
    "r2k2": dict(part="Resistor 2.2k 1% 0805 (ladder: OK)", mpn="UNI-ROYAL 0805W8F2201T5E", shop=LCSC, code="C17520", price=0.0050, cur="USD", min=100),
    "r6k8": dict(part="Resistor 6.8k 1% 0805 (ladder: BACK)", mpn="UNI-ROYAL 0805W8F6801T5E", shop=LCSC, code="C17772", price=0.0028, cur="USD", min=100),
    "r22k": dict(part="Resistor 22k 1% 0805 (ladder: the encoder's push)", mpn="UNI-ROYAL 0805W8F2202T5E", shop=LCSC, code="C17560", price=0.0048, cur="USD", min=100),
    "enc": dict(part="Rotary encoder EC11, vertical, 18 detents / 9 pulses, push switch, 20 mm flat (D) shaft, through hole, top side",
                mpn="ALPS EC11E09244BS", shop=LCSC, code="C470742", price=2.3275, cur="USD", min=1,
                datasheet="https://datasheet.lcsc.com/datasheet/pdf/df84d275ff713f93a7d93396fef04ce5.pdf"),
    "cbulk": dict(part="Electrolytic 100uF 16V 5 x 5.4 mm (not fitted by default)", mpn="ROQANG RVT1C101M0505", shop=LCSC,
                  code="C2977550", price=0.0356, cur="USD", min=20, dnp=True),
    "sw": dict(part="Tactile switch 6 x 6 mm, through hole, 9.5 mm tall, top side (OK, BACK)", mpn="HRO K2-1102DP-L3SW-04",
               shop=LCSC, code="C445493", price=0.0633, cur="USD", min=10,
               datasheet="https://datasheet.lcsc.com/datasheet/pdf/cba7e973dea59ba592ee1d81fd516f49.pdf?productCode=C445493"),
    "pcf": dict(part="I2C GPIO expander, 8 bit, quasi-bidirectional, SOIC-16 wide (the modular variant: CLOCK / RESET; price: JLCPCB's parts list)",
                mpn="NXP PCF8574T/3", shop=LCSC, code="C7605", price=2.0313, cur="USD", min=1,
                datasheet="https://www.nxp.com/docs/en/data-sheet/PCF8574_PCF8574A.pdf"),
    "bat54s": dict(part="Schottky pair in series BAT54S, SOT-23 (the modular outputs' clamp to GND and +5V; price: JLCPCB's parts list)", mpn="Nexperia BAT54S,215",
                   shop=LCSC, code="C47546", price=0.0225, cur="USD", min=20),
    "buf": dict(part="Buffer 74AHCT1G125, SOT-23-5 (4.5 to 5.5 V)", mpn="TI SN74AHCT1G125DBVR", shop=LCSC, code="C7484",
                price=0.1009, cur="USD", min=5),
    "trs_smd": dict(part="3.5 mm stereo jack, right angle, SURFACE MOUNT (4 tabs 1.5 x 2.5, 2 pegs 1.3), plug from the front,"
                         " top side: the four MIDI outputs' jacks (J2..J5 are BOM lines; the pads are in J6..J9's footprint)."
                         " Surface-mount so nothing of it goes through the board under the neighbouring DIN bodies",
                    mpn="XKB PJ-320D-B-SMT", shop=LCSC, code="C2884940", price=PJD_PRICE, cur="USD", min=5,
                    datasheet="https://lcsc.com/datasheet/lcsc_datasheet_2110100930_XKB-Connection-PJ-320D-B-SMT_C2884940.pdf"),
    "swab_smd": dict(part="Slide switch DPDT, surface mount, 6 tabs 2.5 mm, 7.7 x 3.55 mm, knob 3.0 mm (OUT1's TRS type A / B:"
                          " it sits over the DIN's body, where a through-hole switch's pins would hit it)",
                     mpn="XKB SS-3235S-L2", shop=LCSC, code="C500056", price=SWS_PRICE, cur="USD", min=5,
                     datasheet="https://www.lcsc.com/datasheet/lcsc_datasheet_2110111230_XKB-Connection-SS-3235S-L2_C500056.pdf"),
    "swab": dict(part="Slide switch DPDT, 6 pins 2.5 mm, 9.1 x 3.5 mm, handle 1.5 mm (TRS type A / B), face side;"
                      " OUT2..4",
                 mpn="SHOU HAN MSS22D18G2", shop=LCSC, code="C431542", price=0.0996, cur="USD", min=5,
                 datasheet="https://datasheet.lcsc.com/datasheet/pdf/257b9c94564ec11493db9cf754f9732e.pdf?productCode=C431542"),
    "opto": dict(part="Optocoupler H11L1, Schmitt trigger, open collector, DIP-6 (MIDI IN on OUT1)", mpn="EVERLIGHT H11L1",
                 shop=LCSC, code="C78588", price=0.3507, cur="USD", min=5,
                 datasheet="https://en.everlight.com/wp-content/plugins/ItemRelationship/product_files/pdf/H11LX.pdf"),
    "d4148": dict(part="Diode 1N4148W, SOD-123 (the opto LED's reverse protection)", mpn="Semtech 1N4148W", shop=LCSC,
                  code="C81598", price=0.0125, cur="USD", min=50),
    "r220": dict(part="Resistor 220 1% 0805 (MIDI IN, CA-033 / MIDI 1.0)", mpn="UNI-ROYAL 0805W8F2200T5E", shop=LCSC,
                 code="C17557", price=0.0030, cur="USD", min=100),
    "r330": dict(part="Resistor 330 1% 0.25 W axial (6.3 x 2.4 mm), standing", mpn="YAGEO MFR-25FBF52-330R", shop=LCSC,
                 code="C1369900", price=0.0205, cur="USD", min=50),
    "rpar": dict(part="1206 pads in parallel, not fitted (to lower a value on the bench)", mpn="-", shop="-", code="-",
                 price=0.0, cur="USD", min=0, dnp=True),
    "led": dict(part="RGB LED, addressable, 3.7 x 3.5 x 1.95 mm, top emitting (the beat LED)", mpn="OPSCO SK6812MINI-HS",
                shop=LCSC, code="C2922787", price=0.1085, cur="USD", min=5,
                datasheet="https://datasheet.lcsc.com/datasheet/pdf/d15550987339dcd676f3ed17cb498c41.pdf?productCode=C2922787"),
    "oled": dict(part="OLED module 0.96 in 128 x 64, white, SSD1315 (SSD1306 commands), I2C, 27.3 x 27.8 mm, 4 holes 2.54 mm"
                      " (GND VCC SCL SDA)",
                 mpn="HS HS96L03W2C03", shop=LCSC, code="C5248080", price=OLED96_PRICE, cur="USD", min=1),
    "hdr4": dict(part="Pin header 1x4, 2.54 mm (the OLED module's legs)", mpn="XFCN PZ254V-11-04P", shop=LCSC,
                 code="C2691448", price=0.0276, cur="USD", min=20, qty=1),
    "r10": dict(part="Resistor 10 ohm 1% 0.25 W axial (6.3 x 2.4 mm), standing (RC, data)", mpn="YAGEO MFR-25FBF52-10R",
                shop=LCSC, code="C1365579", price=0.0205, cur="USD", min=50),
    "r33": dict(part="Resistor 33 ohm 1% 0.5 W axial (9 x 3.2 mm), standing (RA, source)", mpn="CCO MF1/2W-33R 1% T",
                shop=LCSC, code="C119401", price=0.0241, cur="USD", min=20),
    "trs": dict(part="3.5 mm stereo jack, right angle, plug from the front, 4 pins (its switch pin left open), top side",
                mpn="XKB PJ-320A", shop=LCSC, code="C2884926", price=0.1458, cur="USD", min=5),
    "din": dict(part="DIN-5 180 degree, PCB right angle, 7 pins, no locating pegs, bottom side", mpn="HOOYA DIN-504B-M10", shop=LCSC,
                code="C2939346", price=0.3764, cur="USD", min=5),
}


def bom_md():
    groups = {}
    for ref, _l, value, fp, key, *_ in PARTS:
        if key:
            groups.setdefault(key, []).append(ref)
    lines = [f"# jank board{' (modular variant)' if MODULAR else ''}: bill of materials", "",
             f"Generated by `make_schematic.py`. One price each, read on {DATE} from the shop named, at its first price break,"
             " before VAT and shipping. LCSC prices come from LCSC's own product data (USD). Nothing here was ordered.", "",
             "v5: every output has BOTH connectors fitted, the 3.5 mm jack on the top and the DIN-5 on the bottom (in parallel,",
             "plug one at a time), and its TRS A/B switch. One board: 4 jacks, 4 DINs, 4 A/B switches, the OLED, one LED."
             + (" **Modular variant** (`jank-pcb-mod`): plus 4 mono jacks (CLOCK / RESET, both side walls), the PCF8574T, 4 buffers,"
                " 4 x 1 k, 4 BAT54S, 5 x 100 nF." if MODULAR else ""), "",
             "| Refs | Qty | Part | MPN | Shop, code | Unit | Min order |", "|---|---|---|---|---|---|---|"]
    total, minspend = {}, {}
    for key, info in BOM.items():
        refs = groups.get(key, [])
        qty = info.get("qty", len(refs))
        if not refs and "qty" not in info:      # (a part of the other variant only)
            continue
        if key == "hdr":
            refs = ["(U1)"]
        if key == "hdr4":
            refs = ["(J10)"]
        dnp = bool(info.get("dnp"))
        lines.append(f"| {', '.join(refs)} | {qty}{' (not fitted)' if dnp else ''} | {info['part']} | {info['mpn']} |"
                     f" {info['shop']} {info['code']} | {info['cur']} {info['price']:.4f} | {info['min']} |")
        if dnp or qty == 0:
            continue
        total[info["cur"]] = total.get(info["cur"], 0) + qty * info["price"]
        minspend[info["cur"]] = minspend.get(info["cur"], 0) + max(qty, info["min"]) * info["price"]
    a = ", ".join(f"{c} {t:.2f}" for c, t in sorted(total.items()))
    b = ", ".join(f"{c} {t:.2f}" for c, t in sorted(minspend.items()))
    lines += ["", "Test points TP1 to TP4 and the bypass jumpers JP8, JP11 to JP24 are bare pads, not parts.", "", "## Total, one board", "",
              "| Build | Parts at unit price | With each shop's minimum order |", "|---|---|---|",
              f"| 4 jacks + 4 DINs + 4 A/B switches, OLED, 1 LED{' + the modular outputs' if MODULAR else ''} | {a} | {b} |",
              "", "USD: LCSC and Seeed (the XIAO). The bare board is"
              " ordered at JLCPCB with another board and is not in these totals."]
    return "\n".join(lines) + "\n"


def intended_nets():
    nets = {}
    for ref, _l, _v, _fp, _b, _x, _y, pins in PARTS:
        if ref.startswith("#"):
            continue
        for p, net in pins.items():
            nets.setdefault(net, []).append(f"{ref}.{p}")
    return {"parts": [p[0] for p in PARTS if not p[0].startswith("#")], "nets": nets,
            "xiao": {**{str(i + 1): n for i, n in enumerate(XIAO_LEFT)}, **{str(k): v for k, v in XIAO_RIGHT.items()}}}


if __name__ == "__main__":
    check_parts()
    os.makedirs(os.path.join(HERE, "lib"), exist_ok=True)
    os.makedirs(os.path.join(HERE, "fab", "mod"), exist_ok=True)
    files = {f"{NAME}.kicad_sch": schematic(), "lib/jank.kicad_sym": symbol_lib(), "sym-lib-table": SYM_LIB_TABLE,
             "fp-lib-table": FP_LIB_TABLE, f"{NAME}.kicad_pro": json.dumps(PROJECT, indent=2) + "\n",
             f"{NAME}-square.kicad_pro": json.dumps({**PROJECT, "meta": {"filename": f"{NAME}-square.kicad_pro", "version": 3}},
                                                     indent=2) + "\n",   # (the square layout's board: the same rules)
             f"fab/{'mod/' if MODULAR else ''}intended-nets.json": json.dumps(intended_nets(), indent=1) + "\n",
             "BOM-mod.md" if MODULAR else "BOM.md": bom_md()}
    for rel, text in files.items():
        with open(os.path.join(HERE, rel), "w") as f:
            f.write(text)
        print("saved", rel)
