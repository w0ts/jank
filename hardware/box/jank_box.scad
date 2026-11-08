// SPDX-License-Identifier: GPL-3.0-only
// jank box, v5: the printed box of the jank board v5 (84.0 x 67.0 mm; hardware/pcb/README.md).
//
// The board's TOP (the lid side) carries the XIAO, the OLED, OK, BACK, the encoder, the LED, the four jacks (XKB
// PJ-320A) and the four A/B switches; its BOTTOM carries the four DINs (HOOYA DIN-504B-M10) hanging 19.6 mm down, so
// the board stands about 20 mm above the floor on them. Every output has both: the jack above the board line, the DIN
// below it (OUT2..OUT4 the front wall, OUT1 the back wall).
//
// FIRST REQUIREMENT (the user's ruling 2026-11-08): plugging and unplugging loads the box, not the solder joints.
//   DIN: pull-out -> the plate's front on the DIN pad (the wall made 1.0 mm thicker under the board) round a closed
//        16.0 mm bore; push-in -> the body's back on a lip from the floor; down -> the plate's far edge on the floor.
//   jack: pull-out -> the body's front on the wall's inside (the bushing through a U-slot closed by the lid's key);
//        push-in -> the body's back on the lid's cap (a saddle hanging from the lid); up -> the lid's cap.
//   The board's connector edges are clamped between the lid's ribs and the DIN pads' tops; four M2 standoffs.
//
// Frame: box outer corner at the origin, X along the front (left to right seen from the front, lid up), Y from the
// front to the back, Z up. KiCad x maps to X directly (the board lies top up); KiCad y is measured from the BACK edge.
// Every number marked UNVERIFIED is from a drawing or an estimate, not from a part in hand (README "Unverified").

/* [What to render] */
part = "assembly";      // [assembly, box, lid, front_test, mock, open, front, back, lid_top, check_box, check_lid, check_lidbox, check_drop, check_board_drop, check_text, brace_pull, brace_push, brace_trs, brace_floor, brace_lid, brace_ledge]

/* [Board v5: hardware/pcb/README.md "The layout (v5)", "For the box"] */
board_l = 84.0;
board_w = 67.0;
board_t = 1.6;          // UNVERIFIED: JLCPCB default
ch_x = [10.75, 10.45, 31.55, 52.65];     // OUT1..OUT4 axes, KiCad x
ch_back = [true, false, false, false];   // OUT1 on the back edge
holes = [[80.8, 3.2], [80.8, 63.8], [3.2, 26.0], [3.2, 34.0]];   // H1..H4 (2.2 mm), KiCad x, y from the back
buttons = [[69.15, 25.45], [78.75, 25.45]];   // BACK (SW6), OK (SW1): HRO K2-1102DP-L3SW-04, 9.5 mm
enc = [73.9, 36.5];     // encoder shaft
enc_hole = 7.0;
oled_win = [45.0, 28.55, 24.4, 7.6];     // centre x, y, size x, y (the viewing area)
oled = [25.8, 63.8, 23.5, 35.5];         // the module: x0, x1, y0, y1 (pins at the x1 end)
oled_z = [2.5, 3.9, 5.1];                // above the board: its underside, its PCB's top, the glass top (UNVERIFIED split)
oled_glass_x0 = 28.3;                    // the panel starts 5.5 + 30 mm from the pin end: x 28.3..58.3
led = [33.0, 4.2];
led_win = [3.5, 3.7];
led_h = 1.95;
ab = [[24.3, 9.0], [10.45, 48.0], [31.55, 48.0], [52.65, 48.0]];   // SW2..SW5 (OUT1..OUT4) slot centres
/* the printed caps (the user's answer 2026-11-08: "we can 3D print something") */
btn_cap = [5.4, 8.0, 1.0, 3.45, 3.0, 0.3];   // stem dia, flange dia, flange thickness, bore (plunger 3.4), proud of
                                             // the lid, flange gap under the lid at rest
slider_fit = 0.05;      // the slider cap's pocket over the 1.5 x 2.0 handle: + 0.05 each way (printed small: press)
slider_wall = 0.8;
slider_proud = 1.0;     // the slider cap's top above the lid
slider_lift = 0.2;      // its bottom above the switch body
knob = [14.0, 12.0, 6.1, 4.6, 10.0];   // dia, height, D bore dia, across the flat, bore depth
enc_flat = 4.5;         // UNVERIFIED: the EC11's D shaft, 4.5 across the flat over its top 10 mm
slider_cap = [ab_handle_w() + slider_fit + 2 * slider_wall, ab_handle_l() + slider_fit + 2 * slider_wall];
ab_slot = [slider_cap[0] + 0.35, slider_cap[1] + 2.0 + 0.35];   // the cap and its 2.0 travel, 0.175 round it
ab_handle = [1.5, 2.0, 2.0];
function ab_handle_w() = 1.5;
function ab_handle_l() = 2.0;
ab_body_h = 3.5;
ab_knob_h = 5.0;
sw_body_h = 3.5;        // UNVERIFIED: the buttons' body height
xiao_h = 7.0;           // UNVERIFIED estimate: the XIAO on 2.54 mm headers, the tallest top part but the encoder / buttons
usb_xiao = [45.2, 5.35];    // XIAO socket: KiCad x, centre above the board's top (UNVERIFIED estimate)
usb_j1 = [64.5, 1.65];      // J1 USB4125: KiCad x, centre above the board's top (UNVERIFIED estimate)
usb_overhang = 1.5;
usb_open = [12.5, 7.0];

/* [DIN: HOOYA DIN-504B-M10, drawing rev A1 2019-07-10] */
din_plate = [20.9, 19.6, 1.8];
din_depth = 13.6;       // plate front to the body's back
din_in = 1.0;           // the plate's front 1.0 mm behind the board's edge (v5 footprint)
din_axis = 9.7;         // below the board's bottom
din_bore = 16.0;        // UNVERIFIED: the plug's shell (the HOOYA's opening is not on its sheet)
din_floor_gap = 0.1;
din_back_gap = 0.15;
din_lip_t = 1.4;
din_lip_stop = 5.0;     // the lip stops this far below the board: the 2-pin row's legs (15.8 in) go up past it, UNVERIFIED

/* [Jack: XKB PJ-320A, drawing rev A1 2025-03-03] */
jack = [6.2, 12.2, 5.0];
bush_d = 5.0;           // UNVERIFIED (measured on the sheet)
bush_out = 2.0;
bush_z = 2.5;
bush_slot = 5.2;
jack_play = 0.15;
cap_wall = 1.6;
cap_stop = 1.6;         // the lid's jack cap stops this far above the board: the DINs' legs (1.5) stand there

/* [Box] */
wall = 2.0;             // side walls
conn_t = 1.8;           // front and back walls above the board: the jack's 2.0 mm bushing stands 0.1 proud
edge_gap = 0.1;
floor_t = 2.0;
side_clr = 0.3;
contact = 0.05;
face_h = 7.2;           // the board's top to the lid's inside: the XIAO's 7.0 + 0.2
lid_t = 1.5;
standoff_d = 4.4;       // H3's column must miss R21 (0.1 mm)
pilot_d = 1.7;
screw_d = 2.4;
pilot_depth = 11;       // M2 x 20 self-tapping: lid 1.5 + spacer 7.2 + board 1.6, 9.7 into the column
spacer_d = 5.0;
rib = [0.2, 1.3];
lid_play = 0.15;
key_play = 0.15;
usb_tongue_y = 0.55;
j1_legs = [2.2, 0.7];   // the relief for J1's legs under the board: height, depth into the back wall
led_tube = 0.6;
led_skin = 0.6;
led_tube_gap = 0.3;
button_hole = btn_cap[0] + 0.6;   // the cap's stem through it, 0.3 a side
oled_rib = [26.0, 27.6];  // a rib in the lid over the module's far end (no glass there): x from / to
oled_rib_gap = 0.2;
teardrop_cap = 0.8;

/* [Markings] */
markings = true;
mark_font = "Liberation Sans:style=Bold";
mark_size = 4.2;        // capitals 4.0 mm, stems 0.84 mm
mark_logo_size = 6.3;
mark_depth = 0.4;
mark_gap = 1.0;
mark_clear = 0.6;
ab_a_end = "back";      // UNVERIFIED which end of an A/B slot is A

$fn = 64;

// ---------------------------------------------------------------------------------------------------------------
X0 = wall + side_clr;                    // the board's left edge
Y0 = conn_t + edge_gap;                  // the board's front edge
Ybk = Y0 + board_w;                      // its back edge
Zbot = floor_t + din_floor_gap + din_plate[1];   // the board's bottom
Zb = Zbot + board_t;                     // its top
Zl = Zb + face_h;                        // the lid's underside = the top of every wall
H = Zl + lid_t;
L = X0 + board_l + side_clr + wall;
W = Ybk + edge_gap + conn_t;
function kx(x) = X0 + x;
function ky(y) = Ybk - y;
function chx(k) = kx(ch_x[k]);
encC = [kx(enc[0]), ky(enc[1])];

module at_ch(k) { if (ch_back[k]) translate([chx(k), Ybk, 0]) mirror([0, 1, 0]) children();
                  else translate([chx(k), Y0, 0]) children(); }
module xz_extrude(y0, len) { translate([0, y0 + len, 0]) rotate([90, 0, 0]) linear_extrude(len) children(); }
module teardrop2d(d) { r = d / 2; intersection() {
    union() { circle(d = d); rotate(45) square(r); }
    translate([-r - 1, -r - 1]) square([d + 2, d + 1 + teardrop_cap]); } }
module uslot2d(d, up) { union() { circle(d = d); translate([-d / 2, 0]) square([d, up]); } }

// ---------------------------------------------------------------------------------------------------------------
// The board model for the checks: ../pcb/mockup.stl (both sides, z = 0 its bottom), the buttons, the A/B switches
// and the OLED redrawn (the mockup draws them as full-height blocks).
module mock_m() {       // mockup frame: x 100..184, y -167 (front) .. -100 (back)
    difference() {
        import("../pcb/mockup.stl", convexity = 10);
        for (b = buttons) translate([100 + b[0] - 4.9, -100 - b[1] - 3.9, 1.5]) cube([9.8, 7.8, 10]);
        for (s = ab) translate([100 + s[0] - 2.85, -100 - s[1] - 4.9, 1.5]) cube([5.7, 9.8, 5.2]);
        translate([125.7, -135.6, 4.0]) cube([38.2, 12.2, 2.8]);
    }
    for (b = buttons) {
        translate([100 + b[0] - 4.795, -100 - b[1] - 3.795, 1.59]) cube([9.59, 7.59, sw_body_h + 0.01]);
        translate([100 + b[0], -100 - b[1], 1.59]) cylinder(d = 3.4, h = 9.51);
    }
    for (s = ab) {
        translate([100 + s[0] - 2.795, -100 - s[1] - 4.845, 1.59]) cube([5.59, 9.69, ab_body_h + 0.01]);
        translate([100 + s[0] - ab_handle[0] / 2, -100 - s[1] - (ab_handle[1] + ab_handle[2]) / 2, 1.6 + ab_body_h - 0.01])
            cube([ab_handle[0], ab_handle[1] + ab_handle[2], ab_knob_h - ab_body_h + 0.01]);
    }
    translate([100 + oled[0], -100 - oled[3], 1.6 + oled_z[0]]) cube([oled[1] - oled[0], oled[3] - oled[2], oled_z[1] - oled_z[0]]);
    translate([100 + oled_glass_x0, -100 - oled[3] + 0.22, 1.6 + oled_z[1] - 0.01]) cube([30.0, 11.56, oled_z[2] - oled_z[1] + 0.01]);
}
module mock() { translate([X0 - 100, Y0 + 167, Zbot]) mock_m(); }

// ---------------------------------------------------------------------------------------------------------------
// local frame of a channel: origin on the board's edge at the axis, +y into the board
module din_pad() {      // the wall 1.0 mm thicker under the board: the plate's bearing face, the board edge's ledge
    // its face 0.1 in front of the plate; 0.15 wider than the plate a side, so neighbours (21.1 apart) overlap and merge
    translate([-din_plate[0] / 2 - 0.15, -edge_gap - 0.01, floor_t - 0.01]) cube([din_plate[0] + 0.3, din_in - 0.1 + edge_gap + 0.01, Zbot - contact - floor_t + 0.01]);
}
module din_lip() {
    yb = din_in + din_depth + din_back_gap;
    translate([-din_plate[0] / 2, yb, floor_t - 0.01]) cube([din_plate[0], din_lip_t, Zbot - din_lip_stop - floor_t + 0.01]);
}
module ch_cut() {       // the jack's U-slot (above the board) and the DIN's closed bore (below), through wall and pad
    y0 = -edge_gap - conn_t - 1;
    translate([0, 0, Zb + bush_z]) xz_extrude(y0, conn_t + 1 + edge_gap + 0.01) uslot2d(bush_slot, H);
    translate([0, 0, Zbot - din_axis]) xz_extrude(y0, conn_t + 1 + din_in + 0.01) teardrop2d(din_bore);
}
module lid_key() {
    kw = bush_slot / 2 - key_play;
    translate([0, 0, Zb + bush_z]) xz_extrude(-edge_gap - conn_t, conn_t)
        difference() { translate([-kw, 0]) square([2 * kw, H - (Zb + bush_z)]); circle(d = bush_d + 2 * key_play); }
}
module lid_cap() {      // hangs from the lid round the jack's body: its back stop takes the push-in
    w = jack[0] / 2 + jack_play + cap_wall;
    yb = jack[1] + jack_play + cap_wall;
    difference() {
        translate([-w, 0, Zb + cap_stop]) cube([2 * w, yb, Zl - Zb - cap_stop + 0.01]);
        translate([-jack[0] / 2 - jack_play, -1, Zb - 1]) cube([jack[0] + 2 * jack_play, jack[1] + jack_play + 1, jack[2] + jack_play + 1]);
    }
}
module lid_ribs() {     // press the board's edge onto the DIN pad, beside the jack
    for (s = [-1, 1]) translate([s > 0 ? jack[0] / 2 + jack_play + 0.1 : -din_plate[0] / 2, rib[0], Zb + contact])
        cube([din_plate[0] / 2 - jack[0] / 2 - jack_play - 0.1, rib[1] - rib[0], Zl - Zb - contact + 0.01]);
}

module standoff(h) {
    c = [kx(h[0]), ky(h[1])];
    hull() {
        translate([c[0], c[1], floor_t - 0.01]) cylinder(d = standoff_d, h = Zbot - contact - floor_t + 0.01);
        translate([c[0] < L / 2 ? wall - 0.01 : L - wall - 0.01, c[1] - 1, floor_t - 0.01]) cube([0.02, 2, Zbot - contact - floor_t]);
    }
}
module usb_u2d(w, r, up) { hull() { for (x = [-w / 2 + r, w / 2 - r]) translate([x, r]) circle(r); translate([-w / 2, up]) square([w, 0.01]); } }
module usb_cut(u) {
    translate([kx(u[0]), Ybk - 1, Zb + u[1] - usb_open[1] / 2]) rotate([90, 0, 0]) mirror([0, 0, 1])
        linear_extrude(W - Ybk + 2) usb_u2d(usb_open[0], 1.5, H);
}
module usb_tongue(u) {
    z0 = Zb + u[1] + usb_open[1] / 2;
    if (z0 < Zl) translate([kx(u[0]) - usb_open[0] / 2 + lid_play, Ybk + usb_tongue_y, z0]) cube([usb_open[0] - 2 * lid_play, W - Ybk - usb_tongue_y, Zl - z0 + 0.01]);
}

module box() {
    difference() {
        union() {
            difference() {
                cube([L, W, Zl]);
                translate([wall, Y0 - edge_gap, floor_t]) cube([L - 2 * wall, Ybk - Y0 + 2 * edge_gap, Zl]);
            }
            for (k = [0 : 3]) at_ch(k) { din_pad(); din_lip(); }
            for (h = holes) standoff(h);
        }
        for (k = [0 : 3]) at_ch(k) ch_cut();
        usb_cut(usb_xiao);
        usb_cut(usb_j1);
        // J1's shell legs stand 0.55 past the back edge under the board (mockup): a relief in the wall's inside
        translate([kx(usb_j1[0]) - 5.7, Ybk + edge_gap - 0.01, Zbot - j1_legs[0]]) cube([11.4, j1_legs[1], j1_legs[0] + 0.5]);
        wall_text_cut();
        for (h = holes) translate([kx(h[0]), ky(h[1]), Zbot - pilot_depth]) cylinder(d = pilot_d, h = pilot_depth + 1);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Markings: controls on the lid's top, the plugs on the walls, each once
capH = function(sz) sz * 0.955;
function abA(c) = ab_a_end == "back" ? [c[1] + ab_slot[1] / 2 + mark_gap, c[1] - ab_slot[1] / 2 - mark_gap - capH(mark_size)]
                                     : [c[1] - ab_slot[1] / 2 - mark_gap - capH(mark_size), c[1] + ab_slot[1] / 2 + mark_gap];
function lid_labels() = concat([
        ["BACK", kx(73.5), ky(buttons[0][1]) + button_hole / 2 + mark_gap, "right", "baseline", mark_size],
        ["OK", kx(74.5), ky(buttons[1][1]) + button_hole / 2 + mark_gap, "left", "baseline", mark_size],
        ["BPM", encC[0], encC[1] - knob[0] / 2 - mark_gap - capH(mark_size), "center", "baseline", mark_size],   // under the knob's edge
        ["jank", kx(15.5), ky(31.5), "center", "baseline", mark_logo_size]],
        [for (c0 = ab) let(c = [kx(c0[0]), ky(c0[1])], y = abA(c)) each [
            ["A", c[0], y[0], "center", "baseline", mark_size], ["B", c[0], y[1], "center", "baseline", mark_size]]]);
module label2d(l) { translate([l[1], l[2]]) text(l[0], size = l[5], font = mark_font, halign = l[3], valign = l[4]); }
module lid_text_cut() { if (markings) translate([0, 0, H - mark_depth]) linear_extrude(mark_depth + 1) for (l = lid_labels()) label2d(l); }
// wall labels: [text, x, z (baseline), wall (0 front, 1 back), halign]; on the back, "right" runs towards +X
function wall_labels() = concat(
    [for (k = [1 : 3]) [str(k + 1), chx(k) + bush_slot / 2 + mark_gap, Zb + 1.0, 0, "left"]],
    [["1 / IN", chx(0) + bush_slot / 2 + mark_gap, Zb + 1.0, 1, "right"],
     ["USB", kx(usb_xiao[0]), Zb + usb_xiao[1] - usb_open[1] / 2 - mark_gap - capH(mark_size), 1, "center"],
     ["PWR", kx(usb_j1[0]), Zb + usb_j1[1] - usb_open[1] / 2 - mark_gap - capH(mark_size), 1, "center"]]);
module wall_label(l, depth, grow = 0) {
    if (l[3] == 0) translate([l[1], depth, l[2]]) rotate([90, 0, 0]) linear_extrude(depth + 1)
        offset(r = grow) text(l[0], size = mark_size, font = mark_font, halign = l[4], valign = "baseline");
    else translate([l[1], W - depth, l[2]]) rotate([90, 0, 180]) linear_extrude(depth + 1)
        offset(r = grow) text(l[0], size = mark_size, font = mark_font, halign = l[4], valign = "baseline");
}
module wall_text_cut() { if (markings) for (l = wall_labels()) wall_label(l, mark_depth); }

module lid_openings2d() {
    for (p = buttons) translate([kx(p[0]), ky(p[1])]) circle(d = button_hole);
    translate(encC) circle(d = knob[0]);   // the knob's footprint (over the 7.0 hole)
    translate([kx(oled_win[0]) - oled_win[2] / 2, ky(oled_win[1]) - oled_win[3] / 2]) square([oled_win[2], oled_win[3]]);
    translate([kx(led[0]) - led_win[0] / 2, ky(led[1]) - led_win[1] / 2]) square(led_win);
    for (c = ab) translate([kx(c[0]) - ab_slot[0] / 2, ky(c[1]) - ab_slot[1] / 2]) square(ab_slot);
    for (h = holes) translate([kx(h[0]), ky(h[1])]) circle(d = 4.0);
    translate([kx(usb_xiao[0]) - usb_open[0] / 2, Ybk]) square([usb_open[0], W - Ybk + 1]);
    difference() { translate([-5, -5]) square([L + 10, W + 10]); offset(delta = -0.8) square([L, W]); }
}
module lid() {          // in place
    difference() {
        union() {
            translate([0, 0, Zl]) cube([L, W, lid_t]);
            for (k = [0 : 3]) at_ch(k) { lid_key(); lid_cap(); lid_ribs(); }
            usb_tongue(usb_xiao);
            usb_tongue(usb_j1);
            for (h = holes) translate([kx(h[0]), ky(h[1]), Zb + contact]) cylinder(d = spacer_d, h = Zl - Zb - contact + 0.01);
            difference() {      // the LED's light shroud, cut back where the XIAO (x 35.26 on) stands
                translate([kx(led[0]) - led_win[0] / 2 - led_tube, ky(led[1]) - led_win[1] / 2 - led_tube, Zb + led_h + led_tube_gap])
                    cube([led_win[0] + 2 * led_tube, led_win[1] + 2 * led_tube, Zl - Zb - led_h - led_tube_gap + 0.01]);
                translate([kx(35.255) - 0.2, Y0, Zb - 1]) cube([30, board_w, face_h + 2]);
            }
            translate([kx(oled_rib[0]), ky(oled[3]), Zb + oled_z[1] + oled_rib_gap])   // over the module's far end
                cube([oled_rib[1] - oled_rib[0], oled[3] - oled[2], Zl - Zb - oled_z[1] - oled_rib_gap + 0.01]);
        }
        for (p = buttons) translate([kx(p[0]), ky(p[1]), Zl - 1]) cylinder(d = button_hole, h = lid_t + 2);
        translate([encC[0], encC[1], Zl - 1]) cylinder(d = enc_hole, h = lid_t + 2);
        translate([kx(oled_win[0]) - oled_win[2] / 2, ky(oled_win[1]) - oled_win[3] / 2, Zl - 1]) cube([oled_win[2], oled_win[3], lid_t + 2]);
        translate([kx(led[0]) - led_win[0] / 2, ky(led[1]) - led_win[1] / 2, Zb])
            cube([led_win[0], led_win[1], H - Zb - led_skin + (led_skin > 0 ? 0 : 1)]);
        for (s = ab) translate([kx(s[0]) - ab_slot[0] / 2, ky(s[1]) - ab_slot[1] / 2, Zl - 1]) cube([ab_slot[0], ab_slot[1], lid_t + 2]);
        for (h = holes) translate([kx(h[0]), ky(h[1]), Zb]) cylinder(d = screw_d, h = H);
        // the XIAO's opening reaches above the walls (its socket's top is 7.0 up): a notch in the lid's back edge
        translate([kx(usb_xiao[0]) - usb_open[0] / 2, Ybk + edge_gap - 0.01, Zl - 1]) cube([usb_open[0], W, lid_t + 2]);
        lid_text_cut();
    }
}
// ---------------------------------------------------------------------------------------------------------------
// The printed caps, in place (box frame). Button cap: the bore's floor on the plunger's top (Zb + 9.5).
module button_cap(p, press = 0) {
    top = H + btn_cap[4];                                    // stands btn_cap[4] above the lid's top
    fz = Zl - btn_cap[5] - btn_cap[2];                       // flange bottom
    translate([kx(p[0]), ky(p[1]), -press]) difference() {
        union() {
            translate([0, 0, fz]) cylinder(d = btn_cap[1], h = btn_cap[2]);
            translate([0, 0, fz]) cylinder(d = btn_cap[0], h = top - fz - 0.5);
            translate([0, 0, top - 0.5]) cylinder(d1 = btn_cap[0], d2 = btn_cap[0] - 1.0, h = 0.5);
        }
        translate([0, 0, fz - 1]) cylinder(d = btn_cap[3], h = Zb + 9.5 - fz + 1);
    }
}
function btn_cap_top() = H + btn_cap[4];
module slider_cap(c, pos = 0) {   // pos: -1 / 0 / +1 = one end of the travel, the middle, the other end
    x = kx(c[0]); y = ky(c[1]) + pos * ab_handle[2] / 2;
    z0 = Zb + ab_body_h + slider_lift;
    translate([x, y, 0]) difference() {
        translate([-slider_cap[0] / 2, -slider_cap[1] / 2, z0]) cube([slider_cap[0], slider_cap[1], H + slider_proud - z0]);
        translate([-(ab_handle_w() + slider_fit) / 2, -(ab_handle_l() + slider_fit) / 2, z0 - 1])
            cube([ab_handle_w() + slider_fit, ab_handle_l() + slider_fit, Zb + ab_knob_h - z0 + 1]);
    }
}
module enc_knob() {     // pushed on the D shaft until the bore's floor meets the shaft's top
    z0 = Zb + 20 - knob[4];
    translate([encC[0], encC[1], z0]) difference() {
        cylinder(d = knob[0], h = knob[1], $fn = 96);
        translate([0, 0, -1]) intersection() {
            cylinder(d = knob[2], h = knob[4] + 1);
            translate([-knob[2], -knob[2], 0]) cube([2 * knob[2], knob[2] + knob[3] - knob[2] / 2, knob[4] + 1]);   // the flat at +y
        }
        translate([-0.5, 0, knob[1] - 0.6]) cube([1.0, knob[0], 1]);   // the pointer: a groove on top, on the flat's side
    }
}
module enc_shaft_d() {  // the shaft with its flat, for the knob's check
    translate([encC[0], encC[1], Zb + 6.5]) intersection() {
        cylinder(d = 6.0, h = 20 - 6.5);
        translate([-3.5, -3.5, 0]) cube([7, 3.5 + enc_flat - 3.0, 20]);   // the flat at +y
    }
}
module caps(press = 0, pos = 0) {
    for (b = buttons) button_cap(b, press);
    for (c = ab) slider_cap(c, pos);
    enc_knob();
}

module lid_print() { translate([0, W, H]) rotate([180, 0, 0]) lid(); }   // top face on the bed

// a strip of the front wall: one jack U-slot over one DIN bore, the DIN pad behind the lower part; printed standing
module front_test() {
    w = 21.1; zb = 20; t = conn_t;
    difference() {
        union() {
            cube([w, t, zb + 8]);
            translate([w / 2 - din_plate[0] / 2 - 0.1, t - 0.01, 0]) cube([din_plate[0] + 0.2, din_in + 0.01, zb - board_t]);
            translate([0, t - 0.01, 0]) cube([w, 10, 1.5]);
        }
        translate([w / 2, 0, zb - board_t - din_axis]) xz_extrude(-1, t + din_in + 2) teardrop2d(din_bore);
        translate([w / 2, 0, zb + bush_z]) xz_extrude(-1, t + 2) uslot2d(bush_slot, 30);
    }
}

// ---------------------------------------------------------------------------------------------------------------
module din_model() { translate([-din_plate[0] / 2, din_in, Zbot - din_plate[1]]) cube([din_plate[0], din_depth, din_plate[1]]); }
module jack_body() { translate([-jack[0] / 2, 0, Zb]) cube([jack[0], jack[1], jack[2]]); }
module board_slab(dz) { translate([X0, Y0, Zbot + dz]) cube([board_l, board_w, board_t]); }

if (part == "box") box();
else if (part == "button_cap") translate([0, 0, -(Zl - btn_cap[5] - btn_cap[2])]) translate([-kx(buttons[0][0]), -ky(buttons[0][1]), 0]) button_cap(buttons[0]);   // flange on the bed
else if (part == "slider_cap") translate([0, 0, H + slider_proud]) rotate([180, 0, 0]) translate([-kx(ab[0][0]), -ky(ab[0][1]), 0]) slider_cap(ab[0]);   // top on the bed
else if (part == "knob") translate([0, 0, Zb + 20 - knob[4] + knob[1]]) rotate([180, 0, 0]) translate([-encC[0], -encC[1], 0]) enc_knob();   // top on the bed
else if (part == "check_caps") {   // must be EMPTY: the caps against the lid at rest, pressed 0.3, the sliders at both ends;
    // against the board (the sliders' pockets round their handle, the knob's bore round the D shaft)
    intersection() { lid(); union() { caps(0, 0); caps(0.3, -1); caps(0.3, 1); } }
    intersection() { union() { for (b = buttons) { button_cap(b); button_cap(b, 0.3); } }
                     difference() { mock(); for (b = buttons) translate([kx(b[0]), ky(b[1]), Zb + 3]) cylinder(d = 3.41, h = 7); } }
    intersection() { union() { for (c = ab) for (q = [-1, 1]) slider_cap(c, q); }
                     difference() { mock(); for (c = ab) translate([kx(c[0]) - 1, ky(c[1]) - 3.1, Zb + ab_body_h - 0.01]) cube([2, 6.2, 2]); } }
    intersection() { enc_knob(); union() { difference() { mock(); translate([encC[0], encC[1], Zb + 6.4]) cylinder(d = 6.2, h = 15); } enc_shaft_d(); box(); } }
    for (c = ab) for (q = [-1, 1]) intersection() {   // the handle in its pocket at each end
        slider_cap(c, q);
        translate([kx(c[0]) - ab_handle[0] / 2, ky(c[1]) + q * ab_handle[2] / 2 - ab_handle[1] / 2, Zb + ab_body_h]) cube([ab_handle[0], ab_handle[1], ab_knob_h - ab_body_h]);
    }
}
else if (part == "brace_caps")   // must NOT be empty: a button cap pulled up 0.35 meets the lid (the flange keeps it)
    intersection() { lid(); translate([0, 0, btn_cap[5] + 0.05]) for (b = buttons) button_cap(b); }
else if (part == "lid") lid_print();
else if (part == "front_test") front_test();
else if (part == "mock") mock();
else if (part == "check_box") intersection() { box(); mock(); }
else if (part == "check_lid") intersection() { lid(); mock(); }
else if (part == "check_lidbox") intersection() { translate([0, 0, 0.01]) lid(); box(); }
else if (part == "check_drop") intersection() { union() { for (dz = [0.25 : 0.25 : 22]) translate([0, 0, dz]) lid(); } union() { box(); mock(); for (b = buttons) button_cap(b); for (c = ab) slider_cap(c); } }
else if (part == "check_board_drop") intersection() { union() { for (dz = [0.5 : 0.5 : 26]) translate([0, 0, dz]) mock(); } box(); }
else if (part == "check_text") {
    if (markings) {
        intersection() { lid_text_cut(); translate([-1, -1, -1]) cube([L + 2, W + 2, Zl + 0.3 + 1]); }
        intersection() {
            translate([0, 0, -1]) linear_extrude(H + 2) offset(r = mark_clear) for (l = lid_labels()) label2d(l);
            translate([0, 0, -1]) linear_extrude(H + 2) lid_openings2d();
        }
        intersection() {
            for (l = wall_labels()) wall_label(l, 3, mark_clear);
            union() {
                for (k = [0 : 3]) at_ch(k) ch_cut();
                usb_cut(usb_xiao); usb_cut(usb_j1);
                translate([-1, -1, Zl - 0.5]) cube([L + 2, W + 2, 20]);
                translate([-1, -1, -1]) cube([L + 2, W + 2, 1.5]);
                for (x = [-10, L - 0.5]) translate([x, -1, -1]) cube([10.5, W + 2, H + 2]);
            }
        }
    }
}
else if (part == "caps_view") { color("LightSteelBlue") lid(); color("Orange") caps(); color("SteelBlue") box(); }
else if (part == "lid_top") {
    color("LightSteelBlue") lid();
    color("Orange") caps();
    if (markings) color("DimGray") translate([0, 0, H - mark_depth]) linear_extrude(0.05) for (l = lid_labels()) label2d(l);
}
else if (part == "brace_pull") intersection() { box(); for (k = [0 : 3]) at_ch(k) translate([0, -(edge_gap + 0.05), 0]) { din_model(); jack_body(); } }
else if (part == "brace_push") intersection() { box(); for (k = [0 : 3]) at_ch(k) translate([0, din_back_gap + 0.05, 0]) din_model(); }
else if (part == "brace_trs") intersection() { lid(); for (k = [0 : 3]) at_ch(k) translate([0, jack_play + 0.05, 0]) jack_body(); }
else if (part == "brace_floor") intersection() { box(); for (k = [0 : 3]) at_ch(k) translate([0, 0, -(din_floor_gap + 0.05)]) din_model(); }
else if (part == "brace_lid") intersection() { lid(); board_slab(contact + 0.05); }
else if (part == "brace_ledge") intersection() { box(); board_slab(-(contact + 0.05)); }
else if (part == "open") { color("SteelBlue") box(); color("DarkGreen") mock(); color("LightSteelBlue") translate([0, 0, 30]) lid(); }
else if (part == "front" || part == "back") { color("SteelBlue") box(); color("LightSteelBlue") lid(); color("DarkGreen") mock(); }
else { color("SteelBlue", 0.5) box(); color("LightSteelBlue", 0.35) lid(); color("DarkGreen") mock(); }
echo(str("jank box v5: outer ", L, " x ", W, " x ", H, " mm (X x Y x Z, lid included); board bottom at Z ", Zbot,
         ", top ", Zb, ", lid underside ", Zl));
