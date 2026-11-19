# SPDX-License-Identifier: GPL-3.0-only
"""box.scad's collision / clearance check (OpenSCAD, manifold backend).
For each (case part, board group): the smallest grow step at which the grown board boxes meet the part (an OpenSCAD
intersection that is not empty). Boxes grow in x, y and z alike, so a result "a .. b" means the clearance is at
least a and under b (L-infinity). "touch (0)" is a designed contact (the bosses on our board's top, the cap on the
switches, the cell on the lid). Then: the board coming in from below (every board box swept 40 mm down must miss the
shell) and the lid against the shell. Temporary files in ../.t/ (git-ignored), removed at the end.
    python3 box_check.py [din_depth=19.6 ...]      (extra -D overrides)"""
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


def main() -> None:
    extra = sys.argv[1:]
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
