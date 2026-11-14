// SPDX-License-Identifier: GPL-3.0-only
// jank mini 2.2: the printed case for the mini 2.2 board (mini2.kicad_pcb, 66 x 58 x 1.6 mm), with (din = true) or
// without (din = false) the two 5-pin DIN sockets J5 / J6 on the board's bottom, two lid variants (lid_var = "led": the
// LED window and its light pipe; "screen": a window over a 0.96" OLED module on J10, with a bezel pocket), and the
// cells under the board (cells = true: two 1S cells standing on edge in the band between the DIN bodies and the bottom
// wall, a divider rib between them).
//
// The board lies top up. Its TOP carries the USB-C, the screen module on its header and the four buttons; the LID is
// the deep part over it: 11 mm inside, kept from mini 2 for the upright FPC antenna (10 mm wide) on the lid's bottom
// wall. Its BOTTOM carries (stage 2) the three jacks (PJ-307C, soldered by the user), the DINs (optional), the PH2.0
// sockets and the cells: the BASE is a deep box either way (the cells' height), its walls hold the jacks' noses in
// U-slots open at the top (the board drops in from above).
// Stage 2 (the user's): the four buttons under ONE printed cross-shaped rocker (a D-pad) in a cross-shaped hole in the
// lid: a centre pivot nub rests on the board at the cross's middle, a boss under each arm stops 0.15 mm over its switch,
// a chamfered flange under the roof keeps it in (README "Buttons"; check_dpad.py: the tilt). The power switch (a
// panel-mount MTS-102 toggle) sits in the lid's right wall, its M6 bushing through a round hole, its nut outside.
//
// The case splits 0.1 mm under the board's top face (z 1.5): the base holds the board (its edge, its underside, the
// divider rib), the lid is a cap over it; a half-lap joint (the lid's thin skirt outside, the base's thin lap inside)
// with six snap bumps closes it. No screw, no insert, no metal.
//
// Frame (every number): X = the board's x (0 at its left edge), Y = H - the board's y (0 at the bottom edge: J4's and the
// antenna's side, the USB-C edge at the top, Y = H), Z = 0 at the board's BOTTOM face (the board is z 0..1.6). Board
// point (x, y) = (x, by(y)). Every dimension marked UNVERIFIED is from a drawing or a 3D model, not a part in hand.

/* [What to render] */
part = "assembly";   // [assembly, open, open_full, base, lid, dpad_cap, led_pipe, check_*, section_*]
din = false;         // true: the DIN sockets J5 / J6 fitted: their slots in the base
lid_var = "led";     // "led": the LED window; "screen": the OLED's window (the size: screen)
screen = "s130";     // (72 x 72, "one board, different screens") s130, s154 on J10's header, s242 hung from the roof (s096: out, unreadable)
cells = true;        // the cells under the board (the base's depth, the divider rib)

/* [Board] */
W = 66;  H = 58;  bt = 1.6;     // (make_stl.sh sets W and H from the board)
function by(y) = H - y;
/* [Case] */
e = 1.8;          // the outer face from the board's edge
clr = 0.4;        // the board's side play (each side)
wall = e - clr;   // 1.4
sk = 0.7;         // the lid's skirt (the lap's outer half)
lap_play = 0.15;
r_out = 1.8;          // (the cavity's corner 0.4: the board's square corner clears it)
zs = 1.0;         // the seam: the base's lap top (the lid's shoulder); the jacks' front blocks start at z 1.4
zlid = 0.1;       // the lid's skirt bottom (the base's shoulder is z 0)
z_in = bt + 11.0;         // the lid's inside roof: 11 mm over the board (the antenna's 10 mm upright on the bottom wall)
roof = 1.4;
zt = z_in + roof;         // 14.0
floor_t = 1.4;
/* [Under the board] */
tail = 3.9;       // the longest tail under the board (the USB-C's shell legs), + margin
/* [The jacks (stage 2): PJ-307C under the board, LCSC's drawing + the EasyEDA model (UNVERIFIED in hand)] */
j2 = [6.0, 6.4];  j3 = [60.0, 6.4];  j7 = [17.6, 6.0];   // the footprints' origins, board (x, y) (make_stl.sh sets them)
jb = [12.0, 11.0, 5.0];   // the body: wide (across the axis), deep (from the front face), tall (down from the board)
nose_d = 5.0;  nose_l = 3.0;   // the nose past the front face; the plug's hole in it 3.6
j_face = 6.0;             // the front face: this far from the origin, toward the nose
// stage 2: CLOCK / RESET, two PJ-320A under the board at the top edge, noses up (its JLC footprint: body 12.1 x 6.2,
// the nose 2.0 long, 5 across; 5.0 tall as the jank box's XKB PJ-320A: UNVERIFIED for this maker)
j13 = [42.3, 7.25];  j14 = [49.0, 7.25];
cvb = [6.2, 12.1, 5.0];  cv_face = 7.25;  cv_nose_l = 2.0;
pin_up = 1.8;             // the legs past the board's top (3.3 long - 1.6 board, + margin: UNVERIFIED)
slot_w = nose_d + 1.0;    // the base's U-slot round the nose (0.5 each side), open at the top
/* [The power switch (stage 2): CAX-MTS102-YB-10.5JL (C55159907), panel mount in the lid's right wall (its drawing:
   M6 x 0.75 bushing 8.6 long, body 12.8 x 6.8 x 9.0 + 4.0 terminals, lever 10.5, panel hole 6.0)] */
pwr = [60.6, 15.0];       // J11, its wire pads, board (x, y) (make_stl.sh sets it); the switch's axis 5 mm further down
pwr_dy = 5.0;  pwr_z = bt + 7.0;   // the bushing's axis: board y = pwr.y + pwr_dy, height over the board's bottom
pwr_body = [9.0 + 4.0, 12.8, 6.8];   // inside the wall: deep (body + terminals), along the wall, tall
pwr_hole = 6.3;           // the wall's hole (the drawing's 6.0 + 0.3 for the print)
pwr_nut = 9.0;  pwr_lever = 10.5;   // the nut across flats (ASSUMED: M6 thin nut, ~10 across corners), the lever
din_h = 19.6;  din_gap = 0.35;
/* [The cells: two 1S cells on edge under the board (the user measures them: set these)] */
cell_len = 66;    // along x (GNB 550 mAh 65..66, BT2.0 450 mAh 62); the case's inside width is W + 2 clr
cell_h = 18;      // standing on edge: its width is its height (GNB 18, BT2.0 15.5)
cell_t = 6.7;     // its thickness, along y (BT2.0 6.7, GNB 6..6.2)
cell_y0 = H - 17.0;   // the band's top, board y (17 mm from the bottom edge: 41.0 on the 58 mm board, 47.0 on 64) (the DIN bodies end at 39.0, the ribs of their plates' slots at 40.85; the PH2.0 sockets' bodies are above it, at y 30..36)
rib_t = 2.0;      // the divider rib between the two cells' slots (it also carries the board under the buttons)
cell_room = cell_h + 1.0;                     // the floor under the cells
under = max(din ? din_h + din_gap : tail + 0.4, cells ? cell_room : 0);   // the floor's top under the board
zbot = -under - floor_t;
rib_y = (cell_y0 + H) / 2;                    // the rib's middle, board y (each slot (H - cell_y0 - rib_t) / 2 deep)
/* [DIN: HOOYA DIN-504B-M10, drawing rev A1 + lib/3d model] */
din_y = 28.3;                            // the axis, board y (make_stl.sh sets it from the board)
plate = [20.9, 1.8, 19.6];               // along the edge, thick, high (z down from the board)
body = [17.9, 15.3 - 1.8, 17.6];         // behind the plate: wide, deep, high (UNVERIFIED: the 3D model, +-0.2)
play = 0.35;                             // the body to the back cheek's opening
slot_play = 0.1;                         // the plate in its slot: a face and each side edge
rim = [1.3, 2.2];                        // what the cheeks hold of the plate: each side, the far edge
back_x = 2.4;  back_t = 2.0;             // the back cheek: its front face (clear of the jacks' pegs at x 2.05), its thickness
din_holes = [[12.25, din_y + 7.5], [14.75, din_y + 5], [12.25, din_y], [14.75, din_y - 5], [12.25, din_y - 7.5],
             [2.45, din_y + 5], [2.45, din_y - 5]];   // J5 (board x, y), following din_y
/* [Parts the case meets (make_stl.sh sets these from the board: place_args.py)] */
usb_x = W / 2;  usb_w = 13.0;  usb_top = bt + 5.2;  usb_bot = -0.2;   // USB-C: its mouth axis 1.6 over the board; a plug's overmold ~ 12 x 6.5
btn = [[55.5, 41.7], [49, 48.2], [55.5, 54.7], [62, 48.2]];   // UP, NO / BOOT, DOWN, YES (SW3, SW1, SW4, SW5): the cross
led = [5.5, 16.1];        // the middle of the red and the green LED (one window)
hdr = [34, 14.8];         // J10: the screen's 1 x 4 header (the module's top edge 1.5 mm above its pin row)
j4 = [42.2, 55.4];        // the u.FL
ph = [[36.5, 33], [43, 33]];   // J8, J9: the PH2.0 sockets (bottom)
sw_top = bt + 1.5;        // the tactile switches' plunger top (TS-1187A-B-A-B: H 1.5, travel 0.2 +- 0.1 / 0.25: its drawing)
led_top = bt + 0.8;       // (the mockup's block; the parts are 0.6 / 0.8)
/* [The D-pad (stage 2; README "Buttons"; check_dpad.py checks the tilt)] */
cross_c = [btn[0][0], (btn[0][1] + btn[2][1]) / 2];   // the cross's middle = the pivot (make_board.BTN_C)
arm_w = 5.0;  arm_l = 8.5;   // each arm's width, its tip from the middle
gap_side = 0.9;  gap_end = 0.9;   // the hole round the cap (the brief's 0.3 cannot work: a tilt about the pivot on the
                                  //  board moves the cap's top ~11 mm higher sideways by ~0.7 mm at full travel: every
                                  //  arm's sides and the pressed tip; check_dpad.py)
proud = 0.6;              // the cap's face over the roof
fl_w = 1.2;  fl_t = 1.0;  fl_gap = 1.0;   // the flange: 1.2 mm past the arms' sides (not at the tips: the walls), its
                                          //  thickness, its gap under the roof at rest (the tilt lifts the far side 0.8)
boss_r = 6.5;  boss_d = 2.5;  boss_gap = 0.15;   // the bosses over the actuators: their distance, width, rest gap
pivot_d = 2.0;  pivot_tip = 1.0;   // the centre column and its rounded tip on the board
/* [The screens (72 x 72, the user's "one board, different screens"): the makers' drawings (HanSheng / LCSC sheets,
   page 6: hardware/mini2/screen-study.md). Per size: module w, h, thickness, its header's pin row from its top edge,
   the active area (x, y from the module's top-left, w, h). s096 / s130 / s154 plug into J10 (pins centred on the
   module, the module hanging down from the row); s242 lies across the top, hung from the roof by its four corner
   holes (68 x 39 mm, 3.1), on a 4-wire lead to J10B (its header is down its left side, over J5's DIN)] */
SCREENS = [["s096", 27.3, 27.8, 2.65, 1.44, [2.78, 6.4, 21.74, 10.86]],
           ["s130", 35.4, 33.5, 2.6, 2.0, [2.95, 7.35, 29.42, 14.70]],
           ["s154", 42.4, 38.0, 2.65, 2.5, [3.68, 8.15, 35.05, 17.52]],
           ["s242", 72.0, 43.0, 4.2, 0, [8.5, 5.11, 55.01, 27.49]]];
scr = SCREENS[search([screen], SCREENS)[0]];
hung = screen == "s242";
mod_w = scr[1];  mod_h = scr[2];
mod_xy = hung ? [0, 1.0] : [hdr[0] - mod_w / 2, hdr[1] - scr[4]];   // the module's top-left corner, board (x, y)
mod_z1 = hung ? bt + 10.5 : bt + 2.5 + scr[3];   // its glass top (s242: 0.5 under the roof; the others: on the header's 2.5 mm plastic)
mod_z0 = hung ? bt + 4.25 : bt + 2.5;            // its lowest point (s242: the parts on its back, 2.2 mm: the study)
aa = scr[5];
win = hung ? [57.5, 29.5] : [aa[2] + 2.76, aa[3] + 2.14];   // the window over the active area (as the 0.96"'s 24.5 x 13.0)
win_dy = aa[1] + aa[3] / 2;                                 // its centre below the module's top edge
s242_holes = [[2.0, 2.0], [70.0, 2.0], [2.0, 41.0], [70.0, 41.0]];   // the 2.42"'s holes from its top-left (3.1 mm)
bezel = [win[0] + 4.0, win[1] + 4.0, 0.6];               // the bezel pocket on the roof's outer face (a clear cover sheet, 0.5 mm)
/* [The FPC antenna, upright on the inside of the bottom wall (Y = 0), its lead from J4] */
ant_l = 42;  ant_w = 10.5;  ant_d = 0.4;   // the pocket: for a ~40 x 10 mm FPC antenna (ASSUMED size: C1693673's sheet
ant_x0 = 3.0;                              //  could not be read; LCSC's FPCs with a 50 mm lead are about 40 x 10)
base_pads = [[24.3, 27.6, H - 1.0, H + 0.2],   // the USB edge, between J7 and the USB-C's legs (mirrored: 38.4 .. 41.7)
             [3, 9, -0.2, 1.0], [W - 9, W - 3, -0.2, 1.0],        // the bottom edge (not with the cells)
             [-0.2, 0.95, 6, 12]];    // the left edge (mirrored to the right; in the cells' band: off with the cells)
lid_pads = [[2, 2], [2, 6.5], [W - 1.6, by(67.6)], [W - 5.0, by(71.0)]];   // (72 x 72: the right ones in the bottom right corner: the 2.42" covers y 1 .. 44, the 2 k stack and YES the right edge)   // the lid's feet on the board's top, clear of every part
bumps = [[0, 26], [1, 26], [2, W / 2 - 11], [2, W / 2 + 11], [3, W / 2 - 11], [3, W / 2 + 11]];   // [side 0 left 1 right 2 top 3 bottom, along the wall]

module rbox(x0, y0, x1, y1, z0, z1, r) {
    hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y, z0]) cylinder(r = r, h = z1 - z0, $fn = 40);
}
module cav(inset, z0, z1) rbox(-e + inset, -e + inset, W + e - inset, H + e - inset, z0, z1, max(0.4, r_out - inset));
module both() { children(); translate([W, 0, 0]) mirror([1, 0, 0]) children(); }

// ---- the board and what hangs under / stands past it (for the checks and the renders) ----
module mock() { translate([-100, 100 + H, 0]) import("../mockup.stl"); }
// a jack under the board (stage 2): local frame x = toward the nose, y across, z down from the board; the body from its
// front face back jb[1], the nose past the face, the legs up through the board (pin_up over its top)
module jack_local(b = jb, nl = nose_l) {
    translate([-b[1], -b[0] / 2, -b[2]]) cube([b[1], b[0], b[2]]);
    translate([0, 0, -b[2] / 2]) rotate([0, 90, 0]) cylinder(d = nose_d, h = nl, $fn = 40);
    translate([-b[1] + 1.0, -b[0] / 2 + 0.5, 0]) cube([b[1] - 2.0, b[0] - 1.0, bt + pin_up]);   // (the legs' zone)
}
module jacks() {
    translate([j2[0] - j_face, by(j2[1]), 0]) rotate(180) jack_local();          // J2: nose out of the left edge
    translate([W - (W - j3[0] - j_face), by(j3[1]), 0]) jack_local();          // J3: out of the right edge
    translate([j7[0], by(j7[1] - j_face), 0]) rotate(90) jack_local();          // J7: out of the top edge (Y = H)
    for (j = [j13, j14]) translate([j[0], by(j[1] - cv_face), 0]) rotate(90) jack_local(cvb, cv_nose_l);   // CLOCK, RESET
}
module jack_noses_only() {   // (for the slot checks)
    translate([j2[0] - j_face, by(j2[1]), -jb[2] / 2]) rotate([0, -90, 0]) cylinder(d = nose_d, h = nose_l, $fn = 40);
    translate([j3[0] + j_face, by(j3[1]), -jb[2] / 2]) rotate([0, 90, 0]) cylinder(d = nose_d, h = nose_l, $fn = 40);
    translate([j7[0], by(j7[1] - j_face), -jb[2] / 2]) rotate([-90, 0, 0]) cylinder(d = nose_d, h = nose_l, $fn = 40);
    for (j = [j13, j14]) translate([j[0], by(j[1] - cv_face), -cvb[2] / 2]) rotate([-90, 0, 0]) cylinder(d = nose_d, h = cv_nose_l, $fn = 40);
}
// (72 x 72: the toggle in the LEFT wall, under the screen band: the right wall's geometry mirrored about x = W / 2)
module pwr_side() if (pwr_left) translate([W, 0, 0]) mirror([1, 0, 0]) children(); else children();
pwr_left = true;
module pwr_switch(inside = true) pwr_side() pwr_switch_r(inside);
module pwr_switch_r(inside = true) {   // the toggle in the lid's right wall: the body inside, the bushing, nut and lever out
    yc = by(pwr[1] + pwr_dy);
    if (inside) translate([W + clr - pwr_body[0] - 0.05, yc - pwr_body[1] / 2, pwr_z - pwr_body[2] / 2]) cube([pwr_body[0], pwr_body[1], pwr_body[2]]);
    translate([W + clr - 0.05, yc, pwr_z]) rotate([0, 90, 0]) cylinder(d = 6.0, h = 8.6, $fn = 32);
    translate([W + e + 0.05, yc, pwr_z]) rotate([0, 90, 0]) cylinder(d = pwr_nut / cos(30), h = 2.0, $fn = 6);
    translate([W + clr + 8.6, yc, pwr_z]) rotate([0, 90 - 13, 0]) cylinder(d = 3.0, h = pwr_lever, $fn = 16);
}
module tails() {       // under the board: the USB-C's legs; tail deep (assumed)
    for (p = [[W / 2 + 4.32, 6.8], [W / 2 + 4.32, 2.59], [W / 2 - 4.32, 6.8], [W / 2 - 4.32, 2.59]]) translate([p[0] - 0.5, by(p[1]) - 0.5, -tail]) cube([1, 1, tail + 0.1]);
}
module din_plate() translate([-0.01, by(din_y) - plate[0] / 2, -plate[2]]) cube([plate[1], plate[0], plate[2]]);
module din_model() {   // one socket, J5 (left); face on the edge, body under the board, pins up through it
    din_plate();
    translate([plate[1] - 0.02, by(din_y) - body[0] / 2, -body[2]]) cube([body[1] + 0.02, body[0], body[2]]);
    for (h = din_holes) translate([h[0] - 0.5, by(h[1]) - 0.5, 0]) cube([1, 1, 4.0]);
}
// the PH2.0 sockets under the board with a plug in (body 6.4 x 4.9, ~6 mm, + the plug: 10 mm down, either way round:
// UNVERIFIED), the cells in their slots, the screen module on its header
module ph_model() for (p = ph) translate([p[0] - 3.2, by(p[1]) - 3.0, -10]) cube([6.4, 6.0, 10 + 0.01]);
function slot_y(i) = i == 0 ? [cell_y0, rib_y - rib_t / 2] : [rib_y + rib_t / 2, H];   // the two slots, board y
module cell_model(lift = 0.01) for (i = [0, 1]) let(s = slot_y(i), yc = (s[0] + s[1]) / 2)
    translate([W / 2 - cell_len / 2, by(yc) - cell_t / 2, -under + lift]) cube([cell_len, cell_t, cell_h]);
module module_model() translate([mod_xy[0], by(mod_xy[1] + mod_h), mod_z0]) cube([mod_w, mod_h, mod_z1 - mod_z0]);
module board_all() {
    mock(); jacks(); tails(); ph_model();
    if (din) both() din_model();
    if (cells) cell_model();
    if (lid_var == "screen") module_model();
}

// ---- the base ----
module bump(g = 0) {   // local: x along the wall, y outward from the lap's outer face, z up; g: the pocket's extra
    hull() {
        translate([-3 - g, -0.3, 0.2 - g]) cube([6 + 2 * g, 0.3, 0.65 + 2 * g]);
        translate([-2.4 - g, 0.35 + g, 0.35]) cube([4.8 + 2 * g, 0.01, 0.35]);
    }
}
module at_side(s, u) {
    f = sk + lap_play;
    if (s == 0) translate([-e + f, u, 0]) rotate(90) children();
    if (s == 1) translate([W + e - f, u, 0]) rotate(-90) children();
    if (s == 2) translate([u, H + e - f, 0]) children();
    if (s == 3) translate([u, -e + f, 0]) rotate(180) children();
}
module usb_cut(z0) translate([usb_x - usb_w / 2, H, z0]) cube([usb_w, e + 1, usb_top - z0]);
// the jacks' U-slots in the base's walls (stage 2): round under the nose, open to the top, the board drops in
module jack_slot_local() hull() {
    translate([-1, 0, -jb[2] / 2]) rotate([0, 90, 0]) cylinder(d = slot_w, h = e + 2, $fn = 40);
    translate([-1, -slot_w / 2, 0]) cube([e + 2, slot_w, zs + 2]);
}
module jack_slots() {
    translate([-e - 0.5, by(j2[1]), 0]) jack_slot_local();
    translate([W + e + 0.5, by(j3[1]), 0]) rotate(180) jack_slot_local();
    translate([j7[0], H + e + 0.5, 0]) rotate(-90) jack_slot_local();
    for (j = [j13, j14]) translate([j[0], H + e + 0.5, 0]) rotate(-90) jack_slot_local();   // (CLOCK, RESET)
}
// the posts under the button cross (stage 2): a bar under UP between J6's body and the cells' band (y 39.3 .. 40.8),
// a ledge along the bottom wall under DOWN (over the second cell: 0.75 thick, its underside 0.2 over a cell); NO and
// YES sit over the cells' divider rib, which reaches the board
module button_posts() {
    translate([btn[0][0] - 3.0, by(cell_y0 - 0.2), -under - 0.01]) cube([6.0, 1.5, under - 0.05 + 0.01]);
    translate([btn[2][0] - 4.0, -clr - 0.01, -0.8]) cube([8.0, clr + 0.6 + 0.01, 0.75]);
}

module din_walls() both() {
    ya = by(din_y);
    // the ribs either side of the plate's slot (from the wall to the back cheek), under the board
    for (s = [-1, 1]) translate([-clr - 0.01, ya + (s > 0 ? plate[0] / 2 + slot_play : -plate[0] / 2 - slot_play - 2.0), -under - 0.01])
        cube([back_x + clr + 0.01, 2.0, under - 0.35 + 0.01]);
    // the slot's faces, slot_play from the plate: in front a cheek on the wall (the front window cuts its middle), behind
    // a cheek on the back partition over the plate's rims (the body passes between them)
    translate([-clr - 0.01, ya - plate[0] / 2 - slot_play, -under - 0.01]) cube([clr - slot_play + 0.01, plate[0] + 2 * slot_play, under - 0.35 + 0.01]);
    difference() {
        translate([plate[1] + slot_play, ya - plate[0] / 2 - slot_play, -under - 0.01]) cube([back_x - plate[1] - slot_play + 0.01, plate[0] + 2 * slot_play, under - 0.35 + 0.01]);
        translate([plate[1], ya - body[0] / 2 - play, -body[2] - play]) cube([back_x, body[0] + 2 * play, body[2] + play + 1]);
    }
    // the back cheek, with the body's opening (open at the top); mini 2.2: only beside the DIN (it ran across the case:
    // the cells under the board need the full width below the DINs)
    difference() {
        translate([back_x, ya - plate[0] / 2 - slot_play - 1.5, -under - 0.01]) cube([back_t, plate[0] + 2 * slot_play + 3.0, under - 0.35 + 0.01]);   // (its ends inside the slot ribs' length: no shared faces)
        translate([back_x - 1, ya - body[0] / 2 - play, -body[2] - play]) cube([back_t + 2, body[0] + 2 * play, body[2] + play + 1]);
    }
}
// the divider rib between the two cells' slots: from the floor to 0.05 mm under the board, across the width, so the
// board rests on it under the buttons (the cells are not loaded); its ends leave 1 mm to the walls
module cell_rib() translate([0.6, by(rib_y) - rib_t / 2, -under - 0.01]) cube([W - 1.2, rib_t, under - 0.05 + 0.01]);
module base_core(rib = true) difference() {   // (rib = false: for check_base; the rib has its own checks)
    union() {
        difference() {
            cav(0, zbot, zs - 0.1);
            cav(wall, zbot + floor_t, zs + 1);                               // the cavity
            difference() { cav(0, 0, zs + 1); cav(sk + lap_play, -1, zs + 2); }   // the lap: the outer part off over z 0..
        }
        if (din) din_walls();
        if (cells && rib) cell_rib();
        if (rib) button_posts();
    }
    usb_cut(usb_bot);
    jack_slots();
    if (din) both() translate([-e - 1, by(din_y) - plate[0] / 2 + rim[0], -plate[2] + rim[1]]) cube([e + 1 - 0.2 + 0.001, plate[0] - 2 * rim[0], plate[2] - rim[1] + 0.001]);   // the front window
}
// the base's pads under the board's edge; with the cells, none in their band (Y under H - cell_y0): the rib holds it there
function pad_ok(p) = !cells || p[2] >= by(cell_y0);
module base_pads() {
    for (p = base_pads) if (pad_ok(p)) both() translate([p[0], p[2], -under - 0.01]) cube([p[1] - p[0], p[3] - p[2], under - 0.04]);
}
module base() {   // as printed: floor down
    translate([0, 0, -zbot]) union() {
        base_core();
        base_pads();
        for (b = bumps) at_side(b[0], b[1]) bump();
    }
}

// ---- the lid ----
win_c = [mod_xy[0] + aa[0] + aa[2] / 2, mod_xy[1] + win_dy];   // the screen window's centre, board (x, y)
// the 2.42"'s hangers: a 5 mm post from the roof down to the module's PCB under each hole, a 2.5 mm pilot hole for an
// M3 self-tapping screw from below (its head in the 4.25 mm gap over the board: UNVERIFIED, no part there is taller)
s242_pcb_top = mod_z1 - 2.6;                       // (its 4.2 mm: a 1.6 mm PCB under the panel and its glass, ASSUMED split)
module s242_posts() for (h = s242_holes) translate([h[0], by(1.0 + h[1]), s242_pcb_top])
    difference() { cylinder(d = 5.0, h = z_in - s242_pcb_top + 0.01, $fn = 32); translate([0, 0, -0.01]) cylinder(d = 2.5, h = 6, $fn = 24); }
module screen_frame() {   // an inner frame round the window from the roof down to 0.5 mm over the module's glass
    translate([win_c[0], by(win_c[1]), (mod_z1 + 0.5 + z_in) / 2])
        difference() { cube([win[0] + 2.0, win[1] + 2.0, z_in - mod_z1 - 0.5 + 0.01], center = true); cube([win[0], win[1], 50], center = true); }
}
module lid_core(pads = true) difference() {
    union() {
        difference() {
            cav(0, zlid, zt);
            cav(sk, zlid - 1, zs);                                           // the skirt's room over the base's lap
            cav(wall, zs - 0.01, z_in);                                      // the inside
        }
        if (pads) for (p = lid_pads) translate([p[0], p[1], bt + 0.1 - 0.0]) cylinder(d = 2.4, h = z_in - bt - 0.1 + 0.01, $fn = 24);
        if (lid_var == "led") translate([led[0], by(led[1]), 3.5]) cylinder(d = 4.6, h = z_in - 3.5 + 0.01, $fn = 32);   // the LED's chimney
        if (lid_var == "screen") screen_frame();
        if (lid_var == "screen" && hung) s242_posts();
    }
    usb_cut(usb_bot);                                                    // (stage 2: the jacks' slots are in the base;
    pwr_side() translate([W - 1, by(pwr[1] + pwr_dy), pwr_z]) rotate([0, 90, 0]) cylinder(d = pwr_hole, h = e + 3, $fn = 48);   // the toggle's hole
    intersection() { jack_slots(); translate([-50, -50, -50]) cube([200, 200, 50.6]); }   //  the skirt notched to z 0.6 over each nose)
    cross2d_extrude(gap_side, gap_end, z_in - 1, zt + 1);               // the D-pad's cross-shaped hole
    if (lid_var == "led") {
        translate([led[0], by(led[1]), led_top - 1]) cylinder(d = 3.2, h = zt - led_top + 2, $fn = 24);      // the light pipe's bore
        translate([led[0], by(led[1]), zt - 0.8]) cylinder(d = 4.2, h = 1.0, $fn = 32);                      // its head's recess
    }
    if (lid_var == "screen") {
        translate([win_c[0] - win[0] / 2, by(win_c[1]) - win[1] / 2, z_in - 1]) cube([win[0], win[1], roof + 2]);   // the window
        translate([win_c[0] - bezel[0] / 2, by(win_c[1]) - bezel[1] / 2, zt - bezel[2]]) cube([bezel[0], bezel[1], bezel[2] + 1]);   // the bezel pocket
    }
    for (b = bumps) at_side(b[0], b[1]) bump(0.15);                                                // the snap pockets
    // the FPC antenna's pocket: upright on the inside of the bottom wall (Y = 0 side), ant_d into the wall, its lower edge
    // 0.5 mm over the board; the lead from J4 rises beside it (nothing in the way: open lid)
    translate([ant_x0, -clr - ant_d, bt + 0.5]) cube([ant_l, ant_d + 0.01, ant_w]);
}
// ---- the D-pad (stage 2): a cross in the case frame; grow: added to the arms' sides / tips ----
module cross2d(gs, ge) {
    c = [cross_c[0], by(cross_c[1])];
    translate(c) for (a = [0, 90]) rotate(a) translate([-(arm_l + ge), -(arm_w / 2 + gs)]) square([2 * (arm_l + ge), arm_w + 2 * gs]);
}
module cross2d_extrude(gs, ge, z0, z1) translate([0, 0, z0]) linear_extrude(z1 - z0) cross2d(gs, ge);
fl_top = z_in - fl_gap;                       // the flange's top at rest (its outer edge: a 45 degree chamfer above it)
cap_top = zt + proud;
module dpad_core(pivot = true) {   // in place, at rest: the pivot's tip on the board
    cross2d_extrude(0, 0, fl_top - fl_t, cap_top);                                       // the cross, through the hole
    hull() {                                                                            // the flange, chamfered (prints
        cross2d_extrude(fl_w, 0, fl_top - fl_t, fl_top - fl_w * 0.999);                //  face down without supports)
        cross2d_extrude(0, 0, fl_top - fl_t, fl_top);
    }
    for (a = [0 : 90 : 270]) translate([cross_c[0], by(cross_c[1]), 0]) rotate(a) translate([boss_r, 0, 0])   // the bosses
        translate([0, 0, sw_top + boss_gap]) cylinder(d = boss_d, h = fl_top - fl_t - (sw_top + boss_gap) + 0.01, $fn = 32);
    if (pivot) translate([cross_c[0], by(cross_c[1]), bt]) {                            // the pivot
        translate([0, 0, pivot_tip / 2]) cylinder(d = pivot_d, h = fl_top - fl_t - bt - pivot_tip / 2 + 0.01, $fn = 32);
        translate([0, 0, pivot_tip / 2]) sphere(d = pivot_tip, $fn = 24);
        cylinder(d1 = pivot_tip * 0.6, d2 = pivot_d, h = pivot_tip / 2 + 0.01, $fn = 24);
    }
}
module dpad_bosses() for (a = [0 : 90 : 270]) translate([cross_c[0], by(cross_c[1]), sw_top + boss_gap]) rotate(a) translate([boss_r, 0, 0]) cylinder(d = boss_d, h = 0.5, $fn = 32);
module lid_full(pads = true) difference() { lid_core(pads); pwr_marks(); }
// stage 2 (the user's silk list: "ON / OFF" at the power switch): engraved 0.4 mm into the roof by the right wall, at
// the toggle's height on the board: "ON" over "OFF" with arrows, lever up = ON (which throw is ON depends on the pins
// wired: README "Power switch", check it; the wall round the bushing is the nut's seat: no text there)
module pwr_marks() for (m = [["ON ^", 2.2], ["OFF v", -2.2]])
    translate([pwr_left ? 5.5 : W - 5.5, by(pwr[1] + pwr_dy) + m[1], zt - 0.4]) linear_extrude(1.0)
        text(m[0], size = 2.2, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
module lid() {    // as printed: roof down (turned over about x: a proper rotation, not a mirror)
    translate([0, 0, zt]) rotate([180, 0, 0]) lid_full();
}

// ---- the light pipe ----
module led_pipe_core() {
    translate([0, 0, led_top + 0.5]) cylinder(d = 3.0, h = zt - 0.8 - (led_top + 0.5) + 0.01, $fn = 24);
    translate([0, 0, zt - 0.8]) cylinder(d = 4.0, h = 0.8, $fn = 24);
}
module pipe_shaft_in_place() translate([led[0], by(led[1]), 0]) translate([0, 0, led_top + 0.5]) cylinder(d = 3.0, h = zt - 0.8 - (led_top + 0.5), $fn = 24);
module pipe_in_place() if (lid_var == "led") translate([led[0], by(led[1]), 0]) led_pipe_core();

// ---- print orientations of the small parts ----
module led_pipe_print() translate([0, 0, 0]) rotate([180, 0, 0]) translate([0, 0, -zt]) led_pipe_core();   // head down
module dpad_print() translate([0, 0, cap_top]) rotate([180, 0, 0]) translate([-cross_c[0], -by(cross_c[1]), 0]) dpad_core();   // face down

// ---- the checks (make_stl.sh): "empty" ones must render empty ----
clr_check = 0.3;   // the clearance the checks demand
module inflate(d) { if (d > 0) minkowski() { children(); sphere(r = d * 1.013, $fn = 20); } else children(); }

if (part == "base") base();
if (part == "lid") lid();
if (part == "led_pipe") led_pipe_print();
if (part == "dpad_cap") dpad_print();
module all_closed() {
    color("dimgray") base_core(); color("dimgray") base_pads(); for (b = bumps) color("dimgray") at_side(b[0], b[1]) bump();
    color("silver") lid_full();
    color("seagreen") board_all();
    color("orange") pipe_in_place();
    color("red") dpad_core();
    color("gold") pwr_switch();
}
if (part == "assembly") all_closed();
if (part == "open") {
    color("dimgray") base_core(); color("dimgray") base_pads(); for (b = bumps) color("dimgray") at_side(b[0], b[1]) bump();
    color("seagreen") board_all();
    translate([0, 0, 22]) { color("silver") lid_full(); color("orange") pipe_in_place(); color("red") dpad_core(); color("gold") pwr_switch(); }
}
// sections: the closed case cut across (at a board y), half shown
sec_y = part == "section_din" ? din_y : part == "section_led" ? led[1] : part == "section_cells" ? 50 : part == "section_screen" ? win_c[1] : part == "section_button" ? btn[1][1] : j2[1];
if (part == "section_din" || part == "section_jack" || part == "section_led" || part == "section_cells" || part == "section_screen" || part == "section_button")
    difference() { all_closed(); translate([-10, by(sec_y), -40]) cube([100, 100, 100]); }
// a cut across x (looking along the cells: both cells on edge, the rib, the floor), the half x > sec_x removed
sec_x = 30;
if (part == "section_x") difference() { all_closed(); translate([sec_x, -10, -40]) cube([100, 100, 100]); }
// the checks
module board_check() {
    inflate(clr_check) { mock(); jack_bodies_only(); tails(); ph_model(); if (lid_var == "screen") module_model(); if (din) both() din_body_pins(); }
    inflate(0.3) jack_noses_only();
    if (din) inflate(slot_play / 2) both() din_plate();
    if (cells) inflate(0.2) cell_model(0.3);   // (the cells stand on the floor: checked from 0.3 over it)
}
module din_body_pins() {
    translate([plate[1] + slot_play, by(din_y) - body[0] / 2, -body[2]]) cube([body[1] - slot_play, body[0], body[2]]);
    for (h = din_holes) translate([h[0] - 0.5, by(h[1]) - 0.5, 0]) cube([1, 1, 4.0]);
}
if (part == "check_base") intersection() { base_core(false); board_check(); }        // the base 0.3 clear of the board, every part, the DINs; 0.2 of the cells
if (part == "check_lid") intersection() { lid_full(false); board_check_lid(); }
module board_check_lid() inflate(clr_check) { mock(); jacks(); tails(); if (lid_var == "screen" && !hung) module_model(); }   // (s242: its posts rest on it by design)   // (the D-pad: own checks)
module jack_bodies_only() difference() { jacks(); translate([0, 0, -50]) jack_noses_cut(); }
module jack_noses_cut() {   // (the noses' room, removed from the bodies' check: the slots are checked with the noses)
    translate([-20, 0, 0]) cube([20 + 0.0, 200, 100]); translate([W, 0, 0]) cube([20, 200, 100]); translate([0, H, 0]) cube([200, 20, 100]);
}
// the D-pad at rest: its bosses 0.15 mm over the actuators (not touching at 0.1, within 0.2), its pivot on the board
// (within 0.05), 0.25 mm or more from the lid (the hole's 0.3 sides; the flange 1.0 under the roof), the toggle's body
// clear of the board's parts and of the D-pad
if (part == "check_dpad") intersection() { inflate(0.1) dpad_core(false); board_all(); }
if (part == "check_pivot_near") intersection() { inflate(0.05) translate([cross_c[0], by(cross_c[1]), bt]) cylinder(d = pivot_tip, h = 0.3, $fn = 16); mock(); }
if (part == "check_dpad_near") intersection() { inflate(0.2) dpad_bosses(); board_all(); }
if (part == "check_dpad_lid") intersection() { inflate(0.25) dpad_core(); lid_core(false); }
if (part == "check_pwr") intersection() { inflate(0.3) pwr_switch(); union() { board_all(); dpad_core(); } }
if (part == "check_pwr_lid") intersection() { pwr_switch(); lid_core(false); }
if (part == "check_posts_touch") intersection() { button_posts(); board_all(); }
if (part == "check_posts_near") intersection() { inflate(0.12) button_posts(); mock(); }
if (part == "check_pads_touch") intersection() { inflate(0.0) base_pads(); board_all(); }  // the base's pads must not touch the board...
if (part == "check_pads_near") intersection() { inflate(0.12) base_pads(); board_all(); } // ...and be within 0.12 of it (must NOT be empty)
if (part == "check_rib_touch") intersection() { cell_rib(); board_all(); }                    // the rib must not touch the board or the cells...
if (part == "check_rib_near") intersection() { inflate(0.12) cell_rib(); mock(); }             // ...and be within 0.12 of the board (must NOT be empty)
if (part == "check_pipe" && lid_var == "led") intersection() { inflate(clr_check) pipe_in_place(); board_all(); }
if (part == "check_pipe_lid" && lid_var == "led") intersection() { inflate(0.05) pipe_shaft_in_place(); lid_core(); }
if (part == "check_lid_base") intersection() { inflate(0.05) lid_full(); union() { base_core(); base_pads(); for (b = bumps) at_side(b[0], b[1]) bump(); } }
if (part == "check_lid_pads_touch") intersection() { inflate(0.0) lid_pads_only(); board_all(); }
if (part == "check_lid_pads_near") intersection() { inflate(0.12) lid_pads_only(); board_all(); }
module lid_pads_only() for (p = lid_pads) translate([p[0], p[1], bt + 0.1]) cylinder(d = 2.4, h = 2, $fn = 24);
