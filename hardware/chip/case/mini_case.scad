// SPDX-License-Identifier: GPL-3.0-only
// jank mini: the printed case for the jank chip board (hardware/chip, chip.kicad_pcb, 44.0 x 51.5 x 1.6 mm), with
// (din = true) or without (din = false) the two 5-pin DIN sockets J5 / J6 on the board's bottom.
//
// The board lies top up. Its TOP carries everything tall (the two reservoir cans 10.5 mm, the USB-C, the two 3.5 mm
// jacks, the button, the LED), so the LID is the deep part. Its BOTTOM carries only the optional DINs: the no-DIN BASE
// is a shallow tray, the DIN base a deep box with the sockets' plates in slots (after hardware/box on the `hardware`
// branch: DIN plates slide into slots in the wall, front cheek and back cheek, no wall behind the body).
//
// The case splits 0.1 mm under the board's top face (z 1.5): the base holds the board (its edge, its underside), the
// lid is a cap over it; a half-lap joint (the lid's thin skirt outside, the base's thin lap inside) with six snap
// bumps closes it. The board's top is 0.1 mm clear of the lid's four pads and its bottom 0.05 mm clear of the base's
// eight pads: it is held between them, with 0.2 mm of side play. No screw, no insert, no metal: the antenna (bottom
// edge) needs 10 mm of air round it.
//
// Frame (every number): X = the board's x (0 at its left edge), Y = H - the board's y (0 at the antenna edge, the USB-C
// edge at the top, Y = H), Z = 0 at the board's BOTTOM face (the board is z 0..1.6). Board point (x, y) = (x, by(y)).
// Every dimension marked UNVERIFIED is from a drawing or a 3D model, not a part in hand (README "Untested").

/* [What to render] */
part = "assembly";   // [assembly, open, base, lid, button_cap, led_pipe, check_*]
din = false;         // true: the DIN sockets J5 / J6 fitted: the deep base with their slots

/* [Board] */
W = 44.0;  H = 51.5;  bt = 1.6;
function by(y) = H - y;
/* [Case] */
e = 1.8;          // the outer face from the board's edge
clr = 0.4;        // the board's side play (each side)
wall = e - clr;   // 1.5
sk = 0.7;         // the lid's skirt (the lap's outer half)
lap_play = 0.15;
r_out = 1.8;          // (the cavity's corner 0.4: the board's square corner clears it)
zs = 1.0;         // the seam: the base's lap top (the lid's shoulder); the jacks' front blocks start at z 1.4
zlid = 0.1;       // the lid's skirt bottom (the base's shoulder is z 0)
z_in = bt + 10.5 + 0.5;   // the lid's inside roof: the cans (KNSCHA 8 x 10.5) + 0.5
roof = 1.4;
zt = z_in + roof;         // 14.0
floor_t = 1.4;
/* [Under the board] */
tail = 1.5;       // the longest tail under the board (USB-C legs 0.9, jack pegs 0.55 in their 3D models: UNVERIFIED) + margin
din_h = 19.6;  din_gap = 0.35;
under = din ? din_h + din_gap : tail + 0.4;   // the floor's top under the board
zbot = -under - floor_t;
/* [DIN: HOOYA DIN-504B-M10, drawing rev A1 + lib/3d model] */
din_y = 20.0;                            // the axis, board y
plate = [20.9, 1.8, 19.6];               // along the edge, thick, high (z down from the board)
body = [17.9, 15.3 - 1.8, 17.6];         // behind the plate: wide, deep, high (UNVERIFIED: the 3D model, +-0.2)
play = 0.35;                             // plate / body to the cheeks and the slot
rim = [1.3, 2.2];                        // what the cheeks hold of the plate: each side, the far edge
back_x = 2.4;  back_t = 2.0;             // the back cheek: its front face (clear of the jacks' pegs at x 2.05), its thickness
din_holes = [[12.25, 27.5], [14.75, 25], [12.25, 20], [14.75, 15], [12.25, 12.5], [2.45, 25], [2.45, 15]];   // J5 (board x, y)
/* [Parts the case meets] */
usb_x = 22.0;  usb_w = 13.0;  usb_top = bt + 5.2;  usb_bot = -0.2;   // USB-C: its mouth axis 1.6 over the board; a plug's overmold ~ 12 x 6.5
jack_y = 5.8;  jack_w = 6.9;  jack_top = bt + 5.8;                   // the jacks' front blocks 6.0 wide, 5.3 high, 2.2 past the edge
sw = [11.8, 37.8];  led = [11.2, 31.0];
sw_top = bt + 1.5;                                                   // the tactile switch's plunger top (TS-1187A, 1.5 tall: UNVERIFIED)
led_top = bt + 0.8;      // (the mockup's block; the part is 0.6)
base_pads = [[5, 13, 50.5, 51.7], [31, 39, 50.5, 51.7],     // the USB edge
             [3, 9, -0.2, 1.0], [35, 41, -0.2, 1.0],        // the antenna edge: plastic only, 17 mm from the antenna
             [-0.2, 0.95, 45, 50], [-0.2, 0.95, 6, 12]];    // the left edge (mirrored to the right)
lid_pads = [[2, 2], [42, 2], [2, 6.5], [42, 6.5]];          // the lid's feet on the board's top, clear of every part
bumps = [[0, 26], [1, 26], [2, 11], [2, 33], [3, 11], [3, 33]];   // [side 0 left 1 right 2 top 3 bottom, along the wall]

module rbox(x0, y0, x1, y1, z0, z1, r) {
    hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r]) translate([x, y, z0]) cylinder(r = r, h = z1 - z0, $fn = 40);
}
module cav(inset, z0, z1) rbox(-e + inset, -e + inset, W + e - inset, H + e - inset, z0, z1, max(0.4, r_out - inset));
module both() { children(); translate([W, 0, 0]) mirror([1, 0, 0]) children(); }

// ---- the board and what hangs under / stands past it (for the checks and the renders) ----
module mock() { translate([-100, 100 + H, 0]) import("../mockup.stl"); }
module jack_fronts() both() translate([-2.25, by(jack_y) - 3.0, bt - 0.2]) cube([2.25, 6.0, 5.5]);
module tails() {       // under the board: the jack pegs (x 1.55, 8.55) and the USB-C's legs; tail deep (assumed)
    for (x = [1.55, 8.55]) both() translate([x - 0.5, by(jack_y) - 0.5, -tail]) cube([1, 1, tail + 0.1]);
    for (p = [[26.32, 6.8], [26.32, 2.59], [17.68, 6.8], [17.68, 2.59]]) translate([p[0] - 0.5, by(p[1]) - 0.5, -tail]) cube([1, 1, tail + 0.1]);
}
module din_model() {   // one socket, J5 (left); face on the edge, body under the board, pins up through it
    translate([-0.01, by(din_y) - plate[0] / 2, -plate[2]]) cube([plate[1], plate[0], plate[2]]);
    translate([plate[1] - 0.02, by(din_y) - body[0] / 2, -body[2]]) cube([body[1] + 0.02, body[0], body[2]]);
    for (h = din_holes) translate([h[0] - 0.5, by(h[1]) - 0.5, 0]) cube([1, 1, 4.0]);
}
module board_all() { mock(); jack_fronts(); tails(); if (din) both() din_model(); }

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
    for (s = [-1, 1]) translate([-clr - 0.01, ya + (s > 0 ? plate[0] / 2 + play : -plate[0] / 2 - play - 2.0), -under - 0.01])
        cube([back_x + clr + 0.01, 2.0, under - 0.35 + 0.01]);
    // the back cheek: a partition across the case, with the body's opening (open at the top)
    difference() {
        translate([back_x, -clr - 0.01, -under - 0.01]) cube([back_t, H + 2 * clr + 0.02, under - 0.35 + 0.01]);
        translate([back_x - 1, ya - body[0] / 2 - play, -body[2] - play]) cube([back_t + 2, body[0] + 2 * play, body[2] + play + 1]);
    }
}
module base_core() difference() {
    union() {
        difference() {
            cav(0, zbot, zs - 0.1);
            cav(wall, zbot + floor_t, zs + 1);                               // the cavity
            difference() { cav(0, 0, zs + 1); cav(sk + lap_play, -1, zs + 2); }   // the lap: the outer part off over z 0..
        }
        if (din) din_walls();
    }
    usb_cut(usb_bot);
    if (din) both() translate([-e - 1, by(din_y) - plate[0] / 2 + rim[0], -plate[2] + rim[1]]) cube([e + 1 - 0.2 + 0.001, plate[0] - 2 * rim[0], plate[2] - rim[1] + 0.001]);   // the front window, z -17.4..0
}
module base_pads() {
    for (p = base_pads) both() translate([p[0], p[2], -under - 0.01]) cube([p[1] - p[0], p[3] - p[2], under - 0.04]);
}
module base() {   // as printed: floor down
    translate([0, 0, -zbot]) union() {
        base_core();
        base_pads();
        for (b = bumps) at_side(b[0], b[1]) bump();
    }
}

// ---- the lid ----
module lid_core(pads = true) difference() {
    union() {
        difference() {
            cav(0, zlid, zt);
            cav(sk, zlid - 1, zs);                                           // the skirt's room over the base's lap
            cav(wall, zs - 0.01, z_in);                                      // the inside
        }
        if (pads) for (p = lid_pads) translate([p[0], p[1], bt + 0.1 - 0.0]) cylinder(d = 2.4, h = z_in - bt - 0.1 + 0.01, $fn = 24);
        translate([led[0], by(led[1]), 3.3]) cylinder(d = 3.4, h = z_in - 3.3 + 0.01, $fn = 32);   // the LED's chimney
    }
    // the jacks' slots: open downwards, in the skirt and the wall (the board drops in from below)
    both() translate([-e - 1, by(jack_y) - jack_w / 2, -1]) hull() {   // (a rounded-top slot, open at the lid's bottom edge)
        cube([e + 1.2, jack_w, 1]);
        for (y = [0.6, jack_w - 0.6]) translate([0, y, jack_top + 1 - 0.6]) rotate([0, 90, 0]) cylinder(r = 0.6, h = e + 1.2, $fn = 20);
    }
    usb_cut(usb_bot);
    translate([sw[0], by(sw[1]), z_in - 1]) cylinder(d = 3.8, h = roof + 2, $fn = 32);            // the button cap's hole
    translate([led[0], by(led[1]), led_top - 1]) cylinder(d = 1.95, h = zt - led_top + 2, $fn = 24);      // the light pipe's bore
    translate([led[0], by(led[1]), zt - 0.8]) cylinder(d = 3.2, h = 1.0, $fn = 32);               // its head's recess
    for (b = bumps) at_side(b[0], b[1]) bump(0.15);                                                // the snap pockets
}
module lid() {    // as printed: roof down
    translate([0, 0, zt]) rotate([180, 0, 0]) translate([W, 0, 0]) mirror([1, 0, 0]) lid_core();
}

// ---- the caps ----
module button_cap_core() {
    translate([0, 0, sw_top + 0.4]) cylinder(d = 3.4, h = zt + 1.0 - (sw_top + 0.4), $fn = 32);       // the stem
    translate([0, 0, z_in - 1.0]) cylinder(d = 4.6, h = 0.8, $fn = 32);                              // the flange, 0.2 under the roof
}
module button_cap() { button_cap_core(); }   // (z 0 = the board's bottom face, the stem up: print it standing on the flange? see README)
module led_pipe_core() {
    translate([0, 0, led_top + 0.5]) cylinder(d = 1.8, h = zt - 0.8 - (led_top + 0.5) + 0.01, $fn = 24);
    translate([0, 0, zt - 0.8]) cylinder(d = 3.0, h = 0.8, $fn = 24);
}

module cap_in_place() translate([sw[0], by(sw[1]), 0]) button_cap_core();
module pipe_shaft_in_place() translate([led[0], by(led[1]), 0]) translate([0, 0, led_top + 0.5]) cylinder(d = 1.8, h = zt - 0.8 - (led_top + 0.5), $fn = 24);
module pipe_in_place() translate([led[0], by(led[1]), 0]) led_pipe_core();
module lid_placed() translate([0, 0, 0]) lid_core();

// ---- print orientations of the small parts ----
module button_cap_print() translate([0, 0, 0.8]) rotate([180, 0, 0]) translate([0, 0, -(z_in - 1.0)]) button_cap_core();   // flange down
module led_pipe_print() translate([0, 0, 0]) rotate([180, 0, 0]) translate([0, 0, -zt]) led_pipe_core();                   // head down

// ---- the checks (make_stl.sh): "empty" ones must render empty ----
clr_check = 0.3;   // the clearance the checks demand (-D clr_check=0.5 finds the tightest spot)
module inflate(d) { if (d > 0) minkowski() { children(); sphere(r = d * 1.013, $fn = 20); } else children(); }   // (a ball: a cube's corners reach 1.7 x as far)
module base_solid() { base_core(); }

if (part == "base") base();
if (part == "lid") lid();
if (part == "button_cap") button_cap_print();
if (part == "led_pipe") led_pipe_print();
if (part == "assembly") {
    color("dimgray") base_core(); color("dimgray") base_pads(); for (b = bumps) color("dimgray") at_side(b[0], b[1]) bump();
    color("silver") lid_core();
    color("seagreen") board_all();
    color("red") cap_in_place(); color("orange") pipe_in_place();
}
if (part == "open") {
    color("dimgray") base_core(); color("dimgray") base_pads(); for (b = bumps) color("dimgray") at_side(b[0], b[1]) bump();
    color("seagreen") board_all();
    translate([0, 0, 22]) { color("silver") lid_core(); color("red") cap_in_place(); color("orange") pipe_in_place(); }
}
// sections: the closed case cut at the DIN's axis (y = din_y) and at the jacks' axis, half shown
if (part == "section_din" || part == "section_jack") {
    cy = part == "section_din" ? by(din_y) : by(jack_y);
    difference() {
        union() {
            color("dimgray") base_core(); color("dimgray") base_pads(); color("silver") lid_core(); color("seagreen") board_all();
            color("red") cap_in_place(); color("orange") pipe_in_place();
        }
        translate([-10, cy, -40]) cube([100, 100, 100]);
    }
}
// the checks
if (part == "check_base") intersection() { base_core(); inflate(clr_check) board_all(); }        // the base (no pads) 0.3 clear of the board, every part, the DINs
if (part == "check_lid") intersection() { lid_core(false); inflate(clr_check) board_all(); }          // (the lid's pads are in lid_core: see check_lid_pads)
if (part == "check_pads_touch") intersection() { inflate(0.0) base_pads(); board_all(); }  // the base's pads must not touch the board...
if (part == "check_pads_near") intersection() { inflate(0.12) base_pads(); board_all(); } // ...and be within 0.12 of it (must NOT be empty)
if (part == "check_cap") intersection() { inflate(clr_check) cap_in_place(); board_all(); }
if (part == "check_cap_lid") intersection() { inflate(0.1) cap_in_place(); lid_core(); }
if (part == "check_pipe") intersection() { inflate(clr_check) pipe_in_place(); board_all(); }
if (part == "check_pipe_lid") intersection() { inflate(0.05) pipe_shaft_in_place(); lid_core(); }
if (part == "check_lid_base") intersection() { inflate(0.05) lid_core(); union() { base_core(); base_pads(); for (b = bumps) at_side(b[0], b[1]) bump(); } }
if (part == "check_lid_pads_touch") intersection() { inflate(0.0) lid_pads_only(); board_all(); }
if (part == "check_lid_pads_near") intersection() { inflate(0.12) lid_pads_only(); board_all(); }
module lid_pads_only() for (p = lid_pads) translate([p[0], p[1], bt + 0.1]) cylinder(d = 2.4, h = 2, $fn = 24);
