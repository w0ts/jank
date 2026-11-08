// SPDX-License-Identifier: GPL-3.0-only
// jank box: the printed box of the jank board, round 7 / v4 (84.0 x 52.0 mm; hardware/pcb/README.md).
//
// FIRST REQUIREMENT (the user's ruling 2026-11-08): the print braces the DIN sockets (and the TRS jacks) so that
// plugging and unplugging loads the box, not the solder joints. Every channel gets the same bracing, whichever wall
// it is on (OUT2..OUT4 the front wall, OUT1 the back wall):
//   DIN (CUI SDS-50J: square front plate 20.0 x 21.1 x 1.8 mm, an 18.0 mm housing behind it, 15.8 deep):
//     pull-out -> plate front on the wall's inside (0.1); push-in -> plate back on the cradle's face, a 3.0 mm band
//     above the housing (0.2), and the housing's back on the cradle's lip (0.15); to the floor -> plate edge on the
//     floor (0.1), housing top on the cradle (0.1)
//   TRS (CUI SJ1-3523N: body 12 x 11 x 5, plain 6.0 mm bushing, no nut): bushing in a U-slot of the wall, closed by
//     a key of the lid; pull-out -> body front on the wall's inside; push-in -> the cap's back stop; the cap carries it
//   The board is clamped at its connector edges between the lid's ribs and the connector housings (plastic on board),
//   on two standoffs (H1, H2) and two ledges: no force path runs through a solder joint.
// The lid: 3.6 mm above the face everywhere (the A/B knobs stay reachable), with a window round the encoder's body
// (it stands 1.4 mm through it); it drops straight down and is held by four M2 screws: two through the board's
// H1 / H2, two into the thick +X wall.
//
// Frame: box outer corner at the origin, X along the front (left to right seen from the front, lid up), Y from the
// front to the back, Z up from the floor. The board lies upside down: its parts (KiCad top, the floor side) hang
// towards the floor, its face side (KiCad bottom: buttons, encoder, LEDs, A/B switches) looks up at the lid.
// Board coordinates here: fx = 84 - (KiCad x) = the face view's x; KiCad y is measured from the BACK edge.
// Every number marked UNVERIFIED is from a drawing or an estimate, not from a part in hand (README "Unverified").

/* [What to render] */
outs = "DDDD";          // OUT1..OUT4: D = DIN (SDS-50J), T = TRS (SJ1-3523N). OUT1 is the back one (MIDI IN capable)
part = "assembly";      // [assembly, box, lid, front_test, mock, open, front, back, check_box, check_lid, check_lidbox, check_drop, brace_pull, brace_push, brace_trs, brace_floor, brace_lid, brace_ledge]

/* [Board: hardware/pcb/README.md, round 7 "The box"] */
board_l = 84.0;
board_w = 52.0;
board_t = 1.6;          // UNVERIFIED: JLCPCB default, assumed in the board README
ch_x = [10.75, 10.75, 31.25, 51.75];     // OUT1..OUT4 axes, KiCad x
ch_back = [true, false, false, false];   // OUT1 on the back edge, OUT2..4 on the front edge
holes = [[80.8, 25.0], [80.8, 48.8]];    // H1, H2 (2.2 mm), KiCad x, y from the back
buttons = [[47.75, 13.8], [58.55, 13.8]];   // MODE (SW1), PLAY/STOP (SW6): HRO K2-1102DP-L3SW-04, 9.5 mm
button_body = [[143.955, -118.595], [154.755, -118.595]];   // their mockup blocks' corners (7.59 x 9.59)
enc = [75.5, 14.5];     // encoder SW7 shaft, KiCad
enc_body = [12.5, 12.5, 6.5];  // the mockup's block (README: 11.7 x 12.5); 6.5 tall UNVERIFIED
enc_shaft = [6.0, 20.0];       // D shaft, top 20 mm above the face (UNVERIFIED: "from the mounting surface")
leds = [[45.5, 4.0], [49.85, 4.0]];      // LED1, LED2 windows, KiCad
led_win = [3.2, 3.6];                    // x by y, over the 3.0 x 3.4 cut-outs
ab = [[25.2, 10.75], [10.75, 32.25], [31.25, 32.25], [51.75, 32.25]];  // SW2..SW5 slot centres (= travel middle)
ab_slot = [2.0, 4.5];                    // handle 1.5 x 2.0 + 2.0 travel along y + 0.25 round
ab_handle = [1.5, 2.0, 2.0];             // width, length, travel (MSS22D18G2 drawing via the board README)
ab_body_h = 3.5;
ab_knob_h = 5.0;
sw_body_h = 3.5;        // UNVERIFIED: HRO K2-1102DP body height taken as 3.5 (the plunger is 9.5)
face_h = 3.6;           // board face to the lid's inside: the A/B knobs (5.0) end 0.1 under a 1.5 mm lid's top
lid_t = 1.5;
res_h = 11.5;           // UNVERIFIED estimate: tallest floor-side part without a DIN (33 ohm 0.5 W standing)
usb_xiao = [45.2, 5.35];    // XIAO socket: KiCad x, centre height above the floor-side surface (UNVERIFIED estimate)
usb_j1 = [64.5, 1.65];      // J1 USB4125: KiCad x, centre height (UNVERIFIED: 3.3 mm part, estimate)
usb_overhang = 1.5;         // XIAO socket past the back edge (Seeed footprint, UNVERIFIED)
usb_open = [12.5, 7.0];     // back-wall opening per socket, for a USB-C plug's overmold (UNVERIFIED, typical <= 12 x 6.5)
header_keepout = [[151.755, -121.02, 3.44, 21.065], [135.255, -120.97, 3.34, 20.965]];  // XIAO header pins, face side (mockup)

/* [DIN: CUI SDS-50J, datasheet SDS-J rev 1.08, drawing page 3, tolerance +/-0.3] */
din_plate = [20.0, 21.1, 1.8];
din_body_h = 18.0;
din_depth = 15.8;
din_axis = 10.0;
din_overlap = 1.0;      // the wall's lip (and the lid's tongue) lies 1.0 mm over the plate's edge, in front of it:
                        // the plate's face shows through an 18.0 x 19.1 window, its own socket opening is the plug's way in
edge_gap = 0.1;
din_slot_play = 0.2;
din_floor_gap = 0.1;
din_top_gap = 0.1;
din_back_gap = 0.15;
din_lip_t = 1.4;
din_lip_stop = [2.0, 12.5];  // lip stops this far above the board: [front channels, back channel (its resistors)]

/* [TRS: CUI SJ1-3523N, datasheet SJ1-352XN rev 09/12/2024, page 2] */
jack = [12.0, 11.0, 5.0];
bush_d = 6.0;
bush_out = 3.3;
bush_z = 2.5;
bush_slot = 6.2;
jack_play = 0.15;
cap_wall = 1.6;
cap_stop = 0.5;

/* [Box] */
wall = 2.0;             // the -X wall and the back wall's USB section
conn_t = 1.2;           // the skin in front of the connectors (front wall, OUT1's section of the back wall): the
                        // DIN plate's face sits 1.3 mm in, behind a 1.2 mm lip; the jack's bushing stands 2.0 proud
usb_t = 2.0;            // the back wall's USB section, 0.1 behind the board's edge: the XIAO socket's 1.5 overhang
                        // sits in its U-opening, 0.6 short of the outside
usb_tongue_y = 0.55;    // the USB tongues keep this far behind the board's back edge (J1's shell legs, mockup 0.545)
side_t = 4.0;           // +X wall: thick enough for two M2 pilots (the board has no holes on that side)
side_screws_y = [10, 42];   // mm from the board's front edge
floor_t = 2.0;
side_clr = 0.3;
contact = 0.05;
standoff_d = 5.5;
pilot_d = 1.7;          // M2 self-tapping into PLA / PETG
screw_d = 2.4;
pilot_depth = 8;
spacer_d = 5.0;
rib = [0.2, 1.3];
ledge = [0.5, 4.0];
lid_play = 0.15;
key_play = 0.15;
led_tube = 0.5;
led_skin = 0.6;
led_tube_gap = 0.3;
button_hole = 4.0;      // plunger 3.4 (HRO datasheet)
recess = 0.4;           // lid underside thinned over the button and A/B switch bodies
enc_clr = 0.4;          // encoder body to the lid's window, a side (the body stands through the lid)

/* [Markings] */
markings = true;        // debossed labels on the lid's top and the walls' outsides
mark_font = "Liberation Sans:style=Bold";   // ships with OpenSCAD
mark_size = 4.2;        // text size: Liberation Sans Bold capitals 4.0 mm tall, stems 0.84 mm (measured on the render)
mark_logo_size = 6.3;   // "jank": capitals 6.0 mm
mark_depth = 0.4;       // recess; the lid (1.5) keeps 1.1, 0.7 over a switch recess
mark_gap = 1.0;         // text to the opening it labels
mark_clear = 0.6;       // the least text-to-opening distance the check accepts
ab_a_end = "back";      // UNVERIFIED which end of an A/B slot is A: "back" (towards the back edge) or "front"
led_names = ["LINK", "USB"];   // LED1, LED2: ASSUMED from the firmware (LED1 = Link / network, LED2 = USB), not checked

$fn = 64;

// ---------------------------------------------------------------------------------------------------------------
function is_din(k) = outs[k] == "D";
any_din = len(search("D", outs)) > 0;
floor_side = any_din ? din_plate[1] + din_floor_gap : res_h + 1.0;

X0 = wall + side_clr;
Y0 = conn_t + edge_gap;
Ybk = Y0 + board_w;
Zt = floor_t + floor_side;
Zb = Zt + board_t;
Zl = Zb + face_h;                          // lid's underside = top of every wall
H = Zl + lid_t;
Xh = X0 + board_l + side_clr;              // +X wall's inside
L = Xh + side_t;
YinB = Ybk + edge_gap;                     // the back wall's inside, both sections
W = YinB + usb_t;
assert(usb_overhang < edge_gap + usb_t, "the XIAO's socket must end inside the back wall");
function fx(x) = X0 + board_l - x;
function by(y) = Ybk - y;
function chx(k) = fx(ch_x[k]);
xa0 = chx(0) - din_plate[0] / 2 - 0.5;     // the back wall's connector section (OUT1) runs from here to the +X wall
YoutA = Ybk + edge_gap + conn_t;
encC = [fx(enc[0]), by(enc[1])];         // encoder axis, box frame
enc_win = [enc_body[0] + 2 * enc_clr, enc_body[1] + 2 * enc_clr];   // the lid's window round its body

module outline2d() { union() { square([L, YoutA]); square([xa0, W]); } }
module cavity2d() { translate([wall, conn_t]) square([Xh - wall, YinB - conn_t]); }

module at_ch(k) { if (ch_back[k]) translate([chx(k), Ybk, 0]) mirror([0, 1, 0]) children();
                  else translate([chx(k), Y0, 0]) children(); }

module xz_extrude(y0, len) { translate([0, y0 + len, 0]) rotate([90, 0, 0]) linear_extrude(len) children(); }
module uslot2d(d, up) { union() { circle(d = d); translate([-d / 2, 0]) square([d, up]); } }

// ---------------------------------------------------------------------------------------------------------------
// The board model for the checks: the board's mockup (../pcb/mockup-din.stl) turned over into the box, the
// connectors, buttons and A/B switches redrawn from the datasheets (the mockup draws the DIN as a full 21.1 mm block,
// the buttons as 9.5 mm blocks, the A/B switches as 5.0 mm blocks). The encoder is the mockup's own (body + shaft).
module din_model() {
    translate([-din_plate[0] / 2, 0, Zt - din_plate[1]]) cube([din_plate[0], din_plate[2], din_plate[1] + 0.01]);
    translate([-din_plate[0] / 2, din_plate[2] - 0.01, Zt - din_body_h]) cube([din_plate[0], din_depth - din_plate[2] + 0.01, din_body_h + 0.01]);
}
module jack_model() {
    translate([-jack[0] / 2, 0, Zt - jack[2]]) cube([jack[0], jack[1], jack[2] + 0.01]);
    translate([0, 0.01, Zt - bush_z]) rotate([90, 0, 0]) cylinder(d = bush_d, h = bush_out + 0.01);
}
module mock_m() {       // mockup frame: x 100..184, y -152 (front) .. -100 (back), z up = floor side
    difference() {
        import("../pcb/mockup-din.stl", convexity = 10);
        for (b = button_body) translate([b[0] - 0.05, b[1] - 0.05, -9.6]) cube([7.7, 9.7, 9.6]);
        for (s = ab) translate([100 + s[0] - 2.85, -100 - s[1] - 5.2, -5.1]) cube([5.7, 10.4, 5.1]);
    }
    for (i = [0 : 1]) {
        translate([button_body[i][0], button_body[i][1], -sw_body_h]) cube([7.59, 9.59, sw_body_h + 0.01]);
        translate([100 + buttons[i][0], -100 - buttons[i][1], -9.5]) cylinder(d = 3.4, h = 9.51);   // plunger 3.4 (HRO)
    }
    for (s = ab) {
        translate([100 + s[0] - 2.795, -100 - s[1] + 0.25 - 4.845, -ab_body_h]) cube([5.59, 9.69, ab_body_h + 0.01]);   // (body 0.25 off the slot centre)
        translate([100 + s[0] - ab_handle[0] / 2, -100 - s[1] - (ab_handle[1] + ab_handle[2]) / 2, -ab_knob_h])
            cube([ab_handle[0], ab_handle[1] + ab_handle[2], ab_knob_h - ab_body_h + 0.01]);   // the knob over its whole travel
    }
}
module mock() {
    difference() {
        translate([X0 + 184, Y0 + 152, Zb]) rotate([0, 180, 0]) mock_m();
        for (k = [0 : 3]) at_ch(k) translate([-10.249, -4, -50]) cube([20.498, 4 + din_depth + 0.01, Zt + 50]);
    }
    for (k = [0 : 3]) at_ch(k) if (is_din(k)) din_model(); else jack_model();
}

// ---------------------------------------------------------------------------------------------------------------
module din_cradle(k) {
    yf = din_plate[2] + din_slot_play;
    yb = din_depth + din_back_gap;
    ztop = Zt - din_body_h - din_top_gap;
    stop = din_lip_stop[ch_back[k] ? 1 : 0];
    translate([-din_plate[0] / 2, yf, floor_t - 0.01]) cube([din_plate[0], yb + din_lip_t - yf, ztop - floor_t + 0.01]);
    translate([-din_plate[0] / 2, yb, floor_t - 0.01]) cube([din_plate[0], din_lip_t, Zt - stop - floor_t + 0.01]);
}
module jack_cap() {
    w = jack[0] / 2 + jack_play + cap_wall;
    yb = jack[1] + jack_play + cap_wall;
    difference() {
        translate([-w, -edge_gap - 0.01, floor_t - 0.01]) cube([2 * w, yb + edge_gap + 0.01, Zt - cap_stop - floor_t + 0.01]);
        translate([-jack[0] / 2 - jack_play, -edge_gap - 0.1, Zt - jack[2] - jack_play])
            cube([jack[0] + 2 * jack_play, jack[1] + jack_play + edge_gap + 0.1, jack[2] + jack_play + 1]);
    }
}
// every wall opening is a U open to the top of the wall; the lid's tongue or key closes its upper part
din_win = [din_plate[0] - 2 * din_overlap, din_plate[1] - din_overlap];   // x, from the plate's far edge + overlap upwards
module wall_cut(k) {    // local frame: the skin is y in [-edge_gap - conn_t, -edge_gap]
    y0 = -edge_gap - conn_t - 1;
    if (is_din(k)) translate([-din_win[0] / 2, y0, Zt - din_plate[1] + din_overlap]) cube([din_win[0], conn_t + 1 + 0.01, H]);
    else translate([0, 0, Zt - bush_z]) xz_extrude(y0, conn_t + 1 + edge_gap + 0.2) uslot2d(bush_slot, H);
}
module din_tongue() {   // local frame: closes the window's top, its lower 1.0 mm in front of the plate's board-side edge
    translate([-din_win[0] / 2 + lid_play, -edge_gap - conn_t, Zt - din_overlap]) cube([din_win[0] - 2 * lid_play, conn_t, Zl - Zt + din_overlap + 0.01]);
}
module standoff(h) {
    c = [fx(h[0]), by(h[1])];
    hull() {
        translate([c[0], c[1], floor_t - 0.01]) cylinder(d = standoff_d, h = Zt - contact - floor_t + 0.01);
        translate([wall - 0.01, c[1] - 1, floor_t - 0.01]) cube([0.02, 2, Zt - contact - floor_t]);
    }
}
module usb_u2d(w, r, up) { hull() { for (x = [-w / 2 + r, w / 2 - r]) translate([x, r]) circle(r); translate([-w / 2, up]) square([w, 0.01]); } }
module usb_cut(u) {     // a U from the opening's bottom (rounded, r 1.5) to the top of the wall
    translate([fx(u[0]), YinB - 1, Zt - u[1] - usb_open[1] / 2]) rotate([90, 0, 0]) mirror([0, 0, 1])
        linear_extrude(usb_t + 2) usb_u2d(usb_open[0], 1.5, H);
}
module usb_tongue(u) {  // closes the U down to the opening's top: the opening is 12.5 x 7.0
    z0 = Zt - u[1] + usb_open[1] / 2;
    translate([fx(u[0]) - usb_open[0] / 2 + lid_play, Ybk + usb_tongue_y, z0]) cube([usb_open[0] - 2 * lid_play, W - Ybk - usb_tongue_y, Zl - z0 + 0.01]);
}
function side_screw(i) = [Xh + side_t / 2, Y0 + side_screws_y[i]];

module box() {
    difference() {
        union() {
            difference() {
                linear_extrude(Zl) outline2d();
                translate([0, 0, floor_t]) linear_extrude(Zl) cavity2d();
            }
            for (k = [0 : 3]) at_ch(k) if (is_din(k)) din_cradle(k); else jack_cap();
            for (h = holes) standoff(h);
            for (y = [Y0 - edge_gap - 0.01, Ybk - ledge[1]])
                translate([Xh - side_clr - ledge[0], y, floor_t - 0.01])
                    cube([side_clr + ledge[0] + 0.01, ledge[1] + edge_gap + 0.01, Zt - contact - floor_t + 0.01]);
        }
        for (k = [0 : 3]) at_ch(k) wall_cut(k);
        usb_cut(usb_xiao);
        usb_cut(usb_j1);
        wall_text_cut();
        for (h = holes) translate([fx(h[0]), by(h[1]), Zt - pilot_depth]) cylinder(d = pilot_d, h = pilot_depth + 1);
        for (i = [0 : 1]) translate([side_screw(i)[0], side_screw(i)[1], Zl - pilot_depth]) cylinder(d = pilot_d, h = pilot_depth + 1);
    }
}

// ---------------------------------------------------------------------------------------------------------------
module lid_key(k) {
    kw = bush_slot / 2 - key_play;
    translate([0, 0, Zt - bush_z]) xz_extrude(-edge_gap - conn_t, conn_t)
        difference() { translate([-kw, 0]) square([2 * kw, H - (Zt - bush_z)]); circle(d = bush_d + 2 * key_play); }
    translate([-kw, -edge_gap - 0.01, Zl]) cube([2 * kw, edge_gap + 0.02, lid_t]);
}
// ---------------------------------------------------------------------------------------------------------------
// Markings. Lid labels in the box frame (read from the front, the lid up); each [text, x, y, halign, valign, size].
function abA(c) = ab_a_end == "back" ? [c[1] + ab_slot[1] / 2 + mark_gap, c[1] - ab_slot[1] / 2 - mark_gap - capH(mark_size)]
                                     : [c[1] - ab_slot[1] / 2 - mark_gap - capH(mark_size), c[1] + ab_slot[1] / 2 + mark_gap];
function capH(sz) = sz * 0.955;          // Liberation Sans Bold capital height / size (measured: 4.01 at 4.2)
function lid_labels() = let(
    mode = [fx(buttons[0][0]), by(buttons[0][1])], play = [fx(buttons[1][0]), by(buttons[1][1])],
    l1 = [fx(leds[0][0]), by(leds[0][1])], l2 = [fx(leds[1][0]), by(leds[1][1])])
    concat([
        ["MODE", mode[0] + button_hole / 2 + 0.7, mode[1], "left", "center", mark_size],
        ["PLAY", play[0], play[1] - button_hole / 2 - 1.5 - capH(mark_size), "center", "baseline", mark_size],
        ["BPM", encC[0], encC[1] + enc_win[1] / 2 + mark_gap, "center", "baseline", mark_size],
        [led_names[0], l1[0] + led_win[0] / 2 + 1.2, l1[1], "left", "center", mark_size],
        [led_names[1], l2[0] - led_win[0] / 2 - 1.2, l2[1], "right", "center", mark_size],
        ["jank", fx(12.0), Y0 + 4.5, "center", "baseline", mark_logo_size]],
        [for (c0 = ab) let(c = [fx(c0[0]), by(c0[1])], y = abA(c)) each [
            ["A", c[0], y[0], "center", "baseline", mark_size], ["B", c[0], y[1], "center", "baseline", mark_size]]]);
module label2d(l) { translate([l[1], l[2]]) text(l[0], size = l[5], font = mark_font, halign = l[3], valign = l[4]); }
module lid_text_cut() { if (markings) translate([0, 0, H - mark_depth]) linear_extrude(mark_depth + 1) for (l = lid_labels()) label2d(l); }
// wall labels: [text, x, z (baseline), wall, on, halign]: wall 0 = front (Y = 0), 1 = back at OUT1 (Y = YoutA),
// 2 = back, USB section (Y = W); on = "box" (cut into the wall) or "lid" (cut into the lid's tongue, flush with the wall).
// halign is the text's own: on the back walls, read from behind, "right" runs towards +X of the box.
function ch_label(k, t, w) = is_din(k)
    ? [t, chx(k), Zt - din_overlap + mark_clear + 0.3, w, "lid", "center"]                                     // on the DIN tongue
    : [t, chx(k), Zt - bush_z - bush_slot / 2 - mark_gap - capH(mark_size), w, "box", "center"];   // under the jack
function wall_labels() = concat(
    [for (k = [1 : 3]) ch_label(k, str(k + 1), 0)],
    [ch_label(0, "1 / IN", 1),
     ["USB", fx(usb_xiao[0]) + usb_open[0] / 2 + mark_gap, Zt - usb_xiao[1] - capH(mark_size) / 2, 2, "box", "right"],
     ["PWR", fx(usb_j1[0]), Zt - usb_j1[1] - usb_open[1] / 2 - mark_gap - capH(mark_size), 2, "box", "center"]]);
module wall_label(l, depth, grow = 0) {
    yface = l[3] == 0 ? 0 : l[3] == 1 ? YoutA : W;
    if (l[3] == 0) translate([l[1], depth, l[2]]) rotate([90, 0, 0]) linear_extrude(depth + 1)
        offset(r = grow) text(l[0], size = mark_size, font = mark_font, halign = l[5], valign = "baseline");
    else translate([l[1], yface - depth, l[2]]) rotate([90, 0, 180]) linear_extrude(depth + 1)
        offset(r = grow) text(l[0], size = mark_size, font = mark_font, halign = l[5], valign = "baseline");
}
module wall_text_cut() { if (markings) for (l = wall_labels()) if (l[4] == "box") wall_label(l, mark_depth); }
module lid_wall_text_cut() { if (markings) for (l = wall_labels()) if (l[4] == "lid") wall_label(l, mark_depth); }

module lid_openings2d() {   // what goes through the lid, and the band along its edge
    for (p = buttons) translate([fx(p[0]), by(p[1])]) circle(d = button_hole);
    translate([encC[0] - enc_win[0] / 2, encC[1] - enc_win[1] / 2]) square(enc_win);
    for (p = leds) translate([fx(p[0]) - led_win[0] / 2, by(p[1]) - led_win[1] / 2]) square(led_win);
    for (c = ab) translate([fx(c[0]) - ab_slot[0] / 2, by(c[1]) - ab_slot[1] / 2]) square(ab_slot);
    for (h = holes) translate([fx(h[0]), by(h[1])]) circle(d = 4.0);            // the screw heads (pan head ~3.8)
    for (i = [0 : 1]) translate(side_screw(i)) circle(d = 4.0);
    difference() { translate([-5, -5]) square([L + 10, W + 10]); offset(delta = -0.8) outline2d(); }
}
module lid() {          // in place
    difference() {
        union() {
            translate([0, 0, Zl]) linear_extrude(lid_t) outline2d();
            for (k = [0 : 3]) at_ch(k) {
                translate([-din_plate[0] / 2 - 0.25, rib[0], Zb + contact]) cube([din_plate[0] + 0.5, rib[1] - rib[0], Zl - Zb - contact + 0.01]);
                if (is_din(k)) din_tongue(); else lid_key(k);
            }
            usb_tongue(usb_xiao);
            usb_tongue(usb_j1);
            for (h = holes) translate([fx(h[0]), by(h[1]), Zb + contact]) cylinder(d = spacer_d, h = Zl - Zb - contact + 0.01);
            difference() {      // one light shroud round both LEDs, cut back where the XIAO's header pins stand
                translate([0, 0, Zb + led_tube_gap]) linear_extrude(Zl - Zb - led_tube_gap + 0.01)
                    hull() for (p = leds) translate([fx(p[0]), by(p[1])]) square([led_win[0] + 2 * led_tube, led_win[1] + 2 * led_tube], center = true);
                for (k = header_keepout) translate([X0 + 184 - k[0] - k[2] - 0.2, Y0 + 152 + k[1] - 0.2, Zb - 1]) cube([k[2] + 0.4, k[3] + 0.4, face_h + 2]);
            }
        }
        translate([encC[0] - enc_win[0] / 2, encC[1] - enc_win[1] / 2, Zl - 1]) cube([enc_win[0], enc_win[1], lid_t + 2]);   // encoder window
        for (p = buttons) translate([fx(p[0]), by(p[1]), Zl - 1]) cylinder(d = button_hole, h = lid_t + 2);
        for (b = button_body) translate([X0 + 184 - b[0] - 7.59 - 0.3, Y0 + 152 + b[1] - 0.3, Zl - 1]) cube([8.19, 10.19, 1 + recess]);
        for (p = leds) translate([fx(p[0]) - led_win[0] / 2, by(p[1]) - led_win[1] / 2, Zb])
            cube([led_win[0], led_win[1], Zl + lid_t - Zb - led_skin + (led_skin > 0 ? 0 : 1)]);
        for (s = ab) translate([fx(s[0]) - ab_slot[0] / 2, by(s[1]) - ab_slot[1] / 2, Zl - 1]) cube([ab_slot[0], ab_slot[1], lid_t + 2]);
        for (s = ab) translate([fx(s[0]) - 3.1, by(s[1]) - 5.35, Zl - 1]) cube([6.2, 10.7, 1 + recess]);
        for (h = holes) translate([fx(h[0]), by(h[1]), Zb]) cylinder(d = screw_d, h = H);
        for (i = [0 : 1]) translate([side_screw(i)[0], side_screw(i)[1], Zl - 1]) cylinder(d = screw_d, h = lid_t + 2);
        lid_text_cut();
        lid_wall_text_cut();
    }
}
module lid_print() { translate([0, W, H]) rotate([180, 0, 0]) lid(); }   // top face on the bed

// a strip of the connector skin with one DIN window (its top closed as the lid's tongue closes it) and one jack
// U-slot, printed standing as the walls print, on a foot: try a real SDS-50J plate behind it and the plugs
module front_test() {
    w = 20.5;
    difference() {
        union() { cube([2 * w, conn_t, 26]); translate([0, conn_t - 0.01, 0]) cube([2 * w, 10, 1.5]); }
        translate([w / 2 - din_win[0] / 2, -1, 2.5]) cube([din_win[0], conn_t + 2, din_win[1] - din_overlap]);
        translate([1.5 * w, 0, 8]) xz_extrude(-1, conn_t + 2) uslot2d(bush_slot, 30);
    }
}

// ---------------------------------------------------------------------------------------------------------------
module plates() { for (k = [0 : 3]) if (is_din(k)) at_ch(k) translate([-din_plate[0] / 2, 0, Zt - din_plate[1]]) cube([din_plate[0], din_plate[2], din_plate[1]]); }
module plates_moved(dy) { for (k = [0 : 3]) if (is_din(k)) at_ch(k) translate([-din_plate[0] / 2, dy, Zt - din_plate[1]]) cube([din_plate[0], din_plate[2], din_plate[1]]); }
module bodies_moved(dy) { for (k = [0 : 3]) if (!is_din(k)) at_ch(k) translate([-jack[0] / 2, dy, Zt - jack[2]]) cube([jack[0], jack[1], jack[2]]); }
module board_slab(dz) { translate([X0, Y0, Zt + dz]) cube([board_l, board_w, board_t]); }

if (part == "box") box();
else if (part == "lid") lid_print();
else if (part == "front_test") front_test();
else if (part == "mock") mock();
else if (part == "check_box") intersection() { box(); mock(); }
else if (part == "check_lid") intersection() { lid(); mock(); }
else if (part == "check_lidbox") intersection() { translate([0, 0, 0.01]) lid(); box(); }
else if (part == "check_text") {              // must be EMPTY (with markings = true): every label 0.6 mm clear of
    // every opening it is near, 0.8 inside the lid's edge / 0.5 inside the wall's ends, and 0.3 above the lid's recesses
    if (markings) {
        intersection() { lid_text_cut(); translate([-1, -1, -1]) cube([L + 2, W + 2, Zl + recess + 0.3 + 1]); }
        intersection() {
            translate([0, 0, -1]) linear_extrude(H + 2) offset(r = mark_clear) for (l = lid_labels()) label2d(l);
            translate([0, 0, -1]) linear_extrude(H + 2) lid_openings2d();
        }
        intersection() {
            for (l = wall_labels()) if (l[4] == "box") wall_label(l, 4, mark_clear);
            union() {
                for (k = [0 : 3]) at_ch(k) wall_cut(k);
                usb_cut(usb_xiao); usb_cut(usb_j1);
                translate([-1, -1, Zl - 0.5]) cube([L + 2, W + 2, 20]);
                translate([-1, -1, -1]) cube([L + 2, W + 2, 1.5]);
                for (x = [-10, L - 0.5]) translate([x, -1, -1]) cube([10.5, W + 2, H + 2]);
            }
        }
        difference() {      // the tongue labels must stay on their tongue, 0.6 mm in from its sides and bottom
            for (l = wall_labels()) if (l[4] == "lid") wall_label(l, mark_depth);
            for (k = [0 : 3]) if (is_din(k)) at_ch(k)
                translate([-din_win[0] / 2 + lid_play + mark_clear, -edge_gap - conn_t - 2, Zt - din_overlap + mark_clear]) cube([din_win[0] - 2 * lid_play - 2 * mark_clear, conn_t + 4, H]);
        }
    }
}
else if (part == "lid_top") {                 // the lid seen from above, the recessed labels' floors darkened
    color("LightSteelBlue") lid();
    if (markings) color("DimGray") translate([0, 0, H - mark_depth]) linear_extrude(0.05) for (l = lid_labels()) label2d(l);
}
else if (part == "check_drop")                 // must be EMPTY: the lid lowered straight down from 22 mm up
    intersection() { union() { for (dz = [0.25 : 0.25 : 22]) translate([0, 0, dz]) lid(); } union() { box(); mock(); } }
else if (part == "brace_pull") intersection() { box(); union() { plates_moved(-(edge_gap + 0.05)); bodies_moved(-(edge_gap + 0.05)); } }
else if (part == "brace_push") intersection() { box(); plates_moved(din_slot_play + 0.05); }
else if (part == "brace_trs") intersection() { box(); bodies_moved(jack_play + 0.05); }
else if (part == "brace_floor") intersection() { box(); translate([0, 0, -(din_floor_gap + 0.05)]) plates(); }
else if (part == "brace_lid") intersection() { lid(); board_slab(contact + 0.05); }
else if (part == "brace_ledge") intersection() { box(); board_slab(-(contact + 0.05)); }
else if (part == "open") { color("SteelBlue") box(); color("DarkGreen") mock(); color("LightSteelBlue") translate([0, 0, 30]) lid(); }
else if (part == "front" || part == "back") { color("SteelBlue") box(); color("LightSteelBlue") lid(); color("DarkGreen") mock(); }
else { color("SteelBlue", 0.5) box(); color("LightSteelBlue", 0.35) lid(); color("DarkGreen") mock(); }
echo(str("jank box ", outs, ": outer ", L, " x ", W, " x ", H, " mm (X x Y x Z, lid included); board face at Z ", Zb,
         ", lid underside ", Zl, "; encoder body top ", Zb + enc_body[2], ", shaft top ", Zb + enc_shaft[1]));
