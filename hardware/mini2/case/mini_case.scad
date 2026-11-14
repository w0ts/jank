// SPDX-License-Identifier: GPL-3.0-only
// jank mini 2.2: the printed case for the mini 2.2 board (mini2.kicad_pcb, 66 x 58 x 1.6 mm), with (din = true) or
// without (din = false) the two 5-pin DIN sockets J5 / J6 on the board's bottom, two lid variants (lid_var = "led": the
// LED window and its light pipe; "screen": a window over a 0.96" OLED module on J10, with a bezel pocket), and the
// cells under the board (cells = true: two 1S cells standing on edge in the band between the DIN bodies and the bottom
// wall, a divider rib between them).
//
// The board lies top up. Its TOP carries the tall parts (the three 3.5 mm jacks 5 mm, the USB-C, the screen module on
// its header, the five buttons); the LID is the deep part over it: 11 mm inside, kept from mini 2 for the upright FPC
// antenna (10 mm wide) on the lid's bottom wall. Its BOTTOM carries the DINs (optional), the PH2.0 sockets and the cells:
// the BASE is a deep box either way (the cells' height).
//
// The case splits 0.1 mm under the board's top face (z 1.5): the base holds the board (its edge, its underside, the
// divider rib), the lid is a cap over it; a half-lap joint (the lid's thin skirt outside, the base's thin lap inside)
// with six snap bumps closes it. No screw, no insert, no metal.
//
// Frame (every number): X = the board's x (0 at its left edge), Y = H - the board's y (0 at the bottom edge: J4's and the
// antenna's side, the USB-C edge at the top, Y = H), Z = 0 at the board's BOTTOM face (the board is z 0..1.6). Board
// point (x, y) = (x, by(y)). Every dimension marked UNVERIFIED is from a drawing or a 3D model, not a part in hand.

/* [What to render] */
part = "assembly";   // [assembly, open, base, lid, button_cap, led_pipe, check_*, section_*]
din = false;         // true: the DIN sockets J5 / J6 fitted: their slots in the base
lid_var = "led";     // "led": the LED window; "screen": the OLED's window
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
tail = 3.9;       // the longest tail under the board: the through-hole jacks' legs, 3.5 in their 3D model (UNVERIFIED), + margin
jack_legs = [[11.3, 8.1], [3.2, 3.5], [6.2, 3.5], [10.2, 3.5],               // J2's pads A, B, C, D (board x, y)
             [W - 11.3, 3.5], [W - 3.2, 8.1], [W - 6.2, 8.1], [W - 10.2, 8.1],   // J3's (turned 180)
             [14.1, 11.3], [18.7, 3.2], [18.7, 6.2], [18.7, 10.2]];              // J7's (LINE IN, on the top edge, turned -90)
j7_x = 16.4;   // LINE IN's jack on the top edge: its axis, board x
din_h = 19.6;  din_gap = 0.35;
/* [The cells: two 1S cells on edge under the board (the user measures them: set these)] */
cell_len = 66;    // along x (GNB 550 mAh 65..66, BT2.0 450 mAh 62); the case's inside width is W + 2 clr
cell_h = 18;      // standing on edge: its width is its height (GNB 18, BT2.0 15.5)
cell_t = 6.7;     // its thickness, along y (BT2.0 6.7, GNB 6..6.2)
cell_y0 = 41.0;   // the band's top, board y (the DIN bodies end at 39.0, the ribs of their plates' slots at 40.85; the PH2.0 sockets' bodies are above it, at y 30..36)
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
jack_y = 5.8;  jack_w = 6.9;  jack_top = bt + 5.8;                   // the jacks' front blocks 6.0 wide, 5.3 high, 2.2 past the edge
btn = [[51.5, 44], [61, 44], [51.5, 53.5], [61, 53.5]];   // UP, NO / BOOT, DOWN, YES (SW3, SW1, SW4, SW5)
led = [5.5, 16.1];        // the middle of the red and the green LED (one window)
hdr = [34, 14.8];         // J10: the screen's 1 x 4 header (the module's top edge 1.5 mm above its pin row)
j4 = [42.2, 55.4];        // the u.FL
ph = [[36.5, 33], [43, 33]];   // J8, J9: the PH2.0 sockets (bottom)
sw_top = bt + 1.5;        // the tactile switches' plunger top (TS-1187A, 1.5 tall: UNVERIFIED)
led_top = bt + 0.8;       // (the mockup's block; the parts are 0.6 / 0.8)
cap_d = 8.0;  cap_hole = 8.4;  cap_flange = 9.2;   // the buttons' caps (9.5 mm apart: the flanges 0.3 apart)
/* [The screen: a 0.96" 128 x 64 I2C OLED module on J10 (typical; UNVERIFIED per maker)] */
mod_w = 27.3;  mod_h = 27.8;  mod_z0 = bt + 2.5;  mod_z1 = bt + 2.5 + 1.2 + 2.0;   // its outline, its underside (the header's plastic), its glass top
win = [24.5, 13.0];  win_dy = 12.0;      // the window over the display, its centre this far below the module's top edge
bezel = [28.5, 17.0, 0.6];               // the bezel pocket on the roof's outer face (a clear cover sheet, 0.5 mm)
/* [The FPC antenna, upright on the inside of the bottom wall (Y = 0), its lead from J4] */
ant_l = 42;  ant_w = 10.5;  ant_d = 0.4;   // the pocket: for a ~40 x 10 mm FPC antenna (ASSUMED size: C1693673's sheet
ant_x0 = 3.0;                              //  could not be read; LCSC's FPCs with a 50 mm lead are about 40 x 10)
base_pads = [[W / 2 - 17, W / 2 - 9, H - 1.0, H + 0.2], [W / 2 + 9, W / 2 + 17, H - 1.0, H + 0.2],     // the USB edge
             [3, 9, -0.2, 1.0], [W - 9, W - 3, -0.2, 1.0],        // the bottom edge (not with the cells)
             [-0.2, 0.95, H - 6.5, H - 1.5], [-0.2, 0.95, 6, 12]];    // the left edge (mirrored to the right)
lid_pads = [[2, 2], [2, 6.5], [W - 1.5, by(37.2)], [W - 2, by(15)]];   // the lid's feet on the board's top, clear of every part
bumps = [[0, 26], [1, 26], [2, W / 2 - 11], [2, W / 2 + 11], [3, W / 2 - 11], [3, W / 2 + 11]];   // [side 0 left 1 right 2 top 3 bottom, along the wall]

module rbox(x0, y0, x1, y1, z0, z1, r) {
    hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y, z0]) cylinder(r = r, h = z1 - z0, $fn = 40);
}
module cav(inset, z0, z1) rbox(-e + inset, -e + inset, W + e - inset, H + e - inset, z0, z1, max(0.4, r_out - inset));
module both() { children(); translate([W, 0, 0]) mirror([1, 0, 0]) children(); }

// ---- the board and what hangs under / stands past it (for the checks and the renders) ----
module mock() { translate([-100, 100 + H, 0]) import("../mockup.stl"); }
module jack_fronts() {
    both() translate([-2.25, by(jack_y) - 3.0, bt - 0.2]) cube([2.25, 6.0, 5.5]);
    translate([j7_x - 3.0, H, bt - 0.2]) cube([6.0, 2.25, 5.5]);                 // J7, out past the top edge
}
module tails() {       // under the board: the jacks' legs (3.5) and pegs, the USB-C's legs; tail deep (assumed)
    for (x = [1.6, 8.6]) both() translate([x - 0.5, by(jack_y) - 0.5, -tail]) cube([1, 1, tail + 0.1]);
    for (y = [1.6, 8.6]) translate([j7_x - 0.5, by(y) - 0.5, -tail]) cube([1, 1, tail + 0.1]);   // J7's pegs
    for (p = jack_legs) translate([p[0] - 1.1, by(p[1]) - 0.6, -tail]) cube([2.2, 1.2, tail + 0.1]);
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
module module_model() translate([hdr[0] - mod_w / 2, by(hdr[1] - 1.5 + mod_h), mod_z0]) cube([mod_w, mod_h, mod_z1 - mod_z0]);
module board_all() {
    mock(); jack_fronts(); tails(); ph_model();
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
    }
    usb_cut(usb_bot);
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
win_c = [hdr[0], hdr[1] - 1.5 + win_dy];          // the screen window's centre, board (x, y)
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
    }
    // the jacks' slots: open downwards, in the skirt and the wall (the board drops in from below)
    both() translate([-e - 1, by(jack_y) - jack_w / 2, -1]) hull() {
        cube([e + 1.2, jack_w, 1]);
        for (y = [0.6, jack_w - 0.6]) translate([0, y, jack_top + 1 - 0.6]) rotate([0, 90, 0]) cylinder(r = 0.6, h = e + 1.2, $fn = 20);
    }
    usb_cut(usb_bot);
    translate([j7_x - jack_w / 2, H - 0.2, -1]) hull() {                 // LINE IN's slot in the top wall at J7
        cube([jack_w, e + 1.2, 1]);
        for (x = [0.6, jack_w - 0.6]) translate([x, 0, jack_top + 1 - 0.6]) rotate([-90, 0, 0]) cylinder(r = 0.6, h = e + 1.2, $fn = 20);
    }
    for (p = btn) translate([p[0], by(p[1]), z_in - 1]) cylinder(d = cap_hole, h = roof + 2, $fn = 48);   // the four caps' holes
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
module lid() {    // as printed: roof down (turned over about x: a proper rotation, not a mirror)
    translate([0, 0, zt]) rotate([180, 0, 0]) lid_core();
}

// ---- the caps and the light pipe ----
module button_cap_core() {   // z 0 = the board's bottom face: the stem from 0.4 over the plunger, the flange under the roof, the head
    translate([0, 0, sw_top + 0.4]) cylinder(d = 3.4, h = z_in - 1.0 - (sw_top + 0.4) + 0.01, $fn = 32);
    translate([0, 0, z_in - 1.0]) cylinder(d = cap_flange, h = 0.8, $fn = 48);
    translate([0, 0, z_in - 0.2 - 0.01]) cylinder(d = cap_d, h = zt + 1.0 - (z_in - 0.2), $fn = 48);
}
module button_cap() { button_cap_core(); }
module led_pipe_core() {
    translate([0, 0, led_top + 0.5]) cylinder(d = 3.0, h = zt - 0.8 - (led_top + 0.5) + 0.01, $fn = 24);
    translate([0, 0, zt - 0.8]) cylinder(d = 4.0, h = 0.8, $fn = 24);
}
module cap_in_place() for (p = btn) translate([p[0], by(p[1]), 0]) button_cap_core();   // (the four caps: print four)
module pipe_shaft_in_place() translate([led[0], by(led[1]), 0]) translate([0, 0, led_top + 0.5]) cylinder(d = 3.0, h = zt - 0.8 - (led_top + 0.5), $fn = 24);
module pipe_in_place() if (lid_var == "led") translate([led[0], by(led[1]), 0]) led_pipe_core();

// ---- print orientations of the small parts ----
module button_cap_print() translate([0, 0, zt + 1.0]) rotate([180, 0, 0]) button_cap_core();   // head down
module led_pipe_print() translate([0, 0, 0]) rotate([180, 0, 0]) translate([0, 0, -zt]) led_pipe_core();   // head down

// ---- the checks (make_stl.sh): "empty" ones must render empty ----
clr_check = 0.3;   // the clearance the checks demand
module inflate(d) { if (d > 0) minkowski() { children(); sphere(r = d * 1.013, $fn = 20); } else children(); }

if (part == "base") base();
if (part == "lid") lid();
if (part == "button_cap") button_cap_print();
if (part == "led_pipe") led_pipe_print();
module all_closed() {
    color("dimgray") base_core(); color("dimgray") base_pads(); for (b = bumps) color("dimgray") at_side(b[0], b[1]) bump();
    color("silver") lid_core();
    color("seagreen") board_all();
    color("red") cap_in_place(); color("orange") pipe_in_place();
}
if (part == "assembly") all_closed();
if (part == "open") {
    color("dimgray") base_core(); color("dimgray") base_pads(); for (b = bumps) color("dimgray") at_side(b[0], b[1]) bump();
    color("seagreen") board_all();
    translate([0, 0, 22]) { color("silver") lid_core(); color("red") cap_in_place(); color("orange") pipe_in_place(); }
}
// sections: the closed case cut across (at a board y), half shown
sec_y = part == "section_din" ? din_y : part == "section_led" ? led[1] : part == "section_cells" ? 50 : part == "section_screen" ? win_c[1] : jack_y;
if (part == "section_din" || part == "section_jack" || part == "section_led" || part == "section_cells" || part == "section_screen")
    difference() { all_closed(); translate([-10, by(sec_y), -40]) cube([100, 100, 100]); }
// a cut across x (looking along the cells: both cells on edge, the rib, the floor), the half x > sec_x removed
sec_x = 30;
if (part == "section_x") difference() { all_closed(); translate([sec_x, -10, -40]) cube([100, 100, 100]); }
// the checks
module board_check() {
    inflate(clr_check) { mock(); jack_fronts(); tails(); ph_model(); if (lid_var == "screen") module_model(); if (din) both() din_body_pins(); }
    if (din) inflate(slot_play / 2) both() din_plate();
    if (cells) inflate(0.2) cell_model(0.3);   // (the cells stand on the floor: checked from 0.3 over it)
}
module din_body_pins() {
    translate([plate[1] + slot_play, by(din_y) - body[0] / 2, -body[2]]) cube([body[1] - slot_play, body[0], body[2]]);
    for (h = din_holes) translate([h[0] - 0.5, by(h[1]) - 0.5, 0]) cube([1, 1, 4.0]);
}
if (part == "check_base") intersection() { base_core(false); board_check(); }        // the base 0.3 clear of the board, every part, the DINs; 0.2 of the cells
if (part == "check_lid") intersection() { lid_core(false); board_check(); }
if (part == "check_pads_touch") intersection() { inflate(0.0) base_pads(); board_all(); }  // the base's pads must not touch the board...
if (part == "check_pads_near") intersection() { inflate(0.12) base_pads(); board_all(); } // ...and be within 0.12 of it (must NOT be empty)
if (part == "check_rib_touch") intersection() { cell_rib(); board_all(); }                    // the rib must not touch the board or the cells...
if (part == "check_rib_near") intersection() { inflate(0.12) cell_rib(); mock(); }             // ...and be within 0.12 of the board (must NOT be empty)
if (part == "check_cap") intersection() { inflate(clr_check) cap_in_place(); board_all(); }
if (part == "check_cap_lid") intersection() { inflate(0.1) cap_in_place(); lid_core(); }
if (part == "check_pipe" && lid_var == "led") intersection() { inflate(clr_check) pipe_in_place(); board_all(); }
if (part == "check_pipe_lid" && lid_var == "led") intersection() { inflate(0.05) pipe_shaft_in_place(); lid_core(); }
if (part == "check_lid_base") intersection() { inflate(0.05) lid_core(); union() { base_core(); base_pads(); for (b = bumps) at_side(b[0], b[1]) bump(); } }
if (part == "check_lid_pads_touch") intersection() { inflate(0.0) lid_pads_only(); board_all(); }
if (part == "check_lid_pads_near") intersection() { inflate(0.12) lid_pads_only(); board_all(); }
module lid_pads_only() for (p = lid_pads) translate([p[0], p[1], bt + 0.1]) cylinder(d = 2.4, h = 2, $fn = 24);
