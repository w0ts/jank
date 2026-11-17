"""studies/follow-robust: the steerable DBN, the comb checks, the lock gate and the robust replay's four changes."""
from __future__ import annotations

import importlib
import sys
from pathlib import Path

import numpy as np
import pytest

STUDY = Path(__file__).resolve().parents[1] / "studies" / "follow-robust"
TEMPO = Path(__file__).resolve().parents[1] / "studies" / "tempo-changes"


def _study(name: str):
    for p in (TEMPO, STUDY):
        if str(p) not in sys.path:
            sys.path.insert(0, str(p))
    return importlib.import_module(name)


D = _study("dbn")
CB = _study("comb")
RB = _study("robust")
R = _study("rchain")
RC = _study("rconfigs")
C = _study("chain")


def pulses(bpm_sections: list[tuple[float, float]], dur: float, peak: float = 0.9, floor: float = 0.01,
           seed: int = 0) -> np.ndarray:
    """An activation: peaks (3 frames wide) at a pulse of each (bpm, until s) section, a small noise floor."""
    rng = np.random.default_rng(seed)
    n = int(dur * 100) + 1
    act = (floor + 0.01 * rng.random(n)).astype(np.float32)
    t, k = 1.0, 0
    while t < dur and k < len(bpm_sections):
        bpm, until = bpm_sections[k]
        if t >= until:
            k += 1
            continue
        f = int(round(t * 100))
        act[max(f - 1, 0):f + 2] = [peak * 0.5, peak, peak * 0.5][:len(act[max(f - 1, 0):f + 2])]
        t += 60.0 / bpm
    return act


def dbn_beats(act: np.ndarray, step: int = 5) -> np.ndarray:
    d = D.OnlineDbn(act)
    out = []
    for k in range(0, d.n_frames, step):
        out += d.advance(k + step - 1)
    return np.asarray(out)


def synth(act: np.ndarray, beats: np.ndarray | None = None) -> "C.Track":
    dur = (act.size - 1) / 100.0
    b = dbn_beats(act) if beats is None else beats
    return C.Track(b, np.full(int(dur * 10), 0.1, np.float32), np.zeros((0, 3)), act,
                   np.zeros((act.size, 2), np.float32), dur)


# ------------------------------------------------------------------------------------------------------------ the DBN

def test_dbn_is_madmoms_online_dbn():
    pytest.importorskip("madmom")
    from madmom.features.beats import DBNBeatTrackingProcessor
    act = pulses([(128.0, 20.0), (104.0, 40.0)], 40.0)
    ref = DBNBeatTrackingProcessor(fps=100, online=True, min_bpm=90, max_bpm=180)
    ref.reset()
    want = []
    for a in act:
        want.extend(float(b) for b in ref.process_online(np.atleast_1d(a), reset=False))
    got = dbn_beats(act, step=7)
    assert got.size == len(want) and np.allclose(got, want, atol=1e-12)


def test_dbn_has_the_firmwares_state_space():
    d = D.OnlineDbn(np.zeros(10, np.float32))
    assert d.intervals[0] == 33 and d.intervals[-1] == 67 and d.n_states == 1750


def test_tempo_prior_peaks_at_the_memory_and_its_octaves():
    bpms = np.array([90.0, 100.0, 120.0, 160.0, 180.0])
    w = D.tempo_prior(bpms, 90.0, width=0.03, floor=0.1)
    assert w[0] == pytest.approx(1.0) and w[4] == pytest.approx(1.0)
    assert w[2] == pytest.approx(0.1, abs=1e-3) and w[3] == pytest.approx(0.1, abs=1e-3)


def test_prior_pulls_an_uninformed_dbn_to_the_memory():
    act = np.full(1500, 0.05, np.float32)
    d = D.OnlineDbn(act)
    d.set_prior(D.tempo_prior(d.bpms, 120.0, 0.03, 0.1))
    d.advance(1499)
    mass = d.tempo_mass()
    assert abs(d.bpms[int(np.argmax(mass))] - 120.0) < 4.0
    plain = D.OnlineDbn(act)
    plain.advance(1499)
    near = np.abs(np.log2(d.bpms / 120.0)) < 0.05
    assert mass[near].sum() > 2 * plain.tempo_mass()[near].sum()


def test_reweight_and_reseed_keep_a_distribution():
    d = D.OnlineDbn(pulses([(120.0, 20.0)], 20.0))
    d.advance(1000)
    w = D.tempo_prior(d.bpms, 120.0, 0.03, 0.1)
    d.reweight(w)
    assert d.fwd.sum() == pytest.approx(1.0)
    d.reseed(w)
    assert d.fwd.sum() == pytest.approx(1.0)
    assert abs(d.bpms[int(np.argmax(d.tempo_mass()))] - 120.0) < 3.0


# ------------------------------------------------------------------------------------------------------- comb checks

def test_ratio_margin_prefers_the_true_period():
    act = pulses([(120.0, 30.0)], 30.0)
    r = CB.curve(act, 2900, 1000, CB.max_lag_for(67))
    assert CB.ratio_margin(r, 50.0) > 0.1
    assert CB.ratio_margin(r, 50.0 * 4 / 3) < 0.0      # a 3:4 lock: the true period is one of its relatives


def test_tempo_agreement_high_at_the_tempo_low_at_a_relative():
    act = pulses([(120.0, 30.0)], 30.0)
    r = CB.curve(act, 2900, 800, CB.max_lag_for(67))
    periods = np.arange(33, 68, dtype=float)
    assert CB.tempo_agreement(r, 50.0, periods) > 0.95
    assert CB.tempo_agreement(r, 50.0 * 4 / 3, periods) < 0.6


def test_phase_agreement_on_and_off_the_beat():
    act = pulses([(120.0, 30.0)], 30.0)
    lines = 1.0 + 0.5 * np.arange(30, 50)
    assert CB.phase_agreement(act, lines, 0.5, 2900) == pytest.approx(1.0)
    assert CB.phase_agreement(act, lines + 0.25, 0.5, 2900) < 0.2


def test_curve_reads_no_frame_after_end():
    act = pulses([(120.0, 30.0)], 30.0)
    poisoned = act.copy()
    poisoned[2001:] = 1.0
    a = CB.curve(act, 2000, 800, 300)
    b = CB.curve(poisoned, 2000, 800, 300)
    assert np.array_equal(a, b)


# ------------------------------------------------------------------------------------------------------- the lock gate

def test_related_is_three_four_and_two_three_folded():
    assert RB.related(160.0, 120.0, 0.04) and RB.related(90.0, 120.0, 0.04) and RB.related(180.0, 120.0, 0.04)
    assert not RB.related(126.0, 120.0, 0.04) and not RB.related(240.0, 120.0, 0.04)


def test_consistent_needs_n_beats_on_one_grid():
    b = list(1.0 + 0.5 * np.arange(40))
    assert RB.consistent(b, 0.5, 32)
    assert not RB.consistent(b, 0.5, 48)
    assert not RB.consistent(b, 0.5 * 4 / 3, 32)


def test_memory_delays_a_related_lock_only():
    mem = RB.TempoMemory(mem_s=120.0)
    mem.remember(120.0, 0.0)

    def lock_beat(bpm: float, memory) -> int:
        gate = RB.LockGate(memory=memory, related_beats=32)
        est = RB.RobustBpm(track=None, params={}, gate=gate)
        for i in range(80):
            est.update(1.0 + i * 60.0 / bpm)
            if est.mode == "steady":
                return i + 1
        return 999

    assert lock_beat(160.0, None) < 20
    assert lock_beat(160.0, mem) >= 32
    assert lock_beat(128.0, mem) == lock_beat(128.0, None)


def test_ratio_gate_refuses_a_lock_the_activation_disowns():
    act = pulses([(120.0, 40.0)], 40.0)
    periods = np.arange(33, 68, dtype=float)
    beats = 1.0 + np.arange(80) * 0.5 * 4 / 3            # the tracker on a 3:4 of the music
    gate = RB.LockGate(ratio={"margin": 0.0, "span_s": 10.0}, act=act, periods=periods)
    est = RB.RobustBpm(track=None, params={}, gate=gate)
    for b in beats[beats < 39.0]:
        est.update(float(b))
    assert est.mode == "stopped" and gate.refused_ratio > 0
    gate_ok = RB.LockGate(ratio={"margin": 0.0, "span_s": 10.0}, act=act, periods=periods)
    est_ok = RB.RobustBpm(track=None, params={}, gate=gate_ok)
    for b in 1.0 + 0.5 * np.arange(60):
        est_ok.update(float(b))
    assert est_ok.mode == "steady"


# ------------------------------------------------------------------------------------------------------- the replay

def test_flags_off_is_the_firmware_chain():
    act = pulses([(128.0, 40.0), (128.0, 60.0)], 60.0)
    act[3000:3800] = 0.02                                 # a beat-less passage
    tr = synth(act)
    fw = RC.firmware()
    a = C.replay(tr, fw.base)
    for cfg in (fw, R.RConfig("fw-dbn", fw.base, dbn=True)):
        b = R.replay(tr, cfg)
        assert np.array_equal(a.running, b.running), cfg.name
        assert np.allclose(np.nan_to_num(a.out_bpm), np.nan_to_num(b.out_bpm)), cfg.name
        assert np.allclose(np.nan_to_num(a.conf), np.nan_to_num(b.conf)), cfg.name


def test_coast_runs_through_low_confidence_and_stops_on_silence():
    act = pulses([(128.0, 90.0)], 90.0)
    act[3000:4500] = 0.02
    tr = synth(act)
    rng = np.random.default_rng(3)
    wander = tr.beats + np.where((tr.beats > 30) & (tr.beats < 45), rng.uniform(-0.1, 0.1, tr.beats.size), 0.0)
    tr = C.Track(np.sort(wander), tr.rms, tr.rate, tr.act, tr.flux, tr.dur)
    fw = RC.firmware()
    coast = RC.variant(fw, "coast", {"coast": {"hold_s": 30.0}})
    a, b = R.replay(tr, fw), R.replay(tr, coast)
    sel = (a.t > 31) & (a.t < 45)
    assert b.running[sel].mean() >= a.running[sel].mean()
    assert b.running[sel].mean() > 0.95
    rms = tr.rms.copy()
    rms[600:650] = 0.0                                    # 5 s of silence at 60 s
    quiet = C.Track(tr.beats, rms, tr.rate, tr.act, tr.flux, tr.dur)
    c = R.replay(quiet, coast)
    assert not c.running[(c.t > 61) & (c.t < 65)].any()


def test_memory_replay_runs_the_dbn_and_remembers():
    act = pulses([(128.0, 60.0)], 60.0)
    tr = synth(act)
    fw = RC.firmware()
    mem = RC.variant(fw, "mem", {"memory": {"width": 0.03, "floor": 0.1, "mode": "hold"}})
    run = R.replay(tr, mem)
    assert mem.live_dbn
    late = run.t > 30
    assert run.running[late].mean() > 0.95 and abs(np.nanmedian(run.out_bpm[late]) - 128) < 0.3


def test_agreement_gate_runs_a_clean_track():
    act = pulses([(128.0, 60.0)], 60.0)
    tr = synth(act)
    fw = RC.firmware()
    ag = RC.variant(fw, "agree", {"agree": {"threshold": 0.8, "parts": ["lines", "tempo", "phase"]}})
    run = R.replay(tr, ag)
    late = run.t > 30
    assert run.running[late].mean() > 0.95
    assert np.nanmedian(run.conf[late]) > 0.9


def test_variant_merges_and_turns_off():
    fw = RC.firmware()
    a = RC.variant(fw, "a", {"coast": {"hold_s": 15.0}, "ratio": {"margin": 0.05}})
    b = RC.variant(a, "b", {"coast": {"on_jump": "gate"}, "ratio": None})
    assert b.coast == {"hold_s": 15.0, "on_jump": "gate"} and b.ratio is None
