"""The per-track tables of pertrack.py's rows: how many tracks does the follower hold the whole way.

    python studies/tempo-changes/pertrack_report.py ROWS.jsonl --out results/per_track.md --chain tuned-l16-causal --vs today

ROWS.jsonl derives from madmom (the reference): git-ignored. The tables are summaries; the worst-N list names tracks.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
BEAT = HERE.parents[1]

SHORT_STOP_S = 10.0     # a stop up to this long is "short"
LOW_ACT = 0.4           # the live activation's median peak at the reference beats under this: a weak signal
SWING_PHASE = (0.58, 0.75)
SWING_STRENGTH = 0.6
OFFBEAT_SHARE = 0.2
EDGE_SHARE = 0.5
CATS = ("held whole", "short stops", "long stops", "wrong tempo", "never locked")


def category(r: dict) -> str:
    if not r.get("locked"):
        return "never locked"
    if r["wrongs_2s"] > 0:
        return "wrong tempo"
    if r["stops_2s"] > 0:
        return "short stops" if r["longest_stop_s"] <= SHORT_STOP_S else "long stops"
    return "held whole"


def genre_group(g: str) -> str:
    s = (g or "").lower()
    if not s:
        return "unknown"
    if any(k in s for k in ("hyperpop", "idpop", "pop", "indie", "rock", "chanson", "r&b", "soul", "jazz")):
        return "pop / hyperpop / other vocal"
    if any(k in s for k in ("ambient", "downtempo", "new age", "classical", "soundtrack", "shoegaze", "trip hop")):
        return "ambient / downtempo"
    if any(k in s for k in ("idm", "experimental", "glitch", "abstract", "leftfield", "industrial")):
        return "IDM / experimental"
    if "house" in s and "techno" not in s:
        return "house / tech house"
    if "techno" in s or "minimal" in s:
        return "techno"
    if any(k in s for k in ("trance", "psy")):
        return "trance"
    if any(k in s for k in ("electro", "break", "bass", "dubstep", "grime", "garage", "trap", "bleep", "hip hop",
                            "club", "guaracha", "dance", "acid", "synth")):
        return "electro / breaks / bass"
    return "other"


def cls_group(r: dict) -> str:
    return r["class"]


def tags(r: dict) -> list[str]:
    out = []
    if r.get("act_med") is not None and r["act_med"] < LOW_ACT:
        out.append("low activation")
    sp, ss = r.get("swing_phase"), r.get("swing_strength")
    if sp is not None and ss is not None and sp == sp and SWING_PHASE[0] <= sp <= SWING_PHASE[1] and ss >= SWING_STRENGTH:
        out.append("swing")
    if (r.get("offbeat") or 0) >= OFFBEAT_SHARE:
        out.append("off-beat slides")
    if r["class"] in ("changing", "probable"):
        out.append("tempo change")
    if r.get("locked") and r.get("bad_in_edges", 0) >= EDGE_SHARE:
        out.append("intro / outro")
    if r["octaves"]:
        out.append("half-time section")
    return out


def pct(n: int, d: int) -> str:
    return f"{n} ({100 * n / d:.0f} %)" if d else "0"


def count_table(rows: list[dict], key, title_key: str) -> list[str]:
    groups: dict[str, list[dict]] = {}
    for r in rows:
        groups.setdefault(key(r), []).append(r)
    lines = [f"| {title_key} | tracks | " + " | ".join(CATS) + " |", "|---|---|" + "---|" * len(CATS)]
    for g, rs in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        c = Counter(category(r) for r in rs)
        lines.append(f"| {g} | {len(rs)} | " + " | ".join(pct(c.get(k, 0), len(rs)) for k in CATS) + " |")
    c = Counter(category(r) for r in rows)
    lines.append(f"| **all** | **{len(rows)}** | " + " | ".join(f"**{pct(c.get(k, 0), len(rows))}**" for k in CATS) + " |")
    return lines


def med(rows: list[dict], k: str) -> float:
    v = [r[k] for r in rows if r.get("locked") and r.get(k) is not None]
    return float(np.median(v)) if v else float("nan")


def share_bins(rows: list[dict]) -> str:
    v = [r["within1"] if r.get("locked") else 0.0 for r in rows]
    bins = [(0.95, 2.0, ">= 95 %"), (0.8, 0.95, "80-95 %"), (0.5, 0.8, "50-80 %"), (-1, 0.5, "< 50 %")]
    return " | ".join(pct(sum(1 for x in v if lo <= x < hi), len(v)) for lo, hi, _ in bins)


def summary_row(name: str, rows: list[dict]) -> str:
    n = len(rows)
    c = Counter(category(r) for r in rows)
    return (f"| {name} | {n} | " + " | ".join(pct(c.get(k, 0), n) for k in CATS) +
            f" | {med(rows, 'within1'):.3f} | {med(rows, 'stopped'):.3f} | {med(rows, 'wrong'):.3f} | "
            f"{np.mean([r.get('stops_2s', 0) for r in rows]):.2f} | {np.mean([r.get('wrongs_2s', 0) for r in rows]):.2f} | "
            f"{share_bins(rows)} |")


def title_of(r: dict) -> str:
    return Path(r["file"]).stem.replace("|", "/")


def what(r: dict) -> str:
    if not r.get("locked"):
        return "never locked"
    parts = []
    for kind, t0, d, ratio, _ in r["episodes"][:3]:
        parts.append(f"stopped {d:.0f} s at {t0:.0f} s" if kind == "stop"
                     else f"wrong tempo (x{ratio:.2f} of the reference) {d:.0f} s at {t0:.0f} s")
    return "; ".join(parts) if parts else f"short gaps only ({100 * r['stopped']:.0f} % stopped)"


def worst(rows: list[dict], n: int) -> list[dict]:
    return sorted(rows, key=lambda r: (r["within1"] if r.get("locked") else -1.0))[:n]


def worst_table(rows: list[dict], vs: dict[str, dict], chain: str, other: str) -> list[str]:
    lines = [f"| # | track | BPM | genre | class | held {chain} (today) | stopped | wrong | first lock | what went wrong, when "
             "| signs |", "|---|---|---|---|---|---|---|---|---|---|---|"]
    for i, r in enumerate(rows, 1):
        o = vs.get(r["name"])
        today = f"{o['within1']:.2f}" if o and o.get("locked") else "never locked" if o else "-"
        held = f"{r['within1']:.2f}" if r.get("locked") else "never locked"
        lock = f"{r['first_lock_s']:.0f} s" if r.get("locked") else "-"
        sw = ", ".join(tags(r)) or "none of the signs"
        lines.append(f"| {i} | {title_of(r)[:70]} | {r['traktor_bpm']:.0f} | {r['genre']} | {r['class']} | {held} ({today}) | "
                     f"{100 * r.get('stopped', 1):.0f} % | {100 * r.get('wrong', 0):.0f} % | {lock} | {what(r)} | {sw} |")
    return lines


LEN_BINS = ((0, 2, "< 2 s"), (2, 5, "2-5 s"), (5, 10, "5-10 s"), (10, 30, "10-30 s"), (30, 1e9, ">= 30 s"))
RATIO_BINS = ((0, 0.8, "< 0.80"), (0.8, 0.9, "0.80-0.90"), (0.9, 0.98, "0.90-0.98"), (0.98, 1.02, "~1"),
              (1.02, 1.1, "1.02-1.10"), (1.1, 1.2, "1.10-1.20"), (1.2, 9, ">= 1.20"))


def length_table(by: dict, halves: dict, chains: list[str]) -> list[str]:
    """Tracks by the length of their longest wrong episode / longest stop (locked tracks)."""
    head = ["| half | chain | locked tracks | " + " | ".join(f"wrong {n}" for _, _, n in LEN_BINS) + " | " +
            " | ".join(f"stop {n}" for _, _, n in LEN_BINS) + " |", "|---|---|---|" + "---|" * (2 * len(LEN_BINS))]
    for hname, sel in halves.items():
        for ch in chains:
            rs = [r for r in by[ch].values() if sel(r) and r.get("locked")]
            cells = [pct(sum(1 for r in rs if lo <= r[k] < hi), len(rs)) for k in ("longest_wrong_s", "longest_stop_s")
                     for lo, hi, _ in LEN_BINS]
            head.append(f"| {hname} | {ch} | {len(rs)} | " + " | ".join(cells) + " |")
    return head


def ratio_table(rows: list[dict], min_s: float) -> list[str]:
    """The wrong episodes (the up to 6 longest of each track) of min_s or longer by the output / reference tempo ratio
    (within an octave), and how many began right after a stop."""
    eps = [e for r in rows if r.get("locked") for e in r["episodes"] if e[0] == "wrong" and e[2] >= min_s]
    lines = ["| output / reference | " + " | ".join(n for _, _, n in RATIO_BINS) + " | all | began after a stop |",
             "|---|" + "---|" * (len(RATIO_BINS) + 2)]
    cells = [pct(sum(1 for e in eps if lo <= e[3] < hi), len(eps)) for lo, hi, _ in RATIO_BINS]
    lines.append(f"| wrong episodes >= {min_s:.0f} s | " + " | ".join(cells) +
                 f" | {len(eps)} | {pct(sum(1 for e in eps if e[4]), len(eps))} |")
    return lines


def cause_table(groups: dict[str, list[dict]]) -> list[str]:
    names = ["low activation", "swing", "off-beat slides", "tempo change", "intro / outro", "half-time section"]
    lines = ["| group | tracks | " + " | ".join(names) + " | none |", "|---|---|" + "---|" * (len(names) + 1)]
    for g, rs in groups.items():
        c = Counter(t for r in rs for t in tags(r))
        none = sum(1 for r in rs if not tags(r))
        lines.append(f"| {g} | {len(rs)} | " + " | ".join(pct(c.get(t, 0), len(rs)) for t in names) +
                     f" | {pct(none, len(rs))} |")
    return lines


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("rows", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--chain", default="tuned-l16-causal")
    ap.add_argument("--vs", default="today")
    ap.add_argument("--worst", type=int, default=25)
    a = ap.parse_args()
    genres = json.loads((BEAT / "studies" / "downbeat" / "genres.json").read_text())
    by: dict[str, dict[str, dict]] = {}
    for line in a.rows.read_text().splitlines():
        r = json.loads(line)
        if "error" in r:
            raise SystemExit(f"error row: {r}")
        r["genre"] = genre_group(genres.get(r["name"], ""))
        by.setdefault(r["config"], {})[r["name"]] = r
    chains = [a.chain, a.vs]
    halves = {"test half": lambda r: r["split"] == "test", "dev half": lambda r: r["split"] == "dev",
              "all tracks": lambda r: True}
    L = ["# Per track: does the follower hold the BPM the whole way? (tuned-l16-causal, M2j, the user's Traktor set)", "",
         "Replay of the firmware's follower (`tuned-l16-causal`) and today's chain on every track of the set, scored per track "
         "against the audio reference (madmom offline beats cut into tempo sections, each change checked against Beat This!), "
         "output folded into the reference's octave, as `score.py`. Code: `pertrack.py`, `pertrack_report.py`. "
         "Per-track rows are in `beat/data/pertrack.jsonl` (git-ignored: derived from madmom).", "",
         "Definitions (everything after the track's first lock, in the music from first reference beat + 10 s):",
         "- first lock: the first tick with the outputs running;",
         "- within 1 %: running with the folded tempo within 1 % of the reference; wrong: running and more than 2 % off;",
         "- stop / wrong episode: a run of 2 s or more of stopped / wrong time;",
         "- **held whole**: no wrong episode and no stop of 2 s or more after the first lock;",
         f"- **short stops**: no wrong episode, at least one stop of 2 s or more, all of them {SHORT_STOP_S:.0f} s or shorter;",
         f"- **long stops**: no wrong episode, a stop longer than {SHORT_STOP_S:.0f} s;",
         "- **wrong tempo**: at least one wrong-tempo episode of 2 s or more (it may also have stops);",
         "- **never locked**: the outputs never ran in the music.", ""]
    c = Counter(category(r) for r in by[a.chain].values())
    o = Counter(category(r) for r in by[a.vs].values())
    n = len(by[a.chain])
    L += ["## Headline", "",
          f"{c['held whole']} of {n} tracks held the BPM the whole way ({a.chain}; {o['held whole']} with {a.vs}); "
          f"{c['short stops']} had only short stops (up to {SHORT_STOP_S:.0f} s) and {c['long stops']} longer stops, "
          f"{c['wrong tempo']} had a wrong tempo for 2 s or more at some point, {c['never locked']} never locked "
          f"({a.vs}: {o['short stops']}, {o['long stops']}, {o['wrong tempo']}, {o['never locked']}).", "",
          "Caveats: the reference's transitions between tempo sections (segments under 16 beat intervals) are left out, "
          "the output is not judged there; the half-beat Traktor grid fixes (`eval/ref_phase.py`) are about the Traktor "
          "grid's phase, and this study's reference is madmom's own beats, so they are not applied; the signs in the "
          "worst-track table are heuristics.", ""]
    for hname, sel in halves.items():
        L += [f"## {hname}", "",
              "| chain | tracks | " + " | ".join(CATS) + " | median within 1 % | median stopped | median wrong | "
              "stops >= 2 s / track | wrong episodes >= 2 s / track | within 1 % >= 95 % | 80-95 % | 50-80 % | < 50 % |",
              "|---|---|" + "---|" * (len(CATS) + 9)]
        for ch in chains:
            L.append(summary_row(ch, [r for r in by[ch].values() if sel(r)]))
        L.append("")
    L += ["## How long are the longest episodes? (the 2 s threshold decides much)", "",
          "Locked tracks by the length of their longest wrong-tempo episode and of their longest stop:", ""]
    L += length_table(by, halves, chains) + [""]
    L += ["Wrong episodes by how far off the tempo was (the output's folded tempo over the reference's, brought into one "
          "octave; 0.75 = 3:2 / 4:3 relations, 0.8 / 1.25 = 5:4, 0.9 / 1.1 = 10 %). Only the 6 longest episodes of "
          "each track are kept, all tracks:", ""]
    for ch in chains:
        for m in (2.0, 10.0):
            L += [f"{ch}:", ""] + ratio_table(list(by[ch].values()), m) + [""]
    for hname, sel in halves.items():
        for ch in chains:
            rs = [r for r in by[ch].values() if sel(r)]
            L += [f"### {hname}, {ch}: by reference class", ""]
            L += count_table(rs, cls_group, "class") + ["", f"### {hname}, {ch}: by genre", ""]
            L += count_table(rs, lambda r: r["genre"], "genre") + [""]
    hs = [r for r in by[a.chain].values() if r["octaves"]]
    L += [f"### all tracks, {a.chain}: tracks with a half-time / double-time section in the reference", ""]
    L += count_table(hs, lambda r: "half-time section", "group") + [""]
    ours = list(by[a.chain].values())
    bad = worst(ours, a.worst)
    L += [f"## The {a.worst} worst tracks ({a.chain}, all {len(ours)} tracks, by the share of time within 1 % after the first "
          "lock; never-locked tracks first)", "",
          "Names are the file names of the user's set. The numbers in brackets are today's chain on the same track. "
          "Signs are heuristics: low activation = the live activation's median peak at the reference beats under "
          f"{LOW_ACT}; swing = the strongest off-beat onset at {SWING_PHASE[0]}-{SWING_PHASE[1]} of the beat period and at least "
          f"{SWING_STRENGTH} of the on-beat's; off-beat slides = {int(OFFBEAT_SHARE * 100)} % or more of the running reference "
          f"beats in the right tempo but half a beat out; intro / outro = {int(EDGE_SHARE * 100)} % or more of the "
          "off-tempo time in the first or last 40 s; tempo change / half-time = the reference's class.", ""]
    L += worst_table(bad, by[a.vs], a.chain, a.vs) + [""]
    not_held = [r for r in ours if category(r) in ("wrong tempo", "never locked")]
    L += ["## Do the worst share a cause?", "",
          "Share of tracks with each sign: the worst tracks, every track with a wrong-tempo episode or never locked, and all "
          "tracks (the base rate).", ""]
    kinds = {k: [r for r in ours if category(r) == k] for k in CATS}
    L += cause_table({f"worst {a.worst}": bad, "wrong tempo or never locked": not_held,
                      **{k: v for k, v in kinds.items() if k in ("held whole", "short stops", "long stops")},
                      "all tracks": ours}) + [""]
    a.out.write_text("\n".join(L) + "\n")
    print(f"wrote {a.out} ({len(L)} lines)")


if __name__ == "__main__":
    main()
