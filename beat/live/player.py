"""python -m live.player [--range 90 180] [--dry-run]

A basic mp3 player in the browser whose output is the tracker's signal: open a track, play/pause, and a pitch slider
that speeds playback up or slows it down (varispeed). The page shows the tracker's mode and BPM next to the BPM
Traktor found for the track times the pitch, and this program is the Link master (Ableton Live follows).
The page is served on 127.0.0.1 only."""
from __future__ import annotations

import argparse
import importlib.util
import random
import json
import sys
import threading
import time
import webbrowser
from dataclasses import asdict
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs

import numpy as np

from live.clockmap import ClockMap
from live.controller import LinkController
from live.downbeat import BarView
from live.engine import Engine
from live.latency import path_latencies
from live.link_port import RealLink, SimLink
from live.runner import run
from live.tracker import SAMPLE_RATE
from live.varispeed import VarispeedSource

ROOT = Path(__file__).resolve().parent.parent
PAGE = Path(__file__).resolve().parent / "player.html"
INLINE_UI = ROOT.parent / "scripts" / "inline_ui.py"     # expands the page's `<!--@include ui/...-->` markers
TRAKTOR_INDEX = ROOT / "data" / "traktor" / "index.json"
TRACKS_DIR = Path.home() / "Music" / "Tracks"
ACCURACY_DIR = ROOT / "runs" / "accuracy"      # player --xiao: one CSV a day of the accuracy windows
NUDGE_DIR = ROOT / "runs" / "downbeat"         # one JSON line per NUDGE BEAT: hand labels for the downbeat refit
DEFAULT_PORT = 8791
DEFAULT_LATENCY_S = 0.1       # the speakers' and the jank output's stream latency: measured to ride out the Mac's late callbacks
PREVIOUS_RESTART_S = 3.0      # the previous key restarts a track played longer than this, else goes back one
TAB_MAX_S = 2.0            # one tab_audio post carries at most this much audio (the page sends ~0.1 s)
MAX_OFFSET_MS = 200.0
FOLDS = (0.25, 0.5, 1.0, 2.0, 4.0)   # x2 / /2 steps, two either way
MAX_SHIFT_BEATS = 3        # the device nudges half a beat at a time
DEFAULT_MIN_CONFIDENCE = 0.8    # measured 2026-11-14: locked tracks p10 0.94-0.97, wrong tempos median ~0.4


def render_page(page: Path = PAGE) -> bytes:
    """The page with its shared ui/ sources included, read afresh so an edit of ui/* shows on reload."""
    spec = importlib.util.spec_from_file_location("inline_ui", INLINE_UI)
    inline_ui = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inline_ui)
    return inline_ui.expand_file(page).encode("utf-8")


def load_library(index: Path = TRAKTOR_INDEX, tracks: Path = TRACKS_DIR) -> list[dict]:
    """The Traktor-annotated tracks that exist on disk: [{path, name, bpm}], sorted by name."""
    if not index.is_file():
        return []
    rows = json.loads(index.read_text())
    out = []
    for r in rows:
        p = tracks / r["file"]
        if p.is_file():
            out.append({"path": str(p), "name": r["file"], "bpm": r["bpm"]})
    return sorted(out, key=lambda r: r["name"].lower())


def fold(bpm: float, lo: float, hi: float) -> float:
    """Halve or double into [lo, hi] (the tracker's range), as the tracker would report it."""
    while bpm < lo:
        bpm *= 2
    while bpm > hi:
        bpm /= 2
    return bpm


class App:
    def __init__(self, source: VarispeedSource, engine: Engine, link, controller: LinkController, clock: ClockMap,
                 library: list[dict], bpm_range: tuple[float, float]) -> None:
        self.source, self.engine, self.link, self.controller, self.clock = source, engine, link, controller, clock
        self.library = library
        self.bpm_by_path = {r["path"]: r["bpm"] for r in library}
        self.range = bpm_range
        self.speaker_name = ""   # the speakers' device (--xiao): a wireless one makes the suggested beat_latency unsure
        self.xiao = None     # live.xiao.XiaoOutput with --xiao: the track's mid also goes to the jank XIAO
        self.bench = None    # live.bench.XiaoBench with --xiao: the accuracy of the XIAO, this tracker and Link
        self.queue_paths: list[str] = []     # the page's list (what "next" follows there) and its "then" choice (next / random / stop)
        self.queue_mode = "next"
        self.rng = random.Random()
        self.stall = None    # live.audiostats.StallWatch: how late a sleeping thread wakes (the GIL), gc pauses
        self.nudge_dir: Path | None = None   # the nudge log's folder (main: NUDGE_DIR); None: no log

    def status(self) -> dict:
        st = self.source.state()
        es = self.engine.status()
        traktor = self.bpm_by_path.get(st.path) if st.path else None
        expected = fold(traktor * st.rate, *self.range) if traktor else None
        return {
            "path": st.path, "position_s": st.position_s, "duration_s": st.duration_s,
            "playing": st.playing, "rate": st.rate, "volume": st.volume,
            "traktor_bpm": traktor, "expected_bpm": expected,
            "mode": es.mode, "bpm": es.bpm, "confidence": es.confidence, "silent": es.silent, "gated": es.gated,
            "min_confidence": self.engine.min_confidence, "hold": es.held, "fold": es.fold, "fold_auto": es.fold_auto, "autobar": es.autobar, "level_db": self.engine.level_db, "level_fast_db": self.engine.level_fast_db,
            "source": "link" if self.controller.passive else "audio",
            "link_playing": self.link.playing(), "link_bpm": self.link.tempo(),
            "peers": self.link.peers(), "phase_ms": self.controller.phase_err_ms,
            "offset_ms": self.clock.user_offset_us / 1000.0,
            **self._beat_now(es),
            "bar": bar_status(es.bar, self.controller.bar_moves),
            "follow": self.engine.follow_status() if hasattr(self.engine, "follow_status") else None,
            "tab": self.source.tab_active(),
            "stall": self.stall.snapshot() if self.stall is not None else None,
            "speaker_cb": getattr(getattr(self.source, "cb_stats", None), "snapshot", lambda: None)(),
            **self._latency_status(),
            "bench": self.bench.status() if self.bench is not None else None,
        }

    def _latency_status(self) -> dict:
        """xiao (its stream's numbers + path_ms), speaker (device, latency, path_ms, wireless) and the beat_latency the two
        paths suggest (live/latency.py); all None without --xiao."""
        if self.xiao is None:
            return {"xiao": None, "speaker": None, "suggested_beat_latency_ms": None}
        x = self.xiao.status()
        paths = path_latencies(self.source.latency_ms, self.speaker_name, x)
        return {"xiao": {**x, **paths["xiao"]}, "speaker": paths["speaker"],
                "suggested_beat_latency_ms": paths["suggested_beat_latency_ms"]}

    def tab_audio(self, data: bytes, rate: int) -> None:
        """A block of a browser tab's audio (little-endian float32, mono, 44.1 kHz; the last sample is 'now') goes to the
        tracker as the source's input. The page's AudioContext runs at 44.1 kHz, so no resampling happens here."""
        if rate != SAMPLE_RATE:
            raise ValueError(f"tab audio rate {rate} Hz: {SAMPLE_RATE} Hz only")
        if not data or len(data) % 4:
            raise ValueError(f"tab audio: {len(data)} bytes is not whole float32 samples")
        if len(data) > TAB_MAX_S * SAMPLE_RATE * 4:
            raise ValueError(f"tab audio: more than {TAB_MAX_S:g} s in one post")
        samples = np.frombuffer(data, dtype="<f4").astype(np.float32)
        if not np.isfinite(samples).all():
            raise ValueError("tab audio: samples are not finite")
        self.source.feed_tab(np.clip(samples, -1.0, 1.0), self.link.now_us())

    def _beat_now(self, es) -> dict:
        """Where the beat is right now, for the device emulator's BEAT LED: Link's own beat while its transport runs,
        else the estimator's grid (a tempo heard but not trusted enough), else nothing."""
        if self.controller.passive:      # following Link: only its own beat, even while the session is stopped
            return {"beat": self.link.beat_at(self.link.now_us()), "beat_bpm": self.link.tempo(), "beat_from": "link"}
        if self.link.playing():
            return {"beat": self.link.beat_at(self.link.now_us()), "beat_bpm": self.link.tempo(), "beat_from": "link"}
        if es.grid is not None and not es.silent:
            t_beat, period = es.grid
            return {"beat": (self.engine.stream_time - t_beat) / period, "beat_bpm": 60.0 / period,
                    "beat_from": "estimate"}
        return {"beat": None, "beat_bpm": None, "beat_from": None}

    def _log_nudge(self, beats: float, before: BarView) -> None:
        """Each NUDGE BEAT is a hand label for a later refit: one JSON line with the estimator's state and scores before
        it and the phase Link's bar calls the 1 after it (runs/ is git-ignored)."""
        if self.nudge_dir is None:
            return
        st = self.source.state()
        user_phi = None
        if before.count is not None and before.t_beat is not None and self.controller.beat_offset % 1.0 == 0.0:
            user_phi = self.controller.link_bar_phase(before.count, before.t_beat)
        row = {"at": time.strftime("%Y-%m-%dT%H:%M:%S"), "track": st.path, "position_s": round(st.position_s, 3),
               "rate": st.rate, "nudge_beats": beats, "beat_offset": self.controller.beat_offset,
               "user_phi": user_phi, **asdict(before)}
        self.nudge_dir.mkdir(parents=True, exist_ok=True)
        with (self.nudge_dir / f"nudges-{time.strftime('%Y%m%d')}.jsonl").open("a") as f:
            f.write(json.dumps(row) + "\n")

    def media_command(self, name: str) -> bool:
        """A media key (live/mediakeys.py): play / pause / toggle / next / previous. False: nothing to do (no track, no list)."""
        st = self.source.state()
        if name in ("play", "pause", "toggle"):
            if not st.path:
                return False
            self.source.set_playing({"play": True, "pause": False, "toggle": not st.playing}[name])
            return True
        if name == "previous" and st.path and st.position_s > PREVIOUS_RESTART_S:
            self.source.seek(0.0)
            return True
        if name not in ("next", "previous"):
            raise KeyError(name)
        paths = self.queue_paths or [r["path"] for r in self.library]
        if not paths:
            return False
        if name == "next" and self.queue_mode == "random":
            others = [p for p in paths if p != st.path]
            target = self.rng.choice(others or paths)
        else:
            step = 1 if name == "next" else -1
            target = paths[(paths.index(st.path) + step) % len(paths)] if st.path in paths else paths[0]
        self.source.load(target)
        self.source.set_playing(True)
        return True

    def now_playing(self) -> dict | None:
        """The track now, for Control Center (live/mediakeys.py)."""
        st = self.source.state()
        if not st.path:
            return None
        bpm = self.engine.status().bpm or self.bpm_by_path.get(st.path)
        return {"path": st.path, "bpm": bpm, "position_s": st.position_s, "duration_s": st.duration_s,
                "playing": st.playing, "rate": st.rate}

    def command(self, name: str, body: dict) -> None:
        if name == "queue":              # the page's list and "then" choice, kept for the media keys' next / previous
            paths, mode = body.get("paths"), body.get("mode")
            if not isinstance(paths, list) or not all(isinstance(p, str) for p in paths):
                raise ValueError("queue: paths must be a list of file paths")
            if mode not in ("next", "random", "stop"):
                raise ValueError(f"queue: mode {mode!r} is not next, random or stop")
            self.queue_paths, self.queue_mode = paths, mode
        elif name in ("xiao_beat", "xiao_stats"):
            if self.bench is None:
                raise ValueError(f"{name}: the player runs without --xiao")
            self.bench.command(name, body)
        elif name == "load":
            path = Path(str(body.get("path", ""))).expanduser()
            if not path.is_file():
                raise ValueError(f"no such file: {path}")
            self.source.load(str(path))
        elif name == "play":
            self.source.set_playing(bool(body.get("playing")))
        elif name == "rate":
            self.source.set_rate(float(body["rate"]))
        elif name == "seek":
            self.source.seek(float(body["seconds"]))
        elif name == "volume":
            self.source.set_volume(float(body["volume"]))
        elif name == "threshold":
            value = float(body["min_confidence"])
            if not 0.0 <= value <= 1.0:
                raise ValueError(f"threshold {value} outside 0..1")
            self.engine.min_confidence = value
        elif name == "offset":
            ms = float(body["ms"])
            if not -MAX_OFFSET_MS <= ms <= MAX_OFFSET_MS:
                raise ValueError(f"offset {ms} ms outside +-{MAX_OFFSET_MS:g}")
            self.clock.user_offset_us = ms * 1000.0
        elif name == "resync":
            self.engine.resync()
        elif name == "fold":                     # the device's x2 / /2 on HOME (AUDIO mode): Link gets the folded tempo
            factor = float(body["factor"])
            if factor not in FOLDS:
                raise ValueError(f"fold {factor}: one of {sorted(FOLDS)}")
            self.engine.fold = factor
        elif name == "shift":
            beats = float(body["beats"])
            if beats == 0 or abs(beats) > MAX_SHIFT_BEATS or (beats * 2) % 1:
                raise ValueError(f"shift {beats} beats: halves, 0.5..{MAX_SHIFT_BEATS} either way")
            before = self.engine.bar_view()
            self.controller.nudge_phase(beats)
            self._log_nudge(beats, before)
        elif name == "autobar":                  # off / start / always (true = start, false = off: the old API)
            self.engine.set_autobar(body["mode"] if "mode" in body else body["on"])
        elif name == "hold":
            hold = body["hold"]
            if not isinstance(hold, bool):
                raise ValueError(f"hold must be true or false, not {hold!r}")
            self.engine.hold = hold
        elif name == "tempo":
            bpm = float(body["bpm"])
            if not self.controller.passive:
                raise ValueError("tempo can be set in link mode only (in audio mode the tracker sets it)")
            if not self.range[0] <= bpm <= self.range[1]:
                raise ValueError(f"tempo {bpm} outside {self.range[0]:g}..{self.range[1]:g}")
            self.link.set_tempo(bpm, self.link.now_us())
        elif name == "mode":
            mode = body["mode"]
            if mode not in ("audio", "link"):
                raise ValueError(f"mode {mode!r} is not 'audio' or 'link'")
            self.controller.passive = mode == "link"
        elif name == "transport":                # LINK mode, FOLLOW + LEAD or alone (jank M2l): the device starts / stops it
            playing = body["playing"]
            if not isinstance(playing, bool):
                raise ValueError(f"playing must be true or false, not {playing!r}")
            if not self.controller.passive:
                raise ValueError("AUDIO mode: the tracker runs the transport (use hold to stop it)")
            self.controller.set_session_playing(playing)
        else:
            raise KeyError(name)


def bar_status(bar: BarView, moves: int) -> dict:
    """The downbeat estimator for the page: state, the 1's phase, lead and confidence, Link moves made."""
    return {"state": bar.state, "phi": bar.phi, "lead": round(bar.lead, 3), "conf": round(bar.conf, 3),
            "scores": None if bar.scores is None else [round(x, 3) for x in bar.scores], "moves": moves}


def make_handler(app: App):
    class Handler(BaseHTTPRequestHandler):
        def _send(self, code: int, body: bytes, ctype: str) -> None:
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def _json(self, code: int, obj) -> None:
            self._send(code, json.dumps(obj).encode(), "application/json")

        def do_GET(self) -> None:
            if self.path == "/":
                self._send(HTTPStatus.OK, render_page(), "text/html; charset=utf-8")
            elif self.path == "/api/status":
                self._json(HTTPStatus.OK, app.status())
            elif self.path == "/api/library":
                self._json(HTTPStatus.OK, app.library)
            else:
                self._json(HTTPStatus.NOT_FOUND, {"error": "not found"})

        def do_POST(self) -> None:
            path, _, query = self.path.partition("?")
            name = path.removeprefix("/api/")
            try:
                length = int(self.headers.get("Content-Length") or 0)
                if name == "tab_audio":
                    if length > TAB_MAX_S * SAMPLE_RATE * 4:
                        self.rfile.read(min(length, 16 << 20))      # read it off the socket, or the client sees a reset
                        raise ValueError(f"tab audio: more than {TAB_MAX_S:g} s in one post")
                    rate = int(parse_qs(query).get("rate", ["0"])[0])
                    app.tab_audio(self.rfile.read(length), rate)
                    self._json(HTTPStatus.OK, {"ok": True})
                    return
                body = json.loads(self.rfile.read(length) or b"{}")
                app.command(name, body)
            except KeyError:
                self._json(HTTPStatus.NOT_FOUND, {"error": f"unknown command {name!r}"})
            except (ValueError, TypeError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
            else:
                self._json(HTTPStatus.OK, app.status())

        def log_message(self, *_) -> None:     # no line per request
            return

    return Handler


class QosHTTPServer(ThreadingHTTPServer):
    """Each request's thread asks for the user-interactive class first (live/qos.py): a request holding the interpreter
    lock must not be run late by a busy Mac while the audio callbacks wait for it."""
    qos = True

    def process_request_thread(self, request, client_address) -> None:
        if self.qos:
            from live import qos
            qos.interactive()
        super().process_request_thread(request, client_address)


def serve(port: int, app: App, qos: bool = True) -> ThreadingHTTPServer:
    try:
        server = QosHTTPServer(("127.0.0.1", port), make_handler(app))
    except OSError:
        server = QosHTTPServer(("127.0.0.1", 0), make_handler(app))     # taken: let the OS pick one
    server.qos = qos
    return server


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m live.player", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--range", nargs=2, type=float, metavar=("MIN", "MAX"), default=(90.0, 180.0))
    p.add_argument("--offset-ms", type=float, default=0.0, help="extra latency (+: Link beats later)")
    p.add_argument("--dry-run", action="store_true", help="no Link (a local model instead)")
    p.add_argument("--min-confidence", type=float, default=DEFAULT_MIN_CONFIDENCE,
                   help="Link stops while confidence is under this (0..1; 0 = never; the page has a slider)")
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--no-browser", action="store_true", help="do not open the page")
    p.add_argument("--xiao", action="store_true",
                   help="also play the track's mid to the jank XIAO (USB audio output \"jank\"): the XIAO tracks the beat "
                        "and leads Link, so this program's own Link is the local model (as --dry-run)")
    p.add_argument("--advanced-follow", action=argparse.BooleanOptionalAction, default=None,
                   help="the advanced follow (jank M2i: the Kalman glide, the line confidence, the spectral rate); "
                        "default: on with --xiao (the firmware runs it), off without")
    p.add_argument("--speaker-latency", type=float, default=DEFAULT_LATENCY_S, metavar="S",
                   help="the speakers' stream latency in seconds (default 0.1; 0 = the device's low latency, ~0.018 s). A bigger "
                        "buffer rides out a busy Mac's late callbacks (they were audio holes: a crackle) at the cost of that delay")
    p.add_argument("--jank-latency", type=float, default=DEFAULT_LATENCY_S, metavar="S",
                   help="the same for the jank USB output (default 0.1; 0 = the device's low latency)")
    p.add_argument("--jank-resample", action=argparse.BooleanOptionalAction, default=True,
                   help="the jank FIFO follows the clocks' drift by resampling (ratio near 1; the default) instead of one-sample slips "
                        "(--no-jank-resample: they saturate at 0.23%% drift, buzz at 100 Hz and run dry)")
    p.add_argument("--no-media-keys", action="store_true",
                   help="no media keys (play/pause, next, previous from the keyboard, headphones or Control Center; macOS, needs "
                        "pyobjc-framework-MediaPlayer); with them the player runs its loop on a worker thread and Cocoa's on the main one")
    p.add_argument("--no-qos", action="store_true", help="do not ask macOS for the user-interactive class for the threads")
    p.add_argument("--no-rate", action="store_true", help="with the advanced follow: no spectral rate (jank's set rate off)")
    args = p.parse_args(argv)

    # --xiao: only the XIAO is on Link (it leads, Live follows): this program never joins, its own model runs on a local
    # SimLink, and the Link session's beat comes from the XIAO's own reports (live.bench)
    link = SimLink() if args.dry_run or args.xiao else RealLink()
    xiao = make_xiao(args.jank_latency or None, args.jank_resample) if args.xiao else None
    if not args.no_qos:
        from live import qos
        qos.interactive()
    source = VarispeedSource(link.now_us, latency=args.speaker_latency or None)
    clock = ClockMap(args.offset_ms)
    controller = LinkController(link, clock)
    advanced = args.xiao if args.advanced_follow is None else args.advanced_follow
    if advanced:
        from live.follow import follow_engine
        engine = follow_engine(*args.range, min_confidence=args.min_confidence, rate=not args.no_rate)
    else:
        engine = Engine(*args.range, min_confidence=args.min_confidence)
    if not args.xiao:       # power-on: the user's layer stopped until YES (jank M2h); --xiao: the page shows the XIAO's
        engine.hold = True
    if not args.xiao:       # AUTO RANGE's reference and the /2 anchor's place come from this program's own Link session
        engine.session_bpm = lambda: link.tempo() if link.playing() else None
        engine.place_fn = lambda t: link.beat_at(clock.to_link_us(t)) if link.playing() else None
    app = App(source, engine, link, controller, clock, load_library(), tuple(args.range))
    app.nudge_dir = NUDGE_DIR
    if xiao is not None:
        from live.accuracy import load_beats
        from live.bench import XiaoBench
        app.xiao = xiao
        app.speaker_name = speaker_device_name(source)
        source.tap = xiao.push
        app.bench = XiaoBench(link.now_us, source, engine, clock, xiao, load_beats(TRAKTOR_INDEX, TRACKS_DIR),
                              ACCURACY_DIR / f"accuracy-{time.strftime('%Y%m%d')}.csv", tuple(args.range))
        app.bench.start()
        xiao.start()
    from live.audiostats import StallWatch
    app.stall = StallWatch()
    app.stall.start()
    server = serve(args.port, app, qos=not args.no_qos)
    url = f"http://127.0.0.1:{server.server_address[1]}/"
    if args.no_qos:
        threading.Thread(target=server.serve_forever, daemon=True).start()
    else:
        from live import qos
        qos.thread(server.serve_forever, name="http").start()
    mode = f'XIAO on "{xiao.device_name}", local Link model' if xiao else "dry run" if args.dry_run else "Link master"
    if advanced:
        mode += "; advanced follow" + (" without the rate" if args.no_rate else "")
    print(f"player at {url}  ({mode}; {len(app.library)} Traktor tracks; "
          f"output latency {source.latency_ms:.1f} ms; Ctrl-C quits)", flush=True)
    if not args.no_browser:
        webbrowser.open(url)
    media = None if args.no_media_keys else start_media_keys(app)
    try:
        def loop() -> None:
            run(source, engine, clock, link, controller, out=None)     # the page shows the state: no line a second here

        if media is None:
            loop()
        else:
            from live.mediakeys import run_event_loop
            exc = run_event_loop(loop, media)
            if exc is not None and not isinstance(exc, KeyboardInterrupt):
                raise exc
    except KeyboardInterrupt:
        pass
    finally:
        server.shutdown()
        link.close()
        if xiao is not None:
            app.bench.close()
            xiao.close()
    return 0


def start_media_keys(app: App):
    """The media keys on (live/mediakeys.py), or None with the reason printed: not a Mac, or pyobjc is not installed."""
    import importlib.util
    if importlib.util.find_spec("AppKit") is None or importlib.util.find_spec("MediaPlayer") is None:
        print("media keys: off (pip install pyobjc-framework-MediaPlayer, macOS only)", flush=True)
        return None
    try:
        from live.mediakeys import MediaKeys
    except ImportError:
        print("media keys: off (pip install pyobjc-framework-MediaPlayer, macOS only)", flush=True)
        return None
    keys = MediaKeys({k: (lambda k=k: app.media_command(k)) for k in ("play", "pause", "toggle", "next", "previous")},
                     app.now_playing)
    if not keys.start():
        print("media keys: off (MediaPlayer is not available)", flush=True)
        return None
    print("media keys: on (play/pause, next, previous)", flush=True)
    return keys


def speaker_device_name(source) -> str:
    """The name of the output device the player's own stream plays on."""
    import sounddevice as sd
    return str(sd.query_devices(source._stream.device)["name"])


def make_xiao(latency: float | None = None, resample: bool = False):
    """The second output on the jank XIAO; exits with the reason when there is none."""
    from live.xiao import XiaoOutput
    try:
        return XiaoOutput(latency=latency, resample=resample)
    except RuntimeError as exc:
        raise SystemExit(f"--xiao: {exc}") from exc


if __name__ == "__main__":
    sys.exit(main())
