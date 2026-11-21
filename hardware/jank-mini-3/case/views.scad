// check views: the tray with the placed board (KiCad's 3D models: kicad-cli pcb export stl -o ../.t/board.stl), the
// LCD-2 (Waveshare's STEP as STL: ../.t/lcdm/m.stl, its PCB bottom at model z -2.0, 11 mm over our board) and the cell
// (mots's 2 x 6 x 17 x 60 LiPo cells, on their long edge along the bottom wall). make_views.sh renders it.
use <case.scad>
BOARD_STL = "../.t/edge.stl";
W = 93; H = 62.6; FLOOR = 2.0; STANDOFF = 2.5; ZB = FLOOR + STANDOFF; ZT = ZB + 1.6;
LX = 0.5; LY = (H - 35.01) / 2;
show_lcd = true; show_tray = true; show_lid = false; section = false;   // section: cut along the cell's centre line, seen from the bottom wall
module all() {
    color("dimgray") translate([-100, H + 100, ZB]) import(BOARD_STL);
    if (show_lcd) color("teal") translate([LX + 25.8, H - LY - 17.505, ZT + 11.0 + 2.0]) rotate([0, 0, -90]) import("../.t/lcdm/m.stl");
    for (i = [0 : 1]) color(i ? "darkorange" : "orange") translate([W - 0.8 - 60, 0.8 + i * 6.2, ZT + 0.3]) cube([60, 6, 17]);
    if (show_lid) color("lightsteelblue", 0.5) translate([-3.5, -3.5, ZT + 21.5]) cube([W + 7, H + 7, 2]);
    if (show_tray) color("lightsteelblue") tray();
}
if (section) difference() { all(); translate([-50, -50, -50]) cube([300, 50 + H - LY - 17.505, 200]); }
else all();
