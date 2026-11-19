# jank-mini-3: the LCD-2 module board (placed and routed 2026-11-18; in review, not ordered)

A removable add-on board for a Waveshare ESP32-S3-LCD-2 (sku 30306). The LCD-2 plugs in on its own two 1 x 14
male headers; jank-mini-3: it sits in our notch, its PCB level with ours, and two bridge strips (strip/) join its
rows to our J15 / J16. How it is held is open (mots: one side of the strips soldered). No screws. The plan,
cost, sketches and bench tests are in `worktrees/lcd2-addon-scratch/` (README.md there).

| file | what |
|---|---|
| `parts.py` | the one parts table: every part, value, LCSC code, pad -> net, the GPIO map, the LCD-2's rows |
| `make_schematic.py` | writes `lcd2.kicad_sch`, `lib/chip.kicad_sym`, `fab/intended-nets.json` (mini 2.2's generator) |
| `check_nets.py` | the KiCad netlist against parts.py, plus the pin rules (LCD-2 rows, ADC1, glitch / pull rules, pull-downs, boost < clamp) |
| `sim/` | `power.py`, `clamp.py`, `host.py`, `midi_cv.py`, `linein_esd.py` and their `.txt` results |
| `lcd2-schematic.pdf` | the sheet (one label per pin: it reads as a netlist) |

Rebuild and check (KiCad 10, ngspice 47, Python with numpy):

```bash
cd hardware/jank-mini-3 && python3 make_schematic.py && kicad-cli sch erc --severity-all -o fab/erc.rpt lcd2.kicad_sch && kicad-cli sch export netlist --format kicadxml -o fab/lcd2.net.xml lcd2.kicad_sch && python3 check_nets.py fab/lcd2.net.xml fab/intended-nets.json
```

```bash
cd hardware/jank-mini-3/sim && for s in power clamp host midi_cv linein_esd; do python3 $s.py > /dev/null || echo FAIL $s; done
```

USB (mots, 2026-11-17): the LCD-2's own USB-C sits at the case's side wall and is used directly for a computer or a
charger. For host mode a standard short cable outside the box joins it to J1 (TO ESP, USB-C, VBUS and CC open); D+ / D-
run straight to J4 (TO DEVICE, USB-C), whose VBUS carries only the host signal (R93 / D16 from U21).

OUT gives hosted gear only a 680 ohm host signal on VBUS, never power (mots, 2026-11-17: the small cell is not drained). Bus-powered gear needs its own power or a powered hub.

## jank-mini-3 layout (mots, 2026-11-18)

Board 95.5 x 82 mm (93 x 74 until round 3: +2.5 mm wide for the mid-mount USB-C pair, +4 mm above and below the
LCD-2 for the bands beside it, mots 2026-11-18), the LCD-2 in a notch open to the left edge (its PCB level with ours,
LCD-2 centred). Two bridge strips (strip/: one 1 x 14 socket for the LCD-2's pins, beside it 14 plated holes for our
male rows J15 / J16, C44526 cut to 1 x 14, 11.5 mm of pin below the plastic) join its pins to ours, under the board.
**Assembly (mots):** solder J15 / J16 into our board (the 3 mm tails in our holes, the plastic under the board); push
a strip's holes onto our pins and the LCD-2's pins into its socket; seat the LCD-2 level in the notch; then solder our
pins in the strip's holes from below (~1.4 mm stand out: trim). The LCD-2 stays removable; the strips stay on our board.
**The cell (mots, 2026-11-19, after review 6A R1): on the LCD-2's own MX1.25 connector, not on this board.** One EEMB
LP103454 2000 mAh 1S LiPo (34.5 x 56 x 10.6 mm, its own PCM, a JST PHR-02 plug) through an adapter lead mots makes: a
PH2.0 pigtail that mates with the cell's plug, soldered to an MX1.25 (Molex PicoBlade 51021 type) 2-pin pigtail, red to
red, heat-shrunk; MX1.25 pigtail colours vary: meter which pin is + and check it lands on the LCD-2's pin 1 (VBAT) before
the first plug-in. The LCD-2 charges it (ETA6098, ~2 A, about 1 C) on its own copper, and gives VBAT on P2.14, which our
5 V boost uses on battery (~0.2 A, VBAT 0.3 mm here). The cell lies on the case floor under the right part (x ~50-84.5),
its lead end toward +y or -y; the lead runs to the LCD-2. (J18, a PH2.0 socket on our VBAT, was tried and left again:
it put the whole charge through the LCD-2's unmeasured trace to P2.14.) **The cell must have its own PCM:** nothing on
the LCD-2 or on ours protects it. Under the board, hand-soldered by mots (fab/lcd2-hand.csv): the DINs, the jacks,
J15 / J16. On top, by JLC: every SMD part and both USB-C. Walls: USB-C TO ESP (J1) + TO DEVICE (J4) side by side
(13 mm), right of the two TRS jacks, on the top wall, mid-mount (Shou Han TYPE-C 16P CB1.6 073, C2906290, each in a
9.3 x 6.75 mm notch of our edge, 1.56 mm above our top: the plug's overmold stays under the box top with a 2 mm lid),
MIDI (2 TRS + 2 DIN) top, CLOCK / RESET right, LINE IN bottom under its LCD-2 pins; the LCD-2's own USB-C on the left
wall. One 24 mm cross D-pad: no part under it.
The GPIO map is assigned from the layout (tools/gpio_opt.py, the joint search tools/layout_opt.sh) within
check_nets' pin rules.

```bash
cd hardware/jank-mini-3 && sh tools/route_try.sh 82 1      # place + route one attempt into .t/route-h82/ (never touches lcd2.kicad_pcb)
```

### Review round 1 (2026-11-18: worktrees/mini3-review-A, -B) and what changed

- Fixed: the cells' 2 A path at signal width (BNEG / FET_D 1.0 mm, CELL nets 0.8 mm, routed first, 0.6 mm drill
  power vias, the DW01A's ground pins and C106 a thin tap; check_board item 7 checks widths); LINE IN plug detect
  (LIN_DET on IO18, BTN_DOWN on IO21; the WEAK rule in check_nets / gpio_opt); a 3 A fuse per cell (F1 / F2) and
  R164 470 ohm (reversed cell); the boost's output caps beside U18's VOUT / GND pins (U18 turned, L5 under SW); the
  cut-out as a notch (no 0.2 mm sliver); thermal reliefs on through-hole pads; USB-C 13 mm apart; silk text >= 1 mm.
- Open, mots: the strip's soldered side and J15 / J16's header (soldering to our board needs ~12 mm of pin below the
  plastic, not on LCSC); the cells' charge rate (the LCD-2 charges at 2 A, about 2 C for 450 / 550 mAh cells: check
  the cells' datasheet; R15 on the LCD-2 sets it, bench test 2 measures it); meter the DIN-504B-M10 and PJ-320A pad
  numbering before ordering; meter every cell lead before plugging it in (red = the "+" pin 1).
- Not done: the case for this layout, the strip's fab files.

### Review round 2 (2026-11-18: worktrees/mini3-review2-A, -B) and what changed

- Fixed: CLOCK / RESET on the PJ-320A's D (tip) and A (sleeve) again (KiCad's PJ320D footprints name them; review 2A);
  the boost's output loop (no bottom track under U18 / C100 / C101, 2 GND vias at each ground pad); 3 GND vias at
  U23's source pin (the cells' return); C106 beside U22; thermal spokes 0.4 mm; the Gerbers, drill and CPL share the
  board's lower-left origin (make_assembly refuses a board without it; review 2B R2); the USB-C mid-mount, board
  +2.5 mm (mots; review 2B R3); check_board item 3 checks the notch, item 7 is a resistor network (a thin loop that
  carries little current passes, one that carries more per mm than the minimum track fails).
- Ground loops: as mini 2.2 (its README, "Ground loops"): MIDI OUT isolated at the receiver's opto, the shield / sleeve
  grounded here only; CLOCK / RESET sleeves through 100 ohm (R120 / R121), LINE IN's through 100 ohm (R39) with the
  sleeve-sensing input; USB TO ESP / TO DEVICE on GND directly (no isolator). Hum-free: the cell with Wi-Fi / Link, no USB to an
  earthed computer while CLOCK / RESET or LINE IN go to earthed gear.

### Review round 3 (2026-11-18: worktrees/mini3-review3-A, -B) and what changed

- Placement for routing (mots): the USB-C pair right of the TRS jacks, the TRS drivers by their jacks, the protector
  pair side by side, the 5 V aux group split, the board 82 mm; U5 turned (its data pins face the USB-C); the pin map
  re-assigned by tools/gpio_opt.py for length, crossings and straightness. Routed clean (route.py: +3V3 at signal
  width, a repair round on whole nets with every track locked; tools/bridge_islands.py, stitch_island.py, widen_hot.py).
- Fixed after the review: the USB-C notch to the maker's drawing (6.75 deep, dog-bone reliefs, slots 0.6 x 1.4 / 1.8,
  signal pads trimmed to 0.2 mm from the edge: lcd2.kicad_dru, paste on the 4 shell legs: pin-in-paste); D3 / D17
  SS34F in SMAF (1.06 mm, was SMA up to 2.4 mm against the lid's 2.1 mm); the boost's feedback divider R134 / R135 at
  U18's FB pin, no bottom track under L5; top labels off the D-pad cap and off part bodies; the bridge strip's fab
  files (strip/fab/strip-gerbers.zip, pin-1 end marked); check_nets' J1 VBUS rule for the new part's pads.
- Firmware (firmware/main): MIDI TX inverted in the UART (BOARD_MIDI_TX_INVERT), MIDI OUT 2 on GPIO 9, the mini 3's pins
  in board.h; HOST_EN (GPIO 17) is defined but nothing drives it on this board yet: host mode stays off.
- mots, before ordering: trim every through-hole lead on the bottom parts to 1 mm or less (the tails come up 1.4 to
  1.9 mm through the top, under a lid 2.1 mm above it); check JLC's placement preview for the rotations of D16 / U19,
  Q1 / Q10-Q13, D7 / D10 / D11 / D14 and the USB-C (EasyEDA footprints, origins not cross-checked). On the order: board
  thickness 1.6 mm (the mid-mount USB-C needs it) and the remark "THT-solder the 8 USB-C shell legs" (the stencil's
  pin-in-paste gives them too little solder; otherwise mots fills the slots from below).
- Decided, mots: the LCD-2 is not modified (R15 stays: 2 A charge); the cell is one 1S LiPo on the LCD-2's own MX1.25 through a
  PH2.0 -> MX1.25 adapter lead (2026-11-19, after review 6A R1); the strips are soldered to our board; Claude designs the case after the reviews (the USB-C
  overmolds need one ~26 x 7 mm opening reaching into the lid).

### Review round 4 (2026-11-19: worktrees/mini3-review4-A, -B) and what changed

- mots: the cells, sockets, fuses, sharing resistors and the DW01A + FS8205A protector left this board (one cell on
  the LCD-2's own connector); VBAT now only feeds our 5 V boost (route.PWR / check_board: 0.3 mm minimum).
- The bridge strip: one socket (the LCD-2) and a row of plated holes (our long pins), review 4B W1: with two sockets the
  LCD-2's pins would have reached 0.6 mm into theirs.
- J4's ESD (U5) and its VBUS parts (C28, D16, Rp R160 / R161) moved right behind J4 (review 4A R6); the boost's whole
  feedback chain (R134 / R135 / R154 / R136) at U18's FB pin (4A R5); R167 4.7k on D3's anode against its leakage on
  battery (4A R7); the VBUS sense divider 10k / 15k, 3.0 V at 5 V (4A R8); no paste on unfitted parts (4B N3); the
  USB-C slots 0.6 x 1.4 / 1.2 (the legs hold the part) and its signal pads from 0.20 mm off the notch (4B R1).
- J4 carries VBUS on one pad pair only (A4 / B9); the other pair is open: a plug ties its four VBUS pins together, both
  orientations reach it (review 4A checked it).
- mots, later the same day: 2000 / 1100 mAh cells come with PH2.0 plugs, the LCD-2 has MX1.25, so ONE PH2.0 socket (J18)
  came back on VBAT / GND (no protector: the cell's own PCM); VBAT back to 0.75 mm (route.PWR 1.0 mm, check_board 7).
- Firmware: the low-cell cutoff (the LCD-2's own battery divider on GPIO5) is for the firmware + webpage session.

### Review round 5 (2026-11-19: worktrees/mini3-review5-A, -B) and what changed

- A W1 (the README's PH2.0 cell on the LCD-2's MX1.25): answered by J18 above. A R1 (2 A): mots, 2026-11-19: the LCD-2
  stays as is; a computer's USB port gives far less, a 2-3 A charger the full 2 A (J18, PH2.0, is rated 2 A).
- B R1: the USB-C signal pads start 0.075 mm from the notch floor (lcd2.kicad_dru 0.07; leads on 0.42 mm of pad, was
  0.30): JLC may ask about copper near the edge, answer "as designed, mid-mount USB-C". B R3: a fourth M2 hole in the
  right part (W - 4, H / 2), between CLOCK and RESET. B R4: the strip's holes 1.15 mm, pads 1.9 mm; its "1" / "14" 1.0 mm.
- Silk: the pin-1 arrows point at pin 1 on either side; D3 / D17's lines 0.15 mm; the date. fab/lcd2-hand.csv lists the
  strip set. The DIN symbol's pin names (pad 4 = DIN 4, pad 2 = DIN 5; the copper was right), the PDF's stale notes
  (V5A 5.00 V, clamp 5.83 V), sim/power.py + host.py with R88 / R89 10k / 15k, C29 and R167 (results: unplug VBUS_B
  0.148 -> 0.071 V, the LCD-2's VCC dip 1.7 -> 0.6 ms; host unchanged).
- mots, before ordering: 1 oz copper (16 tracks at 0.10 mm); JLC's high-precision outline (0.2 mm behind the USB-C's
  body, B R2; for an order JLC assembles only that helps, a dry fit is for hand assembly, 7B N3); on arrival, D+ / D- / CC continuity on J1 / J4.
- mots, before soldering the strips (A R3; round 6A N1: Waveshare's pinout graphic confirms J15 = P1, J16 = P2, pin 14
  at the USB end): on the bare LCD-2 with USB in, the pin at our J15.14 spot reads 5 V and J16.1 3.3 V (the strip's silk
  "1" is a label only). The cell's polarity (A R2, 6A N2): the adapter on the cell, meter its MX1.25 pins: + goes to the LCD-2's pin 1 (VBAT).
  Scope GPIO9 and GPIO13 at boot (A: MIDI OUT 2's pin; 6A N8: the S3 drives it low for an instant, harmless).
- The case (Claude, after the reviews): NOT a post under the D-pad centre (B R3 proposed (64.75, 41)): the cell lies
  there (EEMB LP103454, 34.5 x 56 x 10.6 mm, x ~50-84.5 on the floor). The new M2 hole at (91.5, 41) is clear of it;
  with H4 the D-pad sits inside the holes' support (6B N6: ~0.4 mm, ~550 microstrain at 10 N); a case rib under the D-pad
  can still back it, the cell's lead end toward +y (the DIN bodies reach the floor down to y 16.3, 7B N6).

### Review round 6 (2026-11-19: worktrees/mini3-review6-A, -B) and what changed

- 6B R1: J18's "+" lay under its own housing: now a "+" on top beside pad 1 (where mots solders) and one on the bottom
  behind the housing; J18's silk lines 0.15 mm. Stale texts (6A N9): the PDF's VBAT note, parts.py's old cell comment,
  midi.c's comment on the S3's pin routing (6A N8), check_board's VBAT width print.
- 6A R1, mots: J18 LEFT the board (VBAT back to 0.3 mm); the cell on the LCD-2's own MX1.25 through an adapter lead.
  The review's text, for the record: 6A R1 (the LCD-2's side): the 2 A charge now runs through the LCD-2's own copper to P2.14 (width unknown) and its
  charge inductor L3 is rated 1.9 A. Ours: J16.14 -> J18.1 42.8 mohm, 1.0 mm, ~7 C at 2 A. Bench test 2 with the cell on
  J18: the real charge current, the drop from the LCD-2's empty battery pad to J18.1, temperatures at P2.14 / L3 / J18 /
  the cell after 20 min with the lid on; above ~60 C on a part or 45 C on the cell, charge only from 1 A sources.
- mots, handling (6A N3-N4): the LCD-2's pins are live whenever its cell is plugged in (J16.13 GND is 0.74 mm from J16.14):
  unplug the cell before seating or removing the LCD-2 and before soldering J15 / J16 or the strips. Plugging the cell in
  makes a short inrush (7A N1, re-simulated with the cell on the MX1.25: 13-30 A for 30-50 us, mostly the LCD-2's own
  capacitors; ours add 1-3 %): plug it 3 times on the bench and check it boots
  each time (a touchy PCM may latch off; USB in releases it). Nothing on our board limits a VBAT short or charges by
  temperature (6A N5, N10: the cell's own PCM only; no NTC on the LCD-2's charger).
- mots, the cell (6B N3, N5): EEMB's drawing shows the leads leaving one 34.5 mm end, about 60 mm long (scaled, not
  measured); on the floor that end faces +y or -y; with the adapter lead (10 cm pigtails) it reaches the LCD-2's MX1.25 under the
  LCD-2. PH2.0 is 2 A with AWG 24: check the leads and the pigtails.
- mots, the order (6B N1, N4): remark "do not pull back J1 / J4 pad copper near the notch: mid-mount USB-C, as designed";
  measure one DIN-504B-M10's depth below the board (LCSC 19.6 mm, a HOOYA drawing 20.9 mm) before the case.
- Firmware (for the firmware + webpage session, 6A N7): the LCD-2's battery reading on IO5 sits at its end of that path:
  ~40-80 mV low on battery through the MX1.25, the adapter and the cell's PCM (7A N1, estimated 0.1-0.2 ohm; safe for a
  cutoff), ~0.2-0.4 V high while charging at 2 A (do not judge "full" from it on USB). The cutoff drives GPIO4 (V5_EN) low
  and holds it through deep sleep, else R131 keeps the boost on: ~0.2-0.25 mA from the cell (7A N4).

### Review round 7 (2026-11-19: worktrees/mini3-review7-A, -B) and what changed

- 7A: 0 will-not-work, 0 risky. 7B: one risky, the cell's charge through the LCD-2's own MX1.25 (PicoBlade-type contacts
  are usually ~1 A, the charge ~2 A). mots, 2026-11-19: a cell on the LCD-2's own socket is Waveshare's stock design, not
  this board's; kept as a bench note: bench test 2 logs the real charge current and, after 20 min on a 2 A charger with
  the lid on, the temperatures of the MX1.25 housing, the adapter's splice, the LCD-2's L3 and the cell (above ~60 C on a
  part or 45 C on the cell: charge from 1 A sources). The adapter's pigtails 26 AWG or thicker (7A N2).
- The cell's polarity (7A N3): find the LCD-2's MX1.25 "+" contact by a continuity beep to P2.14, unpowered; the adapter's
  + goes there. A reversed cell is survivable for our board, not for the LCD-2.
- Silk: labels avoid signal vias (the CLK_M via printed through "UP", 7B N5); "TO ESP" / "TO DEVICE" below the drawn
  data pair. J18's leftovers (silk branch, parts row, comments) removed (7A N6, 7B N4).
- mots, on arrival: look at the first DIN-504B-M10's underside; V5M runs on the bottom under J5 and V5A / V5M under
  J13 / J14 behind solder mask only; a metal shield there wants Kapton (7A N7). Keep metal off the case floor under the
  board: VBAT runs 55 mm on the bottom over the cell area (7B N9). The tab between J1 and J4 is 0.67 mm of board at the
  edge: the case opening takes the plugs' side loads (7B N7). The maker's drawing says "THICKNESS 0.80MM": order 1.6 mm
  as designed (7B N8). The front shell slots are 1.40 mm for 1.20 mm legs (deliberate since 4B; the maker shows 1.80).

### Review round 8 (2026-11-19: worktrees/mini3-review8-A, -B): clean, the board is FINAL

- 8A and 8B: 0 will-not-work, 0 risky. With round 7 (clean under mots's ruling that the cell on the LCD-2's own MX1.25
  is Waveshare's stock design), two successive clean rounds: mots's rule for final is met. The board was not changed
  after round 8; only these notes and docs/lcd2.md's GPIO43 line.
- mots, the order remarks: "mill the USB-C notch reliefs with a cutter of 1.0 mm or less" (8B N2: 0.9 mm T-bones; a
  cutter of 1.6 mm or more would leave FR4 under the body's corner); "do not pull back J1 / J4 pad copper near the
  notch"; high-precision outline; 1.6 mm; 1 oz; THT-solder the 8 USB-C shell legs.
- mots, before ordering: U19 CJ431 (C3113) pin order 1 REF / 2 K / 3 A, as JSCJ's CJ431D / E and CJ432 sheets give it
  (8A N3; the C3113 sheet itself not read): check it on LCSC's pin picture; the first power-up from a current-limited
  supply (a swapped part would short V5A through R145).
- If the board is ever touched again (8A N1, N2; not needed to work): a 10 uF at L5.1 with its own GND via (the boost's
  input ripple runs ~36 mm to C99), a 100 nF at U21's pins 14 / 7 (C105 sits 13 mm away); bench: scope L5.1 and J16.14.
  8B N3: the PJ307C slots are 0.8 x 1.5 mm (JLC wants 2:1), hand-soldered, at worst a little loose.
