"""jank-mini-3 helper (KiCad's python): widen the high-current segments check_board item 7 flags (more current per mm
than its minimum track) to that minimum, one net at a time, keeping a change only when KiCad's DRC stays clean.
Usage: python3 widen_hot.py <board>"""
import json
import os
import shutil
import subprocess
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import check_board as C  # noqa: E402
import route as R  # noqa: E402

KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
WANT = {"VBAT": 0.3}


def drc(p):
    r = p + ".w.json"
    subprocess.run([KC, "pcb", "drc", "--schematic-parity", "--severity-all", "--format", "json", "-o", r, p],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    d = json.load(open(r))
    os.remove(r)
    return len(d.get("violations", [])), len(d.get("unconnected_items", []))


def main(path):
    base = drc(path)
    taps = {tuple(p.split("-")) for pins in R.FIRST_SKIP.values() for p in pins}
    tmp = os.path.splitext(path)[0] + ".widen.kicad_pcb"
    for e in ("kicad_pro", "kicad_sch", "kicad_dru"):
        src = os.path.splitext(path)[0] + "." + e
        if os.path.exists(src):
            shutil.copy(src, os.path.splitext(tmp)[0] + "." + e)
    for n, w in WANT.items():
        b = pcbnew.LoadBoard(path)
        fps = {f.GetReference(): f for f in b.GetFootprints()}
        segs = [t for t in b.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T and t.GetNetname().lstrip("/") == n]
        pads = [(f.GetReference(), p) for f in fps.values() for p in f.Pads() if p.GetNetname().lstrip("/") == n]
        main_pins = {(r, p.GetNumber()) for r, p in pads if (r, p.GetNumber()) not in taps}
        hot = C.hot_segments(segs, pads, main_pins, w)
        if not hot:
            continue
        for t in hot:
            t.SetWidth(pcbnew.FromMM(w + 0.01))
        pcbnew.ZONE_FILLER(b).Fill(b.Zones())
        b.Save(tmp)
        got = drc(tmp)
        print(f"{n}: {len(hot)} segments to {w + 0.01:.2f} mm: DRC {got} (was {base})")
        if got[0] <= base[0] and got[1] <= base[1]:
            shutil.copy(tmp, path)
            print("  written")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
