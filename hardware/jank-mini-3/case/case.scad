// SPDX-License-Identifier: GPL-3.0-only
// jank LCD-2 add-on: a tray and a lid (and 4 button plungers) to the 93 x 62.6 board (Henri, 2026-11-18: MIDI on the
// top wall, CLOCK / RESET on the right, the USB-C and LINE IN on the left, 2 cells on edge along the bottom wall).
// Board coordinates from make_board.py (x right, y DOWN from the board's top-left corner); here y_scad = H - y_board.
// Heights: ASSUMED from the datasheets / Waveshare's drawing where named, else estimates (marked EST): every wall
// cut-out has 1 mm of play on each side. Confirm on the bench (bench test 1) before printing for good.
// openscad -D 'part="tray"' -o case_tray.stl case.scad   (part = tray | lid | plungers | assembly)
part = "assembly";
W = 93; H = 62.6;                 // the board (make_board.py LCD2_W / LCD2_H)
GAP = 1.0; WALL = 2.5; FLOOR = 2.0; LID = 2.0;
STANDOFF = 2.5;                   // floor top -> board bottom: only the THT pins' tails (<= 2 mm, ASSUMED) + 0.5 mm
BOARD = 1.6;
ZB = FLOOR + STANDOFF;            // board bottom
ZT = ZB + BOARD;                  // board top
LCD_GAP = 11.0;                   // board top -> the LCD-2's PCB (8.5 socket + 2.5 header plastic, ASSUMED)
ZLCD = ZT + LCD_GAP;              // the LCD-2's PCB bottom
GLASS = 16.8;                     // board top -> the LCD-2's glass (KiCad render with Waveshare's STEP, 2026-11-18)
DIN_H = 20.9;                     // the DIN-504B on top of the board (HOOYA drawing; JLC's model 19.6): Henri's option 1
ZTOP = ZT + DIN_H + 0.6;          // the lid's underside: 0.6 mm over the DINs; the screen sits ZTOP - ZT - GLASS = 4.7 mm
                                  // under it (the window's walls)
OUT_W = W + 2 * (GAP + WALL); OUT_D = H + 2 * (GAP + WALL);
LX = 0.5; LY = (H - 35.01) / 2;   // the LCD-2, centred vertically (make_board.py)
function lcd(fx, fy) = [LX + 48.21 - fy, LY + fx];
HOLES = [lcd(3, 3), lcd(32.01, 3), lcd(3, 45.2), lcd(32.01, 45.2)];   // its M2 screws (through the floor)
MOUNT = [[W - 3.5, 30.5], [51.5, 20.0]];                             // the board's own M2 holes
DPX = 49.0 + (W - 13.0 - 49.0) / 2;
BUTTONS = [[DPX, H / 2 - 9], [DPX, H / 2 + 9], [DPX - 9, H / 2], [DPX + 9, H / 2]];
AA = [40.8 + 1.0, 30.6 + 1.0];                    // the screen's active area (landscape) + 0.5 mm all round
AA_C = [LX + 48.21 - (1.8 + 20.4), LY + 2.0 + 15.3];   // its centre (2D drawing: AA 1.8 mm from the top, 2.0 from the side)
$fn = 48;

module at(p, z = 0) { translate([p[0], H - p[1], z]) children(); }

// a wall cut-out: on wall w ("left", "top", "bottom"), centred at board coordinate c along the wall, z centre zc
module cut(w, c, zc, sw, sh, round = false) {
    t = WALL + GAP + 2;
    if (w == "left")   translate([-GAP - WALL - 1, H - c, zc]) rotate([0, 90, 0]) shape(round ? sw : sh, round ? sh : sw, round, t);
    if (w == "top")    translate([c, H + GAP + WALL + 1, zc]) rotate([90, 0, 0]) shape(sw, sh, round, t);
    if (w == "bottom") translate([c, -GAP - WALL - 1, zc]) rotate([-90, 0, 0]) shape(sw, sh, round, t);
    if (w == "right")  translate([W + GAP + WALL + 1, H - c, zc]) rotate([0, -90, 0]) shape(round ? sw : sh, round ? sh : sw, round, t);
}
module shape(a, b, round, t) {
    if (round) cylinder(d = a, h = t, center = false);
    else translate([-a / 2, -b / 2, 0]) cube([a, b, t]);
}

module tray() {
    difference() {
        translate([-GAP - WALL, -GAP - WALL, 0]) cube([OUT_W, OUT_D, ZTOP]);
        translate([-GAP, -GAP, FLOOR]) cube([W + 2 * GAP, H + 2 * GAP, ZTOP]);
        // left wall (Henri, 2026-11-18): our USB-C OUT (J4, y LY + 12) and IN (J1, y LY + 23) under the LCD-2, its own
        // USB-C (LY + 17.5) above them: the plugs' overmolds (EST 11 x 6.5) would leave a 0.3 mm web between the three
        // windows, so they are one opening; and LINE IN (PJ-307C, y H - 6.9, axis EST 2.8 up) below the LCD-2
        hull() {
            cut("left", LY + 12.0, ZT + 1.6, 12.5, 7.5);
            cut("left", LY + 23.0, ZT + 1.6, 12.5, 7.5);
            cut("left", LY + 17.5, ZLCD - 1.6, 12.5, 7.5);
        }
        cut("left", H - 6.9, ZT + 2.8, 7.0, 0, true);
        // top wall = MIDI: the two TRS jacks (PJ-307C, axis EST 2.8 up) and the two DINs (HOOYA: axis 11.8 over the board)
        for (x = [29.8, 42.6]) cut("top", x, ZT + 2.8, 7.0, 0, true);
        for (x = [W - 33.0, W - 10.9]) cut("top", x, ZT + 11.8, 17.0, 0, true);
        // right wall = modular: CLOCK, RESET (PJ-320A, axis EST 2.5)
        for (y = [24.0, 37.0]) cut("right", y, ZT + 2.5, 6.5, 0, true);
        // bottom wall: nothing (the cells lie along it, inside)
        // the LCD-2's four M2 screws come up through the floor: their heads sit in 1.8 mm counterbores from below
        for (p = HOLES) at(p, -1) { cylinder(d = 2.4, h = FLOOR + 2); cylinder(d = 4.4, h = 1 + 1.8); }
    }
    // floor bosses: the LCD-2's four screw stacks (M2 clearance through the floor) and the board's two M2 holes
    // the LCD-2's four points: its M2 screws come up through the floor (heads in the floor's counterbores), the boss
    // carries the board; the board's two holes: an M2 self-tapping screw from above into a 1.8 mm hole
    for (p = HOLES) at(p, FLOOR) difference() {
        cylinder(d = 7.0, h = STANDOFF);
        translate([0, 0, -1]) cylinder(d = 2.4, h = STANDOFF + 2);
    }
    for (p = MOUNT) at(p, FLOOR) difference() {
        cylinder(d = 5.0, h = STANDOFF);
        translate([0, 0, STANDOFF - 8]) cylinder(d = 1.8, h = 9);
    }
}

module lid() {
    translate([0, 0, ZTOP]) difference() {
        union() {
            translate([-GAP - WALL, -GAP - WALL, 0]) cube([OUT_W, OUT_D, LID]);
            // a 1.5 mm lip inside the walls (press fit, 0.2 mm play)
            translate([-GAP + 0.2, -GAP + 0.2, -1.5]) difference() {
                cube([W + 2 * GAP - 0.4, H + 2 * GAP - 0.4, 1.5]);
                translate([1.2, 1.2, -1]) cube([W + 2 * GAP - 2.8, H + 2 * GAP - 2.8, 4]);
            }
        }
        at(AA_C, -2) translate([-AA[0] / 2, -AA[1] / 2, 0]) cube([AA[0], AA[1], LID + 4]);   // the screen window
        for (b = BUTTONS) at(b, -2) cylinder(d = 6.4, h = LID + 4);
    }
}

// printed plungers: from the tactile switch's top (1.5 mm over the board) to 1.5 mm over the lid
module plungers() {
    L = ZTOP + LID + 1.5 - (ZT + 1.5);
    for (i = [0 : 3]) translate([i * 10, -15, 0]) {
        cylinder(d = 6.0, h = L);
        cylinder(d = 8.5, h = 1.2);                          // the collar under the lid
    }
}

if (part == "tray") tray();
if (part == "lid") lid();
if (part == "plungers") plungers();
if (part == "assembly") { tray(); lid(); }
