#!/bin/sh
# The site's pictures of the jank mini 3 (run from anywhere, at nice 19): photo.scad's pieces to STL, photo.py renders
# them in Blender (Cycles, 6 threads) with the white case, the purple labels and the beat screen, then WebP into page/img.
# About: page/img/jank-mini-3.webp. Build: page/img/mini3/box-{assembly,exploded,top,cap-top}.webp. ~6 min on the Mac.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
tmp=$(mktemp -d "$here/photo-tmp.XXXX")
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/stl" "$tmp/out"
for p in top labels cap cap_symbols bottom switch pcb parts_top parts_sw parts_bottom lcd2; do
    nice -n 19 openscad --backend=manifold -D "piece=\"$p\"" -o "$tmp/stl/$p.stl" "$here/photo.scad" 2>&1 | grep -iE "error|warning" || true
done
nice -n 19 blender -b -P "$here/photo.py" -- "$tmp/stl" "$root/page/img/screen-beat.png" "$tmp/out" 2>&1 | grep -iE "error|Traceback|Saved" || true
w() { cwebp -quiet -q 86 -alpha_q 90 "$tmp/out/$1.png" -o "$root/page/img/$2"; }
w hero jank-mini-3.webp
w assembly mini3/box-assembly.webp
w exploded mini3/box-exploded.webp
w top mini3/box-top.webp
w cap mini3/box-cap-top.webp
