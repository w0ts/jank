"""The Mac's media keys (play/pause, next, previous: the keyboard, headphones, Control Center) for the player.

MPRemoteCommandCenter delivers the keys to whichever app is "now playing"; MPNowPlayingInfoCenter is what makes the player
that app and shows the track and its BPM in Control Center. Both need a Cocoa event loop on the main thread: see
`run_event_loop`, which player.py uses (the player's own loop then runs on a worker thread). Without pyobjc
(pyobjc-framework-MediaPlayer, requirements.txt) or off a Mac the player runs as before, without the keys.
"""
from __future__ import annotations

import threading
from pathlib import Path
from typing import Any, Callable

COMMANDS = (("play", "playCommand"), ("pause", "pauseCommand"), ("toggle", "togglePlayPauseCommand"),
            ("next", "nextTrackCommand"), ("previous", "previousTrackCommand"))


class MediaKeys:
    """`actions` maps play / pause / toggle / next / previous to a function; a key without one stays disabled. `info()` gives the
    track now (path, bpm, position_s, duration_s, playing, rate) or None. `framework`: the MediaPlayer module (None: import it;
    False: not available), a stand-in in the tests."""

    def __init__(self, actions: dict[str, Callable[[], Any]], info: Callable[[], dict | None], framework: Any = None) -> None:
        self.actions, self.info, self._fw = actions, info, framework
        self.available: bool | None = None

    def _framework(self) -> Any:
        if self._fw is None:
            try:
                import MediaPlayer
                self._fw = MediaPlayer
            except ImportError:
                self._fw = False
        return self._fw

    def start(self) -> bool:
        fw = self._framework()
        self.available = bool(fw)
        if not fw:
            return False
        center = fw.MPRemoteCommandCenter.sharedCommandCenter()
        for key, getter in COMMANDS:
            cmd = getattr(center, getter)()
            action = self.actions.get(key)
            cmd.setEnabled_(action is not None)
            if action is not None:
                cmd.addTargetWithHandler_(self._handler(action))
        return True

    def _handler(self, action: Callable[[], Any]) -> Callable[[Any], int]:
        fw = self._fw

        def handle(event: Any) -> int:
            try:
                action()
            except Exception:                       # a key press must never take the player down
                return fw.MPRemoteCommandHandlerStatusCommandFailed
            return fw.MPRemoteCommandHandlerStatusSuccess

        return handle

    def update(self) -> None:
        """Write the track now into Control Center (call about once a second)."""
        fw = self._framework()
        if not fw:
            return
        now = fw.MPNowPlayingInfoCenter.defaultCenter()
        st = self.info()
        if st is None or not st.get("path"):
            now.setNowPlayingInfo_(None)
            now.setPlaybackState_(fw.MPNowPlayingPlaybackStateStopped)
            return
        playing = bool(st.get("playing"))
        bpm = st.get("bpm")
        info = {fw.MPMediaItemPropertyTitle: Path(st["path"]).stem,
                fw.MPMediaItemPropertyArtist: f"{bpm:.1f} BPM" if bpm else "",
                fw.MPMediaItemPropertyPlaybackDuration: float(st.get("duration_s", 0.0)),
                fw.MPNowPlayingInfoPropertyElapsedPlaybackTime: float(st.get("position_s", 0.0)),
                fw.MPNowPlayingInfoPropertyPlaybackRate: float(st.get("rate", 1.0)) if playing else 0.0}
        now.setNowPlayingInfo_(info)
        now.setPlaybackState_(fw.MPNowPlayingPlaybackStatePlaying if playing else fw.MPNowPlayingPlaybackStatePaused)


def run_event_loop(work: Callable[[], None], keys: MediaKeys, tick_s: float = 1.0) -> BaseException | None:
    """Run `work` on a worker thread and Cocoa's event loop on this (the main) thread until `work` ends or Ctrl-C; returns what
    `work` raised. Control Center's callbacks need the main thread's loop; `keys.update()` runs from it every tick_s."""
    from AppKit import NSApplication, NSApplicationActivationPolicyAccessory
    from PyObjCTools import AppHelper

    app = NSApplication.sharedApplication()
    app.setActivationPolicy_(NSApplicationActivationPolicyAccessory)      # no Dock icon, no menu bar
    result: dict[str, BaseException] = {}
    done = threading.Event()

    def ticker() -> None:
        while not done.wait(tick_s):
            AppHelper.callAfter(keys.update)

    def run() -> None:
        try:
            work()
        except BaseException as exc:
            result["exc"] = exc
        finally:
            done.set()
            AppHelper.callAfter(AppHelper.stopEventLoop)

    threading.Thread(target=run, name="player-loop", daemon=True).start()
    threading.Thread(target=ticker, name="nowplaying", daemon=True).start()
    AppHelper.runConsoleEventLoop(installInterrupt=True, maxTimeout=0.5)
    done.set()
    return result.get("exc")
