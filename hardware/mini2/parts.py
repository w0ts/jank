# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2 (stage 1e, 2026-11-14: the u.FL only; 4 buttons; LEDs or a screen; standard parts; earlier: stage 1c) : the one parts table. No MIDI power input (no reservoir,
no buck, no detectors): USB or a 1S Li-ion cell (two PH2.0 sockets in parallel, a TP4054 charger, an XB5353A protector,
an ideal-diode power path) into VSYS, one TLV75733P to 3.3 V for both orders (review pass 2); MIDI OUT 2 a passive copy of MIDI OUT 1 (J3 / J6
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
    "qfn56": "QFN-56_L7.0-W7.0-P0.40-TL-EP5.7", "wroom1u": "WIRELM-SMD_ESP32-S3-WROOM-1U", "xtal": "CRYSTAL-SMD_4P-L3.2-W2.5-BL", "ant": "ANT-SMD_L3.2-W1.6",
    "ufl": "RF-SMD_FRF05002-JSS103M",
    "usbc": "USB-C-SMD_TYPE-C-16PIN-2MD-073", "sot236": "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL", "jack": "AUDIO-TH_PJ-320A-4P-DIP",
    "det": "SOT-23-3_L2.9-W1.6-P1.90-LS2.8-BR",
    "led": "LED-SMD_L1.6-W0.8-R-RD", "sw": "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5",
    "r0603": "R0603", "r0805": "R0805", "r1206": "R1206", "c0603": "C0603",
    "c0805": "C0805", "c1206": "C1206", "r0402": "R0402", "c0402": "C0402", "l0402": "L0402", "trs": "TRS_PADS_SMD",
    "din": "CONN-TH_DIN-504B-M10",
    "sot235": "SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BR", "tvs2": "SOT-23_L2.9-W1.3-P1.90-LS2.4-BR",   # (mini 2: LINE IN)
    "sot236bl1": "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL-1",   # (2026-11-15: the Hongjiacheng SRV05-4)
    "sot236br": "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BR", "sma": "SMA_L4.3-W2.6-LS5.0-BI",   # (stage 1e: SRV05-4, SMAJ5.0CA)
    "hdr4": "HDR-TH_4P-P2.54-V-M", "sj3": "SJ3_SMD", "sj2c": "SJ2C_SMD", "sj3c": "SJ3C_SMD",   # (stage 1e: the screen's header, DNP; the VCC / GND jumpers)
    "led0805": "LED0805-R-RD", "sot25bl": "SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BL", "ph2": "CONN-TH_B2B-PH-K-S", "ph2s": "CONN-TH_S2B-PH-K-S-LF-SN",
    "sot223": "SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR",   # (stage 1d: the AMS1117, the "usb" order's LDO)
    "jack5": "AUDIO-TH_PJ307C",   # (stage 2: the switched TRS jack, all three jacks)
    "pads2": "PADS2_HAND",        # (stage 2: the power switch's two wire pads, make_footprints.py)
    "pads6": "PADS6_SMD", "pads4": "PADS4_SMD",         # (stage 2: the recovery pads, make_footprints.py)
    "twin": "R_TWIN_HAND", "twin_c": "C_TWIN_HAND",        # (stage 2: the twins' hand-solder lands, make_footprints.py)
    "sod123": "SOD-123_L2.7-W1.6-LS3.7-RD-1",
    "sc706": "SC-70-6_L2.0-W1.3-P0.65-LS2.1-BL", "tantb": "CASE-B_3528",   # (review pass 1: the LM66100, the tantalums)
    "tanta": "CAP-SMD_L3.2-W1.6-RD-C7171", "sma_rd": "SMA_L4.3-W2.6-LS5.2-RD",   # (review pass 2: C27 / C91, D9 SS34)
    "wson6": "TDFN-6_L2.0-W2.0-P0.65-TL-EP",                                       # (review pass 2: the TLV757P)   # (mini 2.2: the green LED, the charger, the cells' sockets, D3)
    "rgb2020": "LED-SMD_4P-L2.2-W2.0-P1.00_WS2815C-2020-4P",   # (the WS2812B-2020 RGB LEDs, 2026-11-15)
    "tssop14": "TSSOP-14_L5.0-W4.4-P0.65-LS6.4-BL", "vson10": "VSON-10_L3.0-W2.0-P0.50-TL", "sot563": "SOT-563_L1.6-W1.2-P0.50-LS1.6-BR", "ind2016": "IND-SMD_L2.0-W1.6-1",   # (2026-11-15: TPS63802, TPS61023, the 0.47 uH)
    "sot89": "SOT-89-3_L4.5-W2.5-P1.50-LS4.2-BR", "x2qfn12": "X2QFN-12_L1.6-W1.6-P0.40-TL",   # (host mode: U16, U17)
}

# U1, the ESP32-S3-WROOM-1U-N8R2 (8 MB quad flash, 2 MB quad PSRAM): its pins (datasheet v1.8, Table 3-1): 1 GND, 2 3V3,
# 3 EN, 4..7 IO4..7, 8 IO15, 9 IO16, 10 IO17, 11 IO18, 12 IO8, 13 IO19, 14 IO20, 15 IO3, 16 IO46, 17 IO9, 18 IO10,
# 19..22 IO11..14, 23 IO21, 24 IO47, 25 IO48, 26 IO45, 27 IO0, 28..30 IO35..37, 31..35 IO38..42, 36 RXD0, 37 TXD0,
# 38 IO2, 39 IO1, 40 GND, 41 EPAD (GND).
# 2026-11-15, the fresh floorplan (the user's: "make the routing as good as possible"): the placer moved each signal to
# the module pin that faces its block (README "The GPIO map"). Every pin checked against the module's Table 3-1 and the
# ESP32-S3 datasheet v2.2 (Table 2-1 reset pulls, Table 2-2 power-up glitches, Table 3-5 JTAG):
#  - no strapping pin (GPIO0 is BOOT only; GPIO3 / 45 / 46 unused), USB D- / D+ stay on GPIO19 / 20, UART0 on 43 / 44;
#  - the ADC nets on ADC1 (GPIO1..10): LINE_IN GPIO9 (CH8), LINE_SENSE GPIO8 (CH7), VBAT_ADC GPIO10 (CH9);
#  - the outputs that must stay low through reset (CLOCK, RESET, both MIDI loops, the host signal, the LED data, the
#    5 V enable's low) are not on GPIO18 (a 60 us HIGH glitch at power-up) nor GPIO39 (MTCK: a weak pull-up at reset
#    unless EFUSE_DIS_PAD_JTAG); the low-level glitches of GPIO1..17 (60 us) are harmless here: low = idle (MIDI: no
#    loop current, CLOCK / RESET: output low, the gate pull-downs agree);
#  - GPIO35..37: free on the N8R2 (only octal-PSRAM modules use them: Table 3-1 note b); GPIO47 / 48 run at 3.3 V on the
#    N8R2 (1.8 V only on R16V modules: note c); JTAG stays on USB (the default eFuses), so GPIO40..42 are plain GPIOs.
GPIO = {   # firmware's board.h (README "The GPIO map"): function -> (GPIO, module pin)
    "USB_DN": (19, "13"), "USB_DP": (20, "14"),
    "U0TXD": (43, "37"), "U0RXD": (44, "36"),   # J12, the recovery pads (a USB-UART adapter)
    "BOOT": (0, "27"),          # SW1 NO / BOOT to GND, R16 pull-up (strapping: low at reset = download mode)
    "LINE_IN": (9, "17"),       # ADC1_CH8: the digital LINE IN's node A (tip + ring)
    "LINE_SENSE": (8, "12"),    # ADC1_CH7: node B (the sleeve's matched copy)
    "VBAT_ADC": (10, "18"),     # ADC1_CH9: the cell through R10 / R11 1 M / 1 M, C8 at the pin
    "BTN_DOWN": (4, "4"), "BTN_YES": (5, "5"), "BTN_UP": (18, "11"),   # inputs, the internal pull-ups
    "MIDI_TX2": (42, "35"),      # UART2 TX, inverted: MIDI OUT 2's AO3400As (Q12 / Q13), R125 pull-down
    "CLK_IO": (36, "29"),        # CLOCK out, active high, R71 100 k pull-down
    "RST_IO": (21, "23"),       # RESET out, active high, R77 100 k pull-down
    "CC_INT": (13, "21"),       # U17 TUSB320LI's INT_N, open drain: the internal pull-up (battery order)
    "SCL": (39, "32"), "SDA": (35, "28"),   # R60 / R61 3.3 k; U17 through JP6 / JP5
    "HOST_EN": (47, "24"),      # high = the 5 V host signal on VBUS (U21 5A -> 6Y -> JP13 -> HOST5), R91 100 k pull-down
    "VBUS_SENSE": (14, "22"),   # VBUS through R88 22 k / R89 33 k: 5 V -> 3.0 V; high = USB present
    "LIN_DET": (48, "25"),      # J7's ring switch, R38 1 M up; low = no plug
    "LED_DATA": (38, "31"),     # the two WS2812B-2020s, chained, through R40 22 ohm (RMT)
    "MIDI_TX": (37, "30"),      # UART1 TX, inverted: MIDI OUT 1's AO3400As (Q10 / Q11), R124 pull-down
    # the 5 V aux boost's enable (2026-11-15, the user's: the firmware turns the 5 V loads off at the cell's cutoff and
    #  in sleep). GPIO41 (MTDI): no pull and no power-up glitch; R131 100 k up to +3V3 keeps the rail ON while the pin
    #  floats (reset, boot, erased or dead firmware); JP10 (a closed jumper) joins the pin to EN: cut it and the rail is
    #  on for good (the hatch). In deep sleep the firmware holds the pin low (gpio_hold_en + gpio_deep_sleep_hold_en:
    #  GPIO41 is not an RTC pin)
    "V5_EN_IO": (41, "34"),
}
STRAPPING = {0, 3, 45, 46}
CHIP_PIN = {"1": "GND", "2": "+3V3", "3": "CHIP_PU", "40": "GND", "41": "GND"}   # (the module: GND, 3V3, EN, GND, EPAD)

U1 = dict(CHIP_PIN)
U1.update({pin: name for name, (_g, pin) in GPIO.items()})

DNP = {"C24",                        # (pads only: the chip match's second shunt)
       "J5", "J6",                            # (the DIN sockets, bottom side: an optional hand-solder, README "DIN")
       "J2", "J3", "J7",                      # (stage 1f: the three jacks, bottom side, soldered by the user as the DINs)
       "J8", "J9",                            # (mini 2.2: the cells' PH2.0 sockets: the user fits them from the bottom)
       "J10", "JP1", "JP2",                   # (stage 1e: the screen's header and its VCC / GND jumpers: the user)
       "J11"}                                 # (stage 2: the power switch's pads: the user wires the switch, battery order)
# JLC's footprint for the DIN-504B-M10 numbers its pads 1..5 along the two rows, not as the DIN pins. Its frame and
# hardware/pcb's own table for the part (HY_PINS, the CUI SDS-50J pattern; the same in hardware/pcb/lib) agree: the
# shell pins at the front, the 3-pin row 2.5 mm before the 2-pin row, x the same way. So pad 1 (x -7.5) = DIN 3,
# pad 2 (x -5.0) = DIN 5, pad 3 (x 0) = DIN 2 (the middle), pad 4 (x +5.0) = DIN 4, pad 5 (x +7.5) = DIN 1; pads 6, 7 =
# the shell. (Until 2026-11-12 this was read mirrored: pad 2 = DIN 4, pad 4 = DIN 5.) UNVERIFIED: check with a meter.

PARTS = [
    # ---- USB-C: data + VBUS; 5.1 k on each CC (a sink); VBUS straight to the LDO (mini 2.2 simplified: USB power only)
    ("J1", "usbc", "USB-C 16P", "usbc", "C2765186",
     {"A1B12": "GND", "B1A12": "GND", "13": "GND", "14": "GND", "A4B9": "VUSB", "B4A9": "VUSB", "A5": "CC1", "B5": "CC2",
      "A6": "UDP", "B6": "UDP", "A7": "UDN", "B7": "UDN"}),
    # (22 ohm in series on D+ / D- by the chip: the edges and ringing on the 22 mm tracks, Espressif's checklist)
    # (ESD on D+ / D- right at the USB-C, before the 22 ohm: the user's ruling 2026-11-12; VBUS only its clamp reference)
    # (now the Hongjiacheng SRV05-4, C7420376: 5 the rail, 2 GND, I/O 1 / 3 / 4 / 6, the same functions as below)
    # (stage 1e: SRV05-4-P-T7, ProTek, a preferred part (no fee) instead of the USBLC6-2SC6. ProTek's datasheet (05150.R16):
    #  1 I/O1, 2 GND, 3 I/O2, 4 I/O3, 5 REF, 6 I/O4: D+ on I/O1 + I/O4, D- on I/O2 + I/O3, REF on VBUS, the USBLC6's own
    #  pin functions (1 / 6 D+, 3 / 4 D-, 2 GND, 5 VBUS). JLC's footprint is the same SOT-23-6 pattern turned 90: placed at
    #  270 its pads sit where the USBLC6's did)
    # (review pass 2: its REF on +3V3, not VUSB: through it the S3's D+ pull-up lifted VBUS to ~2.7 V with no host)
    ("U5", "esd", "SRV05-4", "sot236bl1", "C7420376", {"1": "UDP", "6": "UDP", "3": "UDN", "4": "UDN", "2": "GND", "5": "+3V3"}),
    #      stage 2 (the user's USB noise): R63, 0 ohm 0603 in VBUS after the USB-C and its ESD, before the charger and the
    #      power path: its land takes a ferrite bead later (BLM18PG121SN1D, C14709, Basic: 120 ohm at 100 MHz, 2 A, 50
    #      mohm: 25 mV at 500 mA); C27 a DNP 0805 after it to GND
    ("R63", "R", "0", "r0603", "C21189", {"1": "VUSB", "2": "VBUS"}),
    #      review pass 2 (L1, the hot-plug ring: VSYS 6.3 V against the LM66100's 6.0 V): two 10 uF 16 V tantalums on
    #      VBUS, their 3 ohm ESR the damping (sim: 5.5 V source, 1 uH cable, caps -20 %: VSYS 5.53 V, VREG usb 5.34 V)
    #      2026-11-15 (the Standard study's experiment A, fewer part types): 10 uF 25 V 0805 ceramics (C15850, as C100 /
    #      C101), each through 1 ohm (R151 / R152, R86's type): sim/vbus.py, 5.25 / 5.5 V hot plug through 0.7 uH: VBUS
    #      peak 5.48 / 5.74 V (the tantalums 6.08 / 6.37; bare ceramics 6.61 / 6.92; through 4.7 ohm 6.99 / 7.32)
    ("C27", "C", "10u 25V", "c0805", "C15850", {"1": "VB_D1", "2": "GND"}),
    ("R151", "R", "1", "r0402", "C25086", {"1": "VBUS", "2": "VB_D1"}),
    ("C91", "C", "10u 25V", "c0805", "C15850", {"1": "VB_D2", "2": "GND"}),
    ("R152", "R", "1", "r0402", "C25086", {"1": "VBUS", "2": "VB_D2"}),
    #      review pass 2 (M1): VBUS sense for the firmware (a self-powered USB device) to GPIO14: 100 k / 150 k (5 V -> 3.0 V;
    #      2026-11-15, experiment A: the board's 100 k / 150 k types; was 22 k / 33 k)
    ("R88", "R", "100k", "r0402", "C25741", {"1": "VBUS", "2": "VBUS_SENSE"}),
    ("R89", "R", "150k", "r0402", "C25755", {"1": "VBUS_SENSE", "2": "GND"}),
    ("R24", "R", "22", "r0402", "C25092", {"1": "UDP", "2": "USB_DP"}),
    ("R25", "R", "22", "r0402", "C25092", {"1": "UDN", "2": "USB_DN"}),
    ("R3", "R", "5.1k", "r0603", "C23186", {"1": "CC1", "2": "GND"}),
    ("R4", "R", "5.1k", "r0603", "C23186", {"1": "CC2", "2": "GND"}),
    # ---- USB HOST mode (the user's option A, 2026-11-15; battery order only, sim/host.py): jank hosts SELF-POWERED gear
    #      (audio sources, MIDI receivers). It never powers the device: a current-limited 5 V "signal" on the connector's
    #      VBUS tells the device a host is there. The usb order keeps R3 / R4 (Rd) and R63 instead (device only).
    #      U17, TI TUSB320LI (SLLSEP2D): the CC logic, a dual-role port. Unpowered (a dead cell, the switch off) it
    #      presents Rd, 5.1 k +-20 % (7.3.3 "Dead Battery Mode"), so a charger still starts. PORT low = UFP (a sink) at
    #      power-up; the firmware writes DRP + Try.SRC into MODE_SELECT / SOURCE_PREF for host mode (with DISABLE_TERM
    #      around it, 7.3.2). I2C on SCL / SDA at 0x47 (ADDR low), INT_N to GPIO13 (open drain: the chip's pull-up), EN_N
    #      low, ID unused. VBUS_DET through R94 (one 910 k since the buffer try; was 2 x 470 k = 940 k; TI asks 855 .. 920 k: its 2.95 .. 3.80 V VBUS
    #      threshold moves to 3.06 .. 3.94 V) from the connector's VBUS
    #      ESCAPE HATCH (the user's rule: device mode and charging must work with every host part dead, misplaced or
    #      removed): U17 sits behind five closed solder jumpers (SJ2C copper bridges), JP3 / JP4 (CC1 / CC2), JP5 / JP6 (SDA / SCL),
    #      JP7 (its VDD). Cut the five with a knife and U17 is out of the circuit (its other pads: GND, VBUS_DET through
    #      940 k, INT_N on a GPIO input); then fit 5.1 k 0603s on R3 / R4's lands (the usb order's Rd, DNP here): a plain
    #      sink. Unpowered, U17 itself presents Rd (SLLSEP2D 7.3.3)
    ("U17", "tusb", "TUSB320LIRWBR", "x2qfn12", "C2836598",
     {"1": "CC1U", "2": "CC2U", "3": "GND", "4": "VBUS_DET", "5": "GND", "6": "CC_INT", "7": "SDAU", "8": "SCLU",
      "10": "GND", "11": "GND", "12": "VDDU"}),
    ("C95", "C", "100n", "c0402", "C1525", {"1": "VDDU", "2": "GND"}),
    ("JP3", "sj2", "CC1 link", "sj2c", "", {"1": "CC1", "2": "CC1U"}),
    ("JP4", "sj2", "CC2 link", "sj2c", "", {"1": "CC2", "2": "CC2U"}),
    ("JP5", "sj2", "SDA link", "sj2c", "", {"1": "SDA", "2": "SDAU"}),
    ("JP6", "sj2", "SCL link", "sj2c", "", {"1": "SCL", "2": "SCLU"}),
    ("JP7", "sj2", "VDD link", "sj2c", "", {"1": "+3V3", "2": "VDDU"}),
    #      (buffer try: ONE 910 k 1 % 0402, C25800: TI SLLSEP2D asks RVBUS 855 .. 920 k (887 k typ; the text says 900 k); the old
    #       2 x 470 k = 940 k was 2 % over the maximum. VBUS_DET sees VBUS x 95 k / (910 k + 95 k).)
    ("R94", "R", "910k", "r0402", "C25800", {"1": "VUSB", "2": "VBUS_DET"}),
    #      the signal (2026-11-15, the power study's B; buffer try 2026-11-16): HOST5 is driven by U21's two spare inverters
    #      (5A -> 5Y -> 6A -> 6Y, VCC on V5C): HOST_EN (GPIO47) high -> HOST5 = V5C (~4.8 V through the gate's ~87 ohm), low or
    #      U21 unpowered -> HOST5 0 V (R92); R91 100 k holds the input low while the GPIO floats (both orders: U21 is in
    #      both). JP13, a closed solder jumper, joins the gate's output to HOST5: cut it and the host side is off HOST5.
    #      (Was Q7 / Q8 / R90 / R133 and the V5H net.) R93 2.2 k limits the signal (sim/host.py: a short on VBUS takes ~2 mA;
    #      RT93, its twin, takes a second resistor if a device wants a stiffer VBUS), D16 (BAT54C, one half) blocks any way
    #      back from the connector (a charger, a host). R93 is the signal's cut: lift it and nothing reaches VBUS
    ("R91", "R", "100k", "r0402", "C25741", {"1": "HOST_EN", "2": "GND"}),
    ("JP13", "sj2", "HOST5 link", "sj2c", "", {"1": "HOST5G", "2": "HOST5"}),
    ("R92", "R", "20k", "r0402", "C25765", {"1": "HOST5", "2": "GND"}),   # (chain list 3: 20 k, was 47 k; sim/host.py: Q9's gate 0.36 V in device mode)
    ("R93", "R", "2.2k", "r0402", "C25879", {"1": "HOST5", "2": "HOST_R"}),
    ("D16", "bav70", "BAT54C", "det", "C37704", {"1": "HOST_R", "3": "VUSB"}),
    #      Q9 (AO3401A) in R63's place: source on the connector's VBUS, drain on the board's VBUS (D3, the charger, C27 /
    #      C91 / C40, R88 / R89), gate on HOST5. Device mode, off or a dead cell: HOST5 = 0 V (R92), Q9 fully on (VGS =
    #      -VBUS: 60 mohm max at -4.5 V). Host mode: HOST5 = V5C (U21), above the signal on VBUS, Q9 off; its body diode points
    #      board -> connector, so the signal never reaches the board, and nothing of ours reaches a charger
    ("Q9", "pfet", "AO3401A", "tvs2", "C15127", {"1": "HOST5", "2": "VUSB", "3": "VBUS"}),
    # ---- power (stage 1d: one board, two orders, VARIANT below): VBUS through D3 into VSYS (both); in "battery" the cell
    #      into VSYS through U15 (an ideal diode) and D9; VSYS into the 3.3 V LDO. Review pass 2 (M2: the LDL1117's
    #      stability plot stops at 22 uF; the board has 49 uF of ceramics + the unplug bulk): ONE regulator for both
    #      orders, TI TLV75733PDRVR (SBVS322A: 1 A, VO +-1.5 %, dropout 475 mV max at 1 A, Iq 25 uA, VIN 1.45 .. 5.5 V
    #      (6 V abs), COUT 1 .. 200 uF in its recommended operating conditions, WSON-6 2 x 2 with a pad: theta-JA 100 C/W):
    #      the ceramics (49 uF; C86, the 100 uF tantalum, gone in experiment A, 2026-11-15) inside. DRV pins: 1 OUT, 2 / 5 NC, 3 GND, 4 EN, 6 IN,
    #      the pad GND. C3 (in) and C5 (out; C6 gone in the buffer try) right at its pins. (Earlier: the HT7833, the AMS1117, the LDL1117)
    ("D3", "D", "SS34", "sma_rd", "C8678", {"1": "VSYS", "2": "VBUS"}),   # (2026-11-15, experiment A: D9's SS34, was a B5819W)
    #      stage 2 (the user's: "off = off, still charges"): the power switch cuts VREG, the regulators' input (and U7's,
    #      LINE IN's LDO) from VSYS, after the charger and the power path: off, the board is dead even on USB while the
    #      cell still charges (U8 from VBUS). The switch (the user's: a toggle, not a slide): a panel-mount MTS-102 (CAX
    #      CAX-MTS102-YB-10.5JL, C55159907, M6 x 0.75 bushing, 2 A 250 VAC / 5 A 120 VAC: its drawing) in the lid's right
    #      wall, two short wires (its common S1 and one throw) to J11's two hand-solder pads (2 x 3 mm, 1.0 mm holes,
    #      4 mm apart): pad 1 VSYS, pad 2 VREG. R62, 0 ohm, across them, fitted in the usb order only (always on)
    ("J11", "pads2", "POWER SW", "pads2", "", {"1": "VSYS", "2": "VREG"}),
    ("R62", "R", "0", "r0402", "C17168", {"1": "VSYS", "2": "VREG"}),
    #      2026-11-15 (the power study's C, the user's): +3V3 from a TI TPS63802 buck-boost (C2845237; SLVSEU9D: VIN 1.3 ..
    #      5.5 V (start-up > 1.8 V), 11 uA, 2 A at VO 3.3 V, TJ -40 .. 125 C; DLA pins 1 EN, 2 MODE, 3 AGND, 4 FB, 5 PG, 6 VOUT,
    #      7 L2, 8 GND, 9 L1, 10 VIN; VFB 0.5 V +-1 %, its low-side FB resistor under 100 k; CIN >= 4 uF, COUT >= 7 uF
    #      effective, no upper limit; L 0.37 .. 0.57 uH, TI's list: Murata DFE201612E 0.47 uH, 5.5 A). +3V3 stays regulated
    #      while VREG is above ~1.8 V: no dropout dips (Wi-Fi bursts, the USB unplug, a switched-on 2.42"), the cell's empty
    #      point down to 3.3 V under load. R141 56 k / R142 10 k: 0.5 x 6.6 = 3.30 V. MODE low (JP11, power save; R144 a
    #      DNP land to +3V3 for forced PWM if its light-load bursts reach LINE IN). ESCAPE HATCH: EN through JP12 (a fitted
    #      0 ohm from VREG, R140 pulls EN low when JP12 is cut) and U2B, a DNP SOT-23-5 land for a 3.3 V LDO
    #      (ME6211 / AP2112 / XC6220 pinout: 1 VIN, 2 GND, 3 EN, 5 VOUT) from VREG: a dead TPS63802 -> cut JP12 (R140 holds EN low), solder the
    #      LDO (today's circuit, the 3.6 V empty point)
    ("U2", "bb10", "TPS63802DLAR", "vson10", "C2845237",
     {"10": "VREG", "1": "V3_EN", "2": "V3_MODE", "3": "GND", "4": "V3_FB", "6": "+3V3", "7": "V3_L2", "8": "GND", "9": "V3_L1"}),
    ("L4", "L", "0.47u", "ind2016", "C668312", {"1": "V3_L1", "2": "V3_L2"}),
    ("JP12", "sj2", "EN link", "sj2c", "", {"1": "VREG", "2": "V3_EN"}),   # (closed: EN on VREG; cut it and R140 holds EN low: the LDO hatch)
    ("R140", "R", "100k", "r0402", "C25741", {"1": "V3_EN", "2": "GND"}),
    #      (2026-11-15, experiment A: R141 100 k + R146 12 k over R142 20 k: 0.5 x (1 + 112 / 20) = 3.30 V, R2 <= 100 k as
    #      TI asks (SLVSEI3, 10.2.2.5); was 56 k / 10 k)
    ("R141", "R", "100k", "r0402", "C25741", {"1": "+3V3", "2": "V3_FBH"}),
    ("R146", "R", "12k", "r0402", "C25752", {"1": "V3_FBH", "2": "V3_FB"}),
    ("R142", "R", "20k", "r0402", "C25765", {"1": "V3_FB", "2": "GND"}),
    ("JP11", "sj2", "MODE link", "sj2c", "", {"1": "V3_MODE", "2": "GND"}),   # (closed: MODE low, power save; cut it AND fit R144 for forced PWM: one step)
    ("R144", "R", "0 DNP", "r0402", "", {"1": "V3_MODE", "2": "+3V3"}),
    ("U2B", "ldo5", "LDO 3.3 DNP", "sot235", "", {"1": "VREG", "2": "GND", "3": "VREG", "5": "+3V3"}),
    #      (D15, the TLV757P's reverse-current Schottky, gone with it: the TPS63802 has a true shutdown)
    # (signoff3, round 3 R4: C3 22 uF 25 V X5R 0805 (Samsung CL21A226MAQNNNE, C45783, Basic) in the 10 uF 0603's place:
    #  ~11 .. 13 uF at 5 V (ASSUMED DC-bias retention) against TI's CI >= 4 uF effective (SLVSEU9D 8.3); no room for a second
    #  cap at U2's pins)
    ("C3", "C", "22u", "c0805", "C45783", {"1": "VREG", "2": "GND"}),
    ("C5", "C", "22u", "c0603", "C59461", {"1": "+3V3", "2": "GND"}),
    #      the power path ("battery"). Review pass 1 / the sim pass: Q1 (an AO3401A with its gate on VBUS, R50 10 k down)
    #      left a ~25 ms gap at the USB unplug (it conducts only once VBUS has drained below the cell: +3V3 2.69 V on a
    #      3.4 V cell) and the S3's D+ pull-up lifted VBUS to ~2.4 V with no host (Q1 half on). Now U15, a TI LM66100
    #      (SLVSEZ8A: 1.5 A, 91 mohm at 3.6 V, VIN 1.5 .. 5.5 V), CE on VOUT = an ideal diode BAT_P -> VSYS: on when VSYS
    #      falls 150 mV below the cell (tON 40 us), off when USB lifts VSYS over it; Q1 and R50 gone. D9 stays (the 40 us)
    #      2026-11-15 (the user's, the BOM study): back to a P-FET, now with the TPS63802 behind it: Q1 AO3401A (Basic, on
    #      the BOM) as Microchip AN1149's load-sharing switch: drain on BAT_P, source on VSYS, gate on VBUS (R50 100 k to
    #      GND): on when VBUS is gone, off while USB lifts VBUS above VSYS; its body diode and D9 carry the cell at the
    #      unplug until VBUS has drained (~0.23 .. 0.28 s). The TPS63802 holds +3V3 through it (VSYS down to 2.67 V at a
    #      3.2 V cell): sim/power.py, the q1 lines (+3V3 >= 3.247 V worst). The LM66100's Extended fee goes. ESCAPE HATCH:
    #      Q1 open -> D9 carries the cell (0.3 .. 0.45 V lower VSYS, inside the TPS63802's range: nothing to do); Q1
    #      shorted (USB would push VSYS into the cell) -> lift Q1
    ("Q1", "pfet", "AO3401A", "tvs2", "C15127", {"1": "VBUS", "2": "VSYS", "3": "BAT_P"}),
    ("R50", "R", "100k", "r0402", "C25741", {"1": "VBUS", "2": "GND"}),
    #      D9 (beside Q1's body diode): carries the load until Q1 turns on
    ("D9", "D", "SS34", "sma_rd", "C8678", {"1": "VSYS", "2": "BAT_P"}),   # (review pass 2, the user's Q1 (c): a 3 A Schottky,
    #  a lower drop for U15's 40 .. 90 us turn-on; with the 3.6 V firmware cutoff)
    #      the charger ("battery"): U8 TP4054-42 (4.2 V) from VBUS, R51 5.1 k on PROG = ~196 mA (1000 V / R_PROG), CHRG
    #      left open; 10 uF at VCC and at BAT
    ("U8", "chg", "TP4054-42", "sot25bl", "C32574", {"4": "VBUS", "3": "BAT_P", "2": "GND", "5": "PROG"}),
    ("R51", "R", "12k", "r0402", "C25752", {"1": "PROG", "2": "GND"}),   # (review pass 2, the user's: ~83 mA, so charge +
    #  a Wi-Fi burst stay under USB's 500 mA: ~490 mA worst; was 5.1 k, ~187 mA)
    ("C40", "C", "10u", "c0603", "C19702", {"1": "VBUS", "2": "GND"}),
    #      review pass 1 (the sim pass, the TP4054's sheet): 1 ohm in series with C41 (a large ceramic on BAT with no cell)
    ("C41", "C", "10u", "c0603", "C19702", {"1": "BAT_CS", "2": "GND"}),
    ("R86", "R", "1", "r0402", "C25086", {"1": "BAT_P", "2": "BAT_CS"}),
    #      the cell's protection ("battery"): U9 XB5353A (over-charge, over-discharge, over-current, its own FETs in the
    #      minus lead): its datasheet's application circuit: VDD through R52 100 ohm from the cell's + (XB_VDD), C42 100 nF
    #      from XB_VDD to its GND (the cell's minus, BAT_N) right at the IC; VM (4, 5) on the board's GND; pin 1 VT open
    ("U9", "prot", "XB5353A", "sot235", "C80628", {"3": "XB_VDD", "2": "BAT_N", "4": "GND", "5": "GND"}),
    ("R52", "R", "100", "r0402", "C25076", {"1": "BAT_P", "2": "XB_VDD"}),
    ("C42", "C", "100n", "c0402", "C1525", {"1": "XB_VDD", "2": "BAT_N"}),
    #      reverse polarity ("battery", stage 1d): Q2 AO3401A in the cell's + lead, drain on the sockets' + (BAT_C), source
    #      on BAT_P, gate straight to GND (buffer try: R53 gone, VGS = -4.2 V on a cell against +-12 V): a cell the right way round turns it on (its body diode first), a cell
    #      the wrong way round meets the body diode reversed and a gate above the source: off
    ("Q2", "pfet", "AO3401A", "tvs2", "C15127", {"1": "GND", "2": "BAT_P", "3": "BAT_C"}),
    #      the cells: two PH2.0 sockets in parallel (the user's choice, risk accepted), never fitted: the user inserts them
    #      from the bottom (the cells under the board) and solders them on top. Pin 1 (square) = +, pin 2 = - (stage 1d:
    #      the common convention, SparkFun / Adafruit); whoop leads vary between vendors: check every cell with a meter
    #      (buffer try, the user's: bottom right above SW3, VERTICAL B2B as before: the side-entry plug hit the UP post and the cells)
    ("J8", "ph2", "PH2.0 cell 1", "ph2", "C131337", {"1": "BAT_C", "2": "BAT_N"}),
    ("J9", "ph2", "PH2.0 cell 2", "ph2", "C131337", {"1": "BAT_C", "2": "BAT_N"}),
    #      the cell's level: 1 M / 1 M + 100 nF on GPIO10 (ADC1_CH9). R10 ("battery" only) from the cell's +; R11 and C8
    #      in both orders: in "usb" GPIO10 is held at 0 V (not floating), which also tells the firmware which board it runs
    #      on. Review pass 1 / the sim pass: with 1 M / 1 M (500 k) the ADC's sampling cap, shared with LINE_IN at 41.7 kHz,
    #      pulled the reading 30 .. 131 mV low (up to 0.64 V); 100 k / 100 k (50 k) cuts it 10 x for 21 uA from the cell
    #      (signoff3, round 3 R8: back to 1 M / 1 M, for the drain while switched off: ~25 uA divider + back-power into the
    #      unpowered S3 -> ~6 uA (the board's whole off drain ~9 .. 17 uA with the XB5353A, TP4054, D3). The shared
    #      sampling cap's error comes back, affine in the cell: V = (Vcell/2 + k Vline) / (1 + k), k = 500 k x Cs x 41.7 kHz;
    #      the firmware must take k from the boot one-shot read (an offset over-reads an empty cell by up to ~0.2 V), or re-read
    #      by one-shot now and then; worktrees/mini22-review-scratch/REVIEW.md section 9)
    ("R10", "R", "1M", "r0402", "C26083", {"1": "BAT_P", "2": "VBAT_ADC"}),
    ("R11", "R", "1M", "r0402", "C26083", {"1": "VBAT_ADC", "2": "GND"}),
    ("C8", "C", "100n", "c0402", "C1525", {"1": "VBAT_ADC", "2": "GND"}),
    # ---- the module (2026-11-15, the user's): Espressif ESP32-S3-WROOM-1U-N8R2 (C3013944: 8 MB quad flash, 2 MB quad
    #      PSRAM, a u.FL for the lid's FPC antenna). Its datasheet (WROOM-1 / WROOM-1U v1.8): 41 pins (Table 3-1; pin 41
    #      the EPAD, GND); IO35 .. IO37 free on quad-PSRAM modules (footnote b); GPIO26 .. 32 are inside; IO33 / IO34 are
    #      not on its pins (LED_DATA moved to IO35). Its peripheral schematic (Figure 9-1): 22 uF + 0.1 uF at 3V3 (C17, C16
    #      by pin 2), the EN RC 10 k / 1 uF (R8 / C7). The crystal, the RF match, the u.FL line, VDD3P3 / VDD_SPI / VDDA
    #      decoupling and the chip's EPAD vias are the module's own now: Y1, C9, C10, R17, L2, L3, C23, C24, C26, R19, J4,
    #      C11 .. C15, C18 .. C22, C28 and the twins CT9 / CT10 are gone
    ("U1", "wroom", "ESP32-S3-WROOM-1U-N8R2", "wroom1u", "C3013944", U1),
    ("C17", "C", "22u", "c0603", "C59461", {"1": "+3V3", "2": "GND"}),
    ("C16", "C", "100n", "c0402", "C1525", {"1": "+3V3", "2": "GND"}),
    #      EN: 10 k / 1 uF (Figure 9-1's RC; no supervisor). GPIO0: 10 k pull-up (the checklist), the button to GND
    ("R8", "R", "10k", "r0402", "C25744", {"1": "+3V3", "2": "CHIP_PU"}),
    ("C7", "C", "1u", "c0402", "C52923", {"1": "CHIP_PU", "2": "GND"}),
    ("R16", "R", "10k", "r0402", "C25744", {"1": "+3V3", "2": "BOOT"}),
    # ---- the buttons (stage 1e, the user's "option 1 and 2 on one board"): four under big caps, NO / BACK = BOOT on
    #      GPIO0 (R16 its pull-up, nothing else), UP / DOWN / YES on GPIO18 / 4 / 5 with the chip's internal pull-ups
    #      (no part). All four the Basic 5.1 mm tactile (C318884). Stage 1f (the user's): no RST button (CHIP_PU keeps its
    #      10 k / 1 uF), the four in a cross: UP top, DOWN bottom, NO / BOOT left, YES right (README "stage 1f")
    ("SW1", "sw", "NO / BOOT", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BOOT", "4": "BOOT"}),
    ("SW3", "sw", "UP", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BTN_UP", "4": "BTN_UP"}),
    ("SW4", "sw", "DOWN", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BTN_DOWN", "4": "BTN_DOWN"}),
    ("SW5", "sw", "YES", "sw", "C318884", {"1": "GND", "2": "GND", "3": "BTN_YES", "4": "BTN_YES"}),
    # ---- the screen (option 2, stage 1e; since review pass 2 a 1.54" module on J10, a 2.42" on J10B's lead): a 1 x 4
    #      2.54 mm header, NOT fitted (stage 1e's text follows): the user solders a cheap 0.96" 128 x 64
    #      I2C OLED module (SSD1306 / SSD1315) on it. Pins 3 / 4 are SCL / SDA; pins 1 / 2 are GND / VCC or VCC / GND
    #      (modules differ): JP1 / JP2, bare 3-pad solder jumpers (no part), take each to GND or +3V3. 10 k pull-ups on
    #      SCL / SDA and 10 uF + 100 nF at the header, fitted on every board (Basic)
    ("J10", "hdr4", "OLED 1x4", "hdr4", "C2691448", {"1": "OLED_P1", "2": "OLED_P2", "3": "SCL", "4": "SDA"}),
    ("JP1", "sj3", "P1: GND | VCC", "sj3", "", {"1": "GND", "2": "OLED_P1", "3": "+3V3"}),
    ("JP2", "sj3", "P2: VCC | GND", "sj3", "", {"1": "+3V3", "2": "OLED_P2", "3": "GND"}),
    #      2026-11-15 (the UI firmware's 400 kHz with the 2.42" module's lead): 3.3 k, not 10 k. Rise time 0.85 x R x C:
    #      3.3 k alone and 100 pF of bus (ASSUMED: the lead) 280 ns (400 kHz asks < 300 ns; 10 k gave ~850 ns). Sink
    #      current, worst case with a module's own 4.7 k pull-up in parallel (ASSUMED: typical SSD1309 boards carry 4.7 k
    #      or 10 k): 3.3 k // 4.7 k = 1.94 k -> 1.70 mA, under I2C's 3 mA; the TUSB320's VOL 0.4 V is specified at 1.6 mA
    #      (SLLSEP2D): 0.1 mA over its test point with a 4.7 k module, inside with a 10 k one (3.3 k // 10 k: 1.33 mA)
    ("R60", "R", "2.2k", "r0402", "C25879", {"1": "+3V3", "2": "SCL"}),   # (chain list 2: R93's 2.2 k, was 3.3 k)
    ("R61", "R", "2.2k", "r0402", "C25879", {"1": "+3V3", "2": "SDA"}),
    ("C60", "C", "10u", "c0603", "C19702", {"1": "+3V3", "2": "GND"}),
    # ---- the indicator (2026-11-15, the user's: two addressable RGB LEDs, chained on one GPIO, behind the one window;
    #      they replace the red 0603 + green 0805, R40 / R41 1 k and their twins). WorldSemi WS2812B-2020-V6 (2.0 x 2.0;
    #      its sheet: VDD 3.3 .. 5.3 V, VIH 0.55 x VDD, 1 uA quiescent, 12 mA a colour; pins 1 DO, 2 GND, 3 DI, 4 VDD, as
    #      JLC's symbol). VDD from VREG (after the switch): 3.4 .. 3.5 V on a cell at the 3.6 V cutoff, ~4.6 .. 4.9 V on
    #      USB, so VIH is 2.7 V at most and GPIO38's 3.3 V drives DI with no level shifter (a 74LVC1G125 on VREG would not:
    #      TI gives VIH 0.7 x VCC at 4.5 .. 5.5 V and nothing between 3.6 and 4.5 V). R40 22 ohm in the data line at
    #      the GPIO, 100 nF at each LED (its sheet's Cn). Both orders. LED4's DO is left open (four since 2026-11-15). VLED: VREG through R98
    #      4.7 ohm with C98 10 uF (the user's: a filter against the hot-plug ring, the LEDs' 5.3 V); sim (the sim-pass2
    #      hot-plug decks, a 5.5 V source, 0.7 .. 1 uH): the usb order's VREG peaks 5.37 V, VLED 5.00 V with the filter
    #      (battery order: 5.11 / 4.94 V); 24 mA through R98 drops 0.11 V. A Wi-Fi burst takes VSYS to 3.24 .. 3.29 V at a
    #      3.6 V cell (sim/power.py): VLED under the LED's 3.3 V for ~2 ms below a ~3.8 V cell (README: the firmware
    #      refreshes the LEDs every frame; one colour each on the cell)
    # (review pass 3, 2026-11-15: WS2812C-2020-V6, C55109522: JLCONE left C52917434 unselected. Same 2020 land (its EasyEDA
    #  footprint has the same four pads), same pins (1 DO, 2 GND, 3 DI, 4 VDD), VDD 3.3 .. 5.3 V, VIH 0.55 VDD (both
    #  World Semi sheets); 5 mA per channel instead of 12 (dimmer: green 250 .. 400 mcd against 425 .. 1700))
    ("LED1", "rgb", "WS2812C-2020-V6", "rgb2020", "C55109522", {"1": "LED_D2", "2": "GND", "3": "LED_DI", "4": "VLED"}),
    # (2026-11-15, the user's "4 LEDs, one per beat": LED1 .. LED4 in a row, beat 1 .. 4 left to right, chained on
    #  LED_DATA (LED1 first), each with its 100 nF (the sheet's Cn). Fitted on every board, so the row sits clear of both
    #  screens' outlines (README "Four beat LEDs"). 4 x 3 x 5 mA = 60 mA all white: R98 10 ohm drops 0.6 V (36 mW of
    #  its 62.5), VLED >= 4.1 V at the rail's low corner (LED minimum 3.3 V); WS2812B parts (12 mA a channel, 144 mA)
    #  would need R98 ~1 ohm)
    ("LED2", "rgb", "WS2812C-2020-V6", "rgb2020", "C55109522", {"1": "LED_D3", "2": "GND", "3": "LED_D2", "4": "VLED"}),
    ("LED3", "rgb", "WS2812C-2020-V6", "rgb2020", "C55109522", {"1": "LED_D4", "2": "GND", "3": "LED_D3", "4": "VLED"}),
    ("LED4", "rgb", "WS2812C-2020-V6", "rgb2020", "C55109522", {"2": "GND", "3": "LED_D4", "4": "VLED"}),
    ("C102", "C", "100n", "c0402", "C1525", {"1": "VLED", "2": "GND"}),
    ("R40", "R", "22", "r0402", "C25092", {"1": "LED_DATA", "2": "LED_DI"}),   # (experiment A: R24's 22 ohm, was 33)
    ("C96", "C", "100n", "c0402", "C1525", {"1": "VLED", "2": "GND"}),
    # (sign-off stage 2, 2026-11-17, C2: one 100 nF per LED as the sheet asks: C96 LED1, C97 LED2, C102 LED3, C103 LED4)
    ("C97", "C", "100n", "c0402", "C1525", {"1": "VLED", "2": "GND"}),
    ("C103", "C", "100n", "c0402", "C1525", {"1": "VLED", "2": "GND"}),
    ("R98", "R", "10", "r0402", "C25077", {"1": "V5A", "2": "VLED"}),   # (chain list 1, 2026-11-15: R145's 10 ohm, was 4.7 0603; from the 5 V aux rail)
    ("C98", "C", "10u", "c0603", "C19702", {"1": "VLED", "2": "GND"}),
    #      ESCAPE HATCHES: R98 is VLED's cut (lift it: the RGB LEDs unpowered). The plain-LED fallback, right under the
    #      window: lift LED1 and solder a 0603 LED across its pads 3 (DI: the anode) and 2 (GND: the cathode), 1.72 mm
    #      apart, and swap R40 (22 ohm) for 1 k: GPIO38 drives it, active high
    # ---- MIDI OUT 1 and MIDI OUT 2 (2026-11-15, the user's: a true 5 V current loop, MIDI 1.0's 5 V circuit, from the
    #      5 V aux rail through JP8 (V5M, the MIDI group's closed jumper). Every connector its own loop (stage 1f): ring /
    #      DIN 4 = the source from V5M through 220 ohm 1206, tip / DIN 5 = the sink through 220 ohm 1206 to an AO3400A's drain (a 2N7002 before signoff3);
    #      the FET's gate on the UART TX (MIDI_TX GPIO37: OUT 1's jack and DIN; MIDI_TX2 GPIO42: OUT 2's), its source GND.
    #      The firmware inverts the TX (uart_set_line_inverse(UART_SIGNAL_TXD_INV)): idle low = the FET off = no current;
    #      R124 / R125 100 k hold the gates low while the GPIOs float. The GPIOs carry no loop current. The FETs are open-
    #      drain, so a short on a tip or out-to-out draws nothing (buffer try 2026-11-16, the user's ruling: single 220 ohm
    #      1206 source and sink, MIDI 1.0's own value, was 2 x 470 ohm 0805 in parallel; 1206 for the ring-to-GND fault:
    #      22.8 mA, 114 mW). The receiver gets 3.52 .. 5.79 mA over the corners (ngspice, generic models: the buffer-try
    #      MIDI sim, sim/midi.py with 220 ohm). ESCAPE HATCH (back to the 3.3 V drive): cut JP8, bridge R123 (+3V3 -> V5M),
    #      lift a dead FET and bridge its DNP land R126 .. R129 (its sink node -> its GPIO); the firmware un-inverts the TX
    ("J2", "jack5", "MIDI OUT", "jack5", "C16684", {"5": "MO_T", "2": "MO_R", "1": "GND"}),
    ("J5", "din", "MIDI OUT DIN", "din", "C2939346", {"4": "MD_R", "2": "MD_T", "3": "GND", "6": "GND", "7": "GND"}),   # (pad 4 = DIN 4, pad 2 = DIN 5)
    ("JP8", "sj2", "MIDI 5V link", "sj2c", "", {"1": "V5A", "2": "V5M"}),
    ("R123", "R", "0 DNP", "r0402", "", {"1": "+3V3", "2": "V5M"}),
    #      (2026-11-16, buffer try, the user's ruling: the MIDI loops stay FET sinks, open-drain, placed at each connector:
    #      a short on a tip or out-to-out draws nothing. Source and sink are single 220 ohm 1206 (MIDI 1.0's own value;
    #      1206 for the ring-to-GND fault: 22.8 mA, 115 mW), was 2 x 470 ohm 0805 in parallel.)
    ("R14", "R", "220", "r1206", "C17957", {"1": "V5M", "2": "MO_R"}),
    ("R15", "R", "220", "r1206", "C17957", {"1": "MO_K", "2": "MO_T"}),
    # (signoff3, round 3 R6: the sinks AO3400A (C20917, Basic; AOS: 1 G, 2 S, 3 D as the 2N7002; VGS(th) 0.65 .. 1.45 V,
    #  RDS(on) < 48 mohm at VGS 2.5 V) for the 2N7002 (Vth up to 2.5 V, RDS(on) given only at 5 V): full current from a 3.3 V gate)
    ("Q10", "nfet", "AO3400A", "tvs2", "C20917", {"1": "MIDI_TX", "2": "GND", "3": "MO_K"}),
    ("R54", "R", "220", "r1206", "C17957", {"1": "V5M", "2": "MD_R"}),
    ("R55", "R", "220", "r1206", "C17957", {"1": "MD_K", "2": "MD_T"}),
    ("Q11", "nfet", "AO3400A", "tvs2", "C20917", {"1": "MIDI_TX", "2": "GND", "3": "MD_K"}),
    ("R124", "R", "100k", "r0402", "C25741", {"1": "MIDI_TX", "2": "GND"}),
    ("R126", "R", "0 DNP", "r0402", "", {"1": "MO_K", "2": "MIDI_TX"}),
    ("R127", "R", "0 DNP", "r0402", "", {"1": "MD_K", "2": "MIDI_TX"}),
    #      MIDI OUT 2: the same parts, its own nets, from MIDI_TX2 (GPIO42)
    ("J3", "jack5", "MIDI OUT 2", "jack5", "C16684", {"5": "MO2_T", "2": "MO2_R", "1": "GND"}),
    ("J6", "din", "MIDI OUT 2 DIN", "din", "C2939346", {"4": "MD2_R", "2": "MD2_T", "3": "GND", "6": "GND", "7": "GND"}),
    ("R21", "R", "220", "r1206", "C17957", {"1": "V5M", "2": "MO2_R"}),
    ("R20", "R", "220", "r1206", "C17957", {"1": "MO2_K", "2": "MO2_T"}),
    ("Q12", "nfet", "AO3400A", "tvs2", "C20917", {"1": "MIDI_TX2", "2": "GND", "3": "MO2_K"}),
    ("R56", "R", "220", "r1206", "C17957", {"1": "V5M", "2": "MD2_R"}),
    ("R57", "R", "220", "r1206", "C17957", {"1": "MD2_K", "2": "MD2_T"}),
    ("Q13", "nfet", "AO3400A", "tvs2", "C20917", {"1": "MIDI_TX2", "2": "GND", "3": "MD2_K"}),
    ("R125", "R", "100k", "r0402", "C25741", {"1": "MIDI_TX2", "2": "GND"}),
    ("R128", "R", "0 DNP", "r0402", "", {"1": "MO2_K", "2": "MIDI_TX2"}),
    ("R129", "R", "0 DNP", "r0402", "", {"1": "MD2_K", "2": "MIDI_TX2"}),
    #      review pass 1 (M7): ESD on the MIDI connectors' lines, one SRV05-4 (the USB's part, now Hongjiacheng C7420376: no new type) per
    #      output, its four I/O on that output's jack tip / ring and DIN 5 / 4, REF on +3V3 (pins 1 I/O1, 2 GND, 3 I/O2,
    #      4 I/O3, 5 REF, 6 I/O4), between the connectors' pads and their resistors
    #      (2026-11-15: their REF on V5M: the lines now idle at 5 V, a REF on +3V3 would feed it through the clamp diodes)
    ("U11", "esd4", "SRV05-4", "sot236bl1", "C7420376", {"1": "MO_T", "3": "MO_R", "4": "MD_R", "6": "MD_T", "2": "GND", "5": "V5M"}),
    ("U12", "esd4", "SRV05-4", "sot236bl1", "C7420376", {"1": "MO2_T", "3": "MO2_R", "4": "MD2_R", "6": "MD2_T", "2": "GND", "5": "V5M"}),
    # ---- LINE IN, digital (2026-11-15, the user's, the BOM study): no op-amp, no AVDD LDO. Two matched passive networks
    #      into two ADC1 pins; the firmware takes the difference. Node A (LINE_IN, GPIO9 = ADC1_CH8, module pin 17): tip
    #      and ring each through R30 / R31 20 k and C30 / C31 1 uF; node B (LINE_SENSE, GPIO8 = ADC1_CH7, module pin 12):
    #      the sleeve through two identical branches, R65 / R66 20 k and C65 / C66 1 uF (so B sees the sleeve exactly as A
    #      sees tip and ring). Each node: 12 k to the bias (R32 / R67) and 2.2 nF C0G 5 % to GND (C32 / C67: the
    #      anti-alias pole, 20 k // 20 k // 12 k = 5.45 k: 13.3 kHz). The coupling caps keep every DC current off the
    #      bias, so the weak divider holds: R33 150 k from +3V3 / R34 120 k = 1.467 V (the middle of ATTEN3's 0 .. 2.9 V),
    #      C33 10 uF (its impedance common to both nodes). A - B = 0.273 x ((tip - sleeve) + (ring - sleeve)): the true
    #      difference (sim/linein.py: gain, levels, the rejection with 1 % R / 5 % C0G, +-12 V). R36 / R37 100 k stay
    #      (the jack side's DC, and the plug detect: the ring switch reads ring = 0 V with no plug). R39 100 ohm (the
    #      ground study) and D7 / D14 (PESD15VL2BT since the buffer try) stay
    ("J7", "jack5", "LINE IN", "jack5", "C16684", {"5": "LIN_T", "2": "LIN_R", "1": "LIN_S", "3": "LIN_DET"}),
    ("R39", "R", "100", "r0402", "C25076", {"1": "LIN_S", "2": "GND"}),
    ("R36", "R", "100k", "r0402", "C25741", {"1": "LIN_T", "2": "GND"}),
    ("R37", "R", "100k", "r0402", "C25741", {"1": "LIN_R", "2": "GND"}),
    ("R38", "R", "1M", "r0402", "C26083", {"1": "+3V3", "2": "LIN_DET"}),
    # (stage 1e: two SMAJ5.0CA, Hongjiacheng, a preferred part (no fee), one per line. Its datasheet ("SMAJ series", rev
    #  2.2, 2023-10): bidirectional, VRWM 5 V, VBR 6.4 .. 7.0 V, VC 9.2 V at 43.5 A, 400 W)
    #  (buffer try 2026-11-16: the SMAJ5.0CA broke down at 6.4 .. 7.0 V and clipped +-10 V modular signals; now two Nexperia
    #  PESD15VL2BT, C38838, SOT-23 (datasheet Rev. 02: pin 1 / 2 the lines, pin 3 the common, VRWM 15 V, VBR 17.1 .. 20.3 V,
    #  VCL 26 V, 350 W, 23 kV ESD): D7 tip + ring, D14 the sleeve; the 20 k series resistors take the rest)
    ("D7", "tvs2l", "PESD15VL2BT", "tvs2", "C38838", {"1": "LIN_T", "2": "LIN_R", "3": "GND"}),
    ("D14", "tvs2l", "PESD15VL2BT", "tvs2", "C38838", {"1": "LIN_S", "3": "GND"}),   # (review pass 2, L7: the sleeve)
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
    ("R33", "R", "150k", "r0402", "C25755", {"1": "+3V3", "2": "LIN_VB"}),        # (the bias: 3.3 V x 120 / 270 = 1.47 V)
    ("R34", "R", "100k", "r0402", "C25741", {"1": "LIN_VB", "2": "LIN_VBL"}),   # (chain list 4: 100 k + R153 20 k = 120 k; RT34 across the 100 k)
    ("R153", "R", "20k", "r0402", "C25765", {"1": "LIN_VBL", "2": "GND"}),
    ("C33", "C", "10u", "c0603", "C19702", {"1": "LIN_VB", "2": "GND"}),
]

# ---- the 5 V aux rail (2026-11-15, the power study's B, the user's): a TI TPS61023 boost (C919459; SLVSF14B: VIN 0.5 ..
#      5.5 V, VOUT abs max 7 V, OVP 5.5 .. 6.0 V, VREF 595 mV (580 .. 610), 1 MHz with auto PFM at light load, 20 uA from
#      VOUT, true disconnect when off, COUT 4 .. 1000 uF effective, L 0.37 .. 2.9 uH, TJ -40 .. 125 C; DRL pins 1 FB, 2 EN,
#      3 VIN, 4 GND, 5 SW, 6 VOUT) from VREG (after the power switch: off = off) -> V5A: the RGB LEDs (R98), the MIDI loops
#      (JP8), CLOCK / RESET (JP9) and the host signal (JP13), each through its own closed jumper (cut it to cut that
#      group). L5 the same 0.47 uH as L4 (one type). R134 1 M / R135 100 k + R154 20 k + R136 22 k: 0.595 x (1 + 1000 / 142) = 4.79 V (signoff3; 20 k: 4.85 V, 18 k: 4.91 V)
#      (4.58 .. 4.99 V with VREF 580 .. 610 mV and 1 % parts: under the clamp's 4.93 V low corner on most boards).
#      EN (2026-11-15, the user's): GPIO41 (V5_EN_IO) through JP10 (0 ohm), R131 100 k up to +3V3: ON unless the
#      firmware drives it low (EN high > 1.2 V, low < 0.42 V: SLVSF14B). ESCAPE HATCH: cut JP10 and the rail is on
#      for good (dead firmware or a bad pin). Device mode and charging never depend on it.
#      The clamp: U19, CJ431 (C3113, Basic; its sheet: Vref 2.475 .. 2.525 V, 1 .. 150 mA, 300 mW, -25 .. 85 C; SOT-23
#      1 REF, 2 K, 3 A) with R138 20 k / R139 20 k on its cathode, behind R145 10 ohm: 5.0 V (4.95 .. 5.05 V; was 51 k: 5.21 V).
#      A wrong +12 V on CLOCK and RESET at once feeds ~12 mA into the rail through the BAV99s and the P-FETs' body
#      diodes; the boost cannot sink it: U19 holds V5A under the WS2812B-2020's 5.3 V maximum (sim/cv.py)
AUX5 = [
    ("U18", "boost6", "TPS61023DRLR", "sot563", "C919459",
     {"1": "V5_FB", "2": "V5_EN", "3": "VREG", "4": "GND", "5": "V5_SW", "6": "V5A"}),
    ("L5", "L", "0.47u", "ind2016", "C668312", {"1": "VREG", "2": "V5_SW"}),
    ("C99", "C", "10u", "c0603", "C19702", {"1": "VREG", "2": "GND"}),
    # (sign-off stage 2, 2026-11-17, R6: C99 sits ~10 mm of copper from U18's VIN; TI (SLVSF14B 10.1) wants the input
    #  capacitor at VIN / GND: C104 by pins 3 and 4. An 0402 (Samsung CL05A106MQ5NUNC, 6.3 V X5R, Basic): an 0603 has
    #  no room between R134 and U18; at 5 V it keeps ~2 .. 3 uF (DC bias, ASSUMED from the class), the HF loop's part;
    #  C99 and C72 / C92 hold the bulk)
    ("C104", "C", "10u", "c0402", "C15525", {"1": "VREG", "2": "GND"}),
    ("C100", "C", "10u 25V", "c0805", "C15850", {"1": "V5A", "2": "GND"}),
    ("C101", "C", "10u 25V", "c0805", "C15850", {"1": "V5A", "2": "GND"}),
    ("R134", "R", "1M", "r0402", "C26083", {"1": "V5A", "2": "V5_FB"}),
    ("R135", "R", "100k", "r0402", "C25741", {"1": "V5_FB", "2": "V5_FBM"}),   # (chain list 4: 100 k + R154 20 k = 120 k, + R136 22 k)
    ("R154", "R", "20k", "r0402", "C25765", {"1": "V5_FBM", "2": "V5_FBL"}),
    ("R136", "R", "22k", "r0402", "C25768", {"1": "V5_FBL", "2": "GND"}),   # (signoff3, round 3 R2: 4.79 V nominal, 4.58 .. 4.99 V
    #  with VREF 580 .. 610 mV and 1 % parts: under the clamp's lowest threshold (4.93 V: Vref 2.475 V, 1 % parts, Iref) on most boards, above the 74AHCT04's 4.5 V. Was 20 k: 4.85 V)
    ("R131", "R", "100k", "r0402", "C25741", {"1": "+3V3", "2": "V5_EN"}),     # (2026-11-15: EN's pull-up: ON by default)
    ("JP10", "sj2", "V5 EN link", "sj2c", "", {"1": "V5_EN", "2": "V5_EN_IO"}),
    ("U19", "shunt", "CJ431", "det", "C3113", {"1": "V5_CL", "2": "V5_CK", "3": "GND"}),
    # (review pass 3, 2026-11-15: the TPS61023 passes VREG through when VREG > its 4.91 V (SLVSF14B 7.3.5): on a 5.4 ..
    #  5.5 V charger V5A reached the clamp's 5.11 V low tolerance and nothing limited U19's current. R145 10 ohm in
    #  series with U19's cathode, the divider on the cathode: a soft clamp, V5A = 5.0 V + 10.2 ohm x I; the threshold
    #  down to 5.0 V (R138 / R139 20 k / 20 k, 4.95 .. 5.05 V) so the worst double fault stays under the WS2812's 5.3 V.
    #  sim/clamp.py: U19 <= 165 mW in pass-through at VREG 5.3 V (its 300), V5A <= 5.17 V with +12 V into CLOCK and
    #  RESET, 7 mA through it with the boost at its +2.5 % corner (35 mW, from the rail))
    ("R145", "R", "10", "r0402", "C25077", {"1": "V5A", "2": "V5_CK"}),
    ("R138", "R", "20k", "r0402", "C25765", {"1": "V5_CK", "2": "V5_CL"}),   # (chain list 3: 20 k / 20 k, the same 5.0 V)
    ("R139", "R", "20k", "r0402", "C25765", {"1": "V5_CL", "2": "GND"}),
    ("JP9", "sj2", "CV 5V link", "sj2c", "", {"1": "V5A", "2": "V5C"}),
]
# ---- CLOCK / RESET outputs for Eurorack, from the 5 V aux rail through JP9 (V5C; 2026-11-15, the user's, in place of
#      the charge pump). Buffer try (2026-11-16, the user's rulings): per output the GPIO (100 k pull-down: 0 V out while
#      the GPIO floats at boot and reset) drives two inverters in series of U21 (74AHCT04, VCC on V5C = a non-inverting
#      buffer, ~4.4 V into 10 k, ~4.8 V into 100 k, sim cv_buf.py) onto the node N, a BAV99 clamping N to GND and V5C,
#      one 1 k 1206 (0.25 W) to the jack's tip, 2.2 nF C0G on the jack side. Modular signals are +-10 V (not 12 / 15): a
#      +10 V mispatch into a LOW output 9.5 mA, 90 mW in the 1 k; into a HIGH output (+10 V) 4.5 mA, and -10 V into a
#      HIGH output 13.8 mA, 191 mW (76 % of 0.25 W; the output stage sources it, 8 mA recommended, 25 mA absolute max).
#      Both outputs at +10 V, high: V5A 5.09 V (clamp 5.0), 5.41 V at the clamp's high corner (like the old circuit).
#      Sleeves (2026-11-15, the ground study): to GND through R120 / R121 100 ohm 0402 (their twins RT120 / RT121: a 0
#      ohm there grounds the sleeve solid). Jacks: PJ-320A (tip = contact D), bottom side at the top edge, user-soldered.
def _cv(n, io, d, r):
    # buffer try (the user's ruling 2026-11-16): see the CLOCK / RESET block above; U21 itself is in PARTS += below
    return [
        (f"R{r + 1}", "R", "100k", "r0402", "C25741", {"1": io, "2": "GND"}),
        (f"C{89 if n == 'CLK' else 90}", "C", "2.2n C0G", "c0603", "C2991172", {"1": f"{n}_OUT", "2": "GND"}),
        (d, "bav99", "BAV99", "tvs2", "C2500", {"1": "GND", "3": f"{n}_N", "2": "V5C"}),
        (f"R{r + 5}", "R", "1k", "r1206", "C4410", {"1": f"{n}_N", "2": f"{n}_OUT"}),
    ]


PARTS += AUX5 + _cv("CLK", "CLK_IO", "D10", 70) + _cv("RST", "RST_IO", "D11", 76) + [
    ("U21", "inv6", "74AHCT04", "tssop14", "C8190",
     {"1": "CLK_IO", "2": "CLK_B", "3": "CLK_B", "4": "CLK_N", "5": "RST_IO", "6": "RST_B", "7": "GND", "8": "RST_N",
      "9": "RST_B", "10": "HOST_M", "11": "HOST_EN", "12": "HOST5G", "13": "HOST_M", "14": "V5C"}),
    ("C105", "C", "100n", "c0402", "C1525", {"1": "V5C", "2": "GND"})]
PARTS += [("J13", "jack", "CLOCK", "jack", "C18185602", {"D": "CLK_OUT", "A": "CLK_S"}),
          ("J14", "jack", "RESET", "jack", "C18185602", {"D": "RST_OUT", "A": "RST_S"}),
          ("R120", "R", "100", "r0402", "C25076", {"1": "CLK_S", "2": "GND"}),   # (experiment A: 100 ohm 0402, was 47 0603)
          ("R121", "R", "100", "r0402", "C25076", {"1": "RST_S", "2": "GND"})]
DNP |= {"J13", "J14"}

# what each LCSC code is (README part list; jlc_parts.py adds stock, price and Basic / Extended as read on a day)
LCSC = {
    "C173752": "JST S2B-PH-K-S(LF)(SN), PH2.0 2-pin socket, SIDE entry, through hole (DNP: the user fits it from the bottom; buffer try)",
    "C38838": "Nexperia PESD15VL2BT,215, double bidirectional ESD diode, VRWM 15 V, SOT-23 (LINE IN: D7, D14; buffer try)",
    "C25800": "910 k 1% 0402 (buffer try: R94, the TUSB320's VBUS_DET resistor: TI asks 855 .. 920 k)",
    "C8190": "TI SN74AHCT04PWR, hex inverter, 4.5 .. 5.5 V, 8 mA, TSSOP-14 (buffer try: U21, the CLOCK / RESET drivers)",
    "C17957": "220 ohm 1% 1206, 0.25 W (the MIDI sources and sinks)", "C4410": "1 k 1% 1206, 0.25 W (Basic; BUFFER TRY: the CLOCK / RESET series)",
    "C7420376": "Hongjiacheng SRV05-4, 4-line + rail ESD array, 5 V, 1 pF, SOT-23-6 (preferred; its Rev 2.2: 5 the rail, 2 GND, I/O 1 / 3 / 4 / 6)",
    "C3013944": "Espressif ESP32-S3-WROOM-1U-N8R2, module: 8 MB quad flash, 2 MB quad PSRAM, u.FL, 18 x 19.2 mm",
    "C3013940": "Espressif ESP32-S3FH4R2, QFN56 7 x 7, 4 MB quad flash + 2 MB quad PSRAM in the package (EPAD 5.7 mm)",
    "C23164": "4.7 ohm 1% 0603 (Basic; R98, the RGB LEDs' supply filter)",
    "C254377": "HELE X3S040000B81H-R, 40 MHz crystal 3225, 8 pF load, +-10 ppm, +-10 ppm over -20..75 C",
    "C85364": "ProTek SRV05-4-P-T7, 4-line ESD array + REF, SOT-23-6 (preferred: no fee; the USB ESD since stage 1e)",
    "C7519": "ST USBLC6-2SC6, USB 2.0 ESD protection, 2 lines + VBUS, SOT-23-6",
    "C88374": "Hirose U.FL-R-SMT-1(80), u.FL receptacle (fitted: the default antenna path, the user's ruling 2026-11-11)",
    "C2939346": "HOOYA DIN-504B-M10, 5-pin DIN socket, female, right angle, through hole (DNP: hand-soldered, bottom side)",
    "C76769": "TDK MLG1005S2N0BT000, 2.0 nH 900 mA 0402 (the VDD3P3 filter)",
    "C87532": "TDK MLG1005S3N3BT000, 3.3 nH 800 mA 0402 (the RF match)",
    "C1550": "FH 0402CG1R0C500NT, 1 pF C0G 0402 (the RF match; a preferred Extended part: no loading fee)",
    "C2765186": "SHOU HAN TYPE-C 16PIN 2MD(073), USB 2.0 receptacle",
    # (the jacks: the PJ-320D's through-hole twin, same body, the pins at the same places; pads A = sleeve, C = ring,
    #  D = tip, B = the switch; the user's ruling 2026-11-12: stronger, JLC solders it)
    "C18185602": "SHOU HAN PJ-320A-4P DIP, 3.5 mm jack, through hole, 6.2 mm wide (stage 2: the CLOCK / RESET outputs; DNP: "
                 "bottom side, the user solders it; bought with the order)",
    "C8545": "2N7002, N-MOSFET 60 V 115 mA, SOT-23 (Basic)", "C20917": "AOS AO3400A, N-MOSFET 30 V 5.7 A, 48 mohm at 2.5 V, SOT-23 (Basic; the MIDI sinks)",
    "C15850": "10 uF 25 V X5R 0805 (Basic; the CV rail)",
    "C2500": "Nexperia BAV99, two switching diodes in series 100 V 215 mA, SOT-23 (Basic)",
    "C17944": "2 k 1% 1206, 0.25 W (Basic)",
    "C55159907": "CAX CAX-MTS102-YB-10.5JL, MTS-102 toggle switch, SPDT ON-ON, panel mount, M6 x 0.75 bushing, 2 A 250 VAC "
                 "/ 5 A 120 VAC (its drawing; no DC figure); in the lid's wall, wired to J11 (battery order; bought with the order)",
    "C21189": "0 ohm 0603 (stage 2: R63, the ferrite bead's place in VBUS)", "C16772": "220 nF 16 V X7R 0402",
    "C17802": "68 ohm 1% 0805, 0.125 W (preferred: no fee; stage 2: the MIDI sinks)",
    "C16684": "BOOMELE PJ-307C, 3.5 mm stereo (TRS) jack with two switch contacts, through hole, right angle (DNP: "
              "bottom side, the user solders it; bought with the order: cost.md)",
    "C318884": "XKB TS-1187A-B-A-B, tactile switch 5.1 x 5.1 x 1.5 mm",
    "C2286": "KT-0603R, red LED 0603",
    "C23186": "5.1 k 1% 0603", "C25092": "22 ohm 5% 0402",
    "C22859": "10 ohm 1% 0603", "C17168": "0 ohm 0402", "C25744": "10 k 1% 0402",
    "C59461": "22 uF 6.3 V X5R 0603", "C19702": "10 uF 10 V X5R 0603",
    "C1525": "100 nF 16 V X7R 0402", "C25271": "1 ohm 1% 0805 (Basic; R155 // R156, the VREG bulk's series 0.5 ohm)", "C15525": "10 uF 6.3 V X5R 0402 (Basic; C104, U18's input)", "C12530": "2.2 uF 6.3 V X5R 0402", "C15195": "10 nF 50 V X7R 0402",
    "C52923": "1 uF 25 V X5R 0402", "C32949": "10 pF 50 V C0G 0402",
    # mini 2, LINE IN
    "C7972": "TI LMV321IDBVR, op-amp, rail-to-rail output, 1 MHz, 130 uA, 2.7..5.5 V, SOT-23-5 (a preferred Extended part)",
    "C5446": "Torex XC6206P332MR, 3.3 V LDO, 1 uA, 200 mA, SOT-23",
    "C19077524": "Hongjiacheng SMAJ5.0CA, 400 W bidirectional TVS, VRWM 5 V, SMA (preferred: no fee; the LINE IN ESD since stage 1e)",
    "C2691448": "XFCN PZ254V-11-04P, 1 x 4 2.54 mm header (DNP: the screen's, fitted by the user)",
    "C25765": "20 k 1% 0402", "C25908": "5.6 k 1% 0402", "C25755": "150 k 1% 0402", "C25750": "120 k 1% 0402",
    "C25079": "120 ohm 1% 0402", "C1531": "2.2 nF 50 V X7R 0402",
    # mini 2.2 simplified
    "C11702": "1 k 1% 0402",
    "C435835": "ST LDL1117S33R, 3.3 V LDO, 1.2 A, dropout 350 mV typ at 1.2 A, 250 uA, 18 V max, SOT-223 (the AMS1117's "
               "pinout; both orders since review pass 1)",
    "C23654": "TI SN74LVC1G125DBVR, single buffer, 3-state (OE low = on), 24 mA at 3 V, SOT-23-5 (the charge pump's driver)",
    "C2868428": "TI TLV75733PDRVR, 3.3 V LDO, 1 A, +-1.5 %, dropout 475 mV max at 1 A, 25 uA, COUT 1 .. 200 uF, WSON-6 2 x 2 "
                "(both orders since review pass 2)",
    "C8678": "SS34, Schottky 40 V 3 A, SMA (Basic; D9 since review pass 2)",
    "C7171": "Kyocera AVX TAJA106K016RNJ, 10 uF 16 V tantalum, 3 ohm ESR, case A 3216 (Basic; VBUS's damping, C27 / C91)",
    "C22790": "12 k 1% 0603 (Basic)", "C25768": "22 k 1% 0402 (Basic)", "C45783": "22 uF 25 V X5R 0805 (Basic; C3, U2's input)", "C25779": "33 k 1% 0402 (Basic)",
    "C2869734": "TI LM66100DCKR, ideal diode, 1.5 A, 79 .. 91 mohm, 1.5 .. 5.5 V, SC-70-6 (the cell -> VSYS path)",
    "C16133": "Kyocera AVX TAJB107K006RNJ, 100 uF 6.3 V tantalum, case B 3528 (Basic; +3V3 bulk, battery order)",
    "C1538": "4.7 nF 50 V X7R 0402 (Basic)", "C25086": "1 ohm 1% 0402 (preferred: no fee)",
    "C25076": "100 ohm 1% 0402 (the XB5353A's VDD resistor)",
    "C32574": "TP4054-42-SOT25R, 1-cell Li-ion charger 4.2 V, up to 500 mA, SOT-23-5 (a preferred Extended part)",
    "C15127": "AOS AO3401A, P-MOSFET 30 V 4 A, SOT-23 (the power path)",
    "C80628": "Xysemi XB5353A, 1-cell Li-ion protection with its own MOSFET, SOT-23-5",
    "C131337": "JST B2B-PH-K-S, PH2.0 2-pin socket, through hole (DNP: the user fits it from the bottom)",
    "C8598": "CJ B5819W SL, Schottky 40 V 1 A, SOD-123", "C26083": "1 M 1% 0402", "C25741": "100 k 1% 0402",
    "C2297": "KT-0805G, emerald green LED 0805, Vf 2.6..3.1 V at 5 mA (Basic)",
    "C17901": "100 ohm 1% 1206, 0.25 W (Basic)",
    "C16106": "Holtek HT7550-1, 5.0 V +-3 % LDO, 30 V in, 100 mA, 4 uA, SOT-89 (preferred; the USB host signal)",
    "C2836598": "TI TUSB320LIRWBR, USB Type-C CC logic, DRP, I2C, Rd while unpowered, X2QFN-12 1.6 x 1.6",
    "C37704": "Nexperia BAT54C,215, dual Schottky 30 V, common cathode, SOT-23 (preferred; one half: D16)",
    "C52917434": "WorldSemi WS2812B-2020-V6, addressable RGB LED, 3.3 .. 5.3 V, 2.0 x 2.0 mm",
    "C25105": "33 ohm 1% 0402 (Basic)", "C25879": "2.2 k 1% 0402 (Basic)",
    "C15008": "100 uF 6.3 V X5R 1206 (Basic; C72 / C92, VREG's bulk behind R155)",
    "C2845237": "TI TPS63802DLAR, 2 A buck-boost, VIN 1.3 .. 5.5 V, 11 uA, VSON-10 2 x 3 (+3V3)",
    "C919459": "TI TPS61023DRLR, 3.7 A synchronous boost, VIN 0.5 .. 5.5 V, PFM, true disconnect, SOT-563 (the 5 V aux rail)",
    "C668312": "Murata DFE201612E-R47M, 0.47 uH 5.5 A, 2016 (L4 / L5: TI's list for the TPS63802)",
    "C3113": "CJ CJ431, shunt reference 2.5 V +-1 %, 150 mA, 300 mW, SOT-23 (Basic; the 5 V aux rail's clamp)",
    "C17798": "680 ohm 1% 0805 (Basic)", "C25890": "3.3 k 1% 0402 (Basic)", "C2991172": "2.2 nF 50 V C0G 5% 0603 (LINE IN's matched poles)", "C25752": "12 k 1% 0402 (Basic)", "C17710": "470 ohm 1% 0805 (Basic)", "C17557": "220 ohm 1% 0805 (Basic)",
    "C23182": "47 ohm 5% 0603 (Basic)", "C25762": "18 k 1% 0402", "C25794": "51 k 1% 0402 (Basic)", "C25750": "120 k 1% 0402", "C25796": "56 k 1% 0402",
    "C25790": "470 k 1% 0402", "C25792": "47 k 1% 0402",
}


# ---- stage 2 (the user's "pads that could save the batch"): spare lands, DNP (no part, no paste, no cost)
SPARES = [
    ("J12", "pads6", "RECOVERY", "pads6", "", {"1": "U0TXD", "2": "U0RXD", "3": "+3V3", "4": "GND", "5": "CHIP_PU", "6": "BOOT"}),
    # (review pass 2, the user's Q2: no bench test, so inside the datasheets: the unplug ride-through as two 100 uF
    #  X5R on VSYS (C72, C92: the LDO's input side) + one 100 uF tantalum on +3V3 (C86: inside the TLV757P's 200 uF).
    #  Battery order only. Sim, the worst corner, a 3.6 V cell, U15's tON 40 / 90 us: +3V3 3.09 / 3.02 V)
    # (sign-off stage 2, 2026-11-17, R1: on VREG, after the power switch, and fitted in BOTH orders. On VSYS they
    #  dumped 80 uF through the switch's wires into VREG's 2 x 10 uF at every switch-on: VREG rang to 5.2 .. 7.9 V
    #  (the TPS63802's VIN abs max 6 V); the usb order's hot plug reached 5.93 / 6.23 V. On VREG: <= 5.59 V
    #  (worktrees/mini22-review-scratch/REVIEW.md). Cost: ~1000 uC more USB inrush in the usb order (accepted, C4))
    # (the same day, mots's pick for the usb order's inrush: R155 1 ohm in series with the bulk only (VREG -> VREG_B).
    #  Sim: VREG <= 5.16 V at switch-on and hot plug (bulk 3 mohm: 5.40), the host port's own rail dips 0.94 V (1.63);
    #  R155 carries 4.9 A peak, 1.0 mJ per plug (0.62 mJ at switch-on from a cell): an 0805, not the 0402 (no pulse
    #  curve in Uniroyal's sheet; adiabatic estimate ~10 K in an 0805's film vs ~65 K in an 0402's))
    ("C72", "C", "100u", "c1206", "C15008", {"1": "VREG_B", "2": "GND"}),
    ("C92", "C", "100u", "c1206", "C15008", {"1": "VREG_B", "2": "GND"}),
    ("R155", "R", "1", "r0805", "C25271", {"1": "VREG", "2": "VREG_B"}),
    # (signoff3, mots's "R155 2x1": R156, the same 1 ohm 0805, in parallel: 0.5 ohm. Round 3's stiff corner (5.5 V, 1 uH,
    #  50 mohm cable, derated ceramics) rang VREG to 5.77 V with 1 ohm (5.85 V with C3 22 uF); 0.5 ohm damps it to ~5.25 V.
    #  No Basic sub-ohm part at JLC; each resistor takes half the pulse)
    ("R156", "R", "1", "r0805", "C25271", {"1": "VREG", "2": "VREG_B"}),
    # (J10B: the 2.42" module's lead (its header is down its left side, over J5's DIN), on the screen's four nets;
    #  review pass 2: dropped, then kept by the user's call (pads only, no part, no cost: through hole). J10 takes the
    #  1.54"; the 1.3" is dropped (the user's, 2026-11-15))
    ("J10B", "hdr4", "OLED 2.42 LEAD", "hdr4", "", {"1": "OLED_P1", "2": "OLED_P2", "3": "SCL", "4": "SDA"}),
    ("C80", "C", "DNP", "c0603", "", {"1": "BTN_UP", "2": "GND"}),          # (debounce caps by the buttons)
    ("C81", "C", "DNP", "c0603", "", {"1": "BTN_DOWN", "2": "GND"}),
    ("C82", "C", "DNP", "c0603", "", {"1": "BOOT", "2": "GND"}),
    ("C83", "C", "DNP", "c0603", "", {"1": "BTN_YES", "2": "GND"}),
]
PARTS += SPARES
DNP |= {p[0] for p in SPARES if p[0] not in ("C72", "C92", "R155", "R156")} | {"R144", "U2B", "R126", "R127", "R128", "R129", "R123"}

# ---- the twins (stage 2, the user's standing rule, as hardware/sd-board's in the optimist repo): every resistor whose
#      value may need changing after the boards arrive gets a bare hand-solder pad pair in parallel (1.4 x 2.0 mm pads
#      1.2 mm apart since stage 2's review: an 0603 .. 1206 or a leaded part; no part, no paste, out of
#      the BOM and the CPL, joined to its resistor by two short top tracks drawn in make_board.py: no via of its own), so a
#      batch is fixed with an iron, not a re-order. A part soldered on a twin LOWERS the value (R // R_twin); README
#      "Twins" lists what each does. (RT40 / RT41, the old LEDs', went with them 2026-11-15.) Not twinned: the jacks' 3 x 100 ohm
#      sources (three pads already: swap one), the fixed parts (pull-downs of FET gates, the XB5353A's 100 ohm, the CC
#      resistors, 0 ohm links, the USB 22 ohm)
TWINS = {   # twin: (its resistor, the reason)
    "RT15": ("R15", "MIDI OUT jack sink"), "RT55": ("R55", "MIDI OUT DIN sink"),
    "RT20": ("R20", "MIDI OUT 2 jack sink"), "RT57": ("R57", "MIDI OUT 2 DIN sink"),
    # (2026-11-15: RT30 / RT31 / RT32 gone with the op-amp: the digital LINE IN's two networks must match; a hand part
    #  on one side only would spoil the rejection)
    "RT33": ("R33", "LINE IN bias, upper"), "RT34": ("R34", "LINE IN bias, lower"),
    "RT36": ("R36", "LINE IN tip pull-down"), "RT37": ("R37", "LINE IN ring pull-down"), "RT38": ("R38", "plug detect pull-up"),
    "RT51": ("R51", "charge current (RPROG)"), "RT10": ("R10", "cell divider, upper"), "RT11": ("R11", "cell divider, lower"),
    "RT60": ("R60", "I2C SCL pull-up"), "RT61": ("R61", "I2C SDA pull-up"),
    "RT39": ("R39", "LINE IN sleeve to GND: a 0 ohm here grounds it solid"),
    "RT120": ("R120", "CLOCK sleeve to GND: a 0 ohm here grounds it solid"),
    "RT121": ("R121", "RESET sleeve to GND: a 0 ohm here grounds it solid"),
    "RT93": ("R93", "USB host signal's current limit (host mode, battery order)"),
    # (the outputs' 1 k series R75 / R81 1206: not twinned)
    # (stage 2, spare lands as twins of capacitors: the crystal's load, the LINE IN filter, CHIP_PU's delay, the
    #  regulators' input / output, +3V3 by the chip)
    # (review pass 1: CT3, C3's twin, gone: C3 sits at the LDO's pin with no room beside it, and C72 / C92, the 2 x 100 uF bulk (VREG since sign-off stage 2), are
    #  fitted now)
    # (72 x 72: these three on the BOTTOM under their parts, make_board.BOTTOM_TWINS: on top CT9 / CT10 closed the way
    #  round the crystal, CT8 had no room in the fan-out lanes)
    "CT8": ("C8", "the cell level's ADC cap (TIMING.md: the shared ADC pattern)"),
    # (did not fit beside their parts, README "spare pads": CT7, CT17, CT5 -> J12's pads 3-4 (+3V3, GND) and 4-5 (GND,
    #  CHIP_PU) take an 0805 across them; CT32 -> RT32's land (C32 sits across R32: the same two nets) takes a capacitor;
    #  RT35, CT36 left out: R35 / C36 sit in the chip's escape area, no room within 8 mm, a longer stub on the ADC pin
    #  would pick up noise)
}
_main = {p[0]: p for p in PARTS}
PARTS += [(t, _main[m][1], "twin DNP", "twin_c" if t.startswith("CT") else "twin", "", dict(_main[m][5]))
          for t, (m, _why) in TWINS.items()]
DNP |= set(TWINS)
DNP |= {"JP3", "JP4", "JP5", "JP6", "JP7", "JP8", "JP9", "JP10", "JP11", "JP12", "JP13"}   # (buffer try: closed solder jumpers, copper only)

# the two orders (stage 1d, the user's 2026-11-14): one board; "usb" = USB power only, "battery" = the cell circuit
# fitted. A part named here is fitted in that order only; every other part (not DNP) in both. make_assembly.py writes one
# BOM / CPL pair per order (fab/usb/, fab/battery/), cost.py prices both.
VARIANTS = ("usb", "battery")
HOST = ("U17", "C95", "R94", "R92", "R93", "D16", "Q9")
ONLY = {"R62": "usb",                                       # (review pass 1: U2, the LDL1117, in both orders)
        "R63": "usb", "R3": "usb", "R4": "usb",              # (host mode: Q9 and U17 take their places in "battery")
        **{r: "battery" for r in ("Q1", "R50", "D9", "U8", "R51", "C40", "C41", "R86", "U9", "R52", "C42", "Q2",
                                  "R10")},   # (sign-off stage 2: C72 / C92, VREG's bulk, now in both orders)
        **{r: "battery" for r in HOST}}                     # (USB host mode: the battery order only, the user's)


def fitted(ref, variant=None):
    """on the board as a part (not pads only); with a variant: fitted in that order"""
    if ref in DNP:
        return False
    return variant is None or ONLY.get(ref, variant) == variant


def nets():
    """{net: [ref.pad, ...]}"""
    out = {}
    for ref, _s, _v, _f, _c, pins in PARTS:
        for pad, net in pins.items():
            out.setdefault(net, []).append(f"{ref}.{pad}")
    return out
