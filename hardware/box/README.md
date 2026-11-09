# jank box: the printed box of the jank board (modular v5c)

Part of the **[jank device](../../README.md)** · the other half: [the PCB](../pcb/README.md)

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
| `stl/jank_box_button_cap.stl` (print 2), `stl/jank_box_ab_tab.stl` (print 3), `stl/jank_box_ab_tab_smd.stl` (print 1), `stl/jank_box_ab_grip.stl` (print 4), `stl/jank_box_knob.stl` (print 1) | OK / BACK caps, the A/B tabs (SW3..SW5, SW2) and their grips, the encoder knob |
| `stl/jank_box_front_test.stl` | A strip of the front edge cut from the real base and lid round OUT3: its DIN in its slot (front cheek, slot, back cheek), its jack's round hole (half in the base's tongue, half in the skirt), the joint with the front catch; the two halves joined by end posts. Try the parts first |
| `renders/open.png`, `box.png`, `front.png`, `back.png`, `left.png`, `right.png`, `lid_underside.png`, `lid_top.png`, `joint.png`, `tab.png`, `tab_print.png`, `caps.png` | The box with the board (lid lifted), the empty base, the four sides, the lid as printed, the lid's top with its labels and caps, the joint (cut at the front catch and OUT3's jack, the lid lifted 6 mm), OUT3's A/B tab cut along its travel at both ends, the tab parts as printed, the closed box |

```
sh hardware/box/make_stl.sh               # about 15 s
```

## Size

**Outer: 89.0 x 81.5 x 32.0 mm** (X x Y x Z, lid included). **The lid and the base have the same footprint on all four
sides, no step** (the user, 2026-11-09: "the lid seems bigger by a few mm than the base"; until then the base's front
and back faces were 0.7 mm inside the lid's):
- **Lid and base:** 89.0 x 81.5, i.e. 1.0 + board 87.0 + 1.0 and 1.0 + board 79.5 + 1.0. Every outer face of both parts
  is 1.0 mm outside the board's edge.
- **Z:** floor 2.0 + 0.1 + DIN 19.6 + board 1.6 + **7.2** to the lid's inside + lid 1.5. The joint line runs 1.6 mm
  above the board's top (the base's rim), and steps up to the jacks' and the XIAO socket's axes there.

**Standing out (not counted):**
- the MIDI jacks' bushings 1.5 mm (front and back), the side jacks' bushings 1.0 mm;
- the XIAO's USB-C socket 0.5 mm (back);
- the knob 13.3 mm, the button caps 3.0 mm, the A/B grips 2.0 mm (2.35 to the ridges' tops) above the lid.

**Parts to print:** box, lid, 2 button caps, 3 + 1 A/B tabs, 4 A/B grips, 1 knob (and `front_test` first).

## Connectors at the wall, the DINs in slots

**The lid (skirt):** on every edge **the board's edge is 1.0 mm inside the outer face** (`edge_in`). The skirt comes
down from the lid to the board's top. It is 2.3 mm thick: 1.0 outside the board, 1.3 over it. Its outer 1.0 stops 1.7
above the board's top, over the base's rim (see "The joint").

**The base, front and back: the DINs slide into slots in the wall** (the user's rulings, 2026-11-09: "slots in which
the MIDI DIN slides/slots in", "slots in the wall", "not a wall at the back"). These walls are **5.9 mm deep**, from
the outer face (1.0 outside the board's edge, flush with the lid) to 4.9 inside it.
Round each DIN plate the wall carries:
- **a front cheek 1.9 mm thick** (the plate's face 0.1 behind it, **2.0 mm inside the outer face**; it was 1.2 before
  the faces were made flush), whose window leaves the plate's face open except its edges;
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

So the front window and the back opening are 18.3 mm wide. The window is 17.35 mm high, up to the base's top under the
board; the rim closes it above.

**Between two neighbouring DINs** (pitch 23.0): the plates are 2.1 mm apart, so the wall's **rib in that gap is 1.9 mm**
(0.1 clearance a side). Each plate's edge has 1.3 mm of cheek over it on that rib, in front and behind. Behind the
plates the bodies are 5.2 mm apart: the posts fill that, 0.3 clear of each body.

**OUT1** at the back is the same.

**OUT2's plate** is flush with the board's left edge. Its slot runs into the side wall, leaving 0.9 mm of wall outside
it.

**Over the jacks' pegs and leads** (above the rib, under the board), the rib and the back cheek stop 1.7 mm below the
board.

**The base's sides** are 1.9 mm (1.0 outside the board, 0.9 under it), with the rim on top of the outer 0.8.

**The board** is clamped between the skirt (1.3 over it) and the base's walls (front and back walls under the edge,
the side walls 0.9 under it). It is screwed at H1..H4.

**Where each connector face ends up** (on the board model in the box):

| Connector | Its face | Distance from the outside | Why |
|---|---|---|---|
| **DIN plate** (front, back) | 1.0 behind the board's edge | **2.0 mm inside the outer face** | the front cheek 1.9 + 0.1 (the user accepted the 2.0, 2026-11-09) |
| **MIDI jack body** (front, back) | 0.5 past the board's edge | **0.5 mm inside the face**, the bushing **1.5 mm proud** | a 0.4 mm web in front of the body: the skirt's above the jack's axis, the base's tongue below it |
| CLOCK / RESET jack body (sides) | on the board's edge | 1.0 mm inside, the bushing 1.0 mm proud | the skirt's 0.9 mm web above the axis, the tongue (0.8, 0.2 clear) below |
| XIAO USB-C socket | 1.545 past the board's edge | **0.5 mm proud** | through a 9.8 x 4.1 hole (see "USB-C") |
| Power USB-C (J1, SHOU HAN TYPE-C 6P, C456012, 8.94 x 6.81 x 3.2) | 0.035 past the board's edge | **0.965 mm inside the face** | a 13.0 x 7.5 recess for the plug's overmold (see "USB-C") |

**The DIN body, scaled off HOOYA's drawing** (the coordinator, 2026-11-09: 33.5 px/mm against the dimensioned 20.9,
19.6, 11.8 and 1.8; ±0.2 mm, *not a caliper*):
- 17.8 mm wide (the plate overhangs it 1.55 a side);
- 17.2 mm high from the board's side (the plate overhangs it 2.4 at the far edge, none at the board's side);
- 13.1 mm deep from the plate's front.

The back cheek's rims depend on these numbers. The checks use them for the DIN's body (`din_body`).

**Print and fit:**
- **The jack holes** are round, **Ø5.4** (the bushing's 5.0 + 0.2 a side), centred on the jack's axis 2.5 mm above the
  board, the same at all eight jacks; the lower half in the base's tongue (open upwards), the upper half in the skirt.
- **The DIN slots** are open at the top. The rim bridges each DIN window (18.3 mm) above it.

## The joint: registration, round jack holes, catches

The user, 2026-11-09: "bottom and lid should have an alignment / locking system; the screws are just there to lock it
in place", and "round on lid and bottom" for the jacks' holes. FDM clearances: **0.2 mm** a side wherever the lid fits
over the base, 0.1 at the tops.

- **The rim (base):** round the whole board, outside its edge: **0.8 mm thick** (the outer face to 0.2 short of the
  board's edge), from the base's top under the board to **1.6 mm above the board's top**. It covers the board's edge.
- **The rebate (lid):** the skirt's outer 1.0 mm stops 0.1 above the rim. The skirt's band over the board (1.3, on the
  board's top) comes down inside the rim, its face on the board's edge: **0.2 mm from the rim all round**. The lid
  cannot move more than 0.2 in X or Y on the base (`brace_reg`: 0.25 in any of the four directions meets the rim).
- **The jacks (eight):** at each, the rim rises to the jack's axis as a **tongue 9.7 mm wide** (the body's cavity 6.5 +
  1.6 a side), **0.8 thick**, with the hole's **lower half-round**; the lid has a notch over it (0.2 a side, 0.1 above)
  and the **upper half-round**. Split line at the axis, 2.5 mm above the board's top. The two side jacks on a wall (6.7
  apart) share one tongue. The jack's body drops into the tongue's cavity (open upwards) with the board.
- **The XIAO's socket:** the same split at its axis (5.35 above the board's top): a tongue 13.0 wide (the hole 9.8 +
  1.6 a side) under it, the lid's notch over it.
- **Snap catches (four, light):** an **8 mm bump on the lid's band, 0.35 high with 45-degree flanks**, 0.8 above the
  board's top, clicks into a **45-degree groove 0.35 deep in the rim's inside** (0.45 of rim behind it). Going down the
  bump pushes the rim out by 0.15; seated it has about 0.14 clear in the groove (`check_lidbox` empty); lifted 0.3 it
  bears on the groove's upper flank (`brace_snap`). Where: front at X 34.45 (between jacks 2 and 3), back at X 21.0
  (between OUT1's jack and the LED), left at Y 69.5, right at Y 61.5 (box coordinates). The 4 x M2 screws stay the
  final lock.
- **Printing:** the rim's grooves (45-degree roof), the tongues and cradles (open upwards) print on the base as it
  stands; the bumps (45-degree flanks), the rebate and the upper half-rounds print on the lid top-down. No supports.
  The only bridges are the rim over each DIN window (18.3 mm, in the base).

## USB-C

**What the cut-out at the XIAO was** (measured 2026-11-09 on `jank-pcb-mod.kicad_pcb` and `mockup-mod.stl`):
- **The XIAO module's PCB does not pass the board's edge.** Its outline (the footprint's F.Fab) runs 0.0 to 21.0 mm in
  from the board's back edge; its courtyard starts 0.025 past it.
- **Only its USB-C socket stands out: 1.5 mm past the board's edge** (footprint silk 1.51, mockup 1.545), **0.5 mm past
  the box's back face**. It is 9.0 wide and sits high: 3.7 to 7.0 above the board's top (mockup), its axis at 5.35.
  The lid's underside is 7.2 above the board's top, its top 8.7.
- **So the old opening was a 12.5 mm notch through the lid's skirt and plate, open to the top** (3.3 mm into the lid's
  top). It was sized for a 12.5 x 7.0 overmold centred on that socket. That shape reaches 8.85 above the board's top,
  higher than the lid's top. A closed 12.5 x 7.0 hole there does not fit under the lid.
- **Now:** the socket stands 0.5 past the face, so a plug's overmold stops outside the box. The opening only has to
  clear the socket: **one hole 9.8 x 4.1 mm** (0.4 round the 9.0 x 3.3 socket, corners r 0.8), 3.3 to 7.4 above the
  board's top. That leaves 1.3 mm of lid above it, split at the socket's axis as described above.
- **If the XIAO is pulled in** (a PCB change: it would collide with the OLED's header in front of it), its socket comes
  behind the face. The overmold must then enter the wall, and a 12.5 x 7.0 overmold centred on the socket reaches above
  the lid's top. That needs either the lid raised about 1.5 mm or the notch through the top again. *Not done.*

**J1 (power):** its mouth is 0.035 past the board's edge, 0.965 behind the face. **One recess, 13.0 x 7.5 mm**
(corners r 1.75), from the face to the board's edge, centred on its axis 1.6 above the board's top. The base has its
lower part (to 0.55 below the board's bottom) and the lid its upper part. It assumes a **typical overmold of 12.5 x
7.0 with r 1.5 corners** (*unverified*: cables vary; a squarer or bigger overmold will not seat). The mouth (8.94 x
3.2) has about 2.0 clear a side, 2.15 above and below. J1's legs keep-out under the board now ends 0.25 past the edge
(the mockup's block, 0.035 past; its THT pads are 1.65 in), so the base's wall is full behind them.

`check_usb`: both plugs' overmolds, seated (from each mouth outwards), clear the base and the lid.

## Bracing

| Part | Pull-out | Push-in | Down / up | Sideways |
|---|---|---|---|---|
| **DIN** (HOOYA: plate 20.9 x 19.6 x 1.8) | the plate's front on the **front cheek** (1.3 a side, 2.2 at the far edge; 0.1) | the plate's back on the **back cheek** (the same rims; 0.1) | the plate's far edge on the floor (0.1); the board above | the plate's side edges in the slot (0.1) |
| **MIDI jack** (PJ-320D-B-SMT, 6.0 x 12.2 x 5.0, front 0.5 past the edge) | the body's front on the 0.4 web (0.1): the skirt's above the axis, the base's tongue below it | the body's back on the **lid's cap** (back stop 1.6, 0.15) | the board below, the cap's roof above | the cap's cheeks; the bushing in its round hole (0.2) |
| **Side jack** (PJ-320A) | the body's front on the skirt's 0.9 web (0.1) | one **cap per side wall** round both jacks (6.7 apart) | as above | as above |
| **The board** | | | clamped all round between the base's wall (0.9 under it) and the skirt (1.3 over it); four M2 x 20 into webbed columns | the rim (0.2 from its edge) |
| **The lid on the base** | the catches (`brace_snap`), then the screws | | | the rim (0.2, `brace_reg`) |

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
| SW2 (OUT1, surface-mount) | 17.5, 8.1 | **4.3 x 4.3** (along x 4.3) | the tab's 4.0 x 2.4 neck: 0.15 a side across (it guides), its 1.5 travel along x + 0.2 at each end |
| SW3..SW5 (OUT2..OUT4) | 10.45 / 33.45 / 56.45, 60.5 | **4.3 x 4.8**, along y | the neck, 0.15 a side across, its 2.0 travel + 0.2 at each end |
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
| **Button cap** (2) | a 5.4 mm stem 3.0 above the lid; an **8.0 mm flange, 1.0 thick, 0.3 under the lid**; a **blind bore 3.45 mm**, 3.1 deep, its floor on the plunger's top (the 9.0 mm HRO K2-1102DP-K4SW-04 since 2026-11-09: the cap is 0.5 mm more solid above the plunger, the same outside and travel) | +0.05 on the 3.4 plunger: a light press once printed | flange on the bed |
| **A/B tab** (3, SW3..SW5) | under the lid a **flange 6.3 x 8.8 x 3.3** (across x along the travel; 0.2 over the switch's body to 0.2 under the lid), wider than the 4.3 x 4.8 slot by 1.0 on every side at every point of the travel; a **neck 4.0 x 2.4** through the slot to 0.2 above the lid; a **post 3.0 x 1.8 x 1.4** for the grip; a **pocket 1.8 x 2.1, 1.3 deep** over the 1.5 x 2.0 handle, its roof on the handle's top | pocket +0.3 across, +0.1 along the travel (see below) | pocket side on the bed (its roof a 1.8 mm bridge) |
| **A/B tab, SMD** (1, SW2) | the same, flange **6.3 x 7.8 x 3.6** (travel 1.5), the pocket **1.8 x 1.6, 2.8 deep** to the knob's top (3.2 + 3.0) | as above; **the knob's length along its travel taken as 1.5: unverified** | pocket side on the bed |
| **A/B grip** (4) | **6.0 across x 4.0 along the travel, 1.8 high**, its bottom 0.2 above the lid (on the neck's shoulder), top 2.0 above the lid; edges chamfered 0.4 (top) and 0.3 (bottom); **3 ridges across the travel** on top, 0.5 wide, 0.35 high, 1.2 apart; a socket 3.0 x 1.8 x 1.4 | the socket = the post: a press fit (an FDM hole prints about 0.1 small); a drop of CA glue if it is loose | socket side on the bed (its roof a 1.8 mm bridge) |
| **Encoder knob** (1) | 14 x 12, a **D bore 6.1 / 4.6** (the shaft's 6.0 / 4.5, *unverified*), 10 deep, a pointer groove | 0.1 play | top on the bed |

**The A/B tabs** (the user, 2026-11-09: "the current option is just not working, I am sure"; the old caps were
pressed onto the handles and stood loose in a slot). Now each tab is **captured**: its flange lies under the lid, wider
than the slot on every side, so it cannot be pulled off or fall out (`brace_tabs`: lifted 0.25 it meets the lid). It
rests on the handle's top, 0.2 under the lid and 0.2 over the switch's body, and slides on its handle.
- **Two parts.** A one-piece tab cannot go into a slot narrower than both its ends: the tab goes onto the switch before
  the lid is lowered, and the grip is pressed onto its post after.
- **The pocket's fit:** +0.1 along the travel, so the handle follows the tab with 0.1 of lost motion; +0.3 across, loose
  on purpose. The tab is guided by its neck in the slot (0.15 a side) and by its flange under the lid, so the pocket
  only has to push. A tight pocket would fight the lid's slot wherever the board and the lid are 0.1 out of line.
- **The slot** is the neck + the travel + 0.2 at each end, so the switch's own end stops end the travel, not the lid.
- **The A / B labels** stay where they were, 1.0 past the slot's ends. A grip at the end of its travel stops 0.4 short
  of the label's edge.

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
| `check_lidbox`: lid against base, seated, with the catches | empty | empty (0.2 at the rim, 0.1 over it, the bumps in their grooves) |
| `check_drop`: the lid (without its bumps) lowered straight down over the base, the board, the button caps and the A/B tabs (no grips yet), each tab at either end and the middle | empty | empty |
| `check_usb`: the USB-C plugs' overmolds (12.5 x 7.0, r 1.5), seated, against the base and the lid | empty | empty |
| `check_board_drop`: the board lowered straight into the base, every DIN plate sliding down its slot | empty | empty |
| `check_text`: labels clear of every opening, slot, cavity and keep-out, inside the edges | empty | empty |
| `check_caps`: caps at rest / pressed / the A/B tabs and grips at both ends of travel, against the lid, the board (SW2's MIDI IN parts, the screen's posts), the shaft; each handle in its pocket | empty | empty |
| `brace_caps`, `brace_pull` (each DIN plate moved 0.15 out meets the front cheek), `brace_push` (0.15 in meets the back cheek), `brace_pull_jack` (MIDI jacks on the webs), `brace_push`, `brace_trs`, `brace_floor`, `brace_lid`, `brace_ledge` | contact | contact |
| `brace_side_pull` / `brace_side_push`, each side jack | contact | 8 of 8 |
| `brace_reg`: the lid (no bumps) moved 0.25 in -X, +X, -Y, +Y meets the base's rim | contact | 4 of 4 |
| `brace_snap`: the lid lifted 0.3 meets the base at the catches | contact | contact |
| `brace_tabs`: each A/B tab lifted 0.25 meets the lid's underside with its flange | contact | 4 of 4 |
| `check_stl.py` (box, lid, button cap, both A/B tabs, the grip, knob, front_test) | OK | OK: one body, 0 open edges, 0 non-manifold each |

**Negative controls collide as they should:**
- `gap_out = -0.05` (a jack body into its web);
- `band_in = 2.0` (the skirt onto the DIN legs);
- `slot_play = -0.05` (a slot narrower than the plate);
- `din_rim = [1.6, 2.6]` (the back cheek onto the DIN's body, in `check_board_drop`);
- `standoff_d = 6.0`;
- 2026-11-09, the tabs (`check_caps`): `tab_top_gap = -0.05`, the slot's end clearance -0.05, the pocket 0.1 short
  along the travel, `tab_overlap = 4`;
- 2026-11-09, the joint: `catch_groove = 0.1` and `tongue_clear = -0.05` (`check_lidbox`), `jack_hole = 4.9` (`check_box`,
  `check_lid`), `plug_clear = -0.1` (`check_usb`).

## Print

| Part | Orientation | Supports |
|---|---|---|
| box (base) | floor on the bed | none: the DIN slots, the tongues' cradles and J1's recess are open at the top; the rim bridges the DIN windows (18.3 mm); the catch grooves have 45-degree roofs |
| lid | top face on the bed | none: the rebate, the notches and the upper half-rounds are open at its free edge; the bumps have 45-degree flanks |
| caps, knob | as listed above | none |
| front_test | standing on its floor | the skirt's half spans between the end posts (a 26.5 mm bridge) |

PETG or PLA, 0.2 mm layers, 4 perimeters, 25 % infill.

## Assembly (who does what)

1. **User solders the board** (board README).
2. **User prints** `front_test.stl`. Check that a DIN plate slides down its slot (snug, not tight) and does not move in
   or out, and that a plug seats with the plate's face 2.0 mm in. Check that a jack's bushing sits in the round hole with
   its body against the web, and that a 3.5 mm plug goes in.
3. **User puts** a 2.5 mm foam pad under the OLED module's front edge (the edge away from its header), and presses the button caps onto OK and BACK.
4. **User lowers** the board straight into the base. The DIN plates slide down their slots onto the floor, the jacks'
   bushings and the XIAO's socket into their cradles, and the board's edge comes to rest on the base's wall and the four
   columns, inside the rim. **User puts** the four A/B tabs (no grips) onto the switches' handles, pocket down, the
   SMD one on SW2.
5. **User lowers** the lid straight down, its slots over the tabs' necks. Its band goes inside the rim, the notches over the tongues, and the catches
   click (four). The caps come through their openings.
6. **User screws** 4 x M2 x 20 self-tapping at H1..H4, snug only. Then the knob on the encoder, and **presses the four
   grips onto the tabs' posts** (ridges up; a drop of CA glue if one is loose).

## Unverified, all in one place

- **The DINs:**
  - the body behind the plate (17.8 x 17.2 x 13.1) is **scaled off the drawing**, not measured: the back cheek's rims
    (1.3 / 2.2) rest on it;
  - a plug seating with the plate's face 2.0 mm in (the user accepted it, 2026-11-09);
  - the slot's 0.1 mm play against the printer's tolerance (a tight print: ream or set `slot_play`);
  - the cheeks' stiffness.
- **The jacks:**
  - the PJ-320D-B-SMT's body (the mockup's 6.2 x 12.2 x 5.0) and its 5.0 bushing (the Ø5.4 holes rest on it);
  - the 0.4 mm web's strength.
- **SW2:** its knob's length (1.5 taken), its travel direction, which end is A.
- **The A/B tabs:** the handles' sizes (1.5 x 2.0, 1.5 x 1.5) and heights (MSS22D18 body 3.5, handle top 5.0; SW2
  3.2 / 6.2) from the datasheets and the mockup, not parts in hand; the grip's press fit on its post (printer-dependent);
  how freely the flange slides on the lid's underside (0.2, first layers of the lid: sand if it drags).
- **The board:**
  - its outline against the 0.1 mm gaps at the connectors (JLCPCB ±0.2: the board's edges are clamped, not fitted
    between walls, so the gap only limits how far the board can shift before a connector bears);
  - the mockup's positions (the PCB files are the board session's).
- **The OLED module:** its PCB 1.0 and glass 1.65 (top at 5.15), its holes' 2.5 and positions, and the glass's
  position, from the board session's numbers, not a part in hand.
- **The joint:** the 0.2 mm clearances against the printer (FDM: a tight print, sand the band); the catches' force (the
  rim, 0.8 thick, bends 0.15 going over a bump: up to about 3 % strain at its root as a cantilever from the board's bottom, less along the wall, *not
  tested*; PETG is the safer material). The rim's 0.8 is thin over the 18.3 mm DIN windows.
- **USB-C:** the overmold taken as 12.5 x 7.0 with r 1.5 corners; the XIAO socket's height (3.7 to 7.0) and J1's (axis
  1.6), both from the mockup.
- **Other:** the XIAO's 7.0 height, the encoder's 6.5 and 20, the buttons' 3.5 body, the cap press
  fits, the EC11 D flat, M2 into PLA, the font.
