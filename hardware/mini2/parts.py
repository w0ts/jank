# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2: the one parts table. jank mini (hardware/chip) unchanged, plus a LINE IN (README "LINE IN"). Below: mini's own words.

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
    "buck": "HVSSOP-8_L3.0-W3.0-P0.65-LS4.9-BL-EP", "det": "SOT-23-3_L2.9-W1.6-P1.90-LS2.8-BR",
    "bav70": "SOT-23-3_L3.0-W1.7-P0.95-LS2.9-BR", "sod123": "SOD-123_L2.7-W1.6-LS3.7-RD-1",
    "led": "LED-SMD_L1.6-W0.8-R-RD", "sw": "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5", "l0806": "L0806",
    "can10": "CAP-SMD_BD10.0-L10.3-W10.3-LS10.9-FD", "det_tl": "SOT-23-3_L2.9-W1.6-P1.90-LS2.8-TL", "r0603": "R0603", "r0805": "R0805", "r1206": "R1206", "r2010": "R2010", "c0603": "C0603",
    "c0805": "C0805", "r0402": "R0402", "c0402": "C0402", "l0402": "L0402", "trs": "TRS_PADS_SMD",
    "sot23fet": "SOT-23-3_L2.9-W1.3-P1.90-LS2.4-BR",
    "din": "CONN-TH_DIN-504B-M10",
    "sot235": "SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BR", "tvs2": "SOT-23_L2.9-W1.3-P1.90-LS2.4-BR",   # (mini 2: LINE IN)
}

# ESP32-S3FN8, QFN56 (ESP32-S3 datasheet, "Pin Overview"; Espressif's checklist "IO Pin Default Configuration"):
#   1 LNA_IN, 2/3 VDD3P3 (the RF supply), 4 CHIP_PU, 5..18 GPIO0..13, 19 GPIO14, 20 VDD3P3_RTC, 21/22 GPIO15/16 (XTAL_32K),
#   23..27 GPIO17..21, 28 SPICS1, 29 VDD_SPI, 30..37 the in-package flash's SPI (SPIHD, SPIWP, SPICS0, SPICLK, SPIQ, SPID,
#   SPICLK_N, SPICLK_P: left open), 38..42 GPIO33..37, 43 GPIO38, 44 MTCK (39), 45 MTDO (40), 46 VDD3P3_CPU, 47 MTDI (41),
#   48 MTMS (42), 49 U0TXD (43), 50 U0RXD (44), 51 GPIO45, 52 GPIO46, 53 XTAL_N, 54 XTAL_P, 55/56 VDDA, 57 GND (the pad)
GPIO = {   # firmware's board.h (README "Pin map"): function -> (GPIO, chip pin); the GPIOs are tiny's, unchanged
    "MIDI_TX": (21, "27"),      # UART TX to the MIDI OUT jack's tip, through 10 ohm; RTC_GPIO21 (the ULP can drive it)
    "LED_DRV": (38, "43"),      # the LED, active high, through 5.1 k
    "BOOT": (0, "5"),           # BOOT / user button to GND (strapping: held low at reset = download mode)
    "VBANK_ADC": (1, "6"),      # ADC1_CH0: the reservoir through 1 M / 1 M (half its voltage)
    "VBUS_SENSE": (2, "7"),     # ADC1_CH1: USB VBUS through 100 k / 100 k (2.5 V when USB is plugged)
    "PWR_HOLD": (4, "9"),       # RTC_GPIO4: high keeps the buck on below the detector's threshold (hold it in sleep)
    "MIDI2_TX": (10, "15"),     # the second MIDI OUT on the POWER IN port (USB power only): the gate of Q1, high = current
                                #  = a 0 bit (UART TX inverted in the GPIO matrix); R22 holds it low in reset and sleep
    "LINE_IN": (9, "14"),       # mini 2: ADC1_CH8, the LINE IN front end's output (README "LINE IN"); the right side's
                                #  top corner pin: the only ADC1 pin (GPIO5..9) with a way out past the fan-out lanes
    "USB_DN": (19, "25"),
    "USB_DP": (20, "26"),
}
STRAPPING = {0, 3, 45, 46}
CHIP_PIN = {"1": "LNA_IN", "2": "VDD3P3", "3": "VDD3P3", "4": "CHIP_PU", "20": "+3V3", "29": "VDD_SPI", "46": "+3V3",
            "53": "XTAL_N", "54": "XTAL_P", "55": "+3V3", "56": "+3V3", "57": "GND"}

U1 = dict(CHIP_PIN)
U1.update({pin: name for name, (_g, pin) in GPIO.items()})

DNP = {"C24", "C25", "C26", "R18",   # (pads only: the matching positions, the chip antenna's 0 ohm)
       "J5", "J6"}                            # (the DIN sockets, bottom side: an optional hand-solder, README "DIN")
# JLC's footprint for the DIN-504B-M10 numbers its pads 1..5 along the two rows, not as the DIN pins. Its frame and
# hardware/pcb's own table for the part (HY_PINS, the CUI SDS-50J pattern; the same in hardware/pcb/lib) agree: the
# shell pins at the front, the 3-pin row 2.5 mm before the 2-pin row, x the same way. So pad 1 (x -7.5) = DIN 3,
# pad 2 (x -5.0) = DIN 5, pad 3 (x 0) = DIN 2 (the middle), pad 4 (x +5.0) = DIN 4, pad 5 (x +7.5) = DIN 1; pads 6, 7 =
# the shell. (Until 2026-11-12 this was read mirrored: pad 2 = DIN 4, pad 4 = DIN 5.) UNVERIFIED: check with a meter.

PARTS = [
    # ---- USB-C: data + VBUS; 5.1 k on each CC (a sink); VBUS joins the reservoir through D3 and R5 (inrush limit)
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
    ("D3", "D", "B5819W", "sod123", "C8598", {"1": "VUSB_D", "2": "VBUS"}),
    ("R5", "R", "4.7", "r2010", "C474307", {"1": "VUSB_D", "2": "VSYS"}),   # (4.7 ohm: 10 would starve USB; a 2010, 0.75 W: the plug-in pulse, 0.9 A / 45 mJ in ~20 ms, sim 2026-11-12)
    # ---- POWER IN: the gear's MIDI OUT / THRU; ring (pin 4) and tip (pin 5) each through 47 ohm and a Schottky
    ("J3", "jack", "POWER IN", "jack", "C18185602", {"D": "PIN_T", "C": "PIN_R", "A": "GND"}),
    ("J6", "din", "POWER IN DIN", "din", "C2939346", {"4": "PIN_R", "2": "PIN_T", "3": "GND"}),   # (pad 4 = DIN 4, pad 2 = DIN 5)
    # ---- the second MIDI OUT on the same port, from USB power only (README "MIDI OUT 2"): ring = +3V3 through 33 ohm
    #      and a Schottky (a gear's 5 V there never reaches +3V3), tip = an open drain through 10 ohm (off: nothing)
    ("R21", "R", "68", "r1206", "C17976", {"1": "+3V3", "2": "M2_A"}),   # (R21 // R27 = 34 ohm, 2 x 1206: 0.26 W into a mono plug)
    ("R27", "R", "68", "r1206", "C17976", {"1": "+3V3", "2": "M2_A"}),
    ("D6", "D", "B5819W", "sod123", "C8598", {"1": "PIN_R", "2": "M2_A"}),
    ("Q1", "nfet", "AO3400A", "sot23fet", "C20917", {"1": "MIDI2_TX", "2": "GND", "3": "M2_D"}),
    ("R20", "R", "10", "r0603", "C22859", {"1": "M2_D", "2": "PIN_T"}),   # (0603 as R15: a DIN net keeps 0.5 mm, an 0402's pads are closer)
    ("R22", "R", "100k", "r0402", "C25741", {"1": "MIDI2_TX", "2": "GND"}),
    ("R1", "R", "47", "r0603", "C23182", {"1": "PIN_R", "2": "PR"}),
    ("R2", "R", "47", "r0603", "C23182", {"1": "PIN_T", "2": "PT"}),
    ("D1", "D", "B5819W", "sod123", "C8598", {"1": "VSYS", "2": "PR"}),
    ("D2", "D", "B5819W", "sod123", "C8598", {"1": "VSYS", "2": "PT"}),
    # ---- the reservoir: 2 x 2200 uF 10 V aluminium (4.4 mF, the user's ruling 2026-11-11), the buck's own 10 uF
    ("C1", "CP", "2200u 10V", "can10", "C51018335", {"1": "VSYS", "2": "GND"}),
    ("C2", "CP", "2200u 10V", "can10", "C51018335", {"1": "VSYS", "2": "GND"}),
    ("C3", "C", "10u", "c0603", "C19702", {"1": "VSYS", "2": "GND"}),
    # ---- start-up: the detector (4.0 V falling / ~4.16 V rising) or VBUS or PWR_HOLD enables the buck; C4 / R6 keep
    #      it on ~1 s after the detector drops, so the firmware has time to raise PWR_HOLD
    ("U3", "det", "BL8506-40CRM", "det", "C2909397", {"3": "VSYS", "2": "GND", "1": "DET"}),
    ("D4", "bav70", "BAV70", "bav70", "C68978", {"1": "DET", "2": "PWR_HOLD", "3": "BUCK_EN"}),
    ("D5", "bav70", "BAV70", "bav70", "C68978", {"1": "VBUS", "2": "VBUS", "3": "BUCK_EN"}),
    ("C4", "C", "100n", "c0402", "C1525", {"1": "BUCK_EN", "2": "GND"}),
    ("R6", "R", "10M", "r0603", "C7250", {"1": "BUCK_EN", "2": "GND"}),
    # ---- 3.3 V: TPS62842 (60 nA), 3.3 V set by R7 = 51 k (TI table 1: 50.21..54.39 k), power-save mode (MODE low)
    ("U2", "buck", "TPS62842DGRR", "buck", "C2873354",
     {"1": "+3V3", "2": "SW", "3": "GND", "4": "VSET", "5": "BUCK_EN", "6": "VSYS", "8": "GND", "9": "GND"}),
    ("R7", "R", "51k", "r0402", "C25794", {"1": "VSET", "2": "GND"}),
    ("L1", "L", "2.2u", "l0806", "C79317", {"1": "SW", "2": "+3V3"}),
    ("C5", "C", "22u", "c0603", "C59461", {"1": "+3V3", "2": "GND"}),
    ("C6", "C", "100n", "c0402", "C1525", {"1": "+3V3", "2": "GND"}),
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
    #      CHIP_PU: 10 k / 1 uF (Espressif); GPIO0: 10 k pull-up (the checklist), the button to GND
    ("R8", "R", "10k", "r0402", "C25744", {"1": "+3V3", "2": "CHIP_PU"}),
    ("C7", "C", "1u", "c0402", "C52923", {"1": "CHIP_PU", "2": "GND"}),
    ("R16", "R", "10k", "r0402", "C25744", {"1": "+3V3", "2": "BOOT"}),
    #      (DNP) pads for a 3.0 V supervisor on CHIP_PU (open drain, R8 the pull-up): README "Start-up"
    ("U4", "det", "BL8506-30NRM", "det_tl", "C2763866", {"3": "+3V3", "2": "GND", "1": "CHIP_PU"}),   # (fitted 2026-11-12: holds CHIP_PU low until +3V3 > 3.0 V)
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
    # ---- the button, the LED
    ("SW1", "sw", "BOOT", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BOOT", "4": "BOOT"}),
    ("SW2", "sw", "RST", "sw", "C318884", {"1": "GND", "2": "GND", "3": "CHIP_PU", "4": "CHIP_PU"}),   # (reset: CHIP_PU to ground)
    ("LED1", "led", "red", "led", "C2286", {"1": "LED_A", "2": "GND"}),
    ("R9", "R", "5.1k", "r0603", "C23186", {"1": "LED_DRV", "2": "LED_A"}),
    # ---- the firmware's senses: the reservoir (1 M / 1 M, 100 nF) and VBUS (100 k / 100 k)
    ("R10", "R", "1M", "r0402", "C26083", {"1": "VSYS", "2": "VBANK_ADC"}),
    ("R11", "R", "1M", "r0402", "C26083", {"1": "VBANK_ADC", "2": "GND"}),
    ("C8", "C", "100n", "c0402", "C1525", {"1": "VBANK_ADC", "2": "GND"}),
    ("R12", "R", "100k", "r0603", "C25803", {"1": "VBUS", "2": "VBUS_SENSE"}),
    ("R13", "R", "100k", "r0603", "C25803", {"1": "VBUS_SENSE", "2": "GND"}),
    # ---- VBUS's pull-down: without USB, D3's leakage could float VBUS up through R12 / R13 and lift BUCK_EN via D5
    ("R23", "R", "10k", "r0603", "C25804", {"1": "VBUS", "2": "GND"}),   # (10 k: D3's leakage must not lift BUCK_EN via D5, sim 2026-11-12)
    # ---- MIDI OUT (MIDI 1.0 at 3.3 V, CA-033): ring = source through 33 ohm, tip = sink through 10 ohm from the UART
    ("J2", "jack", "MIDI OUT", "jack", "C18185602", {"D": "MO_T", "C": "MO_R", "A": "GND"}),
    ("J5", "din", "MIDI OUT DIN", "din", "C2939346", {"4": "MO_R", "2": "MO_T", "3": "GND", "6": "GND", "7": "GND"}),   # (pad 4 = DIN 4, pad 2 = DIN 5)
    ("R14", "R", "68", "r1206", "C17976", {"1": "+3V3", "2": "MO_R"}),   # (R14 // R26 = 34 ohm, 2 x 1206: a mono plug shorts the ring, 0.33 W)
    ("R26", "R", "68", "r1206", "C17976", {"1": "+3V3", "2": "MO_R"}),
    ("R15", "R", "10", "r0603", "C22859", {"1": "MIDI_TX", "2": "MO_T"}),
    # ---- mini 2: LINE IN (README "LINE IN"): a 3.5 mm stereo jack, tip (L) and ring (R) summed to mono by an inverting
    #      summer (each channel 20 k into the op-amp's virtual ground: >= 20 k input impedance, no crosstalk between the
    #      sources), gain 5.6 k / 20 k = 0.28 a channel, -3 dB at ~8.5 kHz (C32 and R35 / C36), AC coupled (7.8 Hz),
    #      biased at 1.47 V (the middle of ATTEN3's 0..2.9 V); powered from VBUS by its own LDO: off without USB
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
    ("U7", "ldo", "XC6206P332MR", "det", "C5446", {"3": "VBUS", "2": "AVDD", "1": "GND"}),   # (Torex SOT-23: 1 VSS, 2 VOUT, 3 VIN)
    ("C34", "C", "1u", "c0402", "C52923", {"1": "VBUS", "2": "GND"}),
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
    "C2873354": "TI TPS62842DGRR, 60 nA buck, 750 mA, HVSSOP-8",
    "C2909397": "Belling BL8506-40CRM, 4.0 V detector, push-pull, 0.5 uA",
    "C2763866": "Belling BL8506-30NRM, 3.0 V detector, open drain (U4, the reset supervisor)",
    "C20917": "AOS AO3400A, N-MOSFET 30 V 5.7 A, Vgs(th) 1.45 V, 48 mohm at 2.5 V, SOT-23",
    "C51018335": "JVB10V2200M10X10, 2200 uF 10 V aluminium, 10 x 10.5 mm",
    "C79317": "Murata DFE201612P-2R2M, 2.2 uH 1.5 A, 0806",
    "C8598": "CJ B5819W SL, Schottky 40 V 1 A, SOD-123",
    "C68978": "CJ BAV70, dual diode common cathode, SOT-23",
    "C318884": "XKB TS-1187A-B-A-B, tactile switch 5.1 x 5.1 x 1.5 mm",
    "C2286": "KT-0603R, red LED 0603",
    "C23186": "5.1 k 1% 0603", "C23182": "47 ohm 1% 0603", "C474307": "UNI-ROYAL 201007J047JT4E, 4.7 ohm 5% 2010, 0.75 W", "C17976": "68 ohm 1% 1206, 0.25 W", "C25804": "10 k 1% 0603", "C25092": "22 ohm 5% 0402", "C7250": "10 M 1% 0603",
    "C23196": "51 k 1% 0603", "C25804": "10 k 1% 0603", "C22935": "1 M 1% 0603", "C25803": "100 k 1% 0603",
    "C17634": "33 ohm 1% 0805", "C22859": "10 ohm 1% 0603", "C17168": "0 ohm 0402", "C25744": "10 k 1% 0402", "C26083": "1 M 1% 0402", "C25077": "10 ohm 1% 0402", "C23140": "33 ohm 1% 0603", "C25741": "100 k 1% 0402", "C25794": "51 k 1% 0402",
    "C14663": "100 nF 50 V X7R 0603", "C15849": "1 uF 50 V X5R 0603", "C15850": "10 uF 25 V X5R 0805",
    "C59461": "22 uF 6.3 V X5R 0603", "C4216": "33 k 1% 0603", "C19702": "10 uF 10 V X5R 0603",
    "C1525": "100 nF 16 V X7R 0402", "C12530": "2.2 uF 6.3 V X5R 0402", "C15195": "10 nF 50 V X7R 0402",
    "C52923": "1 uF 25 V X5R 0402", "C32949": "10 pF 50 V C0G 0402",
    # mini 2, LINE IN
    "C7972": "TI LMV321IDBVR, op-amp, rail-to-rail output, 1 MHz, 130 uA, 2.7..5.5 V, SOT-23-5 (a preferred Extended part)",
    "C5446": "Torex XC6206P332MR, 3.3 V LDO, 1 uA, 200 mA, SOT-23",
    "C49338": "Nexperia PESD5V0S2BT, two-line bidirectional ESD protection, 5 V, 35 pF, SOT-23",
    "C25765": "20 k 1% 0402", "C25908": "5.6 k 1% 0402", "C25755": "150 k 1% 0402", "C25750": "120 k 1% 0402",
    "C25079": "120 ohm 1% 0402", "C1531": "2.2 nF 50 V X7R 0402",
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
