# jank-mini-3 box: STAGE 1 (Claude, 2026-11-19), for mots's approval

`box.scad` (parametric OpenSCAD), the board as boxes in `board_model.scad` (written from `../lcd2.kicad_pcb` by
`board_model.py`, KiCad's python), pictures by `make_renders.sh`, the collision check `box_check.py`. The older
`case.scad` (93 x 62.6 layout) is untouched. No STLs yet: stage 2 after mots's go.

| picture | what |
|---|---|
| `box-assembly.png` | closed box, 3/4 view from the front left |
| `box-exploded.png` | shell up, cap above it, board, lid down |
| `box-top.png` | the face from above: screen window, D-pad, the engraved labels |
| `box-inside.png` | the lid with the parts under the board (shell and our board hidden): strip rails, the cell in its corner, the L, tongues |
| `box-section-din.png` | section x = 62.5 (J5's DIN, the D-pad, the L's top arm, the cell), seen from the right |
| `box-section-screen.png` | section y = 41 (the D-pad centre, the screen, the LCD-2's USB-C slot, the L's left arm, the cell), seen from the front |
| `floor-map-corner.png` | the floor from above (`floor_map.py`, every number read from box.scad): bottom parts and depths, strips, cell + rubber gap, the L, the power switch, D-pad switches, screws |
| `box-left-wall.png` | the left wall from outside: the LCD-2's USB-C slot (its lid tongue) and the power switch's cap (bottom-left) |
| `box-section-switch.png` | section y = 71.5 through the power switch (seen from the front, the top half kept): cap, the thinned wall, body and tabs, the lower strip behind, the board above |

## The parts

- **Top shell**: the face and the four walls, full depth. The board (with the LCD-2 and the strips plugged on) drops in
  from below and screws to 4 bosses on the face's underside through H1-H4. **Printed face down** (the engraved labels
  and the window chamfer on the bed side). The USB-C window's lower edge prints as a 25.8 mm bridge.
- **Bottom lid**: flat, 2.0 mm, with the parts that hold things from below: two rails under each bridge strip, an L on
  the cell's top-left corner (lid up to 0.2 mm under our board: it holds the cell and backs the board) and tongues
  that close the wall slots under the jacks and the LCD-2's USB-C. **Printed outside down**: everything grows straight
  up, no bridges.
- **The cell** (mots, 2026-11-19, after the floor map): EEMB LP103454 lying on the lid in the BOTTOM-RIGHT corner,
  34.5 along x, 56 along y, pushed into the corner: **x 59.4-93.9, y 24.4-80.4** (board coordinates), a `rubber_gap`
  of 2.0 mm on all four held sides (right wall, bottom wall, the L's two arms) for mots's rubber / foam. Its top is
  at z -12.4: it lies under J13 / J14 (5.8 mm above it, their bodies end at -6.6) and H4's screw head (9.2 above), not
  under the DINs (they end at y 15.19, the cell starts at 24.4). Nothing else on the lid comes near it (the towers and
  tongues sit in or outside the walls).
- **The L** (`L_top_t` 3.0, `L_left_w` 4.4, `L_top_len` 20, `L_left_len` 36): outer corner at (53.0, 19.4).
  Top arm x 53.0-73.0, y 19.4-22.4 (along the cell's top side, 2.0 rubber under it). Left arm x 53.0-57.4,
  y 19.4-55.4 (along the cell's left side, 2.0 rubber; 4.4 wide so it reaches leftward under SW1). It backs
  **SW1** directly (SW1's centre x 56.75 lies on the arm, which covers its pads from x 53.75 to 57.4). The others,
  nearest point of the L to the switch centre: **SW3** 7.35 mm (the left arm; the top arm 10.6), **SW4** 7.35 mm (the
  left arm; the arm runs 6.4 mm past SW4's y), **SW5** 15.35 mm (the left arm; the top arm 18.6). H4's screw at
  (91.5, 41) holds the board 18.75 mm right of SW5.
- **The cell's lead**: its lead end toward **-y** (the top end, y 24.4), lead leaving in the part right of the L's top
  arm (x 73-94: 9 mm of free floor there between the cell and J6). From there it runs left in the 4.2 mm channel between
  J5's body (y <= 15.19) and the L's top arm (y >= 19.4), turns down at x ~44-52 (between the strips' right ends at
  x 43.1 and the L's left arm). **Black** goes on down that gap to y ~41 and left under the LCD-2 to its MX1.25
  (on the LCD-2's back, at about x 23 in our coordinates, scaled from Waveshare's back view: NOT verified, its y
  not known): **~87 mm**. **Red** (the power switch in series: cell red -> one tab, the other tab -> the adapter's
  red pigtail -> the MX1.25) goes down the same gap to y ~65.5, left along the floor between the lower strip (y <=
  63.15) and J7 (y >= 67.9; J7 only hangs to z -6.6, the wire runs on the floor under it anyway) to the switch's
  tabs at x 13.2-17.2, y 71.5 (~123 mm), then back up along the left wall at x ~4 (past the lower strip's left end
  at x 6.1, on the floor under H2's spot) to y ~50 and over to the MX1.25 (~57 mm): **red ~180 mm** in all, plus
  slack for the plugs and bends. Lengths are a floor sketch (`floor_map.py` draws and sums them). The cell's ~60
  mm lead plus the 10 cm adapter pigtails do not reach that far for the red: mots makes the red pigtail (or the
  switch's two wires) long enough. If EEMB's lead leaves the left part of the 34.5 mm end, `L_top_len` shortens the
  top arm (or the cell turns so the lead is on the right).
- **The power switch** (mots, 2026-11-19): YIZHI KCD1-106S-B (LCSC C52125269), a round snap-in rocker with 2 pins,
  in the **LEFT wall** below the LCD-2's notch, centre **board y 71.5, height z -13.0** (13 mm under our board's top,
  i.e. 11.4 under its bottom, 10.0 over the lid), as low as the old logic allows (between the lid and the board).
  Its datasheet drawing was not read: all its sizes are ASSUMED parameters: hole d 15.0, cap d 16.5 x 2.0 proud,
  body d 15 x 14 deep behind the panel, 2 tabs in an 8 x 4 x 8 envelope behind the body. The wall (2.0) is thinned
  from the inside to **1.6 mm** in a d 19 patch round the hole (y 62.0-81.0, z -22.5..-3.5; `psw_panel`,
  `psw_thin_d`), for clips that grip 1.0-2.0. Inside, the body + tabs fill x -0.8..17.2, y 64.0-79.0, z
  -20.5..-5.5. It needs no board connection (it sits in the cell's red lead).
  **Why y 71.5** (mots asked ~72-74): the cap (d 16.5) must stay clear of the bottom-left corner tower, whose fillet
  starts at y 80.5 on the wall's outside: the cap ends at y 79.75 (0.75 clear); y 74 would put it 2.0 into the
  tower. Down the other side, the body starts at y 64.0, 0.85 past the lower strip's PCB edge (y 63.15). y 71.5
  balances the two. A 16.0 cap (mots's measurement) adds 0.25 at the tower.
  **Screw access**: H3 (40.5, 79) stays reachable (the switch ends at x 17.2). **H2 (3, 64.5) does not**: the body
  lies 2.3 mm under its head and covers y 64.0-79.0, so a screwdriver's shaft (axis y 64.5) hits it; H2 needs the
  switch out (it snaps in from outside after the board is screwed in, and comes out by squeezing its clips from
  inside with the lid off). Keeping H2 free needs the body at y >= ~67.5 (centre >= 75), which the corner tower
  forbids.
- **D-pad cap**: the 24 mm cross (8 mm arms) through the face, 2.0 mm proud; a flat flange 1.2 mm wider than the cross
  rests on the four switch plungers (SW1/3/4/5) and stops 0.15 mm under the face. No centre pivot: pressing an arm
  tips the cap about the far arm's flange edge. **Printed flange down.**
- The LCD-2's BOOT / RESET stay covered (the left-wall slot is only the USB-C plug's width).

Inserts and screws (brass heat-set inserts; check the hole size against the maker's sheet of the inserts bought):

| where | insert | hole in the print | boss | screw |
|---|---|---|---|---|
| board -> shell (H1-H4) | M2 x 4.0 (OD ~3.6) | d 3.2, 5.0 deep (0.6 into the 2.4 mm face) | d 6.0, from the face to our board top | 4 x M2 x 5 pan head (ISO 7045), from below |
| lid -> shell | M3 x 5.7 (OD ~4.6) | d 4.0, 6.5 deep, above a d 6.4 x 1.0 recess | towers d 7.5 at the four outside corners | 4 x M3 x 8 countersunk (ISO 10642), from below; the lid is 3.0 there (a d 6.0 x 1.0 pad) |

**Why the lid screws are in outside corner towers**: the board fills the whole inside (95.5 x 82 + 0.4 a side) and
comes in from below, so nothing of the shell may sit under it (a first version had columns under the board: the check's
insertion sweep showed the board could not get past them). The towers clear the inside's corners (centre 2.85 mm out
diagonally). They make the outline 109.5 x 96.0 instead of the walls' 100.3 x 86.8. Alternatives if mots dislikes
the lobes: screws through the walls sideways into tabs on the lid, or walls thick enough for M3 inserts (~7.5 mm,
outline ~111 x 98).

## Outside dimensions

| | din_depth 20.9 (HOOYA, default) | din_depth 19.6 (LCSC model) |
|---|---|---|
| footprint, towers included | 109.5 x 96.0 | 109.5 x 96.0 |
| footprint, walls only | 100.3 x 86.8 | 100.3 x 86.8 |
| height, lid bottom to face top | 31.8 | 30.5 |
| with the D-pad cap | 33.8 | 32.5 |

Heights about our board's top (z 0): face top 6.8, face underside 4.4, board bottom -1.6, DIN bottom -22.5, lid top
-23.0, lid bottom -25.0 (at 20.9).

## Openings

| | where | size |
|---|---|---|
| screen window | x 4.9-46.7, y 25.2-56.8 (board mm) | the active area 40.8 x 30.6 + 0.5 a side = 41.8 x 31.6, 45 deg chamfer 1.6 from the top |
| D-pad | centre (64.75, 41) | the cross + 0.4 a side (24.8 / 8.8) |
| USB-C TO ESP + TO DEVICE (J1, J4) | top wall, x 25.55-51.35, z -1.8..+5.5 | one window for both overmolds (12.0 + 0.4 a side, 13 mm apart): a closed window, 1.3 mm of face over it |
| MIDI 1 / 2 TRS (J2, J3) | top wall, x 6.3 / 19.1, axis z -4.1 | d 8.0, a slot open to the wall's bottom edge, closed by a lid tongue |
| MIDI 1 / 2 DIN (J5, J6) | top wall, x 51.65-95.45, from the bottom edge to z -2.2 | the two DIN plates + 0.4 a side; they are 1.2 mm apart, so ONE window; 0.6 mm of wall stays under the board |
| CLK / RST (J13, J14) | right wall, y 31 / 51, axis z -4.1 | d 8.0 slots, lid tongues |
| LINE IN (J7) | bottom wall, x 30, axis z -4.1 | d 8.0 slot, lid tongue |
| the LCD-2's USB-C | left wall, y 34.6-47.4, z -6.9..+0.4 | its plug's overmold (12.0 x 6.5 + 0.4) about its axis (z -3.2, ASSUMED), slot to the bottom edge, lid tongue |
| the power switch | left wall, centre y 71.5, z -13.0 | a closed round hole d 15.0 (ASSUMED), the wall thinned to 1.6 in a d 19 patch inside |

Labels engraved 0.6 mm (Liberation Sans Bold 2.2 mm): MIDI 1, MIDI 2, TO / ESP, TO / DEVICE, MIDI 1, MIDI 2 (top
edge), CLK, RST (right), LINE IN (bottom).

## Parameters and their sources

BOARD = make_board.py / lcd2.kicad_pcb; MODEL = the parts' 3D models in ../lib/3d (bounding boxes read 2026-11-19);
DRAWING = Waveshare ESP32-S3-LCD-2-20250122.pdf; README = this board's README; ASSUMED = not measured, mots checks.

| parameter | value | source |
|---|---|---|
| W, H, T | 95.5 x 82, 1.6 | BOARD |
| MOUNT (H1-H4) | (3, 17.6), (3, 64.5), (40.5, 79), (91.5, 41), M2 | BOARD |
| notch, LCD-2 PCB | x 0-49.01, y 23.195-58.805; LCD-2 at (0.5, 23.495), 48.21 x 35.01 | BOARD |
| D-pad centre, switches | (64.75, 41), +/-8; plunger top 1.55 | BOARD; MODEL (TS-1187A) |
| connector positions | J1 31.95, J4 44.95, J2 6.3, J3 19.1, J5 62.5, J6 84.6, J13 y 31, J14 y 51, J7 x 30 | BOARD |
| every part's outline | courtyards (top SMD) x package height; bodies (bottom THT) | BOARD + MODEL (board_model.py) |
| jack noses, axis | PJ-307C d 6, PJ-320A d 5, axis 2.5 under the seat (z -4.1); bodies 5.0 deep | MODEL |
| DIN plate | 20.9 wide, y -8.61..+6.69 about its origin | MODEL |
| `din_depth` | **20.9** (19.6 in the LCSC model) | ASSUMED (HOOYA drawing): **measure** |
| `usb_h`, `usb_om_z` | 3.46; overmold -1.4..+5.1 | README (GCT drawing, 9B N4) |
| `usb_om_w` | 12.0 | ASSUMED (plug overmold width) |
| `lcd_front` | 4.1 above our top | DRAWING (scaled, 5.70 - 1.6): **measure** |
| `lcd_t`, `lcd_bl`, `lcd_aa`, `lcd_aa_far` | 2.1; BL 47.8 x 34.6; AA 40.8 x 30.6, 2.3 from the BL edge away from the USB-C, centred across | DRAWING |
| `lcd2_pcb_t` | 1.6 (its top level with ours) | ASSUMED: **measure** |
| `lcd2_usb`, its axis | 7.35 x 8.94 x 3.26 on its back, centred across (y 41), axis z -3.2 | ASSUMED (generic 16-pin USB-C): **measure** |
| `lcd2_usb_out` | 0.7 past our left edge | README |
| `lcd2_back_h` | 3.5 (its back parts) | ASSUMED (nothing of the box comes near: 1.5+ mm) |
| strips | 37.06 x 10, rows 3.81 apart; 2.5 plastic + 8.5 socket: underside z -14.2 | strip/make_strip.py, README |
| `strip_t`, `strip_tail` | 1.6, 1.5 | ASSUMED: **measure the strip's underside height** (the rails stop 0.1 under it) |
| `cell` | EEMB LP103454, 34.5 x 56 x 10.6; in the bottom-right corner: x 59.4-93.9, y 24.4-80.4, lead end -y | DATASHEET; position mots (2026-11-19) |
| `rubber_gap` | 2.0 on every held side (walls, the L) | mots |
| `L_top_t`, `L_left_w`, `L_top_len`, `L_left_len` | 3.0, 4.4, 20.0, 36.0 | design (mots: arms not the cell's whole length, the left one past SW4's y + a few mm) |
| `psw_y`, `psw_z` | 71.5, -13.0 (left wall) | design (mots: left wall, bottom-left; between the corner tower and the lower strip, under H2's head, over the lid) |
| `psw_hole`, `psw_cap`, `psw_body`, `psw_tabs` | d 15.0; d 16.5 x 2.0 proud; d 15 x 14 deep; 8 x 4 x 8 | ASSUMED (KCD1-106S-B datasheet not read; mots measured the cap ~16): **measure** |
| `psw_panel`, `psw_thin_d` | 1.6 wall at the switch, a d 19 patch | ASSUMED (clips grip 1.0-2.0; their reach) |
| `plug_d` | 8.0 (TRS plug bodies up to 7.2) | ASSUMED: check the MIDI TRS and CV cables |
| `fit`, `wall`, `face_t`, `face_gap` | 0.4, 2.0, 2.4, 0.3 | design (PETG, 0.4 nozzle) |
| `cap_clr`, `cap_flange`, `cap_play`, `cap_proud` | 0.4, 1.2, 0.15, 2.0 | design |
| `tongue_clr`, `din_clr`, `om_clr`, `strip_post_gap`, `board_gap` | 0.2, 0.4, 0.4, 0.1, 0.2 | design |
| inserts, screws | see the table above | DATASHEET (typical brass inserts): check the ones bought |

## What mots measures first (on the real parts)

1. **One DIN-504B-M10's depth below the board** (20.9 or 19.6): sets the box height (lid top = DIN bottom - 0.5).
2. **The LCD-2**: its PCB thickness (its top level with ours?), the screen front's height above its PCB top (4.1 +
   1.6 assumed: face underside at +4.4), the screen's active area position (2.3 mm from the far end of the
   backlight, read from the drawing).
3. **The LCD-2's USB-C**: how far below its PCB its axis is (3.2 assumed) and how far past our left edge it reaches
   (0.7); where BOOT / RESET sit: they must stay behind the left wall (their centres ~3 mm outside the slot's edges if
   the drawing's 18.90 is their spacing, as assumed).
4. **The jacks**: the PJ-307C / PJ-320A axis 2.5 mm under the board (from the models; the PJ-307C model's nose
   reaches 0.5 mm past its seating plane, into the board's thickness: check where the real nose sits).
5. **A bridge strip plugged on**: the height of its underside below our board's bottom (12.6 assumed + 1.6 strip),
   its tails' length.
6. **The plugs**: the USB-C cables' overmold width (12.0 assumed: the window is one 25.8 mm opening for both) and the
   TRS plug bodies (7.2 max assumed).
7. The cell's real size (34.5 x 56 x 10.6) and where along its 34.5 mm end the lead leaves (the route above wants it
   right of x 73, i.e. right of the L's top arm); where the LCD-2's MX1.25 is on its back.
8. The inserts bought (hole sizes 3.2 / 4.0 assumed).
9. **The power switch (KCD1-106S-B)**: its panel hole, cap diameter and height, body diameter and depth behind the
   panel, the tabs' length, the panel thickness its clips grip and how far out the clips reach (the thinned patch).

## Clearances measured (box_check.py, OpenSCAD manifold)

Each board group grown in x, y and z alike until it meets a case part (L-infinity distance, bracketed by the steps).
The same results at din_depth 19.6, except the power switch vs the lid: 0.4-0.5 there (the lid rises 1.3: the cap's
edge 0.45 over the lid's top level, the body 1.2 over the lid); `psw_z` can then go up ~0.5.

| pair | clearance | what sets it |
|---|---|---|
| shell vs our board / LCD-2 PCB | contact by design | the 4 bosses seat on our top (no overlap at -0.02) |
| shell vs top parts | 0.75-1.0 | the USB4085 bodies (3.46) under the face (4.4): 0.94 |
| shell vs switches | 2.0-3.0 | |
| shell vs bottom parts | 0.25-0.3 | the jack noses / DIN plates reach 0.1 past our edge, wall 0.4 out (inside the slots) |
| shell vs LCD-2 | 0.25-0.3 | the screen under the face: 0.3 by design |
| shell vs strips | >= 3.0 | |
| shell vs cell | 2.0 (the step 2.0 touches) | the right and bottom walls: the rubber gap |
| shell vs M2 screw heads | 1.0-1.5 | the walls by H1 / H2 / H3 |
| lid vs our board | 0.2-0.25 | the L's top, 0.2 by design |
| lid vs strips | 0.1-0.15 | the rails, 0.1 by design |
| lid vs bottom parts | 0.3-0.35 | the tongues vs the jack noses |
| lid vs cell (it lies on the lid) | 2.0 (the step 2.0 touches) | the L's arms: the rubber gap |
| lid vs M2 screw heads | >= 3.0 | |
| lid vs top parts | 1.5-2.0 | |
| lid vs LCD-2 | 2.0-3.0 | |
| board parts vs the cell (computed, not by the check) | J13 / J14 bodies 5.8 above it, H4's screw head 9.2 above it, the DINs 9.2 away in y | |
| power switch vs our board | >= 3.0 | 3.9 under the board's bottom |
| power switch vs top parts, bottom parts, LCD-2 | >= 3.0 | J7's body 6.8 right of the tabs; the LCD-2 (notch to y 58.8) far |
| power switch vs strips | 0.75-1.0 | the body 0.85 past the lower strip's PCB edge (y 63.15) |
| power switch vs M2 screw heads | 2.0-3.0 | H2's head 2.3 above the body |
| power switch vs lid | 1.0-1.5 | the lower strip's rails 1.05 away; the cap 1.75 over the lid's edge, the body 2.5 over the lid |
| power switch vs cell (lifted off its floor) | >= 3.0 | far |
| power switch vs shell | no overlap | it sits in the hole (the part in the hole not modelled); the cap 0.75 from the corner tower's fillet (computed) |
| cap vs switches | contact by design | the flange on the plungers |
| cap vs top parts | 0.35-0.4 | the flange's arm ends vs the parts beside the keep-out (in plan; the flange is at z 1.55+) |
| cap vs shell, lid vs shell | no overlap | |
| **board coming in from below** (every board box swept 40 mm down) vs the shell | clear | |

Simplifications: top SMD parts are their courtyards as tall as the package (conservative); the LCD block is the
backlight outline from our top to the screen front; the LCD-2's back parts one 3.5 mm block between its rows.

## Not settled (mots's call)

- **The corner towers** (outline 109.5 x 96.0), see above.
- **A 0.3 mm sliver** of the top wall stays between the USB-C window (to x 51.35) and the DIN window (from x 51.65); it
  will not print cleanly. Each 1 mm less overmold width (`usb_om_w`) adds 0.5 mm to it; or the two windows merge.
- The slots: only the LCD-2's USB-C actually crosses the wall's inner face (the jacks, DINs and our USB-C stay 0.15-0.3
  inside it), so the jack slots could be closed round holes and the lid without tongues; kept as slots + tongues as
  mots decided.
- The board under the D-pad: the L backs SW1 only; SW3 / SW4 are 7.35 mm and SW5 15.35 mm from it (H4 holds the
  board 18.75 mm right of SW5). Nothing can stand over the cell, so SW5 relies on the board between the L and H4.
- The cell's lead route (above) and the MX1.25's position are not verified; with the switch the red wire needs ~180 mm.
- The power switch's sizes are all assumed (datasheet not read), and H2's screw is reachable only with the switch out.
- The LCD-2 is held between the face (0.3 above its glass) and the strip rails (0.1 under the strips): nothing presses
  it; a thin foam pad on the glass edge would take the play.

## Rebuild

```bash
cd hardware/jank-mini-3/case && /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 board_model.py   # only after a board change
cd hardware/jank-mini-3/case && sh make_renders.sh && python3 floor_map.py && python3 box_check.py   # python3 box_check.py din_depth=19.6
cd hardware/jank-mini-3/case && openscad -D 'part="shell"' -o shell.stl box.scad      # stage 2: shell | lid | cap
```
