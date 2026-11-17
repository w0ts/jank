# beat-listener

Listen to a line input, follow its beat, drive Ableton Link and MIDI clock.

Moved into jank as `beat/` on 2026-11-15 (it was ~/GitHub/beat-listener, never committed there; that folder is
gone). The commands below run from this folder. Kept here but not in git:
- `.venv/`;
- `data/`: GTZAN, Traktor grids, the user's mixes;
- `runs/`.

Studies: `studies/downbeat/REPORT.md` ("which beat is the 1"; not used by the player or the firmware yet).
The C port of the tracker that runs on the device is `core/beat/` on the firmware branches.

First step: learn what quality is reachable, and at what size and delay, before choosing hardware.

## Evaluation

- `eval/trackers.py`: trackers under test. Offline ceilings (Beat This!, madmom DBN) and live ones
  (madmom online, BeatNet particle filter).
- `eval/traktor.py`: the user's tracks with Traktor's auto grids as the reference.
- `eval/grid.py`: the Link metric. At each true beat, the grid fitted from the tracker's past beats must
  land on it: tight 20 ms, loose 50 ms, wrong octave, off-beat, off.
- `eval/lookahead.py`: Beat This! with 0 to 2 s of future; the delay is charged to the grid.
- `eval/rescore.py`: the table, from saved beats.

Test set: GTZAN (999 pieces), held out from the training of Beat This! `final*`/`small*` and BeatNet model 1.
Annotations: [CPJKU/beat_this_annotations](https://github.com/CPJKU/beat_this_annotations).

```
uv venv --python 3.11 .venv
.venv/bin/python -m eval.run            # half of the free cores, one thread per worker, nice 19
.venv/bin/python -m eval.rescore
.venv/bin/python -m eval.lookahead --ckpt small0
```

The Mac is someone's desk: the runs take half of what is free and leave the rest. Long runs belong on the farm.

Data and checkpoints live in `data/` (git-ignored).

## Live (Mac)

A prototype of the jank device: it listens to music and is the Ableton Link master, Ableton Live follows.
Pipeline: madmom online beat tracking (one LSTM net, DBN tempo range narrowed to `--range`) -> `eval.mode_bpm.ModeBpm`
(steady / speeding_up / slowing_down / stopped) -> Link session tempo, beat phase (quantum 4) and start/stop.
Tempo moves only past a 0.02 BPM deadband, phase only when more than 5 ms off, stopped = Link stops, and it starts
again on the next bar once the beat is back.

Install (inside the project venv, no system install):

```
uv pip install --python .venv/bin/python sounddevice LinkPython-extern
```

Player: a basic mp3 player in the browser whose output is the tracker's signal, with a pitch slider (varispeed,
±8/16/50 %) to speed playback up or slow it down. It lists the Traktor-annotated tracks and shows the tracker's BPM
and mode next to Traktor's BPM times the pitch. Page on 127.0.0.1:8791 only.
Volume changes only what you hear. Confidence (`live/confidence.py`) = how well the last 8 s of tracked beats fit the
published grid (F-measure, 0..1); Link stops below the threshold slider (default 80 %) and on 0.5 s of silence.

```
.venv/bin/python -m live.player                # Link master; --dry-run without Link
```

Run, first mode: the program plays an mp3 through the Mac's default output and feeds the same samples to the tracker.
In Live, turn Link on (Preferences > Link, Tempo, Start Stop Sync) and Live joins this session.

```
.venv/bin/python -m live --play "/Users/mots/Music/Tracks/<some techno track>.mp3" --range 90 180
.venv/bin/python -m live --play song.mp3 --dry-run        # everything except joining Link
.venv/bin/python -m live --file song.wav                  # silent analysis as fast as possible, no Link
```

Later: a loopback input from Traktor.

1. Install BlackHole 2ch (existential.audio, a system driver: the user does this).
2. Audio MIDI Setup > + > Create Multi-Output Device: tick the real speakers/headphones and BlackHole 2ch, the speakers
   first (master), drift correction on for BlackHole.
3. Traktor > Preferences > Audio Setup > Output Monitor/Master: the Multi-Output Device (or BlackHole 2ch, then
   you hear nothing unless the Multi-Output Device is the Mac's output).
4. `.venv/bin/python -m live --device "BlackHole 2ch" --range 90 180` (`--device` takes any part of an input's name;
   `--list-devices` lists them; with neither `--play` nor `--device` the default device is "BlackHole").

Output, one line per second: `12 s  steady  127.98 BPM  peers 1  phase +1.2 ms`

- seconds of audio since the start; mode of the estimator; the BPM it publishes (`---` while stopped);
- peers: other Link apps in the session (Live = 1; `-` when there is no Link);
- phase: Link's beat minus the estimator's beat at the same moment (+ = Link early), in ms; it is corrected past 5 ms.

Latency: each audio chunk is stamped with when it is heard, from PortAudio's own output (or input) latency.
`--offset-ms` adds to that (positive = Link beats later) to line the clicks up by ear.
