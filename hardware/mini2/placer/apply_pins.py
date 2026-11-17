# SPDX-License-Identifier: GPL-3.0-only
"""python3 apply_pins.py PLACE.json [write]: the placer's pin map (pad names = the board dump's pins) -> net: (GPIO, pin);
print it, and with "write" set parts.py's GPIO tuples to it (any value they hold now)"""
import json, re, sys
import model as M
import os
P = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "parts.py")
pl = json.load(open(sys.argv[1]))
cur = json.load(open(os.path.join(M.HERE, os.environ.get("CUR", "board.json"))))
net_of = {f"U1.{p[0]}": p[1] for p in cur["U1"]["pads"] if p[1]}
s = open(P).read()
out = {}
for nm, pin in pl["_pins"].items():
    net = net_of[nm]
    out[net] = (M.PIN_GPIO[pin], str(pin))
for net, (g, p) in sorted(out.items(), key=lambda kv: int(kv[1][1])):
    tag = "" if (net in M.OUT_NETS) <= (int(p) in M.OUT_PINS) else "  <-- OUTPUT ON A GLITCH PIN"
    print(f"{net:11s} GPIO{g:<3d} pin {p}{tag}")
    pat = re.compile(r'"%s": \(\d+, "\d+"\)' % re.escape(net))
    assert len(pat.findall(s)) == 1, net
    s = pat.sub('"%s": (%d, "%s")' % (net, g, p), s)
if len(sys.argv) > 2 and sys.argv[2] == "write":
    open(P, "w").write(s)
    print("written")
