"""python -m live --play song.mp3 --range 90 180     play a file and make Link follow its beat
python -m live --device "BlackHole 2ch"             listen to a loopback input (Traktor's output)
python -m live --file song.wav                      analyse a file silently, as fast as possible (no Link)"""
from __future__ import annotations

import argparse
import sys

from live.clockmap import ClockMap


def parse(argv: list[str] | None) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="python -m live", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    src = p.add_mutually_exclusive_group()
    src.add_argument("--play", metavar="FILE", help="play this file through the default output and follow it")
    src.add_argument("--file", metavar="FILE", help="analyse this file silently (fast; with --realtime paced)")
    src.add_argument("--device", metavar="NAME", help='input device, a part of its name (default "BlackHole")')
    p.add_argument("--range", nargs=2, type=float, metavar=("MIN", "MAX"), default=(90.0, 180.0),
                   help="BPM range of the tracker (default 90 180)")
    p.add_argument("--offset-ms", type=float, default=0.0,
                   help="extra latency to add on top of the device's reported one (+: Link beats later)")
    p.add_argument("--realtime", action="store_true", help="with --file: pace like real time (needed for Link)")
    p.add_argument("--dry-run", action="store_true", help="everything except joining Link (a local model instead)")
    p.add_argument("--list-devices", action="store_true", help="list the audio inputs and exit")
    args = p.parse_args(argv)
    if args.range[0] >= args.range[1]:
        p.error("--range MIN must be below MAX")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse(argv)
    if args.list_devices:
        from live.audio import input_devices
        for i, name, ch in input_devices():
            print(f"{i}: {name} ({ch} in)")
        return 0

    from live.controller import LinkController
    from live.engine import Engine
    from live.link_port import RealLink, SimLink
    from live.runner import run
    from live.sources import DeviceSource, FileSource, PlaySource

    fast_file = bool(args.file) and not args.realtime
    link = None if fast_file else (SimLink() if args.dry_run else RealLink())
    now_us = link.now_us if link else (lambda: 0.0)
    try:
        if args.play:
            source = PlaySource(args.play, now_us)
        elif args.file:
            source = FileSource(args.file, realtime=args.realtime, now_us=now_us)
        else:
            source = DeviceSource(args.device, now_us)
    except (OSError, LookupError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    latency = getattr(source, "latency_ms", 0.0)
    clock = ClockMap(args.offset_ms)
    controller = LinkController(link, clock) if link else None
    mode = "dry run (no Link)" if args.dry_run else ("Link master" if link else "analysis only")
    print(f"{mode}; BPM range {args.range[0]:g}-{args.range[1]:g}; device latency {latency:.1f} ms "
          f"+ offset {args.offset_ms:g} ms", flush=True)
    engine = Engine(*args.range)
    try:
        run(source, engine, clock, link, controller)
    except KeyboardInterrupt:
        pass
    finally:
        if link:
            link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
