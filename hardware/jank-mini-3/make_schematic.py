# SPDX-License-Identifier: GPL-3.0-only
"""jank LCD-2 add-on's schematic (lcd2.kicad_sch; mini 2.2's generator, hardware/mini2/make_schematic.py), its project file, its symbols (lib/chip.kicad_sym) and the
intended nets (fab/intended-nets.json, read by check_nets.py), all from parts.py. As in hardware/pcb: every pin carries
a net label (no wires), so the sheet reads as the netlist and KiCad's ERC checks it. The symbols are drawn here, one
per part kind, their pin numbers the JLC footprints' pad names (so the LED's pad 1 is its anode, as JLC's is).

  python3 make_schematic.py        (make_fab.sh runs it, then the ERC, the netlist and the PDF)
"""
import json
import os
import uuid

import sys
sys.dont_write_bytecode = True
import parts as P

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "lcd2"
NS = uuid.UUID("5d1e7b20-3c44-4f8a-9b21-6e0c2a9f4d13")
ROOT = str(uuid.uuid5(NS, "root"))
DATE = "2026-11-17"
G = 2.54
F = "(effects (font (size 1.27 1.27)))"
FH = "(effects (font (size 1.27 1.27)) (hide yes))"


def uid(*key):
    return str(uuid.uuid5(NS, "/".join(map(str, key))))


# ---- symbols: (left pins, right pins), each pin (number, name, electrical type); ref prefix; body half-width
PAS = "passive"
SYMBOLS = {
    "R": ([("1", "~", PAS)], [("2", "~", PAS)], "R", 2.54),
    "F": ([("1", "~", PAS)], [("2", "~", PAS)], "F", 2.54),   # (jank-mini-3: the cells' fuses)
    "C": ([("1", "~", PAS)], [("2", "~", PAS)], "C", 2.54),
    "CP": ([("1", "+", PAS)], [("2", "-", PAS)], "C", 2.54),
    "L": ([("1", "~", PAS)], [("2", "~", PAS)], "L", 2.54),
    "D": ([("2", "A", PAS)], [("1", "K", PAS)], "D", 2.54),
    "led": ([("1", "A", PAS)], [("2", "K", PAS)], "LED", 2.54),
    "bav70": ([("1", "A1", PAS), ("2", "A2", PAS)], [("3", "K", PAS)], "D", 5.08),
    "sw": ([("1", "A", PAS), ("2", "B", PAS)], [("3", "C", PAS), ("4", "D", PAS)], "SW", 5.08),
    "jack": ([("D", "T", PAS), ("C", "R", PAS)], [("A", "S", PAS), ("B", "SW", PAS)], "J", 5.08),
    "trs": ([("T", "T", PAS), ("R", "R", PAS)], [("S", "S", PAS)], "P", 5.08),
    "jack5": ([("5", "T", PAS), ("4", "TSW", PAS), ("2", "R", PAS), ("3", "RSW", PAS)], [("1", "S", PAS)], "J", 5.08),   # (PJ-307C)
    "pads2": ([("1", "A", PAS)], [("2", "B", PAS)], "J", 2.54),   # (the power switch's wire pads)
    "pads6": ([("1", "TX", PAS), ("2", "RX", PAS), ("3", "3V3", PAS)], [("4", "GND", PAS), ("5", "EN", PAS), ("6", "BOOT", PAS)], "J", 5.08),
    "det": ([("3", "VDD", "power_in")], [("1", "VOUT", "output"), ("2", "VSS", "power_in")], "U", 7.62),
    "buck": ([("6", "VIN", "power_in"), ("5", "EN", "input"), ("3", "MODE", "input"), ("4", "VSET", PAS)],
             [("2", "SW", PAS), ("1", "VOS", "power_in"), ("8", "GND", "power_in"), ("9", "EP", "power_in"),
              ("7", "NC", "no_connect")], "U", 7.62),
    "usbc": ([("A4B9", "VBUS", PAS), ("B4A9", "VBUS", PAS), ("A5", "CC1", PAS), ("B5", "CC2", PAS), ("A6", "DP1", PAS),
              ("B6", "DP2", PAS), ("A7", "DN1", PAS), ("B7", "DN2", PAS)],
             [("A8", "SBU1", PAS), ("B8", "SBU2", PAS), ("A1B12", "GND", PAS), ("B1A12", "GND", PAS), ("13", "SHELL", PAS),
              ("14", "SHELL", PAS)], "J", 7.62),
    "usbcm": ([("A4", "VBUS", PAS), ("B9", "VBUS", PAS), ("A9", "VBUS", PAS), ("B4", "VBUS", PAS), ("A5", "CC1", PAS),
               ("B5", "CC2", PAS), ("A6", "DP1", PAS), ("B6", "DP2", PAS), ("A7", "DN1", PAS), ("B7", "DN2", PAS)],
              [("A8", "SBU1", PAS), ("B8", "SBU2", PAS), ("A1", "GND", PAS), ("B12", "GND", PAS), ("B1", "GND", PAS),
               ("A12", "GND", PAS), ("25", "SHELL", PAS), ("26", "SHELL", PAS), ("27", "SHELL", PAS), ("28", "SHELL", PAS)],
              "J", 10.16),
    "esd": ([("1", "IO1", PAS), ("6", "IO1", PAS), ("3", "IO2", PAS), ("4", "IO2", PAS)],
            [("5", "VBUS", PAS), ("2", "GND", PAS)], "U", 7.62),
    "flag": ([], [("1", "pwr", "power_out")], "#FLG", 2.54),
}
# ESP32-S3FN8 (QFN56): supplies and the system pins on the left, the GPIOs on the right (parts.py has the pin list)
_chip_l = [("2", "VDD3P3", "power_in"), ("3", "VDD3P3", "power_in"), ("20", "VDD3P3_RTC", "power_in"),
           ("46", "VDD3P3_CPU", "power_in"), ("55", "VDDA", "power_in"), ("56", "VDDA", "power_in"),
           ("29", "VDD_SPI", "power_out"), ("4", "CHIP_PU", "input"), ("54", "XTAL_P", PAS), ("53", "XTAL_N", PAS),
           ("1", "LNA_IN", PAS), ("57", "GND", "power_in")]
_chip_r = ([(str(5 + g), f"GPIO{g}", "bidirectional") for g in range(15)]
           + [(str(21 + i), f"GPIO{15 + i}", "bidirectional") for i in range(7)]
           + [("28", "SPICS1", "no_connect")]
           + [(str(n), nm, "no_connect") for n, nm in zip(range(30, 38), ("SPIHD", "SPIWP", "SPICS0", "SPICLK", "SPIQ",
                                                                        "SPID", "SPICLK_N", "SPICLK_P"))]
           + [(str(38 + i), f"GPIO{33 + i}", "bidirectional") for i in range(6)]
           + [(n, f"GPIO{g}", "bidirectional") for n, g in (("44", 39), ("45", 40), ("47", 41), ("48", 42), ("49", 43),
                                                             ("50", 44), ("51", 45), ("52", 46))])
SYMBOLS["chip"] = (_chip_l, _chip_r, "U", 12.7)
SYMBOLS["xtal"] = ([("1", "X1", PAS), ("3", "X2", PAS)], [("2", "GND", PAS), ("4", "GND", PAS)], "Y", 5.08)
SYMBOLS["ant"] = ([("1", "FEED", PAS)], [("2", "END", PAS)], "AE", 5.08)
SYMBOLS["din"] = ([("2", "DIN5", PAS), ("4", "DIN4", PAS), ("3", "DIN2", PAS)],
                  [("1", "DIN1", PAS), ("5", "DIN3", PAS), ("6", "SHELL", PAS), ("7", "SHELL", PAS)], "J", 5.08)
SYMBOLS["nfet"] = ([("1", "G", "input")], [("3", "D", PAS), ("2", "S", PAS)], "Q", 5.08)
SYMBOLS["ufl"] = ([("1", "SIG", PAS)], [("2", "GND", PAS), ("3", "GND", PAS)], "J", 5.08)
# mini 2, LINE IN: the op-amp, the LDO, the two-line ESD diode
SYMBOLS["opamp"] = ([("1", "IN+", "input"), ("3", "IN-", "input")], [("4", "OUT", "output"), ("5", "V+", "power_in"),
                                                                 ("2", "GND", "power_in")], "U", 7.62)
SYMBOLS["ldo"] = ([("3", "VIN", "power_in")], [("2", "VOUT", "power_out"), ("1", "VSS", "power_in")], "U", 7.62)
SYMBOLS["tvs2"] = ([("1", "IO1", PAS), ("2", "IO2", PAS)], [("3", "COM", PAS)], "D", 5.08)
# mini 2.2 simplified, with the battery (pins from EasyEDA's symbols): HT7833 (C164106), the P-FET AO3401A (C15127),
# TP4054 (C32574), XB5353A (C80628: GND = the cell's minus, VM = the load's), the PH2.0 socket (C131337)
SYMBOLS["ldo5"] = ([("1", "VIN", "power_in"), ("3", "CE", "input")], [("5", "VOUT", PAS), ("2", "GND", "power_in"),
                                                                     ("4", "NC", "no_connect")], "U", 7.62)
# (HT7833's VOUT passive: it and the AMS1117 are alternatives on one +3V3, one per order; the AMS1117's VOUT drives the
#  net for the ERC.) The SOT-223 LDO (the AMS1117's pinout, review pass 1 the LDL1117: 1 GND, 2 VOUT, 3 VIN, 4 the tab = VOUT)
SYMBOLS["ldo223"] = ([("3", "VIN", "power_in")], [("2", "VOUT", "power_out"), ("4", "TAB", PAS), ("1", "GND", "power_in")],
                     "U", 7.62)
SYMBOLS["nfet"] = ([("1", "G", "input")], [("3", "D", PAS), ("2", "S", PAS)], "Q", 5.08)   # (stage 2: CLOCK / RESET)
SYMBOLS["bav99"] = ([("1", "A1", PAS)], [("3", "K1A2", PAS), ("2", "K2", PAS)], "D", 5.08)
SYMBOLS["pfet"] = ([("1", "G", "input")], [("2", "S", PAS), ("3", "D", PAS)], "Q", 5.08)
SYMBOLS["chg"] = ([("4", "VCC", "power_in"), ("5", "PROG", PAS)], [("3", "BAT", PAS), ("1", "CHRG", "open_collector"),
                                                                  ("2", "GND", "power_in")], "U", 7.62)
SYMBOLS["prot"] = ([("3", "VDD", "power_in"), ("1", "VT", PAS)], [("2", "GND", PAS), ("4", "VM", PAS), ("5", "VM", PAS)],
                   "U", 7.62)
SYMBOLS["ph2"] = ([("1", "1", PAS)], [("2", "2", PAS)], "J", 5.08)
# LCD-2 add-on, the cells (2026-11-18): DW01A (PUOLOP C351410) and FS8205A (TECH PUBLIC C2830320), pin names from
# JLC's own symbols; the PH2.0 SMD socket with its two mounting tabs (C722761)
SYMBOLS["dw01"] = ([("5", "VCC", PAS), ("6", "GND", PAS), ("4", "TD", PAS)],
                   [("1", "OD", "output"), ("3", "OC", "output"), ("2", "VM", PAS)], "U", 7.62)
SYMBOLS["fet2"] = ([("6", "G1", "input"), ("1", "S1", PAS), ("4", "G2", "input")],
                   [("3", "S2", PAS), ("2", "D", PAS), ("5", "D", PAS)], "Q", 7.62)
SYMBOLS["ph2m"] = ([("1", "1", PAS), ("2", "2", PAS)], [], "J", 5.08)   # (jank-mini-3: through-hole PH2.0, no mounting pads)
# stage 1e: a bidirectional TVS (no polarity), the screen's 1 x 4 header, a 3-pad solder jumper (2 = the middle)
SYMBOLS["tvsbi"] = ([("1", "~", PAS)], [("2", "~", PAS)], "D", 2.54)
SYMBOLS["hdr4"] = ([("1", "P1", PAS), ("2", "P2", PAS)], [("3", "SCL", PAS), ("4", "SDA", PAS)], "J", 5.08)
SYMBOLS["sj3"] = ([("1", "A", PAS), ("3", "B", PAS)], [("2", "C", PAS)], "JP", 5.08)
# review pass 1: the MIDI lines' ESD (the SRV05-4's own pin names: four separate lines), the charge pump's buffer
SYMBOLS["esd4"] = ([("1", "IO1", PAS), ("3", "IO2", PAS), ("4", "IO3", PAS), ("6", "IO4", PAS)],
                   [("5", "REF", PAS), ("2", "GND", PAS)], "U", 7.62)
SYMBOLS["ldo6"] = ([("6", "IN", "power_in"), ("4", "EN", "input")], [("1", "OUT", "power_out"), ("3", "GND", "power_in"),
                                                                  ("7", "PAD", PAS)], "U", 7.62)   # (review pass 2: the TLV757P, DRV)
SYMBOLS["idiode"] = ([("1", "VIN", PAS), ("3", "~{CE}", "input")], [("6", "VOUT", PAS), ("5", "~{ST}", PAS),
                                                                   ("2", "GND", "power_in"), ("4", "NC", PAS)], "U", 7.62)
# (the LM66100's VIN / VOUT passive: the cell's and VSYS's power flags drive them; ST tied to GND)
SYMBOLS["bb10"] = ([("10", "VIN", "power_in"), ("1", "EN", "input"), ("2", "MODE", "input"), ("9", "L1", PAS)],
                   [("6", "VOUT", "power_out"), ("4", "FB", "input"), ("5", "PG", "open_collector"), ("7", "L2", PAS),
                    ("8", "GND", "power_in"), ("3", "AGND", "power_in")], "U", 7.62)   # (TPS63802)
SYMBOLS["boost6"] = ([("3", "VIN", "power_in"), ("2", "EN", "input"), ("5", "SW", PAS)],
                     [("6", "VOUT", "power_out"), ("1", "FB", "input"), ("4", "GND", "power_in")], "U", 7.62)   # (TPS61023)
SYMBOLS["shunt"] = ([("1", "REF", "input")], [("2", "K", PAS), ("3", "A", PAS)], "U", 5.08)   # (CJ431)
_wl = [("2", "3V3", "power_in"), ("3", "EN", "input"), ("1", "GND", "power_in"), ("40", "GND", "power_in"), ("41", "EPAD", "power_in")]
_wr = [(n, nm, "bidirectional") for n, nm in (("4", "IO4"), ("5", "IO5"), ("6", "IO6"), ("7", "IO7"), ("8", "IO15"), ("9", "IO16"),
       ("10", "IO17"), ("11", "IO18"), ("12", "IO8"), ("13", "IO19"), ("14", "IO20"), ("15", "IO3"), ("16", "IO46"), ("17", "IO9"),
       ("18", "IO10"), ("19", "IO11"), ("20", "IO12"), ("21", "IO13"), ("22", "IO14"), ("23", "IO21"), ("24", "IO47"),
       ("25", "IO48"), ("26", "IO45"), ("27", "IO0"), ("28", "IO35"), ("29", "IO36"), ("30", "IO37"), ("31", "IO38"),
       ("32", "IO39"), ("33", "IO40"), ("34", "IO41"), ("35", "IO42"), ("36", "RXD0"), ("37", "TXD0"), ("38", "IO2"), ("39", "IO1"))]
SYMBOLS["wroom"] = (_wl, _wr, "U", 12.7)   # (ESP32-S3-WROOM-1U: Table 3-1)
SYMBOLS["rgb"] = ([("3", "DI", "input"), ("4", "VDD", "power_in")], [("1", "DO", "output"), ("2", "GND", "power_in")], "LED", 5.08)   # (WS2812B-2020)
SYMBOLS["ldo89"] = ([("2", "VIN", PAS)], [("3", "VOUT", "power_out"), ("1", "GND", "power_in")], "U", 7.62)   # (HT7550-1)
SYMBOLS["tusb"] = ([("1", "CC1", PAS), ("2", "CC2", PAS), ("4", "VBUS_DET", "input"), ("3", "PORT", "input"),
                    ("5", "ADDR", "input"), ("11", "~{EN}", "input")],
                   [("12", "VDD", "power_in"), ("8", "SCL", "bidirectional"), ("7", "SDA", "bidirectional"),
                    ("6", "~{INT}", "open_collector"), ("9", "~{ID}", "open_collector"), ("10", "GND", "power_in")],
                   "U", 10.16)   # (host mode: the TUSB320LI)
SYMBOLS["inv6"] = ([("1", "1A", "input"), ("3", "2A", "input"), ("5", "3A", "input"), ("9", "4A", "input"), ("11", "5A", "input"), ("13", "6A", "input"), ("14", "VCC", "power_in")],
                   [("2", "1Y", "output"), ("4", "2Y", "output"), ("6", "3Y", "output"), ("8", "4Y", "output"), ("10", "5Y", "output"), ("12", "6Y", "output"), ("7", "GND", "power_in")], "U", 7.62)   # (74AHCT04: TI SCLS228 pin table)
SYMBOLS["sj2"] = ([("1", "A", PAS)], [("2", "B", PAS)], "JP", 2.54)   # (buffer try: a closed 2-pad solder jumper)
SYMBOLS["tvs2l"] = ([("1", "1", PAS), ("2", "2", PAS)], [("3", "3", PAS)], "D", 5.08)   # (PESD15VL2BT: 1 / 2 the lines, 3 the common)
SYMBOLS["buf"] = ([("2", "A", "input"), ("1", "~{OE}", "input")], [("4", "Y", "output"), ("5", "VCC", "power_in"),
                                                                   ("3", "GND", "power_in")], "U", 7.62)


# LCD-2 add-on: the 1 x 14 socket (the LCD-2's P1 / P2) and the spare pads
SYMBOLS["hdr14"] = ([(str(i), str(i), PAS) for i in range(1, 8)], [(str(i), str(i), PAS) for i in range(8, 15)], "J", 5.08)
SYMBOLS["usba"] = ([("1", "VBUS", PAS), ("2", "D-", PAS), ("3", "D+", PAS)], [("4", "GND", PAS), ("5", "SHELL", PAS)], "J", 5.08)
SYMBOLS["pads4a"] = ([("1", "IO6", PAS), ("2", "GND", PAS)], [("3", "3V3", PAS), ("4", "GND", PAS)], "J", 5.08)   # (jank-mini-3: J17, by P1)
SYMBOLS["pads4b"] = ([("1", "IO47", PAS), ("2", "IO48", PAS)], [("3", "3V3", PAS), ("4", "GND", PAS)], "J", 5.08)   # (J20, by P2)
SYMBOLS["pads6s"] = ([("1", "IO2", PAS), ("2", "IO4", PAS), ("3", "3V3", PAS)], [("4", "GND", PAS), ("5", "IO47", PAS),
                     ("6", "IO48", PAS)], "J", 5.08)


def symbol(kind):
    left, right, ref, hw = SYMBOLS[kind]
    n = max(len(left), len(right), 1)
    top = (n - 1) * G / 2
    pins = []
    for side, lst in ((-1, left), (1, right)):
        for i, (num, name, typ) in enumerate(lst):
            x, y, a = side * (hw + G), round(top - i * G, 2), 0 if side < 0 else 180
            pins.append(f'(pin {typ} line (at {x} {y} {a}) (length {G}) (name "{name}" {F}) (number "{num}" {F}))')
    h = top + G
    flag = kind == "flag"
    return (f'(symbol "chip:{kind}" (pin_names (offset 0.508)) (exclude_from_sim no) (in_bom {"no" if flag else "yes"})'
            f' (on_board {"no" if flag else "yes"})'
            f' (property "Reference" "{ref}" (at 0 {h + 1.27} 0) {F}) (property "Value" "{kind}" (at 0 {-h - 1.27} 0) {F})'
            f' (property "Footprint" "" (at 0 0 0) {FH}) (property "Datasheet" "" (at 0 0 0) {FH})'
            f' (property "Description" "jank chip: {kind}" (at 0 0 0) {FH})'
            + (' (power)' if flag else '') +
            f' (symbol "{kind}_0_1" (rectangle (start {-hw} {h}) (end {hw} {-h}) (stroke (width 0.254) (type default))'
            f' (fill (type background))))'
            f' (symbol "{kind}_1_1" {" ".join(pins)}))')


def pin_xy(kind):
    left, right, _r, hw = SYMBOLS[kind]
    top = (max(len(left), len(right), 1) - 1) * G / 2
    out = {}
    for side, lst in ((-1, left), (1, right)):
        for i, (num, _n, typ) in enumerate(lst):
            out[num] = (side * (hw + G), round(top - i * G, 2), side, typ)
    return out


FLAGS = ["GND", "+3V3", "VBAT", "VBUS_B", "VIN_B", "V5C"]   # (power_in pins on these nets need a driver for the ERC: the LCD-2 drives +3V3 / VBAT / VBUS_B through the sockets)


def placed():
    """[(ref, kind, value, footprint, lcsc, pins, x, y)] on an A3 sheet, a simple grid"""
    out, x, y, row_h = [], 30.48, 30.48, 0.0
    for ref, kind, value, fp, lcsc, pins in P.PARTS:
        left, right, _r, hw = SYMBOLS[kind]
        h = (max(len(left), len(right), 1) + 1) * G
        w = 2 * hw + 30.48
        if x + w > 400:
            x, y, row_h = 30.48, y + row_h + 12.7, 0.0
        out.append((ref, kind, value, f"chip:{P.FP[fp]}", lcsc, pins, round(x + hw + 12.7, 2), round(y + h / 2, 2)))
        x += w
        row_h = max(row_h, h)
    y += row_h + 12.7
    for i, net in enumerate(FLAGS):
        out.append((f"#FLG0{i + 1}", "flag", "PWR_FLAG", "", "", {"1": net}, 30.48 + 40.64 * i, y))
    return out


def snap(v):
    return round(round(v / 1.27) * 1.27, 2)


def schematic():
    kinds = sorted({p[1] for p in P.PARTS} | {"flag"})
    out = [f'(kicad_sch (version 20250114) (generator "make_schematic.py") (generator_version "1.0")',
           f'(uuid "{ROOT}") (paper "A2")',
           f'(title_block (title "jank LCD-2 add-on: 5 V aux rail, two MIDI OUTs, CLOCK / RESET, LINE IN, 4 buttons, USB host path") (date "{DATE}")'
           ' (rev "0.1 for review") (comment 1 "hardware/chip/README.md") (comment 2 "generated by make_schematic.py"))',
           "(lib_symbols", *[symbol(k) for k in kinds], ")"]
    for ref, kind, value, fp, lcsc, pins, x, y in placed():
        x, y = snap(x), snap(y)
        flag = ref.startswith("#")
        trs = kind == "trs"
        dnp = ref in P.DNP
        pxy = pin_xy(kind)
        pins_txt = " ".join(f'(pin "{p}" (uuid "{uid(ref, "pin", p)}"))' for p in pxy)
        out.append(f'(symbol (lib_id "chip:{kind}") (at {x} {y} 0) (unit 1) (exclude_from_sim no)'
                   f' (in_bom {"no" if flag or trs or dnp else "yes"}) (on_board {"no" if flag else "yes"}) (dnp {"yes" if dnp else "no"})'
                   f' (uuid "{uid(ref)}")'
                   f' (property "Reference" "{ref}" (at {x} {y - 7.62} 0) {FH if flag else F})'
                   f' (property "Value" "{value}" (at {x} {y + 7.62} 0) {FH if flag else F})'
                   f' (property "Footprint" "{fp}" (at {x} {y} 0) {FH})'
                   f' (property "Datasheet" "" (at {x} {y} 0) {FH})'
                   f' (property "LCSC" "{lcsc}" (at {x} {y} 0) {FH})'
                   f' {pins_txt} (instances (project "{NAME}" (path "/{ROOT}" (reference "{ref}") (unit 1)))))')
        for num, (px, py, side, typ) in pxy.items():
            lx, ly = round(x + px, 2), round(y - py, 2)
            net = pins.get(num)
            if net:
                a, just = (180, "right") if side < 0 else (0, "left")
                out.append(f'(label "{net}" (at {lx} {ly} {a}) (effects (font (size 1.27 1.27)) (justify {just} bottom))'
                           f' (uuid "{uid(ref, "lbl", num)}"))')
            elif typ != "no_connect":
                out.append(f'(no_connect (at {lx} {ly}) (uuid "{uid(ref, "nc", num)}"))')
    notes = [
        "jank-mini-3. The Waveshare ESP32-S3-LCD-2 joins J15 (its P1) / J16 (its P2) through the bridge strips: +3V3, VBAT (its cell, on its own MX1.25 connector; no cell on this board), VBUS_B (its USB-C VBUS) and 14 GPIOs (parts.GPIO).",
        "5 V aux: VIN_B from VBUS_B through D3, or from VBAT through Q1 (off while VBUS_B is up); U18 TPS61023 -> V5A 5.00 V (R134 / R135+R154+R136); U19 clamp 5.83 V behind R145; JP8 MIDI, JP9 CV.",
        "USB: J1 TO ESP (USB-C, the outside loop cable from the LCD-2's own USB-C; VBUS / CC open) -> D+ / D- -> J4 TO DEVICE (USB-C), U5 ESD at J4; J4's VBUS (pads A4 / B9) = the host signal (HOST5 from U21, R93, D16) + C28; Rp 56k on CC.",
        "MIDI OUT 1 (IO13) / 2 (IO9): jack + DIN each, own loops from V5M, AO3400A sinks (UART TX inverted), SRV05-4 at the pins. CLOCK (IO15) / RESET (IO2): 74AHCT04 on V5C, BAV99, 1k. LINE IN: IO7 / IO8 matched networks, LIN_DET IO18.",
    ]
    for i, t in enumerate(notes):
        out.append(f'(text "{t}" (exclude_from_sim no) (at 20.32 {12.7 + 3.81 * i} 0) (effects (font (size 1.5 1.5))'
                   f' (justify left bottom)) (uuid "{uid("note", i)}"))')
    out.append('(sheet_instances (path "/" (page "1")))')
    out.append(")")
    return "\n".join(out) + "\n"


PROJECT = {"meta": {"filename": f"{NAME}.kicad_pro", "version": 3},
           "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
           "sheets": [[ROOT, "Root"]], "text_variables": {},
           "board": {"design_settings": {"rules": {
               "min_clearance": 0.15, "min_copper_edge_clearance": 0.25, "min_hole_clearance": 0.2,
               "min_hole_to_hole": 0.5, "min_through_hole_diameter": 0.3, "min_track_width": 0.10,
               "min_via_annular_width": 0.1, "min_via_diameter": 0.5, "min_text_height": 0.6,
               "min_text_thickness": 0.08, "min_silk_clearance": 0.0}}},
           "net_settings": {"classes": [{"name": "Default", "clearance": 0.15, "track_width": 0.15, "via_diameter": 0.6,
                                         "via_drill": 0.3, "priority": 2147483647}], "meta": {"version": 5}}}
LIB_TABLES = {
    "sym-lib-table": '(sym_lib_table\n  (version 7)\n  (lib (name "chip")(type "KiCad")(uri "${KIPRJMOD}/lib/chip.kicad_sym")(options "")(descr "jank chip, drawn by make_schematic.py"))\n)\n',
    "fp-lib-table": '(fp_lib_table\n  (version 7)\n  (lib (name "chip")(type "KiCad")(uri "${KIPRJMOD}/lib/chip.pretty")(options "")(descr "JLC footprints (fetch_jlc.sh) and the TRS pads"))\n)\n',
}


def main():
    open(os.path.join(HERE, f"{NAME}.kicad_sch"), "w").write(schematic())
    kinds = sorted({p[1] for p in P.PARTS} | {"flag"})
    open(os.path.join(HERE, "lib", "chip.kicad_sym"), "w").write(
        '(kicad_symbol_lib (version 20251024) (generator "make_schematic.py") (generator_version "1.0")\n'
        + "\n".join(symbol(k).replace('(symbol "chip:', '(symbol "', 1) for k in kinds) + "\n)\n")
    open(os.path.join(HERE, f"{NAME}.kicad_pro"), "w").write(json.dumps(PROJECT, indent=2) + "\n")
    for name, body in LIB_TABLES.items():
        open(os.path.join(HERE, name), "w").write(body)
    os.makedirs(os.path.join(HERE, "fab"), exist_ok=True)
    json.dump({"nets": P.nets(), "gpio": {k: v[0] for k, v in P.GPIO.items()}},
              open(os.path.join(HERE, "fab", "intended-nets.json"), "w"), indent=1)
    print(f"wrote {NAME}.kicad_sch ({len(P.PARTS)} parts, {len(P.nets())} nets)")


if __name__ == "__main__":
    main()
