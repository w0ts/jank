"""live/qos.py: the player's threads ask macOS for the top quality-of-service class."""
import sys
import threading

from live import qos


def test_the_call_answers_a_bool_and_never_raises():
    assert isinstance(qos.interactive(), bool)


def test_on_a_mac_the_calling_thread_gets_it():
    if sys.platform != "darwin":
        assert qos.interactive() is False
        return
    seen = []
    t = threading.Thread(target=lambda: seen.append(qos.interactive()))
    t.start()
    t.join()
    assert seen == [True]


def test_a_thread_started_through_the_helper_runs_its_function_after_asking(monkeypatch):
    asked = []
    monkeypatch.setattr(qos, "interactive", lambda: asked.append(threading.current_thread().name) or True)
    out = []
    t = qos.thread(lambda: out.append(1), name="probe")
    t.start()
    t.join()
    assert asked == ["probe"] and out == [1]
