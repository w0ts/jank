# jank mini 2.2: one board, two orders (USB only, or with a 1S cell), two MIDI OUTs, LINE IN, BOOT, RST, one LED window (branch hw/mini2.2)

**Stage 1d (2026-11-14): placed, not routed; nothing ordered, built or measured.** `mini2.kicad_pcb` is the *placed*
board (`LANE_VIAS=1 NO_STITCH=1 PLACE_ONLY=1 make_board.py`): `fab/mini2-gerbers.zip`, `render-top.png`,
`render-bottom.png`, `render-3d.png`, `mockup.stl` and `case/mini_case.scad` are still mini 2's until stage 2. Stage 1d
(next section) supersedes stage 1c where they differ; "mini 2.2 (stage 1)" and "1b" are superseded (the encoder and the
RGB LEDs are gone); everything after them is mini 2's text.

## mini 2.2, stage 1d (the user's answers to 1c, and the review's fixes)

**One board, two orders** (`parts.VARIANTS`, `parts.ONLY`): `make_assembly.py` writes `fab/usb/{bom,cpl}.csv` and
`fab/battery/{bom,cpl}.csv` (and each order's `summary.json`); `cost.py` prices both in `fab/cost.md`.

| Fitted in | Parts |
|---|---|
| both | everything else, D3 (VBUS -> VSYS), C3 / C5 / C6, R11 + C8 (GPIO1 held at 0 V in "usb": not floating, and the firmware can tell the orders apart) |
| **usb** only | U2 **AMS1117-3.3** (C6186, Basic, SOT-223) |
| **battery** only | U10 **HT7833** (C164106), Q1 + R50 (power path), U8 + R51 + C40 + C41 (charger), U9 + R52 + C42 (protection), Q2 + R53 (reverse polarity), R10 (the divider's top) |
| never | J8 / J9 (the PH2.0 sockets: the user fits them), J5 / J6 (DINs), the twins, the RF DNP pads |

The two LDOs are alternative footprints on the same VSYS / +3V3 (U2 left of U10). Checked in "usb" with the battery
parts missing: VSYS = D3 + C3 + U2 + U7 + C34 (complete); VBUS = U5 + D3 (+ nothing of the cell); GPIO1 = R11 + C8 to
GND; Q1's and Q2's gates are on nets that carry only battery parts (VBUS-R50, Q2_G-R53), so no fitted gate floats;
BAT_P / BAT_C / BAT_N / XB_VDD / PROG are copper only. In "battery": U2 out, U10 in; every gate has its resistor.

**MIDI OUT 2 has its own loop** (the review's fix): J3 / J6 on MO2_R / MO2_T, R21 // R27 // R49 (3 x 100 ohm 1206,
C17901, as OUT 1) from +3V3 and R20 10 ohm (C22859, as R15) from GPIO21; no new part type. A short or a mono plug on
one output no longer stops the other, and each receiver gets the full ~5-7 mA (GPIO21 sinks both: ~2 x 6 mA).

**The XB5353A as its datasheet's application circuit:** VDD (pin 3) through R52 100 ohm (C25076, Basic 0402) from the
cell's + (net XB_VDD), C42 100 nF from XB_VDD to the IC's GND (BAT_N) right at the IC; VT (pin 1) open.

**Reverse polarity:** Q2 AO3401A in the cell's + lead (drain on the sockets' + BAT_C, source on BAT_P, gate to GND
through R53 100 k: a cell the right way round turns it on through its body diode, a reversed cell finds it off). Both
types are already on the board (no fee).

**The sockets' polarity: pin 1 (square pad) = +, pin 2 = -**, printed "+" / "-" on both sides. The reasoning:
- The user's Wemos LOLIN32 Lite has the opposite polarity to his whoop cells.
- The LOLIN32 Lite's schematic ("LOLIN32_Lite", rev V1.0.0, 2017-08-30, EAGLE; the PDF Zephyr's board page links,
  done.land/assets/files/lolin32_lite.pdf, read 2026-11-14): its battery socket **P1 has pin 1 on GND and pin 2 on
  VBAT** (the TP4054's BAT).
- So a whoop lead puts its + on pin 1: mini 2.2's J8 / J9 have pin 1 = +, which is also the common SparkFun / Adafruit
  convention.
- *Assumed, not verified:* that LOLIN's footprint numbers the socket as JST does (pin 1 at the same physical end), and
  that the user's board is that revision. JLC's PH2.0 footprint (C131337) has its pad 1 at the end make_footprints.py
  now makes square.
- **Check one cell with a meter before the first plug-in.** If the polarity is wrong, **solder the socket turned 180
  degrees: that swaps + and -.** Both sockets keep room for that: their underside boxes either way round touch nothing
  (`case/renders/sketch-2.2.png`). Q2 stays as the safety net either way.

**The cells on edge under the board, beside the fitted DINs** (the user: "I would put the battery on their side"),
`case/renders/sketch-2.2.png`:
- The sockets moved out of the cell band to the middle (J8 at x 36.5, J9 at x 43.0, y 33, between the DIN bodies; their
  bodies y 30..36 either way round). The band is then **y 39.0 (the DIN bodies' end) .. 58.1 (the ground edge) = 19.1 mm**
  across the full width.
- Two cells standing on edge: **BT2.0 450 mAh (62 x 15.5 x 6.7): 2 x 6.7 = 13.4 mm, 5.7 mm spare; GNB 550 mAh (65..66 x
  18 x 6..6.2): 12.4 mm, 6.7 mm spare.** No cell in the antenna strip.
- Length against the case's inside width (66.1 + 2 x 0.4 = 66.9 mm): BT2.0 62 leaves 4.9 mm, GNB 66 only 0.9 mm (its
  lead leaves one end: a 66 mm cell may not fit with its lead bent; 65 mm leaves 1.9).
- Depth: the DIN base's floor is 19.95 mm under the board: 18 mm cells leave 1.65 mm, 15.5 mm cells 4.15 mm. Today's
  no-DIN base (4.3 mm) needs the DIN base's depth.
- Leads: from a cell's end (x ~0.5 or ~65.5) to the sockets (x 35.5 .. 44) is ~25 .. 45 mm of lead: *the cells' lead
  lengths are not known here*.
- **BT2.0 is BetaFPV's own connector, not PH2.0**: a BT2.0 cell does not mate with these sockets (an adapter lead, or
  the sockets swapped for BT2.0 ones).

**Battery level, firmware (no part):** the LED window blinks to show the cell's level: the emptier, the faster.

**Checks (placed board, 2026-11-14):** ERC 0 errors (1 warning, ANT_END); `check_nets.py` 306 pads, 0 problems;
courtyards as boxes, top and bottom, J8 / J9 counted either way round: no new overlap (mini 2's C34 / U7 0.04 and
R18 / R19 0.07 mm touches remain); no top part in the DIN pads' iron room; DRC: unrouted connections and the DNP parts'
library notes only; both CPLs within 0.01 mm of JLC's footprints. `render-top-placement.png` shows the parts of both
orders.

**Cost (cost.py's method, `fab/cost.md`; snapshot 2026-11-13, newer parts read 2026-11-14; shipping left out):**

| | usb, 5 boards | usb, 10 boards | battery, 5 boards | battery, 10 boards |
|---|---|---|---|---|
| parts / lines / joints a board | 73 / 39 / 239 | | 85 / 44 / 272 | |
| Extended types | 10 | | 12 (+ HT7833, XB5353A) | |
| **total** | **$76.50** | **$102.03** | **$86.63** | **$115.31** |
| per board | $15.30 | $10.20 | $17.33 | $11.53 |

(Stage 1c: $101.07 / $114.00 for 10; +$0.96 / +$1.31 for MIDI OUT 2's loop, R11 + C8 in "usb", R52, Q2 + R53.)
Through-hole: the 3 jacks, by JLC by hand (+ $5.34 for 10: 120 joints x $0.0164 + $3.58) or by the user ($1.66 of
jacks bought separately): `fab/cost.md`, per order.

### To evaluate (not applied)

**Antenna: the u.FL only, no chip antenna (AE1)?**
- **Saves** $3.09 (one Extended type) + AE1 $2.27 for 10 boards (12 x $0.189) = **$5.36 an order**. An external antenna
  costs about as much: JLC's parts search (2026-11-14) lists HJ-2.4G-IPEX1-L=80mm (C1693673) at $0.47 (10+), 401 in
  stock, and KH-FPC2.4G-1.13IPEX1-50mm-F022 (C56127484) at $0.33 / $0.26 (50+), 50 in stock; LCSC sells FPC 2.4 GHz
  antennas on a 50 mm IPEX lead at ~$0.44 (3 dBi listed) and MyAntenna AICF002 (C22467619, adhesive, 4.2..4.4 dBi
  listed). 10 antennas: ~$2.6 .. 4.7: **net ~$0.7 .. 2.8 an order.**
- **Gain:** the chip antenna's datasheet gives 5.05 dBi peak, 70.7 % efficiency, **on YAGEO's evaluation board**
  (fcc.report / components.ru copies, 2013 v0); FPC antennas list 2 .. 4.4 dBi. On paper the chip antenna is not worse.
  On this board it sits right beside the cells' metal (the band ends at the ground edge): *unmeasured*, it may detune.
  An FPC antenna stuck inside the lid is away from the cells and the board's ground.
- **Matching:** the chip side stays (C23 1 pF, L3 3.3 nH, C24 DNP: the S3's own match); the antenna side goes (AE1, the
  DNP pi C25 / C26 / R18); R19 can stay or become a track. Untuned either way (no VNA).
- **The strip can go:** the board 10 mm shorter (66 x 58); J4 at the new bottom edge; the cell band (y 39 .. 58) is
  unchanged. **The board's price does not change** (JLC's 2-layer price is flat up to 100 x 100 mm); the case is 10 mm
  shorter.
- **Case:** an FPC antenna sticks inside the lid (mini 2's case already has a seat for one over the strip; it would
  move): no cable exit, the 50..80 mm lead stays inside. A stick antenna would need an RP-SMA bulkhead and a pigtail
  (more parts, a hole).

**More cost-down (Extended types that are left; JLC's parts search with the Basic filter, 2026-11-14):**

| Part | Basic / preferred alternative | Saves (10 boards) | Risk |
|---|---|---|---|
| 40 MHz crystal (C254377) | none: JLC's Basic crystals are 8, 12, 16, 25 MHz and 32.768 kHz; the S3 needs 40 MHz | - | keep |
| USBLC6-2SC6 (C7519) | **SRV05-4-P-T7 (C85364), Extended *preferred* (no fee)**, SOT-23-6, 4 lines + VDD; $0.28 against $0.16 | ~$3.09 - $1.4 = **~$1.7** | its pin functions match USBLC6's (1 / 3 / 4 / 6 the lines, 2 GND, 5 VDD) per JLC's listing: *the pinout and its footprint unverified*; 3.5 pF a line, fine at full speed |
| USB-C (C2765186) | none Basic or preferred found | - | keep |
| PJ-320A jacks (C18185602) | none Basic or preferred found | - | keep |
| PESD5V0S2BT (C49338) | kept (the user: must-have) | - | - |
| HT7833 (battery) | none Basic that runs from a cell | - | only in the battery order |
| XB5353A (battery) | DW01A + FS8205A: both Extended too (two types) | none | keep |
| PCB size | flat price to 100 x 100 mm (2 layers); a shorter board saves no PCB money | 0 | - |
| PCB thickness | 0.8 .. 1.6 mm cost the same at JLC; **the RF line's 1.2 mm width is for 1.6 mm** (README "RF") | 0 | don't |
| 5 or 10 boards | the order's fixed part (setup $8.24, stencil $1.55, nitrogen $0.91, the types: usb 10 x $3.09, battery 12) is ~46 % of a 10-board order: 10 boards cost a board $10.20 / $11.53 against $15.30 / $17.33 for 5 | - | - |

## mini 2.2 simplified (stage 1c, the user's decisions of 2026-11-14)

"We must drive cost down and simplify a lot. Everything we add is a risk for a bug." What the board is now:

- **Power: no MIDI power input.** Gone: the 2 x 2200 uF reservoir, the TPS62842 buck and its inductor, both BL8506
  detectors, the power-in resistors and diodes, the BUCK_EN / PWR_HOLD logic, the VBUS sense, R5 / R23. **VBUS -> D3
  (B5819W) -> VSYS -> U2 HT7833 (3.3 V, 500 mA, 4 uA) -> +3V3.** CHIP_PU keeps Espressif's 10 k / 1 uF RC (no
  supervisor: +3V3 now rises with the LDO).
- **The cell (fitted by JLC, the sockets by the user):** U8 TP4054-42 charges from VBUS (R51 5.1 k: ~196 mA); Q1 AO3401A
  connects the cell to VSYS while VBUS is absent (gate on VBUS, R50 100 k down; with USB, VSYS ~4.7 V keeps it off); U9
  XB5353A protects the cell in its minus lead (its GND = the cell's minus BAT_N, VM = the board's GND; pin 1 VT open,
  *its datasheet not read*). **Two PH2.0 sockets J8 / J9 (B2B-PH-K-S, C131337) in parallel, not fitted**: the user
  inserts them from the bottom (the cells under the board) and solders them on top; "+" and "-" on both silkscreens.
  Pin 1 = -, pin 2 = + is **UNVERIFIED** against whoop PH2.0 leads: check a lead with a meter first. (Two cells in
  parallel: the user's choice, the risk accepted.) U7 (the LINE IN's LDO) now runs from VSYS: the LINE IN works on
  the cell too.
- **Battery level:** hardware is only R10 / R11 1 M / 1 M + C8 100 nF from the cell's + to GPIO1 (VBAT_ADC, ADC1_CH0,
  ~2 uA). No extra LED or part: the LED window shows it in firmware (green / yellow / red at power-on and on a button
  hold, a low-battery blink); **the LED blinks to show the level, the emptier the cell, the faster it blinks.**
- **MIDI OUT 2 = a passive copy of MIDI OUT 1:** J3 (jack) and J6 (DIN, DNP) on the same nets as J2 / J5: the ring on
  MO_R (+3V3 through R14 // R26 // R48, 3 x 100 ohm 1206 = 33.3 ohm, all Basic: CA-033's 33 ohm, a mono plug's 0.33 W
  shared 0.11 W each), the tip on MO_T (GPIO21 through R15 10 ohm). Two receivers' opto LEDs sit in parallel on one
  source and one sink; MIDI OUT 2's own driver (Q1 AO3400A, D6, R20, R21 / R27, R22) is gone.
- **BOOT is a plain button again:** SW1, the Basic 5.1 mm SMD tactile (C318884), under a big cap (~9 mm, stage 2),
  RST (SW2) the same part under a small cap, 10 mm apart in the middle. GPIO0 carries only pin 5, R16 (10 k pull-up)
  and SW1: `check_nets.py` now fails if anything else lands on BOOT. A through-hole 6 x 6 tactile instead
  (K2-1102DP-C4SW-04 C110153 $0.059 or TS-6645DD C53431127 $0.041) adds one Extended type, +$3.09 an order: not done.
- **One indicator:** a red 0603 (C2286) above a green 0805 (C2297), 2.2 mm apart under one window (both on = yellow),
  both Basic (the RGB LED would add a $3.09 type): GPIO38 red, GPIO39 green, each through 1 k (R40 / R41) with its DNP
  twin pad pair (RT40 / RT41: no cost). Shows beat (a flash) and lock (green locked, yellow searching, red lost).
- **Kept:** the LINE IN (J7, D7, the LMV321 summer, U7), the chip, crystal, RF and antenna, the USB-C and its ESD.
- **Size: 66 x 68 mm kept.** Without the cells the removals allowed 66 x 60.5 (SIZE=66x52.5 placed clean: the chip
  block and the antenna strip move up 7.5 mm, the top row and the DINs stay); the cells need the band under the DIN
  bodies (y 37..58), which a 60.5 mm board would put into the antenna strip.
- **The cells' room under the board** (`case/renders/sketch-2.2.png`): the underside is free from y 13 (above it the
  jacks' and USB-C's legs) to the sockets' bodies at y 52.7 (they sit low, just over the antenna strip at 58). **Two
  65 x 18 mm cells side by side along the width: y 13..49, 3.7 mm left. Only without the DINs:** fitted DIN bodies take
  y 17.6..39.0 at both side edges, leaving 13.7 mm (y 39.0..52.7) at full width: no 18 mm cell fits then. The case is
  not changed yet (stage 2): the no-DIN base needs ~18 mm of depth for the cells (it is 4.3 mm today).

**GPIO map (stage 1c):** GPIO21 MIDI_TX (both OUTs), GPIO0 BOOT, GPIO38 LED red, GPIO39 LED green, GPIO9 LINE_IN,
GPIO1 VBAT_ADC (its old fan-out lane), GPIO19 / 20 USB. Gone: GPIO2 VBUS_SENSE, GPIO4 PWR_HOLD, GPIO10 MIDI2_TX, the
encoder's and the RGB LEDs' pins.

**Checks (placed board, 2026-11-14):** ERC 0 errors (1 warning: ANT_END, as before); `check_nets.py` 287 pads, 0
problems (with the new BOOT rule); courtyards as boxes, every top pair: no new overlap (mini 2's four touches of 0.00 ..
0.07 mm remain: C12 / C13, C12 / C23, C34 / U7, R18 / R19); no top part in the DIN pads' iron room; DRC: only the
unrouted connections and the DNP parts' library notes. `render-top-placement.png`, `render-bottom-placement.png`.

**Cost (cost.py's method, `fab/cost.md`; snapshot 2026-11-13, the new parts read 2026-11-14; shipping left out):**

| | option 1: USB only (AMS1117-3.3, no cell circuit) | **option 2: the cell (built)** |
|---|---|---|
| parts fitted a board / BOM lines / joints | 66 / 37 / 225 | 78 / 43 / 257 |
| Extended types (x $3.09) | 10 | 12 (+ HT7833, XB5353A) |
| **10 boards** (cost.py's table) | **$101.07** ($10.11 a board) | **$114.00** ($11.40 a board) |
| 5 boards | $75.90 ($15.18) | $85.96 ($17.19) |
| 10 boards, A: JLC hand-solders the 3 jacks | $106.41 | $119.34 |
| 10 boards, B: the user solders the jacks (+ $1.66 jacks, + shipping) | $97.72 | $110.65 |

(Stage 1b's encoder board: $168.64, $175.01 with JLC's hand-soldering.) Option 1 is computed from option 2's summary
(the cell parts out, the AMS1117 C6186, Basic, in place of the HT7833), the same snapshot. The PH2.0 sockets are not in
it (DNP; $0.036 each in JLC's listing).

**Extended types left (12) and the sweep:**

| LCSC | Part | Job | Basic instead? |
|---|---|---|---|
| C2913196 | ESP32-S3FN8 | the chip | not touched |
| C254377 | 40 MHz crystal | the clock | not touched (no Basic 40 MHz) |
| C293767 | ANT3216 chip antenna | RF | not touched; proposal below |
| C88374 | u.FL | RF (selected) | not touched; proposal below |
| C87532 | 3.3 nH 0402 | RF match | not touched; proposal below |
| C76769 | 2.0 nH 0402 | VDD3P3 filter | not touched (no Basic RF inductor) |
| C7519 | USBLC6-2SC6 | USB ESD | not touched (no Basic USB ESD found) |
| C2765186 | USB-C 16P | USB | none Basic at JLC (searched with the Basic filter, 2026-11-14) |
| C18185602 | PJ-320A jack x3 | MIDI OUT 1, 2, LINE IN | none Basic |
| C49338 | PESD5V0S2BT | LINE IN ESD | none Basic found; proposal below |
| C164106 | HT7833 | 3.3 V from USB or the cell | option 1's AMS1117 is Basic but cannot run from a cell (1.1 V dropout) |
| C80628 | XB5353A | the cell's protection | kept (whoop cells may have none) |

**Applied:** the 68 ohm 1206 pair (C17976, Extended) -> 3 x 100 ohm 1206 (C17901, Basic): -$3.09, 33.3 ohm instead of
34. **Gone with the removals:** C2763866, C2873354, C2909397, C3029072, C470742, C474307, C51018335, C79317 (8 types).

**Proposals, not applied:**

| Change | Saves (10 boards) | Risk |
|---|---|---|
| One antenna path only: drop AE1 (u.FL only) or J4 (chip antenna only) | $3.09 + $2.27 (AE1) or $1.29 (J4) | the other path is gone; the chip antenna's range on this ground is unmeasured |
| L3 = 2.0 nH (C76769, as L2) instead of 3.3 nH | $3.09 | the RF match moves (Espressif's range 2.0..3.0 nH; untuned either way) |
| Drop D7 (LINE IN ESD): the 20 k / 1 uF in front of the op-amp limit a pulse | $3.09 + $1.16 | a static discharge at the jack reaches the op-amp's inputs |
| U7 -> AVDD from +3V3 through an RC (22 ohm + 10 uF): U7 is Basic | ~$1.5 (U7 $1.51 for 10, less the RC) | Wi-Fi bursts on +3V3 reach the summer (its PSRR, unmeasured) |
| MIDI OUT 2 with its own source / sink resistors (+3V3 and GPIO21; the review's suggestion: e.g. 3 x 100 ohm 0805, C17408, + a 10 ohm; unchecked here): the passive copy gives ~4 mA a receiver worst case with two, and a short or a mono plug on one jack stops both | costs ~$0.3..0.6, no new type | none known; one more resistor group |
| Battery-ready regulator vs USB only: option 1 above | option 1 is $12.93 cheaper | no cell at all |

**For stage 2:** `route_fixups.py`'s MIDI2_TX / +3V3 fix-ups are mini 2's routing and must go; `freeroute.py`'s power
widths need VSYS / BAT_P / BAT_N (DIN_NETS' PIN_R / PIN_T are gone, MO_* covers J6 now); the case (cells, BOOT's big
cap, the LED window over both LEDs, the no-DIN base's depth).

## mini 2.2 (stage 1, superseded by stage 1c)

**What changed (parts.py):** SW1 (BOOT), LED1 (red) and R9 are gone. New: **ENC1**, the big board's encoder
(**ALPS EC11E09244BS, C470742**: 18 detents / 9 pulses, push 0.5 mm, 20 mm D shaft, through hole; JLC solders it by
hand, like the jacks; its fee for that is not in `fab/cost.md`); **LED1 / LED2**, two RGB LEDs **SZYY1615RGB-A
(C3029072)**, 1.6 x 1.5 mm, common cathode, G / B Vf 2.6..3.0 V (low enough for 3.3 V); **R40..R45** 1 k (C11702, Basic),
one per anode, each with a DNP twin (RT40..RT45, below); **R46 / R47** 100 k pull-ups and **C40 / C41** 10 nF on A / B (codes already on the board). 113 parts
in the schematic, 101 fitted, 52 BOM lines, 312 joints.

**Why discrete RGB, not addressable:** 0 mA when off. Every addressable part JLC stocks (SK6805-EC15, SK6812MINI-E,
WS2812...) needs 3.5..3.7 V or more, so it cannot run from +3V3; on VSYS (the bank, 3.0..4.8 V on MIDI power) it browns out below
3.5..3.7 V, needs a level shifter (VIH 0.7 x VDD), and draws **0.25..0.6 mA idle each** (JLC listing) unless a FET
switches it. On MIDI power the LED budget is ~1..2 mA in all (the MIDI-power study), so two idle addressable LEDs would
take most of it. The price: 6 GPIOs instead of 1.

**LED current (ESTIMATES from the Vf ratings, unmeasured):** 3.3 V GPIO, 1 k: red ~1.3 mA, green / blue ~0.4..0.8 mA
each when full on; **0 when off** (no idle draw). Both LEDs fully white: ~5 mA peak. The firmware dims with LEDC PWM (6
of the S3's 8 channels) and keeps them off or very dim on MIDI power. Brightness through the lid at these currents is
unmeasured (mini 2's red LED ran ~0.3 mA through 5.1 k).

**Encoder current:** at a detent the A contact may rest closed (ALPS: 18-detent types; B unspecified), so up to
**2 x 33 uA = 66 uA** with the 100 k pull-ups (10 k would be 0.66 mA). The push closes only while pressed.

**GPIO map (new / changed):**

| Function | GPIO | Chip pin | Notes |
|---|---|---|---|
| BOOT = the encoder's push (D / E) | 0 | 5 | replaces SW1; R16 10 k pull-up; push + RST = download mode (the user's ruling 2026-11-14: kept) |
| ENC_A / ENC_B | 33 / 34 | 38 / 39 | left side; VDD_SPI domain (inputs only here); *free on the FN8 (quad flash in package, no PSRAM): confirm in the datasheet's in-package flash pin table before stage 2* |
| LED1 (beat) anodes 1 / 2 / 3 | 38 / 39 / 40 | 43 / 44 / 45 | bottom-left corner, VDD3P3_CPU domain; GPIO38 was mini 2's LED |
| LED2 (status) anodes 1 / 2 / 3 | 11 / 12 / 13 | 16 / 17 / 18 | top side, by MIDI2_TX (pin 15), towards LED2; *the way out past R35 / C36 / C20 is tight: stage 2 may move them, or use GPIO41 / 42 / 44 instead* |

**GPIO0's strap is untouched:** the BOOT net is the chip's pin 5, R16 (10 k to +3V3) and the encoder's D pin, nothing
else (the netlist, `fab/mini2.net.xml`, 2026-11-14): no capacitor to slow it, no other driver; it is read only at
reset, a normal input after it. Holding the knob down through RST or a replug always enters the ROM's download mode,
whatever the firmware does.

Unchanged: everything else in "Pin map" below (GPIO38's LED_DRV is now LED1's anode 1). Still free: GPIO5..8 (ADC1,
closed in by the fan-out lanes), 14..18, 35..37, 41..44; strapping 3, 45, 46. **Which colour sits on which anode pad is
UNVERIFIED** (EasyEDA's symbol: pad 4 = common cathode, pads 1..3 anodes; the datasheet could not be read here): all
anodes have the same 1 k, so the colour map is firmware only.

**Placement (`render-top-placement.png`):** the encoder's shaft at **(33.0, 27.5)**, the middle of the width, where
BOOT, RST and the red LED were; **LED1 (BEAT) at (21.0, 27.5) and LED2 (STATUS) at (45.0, 27.5)**, 12 mm each side of the
shaft (5 mm outside a 14 mm knob), their resistors between them and the encoder; R46 / C40 by pin A, R47 / C41 by pin B;
**SW2 (RST) at (41.0, 15.6)**, under the USB-C's right side, 14 mm from the shaft. The board stays **66 x 68 mm**.
**The twins (stage 1b, the user's 2026-11-14):** RT40..RT45, a bare 0603 pad pair in parallel with each LED resistor
(R40..R45), as hardware/sd-board's twins in the optimist repo: not fitted (no part, no paste, out of the BOM and the
CPL), no via and no track of their own (stage 2 joins each pad to the net it shares with its resistor, on the top). A
second resistor soldered there brightens that colour (1 k // 1 k = 500 ohm). A column of three above each resistor
column, between the encoder and R14 / R26 (left) or Q1 / R20 (right); RT40 / RT43 nearest their resistors.

**Checks on the placed board (stage 1b, 2026-11-14):** ERC 0 errors (1 warning, ANT_END as before); `check_nets.py` 346
pads, 0 problems; courtyards measured as boxes on every top part pair: **no new overlap**. (The stage-1 note "DRC 0
courtyard overlaps" was not enough: KiCad's DRC let 0.01..0.05 mm touches through, R46 / R47 / C40 / C41 on the
encoder's; they are 0.2 mm further out now. mini 2's own five touches remain: D4 / D5 0.05, R18 / R19 0.07, C34 / U7
0.04 mm, C12 with C13 and C23 0.00.) DRC: only the expected unconnected items (not routed) and the library notes of the
DNP parts. No top part entered the DIN pads' iron room. `fab/cost.md` regenerated: the twins are DNP, so the cost does
not change.

**Case (sketch only: `case/sketch_22.py` -> `case/renders/sketch-2.2.png`):** the lid stays 12.4 mm over the board (the
cans); the shaft top is **24.5 mm** over the board (ALPS drawing, *unverified*), so ~12 mm stands above the lid: the big
box's 14 x 12 knob (10 mm bore) sits ~2 mm over the lid. Shaft hole ~7.6 mm (the 7 mm bushing reaches ~11.5 mm,
*unverified*, into the 11.0..12.4 mm roof). Two LED light pipes as mini 2's LED1 window; RST a button cap like mini 2 (the user's default, 2026-11-14). A post
in the base under the encoder (the push loads the middle of the board) is for stage 2.

**Cost (cost.py's method, `fab/cost.md`; mini 2's 2026-11-13 snapshot, the 3 new codes read 2026-11-14):** **10 boards
$168.64 ($16.86 a board) against mini 2's $141.41: +$27.23**: the encoder $19.68 (10 x $1.97), 2 more Extended types
(encoder, RGB LED) +$6.18, the LEDs $1.11, joints +$0.46, minus SW1 / LED1 / R9. 5 boards $126.43. Not in it: JLC's
hand-soldering fee for the through-hole encoder (as for the jacks). No cheaper EC11 with the same 18 / 9 detents was in
stock at JLC on 2026-11-14; the SMD EC11J1525402 (C209762, $3.06, 30 / 15 detents) is the fallback if JLC will not
hand-solder.

**Through-hole parts: who solders them (stage 1b; `fab/cost.md` "Through-hole parts", from `make_assembly.py`'s new
`tht` / `tht_joints`).** Through hole on mini 2.2 today: the three jacks (J2, J3, J7: PJ-320A-4P DIP, 4 joints each),
the encoder (7 joints), the DINs (DNP: the user solders them): **19 joints a board for JLC**. JLC's prices (help page
"PCB Assembly Cost", updated 2026-10-22, read 2026-11-14): **manual soldering $0.0164 a joint** (1..10,000 joints) and
**$3.58 labour an order**, the same for Economic and Standard; Economic's setup $8.18 / stencil $1.53, Standard's $25.56
/ $8.21 (one side). **Does Economic take through-hole parts?** Not settled: JLC's prototype-assembly page lists "SMT,
through-hole and mixed-technology assembly" for both kinds; forum users report a through-hole part moving the order out
of Economic; only the online quote decides. **mini 2's jacks have never been quoted**: the design assumed JLC solders them by
hand (README "DIN").

| 10 boards (USD, parts snapshot 2026-11-13 + 2026-11-14) | total |
|---|---|
| cost.py's table (through-hole joints at the SMD rate, as mini 2's $141.41) | 168.64 |
| **A: JLC hand-solders the jacks and the encoder** (Economic: 190 joints x $0.0164 + $3.58) | **175.01** |
| A, if JLC moves the order to Standard (+ setup and stencil; Standard's feeder fees not counted) | 198.99 |
| **B: the user solders them**: the JLC order without them (parts, 2 Extended fees, joints) | 140.74 |
| B: the jacks and encoders bought separately (same unit prices, no JLC loss; + shipping) | 21.34 |
| **B in all** (+ shipping) | **162.08** |

**The connectors and controls, SMD or through hole (JLC parts search, 2026-11-14; prices at 10 pieces):**

| Part | Now | Through-hole option | Fits? |
|---|---|---|---|
| Encoder ENC1 | ALPS EC11E09244BS, THT (C470742) $1.97: 10 boards $19.68; A +$1.15 joints, B: the user solders 7 | (SMD instead: EC11J1525402, C209762, $2.60, 30 / 15 detents, 11 SMD pads: $25.98 for 10, machine placed) | THT kept: cheaper, the big board's part, the user's preference |
| Jacks J2, J3, J7 | PJ-320A-4P DIP, THT (C18185602) $0.055 | already through hole | as is |
| DINs J5, J6 | DIN-504B-M10, THT (C2939346) $0.37, DNP | already through hole, the user solders | as is |
| USB-C J1 | TYPE-C 16PIN 2MD(073) (C2765186) $0.073: SMD contacts, **4 through-hole shell legs** (the plug's force goes into them) | TYPE-C-31-D-09 (C7463258) $0.29, 16 pins + 4 legs all through hole (1.1 mm pitch, 0.8 mm pads): +$2.18 for 10, +20 joints a board (A: +$3.28 for 10) | **not a drop-in**: about the same width (pins +-3.85 mm, legs +-2.5 mm against +-4.32 mm now), but the USB corner (U5 under the connector, the drawn D+ / D- bridge, the VBUS via) must be redrawn round its two rows of holes; its height and how far it stands past the edge are unverified (no 3D model read) |
| u.FL J4 | Hirose U.FL-R-SMT-1, SMD | **none**: u.FL exists only as SMD | as is |
| RST SW2 | TS-1187A 5.1 x 5.1 SMD (C318884, Basic) $0.022 | 6 x 6 THT: HRO K2-1102DP-C4SW-04 (C110153) $0.059, 5 mm (JLC refused the K2-1102DP family for the big board on 2026-11-09); TS-6645DD-6x6x5-100 (C53431127) $0.041, stock 519 | the spot (41.0, 15.6) has 7.4 x 5.5 mm: a 6 x 6 with its legs about 6.5 x 4.5 should fit (*unverified*); Basic -> Extended (+$3.09 under A, nothing under B) |

**Power note (from the MIDI-power study, 2026-11-14):** beat follow needs USB power anyway (23..41 mA against
4.6..9.2 mA from a MIDI OUT); the LINE IN's front end (U7, from VBUS) is off without USB, which fits.

**Stage 2 (2026-11-13): routed, every check run, fab files, case. Nothing ordered, built or measured.** This README is jank mini's, kept for everything unchanged; the sections "LINE IN", "Routing (mini 2)", "What changed vs mini 1" and "Checks (mini 2)" below are mini 2's (numbers in "Layout", "DRC and ERC", "Parts and cost" further down are mini 1's, kept for reference).

# (mini 1) jank chip ("jank mini"): the tiny board with a bare ESP32-S3 chip, for JLC's cheap assembly

Part of the **[jank device](../../README.md)** · the module version: [hardware/tiny](../tiny/README.md) · the study
behind it: [docs/dongle.md](../../docs/dongle.md)

The **jank tiny circuit** (USB stick with a computer; alone, powered by a piece of gear's MIDI OUT and sending it jank's
clock) with the ESP32-S3-MINI-1 module replaced by the **bare ESP32-S3FN8 chip** (QFN56, 8 MB flash in the package),
its 40 MHz crystal, its supply filters, its RF matching a **u.FL for an external antenna** (selected) and a **ceramic chip antenna** (fitted; selected by moving one 0 ohm).
Why: the module forces JLC's *Standard* assembly (a 5-board quote: $155); the bare chip is placed by JLC's **Economic**
assembly (one side, SMD only; setup $8.24, stencil $1.55, $3.09 per Extended part type). Target: **under EUR 20 a
board for 5**. The user calls it **jank mini**; the files keep the name "chip". Everything is generated by the scripts here from one parts table (`parts.py`).

> **Stage: design for review (2026-11-11).** Nothing is ordered, built or measured. **The RF matching is not tuned**
> (no VNA here): the values are the XIAO ESP32S3's, every matching position is a pad (README "RF"). Prices and stock are
> one snapshot (`fab/parts-snapshot.json`, JLCPCB's parts search on 2026-11-11), not a quote.

| | |
|---|---|
| Size | mini 2: **66 x 68 mm** (mini 1: 66 x 60), 2 layers, 1.6 mm FR-4 |
| Assembly | JLC **Economic**: every fitted part on the top side; mini 2: 91 parts on 50 BOM lines, 285 joints (mini 1: 74 / 41 / 244) |
| Checks (mini 2, 2026-11-13) | ERC 0 errors (1 warning: ANT_END, a one-pin net by design); DRC **0 errors, 0 unconnected** (6 warnings: the antenna's end-pad copper ends in the open, as on YAGEO's board; 5 library-footprint notes); every board pad against the schematic: 0 problems; every CPL place against JLC's footprint: within 0.01 mm |
| Estimate (shipping left out) | mini 2: **5 boards $107.99, 10 boards $141.41 ($14.14 a board)** (`fab/cost.md`, snapshot 2026-11-13); mini 1: $101.40 / $132.40 on the same method |
| Files for JLC | `fab/chip-gerbers.zip`, `fab/bom.csv`, `fab/cpl.csv` |
| Pictures | `render-top.png`, `render-bottom.png`, `render-3d.png`, `preview/` (routing with the DIN pads' iron room, 3D, a DIN close-up) |

## Files

| File | What |
|---|---|
| `parts.py` | **The parts table**: every part, value, JLC footprint, LCSC code, the net on each pad; the GPIO map; the DNP list |
| `fetch_jlc.sh` | JLC's own footprints and 3D models (EasyEDA, easyeda2kicad 1.0.1 via `uvx`) into `lib/jlc.pretty`, `lib/3d` (committed) |
| `make_footprints.py` | `lib/chip.pretty`: JLC's footprints, models pointed at `lib/3d`, the QFN's via "pads" dropped, peg holes unplated, courtyards redrawn; the DIN sockets' pads made bigger for a hand iron |
| `make_schematic.py` | `chip.kicad_sch` (every pin labelled), `chip.kicad_pro`, `lib/chip.kicad_sym`, `fab/intended-nets.json` |
| `freeroute.py` | the routing (2026-11-12): Freerouting in three rounds on the placed board, then narrow tracks widened, GND stitching every 6 mm, a via in every cut-off pour piece (README "Routing") |
| `fetch_freerouting.sh` | Freerouting 2.5.0 into `.freerouting/` (git-ignored), SHA-256 checked |
| `route.py` | the old grid router (unused since 2026-11-12: it stalled for many minutes on single nets) |
| `make_board.py` | `chip.kicad_pcb`: placement (PLACED: the placer's groups), the RF path drawn (line, fences, antenna feed, VDD3P3's via), pours, antenna strip; `make_fab.sh` runs it with `LANE_VIAS=1 NO_STITCH=1 PLACE_ONLY=1` and routes with `freeroute.py` |
| `tie_groups.py` | after routing: a via for any ground pour piece cut off on both layers at once (`make_fab.sh` runs it) |
| `check_nets.py` | the netlist against `parts.py`, every board pad against it, the pin rules |
| `make_assembly.py` | `fab/bom.csv`, `fab/cpl.csv` (fitted parts only); every part's pads checked against JLC's footprint at the CPL place |
| `jlc_parts.py` | `fab/parts-snapshot.json`: stock, prices, Basic / Extended |
| `cost.py` | `fab/cost.md`: the estimate for 5 and 10 boards at Economic (shipping left out: shared with other boards) |
| `make_mockup.py` | `mockup.stl`: the board and every fitted part as a block, one closed solid |
| `rf_line.py` | the RF line's width and gap (grounded coplanar waveguide, Wadell's formula) |
| `make_fab.sh` | **all of it in order** |
| `case/` | `chip_case.scad`, `make_stl.sh`, `stl/` (base, lid, button cap), `assembly.png` |
| `chip.kicad_dru` | the one DRC exception (the USB-C's own locating holes, as in hardware/tiny) |

```
sh hardware/chip/make_fab.sh           # KiCad 10 (kicad-cli + its Python), python3, OpenSCAD; ~15 min (the router)
ROUTE=0 sh hardware/chip/make_fab.sh   # keep the routed board, redo the checks and outputs
```

**For JLCPCB:** `fab/chip-gerbers.zip` (2 layers, 1.6 mm, FR-4), `fab/bom.csv`, `fab/cpl.csv`. Economic assembly, top
side.

## Schematic

The tiny circuit, unchanged (see [hardware/tiny/README.md](../tiny/README.md) "Schematic" for the why of every value):
POWER IN through 47 ohm and B5819W from both data pins into VSYS (2 x 2200 uF, 4.4 mF), USB VBUS through D3 and R5 4.7 ohm (a 2010, 0.75 W: on plug-in it takes ~0.9 A, 3.9 W, 45 mJ in ~20 ms, ngspice
2026-11-12; R23 10k holds VBUS down without USB; 10 ohm was tried and dropped: it would
starve USB, (VBUS - 0.35 - 3.4) / R5 = 75 mA at 4.5 V against Wi-Fi's ~100 mA, review 2026-11-12); USB D+ / D- clamped by **U5, a USBLC6-2SC6** (C7519, right under the USB-C, the user's ruling 2026-11-12; its pin 5 on VBUS as the clamp's reference), then through **R24 / R25 22 ohm** (0402, by the chip); a **RST button (SW2)** from CHIP_PU to ground beside BOOT, the
BL8506-40 detector / VBUS / PWR_HOLD enabling the TPS62842 buck (3.3 V, 60 nA), MIDI OUT at 3.3 V (33 / 10 ohm), BOOT
button on GPIO0, the LED, the two ADC dividers (tiny's cable pads P2 / P3 are gone: README "DIN"). Same values as tiny; R7, R8, R10, R11, C7 and
C8 are 0402 here (Basic parts too) to make room round the chip. **New, round the chip** (values from the Seeed
XIAO ESP32S3 v1.2 schematic, a proven board with this chip; checked against Espressif's hardware design checklist):

| Pin(s) | What | Parts |
|---|---|---|
| 55, 56 VDDA | analog supply, straight from +3V3 | C11 2.2 uF, C12 100 nF, C13 10 nF |
| 2, 3 VDD3P3 | the RF supply, behind an LC (checklist: "an LC circuit... inductor rated 500 mA and above") | before L2: C17 **10 uF** (the checklist's 10 uF: the buck's 22 uF is ~14 mm away), C14 / C15 2.2 uF, C16 100 nF; **L2 2.0 nH** (900 mA); after: C18 100 nF |
| 46 VDD3P3_CPU, 20 VDD3P3_RTC | digital supplies | C19, C20 100 nF |
| 29 VDD_SPI | the in-package flash's 3.3 V (VDD3P3_RTC through 14 ohm; GPIO45 low = 3.3 V) | C21 100 nF, C22 1 uF |
| 4 CHIP_PU | enable / reset | R8 10 k to +3V3, C7 1 uF (Espressif's RC) |
| 53, 54 XTAL_N / P | 40 MHz crystal Y1, 8 pF load, +-10 ppm | C9 / C10 10 pF each side, R17 0 ohm in series on XTAL_P |
| 5 GPIO0 | BOOT | R16 10 k pull-up (the checklist), SW1 to GND |
| 1 LNA_IN | RF | below |
| 57 | GND, the exposed pad | 9 vias in it (a 3 x 3 grid, 1.3 mm pitch; Espressif: 9 or more) |

Every supply pin of the QFN56 is listed (datasheet "Pin Overview": 2, 3, 20, 29, 46, 55, 56, 57); nothing else is a
supply. Pins 28, 30..37 are the in-package flash's SPI: left open.

**The crystal:** no 40 MHz crystal is a JLC Basic part; HELE X3S040000B81H-R (C254377, Extended) has 8 pF load and
+-10 ppm (the checklist asks +-10 ppm). **The load caps:** C_L = C9 x C10 / (C9 + C10) + C_stray. C_stray here is
*estimated* at ~2.5 pF (the chip's XTAL pins ~1 pF each side in series, the 3.9 / 6.0 mm traces over ground ~0.1 pF/mm,
the crystal's pads): C9 = C10 = 2 x (8 - 2.5) = 11 pF ideal; the E12 10 pF gives C_L = 5 + 2.5 = 7.5 pF (12 pF would
give 8.5 pF). 10 pF kept (the XIAO's value), a little under 8 pF: the crystal runs a few ppm fast. Espressif: tune the caps on the first boards with a frequency-offset test (*not done*).

**Strapping:** GPIO45 and GPIO46 open (internal pull-downs: VDD_SPI 3.3 V, normal SPI boot); GPIO3 open; GPIO0 high
(R16) unless BOOT is held.

**USB:** D- / D+ = GPIO19 / 20 straight to the USB-C (the checklist's optional series resistors and capacitors left
out: not needed at full speed on traces this short).

### Start-up: is a reset supervisor needed?

Espressif's checklist: with a *slow power rise*, the RC on CHIP_PU may not be enough; add a supervisor (~3.0 V). Here the
slow rise is in VSYS (seconds, from MIDI), **not in +3V3**: the detector holds the buck off until VSYS reaches ~4.16 V
and the buck then brings +3V3 up in its soft-start (~1 ms, TI datasheet), after which the 10 k / 1 uF RC releases
CHIP_PU ~10 ms later. **But on USB** +3V3 follows VSYS as the bank charges through R5 (4.4 mF x ~10 ohm: a ~44 ms
time constant), and the 10 k / 1 uF RC then lets CHIP_PU go near 2.2 V, too early (review 2026-11-12). So **U4 is
fitted** (2026-11-12): a BL8506-30NRM (C2763866, SOT-23, Extended), 3.0 V open-drain detector on CHIP_PU, R8 its
pull-up, by the CHIP_PU lane: CHIP_PU stays low until +3V3 is past 3.0 V, whatever the rise. (C7 4.7 uF instead was
the other choice: an RC still only lags +3V3, so it stays a race with a slow rise.) The fall is the open case: if
the firmware keeps PWR_HOLD high while the bank runs down, +3V3 sags slowly with the buck in 100 % mode; the chip's own
brown-out detector then resets it, but a flash write at that moment can be cut. **The firmware must release PWR_HOLD
(and stop writing flash) before the bank drops under ~3.4 V** (it reads the bank on GPIO1). *Unverified on hardware.*
U4 also resets the chip cleanly on that slow fall (+3V3 under 3.0 V).

**The start-up budget (the bank, 2 x 2200 uF = 4.4 mF, the user's ruling 2026-11-11):** the detector releases the buck
at ~4.16 V; the chip runs until the bank is down to ~3.05 V (3.0 V for the ESP32-S3 plus the buck's 100 % mode drop at
light load, *estimated*). Held between them: 4.4 mF x (4.16 - 3.05) V = **4.9 mC: ~163 ms at 30 mA** (the boot's
current and length *unmeasured*; at 2 mF it was ~74 ms).

**VBUS without USB:** D3's reverse leakage (microamps, more when warm) would otherwise float VBUS up through R12 + R13
(200 k), lifting BUCK_EN through D5 and faking VBUS_SENSE. **R23 10 k** holds it near 0 V (at 10 uA leakage: 0.1 V).
With 33 k the simulation (2026-11-12) found the buck starting at VSYS 1.77 V on MIDI power from ~40 uA of leakage
(25 uA at EN's 0.4 V minimum) and the chip reset-looping; 10 k needs 40 uA for 0.4 V.

**For the firmware (ngspice, 2026-11-12, not board changes):** on MIDI power the boot must stay under ~100 ms at
~35 mA (a 300 ms boot reset-loops: ~5 mC a start against the bank's 4.9 mC); take PWR_HOLD only above VBANK ~3.9 V,
else go to deep sleep; on a 4.5 V USB port keep TX bursts short (one 20 ms burst at 340 mA resets the chip via U4).

## RF

```
 U1 pin 1 LNA_IN --+-- L3 3.3 nH --+==== 50 ohm line (RF) ====+-- R18 0 ohm (DNP) +-- ANT_FEED --> AE1 ANT3216 (feed, S1)
                  |                |                          |               |
               C23 1 pF        C24 (DNP)                  C25 (DNP)       C26 (DNP)
                  |                |                          |               |
                 GND              GND                        GND             GND
                                                              +-- R19 0 ohm -- J4 u.FL (fitted, selected)
```

- **The chip's pi** (C23 / L3 / C24): XIAO ESP32S3 v1.2 values: 1 pF shunt at the chip (on a short stub, as Espressif
  asks), 3.3 nH series, the second shunt unfitted. Espressif's range for this board-specific match: 1.2..1.8 pF,
  2.0..3.0 nH, 1.2..1.8 pF, tuned with a VNA to S11 near 35 + j0 ohm and S21 < -35 dB at 4.8 / 7.2 GHz. **We have no
  VNA: the XIAO values are fitted and every position is a pad.** 0402 parts (Espressif prefers 0201; JLC Economic does
  not place 0201).
- **The line:** grounded coplanar waveguide on the top layer over the bottom pour, **1.2 mm wide, 0.2 mm gaps** to the
  top pour, GND via fences on both sides where parts leave room (0.3 / 0.6 mm vias every 0.9 mm, their rings 0.35 mm from the
  line's edge; the matching pads interrupt them: 10 fence vias in all). For JLC's 2-layer
  1.6 mm FR-4 (dielectric 1.53 mm between the copper layers, er 4.5 *assumed*, 35 um copper): Z0 = **50.5 ohm**,
  e_eff 2.83, from Wadell's GCPW formula (*Transmission Line Design Handbook*, 1991, 3.4.3; quasi-static, zero
  thickness): `rf_line.py`. er 4.3..4.6 gives 51.4..50.0 ohm. The line is ~9 mm long (L3 to R18), about an eighth of the wavelength
  on this board (73 mm at 2.45 GHz), so a few ohms off matter little. JLC's own impedance calculator was not used:
  check the figure there before ordering if it matters.
- **The antenna:** YAGEO ANT3216LL00R2400A (C293767), as on YAGEO's evaluation board (datasheet p. 4): the antenna in a
  **10 mm strip with no copper on either layer** across the bottom edge, its feed 2.0 mm past the ground's edge, its end
  pad (S2) unconnected and extended by a 1.8 x 2.5 mm copper pad. Pad 1 of JLC's footprint is taken as S1 (feed):
  *check the part's marking (the dot is at S1) in JLC's placement preview*. Its own pi (C25 / R18 / C26) sits at the
  ground's edge: R18 a 0 ohm, the shunts unfitted (YAGEO's eval board has the same three positions; its values are not
  in the datasheet).
- **The antenna, the user's ruling (2026-11-11): both fitted, the u.FL selected.** R19 (0 ohm, fitted) takes the
  matched line to **J4, a Hirose U.FL-R-SMT-1 (C88374, Extended, $0.09)**, ~2 mm from the selector; R18 (the 0 ohm to
  the chip antenna) is DNP. **Back to the chip antenna:** move the 0 ohm from R19 to R18 (AE1 stays fitted for that:
  $0.19 a board and its $3.09 Extended fee a batch). The external antenna is **the XIAO ESP32S3's own chain** (the same
  1 pF / 3.3 nH match into a u.FL): a 2.4 GHz flex (FPC) antenna on an IPEX cable sticks inside the case's lid, on the
  seat over the antenna strip (case/README.md). J4's metal sits ~1.5 mm from AE1's feed: with the chip antenna
  selected it detunes it a little (*unmeasured*).
- **Why 2 layers is enough:** the line is short (~9 mm), the bottom layer under the chip and
  the RF path is kept solid ground (the router may not use it there). 4 layers would give a thinner dielectric and a
  narrower line (~0.3 mm) but costs more at JLC; not needed for one short line.
- **What only a measurement can tell:** the match, the antenna's resonance on this ground (the eval board's ground is
  30 x 18 mm; ours is 49.6 x 42.5 mm with an aluminium can 5 mm from the line), the range. A Wi-Fi RSSI test against the
  XIAO at the same spot is the first check.

## DNP (pads only, out of the BOM and the CPL)

| Ref | Position | Fit when |
|---|---|---|
| C24 | the chip's pi, second shunt | tuning (with a VNA) |
| C25, C26 | the antenna's pi, both shunts | tuning |
| R18 | 0 ohm to the chip antenna | to use AE1 (and R19 removed) |

## Pin map (for the firmware's board.h: the GPIOs are tiny's, unchanged)

| Function | GPIO | Chip pin | Notes |
|---|---|---|---|
| **MIDI_TX** | **21** | 27 | UART TX through R15 10 ohm to the MIDI OUT tip; RTC GPIO |
| **LED** | **38** | 43 | active high, R9 5.1 k |
| **BOOT / user button** | **0** | 5 | to GND; R16 10 k pull-up |
| **VBANK_ADC** | **1** | 6 | ADC1_CH0: VSYS / 2 |
| **VBUS_SENSE** | **2** | 7 | ADC1_CH1: VBUS / 2 |
| **PWR_HOLD** | **4** | 9 | high keeps the buck on; hold it through sleep |
| USB D- / D+ | 19 / 20 | 25 / 26 | native USB |
| **MIDI2_TX** | **10** | 15 | the POWER IN port as MIDI OUT 2 (USB power only): Q1's gate, UART TX inverted; R22 pull-down |
| CHIP_PU | | 4 | R8 10 k / C7 1 uF; SW2 (RST) to ground |

Not used, left open: every other GPIO, including the strapping pins GPIO3, 45, 46 and the console UART (GPIO43 / 44,
pins 49 / 50: no test pads; flash over USB).

## MIDI OUT 2: the POWER IN port as a second MIDI OUT (USB power only)

The user's feature (2026-11-11): with USB plugged, the POWER IN port (J3 jack and J6 DIN, in parallel) can also send
MIDI. The firmware uses it only while VBUS_SENSE is high.

```
 +3V3 -- R21 // R27 34R -- D6 B5819W -->|-- PIN_R (ring, DIN 4)
 PIN_T (tip, DIN 5) -- R20 10R -- Q1 AO3400A drain; source = GND
 GPIO10 MIDI2_TX -- Q1 gate; R22 100k gate to GND
```

- **Sink:** Q1, an N-MOSFET with a 1.45 V threshold (48 mohm at 2.5 V gate, so fully on from a 3.3 V GPIO; a 2N7002, Vgs(th)
  up to 2.5 V, was the first idea and is marginal at 3.3 V), open drain through R20 10 ohm (CA-033's sink resistor).
  R22 100k holds the gate low in reset, boot and sleep (GPIO10 is high-impedance then; its power-up glitch is a low
  level, harmless). **Off = no current, as if nothing were there:** the port works as the power input as before.
- **Source:** +3V3 through R21 // R27 (2 x 68 ohm 1206 = 34 ohm; CA-033's source resistor is 33) and D6, a Schottky: a gear's 5 V on the ring (its MIDI
  OUT powering the board) never reaches +3V3.
- **Firmware:** MIDI2_TX = GPIO10 (RTC GPIO, not strapping, free), a UART TX with its output inverted in the GPIO matrix:
  idle (1) = gate low = no current; a 0 bit = gate high = current, as MIDI wants. Only while VBUS_SENSE (GPIO2) reads
  high; otherwise keep GPIO10 low (and held low in sleep).
- **The loop** (CA-033: 3.3 V, 33 ohm source, 10 ohm sink; the receiver's 220 ohm and opto LED): I = (3.3 - V_D6 -
  V_LED) / (33 + 220 + 10 + R_DS). D6 at ~5 mA: ~0.2..0.3 V (B5819W, *estimated from its curve, unmeasured*); the opto
  LED 1.2..1.5 V. **I = 5.6..7.2 mA at 3.3 V, 5.1 mA at 3.135 V (-5 %) with the worst diode and LED**: above the 5 mA
  MIDI's current loop is designed round. The Schottky costs ~1 mA. Lowering R21 would win it back but CA-033 fixes 33 ohm
  (it also limits a shorted ring, a mono plug: ~90 mA, 0.26 W, shared by two 0.25 W 1206s since 2026-11-12, as on MIDI OUT 1).
- **The power path stays out of the way:** while sending, PIN_R is <= 3.1 V and PIN_T ~0 V, VSYS ~4.6 V on USB: D1 and
  D2 are reversed (microamps). R1 / R2 carry nothing.
- **Worst case, the firmware sends while a gear's MIDI OUT is plugged in:** the gear's tip (its TX, 5 V through its 220
  ohm) into R20 + Q1: 5 V / 230 ohm = 22 mA, 5 mW in R20, nothing for Q1 (5.7 A part); a MIDI output is made to drive
  220 ohm into a short. The gear's ring (5 V through 220 ohm) meets D6 reversed and flows into VSYS through R1 / D1, as
  when it powers the board. Nothing is damaged; the gear just receives nothing.
- **Parts:** all Basic (AO3400A C20917) or already on the board (33 ohm 0805, 10 ohm 0603, 100k 0603, B5819W): no new
  Extended type.

## DIN sockets: an optional hand-solder (J5, J6)

The user's ruling (2026-11-11): keep both 3.5 mm jacks (JLC places them; **through hole since 2026-11-12**, the user's
ruling: stronger. SHOU HAN PJ-320A-4P DIP, C18185602, the twin of the PJ-320D first used: the same body, its pins at the
same places within 0.15 mm, pads A = sleeve, C = ring, D = tip, B = the switch, Type A TRS map unchanged. Its legs
reach 3.5 mm under the board (its 3D model): they stay 1.7 mm or more clear of the DINs' plates and bodies (y 8.7 at
most, the plates start at y 9.85), so the DINs still sit flat. JLC solders them by hand: *its fee for that is not in
`fab/cost.md` (unverified, the quote will tell)*) and add **optional 5-pin DIN sockets on the
bottom side**, wired in parallel with each jack, **for the user to solder by hand**. They are **DNP**: not in the BOM
or the CPL; JLC drills their holes only.

- **J5, MIDI OUT (left edge):** DIN 4 = MO_R (+3V3 through R14 // R26, 2 x 68 ohm 1206 = 34 ohm: a mono plug's 0.33 W shared, the jack's ring), DIN 5 = MO_T (R15 10 ohm from
  GPIO21, the jack's tip), DIN 2 = GND, the shell = GND (MIDI: the sender grounds its shield). The user asked for a
  **male** DIN that plugs straight into a gear's MIDI IN: **LCSC has no board-mount male 5-pin DIN** (searched
  2026-11-11: "DIN-504B", "DIN 5P male", "MIDI male", "5 pin DIN plug", "circular connector plug 5P": only female
  sockets, cable plugs and IP68 circular connectors), so J5 is the **same female socket**, joined to the gear's MIDI IN by an
  ordinary MIDI cable (male at both ends).
- **J6, POWER IN (right edge):** DIN 4 = PIN_R, DIN 5 = PIN_T (the power path, as the jack's ring and tip), DIN 2 = GND;
  the shell left open (a receiver does not ground the cable's shield).
- **The part:** HOOYA DIN-504B-M10 (C2939346, female, right angle, through hole; JLC's own footprint), its face on the
  side edge, its axis at y 20.3 mm (below the jacks), its body under the board. **JLC's pad numbers are not the DIN pin
  numbers**: pad 1 = DIN 3, pad 2 = DIN 5, pad 3 = DIN 2 (the middle), pad 4 = DIN 4, pad 5 = DIN 1. JLC's footprint and
  hardware/pcb's own table for the part (`HY_PINS` in hardware/pcb/make_footprints.py, the CUI SDS-50J pattern, the same in
  hardware/pcb/lib) share one frame: the shell pins at the front, the 3-pin row 2.5 mm before the 2-pin row, x the same way;
  pin 4 is at x +5.0 there and pad 4 at x +5.0 here. (Until 2026-11-12 this README had pads 2 and 4 the other way round:
  MO_R / MO_T and PIN_R / PIN_T were swapped on the DINs; fixed.) *Unverified against a part: check with a meter before
  soldering*. Pads 6 and 7: the shell.
- **Pads for a hand iron:** HOOYA's drawing asks 7 x 1.5 mm holes (kept). JLC's footprint had 2.2 mm pads (a 0.35 mm
  ring); `make_footprints.py` makes them **3.0 mm round on the used pins DIN 4 and 5 (pads 2, 4: 0.75 mm ring),
  4.0 mm on DIN 2 (pad 3, 1.25 mm ring), 3.5 mm on the shell (1.0 mm ring), 2.4 mm on the unused DIN 1 and 3
  (pads 1, 5: 0.45 mm ring)**. The closest pads are the paired pins 3.54 mm apart: **0.84 mm** between them. Ovals stretched away from the pair's other pin were tried: they meet the top parts
  (U2, C8, C15, C17, J3, R1), so the pads stay round. The ground pins (DIN 2, the MIDI OUT shell) join the pours on
  four 0.5 mm spokes with a 0.5 mm gap, not solid, so the iron can heat them.
- **Bridges (the user's rule, 2026-11-11):** on both layers, no other net's pad within **0.8 mm** of a DIN pad's copper
  and no other net's track within **0.5 mm** (the router keeps a ring round each pad, `DIN_TRACK_ROOM`). Measured
  (`near.py`): tracks 0.55 mm or more; pads: the paired DIN pins 0.84 mm, J5 pad 5 to the MIDI OUT jack's ground
  pad J2.1 0.80 mm.
- **The iron's room:** no top-side part's courtyard and no via within **0.8 mm of a DIN pad's copper** (the router and
  the stitching keep vias out; `IRON` in `make_board.py`), so a tip reaches each pad from its outer side. Three pads
  have a neighbour closer on one side only, the rest of their ring open: J5 pad 5 and J6 pad 1 (DIN 3 / DIN 1, unused
  pins), clear of the jacks (their through-hole pads more than 1 mm away), and J5 pad 2 (DIN 4, MO_R) 0.4 mm from the QFN's
  body corner (the chip is 0.9 mm tall; open above and to the left). Solder from the top.
- **The cable pads P2 / P3 are gone:** they sat between the jacks and the DIN shell pins, where the iron needs room. A
  cable now goes on the DIN pads (ring = DIN 4, tip = DIN 5, sleeve = DIN 2).
- **Size:** the DINs' holes need ~16 mm in from each side edge over 17 mm of height: with 3 mm more
  for the chip's fan-out (below), the board grew from 38 x 47 to 44.0 x 51.5 mm, with the 2200 uF cans to 47.6 x 51.5 mm, and with the DIN pads'
  rings (the cans 1 mm lower, the middle 1 mm right for the buck's tracks) to **49.6 x 52.5 mm**.
- **Don't fit both a jack and its DIN to two pieces of gear at once:** they are the same nets.

## What changed vs mini 1

- **+8 mm in height** (`DY` in `make_board.py`): 66 x 68 mm. The top row (USB-C, MIDI OUT jack J2, POWER IN / MIDI OUT 2 jack J3, U5 and its VBUS parts) stays on the top edge; everything else moved down 8 mm, which opens a band for LINE IN. Width, antenna strip (10 mm), reservoir (2 x 2200 uF), start-up chain, RF section, MIDI OUT, MIDI OUT 2, buttons and LED are mini 1's, unchanged.
- **+17 parts** (J7, D7, U6, U7, 13 passives), 1 more Extended type (C49338, +$3.09); the op-amp (C7972) is a *preferred* Extended part (no fee). GPIO9 = LINE_IN (pin 14).
- **Case (stage 2):** the size is read from the board (66 x 68); a LINE IN slot (6.9 mm wide, rounded top, open downwards like the side jacks') in the lid's top wall at x = 16.4 mm; J7's front block, pegs and legs added to the collision checks; `make_mockup.py` gives J7's bushing past the top edge (it had put it on the left edge). See "Case" below.
- **Revision 2026-11-13 (after the routed board was looked at):** R5 (the 4.7 ohm inrush resistor) 3.0 mm down from the top edge (y 5.4, still beside D3); Q1 / R20 / R22 (MIDI OUT 2's FET group) 1.2 mm left, away from J6's pads; R26 / R14 (the two 68 ohm MIDI OUT resistors) 1.0 mm up (all in `make_board.py` PLACED / PLACE); the LIN_OUT via by U6 1.2 mm right (`route_fixups.py`). Their tracks were cut back to the first junction and re-routed by Freerouting with everything else locked (0.2 mm clearances, DRC unchanged: 0 errors, 0 unconnected); MIDI2_TX's run was re-laid by the router in the process.
- **C20 turned 180 degrees** (its +3V3 pad now east, `make_board.py` PLACED, y 0.15 mm up): the one placement change stage 2 needed (below).

## LINE IN (mini 2)

A 3.5 mm stereo jack (J7, C18185602, the jack mini 1 uses) into the chip's ADC: `J7 -> D7 (PESD5V0S2BT, ESD) -> 20 k per channel -> U6 (LMV321, inverting summer, L + R to mono, gain 5.6 k / 20 k = 0.28 a channel, 2.2 nF across the feedback, ~8.5 kHz low-pass) -> R35 120 ohm + C36 100 nF at the pin -> GPIO9 (ADC1_CH8, net LINE_IN)`. 1 uF coupling in (7.8 Hz high-pass), bias 1.47 V from 150 k / 120 k off AVDD. U6 and the bias run from **AVDD**, a 3.3 V XC6206 LDO (U7) fed from VBUS: **LINE IN works only with USB plugged**, and a plug-in with a hot source cannot push the pin above +3V3 (sim). ngspice (`sim/linein.py`, `sim/results.txt`, generic op-amp, 10 pF ADC cap ASSUMED): +4 dBu stereo gives 0.53 .. 2.41 V at the pin; band 7.8 Hz .. 8.48 kHz; -11.7 dB at 22 kHz; mono plug (ring grounded) half the swing; +20 dBu clips to 0.05 .. 3.29 V without harm.

**Jack position: the top edge, between the MIDI OUT jack and the USB-C** (x 16.4 mm, bushing out past the edge). Why there: the board's two side edges carry the DIN sockets, the reservoir cans and the MIDI current loops, the bottom edge the antenna (no copper, 10 mm); the top edge is the quiet one (USB only). The front end sits in the new band directly under it, 22 .. 30 mm from the jack, about 35 mm from the buck (bottom left), ~45 mm from the RF section and the antenna, and clear of both MIDI loops (left and right edge). Only the one signal (LINE_IN, ~25 mm) crosses the middle to the chip's top right pin, away from the buck's switch node and the crystal; route it on top over ground with the pour on both sides, no digital track beside it (stage 2). U7 and its capacitors sit by the USB-C's VBUS.

**Placement checks (stage 1):** ERC 0 errors; 0 courtyard overlaps; `check_nets.py` 305 pins, 0 problems. `render-top-placement.png`.

## Routing (mini 2, stage 2)

Freerouting 2.5.0 via `freeroute.py` (5 rounds, one thread, `nice -n 19`), then `tie_groups.py`; `route_fixups.py` runs after it in `make_fab.sh` (`ROUTE=1`): the old hand edits (`linein_fixup.py`, now removed) as guarded steps that act only where the router's tracks are the ones they were made for and print "skipped" otherwise, plus the LIN_OUT via move. `ROUTE=0` keeps the committed board. **A full `ROUTE=1` re-route of the revised placement was tried (2026-11-13) and is not clean: 4 connections left open and two 0.10 / 0.12 mm tracks, so the committed board is the old routing with the local changes above; re-routing it from scratch needs a look.** Antenna keep-out, RF line and fences are mini 1's, untouched.

- **LINE_IN: 3.97 mm, all on the top layer, 0 vias**, pin 14 straight up (drawn in `make_board.py`) to C36, then R35. Nothing runs under it on the bottom layer (the bottom is one ground there); the nearest bottom copper is PWR_HOLD's 0.29 mm beside (not under) the pin-14 run and VBUS_SENSE 1.09 mm.
- **Nearest foreign tracks on the top layer (edge to edge):** MIDI2_TX 0.25 mm (pin 15's escape, 3.3 V logic, 31250 baud, unavoidable: pin 15 is the neighbour of pin 14 and its track must get past R35 / C20 to the north), PWR_HOLD 0.29 mm (a static level) beside the 2.4 mm run from pin 14 to C36, LIN_OUT 0.66, +3V3 0.94, everything else 1.1 mm or more; **not** the clean "nothing beside it" of the brief: no clock or fast digital track is near it (the buck's SW, the crystal and USB are 15 mm or more away), but two slow digital tracks are within 0.3 mm for a few mm.
- **LIN_OUT** (U6 -> R35, low impedance): 0.15 .. 0.2 mm tracks, 36 mm of which 20 mm on the bottom layer (vias at 123.93 / 120.08 and 134.85 / 135.86), which cuts the ground there; it is the op-amp's output, not the high-impedance node. **All on top: tried, not possible here (2026-11-13).** Freerouting with LIN_OUT held to the top layer (`TOP_ONLY` in `freeroute.py`, off) found no path round the committed routing (+3V3's run from x 122 to 136 at y 127.9 and the buttons' tracks wall the top layer off between the front end and the chip), and in a full re-route it got there only with 0.10 / 0.12 mm tracks and 4 other connections left open.
- **Analog ground:** U6, U7 and the bias network sit in the band by the USB-C, ~35 mm from the buck and the MIDI loops; their ground returns are the F.Cu / B.Cu pours; by position only (not traced or measured) they share no return path with the buck or the MIDI loops.
- **Fix-ups** (`route_fixups.py`): C20 turned (a GND pad boxed in by MIDI2_TX gave an unconnected pour island Freerouting could not tie; the new position frees it and `tie_groups` finds its via at (29.18, 38.65)); MIDI2_TX's corner and the +3V3 hop to pin 20 re-laid; LINE_IN's last 2 mm tidied 0.03 mm away from MIDI2_TX; one LIN_OUT via moved 0.03 mm (hole to hole 0.4987 -> over 0.5); the LIN_OUT via by U6 1.2 mm right. After the 2026-11-13 local re-route the first fix-up finds the router's own re-laid MIDI2_TX corner (it prints "skipped"), the others "already done". The nearest-foreign-track figures above are from the first stage-2 board and were not re-measured (DRC clearance 0.2 mm holds).
- Tracks 0.15 mm narrow where Freerouting squeezed them (VBANK_ADC, LIN_R, LIN_OUT, UDN, MIDI2_TX): above JLC's 0.127 mm, below mini 1's 0.2.

## Checks (mini 2, the last `make_fab.sh ROUTE=0`, 2026-11-13, after the revision)

- **ERC:** 0 errors, 1 warning (ANT_END: a one-pin label, by design, as mini 1).
- **DRC:** **0 errors, 0 unconnected**, 6 warnings, all mini 1's: the ANT_END track's open end (by design) and 5 library-footprint notes (a footprint differs from its library copy, as on mini 1). `fab/drc.rpt`.
- **Pads against the schematic** (`check_nets.py`): 307 pads, 305 schematic pins, 0 problems.
- **CPL against JLC's footprints:** 91 parts, every one within 0.01 mm; `bom.csv` 50 lines, 285 joints.
- **Fab files:** `fab/mini2-gerbers.zip`, `fab/bom.csv`, `fab/cpl.csv`, `fab/cost.md`; renders `render-top.png`, `render-bottom.png`, `render-3d.png`, `mockup.stl`.

**Cost (final, `fab/cost.md`, snapshot 2026-11-13, shipping left out):** 5 boards $107.99, 10 boards $141.41 ($14.14 a board), against mini 1's $132.40 for 10 on the same method: **+$9.0 for 10 boards** (+$3.09 one Extended type, +$5.3 parts, +$0.7 joints); on the user's real mini 1 cost ($100 for 10) about **$109**.

## Layout (66 x 60 mm, 2 layers, 1.6 mm, every fitted part on top)

**2026-11-13: 66 x 60 mm** (`SIZE=WxH` in `make_board.py` tries other sizes; `PLACED` is for 66 x 60). The USB corner is
fixed, not placed: U5 under the connector, each CC resistor on its own pin's side (CC2 left, CC1 right: they used to cross
the data lines), the VBUS parts further out, U5's VBUS pin fed by a via from the bottom; the strip under U5 stays free for
D+ / D-. Drawn too: the VBUS via, C12 -> C11 (+3V3), the ground vias of C28 and C9. MIDI OUT 2 is two groups (source,
sink), its R20 3.7 mm of track from J6.

**2026-11-12: the board grew to 72 x 65 mm and the groups below the connectors were re-placed by a placer** (simulated
annealing of rigid groups round the fixed connectors, cans, antenna and chip block, each group pulled to the part it
serves, nothing within 3 mm of the RF path; its result is `PLACED` in `make_board.py`). The u.FL selector R19 sits at
the line's end beside R18, J4 beside it (no stub). The notes below describe the earlier 49.6 x 52.5 mm layout where they name
positions; the connectors, the chip block, the RF path and the antenna strip are as described.

- **Top edge:** the USB-C in the middle, **MIDI OUT (J2) on the left edge, POWER IN (J3) on the right edge** (both
  jacks at the top, their bushings past the side edges).
- **Under the jacks, both side edges:** the DIN sockets' holes (J5 / J6, bodies on the bottom); between them, 0.8 mm
  clear of every pad, the LED and the MIDI OUT resistors (left), the POWER IN diodes and resistors and the detector
  (right).
- **Middle, between the DINs:** **the button (SW1) at (24.4, 19.55)**, chosen so a finger reaches it through the lid:
  8.7 mm or more from every part taller than 3 mm (courtyards: the USB-C 8.7, J2 10.1, J3 10.8, C1 11.6, C2 12.3 mm);
  the LED (LED1 at (22.7, 14.9)) just above it with R9; BUCK_EN's diode D4, C4 and R6 to its right; the reservoir's
  sense; CHIP_PU's RC. The two cans down the sides below the DINs.
- **Lower left, beside C1:** the buck (U2, L1, C5, C6, R7, C3), next to the reservoir it draws from.
- **The button and the antenna:** SW1 is 26.6 mm (centre to centre) from AE1, far from the RF line. YAGEO's datasheet
  gives no keep-out of its own, only its evaluation board: the antenna in a 10 mm wide copper-free area across the board's
  end. A button in that strip (a corner far from AE1) would need copper there (its pads and a track): not done.
  **MIDI OUT 2's parts** (D6, Q1, R20, R22, R21 // R27) lower right, beside C2; R14 // R26 top left between J2 and
  the USB-C; R5 (2010) top right between the USB-C and J3.
- **The chip** in the lower middle, turned so LNA_IN is at its lower right corner; its decoupling round it, the crystal
  right under its XTAL pins, the VDD3P3 filter to its right (C28 100 nF right at pins 2 / 3), the u.FL (fitted) left of the RF line, ~2 mm from R19.
- **The antenna strip:** the bottom 10 mm, no copper, track, via or pour on either layer except the antenna's feed and
  end pad.
- The bottom layer: ground, with some tracks; none under the chip's body (a keep-out 7.6 x 7.6 mm round its centre),
  the crystal or the RF path. VDD3P3 still takes 2 vias (2.3 mm on the bottom, outside the chip) from the buck side to
  its filter; +3V3 runs on the bottom along the chip's and the crystal's edges.
- **Paste:** none on unfitted pads (DNP parts, the DIN sockets); the chip's exposed pad gets four 0.8 mm windows between
  its 9 vias (16 % of the pad: the windows keep 0.05 mm off the via rings, so no solder runs into the vias; *a low
  figure, unverified*: QFN guides ask 50-80 %; more paste would need plugged vias, a paid option).
- **Silk:** "MIDI OUT" over J2, "PWR IN / OUT 2" over J3, "BOOT" under SW1.
- Rules (JLC Economic, 2 layers): tracks 0.2 mm signal / 0.4..0.5 mm power, clearance 0.15 mm (JLC: 0.127), vias 0.3 /
  0.6 mm, the pours 0.2 mm from other nets.

## DRC and ERC (the last `make_fab.sh` run, 2026-11-11)

- **ERC:** 0 errors, 1 warning: the label ANT_END sits on one pin (AE1's end pad, unconnected by design).
- **DRC:** 0 errors, 2 warnings: the ANT_END track (the end pad's 1.8 x 2.5 mm extension) has an open end, by design;
  a 0.25 mm PIN_T stub on the bottom ends in the open (harmless copper).
  `fab/drc.rpt`. The one exception in `chip.kicad_dru`: the USB-C's own locating holes, as in hardware/tiny.
- **Nets:** `check_nets.py`: 248 board pads against the schematic, the pin rules (no strapping pin as an output, the
  senses on ADC1, MIDI_TX and PWR_HOLD on RTC GPIOs, USB on 19 / 20): 0 problems.
- **Routing (2026-11-12, Freerouting via `freeroute.py`):** keep-outs: no via within 0.8 mm of a DIN pad, DIN nets 0.5 mm
  from everything, no track in the antenna strip or within 0.5 mm of the edge, no bottom track under the chip block or
  the RF path. Freerouting repeats itself only on a byte-identical input: `make_fab.sh` (ROUTE=1) may route differently
  from the committed board and must pass the same checks; `ROUTE=0` keeps the committed routing. Measured on the committed
  board (routed tracks): VSYS C1 -> buck 13.8 mm; +3V3 buck -> chip pin 20 33.5 mm, -> L2 61.5 mm (0.25 mm, a chain through
  the +3V3 loads); VDD3P3 L2 -> pins 9.8 mm (drawn); CHIP_PU pin 4 -> C7 10.3 mm; USB D+ connector -> U5 9.0 mm; MIDI
  OUT 2's R20 3.8 mm of track from J6; 40 signal vias; BOOT and RST in one level row (the placer turns that group by 0 or
  180 only, room kept for their labels); no via in or touching an SMD pad; a GND via beside every SMD ground pad outside
  the chip block, placed before routing; the USB-C's shell legs 0.3 mm clear of tracks; vias 0.5 mm hole to hole. The
  exposed pad's paste: 16 windows, 35 % (was 16 %).
- **Old routing notes (route.py):** the chip's right-side pins (CHIP_PU, GPIO0, 1, 2, 4) are fanned out by hand in `make_board.py`
  (FANOUT: one lane each, 0.8 mm apart, each ending in a via); the RF path, its via fences and the exposed pad's 9 vias
  are drawn there too; route.py does the rest (the ground vias by each pad first, then the signals). Measured on the
  board: the USB pair and the crystal are routed before anything else (`PRIORITY`): **USB D+ 22.1 mm, D- 21.7 mm, both
  on the top layer, no vias** (the grid router lays them as two tracks, not as a coupled pair: their spacing is not
  controlled, and bottom tracks may cross under them: *unverified*, tolerable at full speed). **Crystal:** right under
  the XTAL pins, all on top, no vias: XTAL_P 0.5 mm to R17, R17 to Y1 3.9 mm, XTAL_N 6.0 mm. **RF:** LNA_IN 3.4 mm to L3; the 1.2 mm line from L3 to R18
  ~9 mm; the antenna feed 4.6 mm.

## Parts and cost

## Parts and cost (snapshot 2026-11-11; `fab/cost.md` has every line)

| | 5 boards | 10 boards |
|---|---|---|
| PCB (bare; 5: as in a quote, 10: an estimate) | $3.30 | $5.00 |
| Economic setup + stencil + nitrogen (as quoted) | $10.70 | $10.70 |
| Extended part types, 12 x $3.09 | $37.08 | $37.08 |
| Joints (223 a board x $0.0017) | $1.90 | $3.79 |
| Parts (with JLC's loss) | $33.31 | $58.57 |
| **Total / a board** (shipping left out: shared with other boards) | **$86.29 / $17.26 (EUR 14.84)** | **$115.14 / $11.51 (EUR 9.90)** |

EUR at an *assumed* 0.86 a dollar. **Extended parts that pay the $3.09 fee (11) and why none can be Basic:**

| LCSC | Part | Unit | Stock | Why Extended |
|---|---|---|---|---|
| C2913196 | ESP32-S3FN8 | $3.34 | 4 820 | the chip |
| C293767 | ANT3216LL00R2400A | $0.19 | 20 968 | the antenna |
| C254377 | 40 MHz crystal 8 pF +-10 ppm | $0.17 | 2 748 | JLC has no 40 MHz Basic crystal |
| C76769 | 2.0 nH 0402 (VDD3P3 filter) | $0.015 | 173 735 | no Basic RF inductors |
| C87532 | 3.3 nH 0402 (RF match) | $0.009 | 5 071 | same; **saving: fit 2.0 nH (C76769) at L3 too** (Espressif's range 2.0..3.0 nH) and drop this type: -$3.09, but not the XIAO value |
| C2765186 | USB-C 16P | $0.07 | 1.09 M | no Basic USB-C |
| C18185602 | SHOU HAN PJ-320A-4P DIP jack, through hole (x2) | $0.06 | 21 846 | no Basic jack; through hole since 2026-11-12 |
| C2873354 | TPS62842 buck | $0.40 | 44 968 | tiny's 60 nA buck |
| C2909397 | BL8506-40 detector | $0.14 | 3 061 | tiny's start-up detector |
| C51018335 | 2200 uF 10 V can, 10 x 10.5 (x2) | $0.38 | 1 972 | no Basic aluminium can |
| C88374 | Hirose U.FL-R-SMT-1 | $0.09 | 326 956 | the u.FL (fitted, the user's ruling) |
| C7519 | ST USBLC6-2SC6, USB ESD (U5) | $0.16 | 21 884 | no Basic USB ESD array (the user's ruling) |
| C2763866 | BL8506-30NRM, 3.0 V reset supervisor (U4) | $0.14 | 4 962 | no Basic supervisor (fitted 2026-11-12) |
| C17976 | 68 ohm 1206 (x4: R14 // R26, R21 // R27) | $0.01 | 75 823 | no Basic 1206 resistor |
| C474307 | 4.7 ohm 2010 0.75 W (R5) | $0.04 | 3 975 | no Basic 2010 resistor |
| C79317 | 2.2 uH 0806 | $0.11 | 6 902 | the buck's inductor; no Basic part of that rating |

**One BOM line per LCSC code** (`make_assembly.py`): JLC unticked the jacks when MIDI OUT and POWER IN were two lines
for the same jack part ("Multiple lines in the BOM have been matched to the same part"); they are one line now (J2,J3),
and the script stops if a code ever lands on two lines. **Fee-free:** C1550 (1 pF) is a *preferred* Extended part; everything else is Basic (0402 / 0603 / 0805 R and C, the
diodes, the LED, the button). **Not ordered:** the DNP parts (C24, C25, C26, R18, J5, J6). Stock is >= 50 for every
line (the lowest: the crystal, 2 748).

## Case (`case/`)

See [case/README.md](case/README.md) (mini 1's text). **Mini 2 (2026-11-13):** 66 x 68 mm board read from the PCB, so the case is 8 mm longer; the lid's top wall has LINE IN's slot at x = 16.4 mm (6.9 mm wide, 5.8 mm over the board, rounded top, open at the lid's bottom edge, like the side jacks'); J7's block, pegs and legs are in the collision checks. **The no-DIN version passes every check** (`check_base`, `check_lid` with 0.3 mm clearance, caps, light pipe, pads); all 5 STLs are one closed solid, 0 open edges (`check_stl.py`). **2026-11-13, the LED window:** `mini_case.scad` already took LED1's place from the board (`place_args.py`: the middle of its pads), but the *printed* lid (`mini_lid.stl`) was a mirror image of the lid (a `mirror([1, 0, 0])` in `lid()` on top of the turn-over): the LED's window and LINE IN's slot came out at W - x. Fixed (the lid is only turned over about x now); `make_stl.sh` runs `case/check_led.py`, which fails unless the STL's window is at LED1 (and not at its mirror image); renders `case/renders/section_led.png` (a section through LED1's centre: the light pipe over the LED) and `case/renders/lid_printed_roof.png`. **The DIN version still fails `check_base`** (its walls against the through-hole jacks' legs and the DIN plates' slots; it failed on mini 1 too): open. New renders `case/renders/closed_top.png` (the top wall with the hole), `closed.png`, `open.png`, sections.

## Unverified, all in one place

- **The RF match and the antenna on this board** (no VNA here; XIAO values; YAGEO's eval-board geometry): range and
  RSSI unmeasured. The antenna's feed side (JLC footprint pad 1 = S1) is taken from the drawing: check JLC's preview.
- **The crystal's frequency offset** (8 pF crystal, 10 pF caps, the stray capacitance guessed): Espressif's TX-tone test
  on the first boards; change C9 / C10 if it is off.
- **The slow fall of the bank with PWR_HOLD high** (README "Start-up"): no supervisor fitted; the firmware must let go
  of PWR_HOLD before the bank sags.
- **USB at full speed over a cut ground:** D+ / D- are matched (README "Layout") but cross bottom-layer tracks.
- **The DIN sockets' pin numbering** in JLC's footprint (README "DIN"), their height and axis (the case's DIN base).
- **The jacks in JLC's BOM step:** the earlier warning was two BOM lines for one part (fixed: one line now); check the
  row is ticked in the next quote.
- **MIDI OUT 2** (README "MIDI OUT 2"): the loop current is a calculation (D6's drop at 5 mA read off nothing but its
  family's curve), not measured; into real gear untested.
- The electrolytics' polarity (pad 1 = +) and every part's rotation: JLC's placement preview.
- The case's fit (rabbet, posts, openings); every height in the mockup.
- **The 2200 uF cans and the DIN sockets' iron room (short of 0.8 mm):** C1 / C2 were moved 1 mm lower (the board
  1 mm taller, the antenna strip kept 10 mm): their courtyards are now **0.33 mm** from DIN pad 1 (J5) and pad 5 (J6)
  (the cans' bodies ~0.6 mm), unused pins. Lower would put the cans' pads into the antenna strip.
- **R5 (4.7 ohm, VBUS):** a 2010 (C474307, 0.75 W, Extended); its pulse rating against 45 mJ in ~20 ms unmeasured
  (no anti-surge part found at JLC in stock).
- **R14 // R26, R21 // R27:** 2 x 68 ohm 1206 (C17976, Extended: no Basic 1206 resistor at JLC); 34 ohm, not 33.
- **C5 22 uF 0603 6.3 V:** its capacitance at 3.3 V DC bias is well below 22 uF (unmeasured).
- Stock and prices: one snapshot (`fab/parts-snapshot.json`); fees from earlier quotes (`fab/cost.md`).
- Everything from hardware/tiny that is unverified there (the cans' leakage, the boot charge, the buck at light load,
  R5's pulse rating, MIDI at 3.3 V into real gear).

## Next steps

1. User reviews the renders, `chip.kicad_pcb` in KiCad, this README and `fab/cost.md`.
2. The main session gets a real JLCPCB quote (Economic, 5 boards) and checks the placement preview: AE1's feed side,
   Y1's pin 1, the cans' polarity, the jacks.
3. First boards: Wi-Fi RSSI against a XIAO; the crystal's frequency offset (Espressif's TX tone test); the boot from a
   MIDI OUT (the slow-fall question above).
