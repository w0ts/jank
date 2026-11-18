"""jank-mini-3 study (KiCad's python): where and why routing fails. Reads a routed board: its open connections (KiCad's
DRC), how many tracks already cross the narrow channels (top band right of the TRS jacks, the corridor right of the
cut-out, the bottom band), and draws an SVG: tracks (top red, bottom blue), pads, the cut-out, the D-pad keep-out, and
each open connection as a thick black dashed line with its net name.  Usage: python3 diagnose.py <board> <out.svg>"""
import json
import os
import re
import subprocess
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import make_board as M  # noqa: E402

KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
OX = OY = 100.0
S = 7.0          # px per mm


def mm(v):
    return pcbnew.ToMM(v)


def open_connections(path):
    rpt = path + ".diag.json"
    subprocess.run(["nice", "-n", "19", KC, "pcb", "drc", "--format", "json", "-o", rpt, path],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    out = []
    for v in json.load(open(rpt)).get("unconnected_items", []):
        its = v.get("items", [])
        if len(its) == 2:
            m = re.search(r"\[([^\]]+)\]", its[0]["description"])
            out.append((m.group(1).lstrip("/") if m else "?", [(i["pos"]["x"] - OX, i["pos"]["y"] - OY, i["description"]) for i in its]))
    os.remove(rpt)
    return out


def crossings(tracks, cut):
    """tracks crossing a cut line, per layer: cut = ('x', x0, y0, y1) vertical or ('y', y0, x0, x1) horizontal"""
    n = {"F": 0, "B": 0}
    for t in tracks:
        (x0, y0), (x1, y1), lay = t
        if cut[0] == "x":
            c, a, b = cut[1], cut[2], cut[3]
            if (x0 - c) * (x1 - c) < 0:
                y = y0 + (y1 - y0) * (c - x0) / (x1 - x0)
                n[lay] += a <= y <= b
        else:
            c, a, b = cut[1], cut[2], cut[3]
            if (y0 - c) * (y1 - c) < 0:
                x = x0 + (x1 - x0) * (c - y0) / (y1 - y0)
                n[lay] += a <= x <= b
    return n


def main(path, svg_path):
    board = pcbnew.LoadBoard(path)
    tracks = [((mm(t.GetStart().x) - OX, mm(t.GetStart().y) - OY), (mm(t.GetEnd().x) - OX, mm(t.GetEnd().y) - OY),
               "F" if t.GetLayer() == pcbnew.F_Cu else "B")
              for t in board.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T]
    opens = open_connections(path)
    cx0, cy0, cx1, cy1 = M.CUT
    cuts = {
        "top band, right of the TRS jacks (x 47, y 0..cut-out)": ("x", 47.0, 0.0, cy0),
        "top band, left of the TRS jacks (x 21, y 0..cut-out)": ("x", 21.0, 0.0, cy0),
        "right part, across (y mid, x cut-out..right wall)": ("y", (cy0 + cy1) / 2, cx1, M.W),
        "bottom band (x 45, y cut-out..edge)": ("x", 45.0, cy1, M.H),
    }
    print("open connections:")
    for net, ends in opens:
        print(f"  {net}: " + "  ->  ".join(f"({x:.1f}, {y:.1f}) {d.split(' of ')[-1] if ' of ' in d else d[:30]}" for x, y, d in ends))
    print("tracks crossing each channel (top layer / bottom layer):")
    for name, cut in cuts.items():
        n = crossings(tracks, cut)
        span = cut[3] - cut[2]
        print(f"  {name}: {n['F']} / {n['B']}  over {span:.1f} mm (room for about {int(span / 0.4)} per layer at 0.2/0.2)")
    W, H = M.W, M.H
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-10 -10 {W * S + 20} {H * S + 20}" width="{W * S + 20}">',
         f'<rect x="0" y="0" width="{W * S}" height="{H * S}" fill="#1e3a2a"/>',
         f'<rect x="{cx0 * S}" y="{cy0 * S}" width="{(cx1 - cx0) * S}" height="{(cy1 - cy0) * S}" fill="#ffffff"/>']
    for k in M.DPAD_KEEP:
        o.append(f'<rect x="{k[0] * S}" y="{k[1] * S}" width="{(k[2] - k[0]) * S}" height="{(k[3] - k[1]) * S}" fill="none" stroke="#e0c040" stroke-dasharray="4 3"/>')
    for (a, b, lay) in tracks:
        col = "#e05050" if lay == "F" else "#5080e0"
        o.append(f'<line x1="{a[0] * S:.1f}" y1="{a[1] * S:.1f}" x2="{b[0] * S:.1f}" y2="{b[1] * S:.1f}" stroke="{col}" stroke-width="1.4" opacity="0.8"/>')
    for fp in board.GetFootprints():
        for p in fp.Pads():
            bb = p.GetBoundingBox()
            o.append(f'<rect x="{(mm(bb.GetLeft()) - OX) * S:.1f}" y="{(mm(bb.GetTop()) - OY) * S:.1f}" '
                     f'width="{mm(bb.GetWidth()) * S:.1f}" height="{mm(bb.GetHeight()) * S:.1f}" fill="#c8a040" opacity="0.7"/>')
    for net, ends in opens:
        (x0, y0, _), (x1, y1, _) = ends
        o.append(f'<line x1="{x0 * S:.1f}" y1="{y0 * S:.1f}" x2="{x1 * S:.1f}" y2="{y1 * S:.1f}" stroke="#000" stroke-width="5" stroke-dasharray="8 4"/>')
        o.append(f'<line x1="{x0 * S:.1f}" y1="{y0 * S:.1f}" x2="{x1 * S:.1f}" y2="{y1 * S:.1f}" stroke="#ffff00" stroke-width="2" stroke-dasharray="8 4"/>')
        o.append(f'<text x="{(x0 + x1) / 2 * S:.1f}" y="{(y0 + y1) / 2 * S - 6:.1f}" font-size="16" font-family="sans-serif" '
                 f'fill="#ffff00" stroke="#000" stroke-width="3" paint-order="stroke">{net}</text>')
    for name, cut in cuts.items():
        if cut[0] == "x":
            o.append(f'<line x1="{cut[1] * S}" y1="{cut[2] * S}" x2="{cut[1] * S}" y2="{cut[3] * S}" stroke="#ff00ff" stroke-width="2"/>')
        else:
            o.append(f'<line x1="{cut[2] * S}" y1="{cut[1] * S}" x2="{cut[3] * S}" y2="{cut[1] * S}" stroke="#ff00ff" stroke-width="2"/>')
    o.append("</svg>")
    open(svg_path, "w").write("\n".join(o))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
