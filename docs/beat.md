# Beat follow (bench build)

**Bench only. The network weights in `core/beat/beat_model.c` are madmom's (CC BY-NC-SA 4.0): never push or ship an
image of this build.** Our own network will replace that file. The build says so at configure time, in the boot log, in
the `beat` answer (`beat_weights=`) and on the control page.

The stick is a USB speaker called **jank**. What the computer plays to it goes to a beat tracker on core 0 (madmom's
online tracker in C: `core/beat/`), and the tracker is the Link session's master: its tempo, beat grid and stop/start go
into the session (`core/jank_beatlink.c`). Core 1 stays the MIDI clock's alone.

## Build and install

```
JANK_SCRATCH=$PWD/../usbaudio-scratch JANK_JOBS=3 scripts/build.sh xiao
```

The app image is `$JANK_SCRATCH/build-xiao/jank.bin`. The build makes `third_party/esp_tinyusb` from the registry's 3.0.0
with `third_party/esp_tinyusb-uac2.patch` (the audio class on) and sets the CPU to 240 MHz (`firmware/sdkconfig.beat`;
the release runs 160). It installs over a stick running fw/ota with `python scripts/ota.py <jank.bin>` (app only: the
bootloader and the partition table stay). Settings in NVS are kept; `beat` is not stored and starts on; `autobar` (AT
START by default) and, since M2d, `beat_latency` per audio source (the computer's 25 by default) are stored.

**Rollback.** The new image confirms itself only after the computer has configured USB *and* spoken on the MIDI control
line (ota.py's `get` after the restart does that, as does a control page), and it has run 20 s. Not confirmed within
120 s, it restarts and the bootloader starts the old image. A crash before that, or a start with no computer, also puts
the old image back (`main/ota.c`, `main/usb.c`).

## Test with a Mac

1. Plug the stick in. Audio MIDI Setup lists an output **jank**: 1 ch, 16-bit, 44100 Hz (the only format).
2. Play to it: beat-listener's player with `--xiao` (a second output stream with the mid of the track, besides the
   speakers), or set the Mac's output to jank (silent, tracker only), or make a **Multi-Output Device** of the speakers
   and jank, 44.1 kHz, the speakers as the clock source and drift correction on for jank.
3. Live (or any Link app) with Link on joins over the stick's network adapter as usual. Within some beats the session's
   tempo follows the track, the stick's LED flashes on the beat and Live's Link beat lines up with what is heard. When
   the music stops, Link stops; when it starts again, Link starts on the next bar (quantum 4).
4. Line up the phase by ear with the page's **Latency, ms** (`set beat_latency <-300..300>`, the auto base; the AUDIO
   SHIFT on top: `nudge ms`, `set lat_trim`): the music is heard that
   long after its samples reach the stick (speaker output and USB buffering); negative when the stick gets the audio
   after the speakers play it. Default 25 for the computer's audio; since M2d stored per audio source (the computer, each
   hosted USB device, the line in: docs/firmware.md), saved once the session and the clock are stopped. `set beat off` stops the
   tracker steering Link and leaves everything else as it was.

## Beat reports

On its own, with no question asked, the stick sends a SysEx `F0 7D 'J' 'B' 0x10 <text> F7` on the MIDI port: `b <n>
<bpm x100> <mode> <audio> <lag us>` on each tracked beat (n counts the beats) and `t <n> <bpm x100> <mode> <audio>
<latency ms>` once a second (n: the last beat's). `mode` is stopped, steady, faster or slower; `audio` is 1 while the
computer streams. `lag` is how long before the report the beat is taken to be heard (its sample's arrival +
`beat_latency`, as the Link grid uses it): a reader that stamps the report's arrival subtracts it to get that instant.
`latency` is the `beat_latency` setting. The control line's readers (ota.py, this page) match `F0 7D 'J' 'K'` only and
never see them (`core/jank_sysex.h`, `JX_PUSH_*`). beat-listener's `player --xiao` scores them against Traktor's beats.

## Reading the numbers

On the control page (Chrome or Edge, Connect), the section **Beat follow** shows the `beat` answer every second. Without
the page, send `beat` on the control line (SysEx on the MIDI port, [jank_sysex.h](../core/jank_sysex.h)):

| Key | Meaning |
|---|---|
| `audio` | 1 while the computer streams to jank |
| `mode`, `bpm` | the tracker's mode (stopped, steady, speeding_up, slowing_down) and tempo |
| `heap_free`, `heap_min`, `heap_largest` | internal heap now, lowest since boot, largest block; Wi-Fi, Link, USB and the tracker running |
| `stack_free` | bytes the tracker task's stack never used |
| `cpu_pct` | the tracker task's share of core 0 over the last second, preemption by Wi-Fi included (an upper bound) |
| `hop_avg_us`, `hop_max_us`, `hop_avg_cycles` | wall time and CPU cycles of the work for one 10 ms frame |
| `packets`, `gaps`, `overflows`, `resets` | audio packets taken; 1 ms frames with no packet while streaming; ring overflows (the tracker restarts); restarts |
| `tempo_sets`, `phase_forces`, `starts`, `stops` | what the tracker did to the Link session |
| `err_us` | the session's beat grid against the tracked beat before the last correction; stays within 5 ms when they agree |
| `offset_us`, `latency_us` | the sample-count-to-local-time offset (µs since the timer started) and the latency setting |
| `bar`, `bar_phi`, `bar_lead`, `bar_moves` | AUTO BAR (also in `status`): off / listening / locked / manual, the 1's count position (-1 none), the best phase's lead (the first move at 2.00), the session's bar moves |
| `conf` | LOW CONFIDENCE's confidence (also in `status`): since M2i against the grid lines the outputs published, not the grid extended back |
| `follow`, `drift_ppm`, `rate_ratio` | the follower (also in `status`): stopped / lock (ModeBpm's grid) / glide (the Kalman follower); the glide's period change a beat in ppm; the last rate ratio it took from the spectrum (1.0000: none yet) |
| `manoeuvres` | tempo moves the glide took from the beats |
| `rate`, `rate_active` | the spectral rate: the setting (`set rate on\|off`; off also without PSRAM) and whether it runs now (only while gliding) |
| `rate_events`, `rate_taken`, `rate_refused`, `rate_ignored` | rate steps proposed; taken, refused by the next beat, not judged (the glide not trusted yet) |
| `rate_us`, `rate_max_us` | the rate's work over the last second in µs (1000 = 0.1 % of core 0) and its longest frame |
| `weak` | the weak-signal hold (M2j): on with PSRAM for its 7.3 KB |
| `weak_holds` | steps the weak confidence kept the outputs running while the strict one was under the threshold (beat-less or quiet passages) |
| `coasts`, `weak_beats` | lines with no tracked beat the grid was corrected on from the activation; weak tracked beats taken gently |
| `fast_jumps` | new grids the follower jumped to at once (16 beats on a grid more than 6 % away, not an octave) |

## How it works

- **USB.** A UAC2 function joins NCM, MIDI and the drive: interface association, clock source 4 (fixed 44.1 kHz), input
  terminal 1 (USB streaming), output terminal 3 (speaker), no feature unit (no volume), one streaming interface with an
  isochronous OUT endpoint 5 (mono, 92 bytes, adaptive: the IN endpoints are all used, so no feedback endpoint). FIFO
  words of the S3's 256: DMA info 14, RX 76 (92-byte packets), TX 80 (EP0, NCM x2, MIDI, drive 16 each): 170 used, 86 spare.
- **Audio path.** Each 1 ms packet goes as it is (mono int16) into a 4096-sample ring in the USB interrupt
  (`usb_audio.c`); the tracker task (priority 3, below Wi-Fi 23, Link/bridge 20, TinyUSB 5) takes it 441 samples at a time.
- **Network weights.** Built once at the start into RAM as int16, one float scale per gate unit, with a tanh table
  (`core/beat/jb_lstm_q16.c`, 61.7 KB): read from flash every frame they cost more than a core. Host test: the same 61
  beats on the same frames as the float network on 30 s of a 124 BPM loop, activation error < 1e-3.
- **Time.** A sample's local time is `offset + s / 44100 + latency`, the offset being the smoothed difference between a
  packet's arrival (the Link host clock, `midi_now_us`) and its sample count, so the computer's audio clock sets the
  stream and jitter in the packets averages out.
- **Link master.** Tempo is set when it differs from the session's by more than 0.02 BPM; the beat grid is forced
  (`lk_force_beat`, new in `third_party/link`) only when more than 5 ms from the tracked beat; stopped: Link stop; running
  again: Link start at the next bar line at least 150 ms ahead.
- **AUTO BAR** (`set autobar on|off`, on by default). The downbeat estimator (`core/beat/jb_down.c`, a port of
  beat-listener's `live/downbeat.py`; weights from the user's Traktor grids and audio, `core/beat/down_model.c`) counts the
  grid's beats, scores the 4 phases from 22 accent features of the tracker's own 81 bands, and once the best phase leads
  by 2.0 the session's bar moves onto it in whole beats (`jbl_bar_step`, the tempo untouched; an armed start re-targets the
  moved grid's next bar line). A later move needs a lead of 3.0 held 8 beats. Reset (listening again, the bar left where
  it is): the tracker stopped, a new audio stream (a tempo jump is not one: `DOWN_JUMP_RESET` in beat.c). A beat nudge (`nudge beat`, the PHASE) no longer makes it
  MANUAL (M2h): AUTO BAR keeps placing the 1 and the PHASE rides on it (docs/firmware.md). The LED's bar flash turns amber while locked. RAM: 2.2 KB plus a 2.9 KB segment ring in PSRAM.
- **ADVANCED FOLLOW** (M2i; beat-listener `studies/advanced-follow`, golden vectors in `tests/follow_ref`). Three parts,
  each a port checked against the Python:
  - the **glide** (`core/beat/jb_follow.c`, `eval/follow_bpm.py`): ModeBpm locks; then a 3-state Kalman filter (phase,
    period, drift) follows each beat. Three beats in a row past 3σ open it, so a pitch move is taken in a few beats with
    the phase kept, with no stop and no relock. 12 misses on a new consistent grid, or the beats' own tempo 4 % away for
    8 beats (a 4:3 lock), start over.
  - the **line confidence** (`core/jank_conf.c`, `live/follow.py`): LOW CONFIDENCE judges the beats against the lines the
    outputs published (up to 64 kept). Extending today's grid backwards made any tempo change look like a bad grid.
  - the **spectral rate** (`core/beat/jb_rate.c`, `live/rate.py`; `set rate on|off`, on): while gliding, the
    log-frequency |FFT| of the tracker's own frames (every 2nd frame) is matched every 0.2 s: the last 0.5 s against the
    4 s before, ±10 %. A step goes to the follower, which takes it only if the next beat backs it.
  - Measured offline on the user's tracks (beat-listener): a ±2 % step stopped the outputs 96 % of the time before, 24 %
    with these (the no-step level). Convergence went from 13 to 3 beats.
  - Cost: the glide is a few hundred double operations a beat. The rate is ~0.7 % of core 0 (the S3 code's instruction
    count, CPI 1.45; PSRAM misses on top: read `rate_us`) and runs only while gliding. RAM: under 1 KB internal; the
    rate's 58 KB in PSRAM.
- **THE TUNED FOLLOW** (M2j; beat-listener `studies/tempo-changes`, the proposal tuned-l16; golden vectors in
  `tests/tempo_ref`). Hard to lock, sticky once locked, quick on clear evidence, held through beat-less passages:
  - **LOCK_BEATS 16** (was 8): 16 consistent beats before the outputs run, and for a jump.
  - **the rate's median of three** (`JR_CONFIRM` 3, was 2): fewer flips; a pitch step is taken ~0.5 beat later.
  - **the fast jump** (`jb_follow_fast_jump`): 16 beats on one grid more than 6 % from the glide's tempo, not an octave:
    jump now (a section change into a quiet part no longer waits for misses).
  - **the lines cleared on any jump**: the new grid is not judged on the old grid's lines (the gate opens ~7 s sooner).
  - **the weak-signal hold** (`core/beat/jb_weak.c`, `weak.py`): the network's activation is kept 2.56 s. A tracked
    beat is strong when the activation around it reaches max(0.05, min(0.3, 0.4 × the 75th percentile of the last 64
    beats')); weak beats are gentle corrections (12 ms, within ±0.12 period, no tempo move) or misses when off the grid;
    a beat-less line takes a salient activation peak (≥ 0.03 and 2.5 × the median of ±1 period) as one. The strict line
    confidence opens the gate; once open, the weak one holds it: lines with no evidence do not count against the grid,
    only strong beats off the lines do (and with fewer than 4 observed lines in 8 s the last value holds).
  - The device sees the activation only up to now: a beat goes to the follower 3 frames (30 ms) late, a beat-less line
    once the frames a period after it are in. `weak.py causal` replays exactly that; the study's numbers were measured
    with its look-ahead (not re-measured on the set: the cache is gone; equal on the synthetic cases).
  - Measured offline (test half of the user's 829 tracks): BPM held like before (0.858), stopped 0.129 → 0.105, wrong
    locks 0.31 → 0.20 a track, no range break; beat-less passages stopped 0.60 → 0.39; pitch moves in ~3 beats.
    Known regression: a tracker that slides to the off-beat for long (Sympathy is a knife: stopped 0.13 → 0.36).
  - `beat`: `weak`, `weak_holds`, `coasts`, `weak_beats`, `fast_jumps`. Cost (estimate): +0.16-0.24 % of core 0 (host
    time scaled by ModeBpm's QEMU-measured soft-double cost; ~0.05 % if it ran like float code). RAM: ~0.3 KB internal,
    7.3 KB PSRAM; without PSRAM, M2i's follow.
