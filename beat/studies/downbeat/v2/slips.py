"""Where do online beats lose? The true phase in the tracker's own count, (c - rp) mod 4, should be constant through a
track; every change is a count slip (a tracker jump, a dropped or extra beat the count misread, an odd bar). Ceilings:
- one constant phase per track, the best one (an oracle that knows the track's phase but not its slips);
- the true phase of the previous scored beat (an oracle that follows every slip at once).

    PYTHONPATH=..:$BL_REPO python slips.py --cache ../cache
"""
from __future__ import annotations

import argparse
import pickle
from pathlib import Path

import numpy as np

from rules import SKIP_BEATS


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--beats", default="on", help="on (tracked beats) or link (Link's beats, linkbeats.py)")
    a = ap.parse_args()
    rows = [r for r in pickle.loads((a.cache / "traktor_desc.pkl").read_bytes())["rows"] if "error" not in r]
    const, follow, slips, mins = [], [], [], []
    octave = {}
    for r in rows:
        if len(r["t_on"]) > 16 and len(r["t_ref"]) > 16:
            q = np.median(np.diff(r["t_on"])) / np.median(np.diff(r["t_ref"]))
            octave[r["name"]] = "same tempo" if abs(q - 1) < 0.06 else ("tracker at double" if abs(q - 0.5) < 0.06
                                                                         else ("tracker at half" if abs(q - 2) < 0.12
                                                                               else "other ratio"))
    groups = {}
    cause = {"gap ratio near 0.5 or 1.5 (half-beat jump)": 0, "gap ratio near 2+ (dropped beat)": 0, "other": 0}
    for r in rows:
        c, rp, t = r[f"c_{a.beats}"], r[f"rp_{a.beats}"], r[f"t_{a.beats}"]
        m = (c >= SKIP_BEATS) & (rp >= 0)
        if m.sum() < 16:
            continue
        ph = (c[m] - rp[m]) % 4
        const.append(np.bincount(ph, minlength=4).max() / len(ph))
        groups.setdefault(octave.get(r["name"], "?"), []).append(const[-1])
        follow.append(np.mean(ph[1:] == ph[:-1]))
        ch = np.flatnonzero(ph[1:] != ph[:-1]) + 1
        slips.append(len(ch))
        mins.append((t[m][-1] - t[m][0]) / 60)
        idx = np.flatnonzero(m)
        for k in ch:
            lo, hi = idx[k - 1], idx[k]
            g = np.diff(t[max(0, lo - 9):hi + 1])
            per = np.median(g[:-1]) if len(g) > 1 else g[-1]
            ratios = np.diff(t[lo:hi + 1]) / per
            if np.any(np.abs(ratios - np.round(ratios)) > 0.3):
                cause["gap ratio near 0.5 or 1.5 (half-beat jump)"] += 1
            elif np.any(np.round(ratios) >= 2):
                cause["gap ratio near 2+ (dropped beat)"] += 1
            else:
                cause["other"] += 1
    const, follow, slips = np.array(const), np.array(follow), np.array(slips)
    print(f"{len(const)} Traktor tracks, {a.beats} beats (matched, after the first 2 bars)")
    print(f"oracle constant phase per track: mean {100 * const.mean():.1f} %, median {100 * np.median(const):.1f} %, "
          f">= 90 %: {100 * np.mean(const >= 0.9):.0f} % of tracks")
    print(f"oracle following every slip (previous beat's true phase): mean {100 * follow.mean():.1f} %")
    print(f"count slips per track: median {np.median(slips):.0f}, mean {slips.mean():.1f}; per minute "
          f"{slips.sum() / sum(mins):.2f}; tracks without a slip {100 * np.mean(slips == 0):.0f} %")
    print("tracker tempo vs Traktor's (median gaps): " + "; ".join(
        f"{k}: {len(v)} tracks, constant-phase oracle {100 * np.mean(v):.1f} %" for k, v in sorted(groups.items())))
    tot = sum(cause.values())
    print("slip causes (gaps between the two scored beats): " + "; ".join(
        f"{k} {100 * v / max(tot, 1):.0f} %" for k, v in cause.items()))


if __name__ == "__main__":
    main()
