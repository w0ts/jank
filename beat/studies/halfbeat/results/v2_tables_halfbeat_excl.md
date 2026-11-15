Half-beat references (fixes.json, desc {ds}_desc.pkl): left out ['traktor_off', 'unclear']: 55 tracks; Traktor-off tracks kept (shifted if the pickle was built with --ref-fix): 0

## Traktor: 829 tracks, dev = even 386, test = odd 388. madmom offline measured on 803; Traktor and madmom agree (>= 90 % of beats) on 320 (dev 151, test 163)

### Dev half, Link beats, 2-fold cross-fitted, trained on reference beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| a5: S, N=16, causal (delay 5), margin 1, all dev tracks for training | 385 | 63.7 | 79.8 | 67.5 | 35 | 5 | 16 | 3 | 0.61 | 88 |
| a5: S, N=16, causal (delay 5), margin 1, self-cleaned training (329 of 386 dev tracks kept) | 385 | 65.1 | 79.3 | 68.8 | 35 | 6 | 15 | 4 | 0.82 | 88 |
|   (madmom measured 56 of the dropped tracks: agreeing with Traktor on 9, mean agreement 26 %) |
| d5: S+Dp, N=16, causal (delay 5), margin 1.5, all dev tracks for training | 385 | 63.9 | 77.0 | 68.3 | 35 | 5 | 16 | 2 | 0.49 | 88 |
| d5: S+Dp, N=16, causal (delay 5), margin 1.5, self-cleaned training (332 of 386 dev tracks kept) | 385 | 64.7 | 75.9 | 69.3 | 34 | 6 | 15 | 3 | 0.69 | 88 |
|   (madmom measured 54 of the dropped tracks: agreeing with Traktor on 9, mean agreement 28 %) |

Chosen on dev: a5 (S, N=16, causal (delay 5)), margin 1, training on 329 dev tracks

Noisy reference, dev half, Link beats (cross-fitted; madmom selects tracks: a measurement, not for shipping)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| chosen training, scored on the 151 dev tracks where Traktor and madmom agree | 151 | 75.6 | 88.7 | 76.8 | 46 | 6 | 3 | 4 | 0.78 | 92 |
| trained on the agreeing dev tracks only (151), all dev | 385 | 63.1 | 77.5 | 66.8 | 32 | 7 | 15 | 5 | 0.93 | 88 |
| trained on the agreeing dev tracks only, agreeing dev | 151 | 75.0 | 88.1 | 76.0 | 43 | 7 | 4 | 4 | 0.88 | 92 |

### Link's bar: moves only when confident, dev half, Link beats (cross-fitted)

| method | tracks | right (Link's bar as heard), mean % | median % | tracks >= 90 % | median s to first move | never moved % | first move right % | later moves/min | later moves wrong % |
|---|---|---|---|---|---|---|---|---|---|
| commit 2.0 switch 1.0 hold 16 wait 0 | 385 | 64.0 | 78.3 | 34 | 4 | 0 | 57 | 0.22 | 41 |
| commit 2.0 switch 1.0 hold 16 wait 16 | 385 | 58.5 | 63.2 | 24 | 17 | 11 | 67 | 0.13 | 36 |
| commit 2.0 switch 1.0 hold 16 wait 32 | 385 | 56.2 | 61.2 | 19 | 35 | 14 | 71 | 0.11 | 34 |
| commit 2.0 switch 1.0 hold 16 wait 64 | 385 | 54.2 | 57.9 | 13 | 52 | 16 | 76 | 0.09 | 34 |
| commit 2.0 switch 2.0 hold 16 wait 0 | 385 | 61.6 | 74.9 | 36 | 4 | 0 | 57 | 0.11 | 31 |
| commit 2.0 switch 2.0 hold 16 wait 16 | 385 | 56.1 | 61.2 | 24 | 17 | 11 | 67 | 0.08 | 26 |
| commit 2.0 switch 2.0 hold 16 wait 32 | 385 | 54.3 | 57.1 | 19 | 35 | 14 | 71 | 0.07 | 23 |
| commit 2.0 switch 2.0 hold 16 wait 64 | 385 | 53.4 | 56.8 | 14 | 52 | 16 | 76 | 0.06 | 21 |
| commit 2.0 switch 3.0 hold 16 wait 0 | 385 | 57.7 | 65.8 | 35 | 4 | 0 | 57 | 0.07 | 19 |
| commit 2.0 switch 3.0 hold 16 wait 16 | 385 | 53.2 | 54.4 | 24 | 17 | 11 | 67 | 0.05 | 17 |
| commit 2.0 switch 3.0 hold 16 wait 32 | 385 | 52.0 | 53.2 | 18 | 35 | 14 | 71 | 0.05 | 15 |
| commit 2.0 switch 3.0 hold 16 wait 64 | 385 | 51.5 | 51.3 | 14 | 52 | 16 | 76 | 0.04 | 11 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 2.0 hold 16 | 385 | 61.4 | 74.9 | 37 | 4 | 0 | 57 | 0.13 | 35 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 2.0 hold 32 | 385 | 59.6 | 70.8 | 36 | 4 | 0 | 57 | 0.11 | 34 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 3.0 hold 16 | 385 | 58.2 | 66.6 | 36 | 4 | 0 | 57 | 0.09 | 28 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 3.0 hold 32 | 385 | 56.3 | 61.2 | 35 | 4 | 0 | 57 | 0.08 | 31 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 2.0 hold 16 | 385 | 61.1 | 73.9 | 36 | 4 | 0 | 57 | 0.14 | 37 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 2.0 hold 32 | 385 | 59.5 | 70.5 | 36 | 4 | 0 | 57 | 0.13 | 38 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 3.0 hold 16 | 385 | 58.4 | 66.6 | 36 | 4 | 0 | 57 | 0.11 | 33 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 3.0 hold 32 | 385 | 57.4 | 64.9 | 35 | 4 | 0 | 57 | 0.10 | 34 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 2.0 hold 16 | 385 | 61.6 | 76.5 | 37 | 4 | 0 | 57 | 0.16 | 40 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 2.0 hold 32 | 385 | 60.1 | 75.0 | 37 | 4 | 0 | 57 | 0.15 | 42 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 3.0 hold 16 | 385 | 59.6 | 72.2 | 37 | 4 | 0 | 57 | 0.13 | 40 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 3.0 hold 32 | 385 | 58.9 | 70.8 | 36 | 4 | 0 | 57 | 0.13 | 42 |

Chosen on dev: responsive (best mean right) commit/switch/hold/wait (1.0, 1.0, 8, 0); stable (best mean right with at most 0.1 later moves a minute) (2.0, 2.0, 32, 0)

## TEST half (odd, 388 tracks), weights trained on 329 dev tracks' reference beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study's weights, its protocol (features 4 beats ahead), online beats | 388 | 62.1 | 71.0 | 65.0 | 24 | 20 | 13 | 4 | 0.81 | 86 |
| first study's weights, its protocol (features 4 beats ahead), link beats | 386 | 65.3 | 77.1 | 68.0 | 33 | 6 | 13 | 3 | 0.78 | 86 |
| first study's weights, its protocol (features 4 beats ahead), ref beats | 388 | 73.3 | 89.7 | 73.0 | 49 | 4 | 5 | 2 | 0.55 | 100 |
| first study's weights, causal (delay 5), online beats | 388 | 61.6 | 70.4 | 64.5 | 22 | 21 | 13 | 4 | 0.85 | 86 |
| NEW: S, N=16, causal (delay 5), margin 1, link beats | 386 | 67.9 | 79.5 | 71.8 | 36 | 6 | 12 | 4 | 0.85 | 86 |
| NEW: S, N=16, causal (delay 5), margin 1, online beats | 388 | 60.1 | 68.0 | 63.2 | 19 | 34 | 13 | 4 | 0.80 | 86 |
| NEW: S, N=16, causal (delay 5), margin 1, ref beats | 388 | 79.1 | 97.0 | 79.8 | 60 | 4 | 6 | 2 | 0.48 | 100 |
| first study's weights, online beats, 163 test tracks where Traktor and madmom agree | 163 | 73.9 | 82.7 | 74.8 | 38 | 16 | 5 | 4 | 0.77 | 88 |
| NEW, Link beats, the same 163 agreeing test tracks | 163 | 78.9 | 89.7 | 80.5 | 49 | 6 | 4 | 3 | 0.78 | 89 |
| NEW, Link beats, reference = madmom offline (383 4/4 test tracks) | 383 | 52.4 | 55.4 | 51.5 | 24 | 8 | 21 | 4 | 0.84 | 86 |
| first study's weights, online beats, reference = madmom offline (same 383 tracks) | 383 | 50.1 | 49.5 | 49.3 | 17 | 26 | 15 | 4 | 0.81 | 88 |
| Traktor's own '1' scored against madmom offline on Link beats (same 383 tracks): see the agreement line above; mean agreement 56 % |
| NEW, Link beats, test tracks where the tracker's tempo = Traktor's (360) | 358 | 71.1 | 84.0 | 72.7 | 39 | 6 | 7 | 4 | 0.83 | 89 |

Splits, TEST, Link beats, pooled: by genre electro / breaks 67 % (66 tracks); house 78 % (46 tracks); other / untagged 72 % (31 tracks); pop / hyperpop 63 % (27 tracks); techno 72 % (218 tracks)
in a break 73 % (39361 beats); not in a break 72 % (187596 beats)

### Link's bar, TEST half, Link beats

| method | tracks | right (Link's bar as heard), mean % | median % | tracks >= 90 % | median s to first move | never moved % | first move right % | later moves/min | later moves wrong % |
|---|---|---|---|---|---|---|---|---|---|
| today: Link's arbitrary bar | - | 25.0 | 25.0 | 0 | - | 100 | 0 | - |
| NEW responsive: (1.0, 1.0, 8, 0) | 386 | 67.1 | 80.1 | 35 | 4 | 0 | 57 | 0.25 | 40 |
| NEW stable: (2.0, 2.0, 32, 0) | 386 | 61.9 | 75.6 | 36 | 4 | 0 | 62 | 0.11 | 30 |

### Track change (simulated cut, same tempo), TEST half pairs, Link beats

- no reset (memory carries over): 194 cuts, re-lock median 38 s; within 15 s 24 %, 30 s 41 %, 60 s 58 %, never 12 %
- accumulators reset at the cut (a detected change): 194 cuts, re-lock median 11 s; within 15 s 50 %, 30 s 64 %, 60 s 71 %, never 12 %

Alternative with half the memory (S, N=8, causal (delay 5), margin 1.5, same training tracks), TEST:

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| S, N=8, causal (delay 5), margin 1.5, link beats | 386 | 68.0 | 78.1 | 71.4 | 35 | 4 | 11 | 3 | 0.77 | 86 |

### Track change (simulated cut, same tempo), TEST half pairs, Link beats

- no reset (memory carries over): 194 cuts, re-lock median 29 s; within 15 s 27 %, 30 s 45 %, 60 s 62 %, never 13 %
- accumulators reset at the cut (a detected change): 194 cuts, re-lock median 9 s; within 15 s 55 %, 30 s 65 %, 60 s 72 %, never 11 %

## GTZAN (human downbeats, 30 s clips), odd half (499)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study's weights, online beats | 484 | 42.8 | 27.5 | 46.9 | 24 | 5 | 58 | 1 | 2.84 | 79 |
| NEW (Traktor-trained), link beats | 420 | 46.2 | 32.3 | 50.3 | 26 | 6 | 58 | 2 | 8.59 | 79 |
| NEW (Traktor-trained), online beats | 484 | 40.5 | 26.7 | 43.9 | 22 | 7 | 60 | 2 | 5.51 | 79 |
| NEW structure trained on GTZAN even half (reference beats), link beats | 420 | 47.8 | 28.6 | 53.2 | 36 | 4 | 58 | 0 | 1.91 | 79 |
- NEW (Traktor-trained), Link beats, by genre: blues 40; classical 32; country 45; disco 55; hiphop 64; jazz 13; metal 53; pop 61; reggae 53; rock 49

88 weights. Run time 131 s.
