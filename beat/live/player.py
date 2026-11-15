"""python -m live.player [--range 90 180] [--dry-run]

A basic mp3 player in the browser whose output is the tracker's signal: open a track, play/pause, and a pitch slider
that speeds playback up or slows it down (varispeed). The page shows the tracker's mode and BPM next to the BPM
Traktor found for the track times the pitch, and this program is the Link master (Ableton Live follows).
The page is served on 127.0.0.1 only."""
from __future__ import annotations

import argparse
import json
import sys
import threading
import time
import webbrowser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from live.clockmap import ClockMap
from live.controller import LinkController
from live.engine import Engine
from live.link_port import RealLink, SimLink
from live.runner import run
from live.varispeed import VarispeedSource

ROOT = Path(__file__).resolve().parent.parent
PAGE = Path(__file__).resolve().parent / "player.html"
TRAKTOR_INDEX = ROOT / "data" / "traktor" / "index.json"
TRACKS_DIR = Path.home() / "Music" / "Tracks"
ACCURACY_DIR = ROOT / "worktrees" / "accuracy"      # player --xiao: one CSV a day of the accuracy windows
DEFAULT_PORT = 8791
MAX_OFFSET_MS = 200.0
FOLDS = (0.25, 0.5, 1.0, 2.0, 4.0)   # x2 / /2 steps, two either way
MAX_SHIFT_BEATS = 3        # the device nudges half a beat at a time
DEFAULT_MIN_CONFIDENCE = 0.8    # measured 2026-11-14: locked tracks p10 0.94-0.97, wrong tempos median ~0.4


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
        self.xiao = None     # live.xiao.XiaoOutput with --xiao: the track's mid also goes to the jank XIAO
        self.bench = None    # live.bench.XiaoBench with --xiao: the accuracy of the XIAO, this tracker and Link

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
            "min_confidence": self.engine.min_confidence, "hold": es.held, "fold": self.engine.fold, "level_db": self.engine.level_db, "level_fast_db": self.engine.level_fast_db,
            "source": "link" if self.controller.passive else "audio",
            "link_playing": self.link.playing(), "link_bpm": self.link.tempo(),
            "peers": self.link.peers(), "phase_ms": self.controller.phase_err_ms,
            "offset_ms": self.clock.user_offset_us / 1000.0,
            **self._beat_now(es),
            "xiao": self.xiao.status() if self.xiao is not None else None,
            "bench": self.bench.status() if self.bench is not None else None,
        }

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

    def command(self, name: str, body: dict) -> None:
        if name in ("xiao_beat", "xiao_stats"):
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
            self.controller.shift_beats(beats)
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
        elif name == "transport":                # LINK mode, FOLLOW + LEAD: the device starts / stops the session
            playing = body["playing"]
            if not isinstance(playing, bool):
                raise ValueError(f"playing must be true or false, not {playing!r}")
            if not self.controller.passive:
                raise ValueError("AUDIO mode: the tracker runs the transport (use hold to stop it)")
            self.controller.set_session_playing(playing)
        else:
            raise KeyError(name)


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
                self._send(HTTPStatus.OK, PAGE.read_bytes(), "text/html; charset=utf-8")
            elif self.path == "/api/status":
                self._json(HTTPStatus.OK, app.status())
            elif self.path == "/api/library":
                self._json(HTTPStatus.OK, app.library)
            else:
                self._json(HTTPStatus.NOT_FOUND, {"error": "not found"})

        def do_POST(self) -> None:
            name = self.path.removeprefix("/api/")
            try:
                length = int(self.headers.get("Content-Length") or 0)
                body = json.loads(self.rfile.read(length) or b"{}")
                app.command(name, body)
            except KeyError:
                self._json(HTTPStatus.NOT_FOUND, {"error": f"unknown command {name!r}"})
            except (ValueError, TypeError, OSError) as exc:
                self._json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
            else:
                self._json(HTTPStatus.OK, app.status())

        def log_message(self, *_) -> None:     # the console carries the tracker's lines, not every poll
            return

    return Handler


def serve(port: int, app: App) -> ThreadingHTTPServer:
    try:
        return ThreadingHTTPServer(("127.0.0.1", port), make_handler(app))
    except OSError:
        return ThreadingHTTPServer(("127.0.0.1", 0), make_handler(app))     # taken: let the OS pick one


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
    args = p.parse_args(argv)

    # --xiao: only the XIAO is on Link (it leads, Live follows): this program never joins, its own model runs on a local
    # SimLink, and the Link session's beat comes from the XIAO's own reports (live.bench)
    link = SimLink() if args.dry_run or args.xiao else RealLink()
    xiao = make_xiao() if args.xiao else None
    source = VarispeedSource(link.now_us)
    clock = ClockMap(args.offset_ms)
    controller = LinkController(link, clock)
    engine = Engine(*args.range, min_confidence=args.min_confidence)
    app = App(source, engine, link, controller, clock, load_library(), tuple(args.range))
    if xiao is not None:
        from live.accuracy import load_beats
        from live.bench import XiaoBench
        app.xiao = xiao
        source.tap = xiao.push
        app.bench = XiaoBench(link.now_us, source, engine, clock, xiao, load_beats(TRAKTOR_INDEX, TRACKS_DIR),
                              ACCURACY_DIR / f"accuracy-{time.strftime('%Y%m%d')}.csv", tuple(args.range))
        app.bench.start()
        xiao.start()
    server = serve(args.port, app)
    url = f"http://127.0.0.1:{server.server_address[1]}/"
    threading.Thread(target=server.serve_forever, daemon=True).start()
    mode = f'XIAO on "{xiao.device_name}", local Link model' if xiao else "dry run" if args.dry_run else "Link master"
    print(f"player at {url}  ({mode}; {len(app.library)} Traktor tracks; "
          f"output latency {source.latency_ms:.1f} ms; Ctrl-C quits)", flush=True)
    if not args.no_browser:
        webbrowser.open(url)
    try:
        run(source, engine, clock, link, controller)
    except KeyboardInterrupt:
        pass
    finally:
        server.shutdown()
        link.close()
        if xiao is not None:
            app.bench.close()
            xiao.close()
    return 0


def make_xiao():
    """The second output on the jank XIAO; exits with the reason when there is none."""
    from live.xiao import XiaoOutput
    try:
        return XiaoOutput()
    except RuntimeError as exc:
        raise SystemExit(f"--xiao: {exc}") from exc


if __name__ == "__main__":
    sys.exit(main())
