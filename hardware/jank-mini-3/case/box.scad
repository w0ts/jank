// SPDX-License-Identifier: GPL-3.0-only
// jank-mini-3 box, STAGE 1 (Claude, 2026-11-19, for mots's approval): a TOP SHELL (the face and all four walls, full
// depth) and a flat BOTTOM LID screwed into it, plus one printed cross D-pad cap. FDM, PETG, 0.4 mm nozzle, 0.2 mm layers.
//   shell: printed FACE DOWN (the face on the bed; walls, bosses and corner towers grow up; the USB-C window's lower
//          edge prints as a 25.8 mm bridge)
//   lid:   printed OUTSIDE DOWN (the strip rails, the cell's L and the tongues grow straight up: no bridges)
//   cap:   printed FLANGE DOWN (its flat underside, which rests on the four switches, on the bed)
// The board screws to the shell from below (4 x M2 into heat-set inserts in bosses on the face); the lid screws to the
// shell from below (4 x M3 countersunk into heat-set inserts in four towers at the outside corners: the board drops in
// from below and fills the whole inside, so nothing of the shell may sit under it).
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
psw_y = 71.5;              // its centre, board y: the cap (y 63.25-79.75) 0.75 short of the bottom-left tower's fillet
                           //  (from y 80.5 on the wall's outside), the body (y 64.0-79.0) 0.85 past the lower strip (y 63.15)
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
m3_ins = [4.0, 6.5];       // M3 x 5.7 insert (OD ~4.6): hole d 4.0, 6.5 deep
col_d = 7.5;               // the lid screws' towers (wall 1.75 round the insert)
lid_pad = [6.0, 1.0];      // a pad on the lid under each tower: the lid is 3.0 thick under the countersunk head
pad_clr = 0.2;             // the pad's recess in the tower (a side)
m3_cs = [3.4, 6.4, 1.7];   // ISO 10642 M3 countersink: hole, head d, head depth (screws M3 x 8)
// the lid screws: the board drops in from below and fills the whole inside, so nothing of the shell may sit under it:
// four towers OUTSIDE the board's corners, their centres just far enough out that they clear the inside's corner
tw_c = col_d / 2 / sqrt(2) + 0.2;
TOWERS = [[-fit - tw_c, -fit - tw_c], [W + fit + tw_c, -fit - tw_c], [W + fit + tw_c, H + fit + tw_c],
          [-fit - tw_c, H + fit + tw_c]];

// the D-pad cap (mots: 24 mm cross, 8 mm arms)
cap_L = 24.0;  cap_A = 8.0;
cap_clr = 0.4;             // face opening a side
cap_flange = 1.2;          // retaining flange beyond the cross (keep-out is cross + 1.0: BOARD DPAD_KEEP)
cap_play = 0.15;           // flange top -> face underside
cap_proud = 2.0;           // above the face
cap_chamfer = 0.6;

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
lab_h = 2.2;  lab_depth = 0.6;  lab_font = "Liberation Sans:style=Bold";

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
module grp_sw(g = 0) for (p = BOARD_PARTS) if (p[2] == "SW-SMD") bx(grow([for (i = [3:8]) p[i]], g));
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
// the outside: the walls' rounded rectangle and the four towers, filleted where they meet
module outline2d() offset(r = -1.0) offset(r = 1.0) {
    rrect(OUT[0], OUT[1], OUT[2], OUT[3], corner_r);
    for (t = TOWERS) at(t) circle(d = col_d);
}
module tower_holes() for (t = TOWERS) {
    at(t, lid_z1 - 0.01) cylinder(d = lid_pad[0] + 2 * pad_clr, h = lid_pad[1] + 0.01);     // the lid pad's recess
    at(t, lid_z1 + lid_pad[1] - 0.01) cylinder(d = m3_ins[0], h = m3_ins[1]);                 // the M3 insert
}

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
module din_windows() {
    zt = -T - din_top_lip;
    if (DIN_MERGE) translate([DIN_U[0][0], lid_z1 - 2]) square([DIN_U[1][1] - DIN_U[0][0], zt - lid_z1 + 2]);
    else for (d = DIN_U) translate([d[0], lid_z1 - 2]) square([d[1] - d[0], zt - lid_z1 + 2]);
}
module openings() {
    through_wall("top") {
        jack_slot(J2X, plug_d);  jack_slot(J3X, plug_d);
        translate([USB_U[0], usb_om_z[0] - om_clr]) offset(1) offset(-1)
            square([USB_U[1] - USB_U[0], usb_om_z[1] - usb_om_z[0] + 2 * om_clr]);
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

// the labels: [x, y (board), lines]; TO ESP / TO DEVICE sit 13 mm apart: two lines each
LABELS = [[J2X, 4.5, ["MIDI 1"]], [J3X, 4.5, ["MIDI 2"]], [J1X, 4.5, ["TO", "ESP"]], [J4X, 4.5, ["TO", "DEVICE"]],
          [J5X, 4.5, ["MIDI 1"]], [J6X, 4.5, ["MIDI 2"]], [J7X, H - 4.0, ["LINE IN"]],
          [W - 5.5, J13Y, ["CLK"]], [W - 5.5, J14Y, ["RST"]]];
module labels2d() for (l = LABELS, i = [0 : len(l[2]) - 1])
    at([l[0], l[1] + (i - (len(l[2]) - 1) / 2) * lab_h * 1.35])
        text(l[2][i], size = lab_h, font = lab_font, halign = "center", valign = "center");

module shell() difference() { shell_body(); openings(); face_cuts(); tower_holes(); }
module label_fill() translate([0, 0, face_top - lab_depth + 0.02]) linear_extrude(0.01) labels2d();   // (pictures only)

// ---------------------------------------------------------------------------------------------------------------
// the lid
module lid() {
    difference() {
        union() {
            translate([0, 0, lid_z0]) linear_extrude(lid_t) outline2d();
            for (t = TOWERS) at(t, lid_z1 - 0.01) cylinder(d = lid_pad[0], h = lid_pad[1] + 0.01);
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
        }
        for (t = TOWERS) at(t, lid_z0 - 0.01) {
            cylinder(d = m3_cs[0], h = lid_t + lid_pad[1] + 1);
            cylinder(d1 = m3_cs[1], d2 = m3_cs[0], h = (m3_cs[1] - m3_cs[0]) / 2);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the D-pad cap: a flat flange resting on the four plungers, the cross through the face, chamfered on top
module cap() {
    ztop = face_top + cap_proud;
    at([DPX, DPY]) {
        translate([0, 0, SW_H]) linear_extrude(face_z0 - cap_play - SW_H) offset(delta = cap_flange) cross2d(cap_L, cap_A);
        for (d = [[cap_L, cap_A], [cap_A, cap_L]]) hull() {
            translate([-d[0] / 2, -d[1] / 2, face_z0 - cap_play - 0.01]) cube([d[0], d[1], ztop - cap_chamfer - face_z0 + cap_play]);
            translate([-d[0] / 2 + cap_chamfer, -d[1] / 2 + cap_chamfer, ztop - 0.01])
                cube([d[0] - 2 * cap_chamfer, d[1] - 2 * cap_chamfer, 0.01]);
        }
    }
}

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
ck_part = "shell";  ck_group = "top";  ck_grow = 0.0;  ck_drop = 0.0;  ck_psw = 0.0;
module ck_case() {
    if (ck_part == "shell") shell();
    if (ck_part == "lid") translate([0, 0, -ck_drop]) lid();   // (ck_drop: off the faces it only touches)
    if (ck_part == "cap") cap();
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
    color("black") translate([0, 0, e]) label_fill();
    color("orange") translate([0, 0, 1.5 * e]) cap();
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
if (part == "board") board();
if (part == "inside") { color("slategray") lid(); color("silver") grp_bottom(); color("teal") grp_strips(); color("royalblue") grp_cell();
                         color("dimgray") grp_screws(); color("#222222") kcd1(); }   // the shell, our board and the LCD-2 off
if (part == "left_wall") { color("lightgray") shell(); color("#222222") kcd1(); color("slategray") lid(); board(); }
if (part == "section_switch") section("y", psw_y);
if (part == "top") { color("lightgray") shell(); color("black") label_fill(); color("orange") cap(); board(); }
if (part == "section_din") section("x", J5X);
if (part == "section_screen") section("y", DPY);
if (part == "check") intersection() { ck_case(); ck_board(); }

// the numbers BOX.md quotes (openscad -o x.echo box.scad)
echo(outside_x = W + 2 * (fit + tw_c + col_d / 2), outside_y = H + 2 * (fit + tw_c + col_d / 2),
     walls_x = W + 2 * (fit + wall), walls_y = H + 2 * (fit + wall), height = face_top - lid_z0,
     with_cap = face_top + cap_proud - lid_z0, face_top = face_top, lid_z1 = lid_z1, cell_top = cell_top,
     usb_din_web = DIN_U[0][0] - USB_U[1], din_merged = DIN_MERGE, window = [AA[0] - win_bezel, AA[1] - win_bezel,
     AA[2] + win_bezel, AA[3] + win_bezel], tower_c = tw_c);
// the numbers floor_map.py draws (it runs: openscad -o x.echo box.scad, and reads this line)
echo(floor = [W, H, din_depth, -T - lid_z1, [cell_x0, cell_y0, cell_x1, cell_y1, cell[2]], rubber_gap,
     [L_TOP[0], L_TOP[1], L_TOP[3], L_TOP[4]], [L_LEFT[0], L_LEFT[1], L_LEFT[3], L_LEFT[4]], CUT, [DPX, DPY, SW_OFF],
     MOUNT, ROW_X, strip_L, strip_W, strip_y,
     [psw_y, psw_z, psw_body[0], PSW_IN, PSW_TAB, psw_tabs[0], PSW_OUT - psw_cap[1], psw_cap[0]]]);
