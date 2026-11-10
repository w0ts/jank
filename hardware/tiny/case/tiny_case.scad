// SPDX-License-Identifier: GPL-3.0-only
// jank tiny: the printed case for the two-sided board (hardware/tiny, tiny.kicad_pcb, 27.0 x 36.0 mm).
//
// The board lies top up. Its TOP side carries the module, the USB-C (top edge) and the button; its BOTTOM side the two
// jacks (MIDI OUT on the left edge, POWER IN on the right, staggered) and the two 10.5 mm reservoir cans. So the BASE
// is deep (the cans) and holds the jack holes; the LID is shallow and holds the USB-C opening, the button and the LED
// window. Base and lid meet at the board's top face with a rabbet; three posts in each clamp the board at its free
// corners (top right, bottom left, bottom right: no parts there on either side). Close it with a drop of glue or a
// strip of tape: there is no screw (UNVERIFIED: the fit of the rabbet; print a test first).
//
// Frame: case outer corner at the origin, X along the board's x, Y up from the board's BOTTOM edge (the antenna end), Z
// up. Board point (x, y) (KiCad, y down from the top edge) is at (b0 + x, b0 + bh - y).
// Every dimension marked UNVERIFIED is from a drawing or an estimate, not a part in hand (README "Unverified").

/* [What to render] */
part = "assembly";   // [assembly, base, lid, button_cap, check_base, check_lid, check_cap]

/* [Board] */
bw = 27.0;  bh = 36.0;  bt = 1.6;         // the board (bt UNVERIFIED: JLC's default thickness)
below = 11.0;                              // room under the board: the cans' 10.5 (ACMECON drawing) + 0.5
above = 4.0;                               // room over the board: the USB-C's ~3.3 (UNVERIFIED) + 0.7
/* [Case] */
wall = 1.6;  clr = 0.3;  floor_t = 1.4;  roof = 1.4;  rab = 2.0;  rab_t = 0.8;  fit = 0.15;
b0 = wall + clr;
L = bw + 2 * b0;  D = bh + 2 * b0;
zb = floor_t + below;  zt = zb + bt;  Ht = zt + above + roof;
// the parts the case meets (board mm, from make_board.py PLACE_2S)
usb_x = 18.0;  usb_w = 9.6;  usb_h = 3.8;  usb_z = 1.63;      // USB-C opening; its axis 1.63 over the board (UNVERIFIED)
jack_l_y = 4.9;  jack_r_y = 12.6;  jack_z = 2.5;  jack_hole = 6.0; // the jacks' axes (bottom side: 2.5 under the board), bushing 5.0
sw = [2.75, 19.5];  sw_top = 1.5;                               // SW1 (TS-1187A, 1.5 tall, XKB drawing)
led = [2.6, 25.1];  led_hole = 1.8;
posts = [[25.4, 1.6], [1.6, 34.4], [25.4, 34.4]];                // board corners free on both sides
post_d = 2.6;

function P(p) = [b0 + p[0], b0 + bh - p[1]];
module rbox(x, y, z, r = 1.0) { hull() for (i = [r, x - r], j = [r, y - r]) translate([i, j, 0]) cylinder(r = r, h = z, $fn = 24); }

module board_mock() { translate([b0 - 100, b0 + bh + 100, zb]) import("../mockup.stl"); }   // (the mockup is in KiCad page mm, y negated)

module cuts() {
    // USB-C, top edge (Y = D side), through the lid and the base's rabbet
    translate([b0 + usb_x - usb_w / 2, D - wall - 1, zt + usb_z - usb_h / 2])
        hull() for (i = [usb_h / 2, usb_w - usb_h / 2]) translate([i, 0, usb_h / 2]) rotate([-90, 0, 0]) cylinder(d = usb_h, h = wall + 2, $fn = 32);
    // the jacks' holes: left wall (MIDI OUT) and right wall (POWER IN), under the board
    for (s = [[0, jack_l_y], [1, jack_r_y]])
        translate([s[0] ? L - wall - 1 : -1, P([0, s[1]])[1], zb - jack_z]) rotate([0, 90, 0]) cylinder(d = jack_hole, h = wall + 2, $fn = 48);
}

module base() {
    difference() {
        union() {
            difference() {
                rbox(L, D, zt);
                translate([wall, wall, floor_t]) cube([L - 2 * wall, D - 2 * wall, zt]);
                // the rabbet: the base's wall thinner on the outside over its top rab mm
                translate([0, 0, zt - rab]) difference() { cube([L, D, rab + 1]); translate([rab_t, rab_t, -1]) cube([L - 2 * rab_t, D - 2 * rab_t, rab + 3]); }
            }
            for (p = posts) translate([P(p)[0], P(p)[1], 0]) cylinder(d = post_d, h = zb, $fn = 24);
        }
        cuts();
    }
}

module lid() {
    difference() {
        union() {
            difference() {
                translate([0, 0, zt - rab]) rbox(L, D, Ht - zt + rab);
                translate([wall, wall, zt - rab - 1]) cube([L - 2 * wall, D - 2 * wall, Ht - zt + rab + 1 - roof]);
                // the rabbet: room for the base's wall
                translate([rab_t - fit, rab_t - fit, zt - rab - 1]) cube([L - 2 * rab_t + 2 * fit, D - 2 * rab_t + 2 * fit, rab + 1]);
            }
            for (p = posts) translate([P(p)[0], P(p)[1], zt]) cylinder(d = post_d, h = Ht - zt, $fn = 24);
        }
        cuts();
        translate([P(sw)[0], P(sw)[1], Ht - roof - 1]) cylinder(d = 3.4, h = roof + 2, $fn = 32);       // the button cap's stem
        translate([P(led)[0], P(led)[1], Ht - roof - 1]) cylinder(d = led_hole, h = roof + 2, $fn = 24); // the LED window
    }
}

// the button cap: a stem through the roof, a flange under it, resting 0.2 over the switch
module button_cap() {
    stem = Ht - (zt + sw_top + 0.2) + 1.0;
    cylinder(d = 3.0, h = stem, $fn = 32);
    cylinder(d = 5.0, h = 0.8, $fn = 32);
}
module cap_in_place() { translate([P(sw)[0], P(sw)[1], zt + sw_top + 0.2]) button_cap(); }

if (part == "base") base();
if (part == "lid") lid();
if (part == "button_cap") button_cap();
if (part == "assembly") { color("gray") base(); color("silver", 0.4) lid(); color("green") board_mock(); color("red") cap_in_place(); }
// the checks: each must render EMPTY (make_stl.sh)
if (part == "check_base") intersection() { base(); board_mock(); }
if (part == "check_lid") intersection() { lid(); board_mock(); }
if (part == "check_cap") intersection() { cap_in_place(); union() { lid(); board_mock(); } }
