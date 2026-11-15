"""DJ sets and live recordings (no annotations): decode them for the trackers.

    python -m eval.mixes prep --src data/mixes_src --out data/mixes --jobs N
writes one mono 44.1 kHz WAV per readable file and data/mixes/index.json; files soundfile cannot read are listed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

AUDIO = (".mp3", ".wav", ".flac", ".aiff", ".aif", ".ogg")
SR = 44100


def _decode(args: tuple[str, str]) -> dict:
    src, out = args
    import soundfile as sf
    import soxr
    name = "mix_" + hashlib.sha1(src.encode()).hexdigest()[:8]
    dst = Path(out) / f"{name}.wav"
    try:
        if not dst.exists():
            with sf.SoundFile(src) as f:
                sr = f.samplerate
                with sf.SoundFile(str(dst), "w", SR, 1, subtype="PCM_16") as w:
                    for block in f.blocks(blocksize=sr * 30, dtype="float32", always_2d=True):
                        y = block.mean(axis=1)
                        w.write(soxr.resample(y, sr, SR) if sr != SR else y)
        dur = sf.info(str(dst)).duration
    except Exception as exc:   # one unreadable file must not stop the others: record it and go on
        return {"src": src, "error": f"{type(exc).__name__}: {exc}"}
    return {"name": name, "src": src, "audio": dst.name, "dur": dur}


def prep(src: Path, out: Path, jobs: int) -> None:
    out.mkdir(parents=True, exist_ok=True)
    files = sorted(str(p) for p in src.rglob("*") if p.suffix.lower() in AUDIO)
    with ProcessPoolExecutor(jobs) as pool:
        done = list(pool.map(_decode, [(f, str(out)) for f in files]))
    ok = [d for d in done if "error" not in d]
    (out / "index.json").write_text(json.dumps(ok, indent=1))
    (out / "errors.json").write_text(json.dumps([d for d in done if "error" in d], indent=1))
    hours = sum(d["dur"] for d in ok) / 3600
    print(f"{len(files)} files, {len(ok)} decoded ({hours:.1f} h), {len(files) - len(ok)} unreadable (errors.json)")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["prep"])
    ap.add_argument("--src", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--jobs", type=int, default=int(os.environ.get("JOBS", "2")))
    args = ap.parse_args()
    prep(args.src, args.out, args.jobs)


if __name__ == "__main__":
    main()
