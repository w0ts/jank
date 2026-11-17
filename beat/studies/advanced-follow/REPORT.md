# Advanced beat follow, stage 1 (2026-11-15)

An offline Python study on the user's Traktor tracks (test = odd positions, dev = even). No firmware.
- Numbers are measured unless marked "estimate".
- Tables are in `results/`: `steps_final.md`, `material_test.md`, `sweep_dev.md`, `detector_dev.md`; the raw test rows are in `eval_test.jsonl.gz`.
- Curves: `curves.png`.

## 1. Before → after: pitch steps (varispeed)

What was replayed:
- **today:** today's live chain (one-net tracker, DBN 90-180, ModeBpm, the LOW CONFIDENCE gate at 0.8);
- **quick:** the Kalman glide plus the line confidence;
- **+ratio:** quick plus the spectral rate, taken only if the next beat agrees.

Each step was placed on a random track at a random place and judged over 30 s.

| step | stopped (today / quick / +ratio) | grid > 50 ms off | converge, median beats | worst ms, first 10 beats |
|---|---|---|---|---|
| none (control) | 0.26 / 0.24 / 0.24 | 0.12 / 0.14 / 0.15 | 0 | 0.6 / 3.2 / 3.4 |
| 1 % | 0.42 / 0.19 / 0.20 | 0.59 / 0.13 / 0.15 | 15 / 10 / 2 | 37 / 14 / 10 |
| 2 % (300 cases) | 0.96 / 0.23 / 0.24 | 0.98 / 0.16 / 0.17 | 13 / 8 / 3 | 70 / 26 / 16 |
| 3 % | 0.98 / 0.22 / 0.22 | 1.00 / 0.17 / 0.17 | 21 / 7 / 2 | 86 / 36 / 21 |
| 5 % | 1.00 / 0.29 / 0.21 | 1.00 / 0.64 / 0.36 | 21 / 7 / 4 | 118 / 54 / 32 |
| 8 % | 1.00 / 0.52 / 0.31 | 1.00 / 0.98 / 0.62 | 21 / 8 / 7 | 166 / 85 / 54 |
| ramp 5 % over 8 s | 1.00 / 0.20 / 0.21 | 1.00 / 0.20 / 0.21 | 29.5 / 22 / 22 | 52 / 26 / 16 |
| ramp 8 % over 30 s | 1.00 / 0.23 / 0.23 | 0.95 / 0.20 / 0.19 | 38 / 37 / 23 | 22 / 11 / 9 |

On the real tracks (the test half, 414 tracks), quick has a small steady-music cost:
- BPM held 0.823 → 0.808;
- flips 1.37 → 1.43 per track;
- dev: 0.822 → 0.804.

## 2. Why today loses a few-percent step

- The tracker's beats stay fine.
- ModeBpm's growing line holds for about 12 beats: its change detector needs the 16-beat fit more than 0.5 % off for 8 beats. Meanwhile the grid drifts 8-9 ms a beat.
- The confidence drops under 0.8 at about beat 9, and the gate stops the outputs.
- The confidence then judges the last 8 s against the new grid extended backwards at constant tempo, so the gate stays shut even once the grid is right.

Each part alone does not help: the Kalman alone still stops 99 %, the line confidence alone 100 %. Both together bring it to 24 %, the level of the no-step control.

"Not always": the median stop is 1.6 s and 66 % are under 2 s. The long stops are on jittery and faster tracks.

## 3. What was built

- **`eval/follow_bpm.py` FollowBpm:** ModeBpm does the first lock, the hold, the stop and the jumps. A 3-state Kalman (phase, period, drift) then glides the steady grid; it opens on 3 innovations in a row beyond 3σ. A guard catches a 4:3 lock. `propose_ratio()` takes the spectral rate.
- **`live/follow.py`:** LineLog and `line_confidence` (the confidence against the lines actually published), behind `python -m live --advanced-follow`.
- **`live/samemusic.py`:** novelty against a 64 s memory. Not recommended (section 5).
- **`ratio_probe.py`:** the log-frequency spectrum at 96 bins per octave, 300 Hz to 8 kHz. The last 0.5 s is matched against the 4 s before it, within ±10 %.

## 4. Rate from the content

- **Speed and accuracy:** detected in 0.50 s median, about 1.15 beats. The ratio error is 0.13-0.23 %, and 81-85 % of steps are found within 3 s.
- **False events:** about 200 per hour raw (chord changes), made harmless by the next-beat check.
- **Blind spots:** no signal on time-stretch, little on slow ramps.
- **Result:** convergence in 2-3 beats at 1-4 %, and 4-7 beats at 5-8 %.

## 5. The same-music detector: does not work on this set

- On this set (mostly techno at 125-140 BPM), timbre and chroma novelty do not separate a new track from the same track varied.
- At no more than 1 false reset per hour, it catches 0-12 % of transitions.
- It is dropped. The content match is kept as a rate measurement only.

## 6. Device cost (estimates from operation counts)

| part | CPU | RAM |
|---|---|---|
| Kalman | under 0.01 % | about 100 B |
| line confidence | about today's | about today's |
| rate match | about 0.3 % | 3.6 KB |

These sit against the XIAO's 43-45 % load and its ~45 % ceiling.

## 7. Stage-2 plan (not started)

1. Port the Kalman glide and the line confidence to `jb_mode` / `jank_conf`, with golden vectors from FollowBpm.
2. Add the rate module, fed by `jb_fe`'s existing magnitudes and accepted at the next beat.
3. Drop the novelty detector.
4. Give `Engine` a confidence hook instead of the prototype's patch.

Risks:
- the CPU ceiling;
- the published BPM wobbles by hundredths, which can cause Link tempo updates;
- the steady-music cost in section 1;
- the steps and transitions are synthetic.

## 8. Not verified

- Nothing ran on the device.
- The half-beat Traktor verdicts (`eval/ref_phase.py`) were not used; tracks with an offbeat share above 0.5 were left out instead.
- "Drums out" and "vocal only" are HPSS separations, not real stems.

## 9. Literature

Checked:
- Davies & Plumbley, IEEE TASLP 15(3), 2007;
- Cemgil, Kappen, Desain & Honing, ICMC 2000 / JNMR 29(4);
- Foote, ICME 2000;
- Adams & MacKay, arXiv:0710.3742.

From memory, not re-checked: Ellis 2007; Stark et al. 2009 (BTrack); Krebs et al. 2015; Kim et al. 2020.
