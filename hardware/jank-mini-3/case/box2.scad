// SPDX-License-Identifier: GPL-3.0-only
// jank-mini-3 box: THE CURRENT DESIGN (mots, 2026-11-22; box.scad, the one-piece shell and lid, is the older one, kept for reference):
// a TOP part (the face and the walls down to a stepped parting line) and a BOTTOM part (the floor and the walls up to
// it), screwed together, plus the printed cross D-pad cap. FDM, PLA, 0.4 mm nozzle, 0.2 mm layers.
// mots (2026-11-20): "the join line between top and bottom at the middle of each hole (except the power switch hole)",
// "tab on the bottom part that go inside the top part and both lock the bottom/top aligned", then "the tube coming up
// from the bottom lid should be wider/bigger/stronger" and "the top of the tube should hold the screw head, and the
// screw sandwiches the pcb between bottom and top" (no snap tabs).
//   top:    printed FACE DOWN (every hole is a half-hole open at the joint edge: no bridges)
//   bottom: printed OUTSIDE DOWN (walls, the lip, the screw tubes, rails, the cell's L grow up)
//   cap:    printed TOP DOWN (its chamfered top on the bed; the flange a 2.75 mm ledge; the pivot nub and pads up)
// Assembly: the cell (with its rubber) into the bottom part, between the L and its own wall; our board into the top
// part; the bottom part on (its inner lip rises inside the top part's outer skirt: alignment); four M2 x 6 screws from
// under the box, up the screw tubes at H1-H4: each head sits on its tube's shoulder, the screw goes through our board
// into the face boss's insert, so it holds the bottom part, our board and the top part together. The power switch
// snaps into the bottom wall last.
// (mots, 2026-11-21: "we can slide the battery left a bit, enough to let the pole fit. and we move the wall and make a
// double wall on the right of the battery": the cell moved left, its own wall on its right, H4's tube between that wall
// and the box's right wall, H4's old short screw from below gone)
//
// Coordinates: BOARD mm as make_board.py (x right, y DOWN from the board's top-left corner); z = 0 is OUR BOARD'S TOP
// (its bottom is z -1.6). In OpenSCAD space Y = -y (so the top wall is at +Y, the pictures read like the board).
// Every dimension that is not read from the board files says where it comes from: BOARD (make_board.py /
// lcd2.kicad_pcb), MODEL (../lib/3d model bboxes), DRAWING (Waveshare ESP32-S3-LCD-2-20250122.pdf), README (this
// board's README), DATASHEET, or ASSUMED (mots measures it; see BOX.md).
//
// openscad -D 'part="top_part"' -o x.stl box2.scad      part = assembly | exploded | top_part | bottom_part | cap |
//                                                              board | outside | inside | tube_section | check

part = "assembly";
$fn = 48;

// ---------------------------------------------------------------------------------------------------------------
// the board (BOARD: make_board.py)
W = 95.5;  H = 82.0;  T = 1.6;                    // BOARD: 95.5 x 82, 1.6 mm (README: order 1.6)
LX = 0.5;  LY = (H - 35.01) / 2;                  // BOARD: the LCD-2's PCB corner in the notch
CUT = [0, LY - 0.3, LX + 48.21 + 0.3, LY + 35.01 + 0.3];   // BOARD: the notch, open to the left edge
MOUNT = [[3.0, 17.6], [3.0, H - 17.5], [40.5, H - 3.0], [W - 4.0, H / 2]];   // BOARD: H1..H4, M2
HOLE_D = 2.4;                                     // BOARD: HOLE_M2_2.4
DPX = 49.0 + (W - 15.0 - 49.0) / 2;  DPY = H / 2; // BOARD: the D-pad centre (64.75, 41)
SW_OFF = 8.0;                                     // BOARD: SW1/3/4/5 at +/-8 around it
SW_H = 1.55;                                      // MODEL: TS-1187A plunger top above the board (lib/3d, 1.55)
J1X = 31.95;  J4X = 44.95;                        // BOARD: USB-C TO ESP / TO DEVICE (top wall, on top)
J2X = 6.3;  J3X = 19.1;                           // BOARD: MIDI 1 / MIDI 2 TRS (PJ-307C, top wall, under)
J5X = W - 33.0;  J6X = W - 10.9;                  // BOARD: MIDI 1 / MIDI 2 DIN (top wall, under)
J13Y = H / 2 - 10.0;  J14Y = H / 2 + 10.0;        // BOARD: CLOCK / RESET (PJ-320A, right wall, under)
J7X = 30.0;                                       // BOARD: LINE IN (PJ-307C, bottom wall, under)
ROW_X = LX + 48.21 - (7.60 + 40.62) / 2;          // BOARD: the rows' centre x (24.6)
J16Y = LY + 2.265 - 3.81;  J15Y = LY + 32.745 + 3.81;     // BOARD: our male rows
P2Y = LY + 2.265;  P1Y = LY + 32.745;             // BOARD: the LCD-2's rows

// the parts (heights)
din_depth = 20.9;          // ASSUMED: DIN-504B-M10 below the board: HOOYA drawing 20.9 (LCSC/JLC model 19.6). MEASURE
din_w = 20.9;              // MODEL: its front plate 20.9 wide (x +/-10.45), y -8.61..+6.69 about its origin
jack_axis = -T - 2.5;      // MODEL: both jacks' noses (PJ-307C d 6, PJ-320A d 5) are centred 2.5 under their seat
usb_h = 3.46;              // README / GCT drawing: USB4085 stands 3.46 above the board
usb_om_w = 12.0;           // ASSUMED: a USB-C plug's overmold width (cables vary 10.5-12.5)
usb_om_z = [-1.4, 5.1];    // README (9B N4): the plugs' overmolds span -1.4..+5.1 about our board's top
lcd2_usb_out = 0.7;        // README: the LCD-2's own USB-C reaches 0.7 past our left edge
lcd2_usb = [7.35, 8.94, 3.26];   // ASSUMED: a 16-pin SMD USB-C receptacle (length, width, height), on the LCD-2's back
lcd2_usb_y = LY + 35.01 / 2;     // DRAWING: centred across the LCD-2 (y 41.0)

// the LCD-2 (DRAWING; README)
lcd2_pcb_t = 1.6;          // ASSUMED: the LCD-2's PCB (its top level with ours, README). MEASURE
lcd_front = 4.1;           // DRAWING (scaled, README): screen front above our board top (5.70 - 1.6). MEASURE
lcd_t = 2.1;               // DRAWING: the LCD + backlight, 2.10 +/- 0.05
lcd_bl = [47.8, 34.6];     // DRAWING: backlight outline (along x on our board, across)
lcd_aa = [40.8, 30.6];     // DRAWING: active area
lcd_aa_far = 0.5 + 1.8;    // DRAWING: AA starts 2.3 from the BL edge AWAY from the USB-C (the right edge here)
lcd2_back_h = 3.5;         // ASSUMED: the LCD-2's back parts (ESP32-S3, MX1.25, USB-C) hang 3.5 below its PCB
lcd2_hdr = 2.5;            // README: the LCD-2's own header plastic (same stack as our C2905490)

// the bridge strips (README, strip/make_strip.py)
strip_L = 37.06;  strip_W = 10.0;  strip_t = 1.6;  // strip/make_strip.py: 37.06 x 10, 1.6 (ASSUMED thickness)
hdr_plastic = 2.5;  sock_h = 8.5;                  // README: C2905490 2.5 mm plastic, C2905420 8.5 mm sockets
strip_tail = 1.5;                                  // ASSUMED: the sockets' soldered tails under the strip
strip_z0 = -T - hdr_plastic - sock_h - strip_t;    // the strip's underside: -14.2 (README "about 14 below")
strip_y = [(J16Y + P2Y) / 2, (J15Y + P1Y) / 2];    // the two strips' centres (23.855, 58.145)
strip_post_gap = 0.1;      // the lid's rails stop 0.1 under the strip (hold it up, no preload)
strip_rail_off = [3.2, 4.8];   // rails under the strip's long edges, clear of the pin rows (+/-1.905) and tails

// the cell (mots, 2026-11-19, after the floor map): EEMB LP103454 on the lid in the BOTTOM-RIGHT corner, held by the
// bottom wall, its own wall on its right (H4's screw tube stands between that wall and the box's right wall: mots,
// 2026-11-21) and an L on its top-left corner, a rubber / foam gap on every held side; its lead end -y
cell = [34.5, 56.0, 10.6];     // DATASHEET: EEMB LP103454 (x, y, thickness)
rubber_gap = 2.0;              // mots: cell -> walls and the L, filled with rubber / foam (the cell: x 51.2-85.7,
                               //  y 24.4-80.4 at the defaults; the DINs end at y 15.19, MODEL)
// the L: from the lid up; its top arm to board_gap under our board (it backs the board too), its left arm (under the
// LCD-2 since the cell moved left) to 0.3 under the LCD-2's back parts
L_top_t = 3.0;                 // the top arm's thickness (along the cell's top side)
L_left_w = 4.4;                // the left arm's width: from the rubber gap leftward (x 44.8-49.2; the strips end at 43.13)
L_top_len = 20.0;              // the top arm, from the L's outer corner (x 53.0 .. 73.0)
L_left_len = 36.0;             // the left arm, from the L's outer corner (y 19.4 .. 55.4: past SW4's y 49 + 6.4)

// the power switch (mots, 2026-11-19): YIZHI KCD1-106S-B (LCSC C52125269), a round snap-in rocker in the BOTTOM wall's
// left part (mots, 2026-11-21: moved from the left wall, so H2's screw tube is whole), in the cell's red lead
// (cell -> switch -> the LCD-2's MX1.25): no board connection.
// Its datasheet drawing was not read: every size ASSUMED (mots measured the cap ~16 across)
psw_x = 11.5;              // its centre, board x: the mirror of the left wall's spot (x 9.1) + 2.4, so the tabs (x
                           //  7.5-15.5) clear H2's tube (x <= 7.0) by 0.5; cap x 3.25-19.75, patch x 2.0-21.0, the
                           //  LINE IN hole (J7, x 26-34) 5 mm away; body x 4.0-19.0, y 68.8-82.8, J7's body x >= 24
psw_z = -13.0;             // its centre height: body z -20.5..-5.5 (lid top -23.0; the parting line -4.1)
psw_hole = 15.0;           // ASSUMED: the panel hole (the "15mm" in its name)
psw_cap = [16.5, 2.0];     // ASSUMED: the round cap's diameter, proud of the wall's outside
psw_body = [15.0, 14.0];   // ASSUMED: the body's diameter, its depth behind the panel
psw_tabs = [8.0, 4.0, 8.0];   // ASSUMED: the 2 solder tabs' envelope behind the body (x wide, deep, z tall)
psw_panel = 1.6;           // the clips grip 1.0-2.0 (ASSUMED): the 2.0 wall thinned to 1.6 from the inside
psw_thin_d = 19.0;         // ASSUMED: the thinned patch round the hole, where the clips sit

// ---------------------------------------------------------------------------------------------------------------
// the box (FDM PETG)
fit = 0.4;                 // board edge -> wall inner face
wall = 2.0;                // walls
corner_r = 3.0;            // outside corner radius
face_gap = 0.3;            // LCD front -> face underside
face_z0 = lcd_front + face_gap;   // 4.4: the face's underside
face_t = 2.4;              // the face (1.3 mm stays over the USB windows)
face_top = face_z0 + face_t;
floor_gap = 0.5;           // DIN bottom -> lid
lid_z1 = -T - din_depth - floor_gap;   // the lid's top surface (-23.0 at din_depth 20.9)
lid_t = 2.0;
lid_z0 = lid_z1 - lid_t;
board_gap = 0.2;           // the cell's L -> our board's underside
// the DIN lock on the lid (mots, 2026-11-19: "there should be a rail to lock the din so that the effort is not on the
// pcb"): a rail behind both DIN bodies takes the push of a plug going in; a tongue in the DIN window, in front of the
// DIN plates' lower edge, takes the pull of a plug coming out
din_rail_clr = 0.2;        // DIN bodies' back (y 15.185) -> the rail
din_rail_y1 = 19.4;        // the rail's back face: the L's top arm (y 19.4-22.4), so the two join
din_rail_end_clr = 1.0;    // the rail's right end -> the right wall's inner face
din_lock_h = 1.5;          // ASSUMED: the bottom part's wall covers the DIN plates' lowest 1.5 mm (below the plug
                           //  opening): it takes a plug's pull. MEASURE

// openings
plug_d = 8.0;              // ASSUMED: TRS plug body <= 7.2 + 0.4 a side
om_clr = 0.4;              // around the USB overmolds
din_clr = 0.4;             // around the DIN plates
din_top_lip = 0.6;         // the wall keeps 0.6 under the board over the DIN plates
min_web = 1.2;             // windows closer than this merge into one
tongue_clr = 0.2;          // the lid's tongues in the wall slots (a side)

// inserts (Ruthex-type brass heat-set; DATASHEET values, check against the inserts mots buys)
m2_ins = [3.2, 5.0];       // M2 x 4.0 insert (OD ~3.6): hole d 3.2, 5.0 deep (into the face 0.6)
boss_d = 6.0;              // board bosses (wall 1.4 round the hole)
m2_head = [4.0, 1.6];      // ISO 7045 M2 pan head under the board (screws M2 x 5)
// ---------------------------------------------------------------------------------------------------------------
// the PARTING LINE (mots, 2026-11-20): stepped per wall, at each hole's centre, so every hole is a half-hole in each
// part. [wall, u0, u1, z]: the wall's stretch u0..u1 (board x on the top / bottom walls, board y on the left / right
// walls; past the board's ends it covers the corners) parts at height z. The steps sit at free wall, never in a hole.
PZ_JACK = jack_axis;                         // -4.1: the TRS / CLK / RST / LINE IN jacks' axis
PZ_USB = usb_om_z[0] - om_clr + 0.5;         // -1.3: 0.5 INSIDE the USB-C window over its lower edge (-1.8): the bottom
                                             //  part has the wall under it (not the window's middle +1.85: see the report);
                                             //  a parting exactly on the edge left a zero-thickness skin in the top part
PZ_DIN = -T - din_depth / 2;                 // -12.05: the middle of the DIN plates
PZ_L2 = -lcd2_pcb_t - lcd2_usb[2] / 2;       // -3.23: the LCD-2's own USB-C axis
// the steps: J3's hole ends at x 23.1, the USB-C window starts at 25.55 (the step there: the top part keeps the 2.45 mm
// between them whole from -4.1 up, the bottom part the wall under the window: no post, no knife edge); the DIN window
// starts at 51.65 and the two windows are one opening from x 51.65 to 52.65 (the 0.4 mm strip between them is cut
// away), so the step to the DIN height sits inside that opening at x 52.15 (a step exactly on the DIN window's edge left a
// zero-thickness sheet in the top part); on the right wall the DIN stretch runs to y 24.5 (J13's hole from 27.0); on
// the left wall the LCD-2's USB-C stretch is y 33-49 (its opening y 34.6-47.4)
DIN_STEP = 52.15;
L2_STEP = [33.6, 48.4];      // the left wall's steps by the LCD-2's USB-C: where its lip stops (the opening -/+ 1.0), so
                             //  no short piece of lip is left between a step and the opening
PARTING = [["top", -9, 25.55, PZ_JACK], ["top", 25.55, DIN_STEP, PZ_USB], ["top", DIN_STEP, W + 9, PZ_DIN],
           ["right", -9, 24.5, PZ_DIN], ["right", 24.5, H + 9, PZ_JACK],
           ["bottom", -9, W + 9, PZ_JACK],
           ["left", -9, L2_STEP[0], PZ_JACK], ["left", L2_STEP[0], L2_STEP[1], PZ_L2], ["left", L2_STEP[1], H + 9, PZ_JACK]];
// the alignment lip: the wall's 2.0 split at o = lip_split (o: outward from the board's edge; the inner face at fit):
// the bottom part's lip (inner, fit .. lip_split - lip_clr / 2) rises lip_h above the parting line inside the top
// part's outer skirt (lip_split + lip_clr / 2 .. fit + wall); interrupted lip_gap round every hole
lip_split = fit + wall / 2;  // 1.4
lip_h = 3.0;
lip_clr = 0.2;             // lip -> skirt (0.1 each side of the split); lip_vclr over the lip's top
lip_vclr = 0.2;
lip_gap = 1.0;             // the lip stops this far from a hole
// SCREW TUBES (mots, 2026-11-20) at H1-H3: from the bottom part's floor up to our board's underside (contact), hollow
// from below (a bore through the floor for the screw and a screwdriver), a shoulder at the top: an M2 pan head (ISO 7045,
// dk 4.0, k 1.6) sits under it, the screw goes up through the shoulder and our board into the face boss's M2 x 4 insert.
// Screws M2 x 6: under the head 1.5 shoulder + 1.6 board + 2.9 into the 4.0 insert (the boss's hole is 5.0 deep).
// [x, y (the hole), the tube's blocks [x0, y0, x1, y1] (board mm)]: solid blocks filling the free space round each hole,
// from the nearest wall's inner face out, 0.3 short of every board part (a round tube would leave 0.8 of wall where a
// part comes close: the blocks keep >= 0.8 everywhere, more where there is room)
tube_bore = 4.4;           // the screw head (d 4.0) + 0.4; a PH1 screwdriver goes in after it
tube_shoulder = 1.5;       // under our board (>= 1.2)
tube_hole = 2.4;           // M2 clearance through the shoulder
cw_t = 1.6;                // the cell's own wall (below): 4 perimeters
TUBES = [[3.0, 17.6, [[-fit, 14.4, 5.77, 22.9],     // H1: J2's body ends at y 14.1, the upper strip starts at x 6.07 /
                      [-fit, 14.4, 7.0, 18.55]]],   //  y 18.855, J16 at x 6.82, the notch at y 23.195
         [3.0, 64.5, [[-fit, 59.1, 5.77, 67.6],     // H2: the notch ends at y 58.805, the lower strip x 6.07 / y 63.145,
                      [-fit, 63.45, 7.0, 67.6]]],   //  J15 x 6.82; the power switch's tabs x >= 7.5, y >= 64.8
         [40.5, 79.0, [[36.5, 75.0, 44.5, H + fit]]],    // H3: J7's body ends at x 36.0; joined to the bottom wall
         [W - 4.0, H / 2, [[W - 4.0 - tube_bore / 2 - cw_t, H / 2 - 4.4, W + fit, H / 2 + 4.4]]]];   // H4: from the cell's wall (its left side,
                               //  x 87.7) to the right wall; J13 / J14's bodies end at y 34.06 / start at 47.94 (MODEL)
// the cell's own wall (mots, 2026-11-21: "a double wall on the right of the battery"): from the lid up, along the
// cell's right side from the DIN rail (y 19.4) to the bottom wall; its right face is H4's bore (so it is that bore's
// left wall too), its top 1.0 under the CLOCK / RESET jacks' bodies (z -6.6, MODEL), which lie over it
CW1 = TUBES[3][0] - tube_bore / 2;     // 89.3
CW0 = CW1 - cw_t;                      // 87.7
cw_z1 = -6.6 - 1.0;                    // MODEL: J13 / J14 bodies' bottom, 1.0 clear (mots: the wall must not
                                       //  block the jacks; 0.3 was too tight for print / seating tolerance)
CELL_WALL = [CW0, din_rail_y1, lid_z1 - 0.01, CW1, H + fit, cw_z1];

// the D-pad cap (mots: 24 mm cross, 8 mm arms)
cap_L = 24.0;  cap_A = 8.0;
cap_clr = 0.6;             // face opening a side (generous PETG; a full diagonal press shifts the cross <= 0.48 at the
                           //  face's top, 0.34 along each axis)
// the retaining lips beyond the cross (mots: 2.5-3 mm, as wide as the parts round the D-pad allow, checked against
// the top parts with the cap tilted by a full press): [side, end] per arm, OpenSCAD directions +x (YES, SW5), -x (NO,
// SW1), +y (board up: UP, SW3), -y (board down: DOWN, SW4). Narrow where a part sits just past the keep-out: U21 and
// D10 / D11 (CLOCK / RESET) at the +x arm's end and sides, Q13 beyond the UP arm's end; a wide lip there would land
// on them when a press tilts the cap toward that side
cap_lips = [[1.2, 1.2], [2.75, 2.75], [2.75, 1.2], [2.75, 2.75]];
cap_play = 1.3;            // flange top -> face underside: a full diagonal press lifts the far flange corner 1.07
cap_proud = 2.0;           // above the face
cap_chamfer = 0.6;
cap_pivot_r = 1.75;        // the pivot nub: d 3.5 with a ball end, the only contact at rest (the board bare there)
cap_preload_gap = 0.15;    // each pad over its switch at rest
cap_pad = [3.0, 0.5];      // the pads under the arms over the actuators: diameter, height under the flange
sw_travel = 0.25;          // ASSUMED: TS-1187A-B-A-B travel (typical for 5.1 mm tact switches; datasheet not read)
// the symbols on the cap (mots, 2026-11-19): a second colour, multi-material, inlaid flush in the top face
sym_depth = 0.6;           // 3 layers of 0.2 (printed top down: the first 3 layers)
sym_r = 8.7;               // centre on the arm, between the pad (r 8) and the arm's end; the flat top ends at r 11.4
sym_tri = [5.0, 4.0];      // the UP / DOWN triangles: wide, tall (mots: solid, ~4.5-5 x 4, no stem)
sym_tri_r = 0.3;           // their corners rounded
sym_x = 4.6;               // the X's square
sym_bold = 1.7;            // the X's and the tick's strokes (mots: ~1.6-1.8, as heavy as the triangles)
sym_tick = [[-2.0, 0.1], [-0.6, -1.3], [1.7, 1.5]];   // the tick's points (its stroke's centre line)
sym_tick_c = [0.12, 0.26];  // its outline (4.96 x 4.6) centred

// the cell pushed against its own wall and the bottom wall (their faces less the rubber), the L's two arms as boxes
cell_x1 = CW0 - rubber_gap;  cell_y1 = H + fit - rubber_gap;
cell_x0 = cell_x1 - cell[0];     cell_y0 = cell_y1 - cell[1];
cell_top = lid_z1 + cell[2];
L_X0 = cell_x0 - rubber_gap - L_left_w;  L_Y0 = cell_y0 - rubber_gap - L_top_t;   // its outer corner
L_TOP = [L_X0, L_Y0, lid_z1 - 0.01, L_X0 + L_top_len, cell_y0 - rubber_gap, -T - board_gap];
L_LEFT = [L_X0, L_Y0, lid_z1 - 0.01, cell_x0 - rubber_gap, L_Y0 + L_left_len, -lcd2_pcb_t - lcd2_back_h - 0.3];

// the screen window
win_bezel = 0.5;           // a side beyond the active area
win_chamfer = 1.6;         // 45 deg, from the top
// the face labels, inlaid in a second colour (multi-material; the shell prints face down: the labels are its first 3
// layers). Font: Avenir Next Condensed Heavy, a macOS system font (/System/Library/Fonts/Avenir Next Condensed.ttc;
// OpenSCAD finds it through fontconfig; elsewhere it falls back to another font: check the picture). Measured with a
// morphological test: at 5.4 mm with -0.05 mm thinning every stroke is >= 0.9 mm and every gap >= 0.55 mm. mots
// (2026-11-19): "too big ! make the text half size", then "a bit smaller": 2.2 mm, thinning -0.02, so strokes about
// >= 0.37 and gaps >= 0.22 (scaled from the 5.4 mm test, not re-measured)
lab_h = 2.2;  lab_depth = 0.6;  lab_font = "Avenir Next Condensed:style=Heavy";
lab_spacing = 1.1;         // letter spacing: the gaps between letters >= 0.55
lab_thin = -0.02;          // thins the strokes a little (the 5.4 mm setting scaled with the size)

include <board_model.scad>   // BOARD_PARTS: every part of lcd2.kicad_pcb as a box (board_model.py writes it)

// ---------------------------------------------------------------------------------------------------------------
// helpers (board coordinates)
module bx(b) translate([b[0], -b[4], b[2]]) cube([b[3] - b[0], b[4] - b[1], b[5] - b[2]]);
ck_sweep = 0;              // (check only: every board box reaches this far down: the board coming in from below)
function grow(b, s) = [b[0] - s, b[1] - s, b[2] - s - ck_sweep, b[3] + s, b[4] + s, b[5] + s];
module at(p, z = 0) translate([p[0], -p[1], z]) children();
module rrect(x0, y0, x1, y1, r) translate([x0, -y1]) offset(r) offset(-r) square([x1 - x0, y1 - y0]);
// a 2D shape in (u along the wall, z) carried into a wall: o0 = outward distance from the board edge where it starts
function wall_m(w, o0) = w == "top"    ? [[1, 0, 0, 0], [0, 0, 1, o0], [0, 1, 0, 0]] :
                         w == "bottom" ? [[1, 0, 0, 0], [0, 0, -1, -H - o0], [0, 1, 0, 0]] :
                         w == "left"   ? [[0, 0, -1, -o0], [-1, 0, 0, 0], [0, 1, 0, 0]] :
                                         [[0, 0, 1, W + o0], [-1, 0, 0, 0], [0, 1, 0, 0]];
module in_wall(w, o0, d) multmatrix(wall_m(w, o0)) linear_extrude(d) children();
module through_wall(w) in_wall(w, fit - 0.2, wall + 1.2) children();

LCD_PCB = [LX, LY, -lcd2_pcb_t, LX + 48.21, LY + 35.01, 0];
LCD_BL = [LX + (48.21 - lcd_bl[0]) / 2, LY + (35.01 - lcd_bl[1]) / 2, 0, LX + (48.21 + lcd_bl[0]) / 2,
          LY + (35.01 + lcd_bl[1]) / 2, lcd_front];          // (z 0..2.0 under the glass: frame / FPC, as a block)
AA = [LCD_BL[3] - lcd_aa_far - lcd_aa[0], LY + (35.01 - lcd_aa[1]) / 2, LCD_BL[3] - lcd_aa_far, LY + (35.01 + lcd_aa[1]) / 2];
OUT = [-fit - wall, -fit - wall, W + fit + wall, H + fit + wall];   // outside, board coordinates

// ---------------------------------------------------------------------------------------------------------------
// the board model (boxes) for the pictures and the check; g grows every box (the check's clearance step)
module grp_pcb(g = 0) {
    translate([0, 0, -T - g - ck_sweep]) linear_extrude(T + 2 * g + ck_sweep) difference() {
        translate([-g, -H - g]) square([W + 2 * g, H + 2 * g]);
        translate([CUT[0] - 1, -CUT[3] + g]) square([CUT[2] + 1 - g, CUT[3] - CUT[1] - 2 * g]);
        for (p = MOUNT) at(p) circle(d = HOLE_D - 2 * g);
    }
    bx(grow(LCD_PCB, g));
}
module grp_top(g = 0) for (p = BOARD_PARTS) if (p[1] == "F" && p[2] != "SW-SMD") bx(grow([for (i = [3:8]) p[i]], g));
module grp_sw(g = 0) for (p = BOARD_PARTS) if (p[2] == "SW-SMD" && (ck_ref == "" || p[0] == ck_ref)) bx(grow([for (i = [3:8]) p[i]], g));
ck_ref = "";               // (check only: one switch of grp_sw)
module grp_bottom(g = 0) for (p = BOARD_PARTS) if (p[1] == "B") bx(grow([for (i = [3:8]) p[i]], g));
module grp_lcd2(g = 0) {
    bx(grow(LCD_BL, g));
    bx(grow([LX + 2.0, LY + 4.0, -lcd2_pcb_t - lcd2_back_h, LX + 46.0, LY + 31.0, -lcd2_pcb_t], g));   // back parts
    for (y = [P2Y, P1Y]) bx(grow([ROW_X - 17.78, y - 1.27, -lcd2_pcb_t - lcd2_hdr, ROW_X + 17.78, y + 1.27, -lcd2_pcb_t], g));
    bx(grow([-lcd2_usb_out, lcd2_usb_y - lcd2_usb[1] / 2, -lcd2_pcb_t - lcd2_usb[2], -lcd2_usb_out + lcd2_usb[0],
             lcd2_usb_y + lcd2_usb[1] / 2, -lcd2_pcb_t], g));
}
module grp_strips(g = 0) for (yc = strip_y) {
    bx(grow([ROW_X - strip_L / 2, yc - strip_W / 2, strip_z0, ROW_X + strip_L / 2, yc + strip_W / 2, strip_z0 + strip_t], g));
    for (s = [-1, 1]) {
        bx(grow([ROW_X - 17.78, yc + s * 1.905 - 1.25, strip_z0 + strip_t, ROW_X + 17.78, yc + s * 1.905 + 1.25,
                 -T - hdr_plastic], g));                                                   // the sockets
        bx(grow([ROW_X - 17.5, yc + s * 1.905 - 0.9, strip_z0 - strip_tail, ROW_X + 17.5, yc + s * 1.905 + 0.9,
                 strip_z0], g));                                                           // their tails
    }
}
ck_cell_lift = 0;          // (check only: lift the cell box off the floor it lies on, to measure the L and the walls)
module grp_cell(g = 0) bx(grow([cell_x0, cell_y0, lid_z1 + ck_cell_lift, cell_x0 + cell[0], cell_y0 + cell[1], lid_z1 + cell[2]], g));
module grp_screws(g = 0) for (i = [0 : 3]) let(p = MOUNT[i], z1 = head_z1(i))   // (H1-H3 under their tube's shoulder)
    at(p, z1 - m2_head[1] - g - ck_sweep) cylinder(d = m2_head[0] + 2 * g, h = m2_head[1] + 2 * g + ck_sweep);
module board(g = 0, pcb_alpha = 1) {
    color("darkgreen", pcb_alpha) grp_pcb(g);
    color("gold") grp_top(g);
    color("white") grp_sw(g);
    color("silver") grp_bottom(g);
    color("black") grp_lcd2(g);
    color("teal") grp_strips(g);
    color("royalblue") grp_cell(g);
    color("dimgray") grp_screws(g);
}

// ---------------------------------------------------------------------------------------------------------------
// the shell
module cross2d(L, A) union() { square([L, A], center = true); square([A, L], center = true); }

module shell_body() {
    difference() {
        translate([0, 0, lid_z1]) linear_extrude(face_top - lid_z1) outline2d();
        translate([0, 0, lid_z1 - 1]) linear_extrude(face_z0 - lid_z1 + 1) rrect(-fit, -fit, W + fit, H + fit, 0.01);
    }
    // board bosses: from the face down onto our board's top, a web to the nearest wall
    for (p = MOUNT) {
        at(p) cylinder(d = boss_d, h = face_z0 + 0.01);
        dw = [p[0] + fit, W + fit - p[0], p[1] + fit, H + fit - p[1]];   // to left, right, top, bottom walls
        m = min(dw);
        if (m - boss_d / 2 < 3.0) {
            v = m == dw[0] ? [-1, 0] : m == dw[1] ? [1, 0] : m == dw[2] ? [0, -1] : [0, 1];
            hull() {
                at(p) cylinder(d = boss_d, h = face_z0 + 0.01);
                at([p[0] + v[0] * m, p[1] + v[1] * m]) translate([-boss_d / 2 * abs(v[1]) - 0.01 * abs(v[0]),
                    -boss_d / 2 * abs(v[0]) - 0.01 * abs(v[1]), 0]) cube([boss_d * abs(v[1]) + 0.02 * abs(v[0]),
                    boss_d * abs(v[0]) + 0.02 * abs(v[1]), face_z0 + 0.01]);
            }
        }
    }
}
// the outside: the walls' rounded rectangle, nothing sticking out
module outline2d() rrect(OUT[0], OUT[1], OUT[2], OUT[3], corner_r);
// ---------------------------------------------------------------------------------------------------------------
// the PARTING LINE, the lip, the screw tubes (mots, 2026-11-20)
// a band of one wall, u0..u1 along it, z0..z1, o0..o1 outward from the board's edge (by default: from just inside the
// wall's inner face to past its outside, so it splits the wall and nothing inside the box)
module wall_band(w, u0, u1, z0, z1, o0 = fit - 0.3, o1 = fit + wall + 1)
    in_wall(w, o0, o1 - o0) translate([u0, z0]) square([u1 - u0, z1 - z0]);
// the walls' region BELOW the parting line: the bottom part's walls
// (each stretch overlaps its neighbours by band_seam: two bands meeting edge to edge at a step left a zero-thickness
// sheet in the top part, at x 25.55 from z -23 to -5.55, which mots found in the preview STL)
band_seam = 0.01;
module below_parting() for (s = PARTING) wall_band(s[0], s[1] - band_seam, s[2] + band_seam, lid_z0 - 1, s[3]);
// the parting height at u on wall w
function pz_at(w, u) = [for (s = PARTING) if (s[0] == w && u >= s[1] && u < s[2]) s[3]][0];
// a ring round the inside, o0..o1 outward from the board's edge (the lip / the skirt's clearance)
// (its outer corners rounded concentric with the box's outside, so the skirt keeps its thickness round the corners)
// (round_in: its inner corners rounded too, concentric, so a ring thinner than the wall keeps its thickness round them)
module ring2d(o0, o1, round_in = false) let(r1 = max(0.01, corner_r - (fit + wall - o1))) difference() {
    rrect(-o1, -o1, W + o1, H + o1, r1);
    rrect(-o0, -o0, W + o0, H + o0, round_in ? max(0.01, r1 - (o1 - o0)) : 0.01);
}
module ring_band(o0, o1, dz0, dz1, round_in = false) intersection() {
    translate([0, 0, lid_z0 - 1]) linear_extrude(face_top - lid_z0 + 2) ring2d(o0, o1, round_in);
    for (s = PARTING) wall_band(s[0], s[1] - band_seam, s[2] + band_seam, s[3] + dz0, s[3] + dz1);
}
// the holes of each wall, 2D in (u along the wall, z)
DIN_Z0 = lid_z1 + floor_gap + din_lock_h;    // -21.0: the DIN window's lower edge (the bottom part's wall below it
                                             //  covers the plates' lowest din_lock_h: the pull of a plug)
module wall_holes2d(w) {
    if (w == "top") {
        for (x = [J2X, J3X]) translate([x, jack_axis]) circle(d = plug_d);
        // the USB-C window, square-cornered (a rounded lower corner left slivers at the parting line), to the DIN window,
        // and the 0.4 mm strip between the two windows (x 51.65-52.65, z -2.2..-1.8) cut away
        translate([USB_U[0], usb_om_z[0] - om_clr]) square([DIN_U[0][0] + 1 - USB_U[0], usb_om_z[1] - usb_om_z[0] + 2 * om_clr]);
        translate([DIN_U[0][0], -T - din_top_lip - 0.01]) square([1, din_top_lip + usb_om_z[0] - om_clr + T + 0.02]);
        for (d = DIN_MERGE ? [[DIN_U[0][0], DIN_U[1][1]]] : DIN_U)
            translate([d[0], DIN_Z0]) square([d[1] - d[0], -T - din_top_lip - DIN_Z0]);
    }
    if (w == "right") for (y = [J13Y, J14Y]) translate([y, jack_axis]) circle(d = plug_d);
    if (w == "bottom") { translate([J7X, jack_axis]) circle(d = plug_d); translate([psw_x, psw_z]) circle(d = psw_hole); }
    if (w == "left") lcd2_usb_open();
}
module openings() {
    for (w = ["top", "right", "bottom", "left"]) through_wall(w) wall_holes2d(w);
    // the power switch's patch: the wall thinned from the inside to psw_panel round its hole, below the parting line
    // only (the bottom part's; the lip above it stays whole)
    // (its top flat at the parting line across its whole width: a round top left a thin wedge of wall under the lip)
    in_wall("bottom", fit - 0.01, wall - psw_panel + 0.01) intersection() {
        hull() {
            translate([psw_x, psw_z]) circle(d = psw_thin_d);
            translate([psw_x - psw_thin_d / 2 + 0.2, pz_at("bottom", psw_x) - 1]) square([psw_thin_d - 0.4, 1]);
        }
        translate([psw_x - 20, lid_z0 - 1]) square([40, pz_at("bottom", psw_x) - lid_z0 + 1]);
    }
}
// the lip (the bottom part's), stopped lip_gap short of every hole
module lip() difference() {
    ring_band(fit, lip_split - lip_clr / 2, -0.01, lip_h, true);
    // round every hole: its whole width + lip_gap a side, all the way up (a round cut left thin crescents of lip over the
    // jacks' holes), stretches of lip shorter than 2.0 between two cuts dropped too
    for (w = ["top", "right", "bottom", "left"]) in_wall(w, fit - 0.5, wall + 1.5)
        offset(delta = -1.0) offset(delta = 1.0) minkowski() {
            offset(delta = lip_gap) wall_holes2d(w);
            square([0.01, 100], center = true);
        }
    // past the DIN window's right end, to the corner: only a 0.35 mm strip of lip was left there
    in_wall("top", fit - 0.5, wall + 1.5) translate([DIN_U[1][1], DIN_Z0]) square([20, -T - DIN_Z0]);
}
// the screw tubes: the solid (the tube and its web to the wall), and what is taken out (the bore from under the box up
// to the shoulder, the screw's hole through the shoulder)
module tube_solid(t) for (b = t[2]) bx([b[0], b[1], lid_z1 - 0.01, b[2], b[3], -T]);
module tube_void(t) {
    at([t[0], t[1]], lid_z0 - 1) cylinder(d = tube_bore, h = -T - tube_shoulder - lid_z0 + 1);
    at([t[0], t[1]], lid_z0 - 1) cylinder(d = tube_hole, h = -T - lid_z0 + 2);
}
module tubes() difference() {
    for (t = TUBES) tube_solid(t);
    for (t = TUBES) tube_void(t);
}
// the M2 heads: H1-H4 under their tube's shoulder
function head_z1(i) = -T - tube_shoulder;

// wall openings: 2D shapes in (u along the wall, z)
L2Z = -lcd2_pcb_t - lcd2_usb[2] / 2;     // the LCD-2's USB-C axis
module lcd2_usb_open() translate([lcd2_usb_y - usb_om_w / 2 - om_clr, L2Z - 3.25 - om_clr])
    offset(1) offset(-1) square([usb_om_w + 2 * om_clr, 6.5 + 2 * om_clr]);
USB_U = [J1X - usb_om_w / 2 - om_clr, J4X + usb_om_w / 2 + om_clr];
DIN_U = [[J5X - din_w / 2 - din_clr, J5X + din_w / 2 + din_clr], [J6X - din_w / 2 - din_clr, J6X + din_w / 2 + din_clr]];
DIN_MERGE = DIN_U[1][0] - DIN_U[0][1] < min_web;
DIN_BACK = 15.185;            // BOARD: the DIN bodies' back (y)
DIN_RAIL = [DIN_U[0][0] + din_clr, DIN_BACK + din_rail_clr, lid_z1 - 0.01,
            W + fit - din_rail_end_clr, din_rail_y1, -T - board_gap];

module face_cuts() {
    // the screen window: the active area + a bezel, chamfered 45 deg from the top
    // (straight up to the chamfer, the chamfer a hull of two thin slabs: one hull from the face's underside swept the
    //  chamfer down to it and left a 0.13 mm skin of face round the window)
    translate([0, 0, face_z0 - 1]) linear_extrude(face_t - win_chamfer + 1.01)
        rrect(AA[0] - win_bezel, AA[1] - win_bezel, AA[2] + win_bezel, AA[3] + win_bezel, 0.5);
    hull() {
        translate([0, 0, face_top - win_chamfer]) linear_extrude(0.01)
            rrect(AA[0] - win_bezel, AA[1] - win_bezel, AA[2] + win_bezel, AA[3] + win_bezel, 0.5);
        translate([0, 0, face_top + 0.01]) linear_extrude(0.01)
            rrect(AA[0] - win_bezel - win_chamfer, AA[1] - win_bezel - win_chamfer, AA[2] + win_bezel + win_chamfer,
                  AA[3] + win_bezel + win_chamfer, 0.5 + win_chamfer);
    }
    // the D-pad cap's opening
    at([DPX, DPY], face_z0 - 1) linear_extrude(face_t + 2) offset(delta = cap_clr) cross2d(cap_L, cap_A);
    // labels (the board's silk is hidden): engraved into the face
    translate([0, 0, face_top - lab_depth]) linear_extrude(lab_depth + 1) labels2d();
    // the M2 inserts (pressed in from below, into the bosses)
    for (p = MOUNT) at(p, -0.01) cylinder(d = m2_ins[0], h = m2_ins[1]);
}

// the labels (mots, 2026-11-19: a second colour, inlaid like the cap's symbols): [x, y (board), text]. Half size
// (mots: "make the text half size"), with a small triangle like the cap's pointing at each socket (mots: "use our
// triangle to point at the socket"). Top edge, from the wall in: the triangles, then the sockets' names with MIDI
// between its 1 and 2 (mots: "move midi between 1 and 2, remove 'to'"). Right side: CLK / RST with the triangle
// between it and the wall; bottom: LINE IN with the triangle below it
LAB_ROWS = [2.4, 5.2];          // the top edge's rows (centres, board y): triangles, names
lab_tri = [2.6, 2.1];           // the socket triangles (the cap's 5.0 x 4.0 shape, about half)
lab_tri_r = 0.2;                // their corners rounded
lab_left_x = 0.4;               // the left edge's band (face edge x -2.4, the window's chamfer from x 3.3)
LABELS = [[J2X, LAB_ROWS[1], "1"], [(J2X + J3X) / 2, LAB_ROWS[1], "MIDI"], [J3X, LAB_ROWS[1], "2"],
          [J1X, LAB_ROWS[1], "ESP"], [J4X, LAB_ROWS[1], "DEVICE"],
          [J5X, LAB_ROWS[1], "1"], [(J5X + J6X) / 2, LAB_ROWS[1], "MIDI"], [J6X, LAB_ROWS[1], "2"],
          [J7X, H - 5.0, "LINE IN"], [W - 7.0, J13Y, "CLK"], [W - 7.0, J14Y, "RST"],
          // the LCD-2's own USB-C in the left wall (mots: "we miss the esp socket on the left"): only ~5.7 mm between
          // the face's edge and the screen window's chamfer, so the word runs up the edge, below the triangle
          [lab_left_x, lcd2_usb_y + 5.6, "ESP", 90]];
// [x, y (board), direction: 0 = up (the top wall), -90 = right, 180 = down]
LAB_ARROWS = [for (x = [J2X, J3X, J1X, J4X, J5X, J6X]) [x, LAB_ROWS[0], 0],
              [W - 2.2, J13Y, -90], [W - 2.2, J14Y, -90], [J7X, H - 1.8, 180], [lab_left_x, lcd2_usb_y, 90]];
module lab_arrow() offset(r = lab_tri_r) offset(delta = -lab_tri_r)
    polygon([[-lab_tri[0] / 2, -lab_tri[1] / 2], [lab_tri[0] / 2, -lab_tri[1] / 2], [0, lab_tri[1] / 2]]);
module labels2d() {
    for (l = LABELS) at([l[0], l[1]]) rotate(len(l) > 3 ? l[3] : 0) offset(delta = lab_thin)
        text(l[2], size = lab_h, font = lab_font, spacing = lab_spacing, halign = "center", valign = "center");
    for (a = LAB_ARROWS) at([a[0], a[1]]) rotate(a[2]) lab_arrow();
}

// the whole shell (the old one-piece top shell with every hole closed round), then split along the parting line
module shell_full() difference() { shell_body(); openings(); face_cuts(); }
// the labels as their own body, filling the face's cuts exactly (the second colour: the same origin as the top's STL)
module face_labels() translate([0, 0, face_top - lab_depth]) linear_extrude(lab_depth) labels2d();

// ---------------------------------------------------------------------------------------------------------------
// the TOP part: the face, the bosses, the walls above the parting line; the inner half of the wall cleared over the
// parting line for the bottom part's lip (the outer half stays: the skirt)
module top_part() difference() {
    shell_full();
    below_parting();
    ring_band(fit - 0.3, lip_split + lip_clr / 2, -0.01, lip_h + lip_vclr);
}

// ---------------------------------------------------------------------------------------------------------------
// the BOTTOM part: the floor, the walls up to the parting line, the lip, the screw tubes, the strip rails, the cell's
// L, the DIN rail (the wall below the DIN window takes a plug's pull)
module bottom_part(extras = true) {   // (extras = false: without the screw tubes, coloured apart in pictures)
    difference() {
        union() {
            translate([0, 0, lid_z0]) linear_extrude(lid_t) outline2d();
            intersection() { shell_full(); below_parting(); }
            lip();
        }
        for (t = TUBES) tube_void(t);
    }
    if (extras) tubes();
    // rails that hold the two bridge strips up (under their long edges)
    for (yc = strip_y, s = [-1, 1])
        bx([ROW_X - strip_L / 2 + 1.0, yc + (s < 0 ? -strip_rail_off[1] : strip_rail_off[0]), lid_z1 - 0.01,
            ROW_X + strip_L / 2 - 1.0, yc + (s < 0 ? -strip_rail_off[0] : strip_rail_off[1]), strip_z0 - strip_post_gap]);
    // the L on the cell's top-left corner: keeps the cell (with the rubber) in the corner, backs our board
    for (b = [L_TOP, L_LEFT]) bx(b);
    // the cell's own wall on its right (H4's tube between it and the right wall)
    bx(CELL_WALL);
    // the DIN lock's rail behind both DIN bodies (up to under our board, like the L)
    bx(DIN_RAIL);
}

// ---------------------------------------------------------------------------------------------------------------
// the D-pad cap (mots, 2026-11-19: a centre pivot): it stands on a ball-ended nub at the D-pad centre on our board's
// bare top; a pad under each arm floats cap_preload_gap over its switch; pressing an arm tilts the cap about the nub so
// only that pad lands on its switch. The flange (under the face) keeps it in; the cross through the face keeps it
// from turning. Printed TOP DOWN (the chamfered top on the bed; the lips a 1.2-2.75 mm ledge, the nub and pads up).
CAP_FB = SW_H + cap_preload_gap + cap_pad[1];        // the flange's underside (z, at rest)
// the flange: each arm with its own side and end lips (cap_lips), the arms' rectangles from the centre out
module flange2d() for (i = [0 : 3]) rotate([0, 180, 90, 270][i])
    translate([0, -cap_A / 2 - cap_lips[i][0]]) square([cap_L / 2 + cap_lips[i][1], cap_A + 2 * cap_lips[i][0]]);
CAP_FT = face_z0 - cap_play;                         // the flange's top
// cap_tilt = [press direction x, y (OpenSCAD axes), degrees]: the cap turned about the nub's contact point
CAP_TOP = face_top + cap_proud;
module cap(tilt = [1, 0, 0]) difference() {
    cap_body(tilt);
    at([DPX, DPY]) rotate(a = tilt[2], v = [-tilt[1], tilt[0], 0])
        translate([0, 0, CAP_TOP - sym_depth]) linear_extrude(sym_depth + 0.01) cap_symbols2d();
}
// the symbols as their own body, filling the cuts exactly (the second colour: the same origin as the cap's STL)
module cap_symbols(tilt = [1, 0, 0]) at([DPX, DPY]) rotate(a = tilt[2], v = [-tilt[1], tilt[0], 0])
    translate([0, 0, CAP_TOP - sym_depth]) linear_extrude(sym_depth) cap_symbols2d();
// the symbols (mots, 2026-11-19), cap coordinates (OpenSCAD axes: +x YES, -x NO, +y UP, -y DOWN; parts.py SW5 YES,
// SW1 NO, SW3 UP, SW4 DOWN, as silk.py's labels), each centred sym_r out on its arm, upright as the face is read
module cap_symbols2d() {
    translate([0, sym_r]) sym_arrow();
    translate([0, -sym_r]) mirror([0, 1]) sym_arrow();
    translate([-sym_r, 0]) sym_cross();
    translate([sym_r, 0]) sym_check();
}
module sym_arrow() {           // pointing +y: a solid triangle (mots: as jank's page D-pad), no stem, its corners
    offset(r = sym_tri_r) offset(delta = -sym_tri_r)   //  rounded so the tip prints (>= 2 nozzle widths across)
        polygon([[-sym_tri[0] / 2, -sym_tri[1] / 2], [sym_tri[0] / 2, -sym_tri[1] / 2], [0, sym_tri[1] / 2]]);
}
// the X and the tick in the triangles' family (mots): bold sym_bold strokes, flat / mitred ends, no round caps
module sym_cross() intersection() {   // an X: two bars cut flush by its sym_x square
    for (a = [45, -45]) rotate(a) square([sym_x * 2, sym_bold], center = true);
    square(sym_x, center = true);
}
// a tick: a sym_bold stroke along p0 -> p1 -> p2, its ends cut square, a mitred corner at p1
module sym_check() translate(sym_tick_c) {
    p = sym_tick;  h = sym_bold / 2;
    d1 = (p[1] - p[0]) / norm(p[1] - p[0]);  d2 = (p[2] - p[1]) / norm(p[2] - p[1]);
    n1 = [-d1[1], d1[0]] * h;  n2 = [-d2[1], d2[0]] * h;
    m = (n1 + n2) / (1 + d1 * d2);            // the mitre's offset at p1
    polygon([p[0] + n1, p[1] + m, p[2] + n2, p[2] - n2, p[1] - m, p[0] - n1]);
}
module cap_body(tilt) {
    ztop = CAP_TOP;
    at([DPX, DPY]) rotate(a = tilt[2], v = [-tilt[1], tilt[0], 0]) {
        translate([0, 0, CAP_FB]) linear_extrude(CAP_FT - CAP_FB) flange2d();
        for (d = [[cap_L, cap_A], [cap_A, cap_L]]) hull() {
            translate([-d[0] / 2, -d[1] / 2, CAP_FT - 0.01]) cube([d[0], d[1], ztop - cap_chamfer - CAP_FT]);
            translate([-d[0] / 2 + cap_chamfer, -d[1] / 2 + cap_chamfer, ztop - 0.01])
                cube([d[0] - 2 * cap_chamfer, d[1] - 2 * cap_chamfer, 0.01]);
        }
        // the pads over the four switches, and the pivot nub (its ball end touches the board at z 0)
        for (a = [0, 90, 180, 270]) rotate(a) translate([SW_OFF, 0, SW_H + cap_preload_gap])
            cylinder(d = cap_pad[0], h = cap_pad[1] + 0.01);
        translate([0, 0, cap_pivot_r]) { sphere(r = cap_pivot_r); cylinder(r = cap_pivot_r, h = CAP_FB - cap_pivot_r + 0.01); }
    }
}
// the tilt of one full press on an arm (its pad down gap + travel at r SW_OFF) and on the diagonal between two arms
TILT_ARM = atan((cap_preload_gap + sw_travel) / SW_OFF);
TILT_DIAG = atan((cap_preload_gap + sw_travel) / (SW_OFF / sqrt(2)));

// ---------------------------------------------------------------------------------------------------------------
// the power switch as boxes / cylinders: its cap outside the wall, its body and tabs inside (the part in the hole
// left out); g grows it (the check's shell test shrinks it 0.01 off the faces it sits on)
PSW_IN = H + fit + wall - psw_panel;      // the thinned panel's inner face (board y; the bottom wall)
PSW_OUT = H + fit + wall;                 // the wall's outer face
PSW_TAB = PSW_IN - psw_body[1] - psw_tabs[1];   // the tabs' far end (board y)
module kcd1(g = 0) {
    // (rotate([-90, 0, 0]): the cylinder runs toward OpenSCAD +Y, i.e. board y DOWN from where at() puts it)
    at([psw_x, PSW_IN + g], psw_z) rotate([-90, 0, 0]) cylinder(d = psw_body[0] + 2 * g, h = psw_body[1] + 2 * g);
    bx(grow([psw_x - psw_tabs[0] / 2, PSW_TAB, psw_z - psw_tabs[2] / 2,
             psw_x + psw_tabs[0] / 2, PSW_IN - psw_body[1], psw_z + psw_tabs[2] / 2], g));
    at([psw_x, PSW_OUT + psw_cap[1] + g], psw_z) rotate([-90, 0, 0]) cylinder(d = psw_cap[0] + 2 * g, h = psw_cap[1] + 2 * g);
}

// ---------------------------------------------------------------------------------------------------------------
// the check: intersection of one case part with one board group grown by ck_grow (empty = clearance >= ck_grow)
ck_part = "top_part";  ck_group = "top";  ck_grow = 0.0;  ck_psw = 0.0;  ck_tilt = [1, 0, 0];
ck_dz = 0.0;               // (check only: the bottom part moved up by this, against the top part)
module ck_case() {
    if (ck_part == "top_part") top_part();
    if (ck_part == "bottom_part") translate([0, 0, ck_dz]) bottom_part();
    if (ck_part == "tubes") tubes();
    if (ck_part == "cap") cap(ck_tilt);   // (ck_tilt: a press, [direction x, y, degrees])
    if (ck_part == "switch") kcd1(ck_psw);    // (the power switch against the board groups / the parts)
    if (ck_part == "cell") grp_cell();        // (the cell against the board's parts underneath)
}
module ck_board() {
    if (ck_group == "pcb") grp_pcb(ck_grow);
    if (ck_group == "top") grp_top(ck_grow);
    if (ck_group == "sw") grp_sw(ck_grow);
    if (ck_group == "bottom") grp_bottom(ck_grow);
    if (ck_group == "lcd2") grp_lcd2(ck_grow);
    if (ck_group == "strips") grp_strips(ck_grow);
    if (ck_group == "cell") grp_cell(ck_grow);
    if (ck_group == "screws") grp_screws(ck_grow);
    if (ck_group == "top_part") top_part();          // case part against case part
    if (ck_group == "bottom_part") bottom_part();
}

// ---------------------------------------------------------------------------------------------------------------
module assembly(e = 0) {
    color("lightgray") translate([0, 0, e]) top_part();
    color("#111111") translate([0, 0, e]) face_labels();
    color("orange") translate([0, 0, 1.5 * e]) cap();
    color("#111111") translate([0, 0, 1.5 * e]) cap_symbols();
    board();
    color("slategray") translate([0, 0, -0.8 * e]) bottom_part();
    color("#222222") translate([0, 0, -0.8 * e]) kcd1();
}
module cutaway(axis, at_) {        // keep x <= at_ ("x") or y_board <= at_ ("y")
    intersection() {
        children();
        if (axis == "x") translate([at_ - 1000, -1000, -1000]) cube([1000, 2000, 2000]);
        if (axis == "y") translate([-1000, -at_, -1000]) cube([2000, 1000, 2000]);
    }
}
module section(axis, at_) {
    color("lightgray") cutaway(axis, at_) top_part();
    color("orange") cutaway(axis, at_) cap();
    color("slategray") cutaway(axis, at_) bottom_part(false);
    color("crimson") cutaway(axis, at_) tubes();
    color("darkgreen") cutaway(axis, at_) grp_pcb();
    color("gold") cutaway(axis, at_) grp_top();
    color("white") cutaway(axis, at_) grp_sw();
    color("silver") cutaway(axis, at_) grp_bottom();
    color("black") cutaway(axis, at_) grp_lcd2();
    color("teal") cutaway(axis, at_) grp_strips();
    color("royalblue") cutaway(axis, at_) grp_cell();
    color("dimgray") cutaway(axis, at_) grp_screws();
    color("#222222") cutaway(axis, at_) kcd1();
}
// the outside: both parts in their colours (the parting line between), the labels, the switch
module outside() {
    color("lightgray") top_part();
    color("#111111") face_labels();
    color("slategray") bottom_part(false);
    color("crimson") tubes();
    color("#222222") kcd1();
    color("orange") cap();
    board();
}

if (part == "assembly") assembly(0);
if (part == "exploded") assembly(40);
if (part == "top_part") top_part();
if (part == "bottom_part") bottom_part();
if (part == "cap") cap();
if (part == "cap_symbols") cap_symbols();
if (part == "board") board();
// mock-ups to print or drop into a slicer next to the box (mots, 2026-11-20: "pcb: with all the connectors
// basically"), same origin as the box's STLs: the PCB with the LCD-2, the connectors (J*: USB-C, TRS and PJ-320A jacks,
// DINs, headers), the strips and the D-pad switches (for the cap), as their bounding boxes; and the cell
function is_conn(r) = (r[0] == "J" && r[1] != "P") || (r[0] == "S" && r[1] == "W");
if (part == "board_mock") {
    grp_pcb(); grp_lcd2(); grp_strips();
    for (p = BOARD_PARTS) if (is_conn(p[0])) bx([for (i = [3:8]) p[i]]);
}
if (part == "cell_mock") grp_cell();
if (part == "outside") outside();
if (part == "inside") { color("slategray") bottom_part(); color("silver") grp_bottom(); color("teal") grp_strips();
                         color("royalblue") grp_cell(); color("dimgray") grp_screws(); color("#222222") kcd1(); }
if (part == "tube_section") section("x", TUBES[0][0]);      // x 3.0: H1 and H2, their tubes
if (part == "tube_section_h3") section("y", TUBES[2][1]);   // y 79: H3's tube
if (part == "tube_section_h4") section("y", TUBES[3][1]);   // y 41: H4's tube, the cell's wall, the cell
if (part == "shell_labels") face_labels();
if (part == "section_din") section("x", DPX);
if (part == "section_screen") section("y", DPY);
if (part == "check") intersection() { ck_case(); ck_board(); }

// the numbers BOX2.md quotes (openscad -o x.echo box2.scad)
echo(outside_x = W + 2 * (fit + wall), outside_y = H + 2 * (fit + wall), height = face_top - lid_z0,
     parting = [for (s = PARTING) [s[0], s[1], s[2], s[3]]],
     tubes = TUBES, din_rail = DIN_RAIL, din_z0 = DIN_Z0, cell_wall = CELL_WALL,
     cell_box = [cell_x0, cell_y0, cell_x1, cell_y1], L_top = L_TOP, L_left = L_LEFT);
