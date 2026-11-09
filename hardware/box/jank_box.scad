// SPDX-License-Identifier: GPL-3.0-only
// jank box: the printed box of the jank board, modular v5c (jank-pcb-mod, 87.0 x 79.5 mm; hardware/pcb/README.md).
//
// The board's TOP (the lid side) carries the XIAO, the OLED, OK, BACK, the encoder, the LED, the MIDI jacks (XKB
// PJ-320D-B-SMT, beside their DINs), the CLOCK / RESET jacks (XKB PJ-320A, on the side edges) and the A/B switches;
// its BOTTOM carries the four DINs (HOOYA DIN-504B-M10) hanging 19.6 mm down: the board stands about 20 mm up.
//
// CONNECTORS AT THE WALL (the user's ruling 2026-11-08/09): on every edge the board's edge comes edge_in (1.0 mm)
// inside the box's outer face (the lid and the base flush on all four sides, 2026-11-09). The base's wall rises to the
// board's bottom and its rim round the edge to 1.6 over its top; the lid's skirt comes down to the board's top inside the
// rim; the board is clamped between them (and screwed at H1..H4). See THE JOINT below. The MIDI jacks' bodies end 0.5 mm inside the outside, their
// bushings 1.5 proud; the DIN plates (1.0 behind the board's edge on the footprint) sit 2.0 behind the outside in a
// window with a 0.5 lip; the side jacks' bodies 1.0 inside, bushings 1.0 proud.
//
// FIRST REQUIREMENT (the user's ruling 2026-11-08): plugging and unplugging loads the box, not the solder joints.
//   DIN: pull-out -> the plate's front on the window's lip (sides, floor edge); push-in -> the body's back on a lip from
//        the floor; down -> the plate's far edge on the floor.
//   jack: pull-out -> the body's front on the web (the skirt's above the axis, the base's tongue below); push-in -> the
//        body's back on the lid's cap.
//
// Frame: box outer corner at the origin, X along the front (left to right seen from the front, lid up), Y from the
// front to the back, Z up. KiCad x maps to X directly (the board lies top up); KiCad y is measured from the BACK edge.
// Every number marked UNVERIFIED is from a drawing or an estimate, not from a part in hand (README "Unverified").

/* [What to render] */
sj = 0;                 // the side jack a brace_side_* check looks at
part = "assembly";      // [assembly, box, lid, button_cap, ab_tab, ab_tab_smd, ab_grip, knob, tab_view, tab_print, front_test, mock, open, front, back, joint, lid_top, caps_view, check_box, check_lid, check_lidbox, check_drop, check_board_drop, check_text, check_caps, check_usb, brace_caps, brace_pull, brace_pull_jack, brace_push, brace_trs, brace_side_pull, brace_side_push, brace_floor, brace_lid, brace_ledge, brace_reg, brace_snap]

/* [Board v5c, modular: hardware/pcb (jank-pcb-mod.kicad_pcb, mockup-mod.stl)] */
board_l = 87.0;
board_w = 79.5;
board_t = 1.6;          // UNVERIFIED: JLCPCB default
din_x = [17.5, 10.45, 33.45, 56.45];      // OUT1..OUT4 DIN axes, KiCad x (OUT1 on the back edge)
jack_x = [6.0, 21.95, 44.95, 67.95];      // OUT1..OUT4 jack axes, beside the DINs (pitch 23.0)
ch_back = [true, false, false, false];
holes = [[83.8, 3.2], [83.8, 76.3], [3.2, 26.0], [3.2, 34.0]];   // H1..H4 (2.2 mm), KiCad x, y from the back
buttons = [[69.15, 25.45], [78.75, 25.45]];   // BACK (SW6), OK (SW1): HRO K2-1102DP-K4SW-04, 9.0 mm (was the 9.5 L3)
sw_h = 9.0;             // the buttons' plunger top above the board
enc = [73.9, 38.7];     // encoder shaft
enc_hole = 7.0;
// the 0.96" 128 x 64 OLED module HS96L03W2C03 (the board's commit 0172784): 27.3 x 27.8, on a 1 x 4 header 2.5 up
oled_win = [38.39, 34.36, 22.76, 11.88]; // centre x, y, size x, y: the viewing area 22.74 x 11.86 + 0.02 (as before)
oled = [24.74, 52.04, 22.56, 50.36];     // the module: x0, x1, y0, y1 (y from the back; its pins along its back edge)
oled_z = [2.5, 3.5, 5.15];               // above the board: its underside, its PCB's top (PCB 1.0), the glass top
oled_glass = [26.02, 50.76, 27.43, 44.33];   // the glass (24.74 x 16.90): x0, x1, y0, y1
oled_holes = [[26.74, 24.56], [50.04, 24.56], [26.74, 48.36], [50.04, 48.36]];   // its four 2.5 mm holes
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
// THE A/B TABS (the user, 2026-11-09: "the current option is just not working"): a captured sliding tab per switch, in
// two printed parts. The TAB: a block under the lid (the flange: wider than the lid's slot on every side, so it cannot
// come out upwards) with the pocket over the switch's handle, a neck through the slot, a post above the lid. The GRIP
// (6 x 4, ridged) is pressed onto the post after the lid is on: a one-piece tab cannot pass a slot narrower than both
// its ends. Vertically the tab rests on the handle's top (the pocket's roof), 0.2 under the lid and 0.2 over the body.
tab_neck = [4.0, 2.4];  // the neck in the slot: across, along the travel
tab_slot_clear = [0.15, 0.2];   // the slot round the neck: a side across (it guides), at each end of the travel
tab_overlap = 1.0;      // the flange past the slot's edges, at every position of the travel
tab_lift = 0.2;         // the tab's bottom above the switch's body
tab_top_gap = 0.2;      // the flange's top under the lid
tab_fit = [0.3, 0.1];   // the pocket over the handle, in all: across (loose: the slot and the flange guide), along (drives)
grip = [6.0, 4.0, 1.8, 0.4];       // across, along, height, top chamfer (bottom chamfer 0.3)
grip_gap = 0.2;         // the grip's bottom (the neck's shoulder) above the lid
grip_post = [3.0, 1.8, 1.4];       // the post on the neck, under the grip: across, along, height (the grip's socket the same: press)
grip_ridge = [3, 0.5, 0.35, 1.2];  // ridges across the travel on the grip's top: count, width, height, pitch
knob = [14.0, 12.0, 6.1, 4.6, 10.0];   // dia, height, D bore dia, across the flat, bore depth
enc_flat = 4.5;         // UNVERIFIED: the EC11's D shaft, 4.5 across the flat over its top 10 mm
function slot_size(c) = [tab_neck[0] + 2 * tab_slot_clear[0], tab_neck[1] + c[5] + 2 * tab_slot_clear[1]];   // across, along
function flange_size(c) = [slot_size(c)[0] + 2 * tab_overlap, slot_size(c)[1] + c[5] + 2 * tab_overlap];
sw_body_h = 3.5;        // UNVERIFIED: the buttons' body height
// USB-C (2026-11-09, the user: "one hole per USB connector"): each socket gets ONE opening of its own.
//   XIAO (U1): the module's PCB ends at the board's back edge (footprint F.Fab 0.0 to 21.0 in from it); only its USB-C
//   socket (9.0 wide, 3.3 high, top 7.0 above the board: mockup, UNVERIFIED) stands 1.5 past the board's edge (footprint
//   silk, mockup 1.545), so 0.5 PAST the box's back face: a plug's overmold stays outside, the hole only clears the socket.
//   J1: its mouth 0.035 past the board's edge, 0.965 behind the box's face: a recess for the plug's overmold to reach it.
usb_xiao = [45.2, 5.35, 9.0, 3.3, 1.545];   // KiCad x, socket centre above the board's top, width, height, mouth past the edge
usb_j1 = [64.5, 1.6, 8.94, 3.2, 0.035];     // J1 SHOU HAN TYPE-C 6P (C456012): the same (centre and height from the mockup)
xiao_clear = 0.4;           // the XIAO socket's hole: 0.4 a side round the socket (9.8 x 4.1)
plug_mold = [12.5, 7.0];    // ASSUMED typical USB-C plug overmold, width x height, centred on the plug (UNVERIFIED: cables vary)
plug_clear = 0.25;          // round it, a side: J1's recess 13.0 x 7.5
plug_r = 1.5;               // the overmold's corner radius (ASSUMED); the recess's: + plug_clear

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
din_face_in = 2.0;      // the plate's face behind the base's outer face: the front cheek 1.9 + 0.1 (the user, 2026-11-09:
                        // "the lid seems bigger than the base": the base's face flush with the lid's, 1.0 past the board's edge)
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
jack_play = 0.15;
cap_wall = 1.6;
cap_stop = 1.6;         // the lid's jack cap stops this far above the board: the DINs' legs (1.5) stand there
side_jacks = [[0, 39.85, "CLK"], [0, 46.55, "RST"], [1, 53.8, "CLK"], [1, 60.5, "RST"]];   // [0 left / 1 right, y, label]

/* [Box] */
edge_in = 1.0;          // the board's edges to the box's outer faces (all four edges carry connectors)
gap_out = 0.1;          // a connector's front to the wall in front of it
base_in = din_in - gap_out;   // the side walls reach 0.9 under the board
band_in = 1.3;          // the lid's skirt reaches 1.3 over the board
// THE JOINT (the user, 2026-11-09: "an alignment / locking system; the screws just lock it"): the base's RIM rises round
// the board's edge (outside it) to rim_h above the board's top; the lid's skirt has a rebate over it, its inner band (over
// the board) inside the rim with reg_gap a side: the lid registers on the rim. At every 3.5 mm jack the rim rises to the
// jack's axis (a TONGUE) and holds the lower half of a round hole, the lid the upper half ("round on lid and base").
// Four light snap catches: a 45-degree bump on the lid's band into a groove in the rim's inside. FDM clearances 0.2.
rim_gap = 0.2;          // the rim's inside to the board's edge (and the lid's band outside = the board's edge: reg_gap)
reg_gap = 0.2;          // the lid's band to the rim's inside, a side
rim_h = 1.6;            // the rim's top above the board's top
reg_top = 0.1;          // the rim's / the tongues' top to the lid above them
jack_hole = 5.4;        // the jacks' round hole: the bushing 5.0 (UNVERIFIED) + 0.2 a side
tongue_side = 1.6;      // the tongue beside the jack's body cavity, a side
tongue_clear = 0.2;     // the lid's notch round a tongue, a side
catch_c = 0.8;          // the catch's centre above the board's top
catch_len = 8.0;
catch_bump = 0.35;      // the bump's height off the lid's band (45 degrees): 0.15 into the rim on the way down
catch_groove = 0.35;    // the groove's depth into the rim's inside (45 degrees): 0.45 of rim behind it
catches = [[0, 34.45], [1, 21.0], [2, 69.5], [3, 61.5]];   // [wall (0 front, 1 back, 2 left, 3 right), box X or Y]
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
oled_post = [4.0, 0.2, 2.1, 0.8];   // lid posts onto the module's PCB at its holes: dia, gap above the PCB, pin dia, pin length
                                    // (the pin enters the 2.5 hole 0.6 deep: it keeps the module from shifting or lifting)
// parts at the board's edges the skirt's band (top) or the base's band (bottom) must leave room for (mockup-mod,
// courtyards + 0.2): [x0, x1, y0, y1 (y from the back), z0, z1 (from the board's top / bottom)]
top_keep = [[35.05, 55.4, -0.2, 1.4, -1, face_h],       // the XIAO (and C5 under it), up to the lid's underside
            [58.9, 70.1, -0.8, 1.4, -1, 3.5],           // J1
            [30.7, 35.3, 0.75, 1.4, -1, 2.2]];          // LED1
bot_keep = [[35.05, 55.4, -0.2, 21.2, -3.2, 1],         // the XIAO's header pins
            [58.9, 70.1, -0.25, 8.0, -2.1, 1],          // J1's legs (the mockup's block 0.035 past the edge; pads 1.65 in)
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
base_off = din_in - din_face_in;         // the base's front / back outer faces, inside the board's edge (-1.0: flush)
wall_d = din_in - slot_play + 2 * slot_play + din_plate[2] + back_cheek;   // the front / back wall's depth from the board's edge
function kx(x) = X0 + x;
function ky(y) = Ybk - y;
encC = [kx(enc[0]), ky(enc[1])];
tongue_w = jack[0] + 2 * jack_play + 2 * tongue_side;   // 9.7
rim_t = edge_in - rim_gap;               // 0.8

// frames: origin on the board's edge at the axis, +y into the board
module at_din(k) { if (ch_back[k]) translate([kx(din_x[k]), Ybk, 0]) mirror([0, 1, 0]) children();
                   else translate([kx(din_x[k]), Y0, 0]) children(); }
module at_jack(k) { if (ch_back[k]) translate([kx(jack_x[k]), Ybk, 0]) mirror([0, 1, 0]) children();
                    else translate([kx(jack_x[k]), Y0, 0]) children(); }
module at_side(j) { s = side_jacks[j];
    if (s[0] == 0) translate([X0, ky(s[1]), 0]) rotate([0, 0, -90]) children();
    else translate([X0 + board_l, ky(s[1]), 0]) rotate([0, 0, 90]) children(); }
module at_wall(w, s) {   // origin on the outer face at s along it, +y inwards
    if (w == 0) translate([s, 0, 0]) children();
    else if (w == 1) translate([s, W, 0]) mirror([0, 1, 0]) children();
    else if (w == 2) translate([0, s, 0]) rotate([0, 0, -90]) children();
    else translate([L, s, 0]) rotate([0, 0, 90]) children(); }
module xz_extrude(y0, len) { translate([0, y0 + len, 0]) rotate([90, 0, 0]) linear_extrude(len) children(); }
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
        translate([100 + oled[0] - 0.1, -100 - oled[3] - 0.1, 1.6 + oled_z[0] - 0.1]) cube([oled[1] - oled[0] + 0.2, oled[3] - oled[2] + 0.2, oled_z[2] - oled_z[0] + 0.2]);
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
        translate([100 + b[0], -100 - b[1], 1.59]) cylinder(d = 3.4, h = sw_h + 0.01);
    }
    for (s = ab) translate([100 + s[0], -100 - s[1], 0]) {
        translate([-s[8] / 2, -s[9] / 2, 1.59]) cube([s[8], s[9], s[6] + 0.01]);                 // the body
        rotate([0, 0, ab_rot(s)]) translate([-s[3] / 2, -(s[4] + s[5]) / 2, 1.6 + s[6] - 0.01])
            cube([s[3], s[4] + s[5], s[7] - s[6] + 0.01]);                                          // the knob, whole travel
    }
    difference() {      // the module's PCB with its four holes, and the glass on it
        translate([100 + oled[0], -100 - oled[3], 1.6 + oled_z[0]]) cube([oled[1] - oled[0], oled[3] - oled[2], oled_z[1] - oled_z[0]]);
        for (h = oled_holes) translate([100 + h[0], -100 - h[1], 1.6 + oled_z[0] - 1]) cylinder(d = 2.5, h = 3);
    }
    translate([100 + oled_glass[0], -100 - oled_glass[3], 1.6 + oled_z[1] - 0.01]) cube([oled_glass[1] - oled_glass[0], oled_glass[3] - oled_glass[2], oled_z[2] - oled_z[1] + 0.01]);
}
module mock() { translate([X0 - 100, Y0 + 100 + board_w, Zbot]) mock_m(); }

// ---------------------------------------------------------------------------------------------------------------
module din_slot() {     // round one DIN, in the front / back wall: the window, the slot, the opening behind
    zp = Zbot - din_plate[1];   // the plate's far edge
    translate([-din_plate[0] / 2 + din_rim[0], -edge_in - 1, zp + din_rim[1]]) cube([din_plate[0] - 2 * din_rim[0], edge_in + wall_d + 2, Zbot - contact - zp - din_rim[1]]);
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
module jack_hole() {    // the round hole round the bushing, half in the base's tongue, half in the lid
    translate([0, 0, Zb + bush_z]) xz_extrude(-edge_in - 1, edge_in + band_in + 1.01) circle(d = jack_hole);
}
module tongue() {       // the base's rim, risen to the jack's axis outside the board's edge (frame: the board's edge at 0)
    translate([-tongue_w / 2, -edge_in, Zbot - contact - 0.01]) cube([tongue_w, rim_t, Zb + bush_z - Zbot + contact + 0.01]);
}
module tongue_cut(f) {  // the hole's lower half and the jack's body (open upwards: the board drops in)
    jack_hole();
    translate([-jack[0] / 2 - jack_play, -f - gap_out, Zb - 0.05]) cube([jack[0] + 2 * jack_play, jack[1], bush_z + 1.05]);
}
module tongue_notch() { // the lid's notch over a tongue
    translate([-tongue_w / 2 - tongue_clear, -edge_in - 1, Zb - 1]) cube([tongue_w + 2 * tongue_clear, edge_in + 1, bush_z + reg_top + 1]);
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
module rim() {          // the base's rim round the board, outside its edge: rim_gap from it, up to rim_h over its top
    translate([0, 0, Zbot - contact - 0.01]) linear_extrude(Zb + rim_h - Zbot + contact + 0.01) difference() {
        square([L, W]); translate([rim_t, rim_t]) square([L - 2 * rim_t, W - 2 * rim_t]); }
}
module lid_rebate() {   // the lid's skirt cut back outside the board's edge, over the rim
    translate([0, 0, Zb - 1]) linear_extrude(rim_h + reg_top + 1) difference() {
        translate([-1, -1]) square([L + 2, W + 2]); translate([X0, Y0]) square([board_l, board_w]); }
}
module catch_prism(pts, len) { translate([-len / 2, 0, 0]) rotate([90, 0, 90]) linear_extrude(len) polygon(pts); }
module catch_groove() { c = Zb + catch_c; g = catch_groove + 0.01;   // in the rim's inside (the wall's frame)
    catch_prism([[rim_t + 0.01, c - g], [rim_t - catch_groove, c], [rim_t + 0.01, c + g]], catch_len + 1.0); }
module catch_bump() { c = Zb + catch_c; b = catch_bump + 0.01;        // on the lid's band, at the board's edge
    catch_prism([[edge_in + 0.01, c - b], [edge_in - catch_bump, c], [edge_in + 0.01, c + b]], catch_len); }
module usb_rrect(u, size, r, y0, len) {   // a rounded opening round a socket's axis, from y0 for len (box Y)
    translate([kx(u[0]), 0, Zb + u[1]]) xz_extrude(y0, len) offset(r = r) square([size[0] - 2 * r, size[1] - 2 * r], center = true);
}
module j1_opening() {   // the overmold's recess: from the face to the board's edge (the mouth 0.035 past it)
    usb_rrect(usb_j1, [plug_mold[0] + 2 * plug_clear, plug_mold[1] + 2 * plug_clear], plug_r + plug_clear, Ybk, W - Ybk + 1);
}
xiao_hole = [usb_xiao[2] + 2 * xiao_clear, usb_xiao[3] + 2 * xiao_clear];   // 9.8 x 4.1
module xiao_opening() { // round the socket that stands through the face
    usb_rrect(usb_xiao, xiao_hole, 0.8, Ybk - band_in - 1, W - Ybk + band_in + 2);
}
// The socket stands through the face, so the hole is split at its axis like the jacks': the base rises under it (a
// tongue, open upwards as the board drops in), the lid comes down over it (its notch open downwards)
module xiao_tongue() { translate([kx(usb_xiao[0]) - xiao_hole[0] / 2 - tongue_side, W - rim_t, Zbot - contact - 0.01])
    cube([xiao_hole[0] + 2 * tongue_side, rim_t, Zb + usb_xiao[1] - Zbot + contact + 0.01]); }
module xiao_notch() { translate([kx(usb_xiao[0]) - xiao_hole[0] / 2 - tongue_side - tongue_clear, Ybk, Zb - 1])
    cube([xiao_hole[0] + 2 * (tongue_side + tongue_clear), W - Ybk + 1, usb_xiao[1] + reg_top + 1]); }
module xiao_cut_box() { usb_rrect(usb_xiao, xiao_hole, 0.8, Ybk - 1, W - Ybk + 2);   // the hole's lower half, open upwards
    translate([kx(usb_xiao[0]) - xiao_hole[0] / 2, Ybk - 1, Zb + usb_xiao[1]]) cube([xiao_hole[0], W - Ybk + 2, 10]); }
module usb_plugs() {    // the plugs' overmolds, seated: from each socket's mouth outwards (for check_usb)
    for (u = [usb_j1, usb_xiao]) usb_rrect(u, plug_mold, plug_r, Ybk + u[4], 20);
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
            rim();
            for (k = [0 : 3]) at_jack(k) tongue();
            for (j = [0 : 3]) at_side(j) tongue();
            xiao_tongue();
            intersection() {    // the posts behind the cheeks, between the DIN bodies (5.2 apart: 4.6 posts), tied to the floor
                translate([0, yf, 0]) cube([L, yb - yf, Zbot]);
                for (k = [0 : 3]) at_din(k) din_posts();
            }
        }
        for (k = [0 : 3]) at_din(k) din_slot();
        for (k = [0 : 3]) at_jack(k) tongue_cut(jack_out);
        for (j = [0 : 3]) at_side(j) tongue_cut(0);
        for (c = catches) at_wall(c[0], c[1]) catch_groove();
        j1_opening();
        xiao_cut_box();
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
side_off = tongue_w / 2 + tongue_clear + mark_gap;   // beside a jack: clear of its tongue's notch and its cavity
mark_z = Zb + rim_h + reg_top + 0.9;   // the skirt's labels' baseline: above the joint
function wall_labels() = concat(
    [for (k = [1 : 3]) [str(k + 1), kx(jack_x[k]) + side_off, mark_z, 0, "left", "lid"]],
    [["1 / IN", kx(jack_x[0]) + side_off, mark_z, 1, "right", "lid"],
     ["USB", kx(usb_xiao[0]), Zbot - 14.5, 1, "center", "box"],     // under the XIAO (below R8), on the base's wall
     ["PWR", kx(usb_j1[0]), Zbot - 7.5, 1, "center", "box"]],       // under J1
    [for (j = [0 : 3]) let(s = side_jacks[j], other = side_jacks[j % 2 == 0 ? j + 1 : j - 1],
                          away = ky(s[1]) > ky(other[1]) ? 1 : -1,          // +1: this jack is the one nearer the back
                          y = ky(s[1]) + away * side_off,
                          hal = (s[0] == 0) == (away > 0) ? "right" : "left")
        [s[2], y, mark_z, 2 + s[0], hal, "lid"]]);
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
    difference() { translate([-5, -5]) square([L + 10, W + 10]); offset(delta = -0.8) square([L, W]); }
}
module lid(bumps = true) {   // in place, with its catches' bumps (on the band, where the rebate has cut the skirt back)
    lid_body();
    if (bumps) for (c = catches) at_wall(c[0], c[1]) catch_bump();
}
module lid_body() {     // the plate, the skirt down to the board's top, the caps, the spacers
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
            for (h = oled_holes) translate([kx(h[0]), ky(h[1]), 0]) {   // posts onto the module's PCB, a pin into each hole
                translate([0, 0, Zb + oled_z[1] + oled_post[1]]) cylinder(d = oled_post[0], h = Zl - Zb - oled_z[1] - oled_post[1] + 0.01);
                translate([0, 0, Zb + oled_z[1] + oled_post[1] - oled_post[3]]) cylinder(d = oled_post[2], h = oled_post[3] + 0.01);
            }
        }
        lid_rebate();
        for (k = [0 : 3]) at_jack(k) { jack_hole(); jack_cavity(jack_out); tongue_notch(); }
        for (j = [0 : 3]) at_side(j) { jack_hole(); jack_cavity(0); tongue_notch(); }
        j1_opening();
        xiao_opening();
        xiao_notch();
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
// The printed caps, in place (box frame). Button cap: the bore's floor on the plunger's top (Zb + sw_h).
module button_cap(p, press = 0) {
    top = H + btn_cap[4];
    fz = Zl - btn_cap[5] - btn_cap[2];                       // flange bottom
    translate([kx(p[0]), ky(p[1]), -press]) difference() {
        union() {
            translate([0, 0, fz]) cylinder(d = btn_cap[1], h = btn_cap[2]);
            translate([0, 0, fz]) cylinder(d = btn_cap[0], h = top - fz - 0.5);
            translate([0, 0, top - 0.5]) cylinder(d1 = btn_cap[0], d2 = btn_cap[0] - 1.0, h = 0.5);
        }
        translate([0, 0, fz - 1]) cylinder(d = btn_cap[3], h = Zb + sw_h - fz + 1);
    }
}
module tab_cut(c) { intersection() { children(); translate([kx(c[0]), ky(c[1]) - 8, Zb - 3]) cube([10, 16, 16]); } }   // tab_view
module at_tab(c, pos) { translate([kx(c[0]), ky(c[1]), 0]) rotate([0, 0, ab_rot(c)]) translate([0, pos * c[5] / 2, 0]) children(); }
function tab_z0(c) = Zb + c[6] + tab_lift;
module ab_tab(c, pos = 0) {   // pos: -1 / 0 / +1 = one end of the travel, the middle, the other end (local y: the travel)
    f = flange_size(c); z0 = tab_z0(c); z1 = Zl - tab_top_gap; zs = H + grip_gap;
    at_tab(c, pos) difference() {
        union() {
            translate([-f[0] / 2, -f[1] / 2, z0]) cube([f[0], f[1], z1 - z0]);                                     // the flange
            translate([-tab_neck[0] / 2, -tab_neck[1] / 2, z1 - 0.01]) cube([tab_neck[0], tab_neck[1], zs - z1 + 0.01]);   // the neck
            translate([-grip_post[0] / 2, -grip_post[1] / 2, zs - 0.01]) cube([grip_post[0], grip_post[1], grip_post[2] + 0.01]);
        }
        translate([-(c[3] + tab_fit[0]) / 2, -(c[4] + tab_fit[1]) / 2, z0 - 1]) cube([c[3] + tab_fit[0], c[4] + tab_fit[1], Zb + c[7] - z0 + 1]);
    }
}
module ab_grip(c, pos = 0) {  // on the post, its bottom on the neck's shoulder
    g = grip; b = 0.3;
    at_tab(c, pos) translate([0, 0, H + grip_gap]) difference() {
        union() {
            hull() {
                translate([-(g[0] - 2 * b) / 2, -(g[1] - 2 * b) / 2, 0]) cube([g[0] - 2 * b, g[1] - 2 * b, 0.01]);
                translate([-g[0] / 2, -g[1] / 2, b]) cube([g[0], g[1], g[2] - b - g[3]]);
                translate([-(g[0] - 2 * g[3]) / 2, -(g[1] - 2 * g[3]) / 2, g[2] - 0.01]) cube([g[0] - 2 * g[3], g[1] - 2 * g[3], 0.01]);
            }
            for (i = [0 : grip_ridge[0] - 1]) translate([0, (i - (grip_ridge[0] - 1) / 2) * grip_ridge[3], g[2] - 0.01]) hull() {
                translate([-(g[0] - 2 * g[3] - 0.4) / 2, -grip_ridge[1] / 2, 0]) cube([g[0] - 2 * g[3] - 0.4, grip_ridge[1], 0.01]);
                translate([-(g[0] - 2 * g[3] - 0.8) / 2, -grip_ridge[1] / 2 + 0.1, grip_ridge[2]]) cube([g[0] - 2 * g[3] - 0.8, grip_ridge[1] - 0.2, 0.01]);
            }
        }
        translate([-grip_post[0] / 2, -grip_post[1] / 2, -1]) cube([grip_post[0], grip_post[1], grip_post[2] + 1]);
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
    for (c = ab) { ab_tab(c, pos); ab_grip(c, pos); }
    enc_knob();
}

module lid_print() { translate([0, W, H]) rotate([180, 0, 0]) lid(); }   // top face on the bed

// a strip of the front edge as built, cut from the real base and lid round OUT3 (its DIN window and slot, its jack's
// round hole in the tongue and the skirt, the joint with a catch), the two joined by end posts into one piece; printed
// standing on the floor
ft = [kx(din_x[2]) - 12.0, kx(jack_x[2]) + 7.0, 7.0];   // x0, x1, depth (y)
module front_test() {
    intersection() { union() { box(); lid(); } translate([ft[0], -1, -1]) cube([ft[1] - ft[0], ft[2] + 1, Zl + 1]); }
    for (x = [ft[0], ft[1] - 2.0]) translate([x, 0, 0]) cube([2.0, ft[2], Zl]);
}

// ---------------------------------------------------------------------------------------------------------------
module din_plate_m() { translate([-din_plate[0] / 2, din_in, Zbot - din_plate[1]]) cube([din_plate[0], din_plate[2], din_plate[1]]); }
module din_model() { din_plate_m(); translate([-din_body[0] / 2, din_in + din_plate[2], Zbot - din_body[1]]) cube([din_body[0], din_body[2] - din_plate[2], din_body[1]]); }
module jack_body(f) { translate([-jack[0] / 2, -f, Zb]) cube([jack[0], jack[1], jack[2]]); }
module board_slab(dz) { translate([X0, Y0, Zbot + dz]) cube([board_l, board_w, board_t]); }

if (part == "box") box();
else if (part == "button_cap") translate([0, 0, -(Zl - btn_cap[5] - btn_cap[2])]) translate([-kx(buttons[0][0]), -ky(buttons[0][1]), 0]) button_cap(buttons[0]);   // flange on the bed
else if (part == "ab_tab") translate([-kx(ab[1][0]), -ky(ab[1][1]), -tab_z0(ab[1])]) ab_tab(ab[1]);   // the pocket side on the bed
else if (part == "ab_tab_smd") translate([-kx(ab[0][0]), -ky(ab[0][1]), -tab_z0(ab[0])]) ab_tab(ab[0]);
else if (part == "ab_grip") translate([-kx(ab[1][0]), -ky(ab[1][1]), -(H + grip_gap)]) ab_grip(ab[1]);   // the socket side on the bed
else if (part == "tab_print") {   // the printed parts, as on the bed: tab, SMD tab, grip
    color("Orange") translate([-kx(ab[1][0]), -ky(ab[1][1]), -tab_z0(ab[1])]) ab_tab(ab[1]);
    color("Orange") translate([12 - kx(ab[0][0]), -ky(ab[0][1]), -tab_z0(ab[0])]) ab_tab(ab[0]);
    color("DarkOrange") translate([24 - kx(ab[1][0]), -ky(ab[1][1]), -(H + grip_gap)]) ab_grip(ab[1]);
}
else if (part == "tab_view") {   // OUT3's tab (SW4) cut through its middle along the travel, at both ends (the second 16 mm on)
    c = ab[2];
    for (q = [-1, 1]) translate([0, q > 0 ? 16 : 0, 0]) {
        color("LightSteelBlue") tab_cut(c) lid(); color("DarkGreen") tab_cut(c) mock();
        color("Orange") tab_cut(c) ab_tab(c, q); color("Chocolate") tab_cut(c) ab_grip(c, q);
    }
}
else if (part == "knob") translate([0, 0, Zb + 20 - knob[4] + knob[1]]) rotate([180, 0, 0]) translate([-encC[0], -encC[1], 0]) enc_knob();   // top on the bed
else if (part == "check_caps") {   // must be EMPTY: the caps against the lid at rest, pressed 0.3, the tabs at both ends;
    // against the board (the tabs' pockets round their handle, the knob's bore round the D shaft)
    intersection() { lid(); union() { caps(0, 0); caps(0.3, -1); caps(0.3, 1); } }
    intersection() { union() { for (b = buttons) { button_cap(b); button_cap(b, 0.3); } }
                     difference() { mock(); for (b = buttons) translate([kx(b[0]), ky(b[1]), Zb + 3]) cylinder(d = 3.41, h = 7); } }
    intersection() { union() { for (c = ab) for (q = [-1, 1]) { ab_tab(c, q); ab_grip(c, q); } }
                     difference() { mock(); for (c = ab) translate([kx(c[0]), ky(c[1]), 0]) rotate([0, 0, ab_rot(c)])
                         translate([-c[3] / 2 - 0.05, -(c[4] + c[5]) / 2 - 0.05, Zb + c[6] - 0.01]) cube([c[3] + 0.1, c[4] + c[5] + 0.1, 5]); } }
    intersection() { enc_knob(); union() { difference() { mock(); translate([encC[0], encC[1], Zb + 6.4]) cylinder(d = 6.2, h = 15); } enc_shaft_d(); box(); } }
    for (c = ab) for (q = [-1, 1]) intersection() { ab_tab(c, q); knob_at(c, q); }   // the knob in its pocket at each end
}
else if (part == "brace_caps")   // must NOT be empty: a button cap pulled up 0.35 meets the lid (the flange keeps it)
    intersection() { lid(); translate([0, 0, btn_cap[5] + 0.05]) for (b = buttons) button_cap(b); }
else if (part == "brace_tabs")   // must NOT be empty, each tab sj (0..3): pulled up 0.25 its flange meets the lid's underside
    intersection() { lid(); translate([0, 0, tab_top_gap + 0.05]) ab_tab(ab[sj], 0); }
else if (part == "lid") lid_print();
else if (part == "front_test") front_test();
else if (part == "mock") mock();
else if (part == "check_box") intersection() { box(); mock(); }
else if (part == "check_lid") intersection() { lid(); mock(); }
else if (part == "check_lidbox") intersection() { lid(); box(); }   // they never touch: the board is between them
else if (part == "check_drop") intersection() { union() { for (dz = [0.25 : 0.25 : 22]) translate([0, 0, dz]) lid(false); } union() { box(); mock(); for (b = buttons) button_cap(b); for (c = ab) for (q = [-1, 0, 1]) ab_tab(c, q); } }   // the tabs on, the grips not yet
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
                for (k = [0 : 3]) at_jack(k) { jack_hole(); translate([0, -edge_in - 1, 0]) jack_cavity(jack_out); }
                for (j = [0 : 3]) at_side(j) { jack_hole(); translate([0, -edge_in - 1, 0]) jack_cavity(0); }
                j1_opening(); xiao_opening(); xiao_notch();
                for (k = [0 : 3]) at_jack(k) tongue_notch();
                for (j = [0 : 3]) at_side(j) tongue_notch();
                for (k = top_keep) keep_cut(k, Zb, 3);
                translate([-1, -1, H - 0.5]) cube([L + 2, W + 2, 20]);
                translate([-1, -1, -20]) cube([L + 2, W + 2, Zb + rim_h + reg_top + 20]);
                if (side) for (y = [-10, W - 0.5]) translate([-1, y, -1]) cube([L + 2, 10.5, H + 2]);   // the side walls' ends
                else for (x = [-10, L - 0.5]) translate([x, -1, -1]) cube([10.5, W + 2, H + 2]);       // the front / back's ends
            }
        }
        intersection() {    // the base's labels
            for (l = wall_labels()) if (l[5] == "box") wall_label(l, 3, mark_clear);
            union() {
                for (k = [0 : 3]) at_din(k) din_slot();
                for (k = bot_keep) keep_cut(k, Zbot, 3);
                j1_opening();
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
else if (part == "brace_pull_jack") intersection() { union() { lid(); box(); } for (k = [0 : 3]) at_jack(k) translate([0, -(gap_out + 0.05), 0]) jack_body(jack_out); }
else if (part == "brace_push") intersection() { box(); for (k = [0 : 3]) at_din(k) translate([0, slot_play + 0.05, 0]) din_plate_m(); }   // into the cheek behind
else if (part == "brace_trs") intersection() { lid(); for (k = [0 : 3]) at_jack(k) translate([0, jack_play + 0.05, 0]) jack_body(jack_out); }
// must NOT be empty, for each side jack sj (0..3): pulled out it meets the skirt, pushed in the lid's cap
else if (part == "brace_side_pull") intersection() { union() { lid(); box(); } at_side(sj) translate([0, -(gap_out + 0.05), 0]) jack_body(0); }
else if (part == "brace_side_push") intersection() { lid(); at_side(sj) translate([0, jack_play + 0.05, 0]) jack_body(0); }
else if (part == "brace_floor") intersection() { box(); for (k = [0 : 3]) at_din(k) translate([0, 0, -(din_floor_gap + 0.05)]) din_model(); }
else if (part == "brace_lid") intersection() { lid(); board_slab(contact + 0.05); }
else if (part == "brace_ledge") intersection() { box(); board_slab(-(contact + 0.05)); }
// must NOT be empty, for each direction sj (0..3: -x, +x, -y, +y): the lid (no catches) moved 0.25 meets the base's rim
else if (part == "brace_reg") intersection() { box(); translate(0.25 * [[-1, 0, 0], [1, 0, 0], [0, -1, 0], [0, 1, 0]][sj]) lid(false); }
// must NOT be empty: the lid lifted 0.3 meets the base at the catches (the bumps under the grooves' upper flanks)
else if (part == "brace_snap") intersection() { box(); translate([0, 0, 0.3]) lid(); for (c = catches) at_wall(c[0], c[1]) translate([-catch_len, -1, Zb]) cube([2 * catch_len, 3, 2]); }
// must be EMPTY: the plugs' overmolds, seated, against the base and the lid
else if (part == "check_usb") intersection() { union() { box(); lid(); } usb_plugs(); }
else if (part == "joint") {   // the joint, cut through the front catch's middle, to OUT3's jack: the lid lifted 6 mm
    for (c = [["SteelBlue", 0], ["LightSteelBlue", 1]]) color(c[0]) intersection() {
        if (c[1] == 0) box(); else translate([0, 0, 6]) lid();
        translate([catches[0][1], -1, Zbot - 6]) cube([kx(jack_x[2]) + 8 - catches[0][1], 7, 22]); }
}
else if (part == "open") { color("SteelBlue") box(); color("DarkGreen") mock(); color("LightSteelBlue") translate([0, 0, 30]) lid(); }
else if (part == "front" || part == "back") { color("SteelBlue") box(); color("LightSteelBlue") lid(); color("DarkGreen") mock(); }
else { color("SteelBlue", 0.5) box(); color("LightSteelBlue", 0.35) lid(); color("DarkGreen") mock(); }
echo(str("jank box (modular v5c): outer ", L, " x ", W, " x ", H, " mm (X x Y x Z, lid included); board bottom at Z ", Zbot,
         ", top ", Zb, ", lid underside ", Zl));
