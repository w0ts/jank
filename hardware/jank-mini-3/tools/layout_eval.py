"""jank-mini-3 study (KiCad's python): score one placed board WITH its best legal pin map (gpio_opt): the estimated
track length of every signal net (a minimum spanning tree over its pads, the detour round the cut-out counted), after
moving each function to its best LCD-2 pin. Prints one JSON line. Usage: python3 layout_eval.py <board>"""
import json
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gpio_opt as G  # noqa: E402  (make_board / parts with this process's M3_* layout options)


def mst(points):
    if len(points) < 2:
        return 0.0
    left, total = list(points[1:]), 0.0
    tree = [points[0]]
    while left:
        d, i = min((min(G.path(t, p) for t in tree), i) for i, p in enumerate(left))
        total += d
        tree.append(left.pop(i))
    return total


def main(path_pcb: str) -> None:
    board = pcbnew.LoadBoard(path_pcb)
    pos, nets, spare_pads = {}, {}, {"J15": [], "J16": []}
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        for pad in fp.Pads():
            xy = (pcbnew.ToMM(pad.GetPosition().x) - G.OX, pcbnew.ToMM(pad.GetPosition().y) - G.OY)
            name = pad.GetNetname().lstrip("/")
            if ref in G.LCD:
                pos[(ref, int(pad.GetNumber()))] = xy
                if (ref, int(pad.GetNumber())) in G.PINS:
                    continue                      # (GPIO pins: placed by the best map below)
            if ref in ("J17", "J20"):
                spare_pads["J15" if ref == "J17" else "J16"].append(xy)
                continue
            if name and name != "GND" and not name.startswith("unconnected"):
                nets.setdefault(name, []).append(xy)
    funcs = [f for f in G.P.GPIO if not f.startswith("SPARE_")] + list(G.SPARES)
    pins = sorted(G.PINS)

    def cost_of(f, pin):
        if not G.legal(f, G.PINS[pin], pin[0]):
            return G.BIG
        if f in G.SPARES:
            return min(G.path(pos[pin], d) for d in spare_pads[G.SPARES[f]])
        return min(G.path(pos[pin], d) for d in nets[f])

    cost = [[cost_of(f, pin) for pin in pins] for f in funcs]
    pick = G.hungarian(cost)
    gpio = {}
    for i, f in enumerate(funcs):
        pin = pins[pick[i]]
        gpio[f] = f"{pin[0]}.{pin[1]} GPIO{G.PINS[pin]}"
        if f not in G.SPARES:
            nets[f] = nets[f] + [pos[pin]]
    total = sum(mst(p) for p in nets.values())
    cross = sorted(n for n, p in nets.items() if {"top", "bottom"} <= {G.band(*q) for q in p})
    print(json.dumps({"total_mm": round(total), "gpio_mm": round(sum(cost[i][pick[i]] for i in range(len(funcs)))),
                      "cross": cross, "gpio": gpio}))


if __name__ == "__main__":
    main(sys.argv[1])
