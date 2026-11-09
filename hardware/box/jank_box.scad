// SPDX-License-Identifier: GPL-3.0-only
// jank box: the printed box of the jank board, modular v5c (jank-pcb-mod, 87.0 x 79.5 mm; hardware/pcb/README.md).
//
// The board's TOP (the lid side) carries the XIAO, the OLED, OK, BACK, the encoder, the LED, the MIDI jacks (XKB
// PJ-320D-B-SMT, beside their DINs), the CLOCK / RESET jacks (XKB PJ-320A, on the side edges) and the A/B switches;
// its BOTTOM carries the four DINs (HOOYA DIN-504B-M10) hanging 19.6 mm down: the board stands about 20 mm up.
//
// CONNECTORS AT THE WALL (the user's ruling 2026-11-08/09): on every edge the board's edge comes edge_in (1.0 mm)
// inside the box's outer face. The base's wall rises to the board's bottom, the lid's skirt comes down to its top, the
// board is clamped between them (and screwed at H1..H4). The MIDI jacks' bodies end 0.5 mm inside the outside, their
// bushings 1.5 proud; the DIN plates (1.0 behind the board's edge on the footprint) sit 2.0 behind the outside in a
// window with a 0.5 lip; the side jacks' bodies 1.0 inside, bushings 1.0 proud.
//
// FIRST REQUIREMENT (the user's ruling 2026-11-08): plugging and unplugging loads the box, not the solder joints.
//   DIN: pull-out -> the plate's front on the window's lip (sides, floor edge); push-in -> the body's back on a lip from
//        the floor; down -> the plate's far edge on the floor.
//   jack: pull-out -> the body's front on the skirt's web; push-in -> the body's back on the lid's cap.
//
// Frame: box outer corner at the origin, X along the front (left to right seen from the front, lid up), Y from the
// front to the back, Z up. KiCad x maps to X directly (the board lies top up); KiCad y is measured from the BACK edge.
// Every number marked UNVERIFIED is from a drawing or an estimate, not from a part in hand (README "Unverified").

/* [What to render] */
sj = 0;                 // the side jack a brace_side_* check looks at
part = "assembly";      // [assembly, box, lid, button_cap, slider_cap, slider_cap_smd, knob, front_test, mock, open, front, back, lid_top, caps_view, check_box, check_lid, check_lidbox, check_drop, check_board_drop, check_text, check_caps, brace_caps, brace_pull, brace_pull_jack, brace_push, brace_trs, brace_side_pull, brace_side_push, brace_floor, brace_lid, brace_ledge]

/* [Board v5c, modular: hardware/pcb (jank-pcb-mod.kicad_pcb, mockup-mod.stl)] */
board_l = 87.0;
board_w = 79.5;
board_t = 1.6;          // UNVERIFIED: JLCPCB default
din_x = [17.5, 10.45, 33.45, 56.45];      // OUT1..OUT4 DIN axes, KiCad x (OUT1 on the back edge)
jack_x = [6.0, 21.95, 44.95, 67.95];      // OUT1..OUT4 jack axes, beside the DINs (pitch 23.0)
ch_back = [true, false, false, false];
holes = [[83.8, 3.2], [83.8, 76.3], [3.2, 26.0], [3.2, 34.0]];   // H1..H4 (2.2 mm), KiCad x, y from the back
buttons = [[69.15, 25.45], [78.75, 25.45]];   // BACK (SW6), OK (SW1): HRO K2-1102DP-L3SW-04, 9.5 mm
enc = [73.9, 38.7];     // encoder shaft
enc_hole = 7.0;
oled_win = [45.0, 28.55, 24.4, 7.6];     // centre x, y, size x, y (the viewing area)
oled = [25.8, 63.8, 23.5, 35.5];         // the module: x0, x1, y0, y1 (pins at the x1 end)
oled_z = [2.5, 3.9, 5.1];                // above the board: its underside, its PCB's top, the glass top (UNVERIFIED split)
oled_glass_x0 = 28.3;
led = [33.0, 4.2];
led_win = [3.5, 3.7];
led_h = 1.95;
// A/B switches: [x, y, travel axis, knob across, knob along, travel, body height, knob top, body x, body y]
//   SW2 (OUT1): surface-mount slide switch, body 7.7 x 3.55 x 3.2, knob 3.0 tall, 1.5 wide, travel 1.5 along x
//   (the knob's length along x taken as 1.5: UNVERIFIED); SW3..SW5: MSS22D18G2 through-hole, handle 1.5 x 2.0, 2.0 along y
ab = [[17.5, 8.1, "x", 1.5, 1.5, 1.5, 3.2, 6.2, 7.7, 3.55],
      [10.45, 60.5, "y", 1.5, 2.0, 2.0, 3.5, 5.0, 5.59, 9.69],
      [33.45, 60.5, "y", 1.5, 2.0, 2.0, 3.5, 5.0, 5.59, 9.69],
      [56.45, 60.5, "y", 1.5, 2.0, 2.0, 3.5, 5.0, 5.59, 9.69]];
/* the printed caps (the user's answer 2026-11-08: "we can 3D print something") */
btn_cap = [5.4, 8.0, 1.0, 3.45, 3.0, 0.3];   // stem dia, flange dia, flange thickness, bore (plunger 3.4), proud of
                                             // the lid, flange gap under the lid at rest
slider_fit = 0.05;      // the slider cap's pocket over the knob: + 0.05 each way (printed small: press)
slider_wall = 0.8;
slider_proud = 1.0;     // the slider cap's top above the lid
slider_lift = 0.2;      // its bottom above the switch body
knob = [14.0, 12.0, 6.1, 4.6, 10.0];   // dia, height, D bore dia, across the flat, bore depth
enc_flat = 4.5;         // UNVERIFIED: the EC11's D shaft, 4.5 across the flat over its top 10 mm
function cap_size(c) = [c[3] + slider_fit + 2 * slider_wall, c[4] + slider_fit + 2 * slider_wall];   // across, along
function slot_size(c) = [cap_size(c)[0] + 0.35, cap_size(c)[1] + c[5] + 0.35];                     // across, along
sw_body_h = 3.5;        // UNVERIFIED: the buttons' body height
usb_xiao = [45.2, 5.35];    // XIAO socket: KiCad x, centre above the board's top (UNVERIFIED estimate)
usb_j1 = [64.5, 1.65];      // J1 USB4125: KiCad x, centre above the board's top (UNVERIFIED estimate)
usb_open = [12.5, 7.0];

/* [DIN: HOOYA DIN-504B-M10, drawing rev A1 2019-07-10] */
din_plate = [20.9, 19.6, 1.8];   // the plate: a flange the wall's slot grips
// the body behind the plate, SCALED OFF the drawing (33.5 px/mm, +/-0.2, not a caliper): 17.8 wide (the plate stands
// 1.55 past it a side), 17.2 high from the board side (the plate stands 2.4 past it at the far edge), 13.1 deep
din_body = [17.8, 17.2, 13.1];
din_in = 1.0;           // the plate's front 1.0 mm behind the board's edge (the footprint's DIN_BACK)
din_floor_gap = 0.1;
// THE SLOTS (the user's ruling 2026-11-09: "slots in the wall"): the front and back walls carry, round each DIN, a
// cheek in front of the plate, a 2.0 slot (the plate's 1.8 + 0.1 a face) and a cheek behind it; the plate slides down
// into the slot as the board goes in. No lip or wall behind the body.
din_face_in = 1.3;      // the plate's face behind the base's outer face: the front cheek 1.2 + 0.1 (the user, 2026-11-09:
                        // "make the wall thicker"; the base's face 0.3 proud of the board's edge)
slot_play = 0.1;        // a face, and at the plate's side edges
din_rim = [1.3, 2.2];   // how much of the plate's edge the cheeks hold: [each side, the far edge] (body overhang - 0.2)
back_cheek = 2.0;       // the cheek behind the plate
post_clear = 0.3;       // the posts behind the cheeks, beside the DIN bodies: this far from a body
post_stop = 1.7;        // the posts stop this far below the board (the jacks' pegs and leads, mockup)
chamfer = 1.0;          // 45-degree fillets where the back cheeks and the posts meet the floor

/* [Jacks] */
// MIDI: XKB PJ-320D-B-SMT (the mockup's block 6.2 x 12.2 x 5.0), its front 0.5 PAST the board's edge
// CLOCK / RESET: XKB PJ-320A (6.2 x 12.2 x 5.0), its front on the side edge
jack = [6.2, 12.2, 5.0];
jack_out = 0.5;         // the MIDI jacks' fronts past the board's edge
bush_d = 5.0;           // UNVERIFIED (measured on the sheet)
bush_out = 2.0;
bush_z = 2.5;
bush_slot = 5.2;
jack_play = 0.15;
cap_wall = 1.6;
cap_stop = 1.6;         // the lid's jack cap stops this far above the board: the DINs' legs (1.5) stand there
side_jacks = [[0, 39.85, "CLK"], [0, 46.55, "RST"], [1, 53.8, "CLK"], [1, 60.5, "RST"]];   // [0 left / 1 right, y, label]

/* [Box] */
edge_in = 1.0;          // the board's edges to the box's outer faces (all four edges carry connectors)
gap_out = 0.1;          // a connector's front to the wall in front of it
base_in = din_in - gap_out;   // the side walls reach 0.9 under the board
band_in = 1.3;          // the lid's skirt reaches 1.3 over the board
floor_t = 2.0;
contact = 0.05;
face_h = 7.2;           // the board's top to the lid's inside: the XIAO's 7.0 + 0.2
lid_t = 1.5;
standoff_d = 4.4;       // H3's column must miss R21
pilot_d = 1.7;
screw_d = 2.4;
pilot_depth = 11;       // M2 x 20 self-tapping: lid 1.5 + spacer 7.2 + board 1.6, 9.7 into the column
spacer_d = 5.0;
led_tube = 0.6;
led_skin = 0.6;
led_tube_gap = 0.3;
button_hole = btn_cap[0] + 0.6;   // the cap's stem through it, 0.3 a side
oled_rib = [26.0, 27.6];  // a rib in the lid over the module's far end (no glass there): x from / to
oled_rib_gap = 0.2;
// parts at the board's edges the skirt's band (top) or the base's band (bottom) must leave room for (mockup-mod,
// courtyards + 0.2): [x0, x1, y0, y1 (y from the back), z0, z1 (from the board's top / bottom)]
top_keep = [[35.05, 55.4, -0.2, 1.4, -1, face_h],       // the XIAO (and C5 under it), up to the lid's underside
            [58.9, 70.1, -0.8, 1.4, -1, 3.5],           // J1
            [30.7, 35.3, 0.75, 1.4, -1, 2.2]];          // LED1
bot_keep = [[35.05, 55.4, -0.2, 21.2, -3.2, 1],         // the XIAO's header pins
            [58.9, 70.1, -0.75, 8.0, -2.1, 1],          // J1's shell legs (0.55 past the edge in the mockup)
            [39.43, 45.01, 2.55, 6.0, -9.2, 1]];        // R8 standing under the board by the back wall

/* [Markings] */
markings = true;
mark_font = "Liberation Sans:style=Bold";
mark_size = 4.2;        // capitals 4.0 mm, stems 0.84 mm
mark_logo_size = 6.3;
mark_depth = 0.4;
mark_gap = 1.0;
mark_clear = 0.6;
ab_a_end = "back";      // UNVERIFIED which end of a through-hole A/B slot is A ("back" / "front")
ab_a_end_x = "right";   // UNVERIFIED which end of SW2's slot is A ("right" / "left", seen from above)

$fn = 64;

// ---------------------------------------------------------------------------------------------------------------
X0 = edge_in;                            // the board's left edge
Y0 = edge_in;                            // its front edge
Ybk = Y0 + board_w;                      // its back edge
Zbot = floor_t + din_floor_gap + din_plate[1];   // the board's bottom
Zb = Zbot + board_t;                     // its top
Zl = Zb + face_h;                        // the lid's underside
H = Zl + lid_t;
L = board_l + 2 * edge_in;
W = board_w + 2 * edge_in;
base_off = din_in - din_face_in;         // the base's front / back outer faces, inside the board's edge (0.2)
wall_d = din_in - slot_play + 2 * slot_play + din_plate[2] + back_cheek;   // the front / back wall's depth from the board's edge
function kx(x) = X0 + x;
function ky(y) = Ybk - y;
encC = [kx(enc[0]), ky(enc[1])];

// frames: origin on the board's edge at the axis, +y into the board
module at_din(k) { if (ch_back[k]) translate([kx(din_x[k]), Ybk, 0]) mirror([0, 1, 0]) children();
                   else translate([kx(din_x[k]), Y0, 0]) children(); }
module at_jack(k) { if (ch_back[k]) translate([kx(jack_x[k]), Ybk, 0]) mirror([0, 1, 0]) children();
                    else translate([kx(jack_x[k]), Y0, 0]) children(); }
module at_side(j) { s = side_jacks[j];
    if (s[0] == 0) translate([X0, ky(s[1]), 0]) rotate([0, 0, -90]) children();
    else translate([X0 + board_l, ky(s[1]), 0]) rotate([0, 0, 90]) children(); }
module xz_extrude(y0, len) { translate([0, y0 + len, 0]) rotate([90, 0, 0]) linear_extrude(len) children(); }
module dslot2d(d, down) { union() { circle(d = d); translate([-d / 2, -down]) square([d, down]); } }   // open downwards
module rect_ring(z0, z1, inset) {         // the outer rectangle minus the board inset by `inset`, from z0 to z1
    translate([0, 0, z0]) linear_extrude(z1 - z0) difference() {
        square([L, W]);
        translate([X0 + inset, Y0 + inset]) square([board_l - 2 * inset, board_w - 2 * inset]);
    }
}
module keep_cut(k, zref, out = 0) {      // a keep-out box (top_keep / bot_keep entry); out > 0 extends it outwards
    translate([kx(k[0]) - (k[0] < 1 ? out : 0), ky(k[3]), zref + k[4]])
        cube([k[1] - k[0] + (k[1] > board_l - 1 ? out : 0) + (k[0] < 1 ? out : 0), k[3] - k[2] + (k[2] < 1 ? out : 0), k[5] - k[4]]);
}

// ---------------------------------------------------------------------------------------------------------------
// The board model for the checks: ../pcb/mockup-mod.stl (both sides, z = 0 its bottom), the buttons, the A/B switches
// and the OLED redrawn (the mockup draws them as full-height blocks).
function ab_rot(c) = c[2] == "x" ? 90 : 0;     // the knob's travel along y in the switch's own frame
module mock_m() {       // mockup frame: x 100..187, y -179.5 (front) .. -100 (back)
    difference() {
        import("../pcb/mockup-mod.stl", convexity = 10);
        for (b = buttons) translate([100 + b[0] - 4.9, -100 - b[1] - 3.9, 1.5]) cube([9.8, 7.8, 10]);
        for (s = ab) translate([100 + s[0] - 4.3, -100 - s[1] - 5.0, 1.5]) cube([8.6, 10.0, 6]);
        translate([125.7, -135.6, 4.0]) cube([38.2, 12.2, 2.8]);
        for (k = [0 : 3]) translate([100 + din_x[k] - 10.55, ch_back[k] ? -100 - 0.9 - 15 : -100 - board_w, -25])   // the mockup's DIN blocks
            cube([21.1, 16, 25]);
    }
    for (k = [0 : 3]) translate([100 + din_x[k], ch_back[k] ? -100 : -100 - board_w, 0]) mirror([0, ch_back[k] ? 1 : 0, 0]) {
        translate([-din_plate[0] / 2, din_in, -din_plate[1]]) cube([din_plate[0], din_plate[2], din_plate[1] + 0.01]);     // the plate
        translate([-din_body[0] / 2, din_in + din_plate[2] - 0.01, -din_body[1]]) cube([din_body[0], din_body[2] - din_plate[2] + 0.01, din_body[1] + 0.01]);
    }
    for (k = [0 : 3]) translate([100 + jack_x[k], ch_back[k] ? -100 : -100 - board_w, 0]) mirror([0, ch_back[k] ? 1 : 0, 0])
        translate([-jack[0] / 2, 1.0, -1.5]) cube([jack[0], jack[1] - 1.5, 1.51]);   // the jacks' leads under the board (mockup)
    for (b = buttons) {
        translate([100 + b[0] - 4.795, -100 - b[1] - 3.795, 1.59]) cube([9.59, 7.59, sw_body_h + 0.01]);
        translate([100 + b[0], -100 - b[1], 1.59]) cylinder(d = 3.4, h = 9.51);
    }
    for (s = ab) translate([100 + s[0], -100 - s[1], 0]) {
        translate([-s[8] / 2, -s[9] / 2, 1.59]) cube([s[8], s[9], s[6] + 0.01]);                 // the body
        rotate([0, 0, ab_rot(s)]) translate([-s[3] / 2, -(s[4] + s[5]) / 2, 1.6 + s[6] - 0.01])
            cube([s[3], s[4] + s[5], s[7] - s[6] + 0.01]);                                          // the knob, whole travel
    }
    translate([100 + oled[0], -100 - oled[3], 1.6 + oled_z[0]]) cube([oled[1] - oled[0], oled[3] - oled[2], oled_z[1] - oled_z[0]]);
    translate([100 + oled_glass_x0, -100 - oled[3] + 0.22, 1.6 + oled_z[1] - 0.01]) cube([30.0, 11.56, oled_z[2] - oled_z[1] + 0.01]);
}
module mock() { translate([X0 - 100, Y0 + 100 + board_w, Zbot]) mock_m(); }

// ---------------------------------------------------------------------------------------------------------------
module din_slot() {     // round one DIN, in the front / back wall: the window, the slot, the opening behind
    zp = Zbot - din_plate[1];   // the plate's far edge
    translate([-din_plate[0] / 2 + din_rim[0], -edge_in - 1, zp + din_rim[1]]) cube([din_plate[0] - 2 * din_rim[0], edge_in + wall_d + 2, din_plate[1]]);
    translate([-din_plate[0] / 2 - slot_play, din_in - slot_play, floor_t]) cube([din_plate[0] + 2 * slot_play, din_plate[2] + 2 * slot_play, din_plate[1] + 1]);
}
module din_posts() {    // local frame: beside the body, behind the back cheek, back to the body's back; and the fillets
    pw = 23.0 - din_body[0] - 2 * post_clear;      // between two bodies at the 23.0 pitch
    yw = din_in + din_plate[2] + slot_play + back_cheek;   // the back cheek's back face
    y1 = din_in + din_body[2];
    for (sd = [-1, 1]) {
        x0 = sd > 0 ? din_body[0] / 2 + post_clear : -din_body[0] / 2 - post_clear - pw;
        translate([x0, yw - 0.01, floor_t - 0.01]) cube([pw, y1 - yw + 0.01, Zbot - post_stop - floor_t + 0.01]);
        // fillet along the post's side at the floor, under the body (its far edge 2.4 above the floor)
        xe = sd > 0 ? x0 : x0 + pw;
        hull() { translate([xe - (sd > 0 ? 0 : 0.01), yw - 0.01, floor_t - 0.01]) cube([0.01, y1 - yw + 0.01, chamfer]);
                 translate([sd > 0 ? xe - chamfer : xe, yw - 0.01, floor_t - 0.01]) cube([chamfer, y1 - yw + 0.01, 0.01]); }
    }
    // fillet along the back cheek's back face at the floor, under the body
    hull() { translate([-din_plate[0] / 2, yw - 0.01, floor_t - 0.01]) cube([din_plate[0], 0.01, chamfer]);
             translate([-din_plate[0] / 2, yw - 0.01, floor_t - 0.01]) cube([din_plate[0], chamfer, 0.01]); }
}
// a jack in its frame, its front at -f (f = jack_out for the MIDI jacks, 0 for the side ones)
module jack_slot(f) {   // through the lid's skirt, open downwards: the skirt comes down over the bushing
    translate([0, 0, Zb + bush_z]) xz_extrude(-edge_in - 1, edge_in + band_in + 1.01) dslot2d(bush_slot, bush_z + 1);
}
module jack_cavity(f) { // the jack's body in the skirt: the skirt keeps edge_in - f - gap_out in front of it
    translate([-jack[0] / 2 - jack_play, -f - gap_out, Zb - 1]) cube([jack[0] + 2 * jack_play, jack[1] + jack_play + gap_out, jack[2] + jack_play + 1]);
}
module lid_cap(f) {     // hangs from the lid round the jack's body: its back stop takes the push-in
    w = jack[0] / 2 + jack_play + cap_wall;
    yb = jack[1] - f + jack_play + cap_wall;
    difference() {
        translate([-w, 0, Zb + cap_stop]) cube([2 * w, yb, Zl - Zb - cap_stop + 0.01]);
        jack_cavity(f);
    }
}
module side_caps() {    // the side jacks' caps, one block per wall round both its jacks (they are 6.7 apart)
    w = jack[0] / 2 + jack_play + cap_wall; yb = jack[1] + jack_play + cap_wall;
    for (side = [0, 1]) difference() {
        hull() for (j = [0 : 3]) if (side_jacks[j][0] == side) at_side(j) translate([-w, 0, Zb + cap_stop]) cube([2 * w, yb, Zl - Zb - cap_stop + 0.01]);
        hull() for (j = [0 : 3]) if (side_jacks[j][0] == side) at_side(j) jack_cavity(0);
    }
}
module standoff(h) {
    c = [kx(h[0]), ky(h[1])];
    xw = c[0] < L / 2 ? X0 + base_in - 0.01 : X0 + board_l - base_in - 0.01;   // its web to the side wall's inside
    hull() {
        translate([c[0], c[1], floor_t - 0.01]) cylinder(d = standoff_d, h = Zbot - contact - floor_t + 0.01);
        translate([xw, c[1] - 1, floor_t - 0.01]) cube([0.02, 2, Zbot - contact - floor_t]);
    }
}
module usb_skirt_cut(u, top) {   // an opening in the lid's skirt at the back, open downwards
    translate([kx(u[0]) - usb_open[0] / 2, Ybk - band_in - 1, Zb - 1]) cube([usb_open[0], W - Ybk + band_in + 2, top - Zb + 1]);
}

module box() {          // the base: the floor, the side walls, the front and back walls with the DIN slots
    yf = Y0 + base_off; yb = Ybk - base_off;
    difference() {
        union() {
            translate([0, yf, 0]) cube([L, yb - yf, floor_t]);
            for (x = [0, X0 + board_l - base_in]) translate([x, yf, floor_t - 0.01]) cube([X0 + base_in, yb - yf, Zbot - contact - floor_t + 0.01]);
            translate([0, yf, floor_t - 0.01]) cube([L, wall_d - base_off, Zbot - contact - floor_t + 0.01]);             // front wall
            translate([0, Ybk - wall_d, floor_t - 0.01]) cube([L, wall_d - base_off, Zbot - contact - floor_t + 0.01]);  // back wall
            for (h = holes) standoff(h);
            intersection() {    // the posts behind the cheeks, between the DIN bodies (5.2 apart: 4.6 posts), tied to the floor
                translate([0, yf, 0]) cube([L, yb - yf, Zbot]);
                for (k = [0 : 3]) at_din(k) din_posts();
            }
        }
        for (k = [0 : 3]) at_din(k) din_slot();
        for (k = [0 : 3]) at_jack(k)     // the jacks' pegs and leads under the board (mockup), over the rib between plates
            translate([-jack[0] / 2 - 0.2, din_in - slot_play - 0.01, Zbot - 1.7]) cube([jack[0] + 0.4, jack[1], 2]);
        for (k = bot_keep) keep_cut(k, Zbot);
        wall_text_cut();
        for (h = holes) translate([kx(h[0]), ky(h[1]), Zbot - pilot_depth]) cylinder(d = pilot_d, h = pilot_depth + 1);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Markings: controls on the lid's top, the plugs on the walls, each once
capH = function(sz) sz * 0.955;
function abA(c) = let(s = slot_size(c), x = kx(c[0]), y = ky(c[1])) c[2] == "y"
    ? (ab_a_end == "back" ? [[x, y + s[1] / 2 + mark_gap, "center"], [x, y - s[1] / 2 - mark_gap - capH(mark_size), "center"]]
                          : [[x, y - s[1] / 2 - mark_gap - capH(mark_size), "center"], [x, y + s[1] / 2 + mark_gap, "center"]])
    : (ab_a_end_x == "right" ? [[x + s[1] / 2 + mark_gap, y - capH(mark_size) / 2, "left"], [x - s[1] / 2 - mark_gap, y - capH(mark_size) / 2, "right"]]
                             : [[x - s[1] / 2 - mark_gap, y - capH(mark_size) / 2, "right"], [x + s[1] / 2 + mark_gap, y - capH(mark_size) / 2, "left"]]);
function lid_labels() = let(
    mode = [kx(buttons[0][0]), ky(buttons[0][1])])
    concat([
        ["BACK", kx(73.5), ky(buttons[0][1]) + button_hole / 2 + mark_gap, "right", "baseline", mark_size],
        ["OK", kx(74.5), ky(buttons[1][1]) + button_hole / 2 + mark_gap, "left", "baseline", mark_size],
        ["BPM", encC[0], encC[1] - knob[0] / 2 - mark_gap - capH(mark_size), "center", "baseline", mark_size],   // under the knob's edge
        ["jank", kx(15.5), ky(31.5), "center", "baseline", mark_logo_size]],
        [for (c = ab) let(p = abA(c)) each [
            ["A", p[0][0], p[0][1], p[0][2], "baseline", mark_size], ["B", p[1][0], p[1][1], p[1][2], "baseline", mark_size]]]);
module label2d(l) { translate([l[1], l[2]]) text(l[0], size = l[5], font = mark_font, halign = l[3], valign = l[4]); }
module lid_text_cut() { if (markings) translate([0, 0, H - mark_depth]) linear_extrude(mark_depth + 1) for (l = lid_labels()) label2d(l); }
// wall labels: [text, x or y, z (baseline), wall (0 front, 1 back, 2 left, 3 right), halign, on ("lid" = the skirt,
// "box" = the base's wall)]; on the back, "right" runs towards +X; on the left wall text runs towards -Y, on the right +Y
side_off = jack[0] / 2 + jack_play + mark_gap;   // beside a jack: clear of its slot and of its cavity behind the skirt
function wall_labels() = concat(
    [for (k = [1 : 3]) [str(k + 1), kx(jack_x[k]) + side_off, Zb + 1.2, 0, "left", "lid"]],
    [["1 / IN", kx(jack_x[0]) + side_off, Zb + 1.2, 1, "right", "lid"],
     ["USB", kx(usb_xiao[0]), Zbot - 14.5, 1, "center", "box"],     // under the XIAO (below R8), on the base's wall
     ["PWR", kx(usb_j1[0]), Zbot - 7.5, 1, "center", "box"]],       // under J1
    [for (j = [0 : 3]) let(s = side_jacks[j], other = side_jacks[j % 2 == 0 ? j + 1 : j - 1],
                          away = ky(s[1]) > ky(other[1]) ? 1 : -1,          // +1: this jack is the one nearer the back
                          y = ky(s[1]) + away * side_off,
                          hal = (s[0] == 0) == (away > 0) ? "right" : "left")
        [s[2], y, Zb + 1.2, 2 + s[0], hal, "lid"]]);
module wall_label(l, depth, grow = 0) {
    if (l[3] >= 2) translate([l[3] == 2 ? depth : L - depth, l[1], l[2]]) rotate([0, 0, l[3] == 2 ? -90 : 90]) rotate([90, 0, 0])
        linear_extrude(depth + 1) offset(r = grow) text(l[0], size = mark_size, font = mark_font, halign = l[4], valign = "baseline");
    else if (l[3] == 0) translate([l[1], depth, l[2]]) rotate([90, 0, 0]) linear_extrude(depth + 1)
        offset(r = grow) text(l[0], size = mark_size, font = mark_font, halign = l[4], valign = "baseline");
    else translate([l[1], W - (l[5] == "box" ? edge_in + base_off : 0) - depth, l[2]]) rotate([90, 0, 180]) linear_extrude(depth + 1)
        offset(r = grow) text(l[0], size = mark_size, font = mark_font, halign = l[4], valign = "baseline");
}
module wall_text_cut() { if (markings) for (l = wall_labels()) if (l[5] == "box") wall_label(l, mark_depth); }
module skirt_text_cut() { if (markings) for (l = wall_labels()) if (l[5] == "lid") wall_label(l, mark_depth); }

module ab_slot2d(c) { s = slot_size(c); translate([kx(c[0]), ky(c[1])]) rotate(ab_rot(c)) square([s[0], s[1]], center = true); }
module lid_openings2d() {
    for (p = buttons) translate([kx(p[0]), ky(p[1])]) circle(d = button_hole);
    translate(encC) circle(d = knob[0]);   // the knob's footprint (over the 7.0 hole)
    translate([kx(oled_win[0]) - oled_win[2] / 2, ky(oled_win[1]) - oled_win[3] / 2]) square([oled_win[2], oled_win[3]]);
    translate([kx(led[0]) - led_win[0] / 2, ky(led[1]) - led_win[1] / 2]) square(led_win);
    for (c = ab) ab_slot2d(c);
    for (h = holes) translate([kx(h[0]), ky(h[1])]) circle(d = 4.0);
    translate([kx(usb_xiao[0]) - usb_open[0] / 2, Ybk - band_in - 1]) square([usb_open[0], W - Ybk + band_in + 2]);
    difference() { translate([-5, -5]) square([L + 10, W + 10]); offset(delta = -0.8) square([L, W]); }
}
module lid() {          // in place: the plate, the skirt down to the board's top, the caps, the spacers
    difference() {
        union() {
            translate([0, 0, Zl]) cube([L, W, lid_t]);
            rect_ring(Zb + contact, Zl + 0.01, band_in);         // the skirt: 1.0 outside the board, 1.3 over it
            for (k = [0 : 3]) at_jack(k) lid_cap(jack_out);
            side_caps();
            for (h = holes) translate([kx(h[0]), ky(h[1]), Zb + contact]) cylinder(d = spacer_d, h = Zl - Zb - contact + 0.01);
            difference() {      // the LED's light shroud, cut back where the XIAO (x 35.26 on) stands
                translate([kx(led[0]) - led_win[0] / 2 - led_tube, ky(led[1]) - led_win[1] / 2 - led_tube, Zb + led_h + led_tube_gap])
                    cube([led_win[0] + 2 * led_tube, led_win[1] + 2 * led_tube, Zl - Zb - led_h - led_tube_gap + 0.01]);
                translate([kx(35.255) - 0.2, Y0, Zb - 1]) cube([30, board_w, face_h + 2]);
            }
            translate([kx(oled_rib[0]), ky(oled[3]), Zb + oled_z[1] + oled_rib_gap])   // over the module's far end
                cube([oled_rib[1] - oled_rib[0], oled[3] - oled[2], Zl - Zb - oled_z[1] - oled_rib_gap + 0.01]);
        }
        for (k = [0 : 3]) at_jack(k) { jack_slot(jack_out); jack_cavity(jack_out); }
        for (j = [0 : 3]) at_side(j) { jack_slot(0); jack_cavity(0); }
        usb_skirt_cut(usb_xiao, H + 1);                          // the XIAO's socket stands out: open to the top
        usb_skirt_cut(usb_j1, Zb + usb_j1[1] + usb_open[1] / 2);
        for (k = top_keep) keep_cut(k, Zb);
        for (p = buttons) translate([kx(p[0]), ky(p[1]), Zl - 1]) cylinder(d = button_hole, h = lid_t + 2);
        translate([encC[0], encC[1], Zl - 1]) cylinder(d = enc_hole, h = lid_t + 2);
        translate([kx(oled_win[0]) - oled_win[2] / 2, ky(oled_win[1]) - oled_win[3] / 2, Zl - 1]) cube([oled_win[2], oled_win[3], lid_t + 2]);
        translate([kx(led[0]) - led_win[0] / 2, ky(led[1]) - led_win[1] / 2, Zb])
            cube([led_win[0], led_win[1], H - Zb - led_skin + (led_skin > 0 ? 0 : 1)]);
        for (c = ab) translate([0, 0, Zl - 1]) linear_extrude(lid_t + 2) ab_slot2d(c);
        for (h = holes) translate([kx(h[0]), ky(h[1]), Zb]) cylinder(d = screw_d, h = H);
        lid_text_cut();
        skirt_text_cut();
    }
}
// ---------------------------------------------------------------------------------------------------------------
// The printed caps, in place (box frame). Button cap: the bore's floor on the plunger's top (Zb + 9.5).
module button_cap(p, press = 0) {
    top = H + btn_cap[4];
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
module slider_cap(c, pos = 0) {   // pos: -1 / 0 / +1 = one end of the travel, the middle, the other end
    cs = cap_size(c);
    z0 = Zb + c[6] + slider_lift;
    translate([kx(c[0]), ky(c[1]), 0]) rotate([0, 0, ab_rot(c)]) translate([0, pos * c[5] / 2, 0]) difference() {
        translate([-cs[0] / 2, -cs[1] / 2, z0]) cube([cs[0], cs[1], H + slider_proud - z0]);
        translate([-(c[3] + slider_fit) / 2, -(c[4] + slider_fit) / 2, z0 - 1]) cube([c[3] + slider_fit, c[4] + slider_fit, Zb + c[7] - z0 + 1]);
    }
}
module knob_at(c, pos) {          // the switch's knob at one end of its travel (for the checks)
    translate([kx(c[0]), ky(c[1]), 0]) rotate([0, 0, ab_rot(c)]) translate([-c[3] / 2, pos * c[5] / 2 - c[4] / 2, Zb + c[6]]) cube([c[3], c[4], c[7] - c[6]]);
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

// a strip of the front edge as built: the base's wall with one DIN slot (window, the 2.0 slot, the cheek behind), the
// 1.6 mm gap the board's edge fills, the skirt (1.2 further out) with the jack beside it (its slot, open downwards,
// and the web in front of its body); end posts join them into one piece; printed standing on a foot
module front_test() {
    dx = 13.0; jx = dx + 11.5; w = jx + 5.0; p = 2.0;          // the DIN's and the jack's axes along the strip
    zb0 = din_plate[1] + 0.6; zt0 = zb0 + board_t; e = edge_in;   // e: the board's edge in strip coordinates (y)
    yf = e + base_off; ts = e + band_in;
    difference() {
        union() {
            translate([p, yf, 0]) cube([w, wall_d - base_off, zb0]);               // the front wall
            intersection() { translate([p, 0, 0]) cube([w, 40, zb0]); translate([p + dx, e, -floor_t]) din_posts_at(zb0); }
            translate([p, 0, zt0]) cube([w, ts, face_h]);                         // the skirt
            for (x = [0, w + p]) cube([p, e + wall_d, zt0 + face_h]);             // the end posts
            translate([0, yf, 0]) cube([w + 2 * p, 12, 1.5]);                      // the foot
        }
        translate([p + dx, e, 0]) din_slot_at(zb0);
        translate([p + jx, 0, zt0 + bush_z]) xz_extrude(-1, ts + 2) dslot2d(bush_slot, bush_z + 1);
        translate([p + jx - jack[0] / 2 - jack_play, e - jack_out - gap_out, zt0 - 1]) cube([jack[0] + 2 * jack_play, ts, jack[2] + jack_play + 1]);
    }
}
module din_posts_at(zb0) {   // din_posts() in the strip (the floor at 0, the board's bottom at zb0)
    pw = 23.0 - din_body[0] - 2 * post_clear; yw = din_in + din_plate[2] + slot_play + back_cheek; y1 = din_in + din_body[2];
    for (sd = [-1, 1]) translate([sd > 0 ? din_body[0] / 2 + post_clear : -din_body[0] / 2 - post_clear - pw, yw - 0.01, floor_t])
        cube([pw, y1 - yw + 0.01, zb0 - post_stop]);
}
module din_slot_at(zb0) {   // the same cut as din_slot(), with the board's bottom at zb0 and the floor at 0
    zp = 0.6;
    translate([-din_plate[0] / 2 + din_rim[0], -edge_in - 1, zp + din_rim[1]]) cube([din_plate[0] - 2 * din_rim[0], edge_in + wall_d + 2, din_plate[1]]);
    translate([-din_plate[0] / 2 - slot_play, din_in - slot_play, 0.5]) cube([din_plate[0] + 2 * slot_play, din_plate[2] + 2 * slot_play, din_plate[1] + 1]);
}

// ---------------------------------------------------------------------------------------------------------------
module din_plate_m() { translate([-din_plate[0] / 2, din_in, Zbot - din_plate[1]]) cube([din_plate[0], din_plate[2], din_plate[1]]); }
module din_model() { din_plate_m(); translate([-din_body[0] / 2, din_in + din_plate[2], Zbot - din_body[1]]) cube([din_body[0], din_body[2] - din_plate[2], din_body[1]]); }
module jack_body(f) { translate([-jack[0] / 2, -f, Zb]) cube([jack[0], jack[1], jack[2]]); }
module board_slab(dz) { translate([X0, Y0, Zbot + dz]) cube([board_l, board_w, board_t]); }

if (part == "box") box();
else if (part == "button_cap") translate([0, 0, -(Zl - btn_cap[5] - btn_cap[2])]) translate([-kx(buttons[0][0]), -ky(buttons[0][1]), 0]) button_cap(buttons[0]);   // flange on the bed
else if (part == "slider_cap") translate([0, 0, H + slider_proud]) rotate([180, 0, 0]) translate([-kx(ab[1][0]), -ky(ab[1][1]), 0]) slider_cap(ab[1]);   // top on the bed
else if (part == "slider_cap_smd") translate([0, 0, H + slider_proud]) rotate([180, 0, 0]) translate([-kx(ab[0][0]), -ky(ab[0][1]), 0]) slider_cap(ab[0]);
else if (part == "knob") translate([0, 0, Zb + 20 - knob[4] + knob[1]]) rotate([180, 0, 0]) translate([-encC[0], -encC[1], 0]) enc_knob();   // top on the bed
else if (part == "check_caps") {   // must be EMPTY: the caps against the lid at rest, pressed 0.3, the sliders at both ends;
    // against the board (the sliders' pockets round their knob, the knob's bore round the D shaft)
    intersection() { lid(); union() { caps(0, 0); caps(0.3, -1); caps(0.3, 1); } }
    intersection() { union() { for (b = buttons) { button_cap(b); button_cap(b, 0.3); } }
                     difference() { mock(); for (b = buttons) translate([kx(b[0]), ky(b[1]), Zb + 3]) cylinder(d = 3.41, h = 7); } }
    intersection() { union() { for (c = ab) for (q = [-1, 1]) slider_cap(c, q); }
                     difference() { mock(); for (c = ab) translate([kx(c[0]), ky(c[1]), 0]) rotate([0, 0, ab_rot(c)])
                         translate([-c[3] / 2 - 0.05, -(c[4] + c[5]) / 2 - 0.05, Zb + c[6] - 0.01]) cube([c[3] + 0.1, c[4] + c[5] + 0.1, 5]); } }
    intersection() { enc_knob(); union() { difference() { mock(); translate([encC[0], encC[1], Zb + 6.4]) cylinder(d = 6.2, h = 15); } enc_shaft_d(); box(); } }
    for (c = ab) for (q = [-1, 1]) intersection() { slider_cap(c, q); knob_at(c, q); }   // the knob in its pocket at each end
}
else if (part == "brace_caps")   // must NOT be empty: a button cap pulled up 0.35 meets the lid (the flange keeps it)
    intersection() { lid(); translate([0, 0, btn_cap[5] + 0.05]) for (b = buttons) button_cap(b); }
else if (part == "lid") lid_print();
else if (part == "front_test") front_test();
else if (part == "mock") mock();
else if (part == "check_box") intersection() { box(); mock(); }
else if (part == "check_lid") intersection() { lid(); mock(); }
else if (part == "check_lidbox") intersection() { lid(); box(); }   // they never touch: the board is between them
else if (part == "check_drop") intersection() { union() { for (dz = [0.25 : 0.25 : 22]) translate([0, 0, dz]) lid(); } union() { box(); mock(); for (b = buttons) button_cap(b); for (c = ab) slider_cap(c); } }
else if (part == "check_board_drop") intersection() { union() { for (dz = [0.5 : 0.5 : 26]) translate([0, 0, dz]) mock(); } box(); }
else if (part == "check_text") {
    if (markings) {
        intersection() { lid_text_cut(); translate([-1, -1, -1]) cube([L + 2, W + 2, Zl + 0.3 + 1]); }
        intersection() {
            translate([0, 0, -1]) linear_extrude(H + 2) offset(r = mark_clear) for (l = lid_labels()) label2d(l);
            translate([0, 0, -1]) linear_extrude(H + 2) lid_openings2d();
        }
        for (side = [false, true]) intersection() {   // the skirt's labels: clear of the slots, the cavities and keep-outs behind them, the edges
            for (l = wall_labels()) if (l[5] == "lid" && (l[3] >= 2) == side) wall_label(l, 3, mark_clear);
            union() {
                for (k = [0 : 3]) at_jack(k) { jack_slot(jack_out); translate([0, -edge_in - 1, 0]) jack_cavity(jack_out); }
                for (j = [0 : 3]) at_side(j) { jack_slot(0); translate([0, -edge_in - 1, 0]) jack_cavity(0); }
                usb_skirt_cut(usb_xiao, H + 1); usb_skirt_cut(usb_j1, Zb + usb_j1[1] + usb_open[1] / 2);
                for (k = top_keep) keep_cut(k, Zb, 3);
                translate([-1, -1, Zl - 0.5]) cube([L + 2, W + 2, 20]);
                translate([-1, -1, -20]) cube([L + 2, W + 2, Zb + 20.3]);
                if (side) for (y = [-10, W - 0.5]) translate([-1, y, -1]) cube([L + 2, 10.5, H + 2]);   // the side walls' ends
                else for (x = [-10, L - 0.5]) translate([x, -1, -1]) cube([10.5, W + 2, H + 2]);       // the front / back's ends
            }
        }
        intersection() {    // the base's labels
            for (l = wall_labels()) if (l[5] == "box") wall_label(l, 3, mark_clear);
            union() {
                for (k = [0 : 3]) at_din(k) din_slot();
                for (k = bot_keep) keep_cut(k, Zbot, 3);
                translate([-1, -1, Zbot - 0.3]) cube([L + 2, W + 2, 20]);
                translate([-1, -1, -1]) cube([L + 2, W + 2, floor_t + 1.5]);
                for (x = [-10, L - 0.5]) translate([x, -1, -1]) cube([10.5, W + 2, H + 2]);
            }
        }
    }
}
else if (part == "show_slot") {   // the base cut to OUT3's DIN (x 17..52, y < 16), the plate in red in its slot
    color("SteelBlue") intersection() { box(); translate([17, -1, -1]) cube([35, 17, H]); }
    color("Red") at_din(2) din_plate_m();
}
else if (part == "caps_view") { color("LightSteelBlue") lid(); color("Orange") caps(); color("SteelBlue") box(); }
else if (part == "lid_top") {
    color("LightSteelBlue") lid();
    color("Orange") caps();
    if (markings) color("DimGray") translate([0, 0, H - mark_depth]) linear_extrude(0.05) for (l = lid_labels()) label2d(l);
}
else if (part == "brace_pull") intersection() { box(); for (k = [0 : 3]) at_din(k) translate([0, -(slot_play + 0.05), 0]) din_plate_m(); }   // into the cheek in front
else if (part == "brace_pull_jack") intersection() { lid(); for (k = [0 : 3]) at_jack(k) translate([0, -(gap_out + 0.05), 0]) jack_body(jack_out); }
else if (part == "brace_push") intersection() { box(); for (k = [0 : 3]) at_din(k) translate([0, slot_play + 0.05, 0]) din_plate_m(); }   // into the cheek behind
else if (part == "brace_trs") intersection() { lid(); for (k = [0 : 3]) at_jack(k) translate([0, jack_play + 0.05, 0]) jack_body(jack_out); }
// must NOT be empty, for each side jack sj (0..3): pulled out it meets the skirt, pushed in the lid's cap
else if (part == "brace_side_pull") intersection() { lid(); at_side(sj) translate([0, -(gap_out + 0.05), 0]) jack_body(0); }
else if (part == "brace_side_push") intersection() { lid(); at_side(sj) translate([0, jack_play + 0.05, 0]) jack_body(0); }
else if (part == "brace_floor") intersection() { box(); for (k = [0 : 3]) at_din(k) translate([0, 0, -(din_floor_gap + 0.05)]) din_model(); }
else if (part == "brace_lid") intersection() { lid(); board_slab(contact + 0.05); }
else if (part == "brace_ledge") intersection() { box(); board_slab(-(contact + 0.05)); }
else if (part == "open") { color("SteelBlue") box(); color("DarkGreen") mock(); color("LightSteelBlue") translate([0, 0, 30]) lid(); }
else if (part == "front" || part == "back") { color("SteelBlue") box(); color("LightSteelBlue") lid(); color("DarkGreen") mock(); }
else { color("SteelBlue", 0.5) box(); color("LightSteelBlue", 0.35) lid(); color("DarkGreen") mock(); }
echo(str("jank box (modular v5c): outer ", L, " x ", W, " x ", H, " mm (X x Y x Z, lid included); board bottom at Z ", Zbot,
         ", top ", Zb, ", lid underside ", Zl));
