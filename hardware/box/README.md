# jank box: the printed box of the jank board (v5)

The box for the board in [`../pcb`](../pcb/README.md), **v5: 84.0 x 67.0 mm**. The board's **top is the lid side**:
the XIAO, the 0.91" OLED, OK, BACK, the encoder, the beat LED, the four jacks (XKB PJ-320A) and the four A/B switches.
Its **bottom** carries the four DINs (HOOYA DIN-504B-M10), 19.6 mm deep, so **the board stands about 20 mm above the
floor on them**. **Every output has both connectors**: the jack above the board line, the DIN below it. OUT2, OUT3 and
OUT4 are on the front wall, left to right seen from the front with the lid up; OUT1 is on the back wall, beside the
XIAO's USB-C and the power USB-C. There is one build now: one box, one lid, and a test strip. OpenSCAD, parametric;
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
| `stl/jank_box_front_test.stl` | A 21.1 mm strip of the front wall: one jack U-slot over one DIN bore with its pad behind; try the plugs first |
| `renders/open.png`, `box.png`, `front.png`, `back.png`, `lid_underside.png`, `lid_top.png` | The box with the board in it (lid lifted), the empty box, the walls, the lid as printed (its underside), the lid's top with its labels |

```
sh hardware/box/make_stl.sh               # about 7 s
```

## Size

**Outer: 88.6 x 70.8 x 32.0 mm** (X x Y x Z, lid included).
- **X:** side walls 2.0 + 0.3 + board 84.0 + 0.3 + 2.0.
- **Y:** front wall 1.8 + 0.1 + board 67.0 + 0.1 + back wall 1.8.
- **Z:** floor 2.0 + 0.1 + DIN 19.6 + board 1.6 + **7.2** to the lid's inside + lid 1.5.

**Standing above the lid (not counted in Z):** the encoder's shaft 11.3 mm, OK and BACK 0.8 mm.

## The walls: two rows, and the wall decision

The board model is the board's own `../pcb/mockup.stl`, top up, the DINs hanging. The jack's front is on the board's
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

## Bracing

| Part | Pull-out | Push-in | Down / up | Sideways |
|---|---|---|---|---|
| **DIN** (HOOYA: plate 20.9 x 19.6 x 1.8, body to 13.6, from the drawing rev A1 2019-07-10) | the plate's front on the **DIN pad** round the bore: about 210 mm2 | the body's back on a **lip** from the floor (1.4 mm, 20.9 wide, to 5.0 mm under the board) | the plate's far edge on the floor (0.1); the board above | the plug's shell in the bore |
| **Jack** (XKB PJ-320A: body 6.2 x 12.2 x 5.0, from the drawing rev A1 2025-03-03) | the body's front on the wall's inside (0.1) | the body's back on the **lid's cap**: a saddle hanging from the lid round the body, back stop 1.6 mm | the board below; the cap's roof above (0.15) | the cap's cheeks; the bushing in the U-slot |
| **The board** | | | **clamped at every connector edge** between the DIN pads' tops (below) and the lid's ribs (above, beside each jack); four M2 standoffs | the walls (0.1 / 0.3) |

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
- **A/B knobs (5.0):** they end 2.2 mm under the lid's inside, reachable with a pen tip through their slots.
- **Buttons:** OK and BACK (9.5) stand **0.8 mm** out of the lid. The 13 mm HRO code (S) would stand 4.3 mm out (*the
  user's call*).

**Openings** (KiCad coordinates, y from the back):

| Opening | Centre | Size | Note |
|---|---|---|---|
| OLED window | 45.0, 28.55 | 24.4 x 7.6 | the viewing area; the glass 2.1 under the lid, untouched |
| BACK (SW6), OK (SW1) | 69.15 / 78.75, 25.45 | 4.0 round | plunger 3.4 |
| Encoder shaft | 73.9, 36.5 | 7.0 round | body under the lid |
| LED window | 33.0, 4.2 | 3.5 x 3.7 | a light shroud from the lid down to 0.3 mm over the LED (cut back at the XIAO), the lid's top **skin of 0.6 mm over it** (`led_skin`): print the lid translucent, or set `led_skin = 0` |
| A/B slots | (24.3, 9.0) OUT1, (10.45 / 31.55 / 52.65, 48.0) OUT2..OUT4 | 2.0 x 4.5 | all four: every channel has its jack |
| XIAO's USB-C | 45.2, back edge | 12.5 wide notch | its 7.0 mm opening reaches above the walls (the socket's top is 7.0 up), so the lid's back edge is notched over it |
| Screws H1..H4 | (80.8, 3.2), (80.8, 63.8), (3.2, 26.0), (3.2, 34.0) | 2.4 | **4 x M2 x 20 self-tapping**: through the lid, a 5.0 mm spacer to the board, the board, 9.7 mm into a webbed column from the floor (pilot 1.7, 11 deep) |

**Under the lid:**
- **OLED rib:** over the module's far end (x 26.0..27.6, no glass there), 0.2 mm above the module's board (taken as
  3.9 above the board's top, *unverified*). It stops the free end from rising. The module has nothing under its far
  end: **User puts a 2.5 mm foam pad under it** (the board README's suggestion).
- **USB-C tongues:** the power USB-C (J1) is a U from its rounded bottom to the top of the wall. A tongue of the lid
  closes it down to the 12.5 x 7.0 opening. The tongue stays 0.55 mm behind the board's edge.
- **J1's shell legs:** they stand 0.55 mm past the board's edge under it (mockup). The back wall has a 0.7 mm relief
  there.

## Markings

0.4 mm deep, Liberation Sans Bold, capitals 4.0 mm, stems 0.84 mm. **Controls on the lid's top, plugs on the walls, each
once.**
- **Lid:** BACK and OK above their buttons, BPM under the encoder's shaft, an A and a B at the two ends of each A/B slot,
  and "jank" (6.0 mm capitals) at the left.
- **Front wall:** "2", "3", "4" right of each jack's slot.
- **Back wall:** "1 / IN" beside OUT1's jack slot, "USB" under the XIAO's opening, "PWR" under J1's.
- **A/B caveat:** which end of a slot is A is not verified (`ab_a_end`, default "back"). Check one switch with a meter.

## Checks (`make_stl.sh`, last run 2026-11-08, OpenSCAD 2026.10.11, manifold)

The model is `../pcb/mockup.stl`, with the buttons (a 3.5 mm body and a 3.4 mm plunger to 9.5), the A/B switches (a
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
| `check_stl.py` (box, lid, front_test) | OK | OK: one body, 0 open edges, 0 non-manifold, 0 misoriented each |

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
3. **User puts** a 2.5 mm foam pad under the OLED's far end.
4. **User lowers** the board, top up, straight into the box, OUT1 to the back. The DIN plates slide down behind the
   pads and stand on the floor, the bushings drop into their U-slots, the XIAO's socket into its notch, and the board
   comes to rest on the pads and the four columns.
5. **User lowers** the lid straight down. The keys close the jack slots, the caps come down over the jacks, the tongue
   closes J1's U, and the shaft, the buttons and the knobs come through their openings.
6. **User screws** 4 x M2 x 20 self-tapping at H1..H4, snug only. Then the knob on the encoder.

## Unverified, all in one place

- **The DIN bore 16.0 mm** against the user's plugs, the face 2.9 mm in (the plug shell's length). HOOYA's opening is
  not on its sheet.
- **The HOOYA's body** behind the plate: its height and width are taken as the plate's 19.6 x 20.9. Its 2-pin legs are
  assumed to stay within 5.0 mm of the board, inside the lip. Its axis of 9.7 mm is read from the plate's board-side
  edge.
- **The PJ-320A:** the bushing's 5.0 mm (measured on the sheet); how much pull-out the 0.5 mm faces beside the slot
  take. The rest goes into the lid's cap and the board through its pegs and tabs.
- **Both connectors on one footprint:** the board README's dry fit, which decides whether the DIN body clears the jack's
  cut legs.
- **The OLED:** its board's top at 3.9 (where the rib is) and the glass panel's position (30 x 11.56 from 5.5 mm off the
  pin end); its header's 2.5 mm plastic.
- **Heights:** the XIAO's 7.0 (sets the lid), the encoder's 6.5 and 20, the buttons' body 3.5, the A/B knob 5.0, the
  DINs' and the standing resistors' legs (1.5), the board's 1.6.
- **C4** (100 µF, not fitted, on the bottom): not in the mockup. If fitted, check it against H1's column.
- **The markings:** which end of a slot is A; the font OpenSCAD reports as Liberation Sans Bold (it substitutes
  silently).
- **Other:** M2 self-tapping into PLA/PETG; the USB-C plugs' overmold size; the LED through the 0.6 mm skin; printer
  hole shrink. None of it printed or fitted.
