"""Tempo against time around a pitch move: today's pipeline and the quick win against the true rate (one track).

    .venv/bin/python studies/advanced-follow/curves.py --track tk_82fba4fd --out studies/advanced-follow/curves.png
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1]))

import ratio_probe as RP
import replay as P
import step_stats as S
from eval.follow_bpm import FollowBpm
from eval.mode_bpm import ModeBpm

CASES = (("+3 % step", 0.03, 0.0), ("+5 % step", 0.05, 0.0), ("-4 % ramp over 30 s", -0.04, 30.0))


def main() -> None:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    ap = argparse.ArgumentParser()
    ap.add_argument("--track", default="tk_82fba4fd")
    ap.add_argument("--at", type=float, default=69.4315)
    ap.add_argument("--cache", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    fig, axes = plt.subplots(len(CASES), 1, figsize=(9, 3.0 * len(CASES)), sharex=False)
    for ax, (label, change, ramp) in zip(axes, CASES):
        case = {"track": args.track, "change": change, "at": args.at, "ramp_s": ramp}
        beats, ref, info = S.case_beats(case, args.cache)
        t0 = info["t_change"]
        span = (t0 - 10, t0 + max(30.0, ramp + 20))
        events = RP.events(info["ratio"])
        for name, factory, conf, colour, ratio in (("today", ModeBpm, "grid", "#d1495b", False),
                                                    ("quick win", FollowBpm, "lines", "#00798c", False),
                                                    ("quick win + rate from the spectrum", FollowBpm, "lines", "#edae49",
                                                     True)):
            pending = list(events) if ratio else []

            def hook(est, now, fed, pending=pending):
                while pending and pending[0][0] <= now:
                    t_ev, r = pending.pop(0)
                    est.propose_ratio(r, t_ev, since=t_ev - RP.DETECT_LAG_S)
            tr = P.run(beats, info["dur"], factory, conf_kind=conf, on_tick=hook)
            sel = (tr.t >= span[0]) & (tr.t < span[1])
            bpm = np.where(tr.running, tr.bpm, np.nan)
            ax.plot(tr.t[sel] - t0, bpm[sel], color=colour, lw=1.6, label=f"{name} (published; gap = outputs stopped)")
        tt = np.linspace(span[0], span[1], 400)
        x = np.clip((tt - t0) / ramp, 0, 1) if ramp > 0 else (tt >= t0).astype(float)
        ax.plot(tt - t0, info["bpm"] * (1 + change * x), color="#333333", lw=1, ls="--", label="true tempo")
        ax.set_title(f"{label}, {info['bpm']:.1f} BPM track")
        ax.set_ylabel("BPM")
        ax.grid(alpha=0.3)
    axes[-1].set_xlabel("seconds from the pitch move")
    axes[0].legend(loc="lower right", fontsize=8)
    fig.tight_layout()
    fig.savefig(args.out, dpi=110)
    print(args.out)


if __name__ == "__main__":
    main()
