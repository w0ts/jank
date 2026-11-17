"""live/audiostats.py: what an audio callback saw (status flags, how long it ran, gaps between calls)."""
from types import SimpleNamespace

from live.audiostats import CallbackStats


class _Flags(SimpleNamespace):
    """PortAudio's CallbackFlags: truthy when any flag is set."""

    def __bool__(self):
        return any(vars(self).values())


def flags(**kw):
    base = dict(output_underflow=False, output_overflow=False, input_underflow=False, input_overflow=False,
                priming_output=False)
    return _Flags(**{**base, **kw})


def run(stats, calls):
    """calls: (clock at entry, duration, status). The clock is fake: enter/leave take the times."""
    for t, dur, status in calls:
        t0 = stats.enter(now=t)
        stats.leave(t0, status, now=t + dur)


def test_a_quiet_stream_counts_callbacks_and_no_trouble():
    s = CallbackStats(nominal_s=0.01)
    run(s, [(i * 0.01, 0.0002, None) for i in range(100)])
    snap = s.snapshot()
    assert snap["callbacks"] == 100
    assert snap["underflows"] == 0 and snap["other_flags"] == 0
    assert snap["late_gaps"] == 0
    assert 0.19 < snap["dur_max_ms"] < 0.21


def test_an_output_underflow_is_counted_apart_from_other_flags():
    s = CallbackStats(nominal_s=0.01)
    run(s, [(0.00, 0.0001, flags(output_underflow=True)), (0.01, 0.0001, flags(priming_output=True)),
            (0.02, 0.0001, None)])
    snap = s.snapshot()
    assert snap["underflows"] == 1 and snap["other_flags"] == 1


def test_a_late_call_is_a_late_gap_and_the_longest_gap_is_kept():
    s = CallbackStats(nominal_s=0.01)
    run(s, [(0.00, 0.0001, None), (0.01, 0.0001, None), (0.035, 0.0001, None), (0.045, 0.0001, None)])
    snap = s.snapshot()
    assert snap["late_gaps"] == 1
    assert abs(snap["gap_max_ms"] - 25.0) < 0.01


def test_the_rate_is_callbacks_per_second_since_the_first_call():
    s = CallbackStats(nominal_s=0.01)
    run(s, [(i * 0.01, 0.0001, None) for i in range(201)])
    assert abs(s.snapshot()["rate_hz"] - 100.0) < 0.5


def test_the_longest_callback_and_the_p99_come_from_the_recent_window():
    s = CallbackStats(nominal_s=0.01, window=100)
    run(s, [(i * 0.01, 0.005 if i == 3 else 0.0001, None) for i in range(300)])
    snap = s.snapshot()
    assert snap["dur_max_ms"] > 4.9          # the all-time maximum survives the window
    assert snap["dur_p99_ms"] < 0.2          # the p99 of the last 100 does not see it


def test_a_stall_watch_sees_a_thread_that_holds_the_interpreter_and_names_where_it_was():
    import threading
    import time

    from live.audiostats import StallWatch

    watch = StallWatch(period_s=0.002, report_s=0.03)
    watch.start()

    def hog():
        t = time.perf_counter()
        while time.perf_counter() - t < 0.12:     # pure Python: keeps the GIL (switching every 5 ms, so the watch wakes late)
            pass

    time.sleep(0.05)
    th = threading.Thread(target=hog, name="hogger")
    th.start()
    th.join()
    time.sleep(0.05)
    watch.stop()
    snap = watch.snapshot()
    assert snap["wakeups"] > 10
    assert snap["max_ms"] > 3.0                       # woke late at least once while the hog ran
    assert "gc_max_ms" in snap and snap["gc_collections"] >= 0


def test_blocks_the_card_asked_for_in_time_but_never_got_are_counted_missing():
    s = CallbackStats(nominal_s=0.01)
    calls = [(i * 0.01, 0.0001, None) for i in range(50)] + [(0.5 + 0.05 + i * 0.01, 0.0001, None) for i in range(50)]
    run(s, calls)                                   # a 50 ms hole: 5 blocks
    snap = s.snapshot()
    assert snap["missing_blocks"] == 5
    assert 4.5 < snap["missing_pct"] < 5.5
