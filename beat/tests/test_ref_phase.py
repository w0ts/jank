from pathlib import Path

import numpy as np
import pytest

from eval import ref_phase as rp

REF = np.arange(0.0, 120.0, 0.5)   # 120 BPM, 2 minutes


def test_wrap_to_half_open_interval():
    assert rp.wrap(0.5) == -0.5 and rp.wrap(-0.5) == -0.5
    assert np.isclose(rp.wrap(0.75), -0.25) and np.isclose(rp.wrap(1.1), 0.1)


def test_beats_on_the_grid_have_phase_zero():
    ph, share = rp.beat_phases(REF, REF)
    assert share == 1.0 and np.allclose(ph, 0.0)
    assert rp.source_verdict(ph, share) == "on"


def test_half_period_out_is_half():
    ph, share = rp.beat_phases(REF, REF + 0.25)
    assert np.allclose(np.abs(ph), 0.5)
    assert rp.source_verdict(ph, share) == "half"


def test_double_tempo_tracker_has_no_say():
    ph, share = rp.beat_phases(REF, np.arange(0.0, 120.0, 0.25))
    assert ph.size == 0 and share == 0.0
    assert rp.source_verdict(ph, share) == "none"


def test_missed_beats_are_skipped_not_counted_as_tempo():
    est = np.delete(REF, np.arange(3, REF.size, 4))
    ph, share = rp.beat_phases(REF, est)
    assert 0.3 < share < 0.8 and np.allclose(ph, 0.0)


def test_warmup_is_skipped():
    est = np.concatenate([REF[REF < 20] + 0.25, REF[REF >= 20]])   # off-beat while settling, then right
    ph, share = rp.beat_phases(REF, est)
    assert rp.source_verdict(ph, share) == "on"


def test_axial_bias_ignores_half_flips():
    phases = np.array([0.1] * 30 + [-0.4] * 10)                   # 0.1 offset, a quarter of the tracks half out
    assert np.isclose(rp.axial_bias(phases), 0.1, atol=1e-6)


def test_bias_moves_a_batch_offset_back_to_on():
    # a -0.12-period batch offset plus half a beat reads 0.38 (a quarter period is no longer the right border)
    rng = np.random.default_rng(0)
    ph = rp.wrap(0.38 + rng.normal(0, 0.06, 200))
    assert rp.source_verdict(ph, 1.0, bias=-0.12) == "half"
    ph_on = rp.wrap(-0.12 + rng.normal(0, 0.06, 200))
    assert rp.source_verdict(ph_on, 1.0, bias=-0.12) == "on"


def test_mixed_phase_is_mixed():
    ph = np.array([0.0] * 50 + [0.5] * 50)
    assert rp.source_verdict(rp.wrap(ph), 1.0) == "mixed"


@pytest.mark.parametrize("votes, want", [
    ({"madmom_offline": "half", "madmom_online": "half", "beatnet": "on"}, "traktor_off"),
    ({"madmom_offline": "half", "madmom_online": "half"}, "traktor_off"),
    ({"madmom_offline": "half", "madmom_online": "on", "beatnet": "on"}, "madmom_off"),
    ({"madmom_offline": "half", "madmom_online": "none"}, "unclear"),
    ({"madmom_offline": "on", "madmom_online": "half"}, "ok"),
    ({"madmom_offline": "on", "madmom_online": "half", "beatnet": "half"}, "unclear"),
    ({"madmom_offline": "mixed", "madmom_online": "on"}, "ok"),
    ({"madmom_offline": "none", "madmom_online": "half"}, "unclear"),
    ({"madmom_offline": "none", "madmom_online": "none"}, "no_evidence"),
    ({}, "no_evidence"),
])
def test_combine(votes, want):
    assert rp.combine(votes) == want


def test_shift_grid_by_half_stays_in_span():
    new = rp.shift_grid(REF, 0.5)
    assert np.allclose(np.diff(new), 0.5)
    assert new[0] == 0.25 and new[-1] <= REF[-1]
    ph, share = rp.beat_phases(new, REF + 0.25)
    assert rp.source_verdict(ph, share) == "on"


def test_shift_grid_adds_the_beat_before_the_first():
    ref = np.arange(0.4, 60.0, 0.5)
    new = rp.shift_grid(ref, 0.5)
    assert np.isclose(new[0], 0.15)


def test_fixed_refs_shifts_and_excludes():
    refs = {"a": REF, "b": REF, "c": REF}
    fixes = {"a": {"verdict": "traktor_off", "shift": 0.5}, "b": {"verdict": "unclear", "shift": 0.0}}
    out = rp.fixed_refs(refs, fixes)
    assert set(out) == {"a", "c"}
    assert out["a"][0] == 0.25 and out["c"] is REF


def _build_module():
    import importlib.util
    path = Path(__file__).resolve().parent.parent / "studies" / "downbeat" / "v2" / "build.py"
    spec = importlib.util.spec_from_file_location("downbeat_build", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


@pytest.mark.parametrize("down_shift, want_one", [(0.5, 1.25), (-0.5, 0.75)])
def test_downbeat_study_shift_moves_the_bar_with_the_marker(down_shift, want_one):
    shifted_ref = _build_module().shifted_ref
    t_ref = np.arange(0.0, 10.0, 0.5)
    ref_pos = (np.arange(t_ref.size) - 2) % 4 + 1          # the marker ('1') at 1.0 s
    new, pos = shifted_ref(t_ref, ref_pos, 0.5, down_shift)
    assert np.allclose(np.diff(new), 0.5) and new[0] == 0.25
    ones = new[pos == 1]
    assert np.isclose(ones[0], want_one) and np.allclose(np.diff(ones), 2.0)
