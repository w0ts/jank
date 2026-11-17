# Downbeat ("which beat is the 1") study, 2026-11-14

Saved by the main session from the study agent's report. Tables behind it: results/tables.md, results/grid_*.md,
results/weights.json (made by the scripts in this folder). "Right" = share of scored beats whose bar position is right
(first 2 bars skipped; tracked beat within 70 ms of a reference beat). Chance = today's arbitrary Link bar = 25 %.

## 1. Numbers

Traktor, 414 test tracks (odd half; choices and weights made on the even half), the tracker's own online beats
(live.tracker.OnlineTracker, 90-180 BPM).

| method | right, mean (median) | tracks >= 90 % | median time to lock | never locked | flips/min |
|---|---|---|---|---|---|
| today: Link's arbitrary bar | 25 % (25) | 0 % | - | - | 0 |
| briefed kick rule (43-129 Hz peak) | 37 % | - | - | - | - |
| best single feature: low-band beat novelty, exp N=16, 20 % hysteresis | 53 % (57) | 16 % | 43 s | 17 % | 0.8 |
| learned linear rule, 22 cheap features, exp N=16, hysteresis 0.5 | 61 % (70) | 24 % | 21 s | 14 % | 0.8 |
| same rule on Traktor's reference beats (perfect beats) | 72 % (88) | 48 % | 4 s | 5 % | 0.6 |
| BeatNet online, its own beats, 827 tracks | 30 % (28) | 0 % | 62 s | 51 % | 16 |

Lock = seconds from the first tracked beat to the first run of 8 right beats.

The reference is the weak point: Traktor's "1" is its grid marker + every 4th beat (the marker is on the file's first
grid beat in 81 % of tracks). madmom's offline downbeat tracker agrees with Traktor's bar position on only 55 % of beats
over 203 tracks (offset 0 beats: 115 tracks, +1: 36, +2: 38, +3: 14). On the 88 tracks where both agree (weights refitted
on the other half, out of sample):

| method | right, mean (median) | tracks >= 90 % | lock | never locked |
|---|---|---|---|---|
| linear rule, online beats | 70 % (77) | 30 % | 15 s | 5 % |
| linear rule, reference beats | 81 % (94) | 60 % | 4 s | 5 % |
| low-band novelty, online / reference | 60 / 78 % | 18 / 65 % | 47 s / 4 s | 8 / 5 % |

Splits (test half, linear rule, online / reference beats): house 75/79, other 73/78, techno 63/70 (233 tracks),
electro/breaks 61/67, pop/hyperpop 55/77. In a break 66/70, outside 64/72 (the 16-bar memory carries through breaks).
GTZAN (human downbeats, 30 s clips, 499 test tracks): rule fitted on GTZAN 50 % online / 60 % reference; Traktor weights
43/50 %; by genre online: disco 68, hiphop 67, pop 65, reggae 62, metal 58, country 52, rock 47, blues 43, classical 38,
jazz 25. BeatNet 52 %, madmom offline (not causal) 65 %. Not 4/4: Traktor none (4/4 by construction; madmom: all 208
4/4); GTZAN 8.1 % of bars in 72 tracks (not scored).

What works: the kick peak can't tell the 1 (the kick hits every beat): 37-39 %. Spectral change at the beat (low-band
novelty: the band average over the beat after vs before) 51 %; all-band 46 %. 1-3 kHz flux 21 % (snares/claps mark 2
and 4: a negative weight). N = 16 bars beats 2-8; exponential forgetting +2-4 points; hysteresis cuts flips 5.2 -> 0.8
per minute at no loss. Online vs reference (61 vs 72 %): 18 % of tracked beats are not within 70 ms of a reference beat,
and a dropped or doubled beat shifts the count.

## 2. ESP32-S3 cost (counted from jank worktrees/beat-follow/core/beat/, not measured)

Inputs: jb_fe's 81 log bands and their 81 positive differences, plus the 5 lowest band sums before the log: reuses the
tracker's spectrogram, no extra FFT. About 250 adds per 10 ms frame (the tracker's frame: 2048-point FFT, 1428-tap
filterbank, 81 logs, 3 x 25 LSTM with 29,251 weights, DBN): well under 1 %. Per beat: two 81-band differences, 88
accumulator updates, 4 x 88 multiply-adds. Memory under 4 KB with the 88 weights. Check for the C port: jb_fe centres its
frame, the study framed as live/tracker.py does (a 23 ms shift): re-measure there.

## 3. Online downbeat models (V verified this session, R recalled, M measured here)

| model | causal | published downbeat F1 (GTZAN) | ours | latency | size / compute | licence | ESP32-S3? |
|---|---|---|---|---|---|---|---|
| BeatNet (Heydari et al., ISMIR 2021) | yes (particle filter) | 46.49 (V) | Traktor 30 %, GTZAN 52 % (M) | 46 ms (V) | 402,325 params (V), 1750 particles | CC-BY-4.0 (V) | would fit (~3x our tracker's MACs/s) but its beats and "1" are worse: no |
| BeatNet+ (TISMIR 2024) | yes | 56.51 (V) | not run | 20 ms (V) | 4 LSTM x 150 (V), ~0.8M params (est.) | no licence file (V) | licence blocks it |
| BEAST (ICASSP 2024) | yes (streaming transformer) | 52.23 (V) | not run | 46 ms (V) | 7.5M params (V) | not stated (R) | no (~30 MB float32) |
| 1D state space (ICASSP 2022) | yes | 42.57 (V) | not run | 20 ms | RTF 0.02 | R | below BeatNet |
| madmom RNNDownBeatProcessor (ISMIR 2016) | no | 51.36 (V) | Traktor 54 %, GTZAN 65 % (M) | offline | ~0.1 s per s per core (M) | CC BY-NC-SA (R) | no (not causal) |
| Beat This! small0 (ISMIR 2024) | no | R | > 1.9x real time per farm core streamed (M) | 0.36 s streamed | 2.1M params (V) | R | no |

Sources: https://arxiv.org/abs/2108.03576, https://github.com/mjhydri/BeatNet, https://par.nsf.gov/servlets/purl/10635884,
https://github.com/mjhydri/BeatNet-Plus, https://arxiv.org/abs/2312.17156

## 4. Recommendation

Build the learned linear rule as a SUGGESTION and keep the manual NO + UP/DOWN: it costs nothing on the device and lifts
the right-bar rate from 25 % to ~61 % (~70 % where the reference is trustworthy). Not good enough to move Link's bar on
its own while people play: choose the bar once when Link starts (after ~8-16 bars), move it later only on a large score
margin and only if the user hasn't shifted it by hand; the user's shift always wins; log every manual shift as a label.
Don't try a model now. Before shipping, settle the reference: ~50 tracks labelled by ear (or the logged shifts) to refit
the 88 weights and give the real accuracy (results/weights.json was fitted against Traktor's "1").

## Files and re-running

Kept here: extract.py, rules.py, linear.py, analyze.py, analyze_main.py, ref_check.py, genres.py, beatthis_stream_db.py,
run_farm.sh, run_farm2.sh, genres.json, results/. Removed: the Mac pickles (~140 MB) and the farm folder (134 MB).
Re-run: run_farm2.sh on gpufarm (~13 min, 12 workers), the madmom line of run_farm.sh (~19 min, 8 workers), then
analyze_main.py and ref_check.py (~2 min, one core).
