# Downbeat, second study ("which beat is the 1"), 2026-11-15 — stage 1

Saved by the main session from the study agent's report. Goal: line Link's bar (quantum 4) up with the music, better
than the first study (REPORT.md). Stage 1 = the estimator offline + the integration design. Nothing is built into the
player or the firmware yet.

Protocol as the first study: the user's Traktor set (829 tracks) split by position, even = dev (every choice, every
weight), odd = test (414 tracks, scored once); the tracker's online beats (live.tracker.OnlineTracker, 90-180 BPM);
GTZAN as a second check. Same columns, computed by the study's own functions (rules.score, analyze.summarize). All
numbers come from runs in this study (results/v2_*.md); estimates are marked as such.

## 1. Numbers (Traktor, TEST half)

| method | right, mean (median) | tracks >= 90 % | median time to lock | never locked | flips/min |
|---|---|---|---|---|---|
| today: Link's arbitrary bar | 25 % (25) | 0 % | - | - | 0 |
| first study: linear rule, online beats (its protocol: features 4 beats ahead) | 61.0 % (69.6) | 24 % | 21 s | 14 % | 0.82 |
| first study, the same rule made causal (features used once complete) | 60.4 % (69.0) | 22 % | 22 s | 14 % | 0.86 |
| first study's weights, counted on Link's beats | 64.2 % (75.2) | 31 % | 6 s | 14 % | 0.79 |
| **NEW: counted on Link's beats, trained on clean reference beats, causal** | **66.2 % (77.3)** | **34 %** | **6 s** | **13 %** | 0.84 |
| NEW, the 379 test tracks where the tracker's tempo = Traktor's | 69.2 % (81.2) | 36 % | 6 s | 8 % | 0.83 |
| NEW, the 168 test tracks where Traktor and madmom agree (the "1" is probably right) | 78.2 % (88.8) | 47 % | 7 s | 4 % | 0.78 |
| first study, the same 168 tracks | 72.9 % (82.5) | 38 % | 16 s | 6 % | 0.77 |
| NEW on Traktor's own beats (perfect beats) | 77.3 % (96.2) | 57 % | 4 s | 7 % | 0.50 |
| first study on Traktor's own beats | 71.5 % (87.7) | 48 % | 4 s | 6 % | 0.57 |
| NEW, Link beats from OUR beat net (models/beat_ours.pt) instead of madmom's * | 69.3 % (82.0) | 38 % | 11 s | 11 % | 0.86 |

\* optimistic: beat_ours.pt was trained on 518 of these Traktor tracks (its own folder split), so part of this test half
is in its training set. It shows the rule does not depend on madmom's net; the number itself is not a test score.

"Right" = share of scored beats whose bar position is right (beats within 70 ms of a reference beat, after the first
2 bars). Lock = seconds from the first beat to the first run of 8 right beats. The NEW rows are the estimator's "time in
correct phase"; what Link would do is in section 2.

The reference is still the weak point. madmom offline (now measured on 803 tracks) agrees with Traktor's "1" on >= 90 %
of beats for only 320 tracks; mean agreement 55 % on the 399 4/4 test tracks. Scored against madmom's "1" instead of
Traktor's, the new rule reads 51.8 % (first study 49.5 %): both references are noisy, and the rule was fitted to
Traktor's. On the tracks where the two agree, the new rule is right 78 % of the time (median 89 %).

Splits (test, Link beats, pooled): house 77 %, techno 72 %, other 72 %, electro/breaks 64 %, pop/hyperpop 61 %;
in a break 72 %, outside 71 %.

GTZAN (human downbeats, 30 s clips, odd half): first study 42.8 % (online beats); new rule (Traktor-trained) 46.6 % on
Link beats; the same structure trained on GTZAN's even half 47.8 %. Jazz 13 %, hip-hop 64 %. 30 s is short: Link's
first beat comes after ModeBpm locks (median 5.8 s on Traktor), and 58 % of clips never lock.

## 2. Link's bar (the user-visible result, test half)

"Right as heard": until the first move Link's bar is arbitrary (25 % expected), then whatever the logic chose.

| policy (chosen on dev) | right as heard, mean (median) | tracks >= 90 % | first move after (median) | first move right | later moves/min | later moves wrong |
|---|---|---|---|---|---|---|
| today | 25 % (25) | 0 % | - | - | 0 | - |
| stable: first move when the best phase leads by 2.0; later moves need a lead of 3.0 held 8 beats | 61.0 % (73.3) | 38 % | 4 s | 60 % | 0.09 (one per 11 min) | 23 % |
| responsive: later moves need a lead of 1.0 held 8 beats | 65.7 % (77.6) | 34 % | 4 s | 60 % | 0.25 (one per 4 min) | 42 % |

- A wrong later move happens about once every 48 minutes with "stable", once every 10 minutes with "responsive".
- Waiting before the first move does not pay: with 16-64 beats of wait the first move is right 66-75 % of the time, but
  the bar stays arbitrary meanwhile; mean right drops 5-10 points (dev, results/v2_tables.md).
- A hybrid (responsive for the first 32-128 beats after the first move, then stable) was not better than either.
- results/v2_weights.json carries the "stable" policy.

Track change (simulated DJ cut at the same tempo: the end of one test track, then the next, Link's count running on,
207 cuts): with the memory carried over the bar re-locks after a median 38 s (within 60 s: 56 %); if the cut is
detected and the accumulators reset, 9 s (within 15 s: 49 %; not within 2 min: 17 %). A shorter memory (8 bars) gives
the same accuracy (66.0 %) and 32 s / 8 s.

## 3. What made the difference (and what did not)

Choices on the dev half, 2-fold cross-fitted (the weights that score a dev track never saw it); the first study's rule
refitted the same way (57.8 %; its in-sample dev number was 59.1 %) is the fair comparison.

1. **Count Link's beats, not the tracker's (+3 points; the largest gain).** Link's bar is 4 of Link's beats, and Link
   plays the estimator's fitted grid, not the raw tracked beats. Counted on the raw beats, the true bar phase slips
   3.0 times a minute (median 2 slips a track, 30 % of tracks without one): even an oracle that knows each track's best
   constant phase is right only 75.4 % of the time (median 76.6 %). Counted on Link's beats (v2/linkbeats.py: ModeBpm
   fed the tracked beats, Link playing its grid until the next one) the same oracle reaches 82.5 % (median 99.3 %),
   48 % of tracks without a slip (results/v2_slips.md). Link's beats sit 4 ms (median) from the tracked beats, so the
   features are the same; only the count is steadier. The first study's weights, unchanged, gain 3.2 points from this.
2. **Train on the reference beats (+2 to +7 points on dev).** Fitted on Traktor's own beats (shifted by one constant,
   the tracker's -33 ms lag) and used on Link's beats: 63.2 % (dev) against 60.6 % when fitted on the tracker's beats
   and 55.7 % when fitted on Link's beats (results/v2_linear_link*.md). The labels are cleaner on the reference beats.
   It also keeps madmom's output out of the weights (section 6).
3. **Self-cleaning the labels (+0.5 on dev, kept).** Dev tracks whose Traktor "1" the cross-fitted rule contradicts on
   their own reference beats (right < 25 %) are left out of training: 69 of 415. madmom offline measured 63 of them and
   agrees with Traktor on only 7 (mean agreement 23 %): mostly wrong labels. Training only on the tracks where Traktor
   and madmom agree gave the same (62.6 % on all dev, 76.1 % on the agreeing ones): no gain, and that selection uses
   madmom's output, so it is not used.
4. **Causal features (-0.4 points, accepted).** The study's nov_bar features look 4 beats ahead and its onset window
   100 ms; the study used them from the next beat on. Here a beat's features enter 5 beats later, as on the device.

What did not help (dev, cross-fitted; kept so nobody redoes them):
- **A per-beat classifier + bar-phase HMM** (v2/evaluate2.py: logistic, MLP, GRU over 29 new beat-synchronous
  descriptors: 8-band levels and onsets, level changes, chroma change, tracker timing): 47-50 % (results/v2_perbeat.md).
- **The accumulator rule on z-scored inputs with a shared MLP scorer** (v2/evaluate3.py, accmodel.py): 48-53 %
  (results/v2_acc_zscore.md). The study's |mean|-relative accumulators are better.
- **More features**: the 29 descriptors with the study's normalisation: alone 61.8 %, added to the study's 22: +0.2
  (noise; results/v2_linear_grid.md). Not shipped: 88 weights instead of 236.
- **Several time scales** (8/32/128, 16/64 bars): 0 to -4 points. One exponential memory of 16 bars stays best (8: same
  accuracy, faster re-lock; 24-32: worse).
- **An MLP scorer instead of the linear one**: worse (overfits on ~400 tracks).

## 4. How it drives Link and the device (design, not built)

### The beats it counts: Link's own
The estimator runs on Link's beats: every beat of the estimator's published grid (ModeBpm.grid / jb_mode_grid, the
beats Link plays), counted one per grid beat. A single off-grid tracked beat, a dropped beat or a half-beat jump of the
tracker does not shift the count. A beat's features are complete 4 beats + 100 ms later; they enter the accumulators at
beat j + 5.

### States
- LISTENING: since a (re)start; Link's bar is left where it is. Ready once every bar position has had complete features
  (about 9 beats, 4-5 s at 120 BPM).
- LOCKED(phi): Link's bar was moved so that the grid beats with count = phi (mod 4) are Link's 1.
- MANUAL: the user nudged. No automatic move.
- OFF: AUTO BAR switched off in the Link menu.

### Moves
- A move is whole beats, n in {-1, 0, +1, +2}, so that the next beat the estimator calls "1" lands on Link's bar line.
  Python: LinkController.shift_beats(n) for the whole-beat part only (beat_offset's half-beat part untouched). C: the
  same force_beat jbl already uses for the phase, by whole beats, under the bridge lock. Applied at a beat; the tempo
  is untouched.
- First move: when LISTENING is ready and the best phase leads the second by 2.0 (score units).
- Later moves: another phase must lead the current one by 3.0 for 8 beats in a row ("stable"). A move needs a lead that
  lasts, so the bar cannot swing back and forth (measured: one later move per 11 minutes).

### The user's NUDGE always wins
- A whole-beat NUDGE (two halves) moves Link's bar and sets MANUAL: no automatic move until the next reset event or
  until the user picks AUTO BAR again. Each nudge is logged with the estimator's scores: free hand labels for refitting.
- A half-beat NUDGE means the tracker is on the off-beats, so every feature is measured in the wrong place: MANUAL too,
  until undone or reset.

### Track changes
- Reset events: the estimator's own "stopped" (the silence gate, 30 s without a steady beat, resync / Start) and a tempo
  jump (ModeBpm restarts its fit). On reset: accumulators cleared, LISTENING, Link's bar left in place until the new bar
  is known, then one move.
- A cut at the same tempo (a DJ mix) raises none of these. Then the later-move rule finds the new bar: median 38 s
  (section 2); 9 s if the cut were detected. No cut detector is built. Stage-2 option: reset on a large spectral change
  at a bar line (nov_bar_all is already computed per beat), only if it measures well first.
- MANUAL survives glitches but not a reset event (a new track is not the user's old bar).

### Screen
- LISTENING: the four beat boxes outlined, the current beat running through them (as today).
- LOCKED: the box of the 1 filled or bright: the user sees where the bar is and can nudge if it is wrong.
- A small confidence tick under the boxes: the best phase's lead over the second, against the commit level.
- MANUAL: an "M" next to the boxes; AUTO BAR on/off in the Link menu.

## 5. ESP32-S3 cost and the C port (plan)

### Where it plugs in
- core/beat/jb_beat.c: struct jb_frame gains `const float *feat` (the 162 inputs: 81 log bands, 81 positive
  differences, computed already) and the 5 lowest bands before the log (`s` in jb_fe's frame_features). No new FFT.
- New core/beat/jb_down.c/.h, portable C, no allocation, no ESP-IDF:
  - `jb_down_frame(d, f)` every 10 ms frame: adds the frame to the current grid-beat segment (81 band sums) and to a
    16-frame ring of the per-frame curves (e150, e100, flux150, flux_all, 6 group fluxes) for the onset window;
  - `jb_down_grid_beat(d, frame_index)` when the beat task passes the next beat of jb_mode's grid (it already works the
    grid out for Link): closes the segment, computes the 22 features of the beat whose look-ahead just completed,
    updates its position's 22 accumulators, scores the 4 candidates;
  - `jb_down_state(d, &phase, &lead, scores)`, `jb_down_reset(d)`, the move logic and MANUAL;
  - jank_beatlink gets `jbl_shift_bar(c, k, n, t)` (whole-beat force).
- firmware/main/beat.c: on_frame -> jb_down_frame; grid beat crossing -> jb_down_grid_beat; a move -> jbl_shift_bar
  under the bridge lock; resets from jb_mode's events and restart_tracker. The UI gets the beat-box state.

### Data layout (float32: the S3 has a single-precision FPU, no need for fixed point)
- Segment ring, the last 9 grid-beat segments' 81-band means (4 before, 4 after a beat, the open one):
  9 x 81 x 4 B = 2.9 KB (PSRAM is fine: touched once a beat).
- Onset-window frame ring: 16 frames x 10 curves x 4 B = 640 B. Pending beats: 6 x 12 B.
- Accumulators 22 x 4 floats = 352 B; scores and state < 64 B.
- Weights: 88 floats (352 B), const in flash, generated from results/v2_weights.json (like beat_model.c).
- Total about 4 KB of RAM (estimate), 2.9 KB of it may sit in PSRAM.

### Cost (counted from the operations, NOT measured on the device)
- Per frame (100 a second): about 250 adds and 2 log10f: estimate 3,000 cycles, 0.3 M cycles a second, about 0.13 % of
  one core at 240 MHz.
- Per grid beat (about 2 a second): 81-band means and differences (~700 flops), 22 features, 22 accumulator updates, the
  |mean| normalisation (88), 4 x 88 multiply-adds: about 2,000 flops, under 0.01 %.
- Total: well under the 2 % budget (the tracker itself is 39-40 %). To be measured with the beat task's cycle counter.

### Checks for the port
- A golden test (tests/test_down.c): Python (v2/final.py's functions) writes per-beat features and scores for a few
  tracks; C must match to 1e-4.
- jb_fe centres its frames, the study frames them as live/tracker.py (23 ms later); both index the frame "at the beat"
  by their own frame numbers, so they should see the same audio. The golden test checks it.

## 6. What each shipped piece is derived from (licences)

| piece | derived from | madmom's models (CC BY-NC-SA 4.0) |
|---|---|---|
| the 88 weights (results/v2_weights.json) | the user's Traktor grids (beat times, grid marker) and the user's audio, through the first study's 22 features (our code on madmom's BSD-3 filterbank code); softmax regression | not an input: the features are taken at Traktor's reference beats |
| the -33 ms shift of the reference beats | the median lag of the online tracker's beats behind Traktor's | one constant measured with madmom's online tracker (an instrument, as the beat net's label offset) |
| the training-track selection (self-cleaning) | the rule itself, on the reference beats | not used |
| the margin, the Link thresholds, the feature set and time scale | dev scores counted on Link beats simulated from the online tracker's beats | the scores behind these choices were measured on madmom's online beats (instrument); no model output enters the weights |
| linkbeats.py, ModeBpm / jb_mode | our code | - |
| madmom offline downbeats (v2/mmdb.py) | | scoring and the "agree" rows only; the variant trained on agreeing tracks is reported, not shipped |

Same policy as models/LICENSE.md: madmom is a measuring instrument (choices, one constant), never the training input,
teacher or initial weights. The device's current beat_model.c is madmom's network (bench only, per beat.c's header);
the rule was also checked on our own network's beats (section 1, with its caveat).

## 7. Stage 2 (after the user's go): plan and size (estimates)

1. Python reference in the live player: beat/live/downbeat.py (features from an 81-band frame tap in the engine, Link
   grid beats, accumulators, the move logic), the controller's whole-beat auto shift and MANUAL, the player page and the
   device emulator showing the beat boxes; unit tests (Link-beat counting, moves, nudge wins, reset). Estimate 400-600
   lines with tests.
2. C: jb_down.c/.h (~300 lines), the weights export script (~80), the golden test (~200), beat.c / jank_beatlink / UI /
   menu wiring (~200). Estimate 800-900 lines.
3. Checks: host tests, the emulator, then the user installs by OTA and plays music through the XIAO; CPU and heap read
   from the beat reports.
4. Before shipping: re-measure with the network the device will ship (beat_ours), and refit the 88 weights on ~50 tracks
   labelled by ear or on logged nudges (Traktor's "1" is wrong on many tracks).

Stage 2 choices made by the main session for the user (away, 2026-11-15): the "stable" policy, no cut detector for now
(accept the ~38 s re-lock), Python first then C.

## Files and re-running

- v2/: extract2.py (cache: spectrogram + online beats), mmdb.py (madmom offline, instrument), build.py (descriptors,
  Link beats), linkbeats.py, feats.py, exp_linear.py (variants), final.py (the final pipeline), slips.py, ours_check.py,
  run_farm3.sh, run_eval.sh; the dead ends evaluate2.py + model.py + phase.py (per-beat + HMM) and evaluate3.py +
  accmodel.py (z-scored accumulators); evalcore.py (shared scoring, Link-view metrics).
- results/: v2_tables.md (final run), v2_weights.json (the rule, "stable" policy), v2_slips.md, v2_ours_net.md,
  v2_linear.md (dev, tracked beats), v2_linear_link.md (trained and scored on Link beats), v2_linear_link_online.md and
  v2_linear_link_ref.md (trained on tracked / reference beats, scored on Link beats), v2_linear_grid.md and
  v2_linear_delay_*.md (trained on reference beats; scored on Link beats, or on tracked beats for *_online),
  v2_perbeat.md, v2_acc_zscore.md.
- Re-run on gpufarm: run_farm3.sh (extraction ~10 min with 14 workers; madmom offline ~59 min with 8), then build.py and
  final.py (~2 min with 12 workers). The farm cache (5.1 GB) was removed after this study.
