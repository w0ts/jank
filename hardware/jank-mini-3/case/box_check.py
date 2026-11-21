# SPDX-License-Identifier: GPL-3.0-only
"""box.scad's collision / clearance check (OpenSCAD, manifold backend).
For each (case part, board group): the smallest grow step at which the grown board boxes meet the part (an OpenSCAD
intersection that is not empty). Boxes grow in x, y and z alike, so a result "a .. b" means the clearance is at
least a and under b (L-infinity). "touch (0)" is a designed contact (the bosses on our board's top, the cap on the
switches, the cell on the lid). Then: the board coming in from below (every board box swept 40 mm down must miss the
shell) and the lid against the shell. Temporary files in ../.t/ (git-ignored), removed at the end.
    python3 box_check.py [din_depth=19.6 ...]      (extra -D overrides)
    python3 box_check.py box2.scad [...]           (the split redesign: top / bottom part, snaps, posts; 2 workers)"""
import concurrent.futures as cf
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CASE = os.path.join(HERE, "box.scad")
STEPS = [-0.02, 0.0, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.5, 0.6, 0.75, 1.0, 1.5, 2.0, 3.0]
GROUPS = ("pcb", "top", "sw", "bottom", "lcd2", "strips", "cell", "screws")
WORKERS = 4                     # (each one OpenSCAD at nice 19: about half of a 16-core Mac's free cores)


def hits(tmp: str, part: str, group: str, g: float, more: tuple, extra: list) -> bool:
    f = os.path.join(tmp, f"{part}-{group}-{g}-{abs(hash(more))}.off")
    var = "ck_psw" if (part, group) == ("switch", "lid") else "ck_grow"   # (the lid does not grow: the switch does)
    cmd = ["nice", "-n", "19", "openscad", "--backend=manifold", "-D", 'part="check"', "-D", f'ck_part="{part}"',
           "-D", f'ck_group="{group}"', "-D", f"{var}={g}"]
    for e in extra + list(more):
        cmd += ["-D", e]
    r = subprocess.run(cmd + ["-o", f, CASE], capture_output=True, text=True)
    txt = r.stdout + r.stderr
    if "ERROR" in txt and "empty" not in txt:
        raise RuntimeError(txt)
    found = "empty" not in txt.lower() and os.path.exists(f)
    if os.path.exists(f):
        os.remove(f)
    return found


def volume(tmp: str, part: str, group: str, more: tuple, extra: list) -> float:
    """the intersection's volume, mm^3 (0 when empty): case part against case part, where touching faces are designed"""
    f = os.path.join(tmp, f"v-{part}-{group}-{abs(hash(more))}.stl")
    cmd = ["nice", "-n", "19", "openscad", "--backend=manifold", "--export-format", "asciistl", "-D", 'part="check"',
           "-D", f'ck_part="{part}"', "-D", f'ck_group="{group}"']
    for e in extra + list(more):
        cmd += ["-D", e]
    subprocess.run(cmd + ["-o", f, CASE], capture_output=True, text=True)
    if not os.path.exists(f):
        return 0.0
    v, tri = 0.0, []
    for line in open(f):
        t = line.split()
        if t and t[0] == "vertex":
            tri.append([float(c) for c in t[1:4]])
            if len(tri) == 3:
                a, b, c = tri
                v += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0])
                      + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6
                tri = []
    os.remove(f)
    return abs(v)


def svg_pieces(path: str) -> list:
    """the closed outlines of an OpenSCAD SVG: [(area, x0, y0, x1, y1)] per outline"""
    import re
    out = []
    for d in re.findall(r' d="([^"]*)"', open(path).read()):
        for sub in re.split(r"[Mm]", d):
            n = [float(v) for v in re.findall(r"[-\d.]+", sub)]
            p = list(zip(n[0::2], n[1::2]))
            if len(p) > 2:
                a = abs(sum(p[i][0] * p[i - 1][1] - p[i - 1][0] * p[i][1] for i in range(len(p))) / 2)
                out.append((a, min(q[0] for q in p), min(q[1] for q in p), max(q[0] for q in p), max(q[1] for q in p)))
    return out


SLIVER_W = 0.8                  # walls thinner than this are slivers (2 lines of the 0.4 nozzle)


def slivers(tmp: str, part: str, axis: str, at: float, extra: list) -> list:
    """one plane cut through a part (z = at, x = at or OpenSCAD Y = at): what an opening (shrink, grow) by SLIVER_W
    removes, i.e. the material thinner than SLIVER_W in that cut: [(area, bbox in the cut's plane)]"""
    tf = {"z": f"translate([0, 0, {-at}])", "x": f"translate([0, 0, {at}]) rotate([0, 90, 0])",
          "y": f"translate([0, 0, {at}]) rotate([-90, 0, 0])"}[axis]
    f = os.path.join(tmp, f"sl-{part}-{axis}-{at}.scad")
    o = f[:-5] + ".svg"
    h = SLIVER_W / 2
    open(f, "w").write(f'include <{CASE}>\npart = "none";\nmodule sl() projection(cut = true) {tf} {part}();\n'
                       f"difference() {{ sl(); offset(delta = {h}) offset(delta = {-h}) sl(); }}\n")
    cmd = ["nice", "-n", "19", "openscad", "--backend=manifold", "-o", o, f]
    for e in extra:
        cmd[4:4] = ["-D", e]
    r = subprocess.run(cmd, capture_output=True, text=True)
    res = [] if "empty" in (r.stdout + r.stderr).lower() or not os.path.exists(o) else svg_pieces(o)
    for x in (f, o):
        if os.path.exists(x):
            os.remove(x)
    return [p for p in res if p[0] > 0.05]


def frange(a: float, b: float, step: float) -> list:
    n = int((b - a) / step) + 1
    return [round(a + i * step, 3) for i in range(n)]


def main2(extra: list) -> None:
    """box2.scad: the top part, the bottom part, its screw tubes, the switch, the cap; then the sliver scan"""
    jobs = [("top_part", g, ()) for g in GROUPS]
    jobs += [("bottom_part", g, ()) for g in GROUPS]
    jobs += [("tubes", g, ()) for g in GROUPS]
    jobs += [("switch", g, ()) for g in ("pcb", "top", "bottom", "lcd2", "strips")]
    jobs += [("cap", g, ()) for g in ("pcb", "top", "sw", "lcd2")]
    # the cell: lifted off the floor it lies on (its rubber to the L, its own wall, the bottom wall), and against the
    # board's parts above / beside it
    jobs += [("bottom_part", "cell", ("ck_cell_lift=3",))]
    jobs += [("cell", g, ()) for g in ("pcb", "bottom", "lcd2", "strips", "screws")]
    once = [("top_part", g, ("ck_sweep=40",)) for g in ("pcb", "top", "sw", "bottom", "lcd2", "strips")]
    vols = [("bottom_part", "top_part", ()), ("bottom_part", "top_part", ("ck_dz=0.05",)),
            ("bottom_part", "top_part", ("ck_dz=-0.05",)), ("cap", "top_part", ()), ("switch", "bottom_part", ("ck_psw=-0.01",)),
            ("switch", "top_part", ())]
    # the sliver scan: plane cuts through both parts every 0.4 mm in z and 1 mm in x and y (offset off the grid's
    # round numbers so a cut never lies in a face)
    cuts = [(p, "z", z) for p in ("top_part", "bottom_part") for z in frange(-24.93, 6.75, 0.4)]
    cuts += [(p, "x", x) for p in ("top_part", "bottom_part") for x in frange(-2.33, 97.8, 1.0)]
    cuts += [(p, "y", y) for p in ("top_part", "bottom_part") for y in frange(-84.33, 2.3, 1.0)]
    os.makedirs(os.path.join(HERE, "..", ".t"), exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="box-check-", dir=os.path.join(HERE, "..", ".t")) as tmp:

        def scan(job):
            part, group, more = job
            prev = None
            for g in STEPS:
                if hits(tmp, part, group, g, more, extra):
                    return job, ("OVERLAP" if g <= -0.02 else "touch (0)" if g == 0.0 else f"{prev} .. {g}")
                prev = g
            return job, f">= {STEPS[-1]}"

        def one(job):
            return job, ("OVERLAP" if hits(tmp, job[0], job[1], -0.02, job[2], extra) else "clear")

        def vol(job):
            return job, f"intersection {volume(tmp, job[0], job[1], job[2], extra):.4f} mm3"

        with cf.ThreadPoolExecutor(max_workers=2) as ex:
            for (part, group, more), res in list(ex.map(scan, jobs)) + list(ex.map(one, once)) + list(ex.map(vol, vols)):
                print(f"{part:11s} vs {group:11s} {' '.join(more):24s}: {res}", flush=True)
            found, labels = 0, 0
            lab_z = 6.15                # the face's inlaid labels' pockets (face top 6.8, 0.6 deep): the base colour round
                                        #  the letters, not walls (counted apart)
            for (part, axis, at), res in zip(cuts, ex.map(lambda c: slivers(tmp, c[0], c[1], c[2], extra), cuts)):
                for a, x0, y0, x1, y1 in res:
                    if (axis == "z" and at >= lab_z) or (axis == "x" and x0 >= lab_z) or (axis == "y" and -y1 >= lab_z):
                        labels += 1
                        continue
                    found += 1
                    print(f"sliver < {SLIVER_W} in {part} cut {axis} = {at}: {a:.3f} mm2, "
                          f"in-plane bbox ({x0:.2f}, {-y1:.2f}) .. ({x1:.2f}, {-y0:.2f})", flush=True)
            print(f"sliver scan: {len(cuts)} cuts, {found} pieces over 0.05 mm2 (+ {labels} in the labels' layer)", flush=True)


def main() -> None:
    global CASE
    extra = sys.argv[1:]
    if extra and extra[0].endswith(".scad"):
        CASE = os.path.join(HERE, extra[0])
        main2(extra[1:])
        return
    jobs = [(p, g, ()) for p in ("shell", "lid") for g in GROUPS]
    jobs += [("cap", g, ()) for g in ("pcb", "top", "sw", "lcd2")]
    jobs += [("lid", "cell", ("ck_cell_lift=3",)), ("shell", "cell", ("ck_cell_lift=3",))]   # the cell off its floor: L, walls
    once = [("shell", g, ("ck_sweep=40",)) for g in ("pcb", "top", "sw", "bottom", "lcd2", "strips", "screws")]
    once += [("lid", "shell", ("ck_drop=0.01",)), ("cap", "shell", ())]
    # the power switch (KCD1): against every board group, the cell (lifted: its rubber), the lid; the shell it sits in
    jobs += [("switch", g, ()) for g in ("pcb", "top", "bottom", "lcd2", "strips", "screws", "lid")]
    jobs += [("switch", "cell", ("ck_cell_lift=3",))]
    once += [("switch", "shell", ("ck_psw=-0.01",))]
    # the D-pad cap pressed: on each arm (its pad down gap + travel) and on each diagonal between two arms, turned about
    # its pivot nub: against the top parts and the shell (the face must not stop the tilt), and switch by switch (the
    # pressed pad reaches into its switch's box by the travel: OVERLAP means that switch is pressed)
    tilts = [f"ck_tilt=[{x},{y},TILT_ARM]" for x, y in ((1, 0), (-1, 0), (0, 1), (0, -1))]
    tilts += [f"ck_tilt=[{x},{y},TILT_DIAG]" for x, y in ((1, 1), (1, -1), (-1, 1), (-1, -1))]
    jobs += [("cap", "top", (t,)) for t in tilts]
    once += [("cap", "shell", (t,)) for t in tilts]
    once += [("cap", "sw", (t, f'ck_ref="{r}"')) for t in tilts for r in ("SW1", "SW3", "SW4", "SW5")]
    os.makedirs(os.path.join(HERE, "..", ".t"), exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="box-check-", dir=os.path.join(HERE, "..", ".t")) as tmp:

        def scan(job):
            part, group, more = job
            prev = None
            for g in STEPS:
                if hits(tmp, part, group, g, more, extra):
                    return job, ("OVERLAP" if g <= -0.02 else "touch (0)" if g == 0.0 else f"{prev} .. {g}")
                prev = g
            return job, f">= {STEPS[-1]}"

        def one(job):
            part, group, more = job
            g = 0.0 if group == "shell" else -0.02
            return job, ("OVERLAP" if hits(tmp, part, group, g, more, extra) else "clear")

        with cf.ThreadPoolExecutor(max_workers=WORKERS) as ex:
            for (part, group, more), res in list(ex.map(scan, jobs)) + list(ex.map(one, once)):
                print(f"{part:6s} vs {group:7s} {' '.join(more):34s}: {res}", flush=True)


if __name__ == "__main__":
    main()
