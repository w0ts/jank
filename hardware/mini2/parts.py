# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2 (simplified, stage 1c, the user's 2026-11-14): the one parts table. No MIDI power input (no reservoir,
no buck, no detectors): USB or a 1S Li-ion cell (two PH2.0 sockets in parallel, a TP4054 charger, an XB5353A protector,
a P-FET power path) into VSYS, an HT7833 to 3.3 V; MIDI OUT 2 a passive copy of MIDI OUT 1 (J3 / J6
on the same nets as J2 / J5); BOOT a plain button again (SW1); one indicator: a red and a green LED behind one window;
the LINE IN of mini 2 unchanged (README "mini 2.2 simplified"). Below: mini's own words.

jank chip: the one parts table (README "Schematic"). The jank tiny circuit (hardware/tiny) with the ESP32-S3-MINI-1
module replaced by the bare ESP32-S3FN8 (QFN56, 8 MB flash in the package), its crystal, its supply filters and
decoupling (values from the Seeed XIAO ESP32S3 v1.2 schematic, checked against Espressif's checklist), its RF matching
and a YAGEO chip antenna (or a u.FL, chosen by one 0 ohm). make_schematic.py, make_board.py, make_assembly.py and
check_nets.py all read it. Every footprint is JLC's own (EasyEDA, fetched by fetch_jlc.sh into lib/jlc.pretty), so a
part's CPL position and rotation are its footprint's origin and KiCad rotation (make_assembly.py checks every pad).

PARTS: (ref, symbol, value, footprint, lcsc, {pad: net}). A pad left out is not connected.
DNP: parts on the board (pads only) but not fitted: left out of the BOM and the CPL (README "DNP").
"""

FP = {   # footprint names in lib/chip.pretty (copies of lib/jlc.pretty, plus our TRS pads)
    "qfn56": "QFN-56_L7.0-W7.0-P0.40-TL-EP4.0", "xtal": "CRYSTAL-SMD_4P-L3.2-W2.5-BL", "ant": "ANT-SMD_L3.2-W1.6",
    "ufl": "RF-SMD_FRF05002-JSS103M",
    "usbc": "USB-C-SMD_TYPE-C-16PIN-2MD-073", "sot236": "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL", "jack": "AUDIO-TH_PJ-320A-4P-DIP",
    "det": "SOT-23-3_L2.9-W1.6-P1.90-LS2.8-BR",
    "led": "LED-SMD_L1.6-W0.8-R-RD", "sw": "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5",
    "r0603": "R0603", "r0805": "R0805", "r1206": "R1206", "c0603": "C0603",
    "c0805": "C0805", "r0402": "R0402", "c0402": "C0402", "l0402": "L0402", "trs": "TRS_PADS_SMD",
    "din": "CONN-TH_DIN-504B-M10",
    "sot235": "SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BR", "tvs2": "SOT-23_L2.9-W1.3-P1.90-LS2.4-BR",   # (mini 2: LINE IN)
    "led0805": "LED0805-R-RD", "sot25bl": "SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BL", "ph2": "CONN-TH_B2B-PH-K-S",
    "sod123": "SOD-123_L2.7-W1.6-LS3.7-RD-1",   # (mini 2.2: the green LED, the charger, the cells' sockets, D3)
}

# ESP32-S3FN8, QFN56 (ESP32-S3 datasheet, "Pin Overview"; Espressif's checklist "IO Pin Default Configuration"):
#   1 LNA_IN, 2/3 VDD3P3 (the RF supply), 4 CHIP_PU, 5..18 GPIO0..13, 19 GPIO14, 20 VDD3P3_RTC, 21/22 GPIO15/16 (XTAL_32K),
#   23..27 GPIO17..21, 28 SPICS1, 29 VDD_SPI, 30..37 the in-package flash's SPI (SPIHD, SPIWP, SPICS0, SPICLK, SPIQ, SPID,
#   SPICLK_N, SPICLK_P: left open), 38..42 GPIO33..37, 43 GPIO38, 44 MTCK (39), 45 MTDO (40), 46 VDD3P3_CPU, 47 MTDI (41),
#   48 MTMS (42), 49 U0TXD (43), 50 U0RXD (44), 51 GPIO45, 52 GPIO46, 53 XTAL_N, 54 XTAL_P, 55/56 VDDA, 57 GND (the pad)
GPIO = {   # firmware's board.h (README "Pin map"): function -> (GPIO, chip pin)
    "MIDI_TX": (21, "27"),      # UART TX to both MIDI OUTs' tips (one net), through 10 ohm; RTC_GPIO21
    "BOOT": (0, "5"),           # BOOT / user button SW1 to GND (strapping: held low at reset = download mode); R16 its
                                #  pull-up, nothing else on the net (no capacitor: the strap is read at reset)
    "LED_R": (38, "43"),        # the red LED, active high, through 1 k (GPIO38: mini 2's LED pin)
    "LED_G": (39, "44"),        # the green LED, active high, through 1 k (red + green behind one window: both = yellow)
    "LINE_IN": (9, "14"),       # mini 2: ADC1_CH8, the LINE IN front end's output (README "LINE IN")
    "VBAT_ADC": (1, "6"),       # mini 2.2: ADC1_CH0, the cell through 1 M / 1 M (half its voltage); mini 2's VBANK pin
    "USB_DN": (19, "25"),
    "USB_DP": (20, "26"),
}
STRAPPING = {0, 3, 45, 46}
CHIP_PIN = {"1": "LNA_IN", "2": "VDD3P3", "3": "VDD3P3", "4": "CHIP_PU", "20": "+3V3", "29": "VDD_SPI", "46": "+3V3",
            "53": "XTAL_N", "54": "XTAL_P", "55": "+3V3", "56": "+3V3", "57": "GND"}

U1 = dict(CHIP_PIN)
U1.update({pin: name for name, (_g, pin) in GPIO.items()})

DNP = {"C24", "C25", "C26", "R18",   # (pads only: the matching positions, the chip antenna's 0 ohm)
       "J5", "J6",                            # (the DIN sockets, bottom side: an optional hand-solder, README "DIN")
       "RT40", "RT41",                        # (mini 2.2: the LED resistors' twins, README "mini 2.2")
       "J8", "J9"}                            # (mini 2.2: the cells' PH2.0 sockets: the user fits them from the bottom)
# JLC's footprint for the DIN-504B-M10 numbers its pads 1..5 along the two rows, not as the DIN pins. Its frame and
# hardware/pcb's own table for the part (HY_PINS, the CUI SDS-50J pattern; the same in hardware/pcb/lib) agree: the
# shell pins at the front, the 3-pin row 2.5 mm before the 2-pin row, x the same way. So pad 1 (x -7.5) = DIN 3,
# pad 2 (x -5.0) = DIN 5, pad 3 (x 0) = DIN 2 (the middle), pad 4 (x +5.0) = DIN 4, pad 5 (x +7.5) = DIN 1; pads 6, 7 =
# the shell. (Until 2026-11-12 this was read mirrored: pad 2 = DIN 4, pad 4 = DIN 5.) UNVERIFIED: check with a meter.

PARTS = [
    # ---- USB-C: data + VBUS; 5.1 k on each CC (a sink); VBUS straight to the LDO (mini 2.2 simplified: USB power only)
    ("J1", "usbc", "USB-C 16P", "usbc", "C2765186",
     {"A1B12": "GND", "B1A12": "GND", "13": "GND", "14": "GND", "A4B9": "VBUS", "B4A9": "VBUS", "A5": "CC1", "B5": "CC2",
      "A6": "UDP", "B6": "UDP", "A7": "UDN", "B7": "UDN"}),
    # (22 ohm in series on D+ / D- by the chip: the edges and ringing on the 22 mm tracks, Espressif's checklist)
    # (ESD on D+ / D- right at the USB-C, before the 22 ohm: the user's ruling 2026-11-12; VBUS only its clamp reference)
    ("U5", "esd", "USBLC6-2SC6", "sot236", "C7519", {"1": "UDP", "6": "UDP", "3": "UDN", "4": "UDN", "2": "GND", "5": "VBUS"}),
    ("R24", "R", "22", "r0402", "C25092", {"1": "UDP", "2": "USB_DP"}),
    ("R25", "R", "22", "r0402", "C25092", {"1": "UDN", "2": "USB_DN"}),
    ("R3", "R", "5.1k", "r0603", "C23186", {"1": "CC1", "2": "GND"}),
    ("R4", "R", "5.1k", "r0603", "C23186", {"1": "CC2", "2": "GND"}),
    # ---- power (mini 2.2 simplified with the battery, the user's 2026-11-14): VBUS through D3 into VSYS; the cell into
    #      VSYS through Q1 (a P-FET, on while VBUS is absent); VSYS into U2 HT7833 (3.3 V, 500 mA, 4 uA, dropout 360 mV at
    #      500 mA: runs from USB and from the cell down to ~3.6 V at full load); CE tied to VIN
    ("D3", "D", "B5819W", "sod123", "C8598", {"1": "VSYS", "2": "VBUS"}),
    ("U2", "ldo5", "HT7833", "sot235", "C164106", {"1": "VSYS", "3": "VSYS", "2": "GND", "5": "+3V3"}),
    ("C3", "C", "10u", "c0603", "C19702", {"1": "VSYS", "2": "GND"}),
    ("C5", "C", "22u", "c0603", "C59461", {"1": "+3V3", "2": "GND"}),
    ("C6", "C", "100n", "c0402", "C1525", {"1": "+3V3", "2": "GND"}),
    #      the power path: Q1 AO3401A, drain on the cell (BAT_P), source on VSYS, gate on VBUS (R50 100 k down): no USB =
    #      gate low, on (its body diode already conducts the same way); USB = gate 5 V, VSYS ~4.7 V above the cell: off
    ("Q1", "pfet", "AO3401A", "tvs2", "C15127", {"1": "VBUS", "2": "VSYS", "3": "BAT_P"}),
    ("R50", "R", "100k", "r0402", "C25741", {"1": "VBUS", "2": "GND"}),
    #      the charger: U8 TP4054-42 (4.2 V) from VBUS, R51 5.1 k on PROG = ~196 mA (1000 V / R_PROG), CHRG left open;
    #      10 uF at VCC and at BAT
    ("U8", "chg", "TP4054-42", "sot25bl", "C32574", {"4": "VBUS", "3": "BAT_P", "2": "GND", "5": "PROG"}),
    ("R51", "R", "5.1k", "r0603", "C23186", {"1": "PROG", "2": "GND"}),
    ("C40", "C", "10u", "c0603", "C19702", {"1": "VBUS", "2": "GND"}),
    ("C41", "C", "10u", "c0603", "C19702", {"1": "BAT_P", "2": "GND"}),
    #      the cell's protection: U9 XB5353A (over-charge, over-discharge, over-current, its own FET in the minus lead):
    #      VDD on the cell's +, its GND on the cell's minus (BAT_N), VM (4, 5) on the board's GND; 100 nF VDD to BAT_N;
    #      pin 1 (VT) left open (EasyEDA's symbol; its datasheet not read: UNVERIFIED)
    ("U9", "prot", "XB5353A", "sot235", "C80628", {"3": "BAT_P", "2": "BAT_N", "4": "GND", "5": "GND"}),
    ("C42", "C", "100n", "c0402", "C1525", {"1": "BAT_P", "2": "BAT_N"}),
    #      the cells: two PH2.0 sockets in parallel (the user's choice, risk accepted), not fitted: the user inserts them
    #      from the bottom (the cells under the board) and solders them on top. Pin 1 = -, pin 2 = + (UNVERIFIED against
    #      whoop PH2.0 leads: the silk marks + and - on both sides; check a lead with a meter first)
    ("J8", "ph2", "PH2.0 cell 1", "ph2", "C131337", {"1": "BAT_N", "2": "BAT_P"}),
    ("J9", "ph2", "PH2.0 cell 2", "ph2", "C131337", {"1": "BAT_N", "2": "BAT_P"}),
    #      the cell's level: 1 M / 1 M + 100 nF on GPIO1 (ADC1_CH0), from the cell's + (mini 2's R10 / R11 / C8)
    ("R10", "R", "1M", "r0402", "C26083", {"1": "BAT_P", "2": "VBAT_ADC"}),
    ("R11", "R", "1M", "r0402", "C26083", {"1": "VBAT_ADC", "2": "GND"}),
    ("C8", "C", "100n", "c0402", "C1525", {"1": "VBAT_ADC", "2": "GND"}),
    # ---- the chip (ESP32-S3FN8) and its supplies (XIAO ESP32S3 v1.2 values; Espressif's checklist)
    ("U1", "chip", "ESP32-S3FN8", "qfn56", "C2913196", U1),
    #      VDDA (55, 56): 2.2 uF + 100 nF + 10 nF
    ("C11", "C", "2.2u", "c0402", "C12530", {"1": "+3V3", "2": "GND"}),
    ("C12", "C", "100n", "c0402", "C1525", {"1": "+3V3", "2": "GND"}),
    #      VDD3P3 right at its pins: 100 nF (the 2 nH's own 100 nF, C18, is past it)
    ("C28", "C", "100n", "c0402", "C1525", {"1": "VDD3P3", "2": "GND"}),
    ("C13", "C", "10n", "c0402", "C15195", {"1": "+3V3", "2": "GND"}),
    #      VDD3P3 (2, 3, the RF supply): 10 uF (the checklist) + 2 x 2.2 uF + 100 nF, the 2 nH (>= 500 mA), 100 nF after
    ("C17", "C", "10u", "c0603", "C19702", {"1": "+3V3", "2": "GND"}),
    ("C14", "C", "2.2u", "c0402", "C12530", {"1": "+3V3", "2": "GND"}),
    ("C15", "C", "2.2u", "c0402", "C12530", {"1": "+3V3", "2": "GND"}),
    ("C16", "C", "100n", "c0402", "C1525", {"1": "+3V3", "2": "GND"}),
    ("L2", "L", "2nH", "l0402", "C76769", {"1": "+3V3", "2": "VDD3P3"}),
    ("C18", "C", "100n", "c0402", "C1525", {"1": "VDD3P3", "2": "GND"}),
    #      VDD3P3_CPU (46), VDD3P3_RTC (20): 100 nF each; VDD_SPI (29, the flash's 3.3 V): 100 nF + 1 uF
    ("C19", "C", "100n", "c0402", "C1525", {"1": "+3V3", "2": "GND"}),
    ("C20", "C", "100n", "c0402", "C1525", {"1": "+3V3", "2": "GND"}),
    ("C21", "C", "100n", "c0402", "C1525", {"1": "VDD_SPI", "2": "GND"}),
    ("C22", "C", "1u", "c0402", "C52923", {"1": "VDD_SPI", "2": "GND"}),
    #      CHIP_PU: 10 k / 1 uF (Espressif's RC; mini 2.2 simplified: no supervisor, +3V3 now rises with the LDO, fast)
    #      GPIO0: 10 k pull-up (the checklist), the button to GND, nothing else
    ("R8", "R", "10k", "r0402", "C25744", {"1": "+3V3", "2": "CHIP_PU"}),
    ("C7", "C", "1u", "c0402", "C52923", {"1": "CHIP_PU", "2": "GND"}),
    ("R16", "R", "10k", "r0402", "C25744", {"1": "+3V3", "2": "BOOT"}),
    #      40 MHz crystal, 8 pF load, +-10 ppm; 10 pF each side (C_L = 10 x 10 / 20 + ~2.5 pF stray = ~7.5 pF: README),
    #      0 ohm in series on XTAL_P (XIAO)
    ("Y1", "xtal", "40MHz 8pF", "xtal", "C254377", {"1": "XTAL_X1", "3": "XTAL_N", "2": "GND", "4": "GND"}),
    ("R17", "R", "0", "r0402", "C17168", {"1": "XTAL_X1", "2": "XTAL_P"}),
    ("C9", "C", "10p", "c0402", "C32949", {"1": "XTAL_X1", "2": "GND"}),
    ("C10", "C", "10p", "c0402", "C32949", {"1": "XTAL_N", "2": "GND"}),
    # ---- RF: the chip's matching (pi: 1 pF shunt at the chip, 3.3 nH series, a second shunt unfitted; XIAO), the 50 ohm
    #      line, then the antenna's own pi (two shunts unfitted, the series a 0 ohm that selects the chip antenna) or,
    #      with R18 moved to R19, the u.FL (J4, unfitted)
    ("C23", "C", "1p", "c0402", "C1550", {"1": "LNA_IN", "2": "GND"}),
    ("L3", "L", "3.3nH", "l0402", "C87532", {"1": "LNA_IN", "2": "RF"}),
    ("C24", "C", "DNP", "c0402", "", {"1": "RF", "2": "GND"}),
    ("C25", "C", "DNP", "c0402", "", {"1": "RF", "2": "GND"}),
    ("R18", "R", "0 DNP", "r0402", "C17168", {"1": "RF", "2": "ANT_FEED"}),
    ("C26", "C", "DNP", "c0402", "", {"1": "ANT_FEED", "2": "GND"}),
    ("AE1", "ant", "ANT3216LL00R2400A", "ant", "C293767", {"1": "ANT_FEED", "2": "ANT_END"}),   # (2: the end pad, its copper extended, unconnected)
    ("R19", "R", "0", "r0402", "C17168", {"1": "RF", "2": "UFL"}),
    ("J4", "ufl", "U.FL", "ufl", "C88374", {"1": "UFL", "2": "GND", "3": "GND"}),
    # ---- the buttons (mini 2.2 simplified: BOOT a plain button again, a Basic SMD tactile under the case's cap)
    ("SW1", "sw", "BOOT", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BOOT", "4": "BOOT"}),
    ("SW2", "sw", "RST", "sw", "C318884", {"1": "GND", "2": "GND", "3": "CHIP_PU", "4": "CHIP_PU"}),   # (reset: CHIP_PU to ground)
    # ---- the indicator (mini 2.2 simplified): a red 0603 and a green 0805 LED side by side behind one window (both
    #      Basic: no type fee; both on = yellow), each from its GPIO through 1 k; JLC's pad 1 = anode for both (EasyEDA)
    ("LED1", "led", "red", "led", "C2286", {"1": "LED_RA", "2": "GND"}),
    ("R40", "R", "1k", "r0402", "C11702", {"1": "LED_R", "2": "LED_RA"}),
    ("LED2", "led", "green", "led0805", "C2297", {"1": "LED_GA", "2": "GND"}),
    ("R41", "R", "1k", "r0402", "C11702", {"1": "LED_G", "2": "LED_GA"}),
    #      the twins (the user, 2026-11-14; as hardware/sd-board's in the optimist repo): a bare pad pair in parallel with
    #      each LED resistor, not fitted (no part, no paste, out of the BOM and the CPL: no cost): a second resistor
    #      soldered by hand there brightens that LED (1 k // 1 k = 500 ohm). 0603 pads: an 0402 or an 0603 fits
    ("RT40", "R", "twin DNP", "r0603", "", {"1": "LED_R", "2": "LED_RA"}),
    ("RT41", "R", "twin DNP", "r0603", "", {"1": "LED_G", "2": "LED_GA"}),
    # ---- MIDI OUT 1 and MIDI OUT 2 (mini 2.2 simplified: OUT 2 is a passive copy: J3 / J6 on the same nets as J2 / J5)
    #      MIDI 1.0 at 3.3 V (CA-033): ring = source through 33 ohm (3 x 100 ohm 1206 in parallel: 33.3 ohm, all Basic;
    #      a mono plug's 0.33 W shared, 0.11 W each), tip = sink through 10 ohm from the UART
    ("J2", "jack", "MIDI OUT", "jack", "C18185602", {"D": "MO_T", "C": "MO_R", "A": "GND"}),
    ("J5", "din", "MIDI OUT DIN", "din", "C2939346", {"4": "MO_R", "2": "MO_T", "3": "GND", "6": "GND", "7": "GND"}),   # (pad 4 = DIN 4, pad 2 = DIN 5)
    ("J3", "jack", "MIDI OUT 2", "jack", "C18185602", {"D": "MO_T", "C": "MO_R", "A": "GND"}),
    ("J6", "din", "MIDI OUT 2 DIN", "din", "C2939346", {"4": "MO_R", "2": "MO_T", "3": "GND", "6": "GND", "7": "GND"}),
    ("R14", "R", "100", "r1206", "C17901", {"1": "+3V3", "2": "MO_R"}),   # (R14 // R26 // R48 = 33.3 ohm)
    ("R26", "R", "100", "r1206", "C17901", {"1": "+3V3", "2": "MO_R"}),
    ("R48", "R", "100", "r1206", "C17901", {"1": "+3V3", "2": "MO_R"}),
    ("R15", "R", "10", "r0603", "C22859", {"1": "MIDI_TX", "2": "MO_T"}),
    # ---- mini 2: LINE IN (README "LINE IN"): a 3.5 mm stereo jack, tip (L) and ring (R) summed to mono by an inverting
    #      summer (each channel 20 k into the op-amp's virtual ground: >= 20 k input impedance, no crosstalk between the
    #      sources), gain 5.6 k / 20 k = 0.28 a channel, -3 dB at ~8.5 kHz (C32 and R35 / C36), AC coupled (7.8 Hz),
    #      biased at 1.47 V (the middle of ATTEN3's 0..2.9 V); powered from VBUS by its own LDO
    ("J7", "jack", "LINE IN", "jack", "C18185602", {"D": "LIN_T", "C": "LIN_R", "A": "GND"}),
    ("D7", "tvs2", "PESD5V0S2BT", "tvs2", "C49338", {"1": "LIN_T", "2": "LIN_R", "3": "GND"}),   # (ESD, both lines, bidirectional 5 V)
    ("R30", "R", "20k", "r0402", "C25765", {"1": "LIN_T", "2": "LIN_A1"}),
    ("C30", "C", "1u", "c0402", "C52923", {"1": "LIN_A1", "2": "LIN_SUM"}),
    ("R31", "R", "20k", "r0402", "C25765", {"1": "LIN_R", "2": "LIN_A2"}),
    ("C31", "C", "1u", "c0402", "C52923", {"1": "LIN_A2", "2": "LIN_SUM"}),
    ("R32", "R", "5.6k", "r0402", "C25908", {"1": "LIN_SUM", "2": "LIN_OUT"}),    # (the gain: 5.6 k / 20 k)
    ("C32", "C", "2.2n", "c0402", "C1531", {"1": "LIN_SUM", "2": "LIN_OUT"}),     # (the first pole, 12.9 kHz)
    ("U6", "opamp", "LMV321IDBVR", "sot235", "C7972",
     {"1": "LIN_VB", "2": "GND", "3": "LIN_SUM", "4": "LIN_OUT", "5": "AVDD"}),   # (TI DBV: 1 IN+, 2 GND, 3 IN-, 4 OUT, 5 V+)
    ("R33", "R", "150k", "r0402", "C25755", {"1": "AVDD", "2": "LIN_VB"}),        # (the bias: 3.3 V x 120 / 270 = 1.47 V)
    ("R34", "R", "120k", "r0402", "C25750", {"1": "LIN_VB", "2": "GND"}),
    ("C33", "C", "1u", "c0402", "C52923", {"1": "LIN_VB", "2": "GND"}),
    ("U7", "ldo", "XC6206P332MR", "det", "C5446", {"3": "VSYS", "2": "AVDD", "1": "GND"}),   # (Torex SOT-23: 1 VSS, 2 VOUT, 3 VIN; mini 2.2: on VSYS, so the LINE IN works on the cell too)
    ("C34", "C", "1u", "c0402", "C52923", {"1": "VSYS", "2": "GND"}),
    ("C35", "C", "1u", "c0402", "C52923", {"1": "AVDD", "2": "GND"}),
    ("R35", "R", "120", "r0402", "C25079", {"1": "LIN_OUT", "2": "LINE_IN"}),     # (the second pole, 13.3 kHz, with C36)
    ("C36", "C", "100n", "c0402", "C1525", {"1": "LINE_IN", "2": "GND"}),          # (at the pin: Espressif's 100 nF)
]

# what each LCSC code is (README part list; jlc_parts.py adds stock, price and Basic / Extended as read on a day)
LCSC = {
    "C2913196": "Espressif ESP32-S3FN8, QFN56 7 x 7, 8 MB flash in the package",
    "C254377": "HELE X3S040000B81H-R, 40 MHz crystal 3225, 8 pF load, +-10 ppm, +-10 ppm over -20..75 C",
    "C293767": "YAGEO ANT3216LL00R2400A, 2.4 GHz ceramic chip antenna 3.2 x 1.6 mm",
    "C7519": "ST USBLC6-2SC6, USB 2.0 ESD protection, 2 lines + VBUS, SOT-23-6",
    "C88374": "Hirose U.FL-R-SMT-1(80), u.FL receptacle (fitted: the default antenna path, the user's ruling 2026-11-11)",
    "C2939346": "HOOYA DIN-504B-M10, 5-pin DIN socket, female, right angle, through hole (DNP: hand-soldered, bottom side)",
    "C76769": "TDK MLG1005S2N0BT000, 2.0 nH 900 mA 0402 (the VDD3P3 filter)",
    "C87532": "TDK MLG1005S3N3BT000, 3.3 nH 800 mA 0402 (the RF match)",
    "C1550": "FH 0402CG1R0C500NT, 1 pF C0G 0402 (the RF match; a preferred Extended part: no loading fee)",
    "C2765186": "SHOU HAN TYPE-C 16PIN 2MD(073), USB 2.0 receptacle",
    # (the jacks: the PJ-320D's through-hole twin, same body, the pins at the same places; pads A = sleeve, C = ring,
    #  D = tip, B = the switch; the user's ruling 2026-11-12: stronger, JLC solders it)
    "C18185602": "SHOU HAN PJ-320A-4P DIP, 3.5 mm jack, through hole",
    "C318884": "XKB TS-1187A-B-A-B, tactile switch 5.1 x 5.1 x 1.5 mm",
    "C2286": "KT-0603R, red LED 0603",
    "C23186": "5.1 k 1% 0603", "C25092": "22 ohm 5% 0402",
    "C22859": "10 ohm 1% 0603", "C17168": "0 ohm 0402", "C25744": "10 k 1% 0402",
    "C59461": "22 uF 6.3 V X5R 0603", "C19702": "10 uF 10 V X5R 0603",
    "C1525": "100 nF 16 V X7R 0402", "C12530": "2.2 uF 6.3 V X5R 0402", "C15195": "10 nF 50 V X7R 0402",
    "C52923": "1 uF 25 V X5R 0402", "C32949": "10 pF 50 V C0G 0402",
    # mini 2, LINE IN
    "C7972": "TI LMV321IDBVR, op-amp, rail-to-rail output, 1 MHz, 130 uA, 2.7..5.5 V, SOT-23-5 (a preferred Extended part)",
    "C5446": "Torex XC6206P332MR, 3.3 V LDO, 1 uA, 200 mA, SOT-23",
    "C49338": "Nexperia PESD5V0S2BT, two-line bidirectional ESD protection, 5 V, 35 pF, SOT-23",
    "C25765": "20 k 1% 0402", "C25908": "5.6 k 1% 0402", "C25755": "150 k 1% 0402", "C25750": "120 k 1% 0402",
    "C25079": "120 ohm 1% 0402", "C1531": "2.2 nF 50 V X7R 0402",
    # mini 2.2 simplified
    "C11702": "1 k 1% 0402",
    "C164106": "Holtek HT7833, 3.3 V LDO, 500 mA, dropout 360 mV at 500 mA, 4 uA, 8 V max, SOT-23-5",
    "C32574": "TP4054-42-SOT25R, 1-cell Li-ion charger 4.2 V, up to 500 mA, SOT-23-5 (a preferred Extended part)",
    "C15127": "AOS AO3401A, P-MOSFET 30 V 4 A, SOT-23 (the power path)",
    "C80628": "Xysemi XB5353A, 1-cell Li-ion protection with its own MOSFET, SOT-23-5",
    "C131337": "JST B2B-PH-K-S, PH2.0 2-pin socket, through hole (DNP: the user fits it from the bottom)",
    "C8598": "CJ B5819W SL, Schottky 40 V 1 A, SOD-123", "C26083": "1 M 1% 0402", "C25741": "100 k 1% 0402",
    "C2297": "KT-0805G, emerald green LED 0805, Vf 2.6..3.1 V at 5 mA (Basic)",
    "C17901": "100 ohm 1% 1206, 0.25 W (Basic)",
}


def fitted(ref):
    return ref not in DNP


def nets():
    """{net: [ref.pad, ...]}"""
    out = {}
    for ref, _s, _v, _f, _c, pins in PARTS:
        for pad, net in pins.items():
            out.setdefault(net, []).append(f"{ref}.{pad}")
    return out
