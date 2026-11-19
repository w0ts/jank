# jank-mini-3 box: STAGE 1 (Claude, 2026-11-19), for mots's approval

`box.scad` (parametric OpenSCAD), the board as boxes in `board_model.scad` (written from `../lcd2.kicad_pcb` by
`board_model.py`, KiCad's python), pictures by `make_renders.sh`, the floor map by `floor_map.py`, the collision check
`box_check.py`. The older `case.scad` (93 x 62.6 layout) is untouched. STLs: `box-shell.stl` + `box-shell-labels.stl`
(the face's inlaid labels, second colour, same origin), `box-lid.stl`, `box-cap.stl` + `box-cap-symbols.stl` (the
cap's inlaid symbols, second colour, same origin), exported in the ASSEMBLY orientation (turn them in the slicer:
shell face down, lid outside down, cap top down; load each part with its inlays as one multi-part object so they stay
aligned).

**Cap symbols** (mots, 2026-11-19; multi-material, AMS / MMU): a solid up-pointing triangle on the UP arm (SW3), a
solid down-pointing one on the DOWN arm (SW4) (as jank's own page D-pad, no stem), an X (NO) on the left arm (SW1), a
tick (YES) on the right arm (SW5), the X and the tick in the triangles' family (mots: bold, flat / mitred ends, no
round caps); mapping checked: parts.py SW1 = NO (BTN_NO), SW3 = UP, SW4 = DOWN, SW5 = YES, and silk.py's labels put
NO left of the D-pad (x 57.5), YES right (75.5), UP above, DOWN below. 2D polygons in box.scad (no font), all four
centred 8.7 mm out on their arms (`sym_r`: between the pad at r 8 and the arm's end; the flat top ends at r 11.4):
triangles 5.0 wide x 4.0 tall (`sym_tri`, corners rounded 0.3 so the tip prints); the X two 1.7 mm bars cut flush by
its 4.6 square (`sym_x`, `sym_bold`); the tick a 1.7 mm stroke with square ends and a mitred corner, 4.96 x 4.6
(`sym_tick`); all at least 0.2 inside the top's 0.6 chamfer (checked: the inlays lie inside the cap's body). Inlaid
flush: cut 0.6 deep (`sym_depth`, 3 layers of 0.2) into the cap's top face; `box-cap-symbols.stl` is the same shapes
0.6 thick, filling the cuts exactly (printed top down, the symbols are the first 3 layers). Picture: `box-cap-top.png`.

| picture | what |
|---|---|
| `box-assembly.png` | closed box, 3/4 view from the front left |
| `box-exploded.png` | shell up, cap above it, board, lid down |
| `box-top.png` | the face from above: screen window, D-pad, the inlaid labels and cap symbols in the second colour |
| `box-inside.png` | the lid with the parts under the board (shell and our board hidden): strip rails, the cell in its corner, the L, the screw tabs, the hook, tongues, the power switch |
| `box-section-din.png` | section x = 64.75 (J5's DIN, the D-pad centre: the cap's pivot nub and the UP / DOWN pads, the L's top arm, the cell), seen from the right |
| `box-section-screen.png` | section y = 41 (the D-pad centre, the screen, the LCD-2's USB-C slot, the L's left arm, the cell), seen from the front |
| `box-left-wall.png` | the left wall from outside: the LCD-2's USB-C slot (its lid tongue), the power switch's cap, the left lid screw |
| `box-section-switch.png` | section y = 72.9 through the power switch (seen from the front, the top half kept) |
| `box-cap-top.png` | the D-pad cap from above, its inlaid symbols in the second colour |
| `floor-map-corner.png` | the floor from above (every number read from box.scad): bottom parts and depths, strips, cell + rubber gap, the L, the lid's screw tabs and hook, the power switch, the cell's lead route with lengths, D-pad switches, screws |

## The parts

- **Top shell**: the face and the four walls, full depth, a plain rounded rectangle 100.3 x 86.8 (no towers). The
  board (with the LCD-2 and the strips plugged on) drops in from below and screws to 4 bosses on the face's underside
  through H1-H4. **Printed face down** (the engraved labels and the window chamfer on the bed side). The USB-C window's
  lower edge prints as a 25.8 mm bridge.
- **Bottom lid**: flat, 2.0 mm, with the parts that hold things from below: two rails under each bridge strip, the
  cell's L, three screw tabs and a hook (below) and tongues that close the wall slots under the jacks and the LCD-2's
  USB-C. **Printed outside down**: everything grows straight up, no bridges.
- **The DIN lock** (mots, 2026-11-19: "a rail to lock the din so that the effort is not on the pcb"), on the lid:
  a **rail** behind both DIN bodies (x 52.05-92.6, y 15.385-19.4, from the lid up to 0.2 under our board; it joins the
  L's top arm and stops 1.5 short of the hook's tab so the tab can flex) takes a plug's push; a **tongue** in the DIN
  window (its width less 0.2 a side) rises 2.0 over the lid, in front of the DIN plates' lowest 1.5 mm, and takes a
  plug's pull. ASSUMED: the DIN-504B's plug opening starts more than 1.5 mm above its bottom (`din_lock_h`): **measure**.
- **The lid's fixing** (mots, 2026-11-19: no corner towers): SIDE SCREWS. Three tabs rise from the lid just inside
  the walls; an M3 countersunk screw goes sideways through the wall into an M3 heat-set insert in each tab, its head
  flush in a countersink in the wall. The right wall has no free stretch for a screw (J6's DIN reaches the floor to
  y 15.19, the cell's rubber starts at y 22.4, then the CLK / RST slots and the cell), so it gets a **hook**: a thin
  tab whose barb catches in a small window in the right wall. Fitting: hook the lid's right side in first (tilted),
  swing the left side up, then the three screws.

  | | wall | position (board mm) | screw axis height | tab (along x inward x high) |
  |---|---|---|---|---|
  | screw 1 | top | x 37.0 (between J3's slot, x <= 23.1, and the DIN window, x >= 51.65) | z -19.0: 4.0 over the lid's top, 6.0 over the box's bottom | 8.0 x 6.0 x 8.0, at y -0.2..5.8 |
  | screw 2 | bottom | x 46.0 (between J7's slot, x <= 34, and the cell's rubber, x >= 57.4) | z -19.0 | 8.0 x 6.0 x 8.0, at y 76.2..82.2 |
  | screw 3 | left | y 10.0 (above the LCD-2's USB-C slot, y >= 34.6; J2's body hangs over it only to z -7.1) | z -19.0 | 8.0 x 6.0 x 8.0, at x -0.2..5.8 |
  | hook | right | y 18.8 (centred in the free 7.2 mm between J6 and the cell's rubber) | barb's catching face z -17.0, window 5.6 x 2.8 through the wall | 5.0 x 1.6 x 9.0, barb 1.0 into the wall |

  Each tab: an insert hole d 4.0 x 5.0 deep from the wall side, 2.0 of tab round it on the sides and above (>= 1.6),
  the lid below. The tabs stand 0.2 inside the walls (the screw pulls them tight) and only 8 mm high, so they pass
  under every bottom part (the jacks hang to z -7.1, the tabs reach z -15.0). The board does not meet them: they come
  with the lid, after it. At din_depth 19.6 everything moves up 1.3.
- **The cell** (mots, 2026-11-19): EEMB LP103454 lying on the lid in the BOTTOM-RIGHT corner, 34.5 along x, 56
  along y, pushed into the corner: **x 59.4-93.9, y 24.4-80.4** (board coordinates), a `rubber_gap` of 2.0 mm on all
  four held sides (right wall, bottom wall, the L's two arms) for mots's rubber / foam. Its top is at z -12.4: it lies
  under J13 / J14 (5.8 mm above it) and H4's screw head (9.2 above), not under the DINs (they end at y 15.19).
- **The L** (`L_top_t` 3.0, `L_left_w` 4.4, `L_top_len` 20, `L_left_len` 36): from the lid to 0.2 under our board,
  outer corner at (53.0, 19.4). Top arm x 53.0-73.0, y 19.4-22.4; left arm x 53.0-57.4, y 19.4-55.4. It backs **SW1**
  directly (its centre x 56.75 lies on the arm). The others, nearest point of the L to the switch centre: **SW3** 7.35,
  **SW4** 7.35, **SW5** 15.35 mm; H4's screw holds the board 18.75 mm right of SW5.
- **The power switch** (mots, 2026-11-19): YIZHI KCD1-106S-B (LCSC C52125269), a round snap-in rocker, 2 pins, in
  the **LEFT wall**, centre **board y 72.9, height z -13.0** (11.4 under our board's bottom, 10.0 over the lid). All
  its sizes ASSUMED (datasheet not read): hole d 15.0, cap d 16.5 x 2.0 proud, body d 15 x 14 deep, 2 tabs in an
  8 x 4 x 8 envelope. The wall is thinned from the inside to **1.6 mm** in a d 19 patch round the hole. Inside it fills
  x -0.8..17.2, y 65.4-80.4, z -20.5..-5.5. **Why y 72.9**: with the corner tower gone, the limit is the box's
  rounded corner: the flat outside of the left wall ends at y 81.4, the cap ends at y 81.15; the thinned patch ends at
  the bottom wall's inner face (y 82.4). **H2 (3, 64.5) is still not reachable with the switch in**: the body's
  nearest edge passes 0.9 mm from H2's screw axis (centre 8.4 from it, radius 7.5), so any screwdriver shaft of 2 mm
  or more hits it; take the switch out (squeeze its clips from inside, lid off) to reach H2. H3 stays reachable.
- **The cell's lead** (a floor sketch, `floor_map.py` draws and adds it up): both wires leave the cell's top end right
  of the L's top arm, run left in the 4.2 mm channel between J5 (y <= 15.19) and the L's top arm (y >= 19.4), and
  turn down in the gap between the strips' right ends (x 43.1) and the L's left arm (x 53.0). **Black** goes on to
  y ~41 and left under the LCD-2 to its MX1.25 (on the LCD-2's back, at about x 23, scaled from Waveshare's back view:
  NOT verified, its y not known): **~87 mm**. **Red** (cell -> switch -> MX1.25) goes down that gap to y ~65.5, left
  on the floor between the lower strip and J7 to the switch's tabs (x ~17, y 72.9): ~124 mm; then up along the left
  wall at x ~4 to y ~50 and over to the MX1.25: ~58 mm; **red ~182 mm** in all, plus slack. The cell's ~60 mm lead and
  the 10 cm adapter pigtails do not reach that far for the red: mots makes the red pigtail (or the switch's wires)
  long enough.
- **D-pad cap** (mots, 2026-11-19: centre pivot, wide lips, generous tolerances): the 24 mm cross (8 mm arms)
  through the face, 2.0 proud. **Printed top down** (the chamfered top on the bed; the lips a 1.2-2.75 mm ledge; the
  nub and pads up).
  - **Pivot nub** at the centre (64.75, 41): d 3.5 with a ball end r 1.75 (`cap_pivot_r`), the only contact at rest,
    on the bare board (no part, track or via within 3 mm, mots).
  - **Pads** d 3.0 x 0.5 (`cap_pad`) under the arms over the four actuators (+/-8), **0.15 over the switch tops**
    (`cap_preload_gap`) at rest. Switch travel 0.25 (`sw_travel`, ASSUMED: TS-1187A-B-A-B datasheet not read).
  - **Flange**: underside z 2.2, top z 3.1 (0.9 thick), **1.3 under the face** (`cap_play`). **Lips** (`cap_lips`,
    [side, end] per arm): NO (SW1) 2.75 / 2.75, DOWN (SW4) 2.75 / 2.75, UP (SW3) 2.75 sides / 1.2 end, YES (SW5)
    1.2 / 1.2. Narrow where a part sits just past the keep-out and a tilted lip would land on it: U21 and D10 / D11
    (CLOCK / RESET) round the YES arm's end, Q13 beyond the UP arm's end. Under the face they overlap 2.15 (wide) and
    0.6 (narrow) with the cap centred; the cross cannot turn (its arms in the cross-shaped opening) or fall out.
  - **Face opening**: the cross + **0.6 a side** (`cap_clr`).
  - **Tilt**: a full press on an arm (its pad down 0.15 + 0.25 = 0.4 at r 8) turns the cap **2.86 deg** about the nub;
    the far lips rise up to 0.74 (1.3 free); the cross shifts sideways 0.34 at the face's top (0.6 free). A full press
    between two arms (both pads down 0.4, at 5.66 from the tilt axis) turns it **4.04 deg**; the far lip corners rise
    1.07 (0.23 left under the face); the cross shifts 0.48 along the diagonal (0.34 on each axis).
  - **Which switches close**: on an arm press the two perpendicular pads stay at their 0.15 gap (they sit on the tilt
    axis) and the opposite one lifts to 0.55: only the pressed switch closes. A press between two arms closes BOTH
    (both pads go down 0.4). With a contact point ~0.2 into the 0.25 travel (ASSUMED), a second switch closes only
    when the press is within ~4 deg of the 45 deg diagonal (its pad needs 0.35 while the first stops at 0.4: tan of the
    angle from the arm >= 0.875, 41 deg); an arm is 18 deg wide at its end, so a press anywhere on an arm farther
    than ~4.6 mm from the centre presses only its own switch; only presses at the cross's inner corners close two.
    The check confirms it switch by switch for the 4 arm presses and the 4 diagonals.
- The LCD-2's BOOT / RESET stay covered (the left-wall slot is only the USB-C plug's width).

Inserts and screws (brass heat-set inserts; check the hole size against the maker's sheet of the inserts bought):

| where | insert | hole in the print | boss | screw |
|---|---|---|---|---|
| board -> shell (H1-H4) | M2 x 4.0 (OD ~3.6) | d 3.2, 5.0 deep (0.6 into the 2.4 mm face) | d 6.0, from the face to our board top | 4 x M2 x 5 pan head (ISO 7045), from below |
| lid -> shell | M3 x 4.0 (OD ~4.6), pressed into the tab from its wall side | d 4.0, 5.0 deep | the tab 8.0 x 6.0 x 8.0 | 3 x M3 x 6 countersunk (ISO 10642), sideways through the 2.0 wall (a 90 deg countersink d 6.4 x 1.5, 0.5 of straight wall under it) + 0.2 gap, ~3.8 into the insert |

The 2.0 wall is thin for a countersunk head: the cone takes 1.5 of it. If the heads pull in, the walls go to 2.4
(`wall`; outline 101.1 x 87.6) or the screws become pan heads standing 1.65 proud.

## Outside dimensions

| | din_depth 20.9 (HOOYA, default) | din_depth 19.6 (LCSC model) |
|---|---|---|
| footprint (a plain rounded rectangle, corner r 3) | 100.3 x 86.8 | 100.3 x 86.8 |
| height, lid bottom to face top | 31.8 | 30.5 |
| with the D-pad cap | 33.8 | 32.5 |
| beyond the outline | the power switch's cap, 2.0 out of the left wall | |

Heights about our board's top (z 0): face top 6.8, face underside 4.4, board bottom -1.6, DIN bottom -22.5, lid top
-23.0, lid bottom -25.0 (at 20.9).

## Openings

| | where | size |
|---|---|---|
| screen window | x 4.9-46.7, y 25.2-56.8 (board mm) | the active area 40.8 x 30.6 + 0.5 a side = 41.8 x 31.6, 45 deg chamfer 1.6 from the top |
| D-pad | centre (64.75, 41) | the cross + 0.6 a side (25.2 / 9.2) |
| USB-C TO ESP + TO DEVICE (J1, J4) | top wall, x 25.55-51.35, z -1.8..+5.5 | one window for both overmolds (12.0 + 0.4 a side, 13 mm apart): a closed window, 1.3 mm of face over it |
| MIDI 1 / 2 TRS (J2, J3) | top wall, x 6.3 / 19.1, axis z -4.1 | d 8.0, a slot open to the wall's bottom edge, closed by a lid tongue |
| MIDI 1 / 2 DIN (J5, J6) | top wall, x 51.65-95.45, from the bottom edge to z -2.2 | the two DIN plates + 0.4 a side; they are 1.2 mm apart, so ONE window; 0.6 mm of wall stays under the board |
| CLK / RST (J13, J14) | right wall, y 31 / 51, axis z -4.1 | d 8.0 slots, lid tongues |
| LINE IN (J7) | bottom wall, x 30, axis z -4.1 | d 8.0 slot, lid tongue |
| the LCD-2's USB-C | left wall, y 34.6-47.4, z -6.9..+0.4 | its plug's overmold (12.0 x 6.5 + 0.4) about its axis (z -3.2, ASSUMED), slot to the bottom edge, lid tongue |
| the power switch | left wall, centre y 72.9, z -13.0 | a closed round hole d 15.0 (ASSUMED), the wall thinned to 1.6 in a d 19 patch inside |
| lid screws | top x 37, bottom x 46, left y 10; z -19.0 | d 3.4 with a 90 deg countersink d 6.4 |
| the hook's window | right wall, y 15.99-21.59, z -17.3..-14.5 | 5.6 x 2.8 through the wall |

**Face labels** (mots, 2026-11-19: a second colour, multi-material): inlaid like the cap's symbols, cut 0.6 deep (3
layers; the shell prints face down, so they are its first 3 layers) into the face's top; `box-shell-labels.stl` is
the same shapes 0.6 thick filling the cuts, same origin as `box-shell.stl`. Font **Avenir Next Condensed Heavy**, a
macOS system font (`/System/Library/Fonts/Avenir Next Condensed.ttc`, found on this Mac; another machine without it
falls back to another font: check the picture), **size 2.2** (mots: 5.4 was "too big ! make the text half size",
then "a bit smaller"), letter spacing 1.1, outlines thinned 0.02 (`lab_h`, `lab_spacing`, `lab_thin`). At 5.4 a
morphological test gave strokes >= 0.9 and gaps >= 0.55 mm; at 2.2 that scales to **strokes about >= 0.37 and gaps
>= 0.22 mm (not re-measured)**: under a 0.4 nozzle's line (thin strokes may print patchy, the E's arms may merge);
meant for a 0.2 nozzle. A small triangle (the
cap's shape, 2.6 x 2.1, `lab_tri`) points at each socket (mots: "use our triangle to point at the socket"):

| label | where (board mm) |
|---|---|
| top edge, from the wall in | triangles (y 2.4) at each socket, then the names (y 5.2); no "TO" (mots) |
| 1 MIDI 2 (TRS) | 1 at x 6.3, MIDI at x 12.7 (between them), 2 at x 19.1 |
| ESP, DEVICE (USB-C) | ESP at x 31.95, DEVICE at x 44.95 |
| 1 MIDI 2 (DIN) | 1 at x 62.5, MIDI at x 73.55, 2 at x 84.6 |
| ESP (the LCD-2's USB-C, left wall) | triangle pointing left at x 0.4, y 41; the word running up the edge below it (y 46.6): only ~5.7 mm between the face's edge and the window's chamfer (mots: "we miss the esp socket on the left") |
| CLK, RST | text at x 88.5, y 31 / 51; triangle pointing right at x 93.3 |
| LINE IN | text at x 30, y 77; triangle pointing down at y 80.2 |

Picture: `box-top.png`.

**Bambu Studio projects** (mots, 2026-11-19: everything in **PLA Basic**, black, pink and grey; the nozzle is per
project, so two files):
- `box-shell-0.2.3mf`: the shell (black, filament 1), face down, labels pink (filament 2). **P2S 0.2 nozzle**,
  0.10 mm Standard (the 2.2 mm text's ~0.37 mm strokes are under a 0.4 nozzle's line).
- `box-lid-cap-0.4.3mf`: plate 1 the lid (black, outside down), plate 2 the D-pad cap (grey, filament 3; face down,
  symbols pink, 1.7 mm strokes). **P2S 0.4 nozzle**, 0.20 mm Standard.

Both: three Bambu PLA Basic filaments (1 black #000000, 2 pink #F5A3C7, 3 grey #8E9089: mots matches them to the
AMS), Textured PEI plate. PLA softens from ~55 C (no car, no sun) and is brittle: the lid's hook bends ~5 % at its
root (estimate, not measured), more than PLA takes. Built with the Bambu Studio CLI (`--load-assemble-list`, the
system presets flattened through their `inherits` chain first: loaded alone, the P2S preset lacks its 256 mm bed),
then patched for what the CLI leaves out (objects missing from their plates, the bed type, the colours, the object
names). The CLI loads and validates them but crashes slicing a P2S (nozzle grouping), so they are not sliced here:
mots slices in the app.

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
| `cell` | EEMB LP103454, 34.5 x 56 x 10.6; x 59.4-93.9, y 24.4-80.4, lead end -y | DATASHEET; position mots |
| `rubber_gap` | 2.0 on every held side (walls, the L) | mots |
| `L_top_t`, `L_left_w`, `L_top_len`, `L_left_len` | 3.0, 4.4, 20.0, 36.0 | design (mots: arms not the cell's whole length) |
| `LID_SCREWS` | top x 37, bottom x 46, left y 10 | design (free low wall, see the table above) |
| `m3_ins`, `m3_cs`, `tab`, `tab_clr`, `tab_screw_h` | d 4.0 x 5.0; d 3.4 / 6.4 / 1.5; 8 x 6 x 8; 0.2; 4.0 | DATASHEET (typical M3 x 4 insert, ISO 10642) / design |
| `hook_y`, `hook`, `hook_barb`, `hook_clr` | 18.8; 5.0 x 1.6 x 9.0; 1.0 deep, 1.2 catch, 1.0 lead-in; 0.3 | design |
| `psw_y`, `psw_z` | 72.9, -13.0 (left wall) | design (mots: left wall, bottom-left) |
| `psw_hole`, `psw_cap`, `psw_body`, `psw_tabs` | d 15.0; d 16.5 x 2.0 proud; d 15 x 14 deep; 8 x 4 x 8 | ASSUMED (KCD1-106S-B datasheet not read; mots measured the cap ~16): **measure** |
| `psw_panel`, `psw_thin_d` | 1.6 wall at the switch, a d 19 patch | ASSUMED (clips grip 1.0-2.0; their reach) |
| `plug_d` | 8.0 (TRS plug bodies up to 7.2) | ASSUMED: check the MIDI TRS and CV cables |
| `fit`, `wall`, `face_t`, `face_gap` | 0.4, 2.0, 2.4, 0.3 | design (PETG, 0.4 nozzle) |
| `cap_clr`, `cap_lips`, `cap_play`, `cap_proud` | 0.6; see the cap; 1.3; 2.0 | design (mots: generous PETG tolerances) |
| `cap_pivot_r`, `cap_pad`, `cap_preload_gap` | 1.75; d 3.0 x 0.5; 0.15 | design (mots) |
| `sym_depth`, `sym_r`, `sym_tri`, `sym_tri_r`, `sym_x`, `sym_bold`, `sym_tick` | 0.6; 8.7; 5.0 x 4.0; 0.3; 4.6; 1.7; 4.96 x 4.6 | design (mots: solid triangles ~4.5-5 x 4, the X / tick bold 1.6-1.8 with flat ends; 3 layers) |
| `sw_travel` | 0.25 | ASSUMED (TS-1187A-B-A-B datasheet not read) |
| `tongue_clr`, `din_clr`, `om_clr`, `strip_post_gap`, `board_gap` | 0.2, 0.4, 0.4, 0.1, 0.2 | design |

## What mots measures first (on the real parts)

1. **One DIN-504B-M10's depth below the board** (20.9 or 19.6): sets the box height (lid top = DIN bottom - 0.5).
2. **The LCD-2**: its PCB thickness (its top level with ours?), the screen front's height above its PCB top (4.1 +
   1.6 assumed: face underside at +4.4), the screen's active area position (2.3 mm from the far end of the
   backlight, read from the drawing).
3. **The LCD-2's USB-C**: how far below its PCB its axis is (3.2 assumed) and how far past our left edge it reaches
   (0.7); where BOOT / RESET sit: they must stay behind the left wall.
4. **The jacks**: the PJ-307C / PJ-320A axis 2.5 mm under the board (from the models; the PJ-307C model's nose
   reaches 0.5 mm past its seating plane, into the board's thickness: check where the real nose sits).
5. **A bridge strip plugged on**: the height of its underside below our board's bottom (12.6 assumed + 1.6 strip),
   its tails' length.
6. **The plugs**: the USB-C cables' overmold width (12.0 assumed) and the TRS plug bodies (7.2 max assumed).
7. The cell's real size and where along its 34.5 mm end the lead leaves (the route wants it right of x 73);
   where the LCD-2's MX1.25 is on its back.
8. The inserts and screws bought (hole sizes 3.2 / 4.0 assumed; the M3 countersunk head's diameter vs the 6.4 seat).
9. **The power switch (KCD1-106S-B)**: its panel hole, cap diameter and height, body diameter and depth, the tabs'
   length, the panel thickness its clips grip and how far out the clips reach.
10. **The tact switches (TS-1187A-B-A-B)**: their height (1.55 from the model) and travel (0.25 assumed): they set
    the cap's pads (0.15 over them) and its tilt.

## Clearances measured (box_check.py, OpenSCAD manifold)

Each board group grown in x, y and z alike until it meets a case part (L-infinity distance, bracketed by the steps).
The same results at din_depth 19.6, except the power switch vs the lid: 0.4-0.5 there (the lid rises 1.3: the switch
cap's edge 0.45 over the lid's top level); `psw_z` can then go up ~0.5.

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
| lid vs bottom parts | 0.3-0.35 | the tongues vs the jack noses; the screw tabs 7.9 under J2 |
| lid vs cell (it lies on the lid) | 2.0 (the step 2.0 touches) | the L's arms: the rubber gap |
| lid vs M2 screw heads, top parts, switches, LCD-2 | >= 1.5 | |
| lid vs shell | no overlap | the tabs 0.2 inside the walls, the barb 0.3 round in its window |
| power switch vs our board, top / bottom parts, LCD-2, cell, M2 screw heads | >= 3.0 | |
| power switch vs strips | 2.0-3.0 | |
| power switch vs lid | 1.5-2.0 | the switch cap 1.75 over the lid's edge |
| power switch vs shell | no overlap | the cap ends 0.25 before the corner's rounding (computed) |
| cap vs our board | contact by design | the pivot nub |
| cap vs switches (at rest) | 0.1-0.15 | the pads' 0.15 gap |
| cap vs top parts (at rest) | 1.0-1.5 | |
| cap vs top parts, full press on an arm | 0.3-0.35 (YES), 0.4-0.5 (UP), 0.6-0.75 (DOWN), 1.0-1.5 (NO) | the lowered lips over the parts beside the keep-out |
| cap vs top parts, full press between two arms | 0.25-0.3 (YES + UP, YES + DOWN), 0.5-0.6 (NO + UP), 0.75-1.0 (NO + DOWN) | |
| cap vs shell, at rest and in all 8 presses | no overlap | the face never stops the tilt |
| cap pressed vs each switch | only the pressed one (arm presses), the two (diagonals) | switch by switch |
| **board coming in from below** (every board box swept 40 mm down) vs the shell | clear | |

Simplifications: top SMD parts are their courtyards as tall as the package (conservative); the LCD block is the
backlight outline from our top to the screen front; the LCD-2's back parts one 3.5 mm block between its rows.

## Not settled (mots's call)

- **A 0.3 mm sliver** of the top wall stays between the USB-C window (to x 51.35) and the DIN window (from x 51.65); it
  will not print cleanly. Each 1 mm less overmold width (`usb_om_w`) adds 0.5 mm to it; or the two windows merge.
- The USB-C window's lower edge prints as a 25.8 mm bridge (shell face down).
- The slots: only the LCD-2's USB-C actually crosses the wall's inner face, so the jack slots could be closed round
  holes and the lid without tongues; kept as slots + tongues as mots decided.
- The lid: 3 side screws + the right-wall hook (no room for a 4th screw); the countersinks take 1.5 of the 2.0 walls.
- The board under the D-pad: the L backs SW1 only; SW3 / SW4 are 7.35 mm and SW5 15.35 mm from it.
- The cell's lead route and the MX1.25's position are not verified; the red wire needs ~182 mm.
- The power switch's sizes are all assumed, and H2's screw is reachable only with the switch out.
- The cap: its flange is 0.9 thick (between the pads' height and the 1.3 play the tilt needs); the narrow lips (YES
  arm, UP end) overlap the face by 0.6; diagonal presses close two switches.
- The LCD-2 is held between the face (0.3 above its glass) and the strip rails (0.1 under the strips): nothing presses
  it; a thin foam pad on the glass edge would take the play.

## Rebuild

```bash
cd hardware/jank-mini-3/case && /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 board_model.py   # only after a board change
cd hardware/jank-mini-3/case && sh make_renders.sh && python3 floor_map.py && python3 box_check.py   # python3 box_check.py din_depth=19.6
cd hardware/jank-mini-3/case && for p in shell lid cap; do nice -n 19 openscad --backend=manifold -D "part=\"$p\"" -o box-$p.stl box.scad; done; for p in cap_symbols shell_labels; do nice -n 19 openscad --backend=manifold -D "part=\"$p\"" -o "box-$(echo $p | tr _ -).stl" box.scad; done
```
