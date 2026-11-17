# SPDX-License-Identifier: GPL-3.0-only
"""python3 export.py PLACE.json OUT_FLOORPLAN.json: the units' places -> every part's (twins too), absolute board mm"""
import json, sys
import model as M
units, _fb, _c = M.load()
place = json.load(open(sys.argv[1]))
out = {}
for r, v in sorted(place.items()):
    if r.startswith("_") or r in M.FIXED or r not in units:
        continue
    x, y, a = v
    out[r] = [round(x, 3), round(y, 3), round(a % 360, 1)]
    u = units[r]
    for m, ox, oy, mr in u.members:              # (a template's parts)
        px, py = M.rot(ox, oy, a)
        out[m] = [round(x + px, 3), round(y + py, 3), round((a + mr) % 360, 1)]
    if u.twin:
        t, dx, dy = u.twin
        px, py = M.rot(dx, dy, a)
        out[t] = [round(x + px, 3), round(y + py, 3), round((a + u.twin_rot) % 360, 1)]
json.dump(out, open(sys.argv[2], "w"), indent=0, sort_keys=True)
print(len(out), "parts")
