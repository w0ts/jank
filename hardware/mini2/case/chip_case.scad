// SPDX-License-Identifier: GPL-3.0-only
// jank chip: the printed case for the one-sided board (hardware/chip, chip.kicad_pcb, 44.0 x 51.5 mm).
//
// The board lies top up and every part is on its TOP side: the two jacks (MIDI OUT on the left edge, POWER IN on the
// right, both at the top), the USB-C (top edge), the two 10.5 mm reservoir cans, the button, the LED, the chip and the
// antenna (bottom edge, in the strip with no copper). So the LID is deep (the cans) and holds the jack holes, the USB-C
// opening, the button and the LED window; the BASE is a shallow tray (the jacks' plastic pegs come through the board).
// They meet at the board's top face with a rabbet; posts clamp the board (four under it at its corners, three over it
// along the antenna strip). With din = true the base is deep enough for the DIN sockets and has their openings. Close it with a
// drop of glue or tape: there is no screw (UNVERIFIED: the fit of the rabbet; print a test first). No metal near the
// antenna: keep it that way (no screw, no label foil over the strip).
//
// Frame: case outer corner at the origin, X along the board's x, Y up from the board's BOTTOM edge (the antenna end), Z
// up. Board point (x, y) (KiCad, y down from the top edge) is at (b0 + x, b0 + bh - y).
// Every dimension marked UNVERIFIED is from a drawing or an estimate, not a part in hand (README "Unverified").

/* [What to render] */
part = "assembly";   // [assembly, base, lid, button_cap, check_base, check_lid, check_cap]
din = false;         // true: the DIN sockets (J5 / J6, hand-soldered on the bottom) fitted: a deep base with their openings

/* [Board] */
bw = 44.0;  bh = 51.5;  bt = 1.6;         // the board (bt UNVERIFIED: JLC's default thickness)
// room under the board: the jacks' pegs (UNVERIFIED: ~1 mm through 1.6), or the DIN sockets' bodies
din_h = 20.5;  din_axis = 10.3;  din_hole = 16.0;  din_y = 20.0;   // DIN-504B-M10: body height and axis under the board
                                                                   // (UNVERIFIED: estimated from its 3D model), face opening
below = din ? din_h + 0.5 : 2.0;
above = 11.2;                              // room over the board: the cans' 10.5 (KNSCHA 8 x 10.5) + 0.7
/* [Case] */
wall = 1.6;  clr = 0.3;  floor_t = 1.4;  roof = 1.4;  rab = 2.0;  rab_t = 0.8;  fit = 0.15;
b0 = wall + clr;
L = bw + 2 * b0;  D = bh + 2 * b0;
zb = floor_t + below;  zt = zb + bt;  Ht = zt + above + roof;
// the parts the case meets (board mm, from make_board.py PLACE)
usb_x = 22.0;  usb_w = 9.6;  usb_h = 3.8;  usb_z = 1.63;      // USB-C opening; its axis 1.63 over the board (UNVERIFIED)
jack_y = 5.8;  jack_z = 2.5;  jack_hole = 6.0;                 // the jacks' axes (top side: 2.5 over the board), bushing 5.0
sw = [7.35, 18.0];  sw_top = 1.5;                               // SW1 (TS-1187A, 1.5 tall, XKB drawing)
led = [7.3, 23.4];  led_hole = 1.8;
// the posts that clamp the board: under it at the four corners (free on the bottom, also with the DINs); over it
// along the antenna strip only (the top is full: the jacks, the USB-C and the parts reach the other edges)
base_posts = [[1.6, 1.6], [bw - 1.6, 1.6], [1.6, bh - 1.6], [bw - 1.6, bh - 1.6]];
lid_posts = [[1.6, bh - 1.6], [bw / 2 - 6, bh - 1.6], [bw - 1.6, bh - 1.6]];
post_d = 2.4;

function P(p) = [b0 + p[0], b0 + bh - p[1]];
module rbox(x, y, z, r = 1.0) { hull() for (i = [r, x - r], j = [r, y - r]) translate([i, j, 0]) cylinder(r = r, h = z, $fn = 24); }

module board_mock() { translate([b0 - 100, b0 + bh + 100, zb]) import("../mockup.stl"); }   // (the mockup is in KiCad page mm, y negated)

module cuts() {
    // USB-C, top edge (Y = D side)
    translate([b0 + usb_x - usb_w / 2, D - wall - 1, zt + usb_z - usb_h / 2])
        hull() for (i = [usb_h / 2, usb_w - usb_h / 2]) translate([i, 0, usb_h / 2]) rotate([-90, 0, 0]) cylinder(d = usb_h, h = wall + 2, $fn = 32);
    // the jacks' holes: left wall (MIDI OUT) and right wall (POWER IN), over the board
    for (s = [0, 1])
        translate([s ? L - wall - 1 : -1, P([0, jack_y])[1], zt + jack_z]) rotate([0, 90, 0]) cylinder(d = jack_hole, h = wall + 2, $fn = 48);
    // the DIN sockets' faces: left wall (MIDI OUT, J5) and right wall (POWER IN, J6), under the board
    if (din) for (s = [0, 1])
        translate([s ? L - wall - 1 : -1, P([0, din_y])[1], zb - din_axis]) rotate([0, 90, 0]) cylinder(d = din_hole, h = wall + 2, $fn = 64);
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
            for (p = base_posts) translate([P(p)[0], P(p)[1], 0]) cylinder(d = post_d, h = zb - 0.05, $fn = 24);   // (0.05 short: a touch, not a shared face)
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
            for (p = lid_posts) translate([P(p)[0], P(p)[1], zt + 0.05]) cylinder(d = post_d, h = Ht - zt - 0.05, $fn = 24);
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
