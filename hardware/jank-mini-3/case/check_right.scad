// a check view: the tray seen from OUTSIDE the RIGHT wall; through the two 17 mm cut-outs you see the DINs
// (HOOYA DIN-504B-M10: front face 20.9 wide x 20.9 tall, a 15 mm mouth on its axis 11.8 mm over the board, 5 pins on
// a 7 mm circle, the front 0.3 mm past the board's right edge; make_board.py: J5 y 20.6, J6 y 43.4)
use <case.scad>
W = 100; H = 64; FLOOR = 2.0; STANDOFF = 6.5; ZB = FLOOR + STANDOFF; ZT = ZB + 1.6;
module din(yb) {
    translate([W + 0.3 - 14.8, H - yb - 10.45, ZT]) color("dimgray") difference() {
        cube([14.8, 20.9, 20.9]);
        translate([14.8 - 6, 10.45, 11.8]) rotate([0, 90, 0]) cylinder(d = 15, h = 7, $fn = 48);
    }
    for (a = [0, 45, 90, 135, 180]) {
        r = 3.5;
        translate([W + 0.3 - 6, H - yb + r * cos(a), ZT + 11.8 + r * sin(a)]) rotate([0, 90, 0]) color("gold") cylinder(d = 1.2, h = 4, $fn = 16);
    }
}
color("green") translate([0, 0, ZB]) cube([W, H, 1.6]);
din(20.6); din(43.4);
color("lightsteelblue") tray();
