# SPDX-License-Identifier: GPL-3.0-only
"""jank LCD-2 add-on (stage 1, 2026-11-17): the one parts table. A removable add-on board for a Waveshare
ESP32-S3-LCD-2 (sku 30306), which plugs in on its own two 1 x 14 male headers (J15 = its P1, J16 = its P2); jank-mini-3: it sits in our
notch, joined by two bridge strips (strip/), how it is held is open (mots). The LCD-2 brings the S3, the screen, the
charger, the cell connector and +3V3; this board keeps mini 2.2's reviewed circuits (hardware/mini2/parts.py,
signoff3) for: the 5 V aux rail (TPS61023) fed from VBAT (Q1) or the LCD-2's 5V pin (D3), two MIDI outs (jack + DIN
each, AO3400A sinks), CLOCK / RESET (74AHCT04), the digital LINE IN, four buttons and the USB host path (an outside
loop cable, no CC chip). make_schematic.py and check_nets.py read it.

PARTS: (ref, symbol, value, footprint, lcsc, {pad: net}). A pad left out is not connected.
Refs: mini 2.2's ref for every circuit taken over unchanged; new parts get new numbers.
"""

# the USB path (mots, 2026-11-17): the LCD-2's own USB-C sits at the case's side wall, reachable. Device mode / charging:
# a computer or a charger straight into it. Host mode: a standard short cable OUTSIDE the box from the LCD-2's USB-C
# into J1 (TO ESP, USB-C, beside it on the same wall); D+ / D- go straight to J4 (TO DEVICE, USB-C too: mots, 2026-11-18).
# J1's VBUS and CC are left open: nothing of ours can ever drive the LCD-2's VBUS (its 5V net is an input only: no
# boost, no OTG, R25 10k down). J4 is a host-side receptacle: Rp 56k ("Default USB Power") from HOST5 on CC1 / CC2,
# so a device on a C-to-C cable sees a source only while host mode is on; its VBUS carries only our host signal
# (HOST5 -> R93 -> D16), never power (mots).

FP = {   # JLC / EasyEDA footprint names (mini 2.2's lib/chip.pretty where it had them)
    "usbc": "USB-C-SMD_TYPE-C-16PIN-2MD-073", "usbcm": "USB-C-SMD_SHOU_TYPE-C-16P-CB1.6", "usbth": "USB-C-TH_GCT_USB4085-GF-A", "sot236bl1": "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL-1",
    "tvs2": "SOT-23_L2.9-W1.3-P1.90-LS2.4-BR", "det": "SOT-23-3_L2.9-W1.6-P1.90-LS2.8-BR",
    "sma_rd": "SMA_L4.3-W2.6-LS5.2-RD", "smaf_rd": "SMAF_L3.5-W2.6-LS4.7-RD-1", "sot563": "SOT-563_L1.6-W1.2-P0.50-LS1.6-BR", "ind2016": "IND-SMD_L2.0-W1.6-1",
    "tssop14": "TSSOP-14_L5.0-W4.4-P0.65-LS6.4-BL", "jack5": "AUDIO-TH_PJ307C", "jack": "AUDIO-TH_PJ-320A-4P-DIP",
    "din": "CONN-TH_DIN-504B-M10", "sw": "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5",
    "r0402": "R0402", "r0402np": "R0402_NP", "c0603np": "C0603_NP", "r0603": "R0603", "r0805": "R0805", "r1206": "R1206", "c0402": "C0402", "c0603": "C0603", "c0805": "C0805",
    "sj2c": "SJ2C_SMD", "twin": "R_TWIN_HAND", "twin_c": "C_TWIN_HAND", "pads6": "PADS6_SMD", "pads4": "PADS4_SMD", "f1206": "R1206",
    "fhdr14": "HDR-TH_14P-P2.54-V-F", "ph2smd": "CONN-SMD_PH2.0-1X2PW", "ph2th": "JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal", "r2512": "RES-SMD_L6.4-W3.2-1",
    "sot236bl": "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL", "sot236br": "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BR",   # (C2905420 KH-2.54FH-1X14P-H8.5, JLC's footprint)
}

# the LCD-2's two rows (Waveshare ESP32-S3-Touch-LCD-2 schematic, "PinOut"; P1 / P2 pin -> what the LCD-2 has there)
LCD2_P1 = {1: "IO2", 2: "IO4", 3: "IO6", 4: "IO16", 5: "IO17", 6: "IO18", 7: "IO21", 8: "IO8", 9: "IO7", 10: "IO10",
           11: "IO20", 12: "IO19", 13: "GND", 14: "5V"}
LCD2_P2 = {1: "3V3", 2: "GND", 3: "IO43", 4: "IO44", 5: "IO47", 6: "IO48", 7: "IO15", 8: "IO13", 9: "IO11", 10: "IO12",
           11: "IO14", 12: "IO9", 13: "GND", 14: "VBAT"}
# NO CAMERA may be fitted to the LCD-2's 24-pin socket (sku 30306 ships without one): its data lines sit on IO2, 4, 6..15
# (MIDI_TX, CLK, RST, V5_EN, LINE_IN, VBUS_SENSE among them) and its power-down on IO17 (our HOST_EN). Review B R4.
# what the LCD-2 itself hangs on those GPIOs (same schematic): pulls that matter at reset
LCD2_PULL = {16: "up 4.7k (R4, camera SCL)", 21: "up 4.7k (R5, camera SDA)", 17: "down 10k (R6, CAM_PWDN)",
             47: "up 4.7k (R30, IMU SCL)", 48: "up 4.7k (R29, IMU SDA)"}

# jank function -> (GPIO, connector.pin). check_nets.py proves that the connector pin carries that GPIO (LCD2_P1 / P2)
GPIO = {   # (jank-mini-3, mots 2026-11-18: assigned from the LAYOUT by tools/gpio_opt.py, the shortest legal map)
    "BTN_NO": (21, "J15.7"), "BTN_UP": (11, "J16.9"), "HOST_EN": (17, "J15.5"), "BTN_YES": (16, "J15.4"),
    "BTN_DOWN": (6, "J15.3"), "LINE_IN": (7, "J15.9"), "LINE_SENSE": (8, "J15.8"), "LIN_DET": (18, "J15.6"),
    "MIDI_TX": (13, "J16.8"), "MIDI_TX2": (9, "J16.12"), "CLK_IO": (15, "J16.7"), "RST_IO": (2, "J15.1"),
    "V5_EN_IO": (4, "J15.2"), "VBUS_SENSE": (10, "J15.10"),
    "SPARE_IO47": (47, "J16.5"), "SPARE_IO48": (48, "J16.6"),
}
# (2026-11-18, round 3, mots: the USB-C pair right of the TRS jacks, then the map re-assigned by tools/gpio_opt.py on
#  that layout: 431 -> 408 mm; then the 82 mm board with the TRS drivers by their jacks: 379 -> 362 mm; then with
#  crossings counted (mots): MIDI_TX2 to GPIO 11, 33 -> 30 crossings; then the three cells' layout (sockets in one
#  column, R134 / R135 at U18): 33 -> 31 crossings at 367 mm. Free: GPIO 12 / 14 (J16.10 / J16.11, no pad: the LCD-2's
#  own header) and 47 / 48 (J20).
#  No bottom-row pin is left free, so J17 (the bottom spare pads) is gone.)
# (2026-11-18, round 3 routing: BTN_YES from IO14 (J16.11, the far end of the top row) to IO16 (J15.4, was a spare): the
#  top row's signals squeeze through a ~2.6 mm corridor between J16 and the TRS / USB group and BTN_YES, 60 mm of it,
#  stayed open after a repair pass; IO16 has the LCD-2's 4.7k pull-up, as IO21. IO14 is now unused, no pad.)
OUTPUTS = ("MIDI_TX", "MIDI_TX2", "CLK_IO", "RST_IO", "HOST_EN", "V5_EN_IO")
ADC1 = ("LINE_IN", "LINE_SENSE", "VBUS_SENSE")
# inputs read through a WEAK external divider (LIN_DET: R38 1M up vs the jack ring's R37 100k down): never on a pin the
# LCD-2 pulls (its 4.7k would win: review mini3-A W1, 2026-11-18, LIN_DET on IO21 read "plugged" always)
WEAK = ("LIN_DET",)
STRAPPING = {0, 3, 45, 46}

J15 = {"1": "RST_IO", "2": "V5_EN_IO", "3": "BTN_DOWN", "4": "BTN_YES", "5": "HOST_EN", "6": "LIN_DET",
       "7": "BTN_NO", "8": "LINE_SENSE", "9": "LINE_IN", "10": "VBUS_SENSE", "13": "GND", "14": "VBUS_B"}
J16 = {"1": "+3V3", "2": "GND", "5": "SPARE_IO47", "6": "SPARE_IO48", "7": "CLK_IO", "8": "MIDI_TX", "9": "BTN_UP",
       "12": "MIDI_TX2", "13": "GND", "14": "VBAT"}

DNP = set()

PARTS = [
    # ---- the LCD-2's sockets (through hole: mots, or JLC's hand soldering; JOIN.md option a)
    # (mots, 2026-11-18: the bridge strips are soldered to OUR board: J15 / J16 are male headers with ~11 mm of pin below
    #  the plastic, C44526 (2.54 mm 1 x 40, 11.5 / 2.5 / 3 mm), cut to 1 x 14; the 3 mm tail in our board, from below)
    # (mots, 2026-11-19, after review 8: a SECOND socket on each strip takes our row too, so standard male headers
    #  (Kinghelm KH-2.54PH180-1X14P-L11.5, C2905490: 6 mm mating pin) instead of the 11.5 mm long pins; the strip plugs
    #  onto both boards and the case holds it from below. Same maker as the strip socket C2905420.)
    ("J15", "hdr14", "LCD-2 P1 1x14 M 2.54 (C2905490)", "fhdr14", "C2905490", J15),
    ("J16", "hdr14", "LCD-2 P2 1x14 M 2.54 (C2905490)", "fhdr14", "C2905490", J16),
    ("C60", "C", "10u", "c0603", "C19702", {"1": "+3V3", "2": "GND"}),
    ("C61", "C", "100n", "c0402", "C1525", {"1": "+3V3", "2": "GND"}),
    # spare GPIOs to pads (no part): the LCD-2's I2C, IO47 / 48 (the IMU's 4.7k pull-ups on it), on J20
    # (jank-mini-3, mots 2026-11-18: the spare pads split, one set by each LCD-2 row: no long run across the board)
    ("J20", "pads4b", "SPARE pads P2", "pads4", "", {"1": "SPARE_IO47", "2": "SPARE_IO48", "3": "+3V3", "4": "GND"}),

    # ---- USB: J1 TO ESP (USB-C, the loop cable from the LCD-2's own USB-C; VBUS / CC open) and J4 TO DEVICE (USB-C,
    #      the hosted gear), D+ / D- straight through, U5 ESD on the shared pair (REF on +3V3: mini 2.2 review pass 2)
    #      (mots, 2026-11-18, review mini3-2B R3: MID-MOUNT, Shou Han TYPE-C 16P CB1.6 073, C2906290: it sinks into a
    #      9.3 x 6.5 mm notch in our edge, 1.56 mm above our top instead of 3.26, the plug's centre at our top's level)
    #      (mots, 2026-11-19, after the JLC quotes: THROUGH-HOLE instead, GCT USB4085-GF-A, C7095263: the mid-mount part
    #       is "Standard only" at JLC, $69 more for 5 boards, and 0.5 mm SMD pads need hot air; this one is soldered by hand
    #       with an iron from below (0.85 mm pitch, two rows), on top of the board at the edge, no notch)
    ("J1", "usbth", "USB-C TO ESP (loop)", "usbth", "C7095263",
     {"A1": "GND", "B12": "GND", "B1": "GND", "A12": "GND", "S1": "GND",
      "A4": "J1_VBUS1", "B9": "J1_VBUS1", "A9": "J1_VBUS2", "B4": "J1_VBUS2",   # (open; a net per touching pair, nothing to route)
      "A6": "UDP", "B6": "UDP", "A7": "UDN", "B7": "UDN"}),
    ("J4", "usbth", "USB-C TO DEVICE (host)", "usbth", "C7095263",
     {"A1": "GND", "B12": "GND", "B1": "GND", "A12": "GND", "S1": "GND",
      # (only the VBUS pair on CC1's side, A4 / B9 (usb_fan rel +2.4, away from J1 / U5), on VUSB: a plug ties its four
      #  VBUS pins together, so one receptacle pair meets VBUS in both orientations, and here VBUS is a mA presence
      #  signal. The other pair's drawn drop sat in the D+ path from J1 / U5, round 3; it is an open net, as J1's)
      "A4": "VUSB", "B9": "VUSB", "A9": "J4_VBUS2", "B4": "J4_VBUS2", "A5": "CC1O", "B5": "CC2O",
      "A6": "UDP", "B6": "UDP", "A7": "UDN", "B7": "UDN"}),
    # (2026-11-19, the through-hole USB-C: R160 on CC2O and R161 on CC1O, so the CC paths from U5 do not cross; the same
    #  56k Rp each, only which resistor serves which CC)
    ("R160", "R", "56k", "r0402", "C25796", {"1": "HOST5", "2": "CC2O"}),   # (Rp, USB Type-C 2.x Table 4-24: 56k to 5 V)
    ("R161", "R", "56k", "r0402", "C25796", {"1": "HOST5", "2": "CC1O"}),
    # U5: one channel per exposed line, D+, D- (both connectors share them) and J4's CC1 / CC2 (REF +3V3: an open CC at
    # HOST5's 5 V leaks ~20 uA into +3V3 through 56k and the steering diode: harmless)
    ("U5", "esd4", "SRV05-4", "sot236bl1", "C7420376", {"1": "UDP", "3": "UDN", "4": "CC1O", "6": "CC2O", "2": "GND", "5": "+3V3"}),
    # C28 at J4's VBUS (sim/host.py: +8 kV contact ESD on VBUS -> ~2 V with it); the host signal's 2.2 ms rise
    ("C28", "C", "1u 50V", "c0603", "C15849", {"1": "VUSB", "2": "GND"}),
    # the host signal (mini 2.2's): U21's spare pair drives HOST5 = V5C when HOST_EN is high; R93 limits it (a short
    # on J4's VBUS: ~2 mA), D16 blocks any way back. Signal only, never power (mots: the small cell is not drained): bus-powered gear needs its own power or a powered hub
    ("JP13", "sj2", "HOST5 link", "sj2c", "", {"1": "HOST5G", "2": "HOST5"}),
    # (review A W1: 2.2k left a self-powered device's ~10k VBUS-detect at 3.7 V, under the 4.0..4.4 V "VBUS valid" of
    #  many chips; 680 ohm: ~4.2 V into 10k, a short takes 6.6 mA from U21 (8 mA recommended), never power)
    ("R93", "R", "680", "r0402", "C25130", {"1": "HOST5", "2": "HOST_R"}),
    ("D16", "bav70", "BAT54C", "det", "C37704", {"1": "HOST_R", "3": "VUSB"}),
    ("R91", "R", "100k", "r0402", "C25741", {"1": "HOST_EN", "2": "GND"}),
    # VBUS sense on the board side (the LCD-2's 5V pin): (was 100k / 100k = 0.5, see below; now 10k / 15k = 0.6) ADC1_CH9 (IO10, since the layout pin map of 2026-11-18): 2.75 V at 5.5 V, inside the
    # 12 dB range (review B: 100k / 150k saturated the ADC above ~4.8 V, presence only)
    # (review mini3-4A R8: 100k / 100k put 5 V at 2.4-2.5 V, on the S3's input-high threshold; 10k / 15k: 3.0 V at 5 V,
    #  3.3 V at 5.5 V, readable as logic or ADC; both values basic and already in the BOM; 0.2 mA, only on USB power)
    ("R88", "R", "10k", "r0402", "C25744", {"1": "VBUS_B", "2": "VBUS_SENSE"}),
    ("R89", "R", "15k", "r0402", "C25756", {"1": "VBUS_SENSE", "2": "GND"}),
    ("C29", "C", "10n", "c0402", "C15195", {"1": "VBUS_SENSE", "2": "GND"}),   # (review C: the hot-plug spike on the VBUS_SENSE pin, IO10 now)
    # hot-plug damping on VBUS_B (a computer / charger into the LCD-2's own USB-C; mini 2.2's C27 / R151; the LCD-2's own
    # C26 1u sits on the same net). sim/host.py, a stiff 5.25 / 5.5 V source through 0.7 / 1 uH: with C27 22u and R157
    # 1 ohm before D3, VBUS_B <= 6.98 V (the LCD-2 alone: 8.75 V) and VIN_B <= 5.33 V (TPS61023: 5.5 V recommended, 7 V
    # abs max); with C27 10u and no R157, VIN_B reached 7.73 V. The capacitance on VBUS (~20 uF effective with the LCD-2's)
    # is over USB's 10 uF attach figure, accepted as on mini 2.2 (REVIEW.md C4)
    # (review C R1: the LCD-2's 5V net is also the gate of its own AO3401 power-path FET; C27 straight on it held that
    #  gate up after a USB unplug (VCC on the FET's body diode for ~21 ms, vs ~2 ms alone: a brownout risk at a low cell).
    #  Now C27 charges through D17 and cannot feed VBUS_B back; R158 10k empties it. Review C's sim: the hot-plug peaks
    #  unchanged, the unplug dip back to ~1.8 ms)
    ("C27", "C", "22u 25V", "c0805", "C45783", {"1": "VB_C", "2": "GND"}),
    ("D17", "D", "SS34F", "smaf_rd", "C18199199", {"1": "VB_C", "2": "VB_D1"}),   # (SMAF, 1.06 mm: the SMA's 2.1-2.4 mm met the lid, review mini3-3B R4)
    ("R158", "R", "10k", "r0402", "C25744", {"1": "VB_C", "2": "GND"}),
    # (review A R1: R151 takes ~4.7 A / 0.18 mJ on each hot plug: an 0805 as R157, not the 1/16 W 0402)
    ("R151", "R", "1", "r0805", "C25271", {"1": "VBUS_B", "2": "VB_D1"}),

    # ---- the 5 V aux rail. VIN_B from VBUS_B through D3 (USB present) or from VBAT through Q1 (mini 2.2's AN1149
    #      load switch: drain VBAT, source VIN_B, gate VBUS_B, R50 down): on USB Q1 is off and its body diode reversed,
    #      so nothing of ours loads the cell while the LCD-2's ETA6098 charges it
    ("R157", "R", "1", "r0805", "C25271", {"1": "VBUS_B", "2": "VB_D3"}),   # (sim/host.py: the hot plug's damping into VIN_B)
    ("D3", "D", "SS34F", "smaf_rd", "C18199199", {"1": "VIN_B", "2": "VB_D3"}),   # (SMAF, 1.06 mm, as D17)
    # (review mini3-4A R7: on battery D3's reverse leakage from VIN_B lifts VB_D3 / VBUS_B (Q1's gate, the LCD-2's own
    #  power FET's gate); 4.7k holds it down: at 5 V on USB it draws ~1 mA)
    ("R167", "R", "4.7k", "r0402", "C25900", {"1": "VB_D3", "2": "GND"}),
    ("Q1", "pfet", "AO3401A", "tvs2", "C15127", {"1": "VBUS_B", "2": "VIN_B", "3": "VBAT"}),
    ("R50", "R", "100k", "r0402", "C25741", {"1": "VBUS_B", "2": "GND"}),
    ("U18", "boost6", "TPS61023DRLR", "sot563", "C919459",
     {"1": "V5_FB", "2": "V5_EN", "3": "VIN_B", "4": "GND", "5": "V5_SW", "6": "V5A"}),
    # (reviews A C3 / C R4, mini 2.2 REVIEW3 R5: 0.47 uH -20 % sat at the TPS61023's 0.37 uH minimum; 1.0 uH is TI's own
    #  nominal (SLVSF14B 6.3), the same Murata family and 2016 land)
    ("L5", "L", "1.0u", "ind2016", "C337892", {"1": "VIN_B", "2": "V5_SW"}),
    ("C99", "C", "10u", "c0603", "C19702", {"1": "VIN_B", "2": "GND"}),
    ("C104", "C", "10u", "c0603", "C19702", {"1": "VIN_B", "2": "GND"}),   # (review D M2: a 10 V 0603, not the 6.3 V 0402)
    ("C100", "C", "10u 25V", "c0805", "C15850", {"1": "V5A", "2": "GND"}),
    ("C101", "C", "10u 25V", "c0805", "C15850", {"1": "V5A", "2": "GND"}),
    ("R134", "R", "1M", "r0402", "C26083", {"1": "V5A", "2": "V5_FB"}),
    ("R135", "R", "100k", "r0402", "C25741", {"1": "V5_FB", "2": "V5_FBM"}),
    ("R154", "R", "20k", "r0402", "C25765", {"1": "V5_FBM", "2": "V5_FBL"}),
    # (review D M1: 5.0 V, not 4.79: MIDI's low corner 3.15 -> ~3.5 mA, the host signal into 10k ~4.4 V; still under the
    #  TPS61023's 5.5 V OVP (5.0 V x 1.025 VREF x 1 % resistors + PFM ~5.3 V max) and the clamp's 5.71 V)
    ("R136", "R", "15k", "r0402", "C25756", {"1": "V5_FBL", "2": "GND"}),
    ("R131", "R", "100k", "r0402", "C25741", {"1": "+3V3", "2": "V5_EN"}),
    ("JP10", "sj2", "V5 EN link", "sj2c", "", {"1": "V5_EN", "2": "V5_EN_IO"}),
    # the clamp: CJ431 behind R145 10 ohm, now 24k / 18k = 5.83 V nominal, >= 5.70 V (no WS2812 any more: the next limit
    # is 7 V). (Review A R5: 22k / 18k, >= 5.44 V, sat only 70 mV over a hot 5.5 V charger's pass-through)
    ("U19", "shunt", "CJ431", "det", "C3113", {"1": "V5_CL", "2": "V5_CK", "3": "GND"}),
    ("R145", "R", "10", "r0402", "C25077", {"1": "V5A", "2": "V5_CK"}),
    ("R138", "R", "24k", "r0402", "C25769", {"1": "V5_CK", "2": "V5_CL"}),
    ("R139", "R", "18k", "r0402", "C25762", {"1": "V5_CL", "2": "GND"}),
    ("JP9", "sj2", "CV 5V link", "sj2c", "", {"1": "V5A", "2": "V5C"}),
    ("JP8", "sj2", "MIDI 5V link", "sj2c", "", {"1": "V5A", "2": "V5M"}),
    ("R123", "R", "0 DNP", "r0402np", "", {"1": "+3V3", "2": "V5M"}),

    # ---- MIDI OUT 1 (MIDI_TX, IO13) and MIDI OUT 2 (MIDI_TX2, IO9): mini 2.2 signoff3, AO3400A sinks, ESD at the pins.
    #      The firmware INVERTS both UART TX lines (uart_set_line_inverse(UART_SIGNAL_TXD_INV)): idle low = FET off = no
    #      loop current; a start bit (high on the pin) = FET on = current. R124 / R125 hold the gates low while the pins float.
    #      DIN pads: JLC's DIN-504B-M10 footprint, pad 4 = DIN 4 (source), pad 2 = DIN 5 (sink), as mini 2.2: SETTLED
    #      (mots 2026-11-18: HOOYA's drawing, every contact drops straight to its leg; worktrees/pinout-scratch J5 / J6
    #      study: CORRECT). The PJ-320A: tip = pad D, sleeve = pad A (KiCad's PJ320D footprints, 2026-11-18): meter one before
    #      ordering (bench test 1)
    ("J2", "jack5", "MIDI OUT", "jack5", "C16684", {"5": "MO_T", "2": "MO_R", "1": "GND"}),
    ("J5", "din", "MIDI OUT DIN", "din", "C2939346", {"4": "MD_R", "2": "MD_T", "3": "GND", "6": "GND", "7": "GND"}),
    ("R14", "R", "220", "r1206", "C17957", {"1": "V5M", "2": "MO_R"}),
    ("R15", "R", "220", "r1206", "C17957", {"1": "MO_K", "2": "MO_T"}),
    ("Q10", "nfet", "AO3400A", "tvs2", "C20917", {"1": "MIDI_TX", "2": "GND", "3": "MO_K"}),
    ("R54", "R", "220", "r1206", "C17957", {"1": "V5M", "2": "MD_R"}),
    ("R55", "R", "220", "r1206", "C17957", {"1": "MD_K", "2": "MD_T"}),
    ("Q11", "nfet", "AO3400A", "tvs2", "C20917", {"1": "MIDI_TX", "2": "GND", "3": "MD_K"}),
    ("R124", "R", "100k", "r0402", "C25741", {"1": "MIDI_TX", "2": "GND"}),
    ("R126", "R", "0 DNP", "r0402np", "", {"1": "MO_K", "2": "MIDI_TX"}),
    ("R127", "R", "0 DNP", "r0402np", "", {"1": "MD_K", "2": "MIDI_TX"}),
    # (layout, arrangement A: the jacks sit on the top wall, the DINs on the bottom wall: U11 protects the two jacks, U12
    #  the two DINs, each right at its two connectors' pins (was: one SRV05-4 per output, jack + DIN, 40 mm apart))
    # (review D R1 / C R2: a eurorack output patched into a MIDI jack went through the array straight into V5M / V5A.
    #  The arrays now sit on the FET drains, behind the 220 ohm sinks: a +12 V mis-patch is limited by R15 to ~25 mA into
    #  the clamp; the ring reaches V5M only through its own 220 ohm (an ESD hit on it lands in V5M's ~20 uF). U11 at the
    #  jacks' FETs, U12 at the DINs')
    ("U11", "esd4", "SRV05-4", "sot236bl1", "C7420376", {"1": "MO_K", "3": "MO2_K", "2": "GND", "5": "V5M"}),
    ("J3", "jack5", "MIDI OUT 2", "jack5", "C16684", {"5": "MO2_T", "2": "MO2_R", "1": "GND"}),
    ("J6", "din", "MIDI OUT 2 DIN", "din", "C2939346", {"4": "MD2_R", "2": "MD2_T", "3": "GND", "6": "GND", "7": "GND"}),
    ("R21", "R", "220", "r1206", "C17957", {"1": "V5M", "2": "MO2_R"}),
    ("R20", "R", "220", "r1206", "C17957", {"1": "MO2_K", "2": "MO2_T"}),
    ("Q12", "nfet", "AO3400A", "tvs2", "C20917", {"1": "MIDI_TX2", "2": "GND", "3": "MO2_K"}),
    ("R56", "R", "220", "r1206", "C17957", {"1": "V5M", "2": "MD2_R"}),
    ("R57", "R", "220", "r1206", "C17957", {"1": "MD2_K", "2": "MD2_T"}),
    ("Q13", "nfet", "AO3400A", "tvs2", "C20917", {"1": "MIDI_TX2", "2": "GND", "3": "MD2_K"}),
    ("R125", "R", "100k", "r0402", "C25741", {"1": "MIDI_TX2", "2": "GND"}),
    ("R128", "R", "0 DNP", "r0402np", "", {"1": "MO2_K", "2": "MIDI_TX2"}),
    ("R129", "R", "0 DNP", "r0402np", "", {"1": "MD2_K", "2": "MIDI_TX2"}),
    ("U12", "esd4", "SRV05-4", "sot236bl1", "C7420376", {"1": "MD_K", "3": "MD2_K", "2": "GND", "5": "V5M"}),

    # ---- LINE IN, digital (mini 2.2 unchanged): two matched networks into IO7 (LINE_IN) / IO8 (LINE_SENSE)
    ("J7", "jack5", "LINE IN", "jack5", "C16684", {"5": "LIN_T", "2": "LIN_R", "1": "LIN_S", "3": "LIN_DET"}),
    ("R39", "R", "100", "r0402", "C25076", {"1": "LIN_S", "2": "GND"}),
    ("R36", "R", "100k", "r0402", "C25741", {"1": "LIN_T", "2": "GND"}),
    ("R37", "R", "100k", "r0402", "C25741", {"1": "LIN_R", "2": "GND"}),
    ("R38", "R", "1M", "r0402", "C26083", {"1": "+3V3", "2": "LIN_DET"}),
    ("D7", "tvs2l", "PESD15VL2BT", "tvs2", "C38838", {"1": "LIN_T", "2": "LIN_R", "3": "GND"}),
    ("D14", "tvs2l", "PESD15VL2BT", "tvs2", "C38838", {"1": "LIN_S", "3": "GND"}),
    ("R30", "R", "20k", "r0402", "C25765", {"1": "LIN_T", "2": "LIN_A1"}),
    ("C30", "C", "1u", "c0402", "C52923", {"1": "LIN_A1", "2": "LINE_IN"}),
    ("R31", "R", "20k", "r0402", "C25765", {"1": "LIN_R", "2": "LIN_A2"}),
    ("C31", "C", "1u", "c0402", "C52923", {"1": "LIN_A2", "2": "LINE_IN"}),
    ("R32", "R", "12k", "r0402", "C25752", {"1": "LINE_IN", "2": "LIN_VB"}),
    ("C32", "C", "2.2n C0G", "c0603", "C2991172", {"1": "LINE_IN", "2": "GND"}),
    ("R65", "R", "20k", "r0402", "C25765", {"1": "LIN_S", "2": "LIN_B1"}),
    ("C65", "C", "1u", "c0402", "C52923", {"1": "LIN_B1", "2": "LINE_SENSE"}),
    ("R66", "R", "20k", "r0402", "C25765", {"1": "LIN_S", "2": "LIN_B2"}),
    ("C66", "C", "1u", "c0402", "C52923", {"1": "LIN_B2", "2": "LINE_SENSE"}),
    ("R67", "R", "12k", "r0402", "C25752", {"1": "LINE_SENSE", "2": "LIN_VB"}),
    ("C67", "C", "2.2n C0G", "c0603", "C2991172", {"1": "LINE_SENSE", "2": "GND"}),
    ("R33", "R", "150k", "r0402", "C25755", {"1": "+3V3", "2": "LIN_VB"}),
    ("R34", "R", "100k", "r0402", "C25741", {"1": "LIN_VB", "2": "LIN_VBL"}),
    ("R153", "R", "20k", "r0402", "C25765", {"1": "LIN_VBL", "2": "GND"}),
    ("C33", "C", "10u", "c0603", "C19702", {"1": "LIN_VB", "2": "GND"}),

    # ---- the buttons: to GND; BTN_YES (IO16) / BTN_NO (IO21) have the LCD-2's 4.7k pull-ups (R4 / R5), BTN_UP (IO11) /
    #      BTN_DOWN (IO6) the S3's own (the firmware enables them)
    ("SW1", "sw", "NO", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BTN_NO", "4": "BTN_NO"}),
    ("SW3", "sw", "UP", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BTN_UP", "4": "BTN_UP"}),
    ("SW4", "sw", "DOWN", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BTN_DOWN", "4": "BTN_DOWN"}),
    ("SW5", "sw", "YES", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BTN_YES", "4": "BTN_YES"}),
    ("C80", "C", "DNP", "c0603np", "", {"1": "BTN_UP", "2": "GND"}),
    ("C81", "C", "DNP", "c0603np", "", {"1": "BTN_DOWN", "2": "GND"}),
    ("C82", "C", "DNP", "c0603np", "", {"1": "BTN_NO", "2": "GND"}),
    ("C83", "C", "DNP", "c0603np", "", {"1": "BTN_YES", "2": "GND"}),
]

# CLOCK / RESET (mini 2.2's buffer try): GPIO -> 2 inverters of U21 (on V5C) -> BAV99 -> 2 x 1k 1206 in series -> jack;
# 100k pull-down on the GPIO, 2.2 nF C0G on the jack side, sleeve to GND through 100 ohm. U21's spare pair drives HOST5.
# (Review B R1, 2026-11-17: with one 1k, -12 V patched into a HIGH output put 16 mA / 262 mW in the 0.25 W 1206 and
#  16 mA on the 74AHCT04 (8 mA recommended). Two Basic 1k in series (no new part type): 8 mA, 64 mW each; ~4.0 V into
#  10k. sim/midi_cv.py)
def _cv(n, io, d, r):
    return [
        (f"R{r + 1}", "R", "100k", "r0402", "C25741", {"1": io, "2": "GND"}),
        (f"C{89 if n == 'CLK' else 90}", "C", "2.2n C0G", "c0603", "C2991172", {"1": f"{n}_OUT", "2": "GND"}),
        (d, "bav99", "BAV99", "tvs2", "C2500", {"1": "GND", "3": f"{n}_N", "2": "V5C"}),
        (f"R{r + 5}", "R", "1k", "r1206", "C4410", {"1": f"{n}_N", "2": f"{n}_M"}),
        (f"R{r + 8}", "R", "1k", "r1206", "C4410", {"1": f"{n}_M", "2": f"{n}_OUT"}),
    ]


PARTS += _cv("CLK", "CLK_IO", "D10", 70) + _cv("RST", "RST_IO", "D11", 76) + [
    ("U21", "inv6", "74AHCT04", "tssop14", "C8190",
     {"1": "CLK_IO", "2": "CLK_B", "3": "CLK_B", "4": "CLK_N", "5": "RST_IO", "6": "RST_B", "7": "GND", "8": "RST_N",
      "9": "RST_B", "10": "HOST_M", "11": "HOST_EN", "12": "HOST5G", "13": "HOST_M", "14": "V5C"}),
    ("C105", "C", "100n", "c0402", "C1525", {"1": "V5C", "2": "GND"}),
    # (PJ-320A, checked 2026-11-18: KiCad's own footprints for this body (Jack_3.5mm_PJ320D, the Hroparts PJ-320D-4A: the
#  same 7.0 mm holes and pad pattern) name our D = T (tip), A = S (sleeve), C = R1, B = R2. A reading of Shouhan's
#  drawing ("deepest leg = tip") had swapped them for one round and review mini3-2-A caught it: the sleeve contact's
#  leg exits at the back corner. Meter one jack before ordering.)
    ("J13", "jack", "CLOCK", "jack", "C18185602", {"D": "CLK_OUT", "A": "CLK_S"}),
    ("J14", "jack", "RESET", "jack", "C18185602", {"D": "RST_OUT", "A": "RST_S"}),
    ("R120", "R", "100", "r0402", "C25076", {"1": "CLK_S", "2": "GND"}),
    ("R121", "R", "100", "r0402", "C25076", {"1": "RST_S", "2": "GND"}),
]

# ---- the cell (mots, 2026-11-19, first ruling, superseded below by J18): not on this board. One ~1500 mAh 1S LiPo
#      (with its own PCM) was to plug into the LCD-2's own battery connector; the LCD-2 charges it (ETA6098, 2 A: ~1.3 C) and brings
#      its voltage up on P2.14 (VBAT), which our 5 V boost uses on battery (Q1). Gone with it: the 3 PH2.0 sockets, the
#      fuses, the 0.33 ohm sharing resistors and the DW01A + FS8205A protector (sim/cells.py studied them).
# (mots, 2026-11-19, later: ONE PH2.0 socket here after all: 2000 / 1100 mAh EEMB cells come with a JST PHR-02 plug, the
#  LCD-2 has MX1.25. J18 sits on VBAT / GND, i.e. on the LCD-2's battery node via P2.14, so the LCD-2 still charges the cell
#  (up to 2 A through J18, P2.14 and the strip: VBAT 0.75 mm); the cell's own PCM protects it. Never a cell in both.)
# (mots, 2026-11-19, after review 6A R1: J18 left again: the 2 A charge would cross the LCD-2's own, unmeasured trace
#  to P2.14. The cell goes on the LCD-2's own MX1.25 through a PH2.0 -> MX1.25 adapter lead; it was ref J18, "ph2m", "ph2th", C173752, on VBAT / GND.)

# the copper-only parts (solder jumpers, hatch lands, pads): DNP (not in the JLC BOM / CPL)
# (jank-mini-3, mots 2026-11-18: the through-hole parts sit UNDER the board and mots solders them: the DINs, the jacks,
#  the male rows J15 / J16 and the cell sockets go to fab/lcd2-hand.csv, not to JLC; JLC assembles the top only.
#  Earlier, stage 2 of the brief: JLC hand-soldered them on top.)
DNP |= {"J20", "JP8", "JP9", "JP10", "JP13", "R123", "R126", "R127", "R128", "R129", "C80", "C81", "C82", "C83"}

# the twins (mini 2.2's rule): bare hand-solder pads in parallel with resistors whose value may change on the bench
TWINS = {
    "RT15": ("R15", "MIDI OUT jack sink"), "RT55": ("R55", "MIDI OUT DIN sink"),
    "RT20": ("R20", "MIDI OUT 2 jack sink"), "RT57": ("R57", "MIDI OUT 2 DIN sink"),
    "RT33": ("R33", "LINE IN bias, upper"), "RT34": ("R34", "LINE IN bias, lower"),
    "RT36": ("R36", "LINE IN tip pull-down"), "RT37": ("R37", "LINE IN ring pull-down"), "RT38": ("R38", "plug detect pull-up"),
    "RT39": ("R39", "LINE IN sleeve to GND: a 0 ohm here grounds it solid"),
    "RT120": ("R120", "CLOCK sleeve to GND"), "RT121": ("R121", "RESET sleeve to GND"),
    "RT93": ("R93", "the host signal's limit"), "RT139": ("R139", "the clamp's threshold"),
}
_main = {p[0]: p for p in PARTS}
PARTS += [(t, _main[m][1], "twin DNP", "twin", "", dict(_main[m][5])) for t, (m, _why) in TWINS.items()]
DNP |= set(TWINS)

# what each LCSC code is (the part list; prices in worktrees/lcd2-addon-scratch/cost/snap.json)
LCSC = {
    "C25900": "4.7 k 1% 0402",
    "C18199199": "hongjiacheng SS34F, 3 A 40 V Schottky, SMAF (1.06 mm tall: under the 2.1 mm lid gap)",
    "C2905420": "Kinghelm KH-2.54FH-1X14P-H8.5, 1 x 14 female socket, 2.54 mm, 8.5 mm tall, through hole (JLC hand-solders it)",
    "C2905490": "Kinghelm KH-2.54PH180-1X14P-L11.5, 1 x 14 male header, 2.54 mm, 11.5 mm long (6 mm mating), through hole",
    "C2906290": "SHOU HAN TYPE-C 16P CB1.6 073, USB 2.0 mid-mount receptacle (1.6 mm board, 1.56 mm above it)",
    "C7095263": "GCT USB4085-GF-A, USB 2.0 Type-C receptacle, through hole, right angle, top mount (hand-soldered)",
    "C25796": "56 k 1% 0402 (Rp)",
    "C7420376": "Hongjiacheng SRV05-4, 4-line + rail ESD array, SOT-23-6 (5 the rail, 2 GND, I/O 1 / 3 / 4 / 6)",
    "C15127": "AOS AO3401A, P-MOSFET 30 V 4 A, SOT-23 (1 G, 2 S, 3 D)", "C20917": "AOS AO3400A, N-MOSFET 30 V 5.7 A, SOT-23",
    "C37704": "Nexperia BAT54C, dual Schottky, common cathode (3), SOT-23", "C8678": "SS34, Schottky 40 V 3 A, SMA (pad 1 = cathode)",
    "C919459": "TI TPS61023DRLR, boost, SOT-563 (1 FB, 2 EN, 3 VIN, 4 GND, 5 SW, 6 VOUT)",
    "C3113": "CJ CJ431, 2.5 V shunt, SOT-23 (1 REF, 2 K, 3 A)",
    "C8190": "TI SN74AHCT04PWR, hex inverter, TSSOP-14", "C2500": "Nexperia BAV99, SOT-23 (1 A1, 2 K2, 3 K1/A2)",
    "C38838": "Nexperia PESD15VL2BT, 2-line ESD 15 V, SOT-23 (1 / 2 lines, 3 common)",
    "C16684": "BOOMELE PJ-307C TRS jack (through hole: JLC hand-solders it)", "C18185602": "SHOU HAN PJ-320A-4P DIP jack (JLC)",
    "C2939346": "HOOYA DIN-504B-M10, 5-pin DIN socket (JLC hand-solders it)", "C318884": "XKB TS-1187A-B-A-B, tactile 5.1 x 5.1",
    "C17957": "220 ohm 1% 1206", "C4410": "1 k 1% 1206", "C2991172": "2.2 nF 50 V C0G 5% 0603",
    "C15850": "10 uF 25 V X5R 0805", "C45783": "22 uF 25 V X5R 0805", "C25271": "1 ohm 1% 0805", "C15849": "1 uF 50 V X5R 0603", "C19702": "10 uF 10 V X5R 0603",
    "C25744": "10 k 1% 0402", "C15195": "10 nF 50 V X7R 0402", "C25756": "15 k 1% 0402", "C337892": "Murata DFE201612E-1R0M, 1.0 uH, 2016",
    "C52923": "1 uF 25 V X5R 0402", "C1525": "100 nF 16 V X7R 0402",
    "C25741": "100 k 1% 0402", "C25755": "150 k 1% 0402", "C25765": "20 k 1% 0402",
    "C26083": "1 M 1% 0402", "C25752": "12 k 1% 0402",
    "C722761": "PH2.0-2P-LT, PH2.0 2-pin socket, SMD vertical (the cells' leads)",
    "C393076": "RLP25FEER330, 0.33 ohm 1% 2 W 2512 (one per cell)",
    "C351410": "PUOLOP DW01A, 1-cell Li-ion protector, SOT-23-6 (1 OD, 2 VM, 3 OC, 4 TD, 5 VCC, 6 GND; no charge over-current level)",
    "C2830320": "TECH PUBLIC FS8205A, dual N-MOSFET 20 V 6 A, SOT-23-6 (1 S1, 2 D, 3 S2, 4 G2, 5 D, 6 G1)",
    "C11702": "1 k 1% 0402",
    "C25762": "18 k 1% 0402", "C25769": "24 k 1% 0402", "C25130": "680 ohm 1% 0402", "C25077": "10 ohm 1% 0402", "C25076": "100 ohm 1% 0402", 
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

# top-side parts JLC's Economic assembly cannot place and mots solders by hand (they go to fab/default/hand.csv; the
# full variant has JLC place them too). (mots, 2026-11-19: the through-hole USB-C.)
HAND = {"J1", "J4"}
