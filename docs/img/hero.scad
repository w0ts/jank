// The jank device for the README: the printed box, the board inside, the caps and knob, the DIN socket faces.
// Renders through hardware/box/jank_box.scad (its modules and sizes); not a check, a picture.
use <../../hardware/box/jank_box.scad>
ZBOT = 2.0 + 0.1 + 19.6;   // the board's bottom (jank_box.scad: floor_t + din_floor_gap + the DIN plate's 19.6)
view = "closed";   // closed | open
lift = view == "open" ? 34 : 0;
color("#3a3d42") box();
color("#1d5c3a") mock();
// the DIN-5 sockets' faces: a metal ring and the black socket with its five pins (at each DIN's axis, 9.7 under the board)
for (k = [0 : 3]) at_din(k) translate([0, 1.0 - 0.35, ZBOT - 9.7]) rotate([-90, 0, 0]) {
    color("#b9bcc0") difference() { cylinder(d = 15.0, h = 0.35, $fn = 64); translate([0, 0, -1]) cylinder(d = 13.4, h = 3, $fn = 64); }
    color("#111214") cylinder(d = 13.4, h = 0.3, $fn = 64);
    color("#c8a24a") for (a = [0, 45, 90, 135, 180]) rotate([0, 0, a + 180]) translate([3.6, 0, -0.1]) cylinder(d = 1.1, h = 0.5, $fn = 16);
}
translate([0, 0, lift]) { color("#ece8df") lid(); color("#e8732a") caps(); }
