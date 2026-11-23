// The site's pictures of the jank mini 3: one piece of box2.scad per STL, so photo.py (Blender) can give each its own
// material (the white printed case, the labels in the site's purple, the screen showing a real frame).
// openscad -D 'piece="top"' -o top.stl photo.scad    (make_photo.sh runs every piece)
use <box2.scad>

piece = "top";   // top | labels | cap | cap_symbols | bottom | switch | pcb | parts_top | parts_sw | parts_bottom | lcd2

if (piece == "top") top_part();
if (piece == "labels") face_labels();
if (piece == "cap") cap();
if (piece == "cap_symbols") cap_symbols();
if (piece == "bottom") bottom_part();
if (piece == "switch") kcd1();
if (piece == "pcb") grp_pcb();
if (piece == "parts_top") grp_top();
if (piece == "parts_sw") grp_sw();
if (piece == "parts_bottom") grp_bottom();
if (piece == "lcd2") grp_lcd2();
