# SPDX-License-Identifier: GPL-3.0-only
"""jank tiny: the one parts table (README "Schematic"). make_schematic.py, make_board.py, make_assembly.py and
check_nets.py all read it. Every footprint is JLC's own (EasyEDA, fetched by fetch_jlc.sh into lib/jlc.pretty), so a
part's CPL position and rotation are its footprint's origin and KiCad rotation, with no correction table
(make_assembly.py checks every pad against JLC's footprint anyway).

PARTS: (ref, symbol, value, footprint, lcsc, {pad: net}). A pad left out is not connected.
"""

FP = {   # footprint names in lib/tiny.pretty (copies of lib/jlc.pretty, plus our TRS pads)
    "module": "BULETM-SMD_ESP32-S3-MINI-1-N8", "usbc": "USB-C-SMD_TYPE-C-16PIN-2MD-073", "jack": "AUDIO-SMD_XKB_PJ-320D-B",
    "buck": "HVSSOP-8_L3.0-W3.0-P0.65-LS4.9-BL-EP", "det": "SOT-23-3_L2.9-W1.6-P1.90-LS2.8-BR",
    "bav70": "SOT-23-3_L3.0-W1.7-P0.95-LS2.9-BR", "sod123": "SOD-123_L2.7-W1.6-LS3.7-RD-1",
    "led": "LED-SMD_L1.6-W0.8-R-RD", "sw": "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5", "l0806": "L0806",
    "can10": "CAP-SMD_BD10.0-L10.3-W10.3-LS11.0-FD", "r0603": "R0603", "r0805": "R0805", "c0603": "C0603",
    "c0805": "C0805", "trs": "TRS_PADS_3x1.0mm",
}

# the module's pads (JLC footprint numbering = Espressif's pin numbers; the nine centre pads are all named "GND")
MODULE_GND = ["1", "2", "42", "43"] + [str(n) for n in range(46, 61)] + ["GND"]
GPIO = {   # firmware's board.h (README "Pin map"): function -> (GPIO, module pin)
    "MIDI_TX": (21, "25"),      # UART TX to the MIDI OUT jack's tip, through 10 ohm; RTC_GPIO21 (the ULP can drive it)
    "LED_DRV": (38, "34"),      # the LED, active high, through 5.1 k
    "BOOT": (0, "4"),           # BOOT / user button to GND (strapping: held low at reset = download mode)
    "VBANK_ADC": (1, "5"),      # ADC1_CH0: the reservoir through 1 M / 1 M (half its voltage)
    "VBUS_SENSE": (2, "6"),     # ADC1_CH1: USB VBUS through 100 k / 100 k (2.5 V when USB is plugged)
    "PWR_HOLD": (4, "8"),       # RTC_GPIO4: high keeps the buck on below the detector's threshold (hold it in sleep)
    "USB_DN": (19, "23"),
    "USB_DP": (20, "24"),
}
STRAPPING = {0, 3, 45, 46}

U1 = {p: "GND" for p in MODULE_GND}
U1.update({"3": "+3V3", "45": "CHIP_EN"})
U1.update({pin: name for name, (_g, pin) in GPIO.items()})

PARTS = [
    # ---- USB-C: data + VBUS; 5.1 k on each CC (a sink); VBUS joins the reservoir through D3 and R5 (inrush limit)
    ("J1", "usbc", "USB-C 16P", "usbc", "C2765186",
     {"A1B12": "GND", "B1A12": "GND", "13": "GND", "14": "GND", "A4B9": "VBUS", "B4A9": "VBUS", "A5": "CC1", "B5": "CC2",
      "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DN", "B7": "USB_DN"}),
    ("R3", "R", "5.1k", "r0603", "C23186", {"1": "CC1", "2": "GND"}),
    ("R4", "R", "5.1k", "r0603", "C23186", {"1": "CC2", "2": "GND"}),
    ("D3", "D", "B5819W", "sod123", "C8598", {"1": "VUSB_D", "2": "VBUS"}),
    ("R5", "R", "4.7", "r0805", "C17675", {"1": "VUSB_D", "2": "VSYS"}),
    # ---- POWER IN: the gear's MIDI OUT / THRU; ring (pin 4) and tip (pin 5) each through 47 ohm and a Schottky
    ("J3", "jack", "POWER IN", "jack", "C2884940", {"4": "PIN_T", "3": "PIN_R", "1": "GND"}),
    ("P3", "trs", "POWER IN pads", "trs", "", {"T": "PIN_T", "R": "PIN_R", "S": "GND"}),
    ("R1", "R", "47", "r0603", "C23182", {"1": "PIN_R", "2": "PR"}),
    ("R2", "R", "47", "r0603", "C23182", {"1": "PIN_T", "2": "PT"}),
    ("D1", "D", "B5819W", "sod123", "C8598", {"1": "VSYS", "2": "PR"}),
    ("D2", "D", "B5819W", "sod123", "C8598", {"1": "VSYS", "2": "PT"}),
    # ---- the reservoir: 2 x 2200 uF 6.3 V aluminium (4.4 mF), the buck's own 10 uF
    ("C1", "CP", "2200u 6.3V", "can10", "C22383517", {"1": "VSYS", "2": "GND"}),
    ("C2", "CP", "2200u 6.3V", "can10", "C22383517", {"1": "VSYS", "2": "GND"}),
    ("C3", "C", "10u", "c0805", "C15850", {"1": "VSYS", "2": "GND"}),
    # ---- start-up: the detector (4.0 V falling / ~4.16 V rising) or VBUS or PWR_HOLD enables the buck; C4 / R6 keep
    #      it on ~1 s after the detector drops, so the firmware has time to raise PWR_HOLD
    ("U3", "det", "BL8506-40CRM", "det", "C2909397", {"3": "VSYS", "2": "GND", "1": "DET"}),
    ("D4", "bav70", "BAV70", "bav70", "C68978", {"1": "DET", "2": "PWR_HOLD", "3": "BUCK_EN"}),
    ("D5", "bav70", "BAV70", "bav70", "C68978", {"1": "VBUS", "2": "VBUS", "3": "BUCK_EN"}),
    ("C4", "C", "100n", "c0603", "C14663", {"1": "BUCK_EN", "2": "GND"}),
    ("R6", "R", "10M", "r0603", "C7250", {"1": "BUCK_EN", "2": "GND"}),
    # ---- 3.3 V: TPS62842 (60 nA), 3.3 V set by R7 = 51 k (TI table 1: 50.21..54.39 k), power-save mode (MODE low)
    ("U2", "buck", "TPS62842DGRR", "buck", "C2873354",
     {"1": "+3V3", "2": "SW", "3": "GND", "4": "VSET", "5": "BUCK_EN", "6": "VSYS", "8": "GND", "9": "GND"}),
    ("R7", "R", "51k", "r0603", "C23196", {"1": "VSET", "2": "GND"}),
    ("L1", "L", "2.2u", "l0806", "C79317", {"1": "SW", "2": "+3V3"}),
    ("C5", "C", "22u", "c0805", "C45783", {"1": "+3V3", "2": "GND"}),
    ("C6", "C", "100n", "c0603", "C14663", {"1": "+3V3", "2": "GND"}),
    # ---- the module, its reset RC, the button, the LED
    ("U1", "module", "ESP32-S3-MINI-1-N8", "module", "C2913206", U1),
    ("R8", "R", "10k", "r0603", "C25804", {"1": "+3V3", "2": "CHIP_EN"}),
    ("C7", "C", "1u", "c0603", "C15849", {"1": "CHIP_EN", "2": "GND"}),
    ("SW1", "sw", "BOOT", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BOOT", "4": "BOOT"}),
    ("LED1", "led", "red", "led", "C2286", {"1": "LED_A", "2": "GND"}),
    ("R9", "R", "5.1k", "r0603", "C23186", {"1": "LED_DRV", "2": "LED_A"}),
    # ---- the firmware's senses: the reservoir (1 M / 1 M, 100 nF) and VBUS (100 k / 100 k)
    ("R10", "R", "1M", "r0603", "C22935", {"1": "VSYS", "2": "VBANK_ADC"}),
    ("R11", "R", "1M", "r0603", "C22935", {"1": "VBANK_ADC", "2": "GND"}),
    ("C8", "C", "100n", "c0603", "C14663", {"1": "VBANK_ADC", "2": "GND"}),
    ("R12", "R", "100k", "r0603", "C25803", {"1": "VBUS", "2": "VBUS_SENSE"}),
    ("R13", "R", "100k", "r0603", "C25803", {"1": "VBUS_SENSE", "2": "GND"}),
    # ---- MIDI OUT (MIDI 1.0 at 3.3 V, CA-033): ring = source through 33 ohm, tip = sink through 10 ohm from the UART
    ("J2", "jack", "MIDI OUT", "jack", "C2884940", {"4": "MO_T", "3": "MO_R", "1": "GND"}),
    ("P2", "trs", "MIDI OUT pads", "trs", "", {"T": "MO_T", "R": "MO_R", "S": "GND"}),
    ("R14", "R", "33", "r0805", "C17634", {"1": "+3V3", "2": "MO_R"}),
    ("R15", "R", "10", "r0603", "C22859", {"1": "MIDI_TX", "2": "MO_T"}),
]

# what each LCSC code is (README part list; jlc_parts.py adds stock, price and Basic / Extended as read on a day)
LCSC = {
    "C2913206": "Espressif ESP32-S3-MINI-1-N8 (8 MB flash, PCB antenna)",
    "C2765186": "SHOU HAN TYPE-C 16PIN 2MD(073), USB 2.0 receptacle",
    "C2884940": "XKB PJ-320D-B-SMT, 3.5 mm TRS jack, surface mount",
    "C2873354": "TI TPS62842DGRR, 60 nA buck, 750 mA, HVSSOP-8",
    "C2909397": "Belling BL8506-40CRM, 4.0 V detector, push-pull, 0.5 uA",
    "C22383517": "ACMECON GVM0J228M1010CN, 2200 uF 6.3 V aluminium, 10 x 10.5 mm",
    "C79317": "Murata DFE201612P-2R2M, 2.2 uH 1.5 A, 0806",
    "C8598": "CJ B5819W SL, Schottky 40 V 1 A, SOD-123",
    "C68978": "CJ BAV70, dual diode common cathode, SOT-23",
    "C318884": "XKB TS-1187A-B-A-B, tactile switch 5.1 x 5.1 x 1.5 mm",
    "C2286": "KT-0603R, red LED 0603",
    "C23186": "5.1 k 1% 0603", "C23182": "47 ohm 1% 0603", "C17675": "4.7 ohm 1% 0805", "C7250": "10 M 1% 0603",
    "C23196": "51 k 1% 0603", "C25804": "10 k 1% 0603", "C22935": "1 M 1% 0603", "C25803": "100 k 1% 0603",
    "C17634": "33 ohm 1% 0805", "C22859": "10 ohm 1% 0603",
    "C14663": "100 nF 50 V X7R 0603", "C15849": "1 uF 50 V X5R 0603", "C15850": "10 uF 25 V X5R 0805",
    "C45783": "22 uF 25 V X5R 0805",
}


def nets():
    """{net: [ref.pad, ...]}"""
    out = {}
    for ref, _s, _v, _f, _c, pins in PARTS:
        for pad, net in pins.items():
            out.setdefault(net, []).append(f"{ref}.{pad}")
    return out
