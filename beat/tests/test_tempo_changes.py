"""studies/tempo-changes: the reference's tempo segmentation, the replay's scoring and the weak-signal follower."""
from __future__ import annotations

import importlib
import sys
from pathlib import Path

import numpy as np
import pytest

STUDY = Path(__file__).resolve().parents[1] / "studies" / "tempo-changes"


def _study(name: str):
    """A module of the study folder (its name has a dash: not a package)."""
    if str(STUDY) not in sys.path:
        sys.path.insert(0, str(STUDY))
    return importlib.import_module(name)


RT = _study("reftempo")


def beats_from(bpms: list[tuple[float, int]], jitter_s: float = 0.005, seed: int = 0) -> np.ndarray:
    """Beats of consecutive sections (bpm, beats), with Gaussian jitter."""
    rng = np.random.default_rng(seed)
    t, out = 1.0, []
    for bpm, n in bpms:
        for _ in range(n):
            out.append(t)
            t += 60.0 / bpm
    return np.asarray(out) + rng.normal(0, jitter_s, len(out))


def ramp_beats(b0: float, b1: float, n: int, before: int = 64, after: int = 64, seed: int = 0) -> np.ndarray:
    rng = np.random.default_rng(seed)
    bpm = np.concatenate([np.full(before, b0), np.exp(np.linspace(np.log(b0), np.log(b1), n)), np.full(after, b1)])
    t = 1.0 + np.concatenate([[0.0], np.cumsum(60.0 / bpm[:-1])])
    return t + rng.normal(0, 0.005, t.size)


def test_relation():
    assert RT.relation(2.0) == "octave"
    assert RT.relation(0.5) == "octave"
    assert RT.relation(1.5) == "triplet"
    assert RT.relation(0.75) == "triplet"
    assert RT.relation(1.05) == "step"
    assert RT.relation(2.1) == "step"
    assert RT.relation(1.01) == "same"


@pytest.mark.parametrize("seed", range(5))
def test_steady_is_one_segment(seed):
    segs = RT.segment(beats_from([(128.0, 600)], seed=seed))
    assert len(segs) == 1
    assert abs(segs[0].bpm0 - 128.0) < 0.3
    assert RT.track_class(RT.changes(segs)) == "steady"


def test_step_found_with_its_time():
    b = beats_from([(130.0, 200), (140.0, 200)])
    segs = RT.segment(b)
    chs = RT.changes(segs)
    steps = [c for c in chs if c.kind == "step"]
    assert len(steps) == 1
    assert abs(steps[0].t - b[200]) < 3 * 60 / 135
    assert abs(steps[0].bpm_after / steps[0].bpm_before - 140 / 130) < 0.005


def test_half_time_is_octave_not_a_change():
    b = beats_from([(140.0, 200), (70.0, 64), (140.0, 200)])
    chs = RT.changes(RT.segment(b))
    assert [c.kind for c in chs] == ["octave", "octave"]
    assert RT.track_class(chs) == "steady"


def test_ramp_is_one_change():
    segs = RT.segment(ramp_beats(120.0, 130.0, 96))
    chs = RT.changes(segs)
    real = [c for c in chs if c.kind in ("step", "ramp")]
    assert real, chs
    assert RT.track_class(chs) == "probable"           # madmom's alone
    assert RT.track_class(RT.changes(segs, ramp_beats(120.0, 130.0, 96, seed=9))) == "changing"
    lo = min(c.bpm_before for c in real)
    hi = max(c.bpm_after for c in real)
    assert lo < 122 and hi > 128


def test_confirm_by_second_source():
    b = beats_from([(130.0, 200), (140.0, 200)])
    bt_same = beats_from([(130.0, 200), (140.0, 200)], seed=7)
    bt_flat = beats_from([(130.0, 400)], seed=7)
    segs = RT.segment(b)
    assert [c.confidence for c in RT.changes(segs, bt_same) if c.kind == "step"] == ["both"]
    assert [c.confidence for c in RT.changes(segs, bt_flat) if c.kind == "step"] == ["disputed"]
    assert [c.confidence for c in RT.changes(segs, np.array([])) if c.kind == "step"] == ["madmom"]


def test_confirm_bridges_missed_beats():
    bt = beats_from([(130.0, 200), (140.0, 200)], seed=3)
    bt = np.delete(bt, [190, 195, 205, 210])
    segs = RT.segment(beats_from([(130.0, 200), (140.0, 200)]))
    assert [c.confidence for c in RT.changes(segs, bt) if c.kind == "step"] == ["both"]


def test_ref_bpm_follows_segments():
    b = beats_from([(130.0, 200), (140.0, 200)], jitter_s=0.0)
    segs = RT.segment(b)
    t = np.array([b[50], b[300], b[-1] + 10])
    r = RT.ref_bpm(segs, t)
    assert abs(r[0] - 130) < 0.2 and abs(r[1] - 140) < 0.2 and np.isnan(r[2])


# ------------------------------------------------------------------------------------------------- the replay chain

C = _study("chain")
S = _study("score")
W = _study("weak")
CF = _study("configs")


def synth_track(beats: np.ndarray, dur: float, peak: float = 0.9, weak_from: float | None = None,
                weak_to: float | None = None, weak_peak: float = 0.08) -> "C.Track":
    """Activation peaks at the beats (weak_peak inside [weak_from, weak_to)), music all along."""
    n = int(dur * 100) + 1
    act = np.full(n, 0.005, np.float32)
    for b in beats:
        f = int(round(b * 100))
        if f < n:
            inside = weak_from is not None and weak_from <= b < weak_to
            act[f] = weak_peak if inside else peak
    return C.Track(np.asarray(beats, float), np.full(int(dur * 10), 0.1, np.float32), np.zeros((0, 3)), act,
                   np.zeros((n, 2), np.float32), dur)


def test_replay_steady_runs_at_the_tempo():
    beats = beats_from([(128.0, 300)], jitter_s=0.003)
    tr = synth_track(beats, float(beats[-1]) + 1)
    for cfg in (CF.TODAY, CF.M2I, CF.M2I_RATE, CF.WEAK):
        run = C.replay(tr, cfg)
        late = run.t > 30
        assert run.running[late].mean() > 0.95, cfg.name
        assert abs(np.nanmedian(run.out_bpm[late]) - 128) < 0.2, cfg.name


def test_silence_starts_over_and_the_fold_keeps_the_range():
    beats = np.concatenate([beats_from([(128.0, 120)]), beats_from([(128.0, 120)]) + 70.0])
    tr = synth_track(beats, float(beats[-1]) + 1)
    rms = tr.rms.copy()
    rms[int(62 * 10):int(66 * 10)] = 0.0
    tr = C.Track(tr.beats, rms, tr.rate, tr.act, tr.flux, tr.dur)
    run = C.replay(tr, CF.M2I)
    assert ("silence" in [e for _, e in run.events]) or any(e == "silence" for _, e in run.events)
    assert not run.running[(run.t > 63) & (run.t < 65)].any()


def test_rate_events_wait_for_a_full_window_and_keep_the_refractory():
    est = np.array([[t, 1.02, 0.9] for t in np.arange(0.2, 30.0, 0.2)])
    ev = C.RateEvents(est)
    out = []
    for now in np.arange(0.05, 30.0, 0.05):
        out += ev.step(float(now), active=now >= 2.0)
    assert out, "no event"
    assert out[0][0] >= 2.0 + C.PROBE_WARM_S - 1e-9
    gaps = np.diff([t for t, _ in out])
    assert np.all(gaps >= C.LRATE.REFRACTORY_S - 1e-9)


def test_consts_are_restored():
    import eval.follow_bpm as FB
    before = FB.MANOEUVRE_SIGMA
    with C.consts_applied({"follow_bpm.MANOEUVRE_SIGMA": 9.0}):
        assert FB.MANOEUVRE_SIGMA == 9.0
    assert FB.MANOEUVRE_SIGMA == before
    with pytest.raises(KeyError):
        with C.consts_applied({"follow_bpm.NOPE": 1}):
            pass


def test_named_sweep_variant_merges_dicts():
    cfg = CF.named(["x"], {"x": {"base": "weak+rate", "weak": {"salience": 3.0}, "consts": {"mode_bpm.HOLD_S": 60.0}}})[0]
    assert cfg.weak == {"salience": 3.0} and cfg.consts == {"mode_bpm.HOLD_S": 60.0} and cfg.rate


# ------------------------------------------------------------------------------------------------------- scoring

def test_folded_dev_ignores_octaves():
    d = S.folded_dev(np.array([140.0, 70.0, 143.0, np.nan]), np.array([140.0, 140.0, 140.0, 140.0]))
    assert d[0] < 1e-9 and d[1] < 1e-9 and abs(d[2] - np.log(143 / 140)) < 1e-9 and np.isnan(d[3])


def test_range_break_counts_an_output_octave_jump():
    t = np.arange(0.05, 20, 0.05)
    out = np.where(t < 10, 130.0, 65.0)
    run = C.Run(t, np.ones(t.size, bool), out, t * 0, t * 0 + 0.46, out, t * 0 + 1, t * 0 + 1)
    assert S.range_breaks(run) == 1


def test_follow_delay_of_a_step():
    b = beats_from([(130.0, 200), (140.0, 200)], jitter_s=0.0)
    segs = RT.segment(b)
    chs = [c for c in RT.changes(segs) if c.kind == "step"]
    t = np.arange(0.05, b[-1], 0.05)
    t_change = chs[0].t
    out = np.where(t < t_change + 3.0, 130.0, 140.0)
    run = C.Run(t, np.ones(t.size, bool), out, t * 0, 60 / out, out, t * 0 + 1, t * 0 + 1)
    ref = RT.ref_bpm(segs, t)
    right = S.folded_dev(out, ref) <= np.log(1.01)
    f = S.follow(run, right, chs)
    assert len(f) == 1 and abs(f[0]["delay_s"] - 3.0) < 0.15


def test_quiet_passage_found():
    beats = beats_from([(128.0, 200)], jitter_s=0.0)
    tr = synth_track(beats, float(beats[-1]) + 1, weak_from=40.0, weak_to=55.0, weak_peak=0.02)
    ps = S.quiet_passages(tr.act, beats, tr.rms)
    assert len(ps) == 1 and 39.5 < ps[0][0] < 41.0 and 53.5 < ps[0][1] < 55.5


# ---------------------------------------------------------------------------------------- the weak-signal follower

def test_weak_peak_needs_salience():
    x = np.full(1000, 0.01)
    x[503] = 0.08
    assert abs(W.weak_peak(x, 5.0, 0.46, 0.12, 2.5, 0.03) - 5.03) < 0.006
    assert W.weak_peak(x, 6.0, 0.46, 0.12, 2.5, 0.03) is None


def test_weak_follower_coasts_through_a_breakdown_on_time():
    """16 s of beat-less music: small activation peaks on the true beats, and the online DBN's beats wandering off them
    (its tempo model alone): the weak follower keeps running and is on the beat when the drums return; M2i stops."""
    clean = beats_from([(128.0, 260)], jitter_s=0.0)
    rng = np.random.default_rng(5)
    gap = (clean >= 60.0) & (clean < 76.0)
    tracked = clean + np.where(gap, rng.uniform(-0.12, 0.12, clean.size), rng.normal(0, 0.003, clean.size))
    tr = synth_track(clean, float(clean[-1]) + 1, weak_from=60.0, weak_to=76.0, weak_peak=0.08)
    tr = C.Track(np.sort(tracked), tr.rms, tr.rate, tr.act, tr.flux, tr.dur)
    first = int(np.searchsorted(clean, 76.0))
    weak_run = C.replay(tr, CF.WEAK)
    sel = (weak_run.t > 61) & (weak_run.t < 76)
    assert weak_run.running[sel].mean() > 0.95
    k = int(np.searchsorted(weak_run.t, clean[first])) - 1
    g0, gp = weak_run.g0[k], weak_run.gp[k]
    err = (clean[first] - g0) % gp
    assert min(err, gp - err) < 0.03
    m2i = C.replay(tr, CF.M2I)
    assert m2i.running[sel].mean() < weak_run.running[sel].mean()


def test_fast_jump_follows_a_big_step_sooner():
    beats = beats_from([(138.0, 200), (110.0, 120)], jitter_s=0.003)
    tr = synth_track(beats, float(beats[-1]) + 1)
    t_change = float(beats[200])
    fast = C.Config("fast", follow=True, conf="lines", fast_jump=0.06, clear_lines=True)

    def first_right(run):
        ok = run.running & (np.abs(run.out_bpm / 110.0 - 1) < 0.01) & (run.t > t_change)
        return float(run.t[np.argmax(ok)]) - t_change if ok.any() else np.inf

    assert first_right(C.replay(tr, fast)) < first_right(C.replay(tr, CF.M2I)) - 2.0


def test_fast_jump_ignores_an_octave_and_takes_a_new_tempo():
    from eval.follow_bpm import FollowBpm

    def locked_then(bpm2: float) -> bool:
        est = FollowBpm()
        base = beats_from([(140.0, 120)], jitter_s=0.002, seed=4)
        for b in base:
            est.update(float(b))
        jumped = False
        for b in base[-1] + 60.0 / bpm2 * np.arange(1, 9):
            est.update(float(b))
            jumped = jumped or C.fast_jump(est, float(b), 0.06)
        return jumped

    assert not locked_then(70.0)
    assert locked_then(115.0)


def test_adaptive_r_follows_the_tracker_jitter():
    from eval.follow_bpm import R_S, FollowBpm
    est = FollowBpm()
    innov: list[float] = []
    beats = beats_from([(128.0, 120)], jitter_s=0.02, seed=2)
    r = R_S
    for b in beats:
        r = C.adapt_r(est, float(b), innov, R_S)
        est.update(float(b))
    assert r > 2 * R_S
    quiet = FollowBpm()
    innov2: list[float] = []
    for b in beats_from([(128.0, 120)], jitter_s=0.002, seed=2):
        r2 = C.adapt_r(quiet, float(b), innov2, R_S)
        quiet.update(float(b))
    assert r2 == R_S


# ------------------------------------------------------------------------------------------- synthetic steps (warps)

ST = _study("steps")


def test_warp_speeds_up_after_t0_only():
    beats = beats_from([(120.0, 300)], jitter_s=0.0)
    tr = synth_track(beats, float(beats[-1]) + 1)
    w, _ = ST.warped(tr, 1.05, 0.0, t0=70.0)
    before = np.diff(w.beats[w.beats < 69])
    after = np.diff(w.beats[w.beats > 72])
    assert abs(np.median(before) - 0.5) < 1e-3
    assert abs(np.median(after) - 0.5 / 1.05) < 1e-3
    assert w.act.size == int(w.dur * 100)


def test_warp_ramp_reaches_the_ratio():
    beats = beats_from([(120.0, 300)], jitter_s=0.0)
    tr = synth_track(beats, float(beats[-1]) + 1)
    w, _ = ST.warped(tr, 1.08, 30.0, t0=70.0)
    late = np.diff(w.beats[w.beats > 105])
    mid = np.diff(w.beats[(w.beats > 84) & (w.beats < 86)])
    assert abs(np.median(late) - 0.5 / 1.08) < 1e-3
    assert 0.5 / 1.08 < np.median(mid) < 0.5


def test_judge_measures_the_delay():
    t = np.arange(0.05, 120, 0.05)
    out = np.where(t < 75.0, 120.0, 120 * 1.05)
    run = C.Run(t, np.ones(t.size, bool), out, t * 0, 60 / out, out, t * 0 + 1, t * 0 + 1)
    j = ST.judge(run, 120.0, 1.05, 0.0, t0=70.0)
    assert abs(j["delay_s"] - 5.0) < 0.06 and j["right_before"] == 1.0 and j["stopped"] == 0.0


def test_weak_beats_on_a_consistent_new_grid_are_contrary_evidence():
    """The tracked beats slide half a beat and stay there with a weak activation: the weak follower must not hold the
    old grid forever on them (they count as misses; consistent, so the jump rule moves the grid)."""
    p = 60.0 / 128
    clean = 1.0 + p * np.arange(200)
    tracked = np.where(np.arange(200) < 120, clean, clean + p / 2)
    tr = synth_track(tracked, float(tracked[-1]) + 1, weak_from=float(tracked[120]) - 0.01, weak_to=1e9,
                     weak_peak=0.05)
    est = W.CoastBpm(track=tr, params={})
    for b in tracked:
        est.update(float(b))
    t0, period = est.grid()
    err = (tracked[-1] - t0) % period
    assert est.jumps >= 1
    assert min(err, period - err) < 0.03
