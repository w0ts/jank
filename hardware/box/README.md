# jank box: the printed box of the jank board (modular v5c)

The box for the **modular** board in [`../pcb`](../pcb/README.md) (`jank-pcb-mod`, v5c), **87.0 x 79.5 mm**.

**The board's top (the lid side):**
- the XIAO, the 0.96" 128 x 64 OLED (HS96L03W2C03), OK, BACK, the encoder, the beat LED;
- the four MIDI jacks (XKB PJ-320D-B-SMT), each **beside** its DIN;
- the four mono CLOCK / RESET jacks (XKB PJ-320A) on the side edges;
- the A/B switches (SW2 surface-mount, SW3..SW5 through-hole).

**Its bottom:** the four DINs (HOOYA DIN-504B-M10), 19.6 mm deep, so the board stands about 20 mm above the floor on
them.

**Outputs:** OUT2, OUT3, OUT4 on the front, **2, 3, 4 left to right** seen from the front with the lid up. OUT1 is on
the back beside the XIAO's USB-C and the power USB-C.

OpenSCAD, parametric, one build. Every STL is one closed manifold solid.

> **Stage: designed and checked in OpenSCAD only (2026-11-09).** Nothing printed, no part measured. Every number marked
> *unverified* comes from a drawing or an estimate.

**First requirement (the user's ruling, 2026-11-08):** plugging and unplugging loads the box, not the solder joints, on
every jack and every DIN. **Connectors at the wall (the user's ruling, 2026-11-08/09):** see "Connectors at the wall"
below.

![open](renders/open.png)

## Files

| File | What |
|---|---|
| `jank_box.scad` | The box, the lid, the caps, the knob, the wall test strip, the checks (`part` picks the output) |
| `make_stl.sh` | Exports the STLs, runs the checks, renders the PNGs, runs `../check_stl.py` |
| `stl/jank_box_box.stl`, `stl/jank_box_lid.stl` | The base and the lid, in print orientation |
| `stl/jank_box_button_cap.stl` (print 2), `stl/jank_box_slider_cap.stl` (print 3), `stl/jank_box_slider_cap_smd.stl` (print 1), `stl/jank_box_knob.stl` (print 1) | OK / BACK caps, the A/B slider caps (SW3..SW5, SW2), the encoder knob |
| `stl/jank_box_front_test.stl` | A strip of the front edge as built: one DIN in its slot (front cheek, slot, back cheek) and the jack beside it in the skirt; try the parts first |
| `renders/open.png`, `box.png`, `front.png`, `back.png`, `left.png`, `right.png`, `lid_underside.png`, `lid_top.png`, `caps.png` | The box with the board (lid lifted), the empty base, the four sides, the lid as printed, the lid's top with its labels and caps, the closed box |

```
sh hardware/box/make_stl.sh               # about 15 s
```

## Size

**Outer: 89.0 x 81.5 x 32.0 mm** (X x Y x Z, lid included). The box is stepped at the board's line on the front and
back:
- **Lid (skirt):** 89.0 x 81.5, i.e. 1.0 + board 87.0 + 1.0 and 1.0 + board 79.5 + 1.0.
- **Base:** 89.0 x **80.1**. Its front and back faces stand 0.3 mm *proud* of the board's edges, 0.7 mm inside the
  lid's faces, so the DIN plates come to 1.3 mm behind them. Its sides are flush with the lid's.
- **Z:** floor 2.0 + 0.1 + DIN 19.6 + board 1.6 + **7.2** to the lid's inside + lid 1.5.

**Standing out (not counted):**
- the MIDI jacks' bushings 1.5 mm (front and back), the side jacks' bushings 1.0 mm;
- the XIAO's USB-C 0.5 mm (back);
- the knob 13.3 mm, the button caps 3.0 mm, the slider caps 1.0 mm above the lid.

**Parts to print:** box, lid, 2 button caps, 3 + 1 slider caps, 1 knob (and `front_test` first).

## Connectors at the wall, the DINs in slots

**The lid (skirt):** on every edge **the board's edge is 1.0 mm inside the lid's outer face** (`edge_in`). The skirt
comes down from the lid to the board's top. It is 2.3 mm thick: 1.0 outside the board, 1.3 over it.

**The base, front and back: the DINs slide into slots in the wall** (the user's rulings, 2026-11-09: "slots in which
the MIDI DIN slides/slots in", "slots in the wall", "not a wall at the back"). These walls are **5.2 mm deep**, from
0.3 outside the board's edge to 4.9 inside it (thickened 2026-11-09, the user: "can you make the wall thicker?").
Round each DIN plate the wall carries:
- **a front cheek 1.2 mm thick** (the plate's face 0.1 behind it), whose window leaves the plate's face open except its
  edges;
- **a 2.0 mm slot** (the plate's 1.8 + 0.1 a face, and 0.1 at its side edges), from the floor up to the board;
- **a back cheek 2.0 mm thick**, whose opening lets the DIN's narrower body through;
- **posts behind the back cheek**, beside the bodies, **4.6 mm wide** (the 5.2 between two bodies less 0.3 a side) and
  **9.2 mm deep** (from the back cheek to the bodies' backs, 14.1 behind the board's edge), from the floor to 1.7 mm
  under the board (the jacks' pegs and leads). The outermost posts stop at the side walls;
- **1.0 mm 45-degree fillets** where the back cheek and the posts meet the floor (under the bodies, 2.4 above it).

The plate slides **down** into its slot as the board is dropped into the base. Its far edge stands on the floor (0.1).
There is no lip or wall behind the DIN's body any more.

**How much of the plate the slot holds** (the cheeks' rims, the same in front and behind):
- **1.3 mm on each side edge.** The body is 1.55 narrower a side; 0.2 is kept clear of it.
- **2.2 mm at the far edge.** The body is 2.4 shorter there; 0.2 is kept clear.
- **Nothing on the board-side edge.** The plate meets the board there.

So the front window and the back opening are 18.3 x 17.4 mm.

**Between two neighbouring DINs** (pitch 23.0): the plates are 2.1 mm apart, so the wall's **rib in that gap is 1.9 mm**
(0.1 clearance a side). Each plate's edge has 1.3 mm of cheek over it on that rib, in front and behind. Behind the
plates the bodies are 5.2 mm apart: the posts fill that, 0.3 clear of each body.

**OUT1** at the back is the same.

**OUT2's plate** is flush with the board's left edge. Its slot runs into the side wall, leaving 0.9 mm of wall outside
it.

**Over the jacks' pegs and leads** (above the rib, under the board), the rib and the back cheek stop 1.7 mm below the
board.

**The base's sides** are 1.9 mm (1.0 outside the board, 0.9 under it).

**The board** is clamped between the skirt (1.3 over it) and the base's walls (front and back walls under the edge,
the side walls 0.9 under it). It is screwed at H1..H4.

**Where each connector face ends up** (on the board model in the box):

| Connector | Its face | Distance from the outside | Why |
|---|---|---|---|
| **DIN plate** (front, back) | 1.0 behind the board's edge | **1.3 mm inside the base's face** (2.0 inside the lid's) | the front cheek 1.2 + 0.1 |
| **MIDI jack body** (front, back) | 0.5 past the board's edge | **0.5 mm inside the lid's face**, the bushing **1.5 mm proud** | the skirt's 0.4 mm web in front of the body takes the pull-out |
| CLOCK / RESET jack body (sides) | on the board's edge | 1.0 mm inside, the bushing 1.0 mm proud | a 0.9 mm web |
| XIAO USB-C | 1.5 past the board's edge | 0.5 mm proud | |
| Power USB-C (J1) | on the board's edge | 1.0 mm inside the lid's face; its legs in a notch of the base's back wall | |

**Why the step:** the jack's front is 1.5 mm in front of the DIN plate's face on the footprint. One flat face would
leave either the jacks with no web in front of them, or the DINs 2.0 mm in. The skirt (jacks) and the base (DINs)
therefore have their own faces, 0.7 mm apart. The base's face stands 0.3 mm proud of the board's edge above it.

**The DIN body, scaled off HOOYA's drawing** (the coordinator, 2026-11-09: 33.5 px/mm against the dimensioned 20.9,
19.6, 11.8 and 1.8; ±0.2 mm, *not a caliper*):
- 17.8 mm wide (the plate overhangs it 1.55 a side);
- 17.2 mm high from the board's side (the plate overhangs it 2.4 at the far edge, none at the board's side);
- 13.1 mm deep from the plate's front.

The back cheek's rims depend on these numbers. The checks use them for the DIN's body (`din_body`).

**Print and fit:**
- **The skirt's jack slots** are open downwards: round at the bushing's axis, 5.2 mm, 2.5 mm above the board.
- **The XIAO's opening** is open full height through the lid's edge. **J1's opening** is 12.5 x 5.15, open downwards.
- **The DIN slots** are open at the top, so the base prints without supports.

## Bracing

| Part | Pull-out | Push-in | Down / up | Sideways |
|---|---|---|---|---|
| **DIN** (HOOYA: plate 20.9 x 19.6 x 1.8) | the plate's front on the **front cheek** (1.3 a side, 2.2 at the far edge; 0.1) | the plate's back on the **back cheek** (the same rims; 0.1) | the plate's far edge on the floor (0.1); the board above | the plate's side edges in the slot (0.1) |
| **MIDI jack** (PJ-320D-B-SMT, 6.0 x 12.2 x 5.0, front 0.5 past the edge) | the body's front on the skirt's 0.4 web (0.1) | the body's back on the **lid's cap** (back stop 1.6, 0.15) | the board below, the cap's roof above | the cap's cheeks; the bushing in its slot |
| **Side jack** (PJ-320A) | the body's front on the skirt's 0.9 web (0.1) | one **cap per side wall** round both jacks (6.7 apart) | as above | as above |
| **The board** | | | clamped all round between the base's wall (0.9 under it) and the skirt (1.3 over it); four M2 x 20 into webbed columns | the walls (0.1 at the connectors) |

**Unverified:**
- The jacks' webs (0.4 at the MIDI jacks) take a 3–20 N pull together with the jacks' own SMD pads and pegs; *not
  computed*.
- The DIN's cheeks are 1.2 (front) and 2.0 (back) mm thick, the posts 4.6 x 9.2: how far they flex under a 5 kg pull is
  *not computed*.

## Lid

**Height:** inside 7.2 mm above the board, 1.5 mm thick. The XIAO's 7.0 sets it.

**Openings** (KiCad coordinates, y from the back):

| Opening | Centre | Size | Note |
|---|---|---|---|
| OLED window | **38.39, 34.36** | **22.76 x 11.88** | the 0.96" module's viewing area (22.74 x 11.86, board y 28.43..40.29) + 0.02, as before; the glass top (5.15) 2.05 under the lid |
| BACK, OK | 69.15 / 78.75, 25.45 | 6.0 round | the button caps' 5.4 stems |
| Encoder | 73.9, 38.7 | 7.0 round | under the 14 mm knob |
| LED | 33.0, 4.2 | 3.5 x 3.7 | a shroud to 0.3 over the LED, a 0.6 mm skin over it (`led_skin`): print the lid translucent |
| SW2 (OUT1, surface-mount) | 17.5, 8.1 | **6.0 x 3.5**, along x | the cap (3.15 square) and its 1.5 travel along x, 0.175 round |
| SW3..SW5 (OUT2..OUT4) | 10.45 / 33.45 / 56.45, 60.5 | 3.5 x 6.0, along y | the cap (3.15 x 3.65) and its 2.0 travel |
| Screws H1..H4 | (83.8, 3.2), (83.8, 76.3), (3.2, 26.0), (3.2, 34.0) | 2.4 | **4 x M2 x 20 self-tapping** through the lid, a 5.0 spacer, the board, 9.7 into a column |

**Under the lid:** the skirt, the jack caps, the spacers, the LED shroud, and **four posts onto the OLED module**.
- **The module:** 27.3 x 27.8 mm at board x 24.74..52.04, y 22.56..50.36, on its 1 x 4 header 2.5 mm up.
- **The posts:** each is Ø4.0 and stands 0.2 mm above the module's PCB at its four Ø2.5 holes (x 26.74 / 50.04, y
  24.56 / 48.36). There is no glass there: the glass spans y 27.43..44.33.
- **The pins:** each post ends in a **Ø2.1 pin**, 0.6 mm into its hole. The pins hold the module on its header against
  shifting and lifting.
- **The front edge:** it has nothing under it. A 2.5 mm foam pad under it is still a good idea.

## Printed caps and knob

| Part | Shape | Fit | Print |
|---|---|---|---|
| **Button cap** (2) | a 5.4 mm stem 3.0 above the lid; an **8.0 mm flange, 1.0 thick, 0.3 under the lid**; a **blind bore 3.45 mm**, 3.6 deep, on the plunger | +0.05 on the 3.4 plunger: a light press once printed | flange on the bed |
| **Slider cap** (3, SW3..SW5) | 3.15 x 3.65 block, 1.0 above the lid, a **pocket 1.55 x 2.05 x 1.3** | +0.05 on the 1.5 x 2.0 handle | top on the bed |
| **Slider cap, SMD** (1, SW2) | 3.15 x 3.15 block, 1.0 above the lid, a **pocket 1.55 x 1.55** to the knob's top (3.2 + 3.0) | +0.05 on the 1.5 knob; **the knob's length along its travel taken as 1.5: unverified** | top on the bed |
| **Encoder knob** (1) | 14 x 12, a **D bore 6.1 / 4.6** (the shaft's 6.0 / 4.5, *unverified*), 10 deep, a pointer groove | 0.1 play | top on the bed |

## Markings

0.4 mm deep, Liberation Sans Bold, capitals 4.0 mm. **Controls on the lid's top, plugs on the walls, each once.**
- **Lid:** BACK and OK above their buttons, BPM under the knob, A / B at the ends of each slot (SW2's left and right),
  "jank" at the left.
- **Front (on the skirt):** "2", "3", "4" right of each jack.
- **Back:** "1 / IN" beside OUT1's jack (skirt); "USB" under the XIAO and "PWR" under J1 (base's wall).
- **Sides (on the skirt):** "CLK" and "RST" beside their jacks.
- **Caveat:** which end of a slot is A is not verified (`ab_a_end`, `ab_a_end_x`). Check each switch with a meter.

## Checks (`make_stl.sh`, last run 2026-11-09, OpenSCAD 2026.10.11, manifold)

The board model is `../pcb/mockup-mod.stl` (v5c), with the buttons, the A/B switches (body plus the knob over its
travel) and the OLED redrawn.

| Check | Must be | Result |
|---|---|---|
| `check_box`: the base against the board and every part | empty | empty |
| `check_lid`: the lid (skirt, caps, shroud, spacers) against the board and every part | empty | empty |
| `check_lidbox`: lid against base | empty | empty (the board is between them) |
| `check_drop`: the lid lowered straight down over the board and the caps | empty | empty |
| `check_board_drop`: the board lowered straight into the base, every DIN plate sliding down its slot | empty | empty |
| `check_text`: labels clear of every opening, slot, cavity and keep-out, inside the edges | empty | empty |
| `check_caps`: caps at rest / pressed / both ends of travel against the lid, the board, the shaft; each knob in its pocket | empty | empty |
| `brace_caps`, `brace_pull` (each DIN plate moved 0.15 out meets the front cheek), `brace_push` (0.15 in meets the back cheek), `brace_pull_jack` (MIDI jacks on the webs), `brace_push`, `brace_trs`, `brace_floor`, `brace_lid`, `brace_ledge` | contact | contact |
| `brace_side_pull` / `brace_side_push`, each side jack | contact | 8 of 8 |
| `check_stl.py` (box, lid, button cap, both slider caps, knob, front_test) | OK | OK: one body, 0 open edges, 0 non-manifold each |

**Negative controls collide as they should:**
- `gap_out = -0.05` (a jack body into its web);
- `band_in = 2.0` (the skirt onto the DIN legs);
- `slot_play = -0.05` (a slot narrower than the plate);
- `din_rim = [1.6, 2.6]` (the back cheek onto the DIN's body, in `check_board_drop`);
- `standoff_d = 6.0`.

## Print

| Part | Orientation | Supports |
|---|---|---|
| box (base) | floor on the bed | none: the DIN slots and windows are open at the top |
| lid | top face on the bed | none: the skirt's slots and openings are open at its free edge |
| caps, knob | as listed above | none |
| front_test | standing on its foot | none |

PETG or PLA, 0.2 mm layers, 4 perimeters, 25 % infill.

## Assembly (who does what)

1. **User solders the board** (board README).
2. **User prints** `front_test.stl`. Check that a DIN plate slides down its slot (snug, not tight) and does not move in
   or out, and that a plug seats with the plate's face 1.3 mm in. Check that a jack's bushing comes through the skirt's slot with its body against the web.
3. **User puts** a 2.5 mm foam pad under the OLED module's front edge (the edge away from its header), and presses the button caps onto OK and BACK and the slider
   caps onto the four A/B knobs.
4. **User lowers** the board straight into the base. The DIN plates slide down their slots onto the floor, and
   the board's edge comes to rest on the base's wall and the four columns.
5. **User lowers** the lid straight down. The skirt comes down over the bushings and onto the board's edge, and the
   caps come through their openings.
6. **User screws** 4 x M2 x 20 self-tapping at H1..H4, snug only. Then the knob on the encoder.

## Unverified, all in one place

- **The DINs:**
  - the body behind the plate (17.8 x 17.2 x 13.1) is **scaled off the drawing**, not measured: the back cheek's rims
    (1.3 / 2.2) rest on it;
  - a plug seating with the plate's face 1.3 mm in;
  - the slot's 0.1 mm play against the printer's tolerance (a tight print: ream or set `slot_play`);
  - the cheeks' stiffness (0.7 / 1.0 mm).
- **The jacks:**
  - the PJ-320D-B-SMT's body (the mockup's 6.2 x 12.2 x 5.0) and its 5.0 bushing;
  - the 0.4 mm web's strength.
- **SW2:** its knob's length (1.5 taken), its travel direction, which end is A.
- **The board:**
  - its outline against the 0.1 mm gaps at the connectors (JLCPCB ±0.2: the board's edges are clamped, not fitted
    between walls, so the gap only limits how far the board can shift before a connector bears);
  - the mockup's positions (the PCB files are the board session's).
- **The OLED module:** its PCB 1.0 and glass 1.65 (top at 5.15), its holes' 2.5 and positions, and the glass's
  position, from the board session's numbers, not a part in hand.
- **Other:** the XIAO's 7.0 height, the encoder's 6.5 and 20, the buttons' 3.5 body, the cap press
  fits, the EC11 D flat, M2 into PLA, the USB overmold sizes, the font.
