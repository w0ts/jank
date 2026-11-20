// SPDX-License-Identifier: GPL-3.0-only
// jank-mini-3 box, STAGE 1 (Claude, 2026-11-19, for mots's approval): a TOP SHELL (the face and all four walls, full
// depth) and a flat BOTTOM LID screwed into it, plus one printed cross D-pad cap. FDM, PETG, 0.4 mm nozzle, 0.2 mm layers.
//   shell: printed FACE DOWN (the face on the bed; walls and bosses grow up; the USB-C window's lower edge prints as a
//          25.8 mm bridge; the side screws' countersinks and the hook window are small holes in the walls)
//   lid:   printed OUTSIDE DOWN (the strip rails, the cell's L, the screw tabs, the hook and the tongues grow up)
//   cap:   printed TOP DOWN (its chamfered top on the bed; the flange a 2.75 mm ledge; the pivot nub and pads up)
// The board screws to the shell from below (4 x M2 into heat-set inserts in bosses on the face); the lid is held by
// 3 x M3 countersunk screws sideways through the walls into heat-set inserts in tabs on the lid, and a hook on the right
// (mots: no corner towers; the board drops in from below and fills the whole inside, so nothing of the shell may sit
// under it).
//
// Coordinates: BOARD mm as make_board.py (x right, y DOWN from the board's top-left corner); z = 0 is OUR BOARD'S TOP
// (its bottom is z -1.6). In OpenSCAD space Y = -y (so the top wall is at +Y, the pictures read like the board).
// Every dimension that is not read from the board files says where it comes from: BOARD (make_board.py /
// lcd2.kicad_pcb), MODEL (../lib/3d model bboxes), DRAWING (Waveshare ESP32-S3-LCD-2-20250122.pdf), README (this
// board's README), DATASHEET, or ASSUMED (mots measures it; see BOX.md).
//
// openscad -D 'part="shell"' -o shell.stl box.scad      part = assembly | exploded | shell | lid | cap | board |
//                                                              section_din | section_screen | check

part = "assembly";
$fn = 48;

// ---------------------------------------------------------------------------------------------------------------
// the board (BOARD: make_board.py)
W = 95.5;  H = 82.0;  T = 1.6;                    // BOARD: 95.5 x 82, 1.6 mm (README: order 1.6)
LX = 0.5;  LY = (H - 35.01) / 2;                  // BOARD: the LCD-2's PCB corner in the notch
CUT = [0, LY - 0.3, LX + 48.21 + 0.3, LY + 35.01 + 0.3];   // BOARD: the notch, open to the left edge
MOUNT = [[3.0, 17.6], [3.0, H - 17.5], [40.5, H - 3.0], [W - 4.0, H / 2]];   // BOARD: H1..H4, M2
HOLE_D = 2.2;                                     // BOARD: HOLE_M2_2.2
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
// right and bottom walls and an L on its top-left corner, a rubber / foam gap on every held side; its lead end -y
cell = [34.5, 56.0, 10.6];     // DATASHEET: EEMB LP103454 (x, y, thickness)
rubber_gap = 2.0;              // mots: cell -> walls and the L, filled with rubber / foam (the cell: x 59.4-93.9,
                               //  y 24.4-80.4 at the defaults; the DINs end at y 15.19, MODEL)
// the L: from the lid up to board_gap under our board (it backs the board too)
L_top_t = 3.0;                 // the top arm's thickness (along the cell's top side)
L_left_w = 4.4;                // the left arm's width: from the rubber gap leftward to x 53.0, under SW1 (pads x 53.75-59.75)
L_top_len = 20.0;              // the top arm, from the L's outer corner (x 53.0 .. 73.0)
L_left_len = 36.0;             // the left arm, from the L's outer corner (y 19.4 .. 55.4: past SW4's y 49 + 6.4)

// the power switch (mots, 2026-11-19): YIZHI KCD1-106S-B (LCSC C52125269), a round snap-in rocker in the LEFT wall
// below the LCD-2's notch, in the cell's red lead (cell -> switch -> the LCD-2's MX1.25): no board connection.
// Its datasheet drawing was not read: every size ASSUMED (mots measured the cap ~16 across)
psw_y = 72.9;              // its centre, board y: the cap (y 64.65-81.15) ends 0.25 before the corner's rounding (flat
                           //  wall face to y 81.4), the thinned patch (to y 82.4) fits; body y 65.4-80.4
psw_z = -13.0;             // its centre height: body z -20.5..-5.5 (lid top -23.0, H2's screw head down to -3.2)
psw_hole = 15.0;           // ASSUMED: the panel hole (the "15mm" in its name)
psw_cap = [16.5, 2.0];     // ASSUMED: the round cap's diameter, proud of the wall's outside
psw_body = [15.0, 14.0];   // ASSUMED: the body's diameter, its depth behind the panel
psw_tabs = [8.0, 4.0, 8.0];   // ASSUMED: the 2 solder tabs' envelope behind the body (y wide, deep, z tall)
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
din_rail_hook_clr = 1.5;   // the rail's right end -> the hook's tab (the tab flexes inward 1.0 + 0.3 as it snaps)
din_lock_h = 1.5;          // ASSUMED: the tongue covers the DIN plates' lowest 1.5 mm (below the plug opening). MEASURE

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
// the lid's fixing (mots, 2026-11-19: no corner towers, the outline a plain rectangle): tabs rise from the lid just
// inside the walls, M3 countersunk screws go sideways through the walls into heat-set inserts in the tabs, heads flush.
// The board drops in from below and fills the whole inside, so the tabs (on the lid, fitted after it) stay LOW: under
// every bottom part they pass (the jacks hang to z -7.1, the strips' tails to -15.7 elsewhere)
m3_ins = [4.0, 5.0];       // M3 x 4.0 insert (OD ~4.6): hole d 4.0, 5.0 deep, pressed in from the tab's wall side
m3_cs = [3.4, 6.4, 1.5];   // ISO 10642 M3 in the 2.0 wall: hole, seat d at the outside, cone depth (0.5 of straight
                           //  wall stays); screws M3 x 6 countersunk (2.0 wall + 0.2 gap, ~3.8 into the insert)
tab = [8.0, 6.0, 8.0];     // a screw tab: along the wall, inward (insert 5.0 + 1.0), height over the lid's top
tab_clr = 0.2;             // tab -> the wall's inner face (the screw pulls it tight)
tab_screw_h = 4.0;         // the screw's axis over the lid's top (2.0 of tab above the insert hole, the head 0.8
                           //  above the wall's bottom edge)
// [wall, position along it (board mm)]: free low wall, clear of slots, the DINs, the switch, the cell's rubber, the L,
// the strip rails: top x 23.1-51.65 (between J3's slot and the DIN window), bottom x 34-57.4 (J7's slot to the cell's
// rubber), left y 0-34.6 (above the LCD-2's USB-C slot; J2 hangs only to z -7.1 over it)
// (the top screw at x 37 is gone: the wall piece it went through, under the USB-C window, broke on the first print,
// hanging from a 2.45 mm strip and a 0.3 mm sliver; mots, 2026-11-20: "it should be attached to the bottom instead",
// so that piece is now a panel on the lid, and a screw from it into the lid would hold nothing)
LID_SCREWS = [["bottom", 46.0], ["left", 10.0]];
// the right wall has no room for a screw (the DIN J6 reaches the floor to y 15.19, the cell's rubber starts at y 22.4,
// J13 / J14 slots): a HOOK there instead, a thin tab with a barb that catches in a small window in the right wall
// (fit the lid right side first, tilted, then swing it up and screw the other three)
hook_y = (15.19 + 22.4) / 2;   // 18.8: centred in the free 7.2 mm
hook = [5.0, 1.6, 9.0];    // along the wall, thickness, height over the lid
hook_barb = [1.0, 1.2, 1.0];   // into the wall, flat catching face height, the 45 deg lead-in above it
hook_clr = 0.3;            // the window round the barb

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

// the cell pushed into the corner (the walls' inner faces less the rubber), the L's two arms as boxes
cell_x1 = W + fit - rubber_gap;  cell_y1 = H + fit - rubber_gap;
cell_x0 = cell_x1 - cell[0];     cell_y0 = cell_y1 - cell[1];
cell_top = lid_z1 + cell[2];
L_X0 = cell_x0 - rubber_gap - L_left_w;  L_Y0 = cell_y0 - rubber_gap - L_top_t;   // its outer corner
L_TOP = [L_X0, L_Y0, lid_z1 - 0.01, L_X0 + L_top_len, cell_y0 - rubber_gap, -T - board_gap];
L_LEFT = [L_X0, L_Y0, lid_z1 - 0.01, cell_x0 - rubber_gap, L_Y0 + L_left_len, -T - board_gap];

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
module grp_screws(g = 0) for (p = MOUNT) bx(grow([p[0] - m2_head[0] / 2, p[1] - m2_head[0] / 2, -T - m2_head[1],
                                                  p[0] + m2_head[0] / 2, p[1] + m2_head[0] / 2, -T], g));
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
// the lid's side screws through the walls (countersunk from outside, flush) and the hook's window in the right wall
HOOK_VB = lid_z1 + hook[2] - 0.8 - hook_barb[2] - hook_barb[1];   // the barb's flat catching face (z)
module lid_screw_holes() {
    for (s = LID_SCREWS) {
        multmatrix(wall_m(s[0], fit - 0.2)) translate([s[1], lid_z1 + tab_screw_h, 0]) cylinder(d = m3_cs[0], h = wall + 1.2);
        multmatrix(wall_m(s[0], fit + wall - m3_cs[2])) translate([s[1], lid_z1 + tab_screw_h, 0])
            cylinder(d1 = m3_cs[0], d2 = m3_cs[1], h = m3_cs[2] + 0.01);
    }
    through_wall("right") translate([hook_y - hook[0] / 2 - hook_clr, HOOK_VB - hook_clr])
        square([hook[0] + 2 * hook_clr, hook_barb[1] + hook_barb[2] + 2 * hook_clr]);
}
// the lid's screw tabs (an M3 insert pressed in from the wall side) and the hook with its barb
module lid_tabs() {
    for (s = LID_SCREWS) multmatrix(wall_m(s[0], fit - tab_clr - tab[1])) difference() {
        translate([s[1] - tab[0] / 2, lid_z1 - 0.01, 0]) cube([tab[0], tab[2] + 0.01, tab[1]]);
        translate([s[1], lid_z1 + tab_screw_h, tab[1] - m3_ins[1]]) cylinder(d = m3_ins[0], h = m3_ins[1] + 0.01);
    }
    multmatrix(wall_m("right", fit - tab_clr - hook[1]))
        translate([hook_y - hook[0] / 2, lid_z1 - 0.01, 0]) cube([hook[0], hook[2] + 0.01, hook[1]]);
    multmatrix(wall_m("right", fit - tab_clr)) hull() {
        translate([hook_y - hook[0] / 2, HOOK_VB, -0.01]) cube([hook[0], hook_barb[1], hook_barb[0] + tab_clr + 0.01]);
        translate([hook_y - hook[0] / 2, HOOK_VB + hook_barb[1] + hook_barb[2], -0.01]) cube([hook[0], 0.01, 0.01]);
    }
}
// the rectangles the tabs and the hook take on the floor (board coordinates; floor_map.py draws them)
function wall_rect(w, u, a, d0, d1) = w == "top" ? [u - a / 2, -d1, u + a / 2, -d0] :
                                     w == "bottom" ? [u - a / 2, H + d0, u + a / 2, H + d1] :
                                     w == "left" ? [-d1, u - a / 2, -d0, u + a / 2] : [W + d0, u - a / 2, W + d1, u + a / 2];
TAB_RECTS = concat([for (s = LID_SCREWS) wall_rect(s[0], s[1], tab[0], fit - tab_clr - tab[1], fit - tab_clr)],
                   [wall_rect("right", hook_y, hook[0], fit - tab_clr - hook[1], fit - tab_clr)]);

// wall openings: 2D shapes in (u along the wall, z)
module jack_slot(u, d) { translate([u, jack_axis]) circle(d = d); translate([u - d / 2, lid_z1 - 2]) square([d, jack_axis - lid_z1 + 2]); }
module jack_tongue(u) difference() {
    translate([u - plug_d / 2 + tongue_clr, lid_z1]) square([plug_d - 2 * tongue_clr, jack_axis - lid_z1]);
    translate([u, jack_axis]) circle(d = plug_d + 0.01);
}
L2Z = -lcd2_pcb_t - lcd2_usb[2] / 2;     // the LCD-2's USB-C axis
module lcd2_usb_open() translate([lcd2_usb_y - usb_om_w / 2 - om_clr, L2Z - 3.25 - om_clr])
    offset(1) offset(-1) square([usb_om_w + 2 * om_clr, 6.5 + 2 * om_clr]);
module lcd2_usb_slot() { lcd2_usb_open(); translate([lcd2_usb_y - usb_om_w / 2 - om_clr, lid_z1 - 2])
    square([usb_om_w + 2 * om_clr, L2Z - 3.25 - om_clr - lid_z1 + 2 + 1]); }
module lcd2_usb_tongue() translate([lcd2_usb_y - usb_om_w / 2 - om_clr + tongue_clr, lid_z1])
    square([usb_om_w + 2 * om_clr - 2 * tongue_clr, L2Z - 3.25 - om_clr - lid_z1]);
USB_U = [J1X - usb_om_w / 2 - om_clr, J4X + usb_om_w / 2 + om_clr];
DIN_U = [[J5X - din_w / 2 - din_clr, J5X + din_w / 2 + din_clr], [J6X - din_w / 2 - din_clr, J6X + din_w / 2 + din_clr]];
DIN_MERGE = DIN_U[1][0] - DIN_U[0][1] < min_web;
DIN_BACK = 15.185;            // BOARD: the DIN bodies' back (y)
DIN_RAIL = [DIN_U[0][0] + din_clr, DIN_BACK + din_rail_clr, lid_z1 - 0.01,
            W + fit - tab_clr - hook[1] - din_rail_hook_clr, din_rail_y1, -T - board_gap];
// the tongue: the DIN window's width less tongue_clr a side, from the lid's top to din_lock_h over the DIN bottoms
module din_tongue() for (d = DIN_MERGE ? [[DIN_U[0][0], DIN_U[1][1]]] : DIN_U)
    translate([d[0] + tongue_clr, lid_z1 - 0.01]) square([d[1] - d[0] - 2 * tongue_clr, floor_gap + din_lock_h + 0.01]);
// the wall under the USB-C window, from J3's slot to the DIN window, is the lid's (mots, 2026-11-20): the shell is
// open there from its bottom edge to the window, and the lid's panel fills it, its top edge the window's lower edge
USB_PANEL_U = [J3X + plug_d / 2, DIN_U[0][0]];     // 23.1 .. 51.65
USB_PANEL_TOP = usb_om_z[0] - om_clr;              // -1.8: the USB-C window's lower edge (under our board, -1.6)
module usb_panel_slot() translate([USB_PANEL_U[0], lid_z1 - 2]) square([USB_PANEL_U[1] - USB_PANEL_U[0], USB_PANEL_TOP - lid_z1 + 2 + 0.01]);
// one wall with J3's tongue and the DIN tongue (mots: "why the slit on left and right?": no shell left between them):
// on the left it joins J3's tongue below the plug's axis (the round opening cut out); on the right it reaches the DIN tongue under
// the shell's 0.6 lip over the DIN window (z -2.2), tongue_clr below it
module usb_panel() difference() {
    union() {
        translate([USB_PANEL_U[0] + tongue_clr, lid_z1 - 0.01])          // the panel (the shell above J3's opening:
            square([USB_PANEL_U[1] - USB_PANEL_U[0] - 2 * tongue_clr, USB_PANEL_TOP - lid_z1 + 0.01]);   // tongue_clr)
        translate([J3X + plug_d / 2 - tongue_clr, lid_z1 - 0.01])          // joined to J3's tongue below the plug's axis
            square([2 * tongue_clr + 0.02, jack_axis - lid_z1 + 0.01]);
        translate([USB_PANEL_U[1] - tongue_clr - 0.01, lid_z1 - 0.01])
            square([2 * tongue_clr + 0.02, -T - din_top_lip - tongue_clr - lid_z1 + 0.01]);
    }
    translate([J3X, jack_axis]) circle(d = plug_d + 0.01);
}
module din_windows() {
    zt = -T - din_top_lip;
    if (DIN_MERGE) translate([DIN_U[0][0], lid_z1 - 2]) square([DIN_U[1][1] - DIN_U[0][0], zt - lid_z1 + 2]);
    else for (d = DIN_U) translate([d[0], lid_z1 - 2]) square([d[1] - d[0], zt - lid_z1 + 2]);
}
module openings() {
    through_wall("top") {
        jack_slot(J2X, plug_d);  jack_slot(J3X, plug_d);
        translate([USB_U[0], usb_om_z[0] - om_clr]) offset(1) offset(-1)   // (to the DIN window: no 0.3 mm sliver)
            square([DIN_U[0][0] + 1 - USB_U[0], usb_om_z[1] - usb_om_z[0] + 2 * om_clr]);
        usb_panel_slot();
        din_windows();
    }
    through_wall("right") { jack_slot(J13Y, plug_d); jack_slot(J14Y, plug_d); }
    through_wall("bottom") jack_slot(J7X, plug_d);
    // the power switch in the left wall, and its patch: the wall thinned from the inside to psw_panel round its hole
    through_wall("left") translate([psw_y, psw_z]) circle(d = psw_hole);
    in_wall("left", fit - 0.01, wall - psw_panel + 0.01) translate([psw_y, psw_z]) circle(d = psw_thin_d);
    through_wall("left") lcd2_usb_slot();
}

module face_cuts() {
    // the screen window: the active area + a bezel, chamfered 45 deg from the top
    hull() {
        translate([0, 0, face_z0 - 1]) linear_extrude(face_t - win_chamfer + 1)
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

module shell() difference() { shell_body(); openings(); face_cuts(); lid_screw_holes(); }
// the labels as their own body, filling the face's cuts exactly (the second colour: the same origin as the shell's STL)
module face_labels() translate([0, 0, face_top - lab_depth]) linear_extrude(lab_depth) labels2d();

// ---------------------------------------------------------------------------------------------------------------
// the lid
module lid() {
    difference() {
        union() {
            translate([0, 0, lid_z0]) linear_extrude(lid_t) outline2d();
            // the side screws' tabs and the right wall's hook
            lid_tabs();
            // tongues: close the wall slots under the jacks and the LCD-2's USB-C (flush with the walls)
            in_wall("top", fit, wall) { jack_tongue(J2X); jack_tongue(J3X); }
            in_wall("right", fit, wall) { jack_tongue(J13Y); jack_tongue(J14Y); }
            in_wall("bottom", fit, wall) jack_tongue(J7X);
            in_wall("left", fit, wall) lcd2_usb_tongue();
            // rails that hold the two bridge strips up (under their long edges)
            for (yc = strip_y, s = [-1, 1])
                bx([ROW_X - strip_L / 2 + 1.0, yc + (s < 0 ? -strip_rail_off[1] : strip_rail_off[0]), lid_z1 - 0.01,
                    ROW_X + strip_L / 2 - 1.0, yc + (s < 0 ? -strip_rail_off[0] : strip_rail_off[1]),
                    strip_z0 - strip_post_gap]);
            // the L on the cell's top-left corner: keeps the cell (with the rubber) in the corner, backs our board
            for (b = [L_TOP, L_LEFT]) bx(b);
            // the DIN lock: the rail behind both DIN bodies (up to under our board, like the L) and the tongue in front
            bx(DIN_RAIL);
            in_wall("top", fit, wall) { din_tongue(); usb_panel(); }
        }
    }
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
PSW_IN = -(fit + wall - psw_panel);       // the thinned panel's inner face (board x; the left wall)
PSW_OUT = -(fit + wall);                  // the wall's outer face
PSW_TAB = PSW_IN + psw_body[1] + psw_tabs[1];   // the tabs' far end (board x)
module kcd1(g = 0) {
    at([PSW_IN - g, psw_y], psw_z) rotate([0, 90, 0]) cylinder(d = psw_body[0] + 2 * g, h = psw_body[1] + 2 * g);
    bx(grow([PSW_IN + psw_body[1], psw_y - psw_tabs[0] / 2, psw_z - psw_tabs[2] / 2,
             PSW_TAB, psw_y + psw_tabs[0] / 2, psw_z + psw_tabs[2] / 2], g));
    at([PSW_OUT - psw_cap[1] - g, psw_y], psw_z) rotate([0, 90, 0]) cylinder(d = psw_cap[0] + 2 * g, h = psw_cap[1] + 2 * g);
}

// ---------------------------------------------------------------------------------------------------------------
// the check: intersection of one case part with one board group grown by ck_grow (empty = clearance >= ck_grow)
ck_part = "shell";  ck_group = "top";  ck_grow = 0.0;  ck_drop = 0.0;  ck_psw = 0.0;  ck_tilt = [1, 0, 0];
module ck_case() {
    if (ck_part == "shell") shell();
    if (ck_part == "lid") translate([0, 0, -ck_drop]) lid();   // (ck_drop: off the faces it only touches)
    if (ck_part == "cap") cap(ck_tilt);   // (ck_tilt: a press, [direction x, y, degrees])
    if (ck_part == "switch") kcd1(ck_psw);    // (the power switch against the board groups / the shell / the lid)
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
    if (ck_group == "shell") shell();          // case part against case part (lid / cap against the shell)
    if (ck_group == "lid") lid();
}

// ---------------------------------------------------------------------------------------------------------------
module assembly(e = 0) {
    color("lightgray") translate([0, 0, e]) shell();
    color("#111111") translate([0, 0, e]) face_labels();
    color("orange") translate([0, 0, 1.5 * e]) cap();
    color("#111111") translate([0, 0, 1.5 * e]) cap_symbols();
    board();
    color("slategray") translate([0, 0, -0.8 * e]) lid();
    color("#222222") translate([0, 0, e]) kcd1();
}
module cutaway(axis, at_) {        // keep x <= at_ ("x") or y_board <= at_ ("y")
    intersection() {
        children();
        if (axis == "x") translate([at_ - 1000, -1000, -1000]) cube([1000, 2000, 2000]);
        if (axis == "y") translate([-1000, -at_, -1000]) cube([2000, 1000, 2000]);
    }
}
module section(axis, at_) {
    color("lightgray") cutaway(axis, at_) shell();
    color("orange") cutaway(axis, at_) cap();
    color("slategray") cutaway(axis, at_) lid();
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

if (part == "assembly") assembly(0);
if (part == "exploded") assembly(40);
if (part == "shell") shell();
if (part == "lid") lid();
if (part == "cap") cap();
if (part == "cap_symbols") cap_symbols();
if (part == "cap_view") { color("orange") cap(); color("#111111") cap_symbols(); }
if (part == "board") board();
if (part == "inside") { color("slategray") lid(); color("silver") grp_bottom(); color("teal") grp_strips(); color("royalblue") grp_cell();
                         color("dimgray") grp_screws(); color("#222222") kcd1(); }   // the shell, our board and the LCD-2 off
if (part == "left_wall") { color("lightgray") shell(); color("#222222") kcd1(); color("slategray") lid(); board(); }
if (part == "section_switch") section("y", psw_y);
if (part == "top") { color("lightgray") shell(); color("#111111") face_labels(); color("orange") cap(); color("#111111") cap_symbols(); board(); }
if (part == "shell_labels") face_labels();
if (part == "section_din") section("x", DPX);   // (through J5's DIN and the D-pad centre: the nub, the UP / DOWN pads)
if (part == "section_screen") section("y", DPY);
if (part == "check") intersection() { ck_case(); ck_board(); }

// the numbers BOX.md quotes (openscad -o x.echo box.scad)
echo(outside_x = W + 2 * (fit + wall), outside_y = H + 2 * (fit + wall),
     walls_x = W + 2 * (fit + wall), walls_y = H + 2 * (fit + wall), height = face_top - lid_z0,
     with_cap = face_top + cap_proud - lid_z0, face_top = face_top, lid_z1 = lid_z1, cell_top = cell_top,
     usb_din_web = DIN_U[0][0] - USB_U[1], din_merged = DIN_MERGE, window = [AA[0] - win_bezel, AA[1] - win_bezel,
     AA[2] + win_bezel, AA[3] + win_bezel], cap_fb = CAP_FB, cap_ft = CAP_FT, tilt_arm = TILT_ARM, tilt_diag = TILT_DIAG,
     hook_vb = HOOK_VB, tab_rects = TAB_RECTS);
// the numbers floor_map.py draws (it runs: openscad -o x.echo box.scad, and reads this line)
echo(floor = [W, H, din_depth, -T - lid_z1, [cell_x0, cell_y0, cell_x1, cell_y1, cell[2]], rubber_gap,
     [L_TOP[0], L_TOP[1], L_TOP[3], L_TOP[4]], [L_LEFT[0], L_LEFT[1], L_LEFT[3], L_LEFT[4]], CUT, [DPX, DPY, SW_OFF],
     MOUNT, ROW_X, strip_L, strip_W, strip_y,
     [psw_y, psw_z, psw_body[0], PSW_IN, PSW_TAB, psw_tabs[0], PSW_OUT - psw_cap[1], psw_cap[0]], TAB_RECTS]);
