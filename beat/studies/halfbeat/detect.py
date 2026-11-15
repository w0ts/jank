"""Find the Traktor reference grids that are half a beat out (eval.ref_phase), write the fix file the scorers take
with --ref-fix, and a table of the flagged tracks with what they share.

Sources: madmom OFFLINE beats (the primary; the downbeat study's mmdb cache, RNNDownBeatProcessor + DBN, a measuring
instrument), and the saved online runs of other trackers (runs/traktor.*_<tracker>.json): madmom online (our live
tracker's family), BeatNet, Beat This! streaming (104 tracks).

    .venv/bin/python -m studies.halfbeat.detect [--mmdb data/mmdb_traktor] [--nml collection.nml] [--out studies/halfbeat]

The mmdb cache (studies/downbeat/v2/mmdb.py, ~39 min with 12 workers on gpufarm) is madmom output (CC BY-NC-SA): it
stays in the git-ignored data/; fixes.json only selects and scores, it is never a training target.
Results 2026-11-15 (results/): 33 of 829 grids half a beat out, 3 where madmom is the one off, 22 unclear;
eval.rescore --ref-fix fixes.json (rescore_*.txt) and the downbeat study's final.py --ref-fix (v2_tables_halfbeat_*:
excl = left out of both halves, testonly = of the test half, shift = grids moved, their '1' by madmom's downbeats).
"""
from __future__ import annotations

import argparse
import json
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np

from eval import ref_phase as rp
from eval.data import ROOT, traktor
from eval.traktor import _relative_path

ONLINE = {"madmom_online": "madmom_online", "beatnet": "beatnet_online_pf", "beat_this": "beat_this_small0_stream_L5"}
WARMUP = {"madmom_offline": 5.0}       # offline beats need no settling; the online trackers take rp.WARMUP_S


def online_beats(tracker: str) -> dict[str, np.ndarray]:
    out = {}
    for p in sorted((ROOT / "runs").glob(f"traktor.*of*_{tracker}.json")):
        for r in json.loads(p.read_text())["pieces"]:
            if "error" not in r and isinstance(r.get("est_beats"), list):
                est = np.asarray(r["est_beats"], dtype=float)
                if "window" in r:      # an excerpt: its beats count from the window's start, and it starts cold
                    est = est[est >= rp.WARMUP_S] + r["window"][0]
                out[r["name"]] = est
    return out


def mmdb_beats(d: Path) -> dict[str, tuple[np.ndarray, np.ndarray]]:
    out = {}
    for f in sorted(d.glob("tk_*.npz")):
        z = np.load(f)
        out[f.stem] = (np.asarray(z["beats"], dtype=float), np.asarray(z["pos"], dtype=int))
    return out


def nml_meta(nml: Path) -> dict[str, dict]:
    """Per track file (relative to the tracks folder): the collection's tags that might explain a bad grid."""
    import hashlib
    out = {}
    for e in ET.parse(nml).getroot().find("COLLECTION").findall("ENTRY"):
        loc, info, tempo = e.find("LOCATION"), e.find("INFO"), e.find("TEMPO")
        if loc is None or info is None:
            continue
        file = _relative_path(loc)
        if file is None:
            continue
        grid = [q for q in e.findall("CUE_V2") if q.get("TYPE") == "4"]
        mod = e.find("MODIFICATION_INFO")
        out["tk_" + hashlib.sha1(file.encode()).hexdigest()[:8]] = {
            "modified": e.get("MODIFIED_DATE"), "import": info.get("IMPORT_DATE"), "bitrate": info.get("BITRATE"),
            "genre": info.get("GENRE") or "", "ext": Path(file).suffix.lower(), "folder": file.split("/")[0],
            "bpm_quality": tempo.get("BPM_QUALITY") if tempo is not None else None,
            "grid_name": grid[0].get("NAME") if grid else None, "grid_ms": float(grid[0].get("START")) if grid else None,
            "author": mod.get("AUTHOR_TYPE") if mod is not None else None, "flags": info.get("FLAGS"),
            "n_grid": len(grid), "file": file}
    return out


def measure(refs: dict, sources: dict[str, dict]) -> dict[str, dict]:
    """Per track and source: the phases of the tempo-matched beats and the tempo share."""
    out: dict[str, dict] = defaultdict(dict)
    for src, beats in sources.items():
        for name, est in beats.items():
            if name in refs:
                ph, share = rp.beat_phases(refs[name], est, warmup_s=WARMUP.get(src, rp.WARMUP_S))
                out[name][src] = (ph, share)
    return out


def biases(meas: dict, folder_of: dict) -> dict[tuple[str, str], float]:
    """Per (source, folder): the axial bias over the folder's tracks (each track's own axial mean, one vote each)."""
    per: dict[tuple[str, str], list[float]] = defaultdict(list)
    for name, by_src in meas.items():
        for src, (ph, share) in by_src.items():
            if ph.size >= rp.MIN_BEATS and share >= rp.MIN_TEMPO_SHARE:
                per[(src, folder_of[name])].append(rp.axial_bias(ph))
    return {k: rp.axial_bias(np.asarray(v)) for k, v in per.items()}


def down_shift(ref: np.ndarray, down: np.ndarray, mm: tuple[np.ndarray, np.ndarray] | None) -> float:
    """For a grid moved by half a period: +0.5 or -0.5, whichever puts the marker's '1' nearer madmom's downbeats
    (a measurement for the downbeat study only; the beats are the same either way). +0.5 when madmom has no say."""
    if mm is None or down.size == 0:
        return 0.5
    beats, pos = mm
    mdown = beats[pos == 1]
    if mdown.size < 8:
        return 0.5
    period = rp.grid_period(ref)
    bar = 4 * period
    score = {}
    for s in (0.5, -0.5):
        g = down[0] + s * period
        ph = rp.wrap((mdown - g) / bar)
        score[s] = float(np.mean(np.abs(ph) * bar < 0.07))
    return max(score, key=score.get)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--mmdb", type=Path, default=ROOT / "data" / "mmdb_traktor", help="the mmdb cache: <name>.npz (beats, pos)")
    ap.add_argument("--nml", type=Path)
    ap.add_argument("--out", type=Path, default=ROOT / "studies" / "halfbeat")
    a = ap.parse_args()
    pieces = traktor()
    refs = {p.name: p.beats for p in pieces}
    downs = {p.name: p.downbeats for p in pieces}
    index = {r["name"]: r for r in json.loads((ROOT / "data" / "traktor" / "index.json").read_text())}
    folder_of = {n: index[n]["file"].split("/")[0] for n in refs}
    mm = mmdb_beats(a.mmdb)
    sources = {"madmom_offline": {n: b for n, (b, _) in mm.items()}, **{k: online_beats(v) for k, v in ONLINE.items()}}
    meas = measure(refs, sources)
    bias = biases(meas, folder_of)
    fixes, rows = {}, []
    for name in refs:
        by_src = meas.get(name, {})
        verdicts, detail = {}, {}
        for src, (ph, share) in by_src.items():
            b = bias.get((src, folder_of[name]), 0.0)
            verdicts[src] = rp.source_verdict(ph, share, b)
            mean, R = rp.circ_mean(rp.wrap(ph - b))
            detail[src] = {"verdict": verdicts[src], "phase": round(mean, 3), "R": round(R, 3), "n": int(ph.size),
                           "half_share": round(rp.half_share(ph, b), 3) if ph.size else None,
                           "tempo_share": round(share, 3), "bias": round(b, 3)}
        v = rp.combine(verdicts)
        fix = {"verdict": v, "shift": 0.5 if v == "traktor_off" else 0.0, "sources": detail}
        if v == "traktor_off":
            fix["down_shift"] = down_shift(refs[name], downs[name], mm.get(name))
        fixes[name] = fix
        rows.append((name, v))
    a.out.mkdir(parents=True, exist_ok=True)
    (a.out / "fixes.json").write_text(json.dumps(fixes, indent=1))
    report(fixes, index, bias, nml_meta(a.nml) if a.nml else {}, a.out / "tracks.md")


def _share(sub: list, key) -> Counter:
    return Counter(key(x) for x in sub)


def report(fixes: dict, index: dict, bias: dict, meta: dict, out: Path) -> None:
    n = len(fixes)
    cnt = Counter(f["verdict"] for f in fixes.values())
    lines = [f"# Half-beat-off Traktor grids ({n} tracks)\n",
             "Verdicts: " + ", ".join(f"{k} {cnt.get(k, 0)} ({100 * cnt.get(k, 0) / n:.1f} %)" for k in rp.VERDICTS),
             "\nSource verdicts (on / half / mixed / none): " + "; ".join(
                 f"{s} " + "/".join(str(sum(1 for f in fixes.values() if f['sources'].get(s, {}).get('verdict') == v))
                                    for v in ("on", "half", "mixed", "none"))
                 for s in ("madmom_offline", *ONLINE)),
             "\nPer-folder bias (periods, axial), madmom offline: " + "; ".join(
                 f"{f[:24]} {b:+.3f}" for (s, f), b in sorted(bias.items()) if s == "madmom_offline"),
             "\n## Flagged tracks\n",
             "| verdict | track | BPM | phase (periods, madmom offline / madmom online / BeatNet / Beat This) | "
             "half share madmom offline | R | down shift |", "|---|---|---|---|---|---|---|"]
    for name, f in sorted(fixes.items(), key=lambda x: (x[1]["verdict"], index[x[0]]["file"])):
        if f["verdict"] not in ("traktor_off", "madmom_off", "unclear"):
            continue
        s = f["sources"]
        ph = " / ".join(f"{s[k]['phase']:+.2f} {s[k]['verdict']}" if k in s else "-" for k in ("madmom_offline", *ONLINE))
        mo = s.get("madmom_offline", {})
        lines.append(f"| {f['verdict']} | {index[name]['file']} | {index[name]['bpm']:.2f} | {ph} | "
                     f"{mo.get('half_share', '-')} | {mo.get('R', '-')} | {f.get('down_shift', '')} |")
    off = [k for k, f in fixes.items() if f["verdict"] == "traktor_off"]
    ok = [k for k, f in fixes.items() if f["verdict"] == "ok"]
    lines.append("\n## What the Traktor-off tracks share (against the 'ok' tracks)\n")

    def comp(label: str, key) -> None:
        a_, b_ = _share(off, key), _share(ok, key)
        keys = sorted(set(a_) | set(b_), key=lambda k: (-a_.get(k, 0), -b_.get(k, 0)))[:12]
        lines.append(f"- {label}: " + "; ".join(
            f"{k}: {a_.get(k, 0)}/{a_.get(k, 0) + b_.get(k, 0)} off ({100 * a_.get(k, 0) / max(1, a_.get(k, 0) + b_.get(k, 0)):.0f} %)"
            for k in keys))
    comp("folder (download batch)", lambda k: index[k]["file"].split("/")[0])
    comp("sub-folder (album / compilation)", lambda k: "/".join(index[k]["file"].split("/")[:-1])[-48:])
    comp("BPM band", lambda k: f"{int(index[k]['bpm'] // 10 * 10)}s")
    comp("grid marker in the first beat (marker < 60/BPM s)", lambda k: index[k]["grid_s"] < 60 / index[k]["bpm"])
    comp("grid marker before 1 s", lambda k: index[k]["grid_s"] < 1.0)
    comp("BPM an integer (+-0.01)", lambda k: abs(index[k]["bpm"] - round(index[k]["bpm"])) < 0.01)
    if meta:
        comp("file type", lambda k: meta.get(k, {}).get("ext"))
        comp("bitrate", lambda k: meta.get(k, {}).get("bitrate"))
        comp("import year", lambda k: (meta.get(k, {}).get("import") or "?")[:4])
        comp("modified (year/month)", lambda k: "/".join((meta.get(k, {}).get("modified") or "?").split("/")[:2]))
        comp("grid marker name", lambda k: meta.get(k, {}).get("grid_name"))
        comp("BPM quality", lambda k: meta.get(k, {}).get("bpm_quality"))
        comp("genre tag", lambda k: meta.get(k, {}).get("genre") or "(none)")
        comp("FLAGS", lambda k: meta.get(k, {}).get("flags"))
    out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines[:4]))


if __name__ == "__main__":
    main()
