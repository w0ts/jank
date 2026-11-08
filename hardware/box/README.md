# jank box: the printed box of the jank board (modular)

The box for the **modular** board in [`../pcb`](../pcb/README.md) (`jank-pcb-mod`, the user's choice 2026-11-08),
**84.0 x 79.5 mm**: v5 plus a 12.5 mm band in front of the screen row, and **four mono jacks on the side walls**
(CLOCK and RESET left, CLOCK and RESET right). The board's **top is the lid side**:
the XIAO, the 0.91" OLED, OK, BACK, the encoder, the beat LED, the four jacks (XKB PJ-320A) and the four A/B switches.
Its **bottom** carries the four DINs (HOOYA DIN-504B-M10), 19.6 mm deep, so **the board stands about 20 mm above the
floor on them**. **Every output has both connectors**: the jack above the board line, the DIN below it. OUT2, OUT3 and
OUT4 are on the front wall, left to right seen from the front with the lid up; OUT1 is on the back wall, beside the
XIAO's USB-C and the power USB-C. The side jacks are on the left and right walls. There is one build: one box, one lid, and a test strip. OpenSCAD, parametric;
every STL is one closed manifold solid.

> **Stage: designed and checked in OpenSCAD only (2026-11-08).** Nothing printed, no part measured. Every number marked
> *unverified* comes from a drawing or an estimate.

**First requirement (the user's ruling, 2026-11-08):** *the 3D print must brace the DIN sockets to relieve the stress
from the PCB.* Plugging and unplugging loads the box, not the solder joints, on every jack and every DIN.

![open](renders/open.png)

## Files

| File | What |
|---|---|
| `jank_box.scad` | The box, the lid, the wall test strip, the checks (`part` picks the output) |
| `make_stl.sh` | Exports the STLs, runs the collision, drop, brace and label checks, renders the PNGs, runs `../check_stl.py` |
| `stl/jank_box_box.stl`, `stl/jank_box_lid.stl` | The printable parts, in print orientation |
| `stl/jank_box_button_cap.stl` (print 2), `stl/jank_box_slider_cap.stl` (print 4), `stl/jank_box_knob.stl` (print 1) | The printed caps for OK / BACK, the A/B slider caps, the encoder knob |
| `stl/jank_box_front_test.stl` | A 21.1 mm strip of the front wall: one jack U-slot over one DIN bore with its pad behind; try the plugs first |
| `renders/open.png`, `box.png`, `front.png`, `back.png`, `left.png`, `right.png`, `lid_underside.png`, `lid_top.png`, `caps.png` | The box with the board in it (lid lifted), the empty box, the four walls, the lid as printed (its underside), the lid's top with its labels and the caps in place, the closed box with its caps and knob |

```
sh hardware/box/make_stl.sh               # about 8 s
```

## Size

**Outer: 87.8 x 83.3 x 32.0 mm** (X x Y x Z, lid included).
- **X:** side walls 1.8 + 0.1 + board 84.0 + 0.1 + 1.8. The side walls carry jacks now: 1.8 mm so the bushings stand
  0.1 proud, and 0.1 mm to the board's side edges (was 0.3: *the board's width tolerance must allow it*).
- **Y:** front wall 1.8 + 0.1 + board 79.5 + 0.1 + back wall 1.8.
- **Z:** floor 2.0 + 0.1 + DIN 19.6 + board 1.6 + **7.2** to the lid's inside + lid 1.5.

**Standing above the lid (not counted in Z):** the knob 13.3 mm (its top), the button caps 3.0 mm, the slider caps
1.0 mm.

**Parts to print:** box, lid, 2 button caps, 4 slider caps, 1 encoder knob (and `front_test` first).

## The walls: two rows, and the wall decision

The board model is the board's own `../pcb/mockup-mod.stl`, top up, the DINs hanging. The jack's front is on the board's
edge, and its bushing (5.0 mm, *unverified*) stands only **2.0 mm** past it. The DIN's plate front is **1.0 mm** behind
the edge (the v5 footprint). The two rows are handled differently:

- **Above the board (the jacks):** the wall is **1.8 mm**, its inside 0.1 mm off the board's edge. The bushing stands
  **0.1 mm proud** of the outside, so a plug seats against it.
- **Below the board (the DINs):** **the same flat outside, and the wall 1.0 mm thicker on the inside**. Under each DIN a
  **DIN pad** fills the 1.0 mm to the plate (its face 0.1 mm off it), from the floor up to the board's bottom. The
  outside stays one flat face; the DIN's face is **2.9 mm behind it** (wall 1.8 + 0.1 + the 1.0 the footprint sets it in).
- **Why not a step outside:** a flat outside needs no overhang. The pad's top doubles as the ledge the board's edge
  rests on.

**The DIN's bore is closed now.** Its top is 1.7 mm below the board's bottom, and the plate slides down behind the pad
when the board goes in. It is a round **16.0 mm** bore (`din_bore`, *unverified* against the user's plugs; HOOYA's own
opening is not on its sheet) centred **9.7 mm below the board** (`din_axis`). It prints as a teardrop cut flat 0.8 mm
above the circle (a 5 mm bridge). **Between two neighbouring bores** (pitch 21.1) the wall is 5.1 mm. The plates
themselves are 0.2 mm apart behind the pads (0 at the worst tolerance).

**The jack's U-slot** is **5.2 mm**, round at the bottom around the bushing's axis 2.5 mm above the board's top, and open
to the top of the wall. The lid's **key** closes it: 4.9 wide, with a 5.3 mm half-round end.

## The side jacks (modular)

Four XKB PJ-320A used mono (CLOCK, RESET), on the top, their fronts on the side edges. Positions are y from the back
edge: **left wall CLOCK 39.85, RESET 46.55; right wall CLOCK 53.8, RESET 60.5**. Each is built like a front jack:
- **Slot:** a **5.2 mm U-slot** in the 1.8 mm side wall, round at the bottom round the bushing's axis 2.5 mm above the
  board's top, open to the wall's top. The bushing (Ø5.0, *unverified*) stands 0.1 mm proud of the outside.
- **Key:** a **lid key** (4.9 wide, 5.3 half-round) closes the slot.
- **Pull-out:** the body's front on the wall's inside (0.1 mm).
- **Push-in:** onto a **cap hanging from the lid**. The two jacks of a wall are only 6.7 mm apart, so their caps are one
  block per wall with one cavity round both bodies: cheeks outside them, a back stop behind them, 1.6 mm above the board.
- **Nearby parts:** H3 / H4's columns and spacers (y 26 and 34) are clear of the left jacks (J11's body starts at y
  36.75); `check_box` and `check_lid` include them.
- **Checks:** `brace_side_pull` and `brace_side_push` are run for each of the four jacks.

## Bracing

| Part | Pull-out | Push-in | Down / up | Sideways |
|---|---|---|---|---|
| **DIN** (HOOYA: plate 20.9 x 19.6 x 1.8, body to 13.6, from the drawing rev A1 2019-07-10) | the plate's front on the **DIN pad** round the bore: about 210 mm2 | the body's back on a **lip** from the floor (1.4 mm, 20.9 wide, to 5.0 mm under the board) | the plate's far edge on the floor (0.1); the board above | the plug's shell in the bore |
| **Jack** (XKB PJ-320A: body 6.2 x 12.2 x 5.0, from the drawing rev A1 2025-03-03) | the body's front on the wall's inside (0.1) | the body's back on the **lid's cap**: a saddle hanging from the lid round the body, back stop 1.6 mm | the board below; the cap's roof above (0.15) | the cap's cheeks; the bushing in the U-slot |
| **The board** | | | **clamped at every connector edge** between the DIN pads' tops (below) and the lid's ribs (above, beside each jack); four M2 standoffs | the walls (0.1) |

**Details:**
- **Lid caps:** they stop 1.6 mm above the board, because the DINs' legs (about 1.5 mm, *estimate*) stand on the top
  there. Their back stops end 0.2 mm before the A/B switches.
- **DIN lip:** it stops 5.0 mm under the board. The DIN's 2-pin row goes up into the board 15.8 mm behind the edge,
  inside the lip; it is *unverified* that those legs stay within 5.0 mm of the board.
- **Design gaps:** every bearing surface is 0.05 mm off (`contact`), so the checks stay clean; in print that is contact.

## Lid

**Height:** the inside is **7.2 mm above the board's top**, 1.5 mm thick. The tallest top part, apart from the encoder's
body and the buttons, is the XIAO on its headers (7.0, *estimate*).
- **OLED:** the glass top (5.1) is 2.1 mm under the lid.
- **Encoder:** its body (6.5, *unverified*) is 0.7 mm under the lid.
- **A/B knobs (5.0):** they end 2.2 mm under the lid's inside; **printed slider caps** bring them 1.0 mm above the lid
  (below).
- **Buttons:** OK and BACK (9.5) would stand only 0.8 mm out of the lid; **printed button caps** bring them 3.0 mm out
  (below). The user's answer (2026-11-08): "we can 3D print something".

**Openings** (KiCad coordinates, y from the back):

| Opening | Centre | Size | Note |
|---|---|---|---|
| OLED window | 45.0, 28.55 | 24.4 x 7.6 | the viewing area; the glass 2.1 under the lid, untouched |
| BACK (SW6), OK (SW1) | 69.15 / 78.75, 25.45 | **6.0** round | the button cap's 5.4 stem, 0.3 a side |
| Encoder shaft | 73.9, 38.7 | 7.0 round | body under the lid (v5b: 2.2 mm forward) |
| LED window | 33.0, 4.2 | 3.5 x 3.7 | a light shroud from the lid down to 0.3 mm over the LED (cut back at the XIAO), the lid's top **skin of 0.6 mm over it** (`led_skin`): print the lid translucent, or set `led_skin = 0` |
| A/B slots | (24.3, 9.0) OUT1, (10.45 / 31.55 / 52.65, 60.5) OUT2..OUT4 | **3.5 x 6.0** | all four; the slider cap (3.15 x 3.65) and its 2.0 travel, 0.175 round it |
| XIAO's USB-C | 45.2, back edge | 12.5 wide notch | its 7.0 mm opening reaches above the walls (the socket's top is 7.0 up), so the lid's back edge is notched over it |
| Screws H1..H4 | (80.8, 3.2), (80.8, 76.3), (3.2, 26.0), (3.2, 34.0) | 2.4 | **4 x M2 x 20 self-tapping**: through the lid, a 5.0 mm spacer to the board, the board, 9.7 mm into a webbed column from the floor (pilot 1.7, 11 deep) |

**Under the lid:**
- **OLED rib:** over the module's far end (x 26.0..27.6, no glass there), 0.2 mm above the module's board (taken as
  3.9 above the board's top, *unverified*). It stops the free end from rising. The module has nothing under its far
  end: **User puts a 2.5 mm foam pad under it** (the board README's suggestion).
- **USB-C tongues:** the power USB-C (J1) is a U from its rounded bottom to the top of the wall. A tongue of the lid
  closes it down to the 12.5 x 7.0 opening. The tongue stays 0.55 mm behind the board's edge.
- **J1's shell legs:** they stand 0.55 mm past the board's edge under it (mockup). The back wall has a 0.7 mm relief
  there.

## Printed caps and knob

| Part | Shape | Fit | Print |
|---|---|---|---|
| **Button cap** (2) | a 5.4 mm stem, chamfered 0.5 on top, 3.0 mm above the lid; an **8.0 mm flange, 1.0 thick, 0.3 under the lid** at rest (it cannot come out through the 6.0 hole); a **blind bore 3.45 mm**, 3.6 deep, its floor on the plunger's top | on the HRO's 3.4 mm plunger: **+0.05 nominal**; printed small holes come out 0.1 to 0.2 under, so it is a light press. Ream or reprint at 3.5 if too tight | flange on the bed: the bore's ceiling bridges 3.45 mm |
| **Slider cap** (4) | a 3.15 x 3.65 mm block from 0.2 above the switch body to 1.0 above the lid (6.0 tall), a **pocket 1.55 x 2.05 x 1.3** over the handle | on the 1.5 x 2.0 handle: **+0.05 each way** (`slider_fit`); printed, a press | top on the bed, the pocket up |
| **Encoder knob** (1) | 14 mm across, 12 tall, a **D bore 6.1 mm with its flat at 4.6** (the shaft's 6.0 / 4.5, *unverified*), 10 deep, a pointer groove (1.0 wide, 0.6 deep) on the flat's side | **0.1 mm play** both ways; pushed on until the shaft's top meets the bore's floor: the knob's bottom is then 1.3 mm over the lid | top on the bed (the groove is the first layers) |

**Checks** (`check_caps`, must be empty):
- the caps against the lid at rest, with the buttons pressed 0.3 mm, and with the sliders at both ends of the 2.0 mm
  travel;
- the button caps (at rest and pressed) against the board model, the plunger excepted;
- the slider caps at both ends against the board, the handle excepted;
- the handle inside its pocket at each end;
- the knob against the D shaft (modelled with its 4.5 flat over the top 10 mm), the encoder's body, the board and the
  box.

**`brace_caps`** (must touch): a button cap lifted 0.35 meets the lid, so the flange keeps it in.

**Negative controls fail as they should:**
- a flange 0.1 into the lid (`btn_cap` gap -0.1);
- `slider_fit = -0.1`;
- a knob flat of 4.4;
- a knob bore of 5.9.

## Markings

0.4 mm deep, Liberation Sans Bold, capitals 4.0 mm, stems 0.84 mm. **Controls on the lid's top, plugs on the walls, each
once.**
- **Lid:** BACK and OK above their buttons, BPM under the knob's edge, an A and a B at the two ends of each A/B slot,
  and "jank" (6.0 mm capitals) at the left.
- **Front wall:** "2", "3", "4" right of each jack's slot.
- **Back wall:** "1 / IN" beside OUT1's jack slot, "USB" under the XIAO's opening, "PWR" under J1's.
- **Side walls:** "CLK" and "RST" beside their slots, each on the side away from the other jack of its pair. Left
  wall: CLK towards the back, RST towards the front. Right wall: the same. Each text reads left to right seen from
  outside that wall.
- **A/B caveat:** which end of a slot is A is not verified (`ab_a_end`, default "back"). Check one switch with a meter.

## Checks (`make_stl.sh`, last run 2026-11-08, OpenSCAD 2026.10.11, manifold)

The model is `../pcb/mockup-mod.stl`, with the buttons (a 3.5 mm body and a 3.4 mm plunger to 9.5), the A/B switches (a
3.5 mm body and the knob swept over its travel) and the OLED (its board to 3.9, the glass panel to 5.1) redrawn. The
mockup draws all three as full-height blocks.

| Check | Must be | Result |
|---|---|---|
| `check_box`: box against the board and every part | empty | empty |
| `check_lid`: lid against the board and every part (XIAO, OLED, encoder, buttons, knobs, LED, jacks, DIN legs) | empty | empty |
| `check_lidbox`: lid against box (lifted 0.01) | empty | empty |
| `check_drop`: the lid with its keys, caps, ribs and tongues lowered straight down from 22 mm | empty | empty |
| `check_board_drop`: the board with every part lowered straight in from 26 mm (the DIN plates behind the pads, the bushings into their U-slots) | empty | empty |
| `check_text`: every label 0.6 mm clear of the openings, inside the edges, 0.3 mm above the lid's underside | empty | empty |
| `brace_pull`: DINs and jacks moved 0.15 mm outwards meet the pads / walls | contact | contact |
| `brace_push`: DINs moved 0.2 mm inwards meet the lip | contact | contact |
| `brace_trs`: jacks moved 0.2 mm inwards meet the lid's caps | contact | contact |
| `brace_floor`: DINs moved 0.15 mm down meet the floor | contact | contact |
| `brace_lid`: the board lifted 0.1 mm meets the lid's ribs and spacers | contact | contact |
| `brace_ledge`: the board lowered 0.1 mm meets the pads and the columns | contact | contact |
| `brace_side_pull` / `brace_side_push`, each side jack: moved 0.15 out meets its wall, 0.2 in meets the lid's cap | contact | contact (8 of 8) |
| `check_caps`: button caps (rest, pressed 0.3), slider caps (both ends), knob, against the lid, the board, the shaft | empty | empty |
| `brace_caps`: a button cap lifted 0.35 meets the lid | contact | contact |
| `check_stl.py` (box, lid, the caps, the knob, front_test) | OK | OK: one body, 0 open edges, 0 non-manifold, 0 misoriented each |

**Negative controls collide as they should:**
- `bush_slot = 4.9` (`check_board_drop`).
- `face_h = 6.8` (the lid on the XIAO).
- `din_in = 0.85` (a pad into its plate).
- `oled_rib_gap = -0.3`.
- `cap_stop = 1.0` (a cap on the DIN legs).
- `standoff_d = 5.5` (H3's column on R21).
- `j1_legs = [1.5, 0.7]` (the back wall on J1's legs).

## Print

| Part | Orientation | Supports | Notes |
|---|---|---|---|
| box | floor on the bed | none | the jack slots and USB U's are open upwards; the DIN bores are flat-topped teardrops (5 mm bridge) |
| lid | top face on the bed | none | labels and the LED skin are the first layers; keys, caps, ribs, tongues, spacers, the shroud and the OLED rib grow up |
| button cap | flange on the bed | none | the bore's ceiling bridges 3.45 mm |
| slider cap | top on the bed | none | |
| knob | top on the bed | none | the pointer groove is the first layers |
| front_test | standing, the pad behind, on a foot | none | |

PETG or PLA, 0.2 mm layers, 4 perimeters, 25 % infill; the lid translucent if `led_skin > 0`. Printed holes come out
0.1 to 0.2 mm small: ream the 5.2 mm U-slots to a slip fit on the bushings if tight.

## Assembly (who does what)

1. **User solders the board** in the board README's order: the parts under the XIAO, then its headers and the XIAO;
   the parts under the OLED, then its header and the module. Each jack goes first, its legs cut flush on the bottom,
   then its DIN from the bottom.
2. **User prints** `front_test.stl` and tries a DIN plug through the 16.0 mm bore into a DIN held behind the pad (it must
   go home with the face 2.9 mm in), and a 3.5 mm jack's bushing in the slot. If one does not fit, Claude changes
   `din_bore` / `bush_slot` / `conn_t` and runs `make_stl.sh` again.
3. **User puts** a 2.5 mm foam pad under the OLED's far end, **presses a button cap onto each of OK and BACK** (flange
   down) and **a slider cap onto each A/B handle**.
4. **User lowers** the board, top up, straight into the box, OUT1 to the back. The DIN plates slide down behind the
   pads and stand on the floor, the bushings drop into their U-slots, the XIAO's socket into its notch, and the board
   comes to rest on the pads and the four columns.
5. **User lowers** the lid straight down. The keys close the jack slots, the caps come down over the jacks, the tongue
   closes J1's U, and the shaft, the button caps and the slider caps come through their openings.
6. **User screws** 4 x M2 x 20 self-tapping at H1..H4, snug only. Then the knob onto the encoder's shaft, its groove on the flat's side.

## Unverified, all in one place

- **The DIN bore 16.0 mm** against the user's plugs, the face 2.9 mm in (the plug shell's length). HOOYA's opening is
  not on its sheet.
- **The HOOYA's body** behind the plate: its height and width are taken as the plate's 19.6 x 20.9. Its 2-pin legs are
  assumed to stay within 5.0 mm of the board, inside the lip. Its axis of 9.7 mm is read from the plate's board-side
  edge.
- **The PJ-320A:** the bushing's 5.0 mm (measured on the sheet); how much pull-out the 0.5 mm faces beside the slot
  take. The rest goes into the lid's cap and the board through its pegs and tabs.
- **Modular:** the board's width against the 0.1 mm side clearance; the side jacks' bushings (5.0); the board
  README's modular list.
- **Both connectors on one footprint:** the board README's dry fit, which decides whether the DIN body clears the jack's
  cut legs.
- **The OLED:** its board's top at 3.9 (where the rib is) and the glass panel's position (30 x 11.56 from 5.5 mm off the
  pin end); its header's 2.5 mm plastic.
- **Heights:** the XIAO's 7.0 (sets the lid), the encoder's 6.5 and 20, the buttons' body 3.5, the A/B knob 5.0, the
  DINs' and the standing resistors' legs (1.5), the board's 1.6.
- **C4** (100 µF, not fitted, on the bottom): not in the mockup. If fitted, check it against H1's column.
- **The markings:** which end of a slot is A; the font OpenSCAD reports as Liberation Sans Bold (it substitutes
  silently).
- **The caps:** the press fits (+0.05 nominal) depend on the printer; the EC11's D shaft (4.5 across the flat over its
  top 10 mm) is not read from a drawing.
- **Other:** M2 self-tapping into PLA/PETG; the USB-C plugs' overmold size; the LED through the 0.6 mm skin; printer
  hole shrink. None of it printed or fitted.
