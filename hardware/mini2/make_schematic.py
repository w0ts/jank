# SPDX-License-Identifier: GPL-3.0-only
"""jank chip's schematic (mini2.kicad_sch), its project file (mini2.kicad_pro), its symbols (lib/chip.kicad_sym) and the
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
NAME = "mini2"
NS = uuid.UUID("0c3a8e51-7d2f-4b61-9e04-5a1f3c7b9d26")
ROOT = str(uuid.uuid5(NS, "root"))
DATE = "2026-11-11"
G = 2.54
F = "(effects (font (size 1.27 1.27)))"
FH = "(effects (font (size 1.27 1.27)) (hide yes))"


def uid(*key):
    return str(uuid.uuid5(NS, "/".join(map(str, key))))


# ---- symbols: (left pins, right pins), each pin (number, name, electrical type); ref prefix; body half-width
PAS = "passive"
SYMBOLS = {
    "R": ([("1", "~", PAS)], [("2", "~", PAS)], "R", 2.54),
    "C": ([("1", "~", PAS)], [("2", "~", PAS)], "C", 2.54),
    "CP": ([("1", "+", PAS)], [("2", "-", PAS)], "C", 2.54),
    "L": ([("1", "~", PAS)], [("2", "~", PAS)], "L", 2.54),
    "D": ([("2", "A", PAS)], [("1", "K", PAS)], "D", 2.54),
    "led": ([("1", "A", PAS)], [("2", "K", PAS)], "LED", 2.54),
    "bav70": ([("1", "A1", PAS), ("2", "A2", PAS)], [("3", "K", PAS)], "D", 5.08),
    "sw": ([("1", "A", PAS), ("2", "B", PAS)], [("3", "C", PAS), ("4", "D", PAS)], "SW", 5.08),
    "jack": ([("D", "T", PAS), ("C", "R", PAS)], [("A", "S", PAS), ("B", "SW", PAS)], "J", 5.08),
    "trs": ([("T", "T", PAS), ("R", "R", PAS)], [("S", "S", PAS)], "P", 5.08),
    "det": ([("3", "VDD", "power_in")], [("1", "VOUT", "output"), ("2", "VSS", "power_in")], "U", 7.62),
    "buck": ([("6", "VIN", "power_in"), ("5", "EN", "input"), ("3", "MODE", "input"), ("4", "VSET", PAS)],
             [("2", "SW", PAS), ("1", "VOS", "power_in"), ("8", "GND", "power_in"), ("9", "EP", "power_in"),
              ("7", "NC", "no_connect")], "U", 7.62),
    "usbc": ([("A4B9", "VBUS", PAS), ("B4A9", "VBUS", PAS), ("A5", "CC1", PAS), ("B5", "CC2", PAS), ("A6", "DP1", PAS),
              ("B6", "DP2", PAS), ("A7", "DN1", PAS), ("B7", "DN2", PAS)],
             [("A8", "SBU1", PAS), ("B8", "SBU2", PAS), ("A1B12", "GND", PAS), ("B1A12", "GND", PAS), ("13", "SHELL", PAS),
              ("14", "SHELL", PAS)], "J", 7.62),
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
SYMBOLS["din"] = ([("2", "DIN4", PAS), ("4", "DIN5", PAS), ("3", "DIN2", PAS)],
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
SYMBOLS["ldo5"] = ([("1", "VIN", "power_in"), ("3", "CE", "input")], [("5", "VOUT", "power_out"), ("2", "GND", "power_in"),
                                                                     ("4", "NC", "no_connect")], "U", 7.62)
SYMBOLS["pfet"] = ([("1", "G", "input")], [("2", "S", PAS), ("3", "D", PAS)], "Q", 5.08)
SYMBOLS["chg"] = ([("4", "VCC", "power_in"), ("5", "PROG", PAS)], [("3", "BAT", PAS), ("1", "CHRG", "open_collector"),
                                                                  ("2", "GND", "power_in")], "U", 7.62)
SYMBOLS["prot"] = ([("3", "VDD", "power_in"), ("1", "VT", PAS)], [("2", "GND", PAS), ("4", "VM", PAS), ("5", "VM", PAS)],
                   "U", 7.62)
SYMBOLS["ph2"] = ([("1", "1", PAS)], [("2", "2", PAS)], "J", 5.08)


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


FLAGS = ["GND", "VBUS", "VDD3P3", "VSYS", "BAT_P"]   # (power_in pins on these nets need a driver for the ERC; +3V3: U2's VOUT)


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
           f'(title_block (title "jank mini 2.2 simplified: USB power, two MIDI OUTs (one signal), LINE IN, BOOT / RST, a red + green LED") (date "{DATE}")'
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
        "jank mini 2.2 simplified (hardware/mini2/README.md). POWER: VBUS through D3 into VSYS; the cell (BAT_P, J8 / J9 PH2.0 in parallel, DNP)"
        " through Q1 AO3401A (gate on VBUS, R50 100k) into VSYS; U2 HT7833 VSYS -> +3V3 (C3 10u in, C5 22u + C6 100n out). No MIDI power input.",
        "CELL: U8 TP4054-42 charges BAT_P from VBUS (R51 5.1k: ~196 mA; C40 / C41 10u); U9 XB5353A protects the cell in its minus lead"
        " (GND = BAT_N, VM = GND; C42 100n). R10 / R11 1M / 1M + C8 100n: BAT_P / 2 on GPIO1 (VBAT_ADC).",
        "MIDI OUT 1 (J2, DIN J5) and MIDI OUT 2 (J3, DIN J6) on the same nets (type A): ring = +3V3 through R14 // R26 // R48 (3 x 100 ohm = 33.3 ohm),"
        " tip = MIDI_TX (GPIO21) through R15 10 ohm, sleeve = GND. J5 / J6: DIN sockets on the bottom (DNP, hand-soldered).",
        "Pins: GPIO21 MIDI_TX, GPIO0 BOOT (SW1; R16 pull-up only), GPIO38 LED_R (LED1 red, R40 1k), GPIO39 LED_G (LED2 green, R41 1k),"
        " GPIO9 LINE_IN, GPIO1 VBAT_ADC, GPIO19 / 20 USB D- / D+. GPIO3, 45, 46 (strapping) open: internal defaults (3.3 V flash, SPI boot).",
        "U1 ESP32-S3FN8: VDDA C11-C13; VDD3P3 through L2 2 nH (C14-C17 before, C18 after); VDD3P3_CPU C19, VDD3P3_RTC C20; VDD_SPI C21 + C22;"
        " CHIP_PU R8 10k / C7 1u; Y1 40 MHz, C9 / C10 10 pF, R17 0 ohm on XTAL_P (XIAO ESP32S3 v1.2 values).",
        "RF: LNA_IN, C23 1 pF shunt, L3 3.3 nH series, C24 shunt (DNP), the 50 ohm line (net RF), C25 (DNP), R18 0 ohm, C26 (DNP),"
        " AE1 chip antenna. External antenna instead: R18 moved to R19 (DNP), J4 u.FL (DNP). Matching values UNTUNED (no VNA).",
        "mini 2 LINE IN (hardware/mini2/README.md): J7 tip / ring, D7 ESD, R30 / R31 20k + C30 / C31 1u into U6 LMV321's virtual ground"
        " (inverting summer, R32 5.6k // C32 2.2n: gain 0.28 a channel), IN+ at LIN_VB 1.47 V (R33 150k / R34 120k / C33 1u),"
        " R35 120 + C36 100n to GPIO9 (ADC1_CH8). AVDD: U7 XC6206 3.3 V from VBUS.",
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
               "min_hole_to_hole": 0.5, "min_through_hole_diameter": 0.3, "min_track_width": 0.15,
               "min_via_annular_width": 0.1, "min_via_diameter": 0.5, "min_text_height": 0.6,
               "min_text_thickness": 0.08, "min_silk_clearance": 0.0}}},
           "net_settings": {"classes": [{"name": "Default", "clearance": 0.15, "track_width": 0.2, "via_diameter": 0.6,
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
