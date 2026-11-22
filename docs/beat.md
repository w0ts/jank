# Beat follow

**The network's weights are not in the firmware image or in this repository.** The firmware reads them from its own
flash partition, `beatw` (128 KB at 0x620000; 0x340000 on the mini 2.2), as a blob (`core/beat/beat_weights.h`: "JBW1",
count, CRC-32, then 29,251 float32). With no valid blob the tracker does not start and everything else runs; the `beat`
answer says `beat_weights=none|ok|damaged`.

The weights used today are madmom's beat model (`beats/2016/beats_lstm_1.pkl` of `CPJKU/madmom_models`, CC BY-NC-SA 4.0,
non-commercial). Jank's firmware is GPL-3.0 and carries Ableton Link code under the GPL, so the two cannot be shipped in
one image. Instead the Install page fetches madmom's model from madmom's own repository at the user's click, converts it
in the browser (`ui/beatw.js`) and writes it to the user's device with the firmware, after the user confirms
non-commercial use. The site never holds the model or an image with it. Our own network will replace madmom's.

On the bench: `node scripts/beat_weights.js <beats_lstm_1.pkl> beat_weights.bin` makes the blob (the same converter;
`beat/scripts/export_beat_c.py model` makes it with Python, byte for byte the same), then write it at the partition:
`python -m esptool --chip esp32s3 write_flash 0x620000 beat_weights.bin`. The host tests make it from the madmom copy in
`beat/.venv` (`tests/run.sh`, `MADMOM_BEATS_PKL`).

The stick is a USB speaker called **Jank**. What the computer plays to it goes to a beat tracker on core 0 (madmom's
online tracker in C: `core/beat/`), and the tracker is the Link session's master: its tempo, beat grid and stop/start go
into the session (`core/jank_beatlink.c`). Core 1 stays the MIDI clock's alone.

## Build and install

```
JANK_SCRATCH=$PWD/../usbaudio-scratch JANK_JOBS=3 scripts/build.sh xiao
```

The app image is `$JANK_SCRATCH/build-xiao/jank.bin`. The build makes `third_party/esp_tinyusb` from the registry's 3.0.0
with `third_party/esp_tinyusb-uac2.patch` (the audio class on) and sets the CPU to 160 MHz (`firmware/sdkconfig.beat`;
it was 240 MHz until 2026-11-18, before the front end got cheaper). It installs over a stick running fw/ota with `python scripts/ota.py <jank.bin>` (app only: the
bootloader and the partition table stay). Settings in NVS are kept; `beat` is not stored and starts on; `autobar` (AT
START by default) and, since M2d, `beat_latency` per audio source (the computer's 25 by default) are stored.

**Rollback.** The new image confirms itself only after the computer has configured USB *and* spoken on the MIDI control
line (ota.py's `get` after the restart does that, as does a control page), and it has run 20 s. Not confirmed within
120 s, it restarts and the bootloader starts the old image. A crash before that, or a start with no computer, also puts
the old image back (`main/ota.c`, `main/usb.c`).

## Test with a Mac

1. Plug the stick in. Audio MIDI Setup lists an output **Jank**: 1 ch, 16-bit, 44100 Hz (the only format).
2. Play to it: beat-listener's player with `--xiao` (a second output stream with the mid of the track, besides the
   speakers), or set the Mac's output to Jank (silent, tracker only), or make a **Multi-Output Device** of the speakers
   and Jank, 44.1 kHz, the speakers as the clock source and drift correction on for Jank.
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
| `audio` | 1 while the computer streams to Jank |
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
| `conf` | the confidence SHOWN (also in `status`): with `graded=on` (default) the graded value, see "The graded confidence"; with `graded=off` LOW CONFIDENCE's gate value, since M2i against the grid lines the outputs published, not the grid extended back. `status` `conf_gate` is the gate's own value either way |
| `conf_hold` | 1 while the confidence holds (no evidence: a break, a track change; M2k): `conf` is then the shown value easing toward the threshold, the gate keeps the held one |
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
  **M2k: every start on the first beat.** The user saw the first beat after a Start read 3, off the music's 1. Two
  causes, reproduced in the host model (tests/test_solo.c, a Start pressed at 100 places in the bar, the estimator's 1
  found after START WAIT ran out, 2 beats off): with LINK TRANSPORT IGNORE / FOLLOW the Start let the MIDI clock start on
  the session's old bar line before the beat task's step placed the 1 (up to 250 ms later): 11 % of presses started, then
  moved 2 beats under the playing clock; with FOLLOW+LEAD the session held since power-on was forced onto the beat at the
  Start and the bar step refused on the error before that force, so the 1 was never placed: 13 % on the music's 3. Now:
  a Start (or a fold) asks the beat task for its step at once and the clocks wait for it (`jho_in.bar_settling`); the
  step that forces the phase places the bar too (`jbl.on_beat`); AT START never moves the bar under a playing MIDI clock
  (`jbl_in.clock_playing`, a fold mid-play included); and the clock never starts on a tick scheduled from before a move
  (`jc_step`). All 0 of 200 presses and 784 decision / fold timings now. START WAIT 8 → 12 tracked beats: the
  estimator's first lock comes a median 4.0 s after the tempo; with 8, 20 % of the starts went out before it on an
  arbitrary beat of the music, with 12 4 %, and the start comes no later (beat-listener
  studies/downbeat/results/v2_start_check.md). The estimator's own 1-vs-3 confusion at its first lock: 9-10 % of the
  starts it places (70 % right; 25 % if unplaced). **The CF bar's HOLD**: while the weak confidence holds its last value
  (no evidence), `conf_hold=1` (status, beat; the HOME line's conf as `<pct>h`) and the shown confidence eases toward the
  threshold (most of the way in 8 s) instead of a frozen 100 %, then snaps to the measured value; the gate is unchanged.
- **The graded confidence** (`core/beat/jb_graded.[ch]`, beat-listener `studies/follow-robust/graded.py`; display only, `set graded on|off`,
  default on, both followers). The gate's value is a wall and a cliff (about 100 % while locked, 0 % when not), so the number
  and the bar shown are the mean of four parts, each 0..1, clipped: **soft F** (the tracker's beats against the published lines
  over 8 s, a triangular kernel: 1 on the line, 0 half a period off), **jitter** (1 - clip(cv of the last 16 beat intervals /
  0.15)), the **tempo** agreement (the comb of the activation's last 8 s at the follower's period against the best period) and
  the **DBN's mass** near the follower's tempo. All four exist on the tuned follower (the default): the beats are the weak
  follower's list, the lines the confidence's, the DBN runs every frame, and the activation ring is the robust follower's
  8 KB (PSRAM, allocated at boot; `jrb_act` now runs with every frame, one store, on both followers). Only the comb is new work:
  `jg_tuned_step` (`core/beat/jb_graded.c`) refreshes all four once a second (no grid: 0) and keeps the mean in between;
  estimated 0.3 .. 0.5 M cycles/s on the S3 from the operation counts (about 200 autocorrelation lags of 800 products per
  refresh, soft F's 8 s window), no heap, 16 bytes of DRAM; NOT measured on the Jank. The robust follower keeps its own 0.5 s
  refresh of the tempo and DBN parts and the per-step soft F. The gate, the lock and jump decisions, the tempo and the lines are
  bit for bit the same with it (tests/test_graded.c: the tuned chain on tests/beat_ref and tests/robust_ref with and without it;
  and the same trace from the previous core). The first 5.5 s after a lock the tempo part is 0 (the comb needs that much
  activation), so the value ramps up. The graded value is a measurement: no HOLD easing, `conf_hold=0` while it is shown.
  **Fixed 2026-11-19** (the user: "before the confidence was 0 or 100, now 0 or 72 or 73"): the comb was asked to end at the
  stream clock, ~2 frames past the tracker's newest frame (a frame needs 1024 samples after its centre), and `jc_curve`
  refuses that: the tempo part was always 0 on the Jank and the value shown never passed 75 %. `jg_tuned_step` now ends the
  comb at the newest frame. Replayed from the audio of 39 of the user's tracks (the firmware's code, threshold 55): the
  tempo part while running 0.00 -> 1.00 (median), the value shown 71 -> 96 (Rhyw - Driptych 70 -> 94, Gary Beck - Mango
  Circus 71 -> 96). tests/test_graded.c checks it at the device's clock. Open: the robust follower's refresh and its
  ratio check (`jb_robust.c`) ask the same way, so its tempo part and the ratio check never ran either; not changed, as
  that changes when the robust follower locks (to be measured first).
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
- **RESUME: back on the beat after a pause** (2026-11-19, the user: "a pause can resume the music in a completely
  different phase, so we need to find the new phase ... one second, 10 seconds, 10 minutes or even 2 years"; a drop with
  sound keeps the grid, LOST above). `core/beat/jb_resume.c`, in `follow_conf`, the tuned follower with STOP / START.
  While the outputs run on a trusted glide the period and the SHAPE of the song's beat are remembered (the activation
  folded at the period from the published lines, 20 points: which peak is the beat and which the off-beat), no time
  limit. Silence or LOST's near silence (the PAUSE LEVEL below) arms it: the old phase is thrown away, neither the gate
  nor LOST's own return may bring the old grid back until RESUME matches, the follower locks or jumps on a grid of its
  own, or the sound has been back 4 s without a match (then the song comes back as before). Each step the last 4
  periods of activation (all 4 with a peak of 0.2) are folded at the remembered period and cross-correlated with the shape
  over 20 shifts; Pearson r >= 0.6: the follower is seeded on the new phase with the remembered period (a clean ModeBpm fed
  the grid as beats) and the outputs run at once; the DBN's tempo prior (jb_dbn.h, the robust follower's) is held on the
  remembered tempo while armed and 8 s after (its own tempo drifts in a pause: 140 -> 93 BPM after 10 min of -40 dB
  noise, and its beats dragged the grid off).
  - Measured (the firmware's code from the audio of 30 of the user's tracks, a pause of white noise at -40 dBFS, the song
    back where it stopped, threshold 55; today / RESUME, time to running after the music returns / the next 10 s on the
    grid; with the PAUSE LEVEL at -40 dBFS): 5 s 2.0 s, 44 % / 1.5 s, 100 %; 30 s 2.4 s, 65 % / 1.4 s, 100 %; 2 min 8.3 s
    / 1.4 s; 10 min 17.3 s / 1.5 s (100 % on the grid both). 200 whole tracks, normal play: the same 87 stops, 653 ->
    613 s stopped, 19 resumes after quiet passages; the worst track 2.8 -> 5.1 s stopped (before the 4 s limit: 102 s, a
    quiet part RESUME did not recognise). Rejected: waiting for the tracker's beats (after a pause its
    DBN comes back at a wrong tempo); a band-profile fingerprint (57-69 % of other songs looked the same); a comb of the
    activation alone (it took the off-beat on house tracks); the kick band to choose beat or off-beat (worse).
  - `tests/test_tempo.c`: after 5 s, 2 min and 10 min of near silence the song back 0.17 s off the old grid: running on
    the new phase 1.53 s after its first beat (4 beats in); the shape found on the beat when the off-beat peaks higher; one beat back is
    not enough. RAM: ~110 bytes + the 40-float prior. Cost: per step 80 activation reads, when armed 20 correlations of 20.
- **PAUSE LEVEL** (`set pause_db -70..-20`, stored, -40 dBFS by default; the colour layout's AUDIO > PAUSE LEVEL; the
  INPUT bar's notch on the colour screen and the page): the input under it for 2 grid lines is a pause (LOST's QUIET:
  the outputs stop, RESUME finds the new phase after it). The user, 2026-11-19: "silence does not have to be -100 dB ...
  it could be like -40 dB"; "40 sounds about right"; "make it a setting, also visible on the input bar". It replaced
  LOST's earlier rule (under -55 dBFS or 30 dB under the song): what is shown is what counts. Measured (200 of the user's
  tracks, LOST off / on): the -40 dBFS rule stops 4 more times in quiet song parts than the old one (87 against 83 stops,
  653 against 560 s stopped); with RESUME those come back on the beat. The mono OLED layout (the frozen mini 2.2) keeps
  its silence notch and has no PAUSE LEVEL row (its references, tests/oled_ref/cases.txt, are not made again here).
- **LOST: when the outputs stop once they run** (`core/beat/jb_lost.c`, `jb_kick.c`, `jb_song.c`, in `follow_conf`, the
  tuned follower with STOP / START; silence, LINK mode, KEEP RUNNING, a threshold of 0 and the robust follower: off).
  History: the first LOST (2026-11-18, "it should stop sooner when it does not know") stopped after a bar of grid lines
  with no strong beat. On the user's tracks it stopped with the kick plainly there (Rhyw - Driptych: the online network's
  activation 0.12-0.35 on a full four-on-the-floor kick, 6 stops, 102 s of the music; Gary Beck - Mango Circus: the
  tracked beats slid a third of a beat off a right grid). The user's rule since (2026-11-19): "if this looks like the
  same song but there is no beat, try to stick to it" (drops of 16-32 beats); stop on (near) silence for more than 2
  beats; beats at another tempo: match them or stop; a new song with a beat-less intro: an option.
  - Each line of the published grid, a period after it: FIT when a strong beat (jb_weak's test) or a KICK (the input
    under 120 Hz: a transient within 10 % of a period of the line, 15 dB over the band between the lines, within 20 dB
    of its loudest of the last 8 s) is on it; OFF when no fit and a strong beat lies 10-50 % of a period from it;
    QUIET when the input over its period is under the PAUSE LEVEL (above, -40 dBFS by default). Neither:
    a beat-less line of the song, it holds. LOST: 2 quiet lines in a row; 8 off lines since the last fit (the
    follower's jumps and the spectral rate did not take those beats); with `set newsong on`, 4 lines with no fit and
    jb_song's NEW (the harmony of the last 6 s, 400 Hz-4 kHz folded to 12 pitch classes, against the 90 s before:
    1 - r >= 0.8). Back: 3 fits in a row with sound, at once, on the grid. A Start pressed while LOST keeps the outputs
    running until then (`conf_open` 0 while LOST).
  - **Once the outputs run, the confidence no longer stops them** (beat.c, STOP / START): only LOST, silence (-60 dBFS
    for 0.5 s) or the follower giving up (30 s with no fit). The threshold decides the start. (In a drop the DBN's
    wandering beats pull the weak confidence down, and a fast jump clears its lines: it read 0 and closed the gate.)
  - Measured offline (the firmware's C tracker and these modules, beat.c's chain, from the audio of the user's 795
    Traktor tracks, threshold 55; 49 h of clear beat; ground truth: a Traktor grid beat has the beat when the kick band
    shows a transient on it or the network's activation peaks there; 80 synthetic cuts from one track into another).
    LOST off / the first LOST / now: drops of the same song held through 54 / 25 / 69 %; stops on a clear beat 13.2 /
    10.6 / 2.4 an hour; clear-beat time stopped 2.0 / 4.5 / 0.3 %; all stops 35 / 40 / 13 an hour; back after a stop
    0.8 / 4.2 / 0.2 s median; running off Traktor's grid while the beat plays 5.0 / 3.8 / 5.9 %; after a cut, the
    lines on the new track's grid 91 / 85 / 86 %; a cut to a beat-less intro stopped before its beat 4 / 12 / 3 of 12
    (with `newsong on` 4 of 12; the new-song check alone, offline: half the beat-less intros within 8 s, but ~1 drop
    in 7 of the same song trips it). Driptych: no stop in the music; Mango Circus: one, 3:48-3:52, where the follower's
    grid sits 0.17 s off the kick. Rejected: the network's activation on the line as evidence (wrong stops 9.7 -> 8.1
    an hour only); the spectral shape for the new song (cuts 39 %, drops 34 %); evidence reopening a gate closed by
    the confidence (it flapped every 0.5 s).
  - `tests/test_tempo.c` pins it on synthetic audio (a 60 Hz kick on the beats over a pad, 120 BPM): a 20 s drop holds;
    near silence stops 1.53 s after it begins (2 lines, judged a period + 50 ms after the 2nd), back 1.78 s after the
    beat returns (3 lines); the kick with a deaf network holds; the kick band, the off count and the new-song check on
    their own. Cost: per sample a few float multiply-adds; per hop two log10; per line judged ~50 frames read; the
    new-song check (when on) ~40 adds a frame. RAM: ~7.3 KB PSRAM (jb_kick 2.2 KB, jb_song 5 KB), 24 bytes internal.
