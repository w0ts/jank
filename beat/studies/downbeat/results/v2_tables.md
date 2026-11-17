## Traktor: 829 tracks, dev = even 415, test = odd 414. madmom offline measured on 803; Traktor and madmom agree (>= 90 % of beats) on 320 (dev 152, test 168)

### Dev half, Link beats, 2-fold cross-fitted, trained on reference beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| a5: S, N=16, causal (delay 5), margin 1, all dev tracks for training | 411 | 62.8 | 77.5 | 67.7 | 34 | 6 | 17 | 3 | 0.65 | 84 |
| a5: S, N=16, causal (delay 5), margin 1, self-cleaned training (346 of 415 dev tracks kept) | 411 | 63.3 | 76.3 | 68.2 | 33 | 9 | 16 | 4 | 0.85 | 84 |
|   (madmom measured 63 of the dropped tracks: agreeing with Traktor on 7, mean agreement 23 %) |
| d5: S+Dp, N=16, causal (delay 5), margin 1.5, all dev tracks for training | 411 | 61.9 | 72.7 | 67.6 | 36 | 6 | 18 | 2 | 0.52 | 84 |
| d5: S+Dp, N=16, causal (delay 5), margin 1.5, self-cleaned training (358 of 415 dev tracks kept) | 411 | 62.8 | 75.6 | 68.6 | 33 | 6 | 16 | 3 | 0.72 | 84 |
|   (madmom measured 54 of the dropped tracks: agreeing with Traktor on 6, mean agreement 24 %) |

Chosen on dev: a5 (S, N=16, causal (delay 5)), margin 1, training on 346 dev tracks

Noisy reference, dev half, Link beats (cross-fitted; madmom selects tracks: a measurement, not for shipping)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| chosen training, scored on the 152 dev tracks where Traktor and madmom agree | 152 | 75.6 | 88.8 | 77.1 | 47 | 6 | 4 | 4 | 0.78 | 92 |
| trained on the agreeing dev tracks only (152), all dev | 411 | 62.6 | 75.2 | 67.4 | 32 | 9 | 16 | 5 | 0.95 | 84 |
| trained on the agreeing dev tracks only, agreeing dev | 152 | 76.1 | 88.1 | 76.9 | 45 | 7 | 4 | 4 | 0.87 | 92 |

### Link's bar: moves only when confident, dev half, Link beats (cross-fitted)

| method | tracks | right (Link's bar as heard), mean % | median % | tracks >= 90 % | median s to first move | never moved % | first move right % | later moves/min | later moves wrong % |
|---|---|---|---|---|---|---|---|---|---|
| commit 2.0 switch 1.0 hold 16 wait 0 | 411 | 62.3 | 74.8 | 32 | 4 | 0 | 55 | 0.21 | 40 |
| commit 2.0 switch 1.0 hold 16 wait 16 | 411 | 57.1 | 61.4 | 24 | 15 | 14 | 66 | 0.13 | 36 |
| commit 2.0 switch 1.0 hold 16 wait 32 | 411 | 55.0 | 58.0 | 18 | 31 | 17 | 70 | 0.11 | 36 |
| commit 2.0 switch 1.0 hold 16 wait 64 | 411 | 52.3 | 53.0 | 13 | 52 | 21 | 75 | 0.08 | 32 |
| commit 2.0 switch 2.0 hold 16 wait 0 | 411 | 60.3 | 71.2 | 35 | 4 | 0 | 55 | 0.12 | 30 |
| commit 2.0 switch 2.0 hold 16 wait 16 | 411 | 54.8 | 56.0 | 24 | 15 | 14 | 66 | 0.08 | 26 |
| commit 2.0 switch 2.0 hold 16 wait 32 | 411 | 53.1 | 53.4 | 18 | 31 | 17 | 70 | 0.07 | 25 |
| commit 2.0 switch 2.0 hold 16 wait 64 | 411 | 51.4 | 50.5 | 14 | 52 | 21 | 75 | 0.05 | 23 |
| commit 2.0 switch 3.0 hold 16 wait 0 | 411 | 55.7 | 59.9 | 33 | 4 | 0 | 55 | 0.07 | 19 |
| commit 2.0 switch 3.0 hold 16 wait 16 | 411 | 51.5 | 45.9 | 24 | 15 | 14 | 66 | 0.05 | 19 |
| commit 2.0 switch 3.0 hold 16 wait 32 | 411 | 50.7 | 46.5 | 18 | 31 | 17 | 70 | 0.05 | 18 |
| commit 2.0 switch 3.0 hold 16 wait 64 | 411 | 49.4 | 44.9 | 14 | 52 | 21 | 75 | 0.03 | 13 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 2.0 hold 16 | 411 | 60.4 | 71.8 | 35 | 4 | 0 | 55 | 0.13 | 33 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 2.0 hold 32 | 411 | 58.7 | 67.5 | 34 | 4 | 0 | 55 | 0.12 | 33 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 3.0 hold 16 | 411 | 56.2 | 61.2 | 34 | 4 | 0 | 55 | 0.09 | 30 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 3.0 hold 32 | 411 | 54.4 | 58.6 | 33 | 4 | 0 | 55 | 0.08 | 34 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 2.0 hold 16 | 411 | 59.5 | 70.9 | 35 | 4 | 0 | 55 | 0.15 | 37 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 2.0 hold 32 | 411 | 58.4 | 67.2 | 34 | 4 | 0 | 55 | 0.13 | 35 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 3.0 hold 16 | 411 | 56.2 | 63.3 | 34 | 4 | 0 | 55 | 0.11 | 34 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 3.0 hold 32 | 411 | 55.3 | 59.9 | 33 | 4 | 0 | 55 | 0.10 | 37 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 2.0 hold 16 | 411 | 59.5 | 73.0 | 35 | 4 | 0 | 55 | 0.17 | 42 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 2.0 hold 32 | 411 | 58.6 | 68.5 | 35 | 4 | 0 | 55 | 0.15 | 42 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 3.0 hold 16 | 411 | 57.8 | 69.3 | 35 | 4 | 0 | 55 | 0.14 | 42 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 3.0 hold 32 | 411 | 57.2 | 67.2 | 34 | 4 | 0 | 55 | 0.13 | 44 |

Chosen on dev: responsive (best mean right) commit/switch/hold/wait (2.0, 1.0, 8, 0); stable (best mean right with at most 0.1 later moves a minute) (2.0, 3.0, 8, 0)

## TEST half (odd, 414 tracks), weights trained on 346 dev tracks' reference beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study's weights, its protocol (features 4 beats ahead), online beats | 411 | 61.0 | 69.6 | 64.6 | 24 | 21 | 14 | 4 | 0.82 | 82 |
| first study's weights, its protocol (features 4 beats ahead), link beats | 408 | 64.2 | 75.2 | 67.8 | 31 | 6 | 14 | 3 | 0.79 | 83 |
| first study's weights, its protocol (features 4 beats ahead), ref beats | 414 | 71.5 | 87.7 | 71.2 | 48 | 4 | 6 | 2 | 0.57 | 100 |
| first study's weights, causal (delay 5), online beats | 411 | 60.4 | 69.0 | 64.1 | 22 | 22 | 14 | 4 | 0.86 | 82 |
| NEW: S, N=16, causal (delay 5), margin 1, link beats | 408 | 66.2 | 77.3 | 71.1 | 34 | 6 | 13 | 4 | 0.84 | 83 |
| NEW: S, N=16, causal (delay 5), margin 1, online beats | 411 | 58.6 | 66.2 | 62.9 | 18 | 38 | 14 | 4 | 0.81 | 82 |
| NEW: S, N=16, causal (delay 5), margin 1, ref beats | 414 | 77.3 | 96.2 | 77.8 | 57 | 4 | 7 | 2 | 0.50 | 100 |
| first study's weights, online beats, 168 test tracks where Traktor and madmom agree | 168 | 72.9 | 82.5 | 74.6 | 38 | 16 | 6 | 4 | 0.77 | 87 |
| NEW, Link beats, the same 168 agreeing test tracks | 168 | 78.2 | 88.8 | 80.2 | 47 | 7 | 4 | 4 | 0.78 | 87 |
| NEW, Link beats, reference = madmom offline (399 4/4 test tracks) | 399 | 51.8 | 54.8 | 51.0 | 23 | 8 | 22 | 4 | 0.84 | 86 |
| first study's weights, online beats, reference = madmom offline (same 399 tracks) | 399 | 49.5 | 48.5 | 48.9 | 17 | 27 | 15 | 4 | 0.81 | 87 |
| Traktor's own '1' scored against madmom offline on Link beats (same 399 tracks): see the agreement line above; mean agreement 55 % |
| NEW, Link beats, test tracks where the tracker's tempo = Traktor's (385) | 379 | 69.2 | 81.2 | 72.0 | 36 | 6 | 8 | 4 | 0.83 | 85 |

Splits, TEST, Link beats, pooled: by genre electro / breaks 64 % (75 tracks); house 77 % (46 tracks); other / untagged 72 % (32 tracks); pop / hyperpop 61 % (28 tracks); techno 72 % (233 tracks)
in a break 72 % (40246 beats); not in a break 71 % (190808 beats)

### Link's bar, TEST half, Link beats

| method | tracks | right (Link's bar as heard), mean % | median % | tracks >= 90 % | median s to first move | never moved % | first move right % | later moves/min | later moves wrong % |
|---|---|---|---|---|---|---|---|---|---|
| today: Link's arbitrary bar | - | 25.0 | 25.0 | 0 | - | 100 | 0 | - |
| NEW responsive: (2.0, 1.0, 8, 0) | 408 | 65.7 | 77.6 | 34 | 4 | 0 | 60 | 0.25 | 42 |
| NEW stable: (2.0, 3.0, 8, 0) | 408 | 61.0 | 73.3 | 38 | 4 | 0 | 60 | 0.09 | 23 |

### Track change (simulated cut, same tempo), TEST half pairs, Link beats

- no reset (memory carries over): 207 cuts, re-lock median 38 s; within 15 s 20 %, 30 s 36 %, 60 s 56 %, never 18 %
- accumulators reset at the cut (a detected change): 207 cuts, re-lock median 9 s; within 15 s 49 %, 30 s 62 %, 60 s 67 %, never 17 %

Alternative with half the memory (S, N=8, causal (delay 5), margin 1.5, same training tracks), TEST:

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| S, N=8, causal (delay 5), margin 1.5, link beats | 408 | 66.0 | 76.6 | 70.6 | 35 | 4 | 13 | 3 | 0.78 | 83 |

### Track change (simulated cut, same tempo), TEST half pairs, Link beats

- no reset (memory carries over): 207 cuts, re-lock median 32 s; within 15 s 21 %, 30 s 41 %, 60 s 58 %, never 17 %
- accumulators reset at the cut (a detected change): 207 cuts, re-lock median 8 s; within 15 s 52 %, 30 s 63 %, 60 s 68 %, never 16 %

## GTZAN (human downbeats, 30 s clips), odd half (499)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study's weights, online beats | 484 | 42.8 | 27.5 | 46.9 | 24 | 5 | 58 | 1 | 2.84 | 79 |
| NEW (Traktor-trained), link beats | 420 | 46.6 | 33.3 | 50.9 | 26 | 6 | 58 | 2 | 8.23 | 79 |
| NEW (Traktor-trained), online beats | 484 | 40.9 | 26.9 | 44.2 | 22 | 7 | 60 | 2 | 5.23 | 79 |
| NEW structure trained on GTZAN even half (reference beats), link beats | 420 | 47.8 | 28.6 | 53.2 | 36 | 4 | 58 | 0 | 1.91 | 79 |
- NEW (Traktor-trained), Link beats, by genre: blues 40; classical 48; country 45; disco 57; hiphop 64; jazz 13; metal 52; pop 59; reggae 54; rock 49

88 weights. Run time 128 s.
