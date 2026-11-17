"""Test material for "same music, varied" vs "different music", built from the user's Traktor tracks (seeded).

Three families, each item with its reference beats in the item's own time base:
  real   a whole track as it is (its own breaks, breakdowns, fills; Traktor's 'tk: Break' .. 'tk: Drop' cues)
  same   a track's first minutes with one variation over 8-32 bars from the downbeat nearest 70 s:
         lpf (low-pass sweep down and back), hpf (high-pass sweep up and back), drums_out (harmonic part only, HPSS),
         gain_dip (-12..-24 dB), vocal_only (harmonic part, 300-3400 Hz band: a tempo-faithful vocal/pad-only part)
  diff   track A then track B:
         cut_same / fade_same     B varisped to A's tempo, B's downbeat on A's (a beatmatched DJ cut / crossfade)
         cut_shift / fade_shift   B at its own tempo, 3-8 % away from A's, random phase
         half                     B at exactly half or double A's tempo, beatmatched (cut or 16-bar fade)
         source                   A stops, 3 s of digital silence, then B at its own tempo (a new source)
         fades last 8, 16 or 32 bars of A's tempo, equal power; t_switch = the cut or the fade's middle.
Split by the track's position in data/traktor/index.json: even = dev, odd = test (as the earlier studies).
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

SR = 44100
SEED = 20261115
BEAT_DIR = Path(__file__).resolve().parents[2]
DATA = BEAT_DIR / "data" / "traktor"
MP3_ROOT = Path.home() / "Music" / "Tracks"
SAME_KINDS = ("lpf", "hpf", "drums_out", "gain_dip", "vocal_only")
PITCH_KINDS = ("pitch_step", "pitch_ramp", "stretch_step")    # the same track, its tempo moved from the downbeat at 70 s
PITCH_CHANGES = (0.01, 0.02, 0.03, 0.04, 0.05, 0.065, 0.08)
PITCH_RAMP_S = (1.0, 8.0)
AFTER_PITCH_S = 60.0
DIFF_KINDS = ("cut_same", "fade_same", "cut_shift", "fade_shift", "half", "source")
FADE_BARS = (8, 16, 32)
VAR_BARS = (8, 16, 32)
VAR_AT_S = 70.0
AFTER_VAR_S = 40.0      # a same item runs this long past its variation
A_FROM_S, A_LEN_S = 20.0, 90.0
B_FROM_S, B_LEN_S = 30.0, 90.0
SILENCE_GAP_S = 3.0
SHIFT_RANGE = (0.03, 0.08)
SAME_MAX = 0.08


@dataclass(frozen=True)
class Item:
    name: str
    family: str                 # real / same / diff
    kind: str
    split: str                  # dev / test
    tracks: tuple[str, ...]
    params: dict = field(default_factory=dict, hash=False, compare=False)


def load_index() -> list[dict]:
    return json.loads((DATA / "index.json").read_text())


def split_of(position: int) -> str:
    return "dev" if position % 2 == 0 else "test"


def fold_ratio(r: float) -> float:
    """A tempo ratio folded into [1/sqrt(2), sqrt(2)): what a one-octave BPM range does."""
    while r < 1 / math.sqrt(2):
        r *= 2
    while r >= math.sqrt(2):
        r /= 2
    return r


def _pairs(rng, recs: list[dict], ok, n: int) -> list[tuple[dict, dict]]:
    """n distinct (A, B) pairs drawn at random among those for which ok(bpmA, bpmB)."""
    cands = [(a, b) for a in recs for b in recs if a is not b and ok(a["bpm"], b["bpm"])]
    if not cands:
        return []
    pick = rng.choice(len(cands), size=min(n, len(cands)), replace=False)
    return [cands[i] for i in pick]


def plan(n_same: dict[str, int], n_diff: dict[str, int], seed: int = SEED) -> list[Item]:
    """Every item, deterministic: all real tracks, then n_same[split] same items per kind, n_diff[split] diff items
    per kind, tracks drawn from that split only."""
    rng = np.random.default_rng(seed)
    index = load_index()
    items = [Item(f"real/{r['name']}", "real", "track", split_of(i), (r["name"],)) for i, r in enumerate(index)]
    for split in ("dev", "test"):
        recs = [r for i, r in enumerate(index) if split_of(i) == split and r["dur"] > VAR_AT_S + 80]
        for kind in SAME_KINDS:
            for j in rng.choice(len(recs), size=n_same[split], replace=False):
                r = recs[int(j)]
                bars = int(rng.choice(VAR_BARS))
                params = {"bars": bars, "gain_db": float(rng.uniform(-24, -12))}
                items.append(Item(f"same/{kind}/{r['name']}", "same", kind, split, (r["name"],), params))
        for kind in PITCH_KINDS:
            count = n_same[split] * (1 if kind == "stretch_step" else 3)
            for k, j in enumerate(rng.choice(len(recs), size=count, replace=False)):
                r = recs[int(j)]
                change = PITCH_CHANGES[k % len(PITCH_CHANGES)] * (1 if rng.uniform() < 0.5 else -1)
                ramp = float(rng.uniform(*PITCH_RAMP_S)) if kind == "pitch_ramp" else 0.0
                params = {"change": change, "ramp_s": ramp}
                items.append(Item(f"same/{kind}/{r['name']}", "same", kind, split, (r["name"],), params))
        recs = [r for i, r in enumerate(index) if split_of(i) == split and r["dur"] > B_FROM_S + B_LEN_S + 40]
        n = n_diff[split]
        rules = {
            "cut_same": lambda a, b: abs(fold_ratio(b / a) - 1) <= SAME_MAX,
            "fade_same": lambda a, b: abs(fold_ratio(b / a) - 1) <= SAME_MAX,
            "cut_shift": lambda a, b: SHIFT_RANGE[0] <= abs(fold_ratio(b / a) - 1) <= SHIFT_RANGE[1],
            "fade_shift": lambda a, b: SHIFT_RANGE[0] <= abs(fold_ratio(b / a) - 1) <= SHIFT_RANGE[1],
            "half": lambda a, b: 0.47 <= b / a <= 0.53 or 1.88 <= b / a <= 2.12,
            "source": lambda a, b: True,
        }
        for kind in DIFF_KINDS:
            for k, (a, b) in enumerate(_pairs(rng, recs, rules[kind], n)):
                fade = 0 if kind in ("cut_same", "cut_shift", "source") else int(rng.choice(FADE_BARS))
                if kind == "half":
                    fade = 16 if k % 2 else 0
                params = {"fade_bars": fade, "phase": float(rng.uniform(0, 1))}
                items.append(Item(f"diff/{kind}/{a['name']}+{b['name']}", "diff", kind, split,
                                  (a["name"], b["name"]), params))
    return items


# ----------------------------------------------------------------------------------------------------------------- audio

def load_track(rec: dict) -> np.ndarray:
    """Mono float32 at 44.1 kHz: the decoded WAV when there (the farm), else the mp3 (the Mac)."""
    import soundfile as sf
    import soxr
    wav = DATA / rec["audio"]
    path = wav if wav.exists() else MP3_ROOT / rec["file"]
    y, sr = sf.read(str(path), dtype="float32", always_2d=True)
    y = y.mean(axis=1)
    return (soxr.resample(y, sr, SR) if sr != SR else y).astype(np.float32)


def varispeed(y: np.ndarray, r: float) -> np.ndarray:
    """Play r times faster (a pitch fader without key lock): tempo and pitch move together."""
    import soxr
    return y if abs(r - 1) < 1e-9 else soxr.resample(y, SR * r, SR).astype(np.float32)


def nearest(values, t: float) -> float:
    v = np.asarray(values, dtype=float)
    return float(v[np.argmin(np.abs(v - t))])


def _rms(y: np.ndarray) -> float:
    return float(np.sqrt(np.mean(y.astype(np.float64) ** 2))) if y.size else 0.0


def _sweep_filter(y: np.ndarray, btype: str, f_lo: float, f_hi: float, down_first: bool) -> np.ndarray:
    """A DJ filter: a 2nd-order Butterworth whose cutoff sweeps log-wise to the far end at the middle and back."""
    from scipy.signal import butter, sosfilt, sosfilt_zi
    block = 441
    n = len(y)
    out = np.empty_like(y)
    zi = None
    for s in range(0, n, block):
        x = s / max(n - 1, 1)
        depth = 1 - abs(2 * x - 1)                      # 0 at the ends, 1 in the middle
        f = f_hi * (f_lo / f_hi) ** depth if down_first else f_lo * (f_hi / f_lo) ** depth
        sos = butter(2, f, btype=btype, fs=SR, output="sos")
        if zi is None:
            zi = sosfilt_zi(sos) * y[0]
        out[s:s + block], zi = sosfilt(sos, y[s:s + block], zi=zi)
    return out


def _harmonic(y: np.ndarray) -> np.ndarray:
    import librosa
    return librosa.effects.hpss(y, margin=3.0)[0].astype(np.float32)


def _splice(y: np.ndarray, seg: np.ndarray, i0: int, ramp: int = 2205) -> np.ndarray:
    """y with y[i0:i0+len(seg)] replaced by seg, 50 ms crossfades at both edges."""
    out = y.copy()
    n = len(seg)
    w = np.ones(n, np.float32)
    r = min(ramp, n // 2)
    w[:r] = np.linspace(0, 1, r)
    w[n - r:] = np.linspace(1, 0, r)
    out[i0:i0 + n] = y[i0:i0 + n] * (1 - w) + seg * w
    return out


def rate_curve(n_out: int, t0: float, ramp_s: float, change: float) -> np.ndarray:
    """Playback rate per output sample: 1 until t0, then 1 + change, reached linearly over ramp_s (0: a step)."""
    t = np.arange(n_out) / SR
    x = np.clip((t - t0) / ramp_s, 0, 1) if ramp_s > 0 else (t >= t0).astype(float)
    return 1.0 + change * x


def render_pitch(item: Item, rec: dict, y: np.ndarray) -> tuple[np.ndarray, np.ndarray, dict]:
    """The user's case: the pitch fader moved while the track plays. pitch_step / pitch_ramp resample (tempo and pitch
    move together, as the player's varispeed); stretch_step keeps the pitch (librosa's phase vocoder, a step)."""
    change, ramp = item.params["change"], item.params["ramp_s"]
    t0 = item.params["at"] if "at" in item.params else nearest(rec["downbeats"], VAR_AT_S)
    src_end = min(len(y) / SR, t0 + AFTER_PITCH_S * (1 + change) + 1.0)
    beats = np.asarray(rec["beats"], dtype=float)
    if item.kind == "stretch_step":
        import librosa
        i0 = int(t0 * SR)
        tail = librosa.effects.time_stretch(y[i0:int(src_end * SR)], rate=1 + change).astype(np.float32)
        out = np.concatenate([y[:i0], tail])
        after = beats[beats >= t0]
        ref = np.concatenate([beats[beats < t0], t0 + (after - t0) / (1 + change)])
    else:
        n_out = int((t0 + AFTER_PITCH_S) * SR)
        pos = np.concatenate([[0.0], np.cumsum(rate_curve(n_out, t0, ramp, change))[:-1]])   # source sample per output
        keep = pos < src_end * SR - 1
        pos = pos[keep]
        out = np.interp(pos, np.arange(len(y)), y).astype(np.float32)
        ref = np.interp(beats * SR, pos, np.arange(len(pos))) / SR
        ref = ref[beats * SR <= pos[-1]]
    dur = len(out) / SR
    ref = ref[ref < dur]
    return out, ref, {"t_change": t0, "ramp_s": ramp, "change": change, "bpm": rec["bpm"],
                      "bpm_after": rec["bpm"] * (1 + change), "dur": dur, "breaks": rec["breaks"], "drops": rec["drops"]}


def render_same(item: Item, rec: dict, y: np.ndarray) -> tuple[np.ndarray, np.ndarray, dict]:
    if item.kind in PITCH_KINDS:
        return render_pitch(item, rec, y)
    period = 60.0 / rec["bpm"]
    t0 = nearest(rec["downbeats"], VAR_AT_S)
    t1 = t0 + item.params["bars"] * 4 * period
    end = min(t1 + AFTER_VAR_S, len(y) / SR)
    y = y[:int(end * SR)]
    i0, i1 = int(t0 * SR), min(int(t1 * SR), len(y))
    seg = y[i0:i1]
    if item.kind == "lpf":
        seg = _sweep_filter(seg, "lowpass", 250.0, 18000.0, down_first=True)
    elif item.kind == "hpf":
        seg = _sweep_filter(seg, "highpass", 20.0, 1200.0, down_first=False)
    elif item.kind == "drums_out":
        seg = _harmonic(seg)
    elif item.kind == "gain_dip":
        seg = seg * np.float32(10 ** (item.params["gain_db"] / 20))
    elif item.kind == "vocal_only":
        from scipy.signal import butter, sosfiltfilt
        seg = sosfiltfilt(butter(4, (300.0, 3400.0), btype="bandpass", fs=SR, output="sos"), _harmonic(seg))
        seg = seg.astype(np.float32)
    else:
        raise ValueError(item.kind)
    y = _splice(y, seg.astype(np.float32), i0)
    ref = np.asarray(rec["beats"], dtype=float)
    ref = ref[ref < end]
    return y, ref, {"t_var": (t0, t1), "bpm": rec["bpm"], "dur": end, "breaks": rec["breaks"], "drops": rec["drops"]}


def render_diff(item: Item, ra: dict, rb: dict, ya: np.ndarray, yb: np.ndarray) -> tuple[np.ndarray, np.ndarray, dict]:
    bpm_a, bpm_b = ra["bpm"], rb["bpm"]
    pa = 60.0 / bpm_a
    if item.kind in ("cut_same", "fade_same"):
        r = bpm_a * fold_ratio(bpm_b / bpm_a) / bpm_b      # B's own octave, A's tempo
    elif item.kind == "half":
        r = bpm_a * (0.5 if bpm_b < bpm_a else 2.0) / bpm_b
    else:
        r = 1.0
    bpm_b_out = bpm_b * r
    yb = varispeed(yb, r)
    beats_b = np.asarray(rb["beats"], dtype=float) / r
    down_b = np.asarray(rb["downbeats"], dtype=float) / r
    a0 = nearest(ra["downbeats"], A_FROM_S)
    sw = nearest(ra["downbeats"], a0 + A_LEN_S)          # A-time of the switch (a downbeat)
    fade_s = item.params["fade_bars"] * 4 * pa
    b0 = nearest(down_b, B_FROM_S / r)
    if item.kind in ("cut_shift", "fade_shift"):
        b0 += item.params["phase"] * 60.0 / bpm_b_out     # random phase against A's grid
    # B-time b0 plays at output time (sw - a0) - fade_s / 2 (+ the gap for a new source)
    gap = SILENCE_GAP_S if item.kind == "source" else 0.0
    out_sw = sw - a0                                       # output time of the cut / the fade's middle
    out_b = out_sw - fade_s / 2 + gap
    a_end = sw + fade_s / 2
    seg_a = ya[int(a0 * SR):int(a_end * SR)]
    seg_b = yb[int(b0 * SR):int((b0 + B_LEN_S + fade_s / 2) * SR)]
    gain = _rms(ya[int(a0 * SR):int(sw * SR)]) / max(_rms(seg_b), 1e-9)
    seg_b = seg_b * np.float32(min(gain, 10.0))
    n = int(round((out_b + B_LEN_S + fade_s / 2) * SR))
    y = np.zeros(n, np.float32)
    ib = int(round(out_b * SR))
    nb = min(len(seg_b), n - ib)
    if fade_s > 0:
        f0, f1 = int(round((out_sw - fade_s / 2) * SR)), int(round((out_sw + fade_s / 2) * SR))
        x = np.clip((np.arange(n) - f0) / max(f1 - f0, 1), 0, 1).astype(np.float32)
        ga, gb = np.cos(np.pi / 2 * x), np.sin(np.pi / 2 * x)
    else:
        ga = (np.arange(n) < int(round(out_sw * SR))).astype(np.float32)
        gb = 1 - ga
    y[:len(seg_a)] += seg_a[:n] * ga[:len(seg_a)]
    y[ib:ib + nb] += seg_b[:nb] * gb[ib:ib + nb]
    ref_a = np.asarray(ra["beats"], dtype=float) - a0
    ref_b = beats_b - b0 + out_b
    t_switch = out_sw + gap
    ref = np.concatenate([ref_a[(ref_a >= 0) & (ref_a < out_sw)], ref_b[(ref_b >= t_switch) & (ref_b < n / SR)]])
    info = {"t_switch": t_switch, "fade_s": fade_s, "bpm_a": bpm_a, "bpm_b": bpm_b_out, "r": r, "dur": n / SR,
            "gap_s": gap}
    return y, ref, info


def render(item: Item, recs: dict[str, dict], cache: dict | None = None) -> tuple[np.ndarray, np.ndarray, dict]:
    """(audio, reference beats, info) of one item."""
    def get(name):
        if cache is not None and name in cache:
            return cache[name]
        y = load_track(recs[name])
        if cache is not None:
            cache[name] = y
        return y
    if item.family == "real":
        rec = recs[item.tracks[0]]
        y = get(rec["name"])
        return y, np.asarray(rec["beats"], dtype=float), {"bpm": rec["bpm"], "dur": len(y) / SR,
                                                          "breaks": rec["breaks"], "drops": rec["drops"]}
    if item.family == "same":
        rec = recs[item.tracks[0]]
        return render_same(item, rec, get(rec["name"]))
    ra, rb = recs[item.tracks[0]], recs[item.tracks[1]]
    return render_diff(item, ra, rb, get(ra["name"]), get(rb["name"]))
