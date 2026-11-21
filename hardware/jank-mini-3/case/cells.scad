// cell options (2026-11-18): the placed board (its STL), the LCD-2 (Waveshare's STEP as STL) and N of mots's cells
// (Gaoneng 6 x 17 x 60, PH2.0 lead) stacked flat on their pad along the bottom wall, right of LINE IN.
H = 72; LY = (72 - 35.01) / 2; N = 3; STL = "../.t/top-72.stl";
W = 93; ZB = 4.5; ZT = ZB + 1.6; LX = 0.5;
color("dimgray") translate([-100, H + 100, ZB]) import(STL);
color("teal") translate([LX + 25.8, H - LY - 17.505, ZT + 11.0 + 2.0]) rotate([0, 0, -90]) import("../.t/lcdm/m.stl");
for (i = [0 : N - 1]) color(i % 2 ? "darkorange" : "orange") translate([W - 0.75 - 60, 0.75, ZT + 0.3 + i * 6.05]) cube([60, 17, 6]);
