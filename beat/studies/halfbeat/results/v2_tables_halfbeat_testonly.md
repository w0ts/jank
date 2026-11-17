Half-beat references (fixes.json, desc {ds}_desc.pkl): left out ['traktor_off', 'unclear']: 55 tracks; Traktor-off tracks kept (shifted if the pickle was built with --ref-fix): 0

## Traktor: 829 tracks, dev = even 415, test = odd 388. madmom offline measured on 803; Traktor and madmom agree (>= 90 % of beats) on 320 (dev 152, test 163)

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

## TEST half (odd, 388 tracks), weights trained on 346 dev tracks' reference beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study's weights, its protocol (features 4 beats ahead), online beats | 388 | 62.1 | 71.0 | 65.0 | 24 | 20 | 13 | 4 | 0.81 | 86 |
| first study's weights, its protocol (features 4 beats ahead), link beats | 386 | 65.3 | 77.1 | 68.0 | 33 | 6 | 13 | 3 | 0.78 | 86 |
| first study's weights, its protocol (features 4 beats ahead), ref beats | 388 | 73.3 | 89.7 | 73.0 | 49 | 4 | 5 | 2 | 0.55 | 100 |
| first study's weights, causal (delay 5), online beats | 388 | 61.6 | 70.4 | 64.5 | 22 | 21 | 13 | 4 | 0.85 | 86 |
| NEW: S, N=16, causal (delay 5), margin 1, link beats | 386 | 67.6 | 79.7 | 71.4 | 35 | 5 | 12 | 4 | 0.83 | 86 |
| NEW: S, N=16, causal (delay 5), margin 1, online beats | 388 | 60.0 | 67.9 | 63.2 | 19 | 36 | 12 | 4 | 0.79 | 86 |
| NEW: S, N=16, causal (delay 5), margin 1, ref beats | 388 | 78.8 | 97.0 | 79.4 | 60 | 4 | 6 | 2 | 0.48 | 100 |
| first study's weights, online beats, 163 test tracks where Traktor and madmom agree | 163 | 73.9 | 82.7 | 74.8 | 38 | 16 | 5 | 4 | 0.77 | 88 |
| NEW, Link beats, the same 163 agreeing test tracks | 163 | 79.1 | 89.4 | 80.4 | 48 | 7 | 4 | 4 | 0.78 | 89 |
| NEW, Link beats, reference = madmom offline (383 4/4 test tracks) | 383 | 52.6 | 55.3 | 51.6 | 24 | 8 | 21 | 4 | 0.82 | 86 |
| first study's weights, online beats, reference = madmom offline (same 383 tracks) | 383 | 50.1 | 49.5 | 49.3 | 17 | 26 | 15 | 4 | 0.81 | 88 |
| Traktor's own '1' scored against madmom offline on Link beats (same 383 tracks): see the agreement line above; mean agreement 56 % |
| NEW, Link beats, test tracks where the tracker's tempo = Traktor's (360) | 358 | 70.8 | 82.3 | 72.3 | 38 | 5 | 6 | 4 | 0.82 | 89 |

Splits, TEST, Link beats, pooled: by genre electro / breaks 64 % (66 tracks); house 77 % (46 tracks); other / untagged 72 % (31 tracks); pop / hyperpop 61 % (27 tracks); techno 72 % (218 tracks)
in a break 73 % (39361 beats); not in a break 71 % (187596 beats)

### Link's bar, TEST half, Link beats

| method | tracks | right (Link's bar as heard), mean % | median % | tracks >= 90 % | median s to first move | never moved % | first move right % | later moves/min | later moves wrong % |
|---|---|---|---|---|---|---|---|---|---|
| today: Link's arbitrary bar | - | 25.0 | 25.0 | 0 | - | 100 | 0 | - |
| NEW responsive: (2.0, 1.0, 8, 0) | 386 | 67.2 | 80.1 | 36 | 4 | 0 | 62 | 0.24 | 41 |
| NEW stable: (2.0, 3.0, 8, 0) | 386 | 62.2 | 75.2 | 39 | 4 | 0 | 62 | 0.09 | 24 |

### Track change (simulated cut, same tempo), TEST half pairs, Link beats

- no reset (memory carries over): 194 cuts, re-lock median 41 s; within 15 s 24 %, 30 s 40 %, 60 s 57 %, never 12 %
- accumulators reset at the cut (a detected change): 194 cuts, re-lock median 11 s; within 15 s 51 %, 30 s 64 %, 60 s 70 %, never 11 %

Alternative with half the memory (S, N=8, causal (delay 5), margin 1.5, same training tracks), TEST:

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| S, N=8, causal (delay 5), margin 1.5, link beats | 386 | 67.4 | 79.9 | 70.9 | 36 | 4 | 12 | 3 | 0.77 | 86 |

### Track change (simulated cut, same tempo), TEST half pairs, Link beats

- no reset (memory carries over): 194 cuts, re-lock median 29 s; within 15 s 27 %, 30 s 44 %, 60 s 63 %, never 13 %
- accumulators reset at the cut (a detected change): 194 cuts, re-lock median 9 s; within 15 s 54 %, 30 s 65 %, 60 s 71 %, never 11 %

## GTZAN (human downbeats, 30 s clips), odd half (499)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study's weights, online beats | 484 | 42.8 | 27.5 | 46.9 | 24 | 5 | 58 | 1 | 2.84 | 79 |
| NEW (Traktor-trained), link beats | 420 | 46.6 | 33.3 | 50.9 | 26 | 6 | 58 | 2 | 8.23 | 79 |
| NEW (Traktor-trained), online beats | 484 | 40.9 | 26.9 | 44.2 | 22 | 7 | 60 | 2 | 5.23 | 79 |
| NEW structure trained on GTZAN even half (reference beats), link beats | 420 | 47.8 | 28.6 | 53.2 | 36 | 4 | 58 | 0 | 1.91 | 79 |
- NEW (Traktor-trained), Link beats, by genre: blues 40; classical 48; country 45; disco 57; hiphop 64; jazz 13; metal 52; pop 59; reggae 54; rock 49

88 weights. Run time 127 s.
