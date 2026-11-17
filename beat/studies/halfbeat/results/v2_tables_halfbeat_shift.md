Half-beat references (fixes.json, desc {ds}_desc_fix.pkl): left out ['unclear']: 22 tracks; Traktor-off tracks kept (shifted if the pickle was built with --ref-fix): 33

## Traktor: 829 tracks, dev = even 408, test = odd 399. madmom offline measured on 822; Traktor and madmom agree (>= 90 % of beats) on 335 (dev 161, test 169)

### Dev half, Link beats, 2-fold cross-fitted, trained on reference beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| a5: S, N=16, causal (delay 5), margin 1, all dev tracks for training | 407 | 64.5 | 79.1 | 67.9 | 34 | 6 | 14 | 3 | 0.63 | 88 |
| a5: S, N=16, causal (delay 5), margin 1, self-cleaned training (349 of 408 dev tracks kept) | 407 | 64.0 | 78.4 | 67.4 | 34 | 7 | 14 | 4 | 0.81 | 88 |
|   (madmom measured 58 of the dropped tracks: agreeing with Traktor on 9, mean agreement 26 %) |
| d5: S+Dp, N=16, causal (delay 5), margin 1.5, all dev tracks for training | 407 | 62.9 | 75.8 | 67.0 | 34 | 6 | 15 | 2 | 0.50 | 88 |
| d5: S+Dp, N=16, causal (delay 5), margin 1.5, self-cleaned training (349 of 408 dev tracks kept) | 407 | 64.2 | 75.8 | 68.5 | 33 | 6 | 13 | 3 | 0.72 | 88 |
|   (madmom measured 59 of the dropped tracks: agreeing with Traktor on 11, mean agreement 29 %) |

Chosen on dev: a5 (S, N=16, causal (delay 5)), margin 1, training on 408 dev tracks

Noisy reference, dev half, Link beats (cross-fitted; madmom selects tracks: a measurement, not for shipping)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| chosen training, scored on the 161 dev tracks where Traktor and madmom agree | 161 | 72.1 | 84.3 | 73.3 | 42 | 5 | 4 | 3 | 0.58 | 91 |
| trained on the agreeing dev tracks only (161), all dev | 407 | 64.0 | 76.6 | 66.9 | 34 | 8 | 14 | 4 | 0.91 | 88 |
| trained on the agreeing dev tracks only, agreeing dev | 161 | 74.7 | 87.3 | 75.5 | 45 | 7 | 3 | 4 | 0.80 | 91 |

### Link's bar: moves only when confident, dev half, Link beats (cross-fitted)

| method | tracks | right (Link's bar as heard), mean % | median % | tracks >= 90 % | median s to first move | never moved % | first move right % | later moves/min | later moves wrong % |
|---|---|---|---|---|---|---|---|---|---|
| commit 2.0 switch 1.0 hold 16 wait 0 | 407 | 60.4 | 71.9 | 32 | 4 | 8 | 61 | 0.14 | 36 |
| commit 2.0 switch 1.0 hold 16 wait 16 | 407 | 52.0 | 46.1 | 18 | 23 | 29 | 74 | 0.08 | 34 |
| commit 2.0 switch 1.0 hold 16 wait 32 | 407 | 49.8 | 36.9 | 14 | 43 | 34 | 78 | 0.06 | 33 |
| commit 2.0 switch 1.0 hold 16 wait 64 | 407 | 46.8 | 25.6 | 10 | 61 | 38 | 81 | 0.04 | 30 |
| commit 2.0 switch 2.0 hold 16 wait 0 | 407 | 56.1 | 60.5 | 33 | 4 | 8 | 61 | 0.07 | 22 |
| commit 2.0 switch 2.0 hold 16 wait 16 | 407 | 49.6 | 38.3 | 18 | 23 | 29 | 74 | 0.04 | 25 |
| commit 2.0 switch 2.0 hold 16 wait 32 | 407 | 48.3 | 33.2 | 15 | 43 | 34 | 78 | 0.03 | 19 |
| commit 2.0 switch 2.0 hold 16 wait 64 | 407 | 45.8 | 25.2 | 10 | 61 | 38 | 81 | 0.02 | 11 |
| commit 2.0 switch 3.0 hold 16 wait 0 | 407 | 51.3 | 46.7 | 32 | 4 | 8 | 61 | 0.03 | 21 |
| commit 2.0 switch 3.0 hold 16 wait 16 | 407 | 47.9 | 32.2 | 18 | 23 | 29 | 74 | 0.02 | 22 |
| commit 2.0 switch 3.0 hold 16 wait 32 | 407 | 47.0 | 28.9 | 15 | 43 | 34 | 78 | 0.02 | 12 |
| commit 2.0 switch 3.0 hold 16 wait 64 | 407 | 44.8 | 25.1 | 11 | 61 | 38 | 81 | 0.01 | 8 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 2.0 hold 16 | 407 | 55.9 | 60.5 | 33 | 4 | 8 | 61 | 0.08 | 27 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 2.0 hold 32 | 407 | 54.3 | 56.6 | 32 | 4 | 8 | 61 | 0.07 | 29 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 3.0 hold 16 | 407 | 52.4 | 48.8 | 33 | 4 | 8 | 61 | 0.04 | 29 |
| hybrid: commit 2 switch 1 hold 8 for 32 beats, then switch 3.0 hold 32 | 407 | 51.4 | 45.6 | 33 | 4 | 8 | 61 | 0.04 | 27 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 2.0 hold 16 | 407 | 56.2 | 60.5 | 33 | 4 | 8 | 61 | 0.08 | 30 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 2.0 hold 32 | 407 | 55.2 | 58.6 | 33 | 4 | 8 | 61 | 0.08 | 32 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 3.0 hold 16 | 407 | 53.7 | 51.6 | 34 | 4 | 8 | 61 | 0.05 | 33 |
| hybrid: commit 2 switch 1 hold 8 for 64 beats, then switch 3.0 hold 32 | 407 | 53.2 | 50.7 | 34 | 4 | 8 | 61 | 0.05 | 32 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 2.0 hold 16 | 407 | 57.2 | 65.1 | 34 | 4 | 8 | 61 | 0.10 | 36 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 2.0 hold 32 | 407 | 56.6 | 61.1 | 33 | 4 | 8 | 61 | 0.09 | 38 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 3.0 hold 16 | 407 | 55.4 | 59.5 | 34 | 4 | 8 | 61 | 0.07 | 39 |
| hybrid: commit 2 switch 1 hold 8 for 128 beats, then switch 3.0 hold 32 | 407 | 55.1 | 56.7 | 34 | 4 | 8 | 61 | 0.07 | 39 |

Chosen on dev: responsive (best mean right) commit/switch/hold/wait (1.0, 1.0, 8, 0); stable (best mean right with at most 0.1 later moves a minute) (2.0, 1.0, 8, 0, 128, 2.0, 16)

## TEST half (odd, 399 tracks), weights trained on 408 dev tracks' reference beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study's weights, its protocol (features 4 beats ahead), online beats | 399 | 61.5 | 70.4 | 64.3 | 25 | 20 | 13 | 4 | 0.80 | 86 |
| first study's weights, its protocol (features 4 beats ahead), link beats | 397 | 64.6 | 76.9 | 67.3 | 32 | 5 | 14 | 3 | 0.78 | 86 |
| first study's weights, its protocol (features 4 beats ahead), ref beats | 399 | 72.6 | 89.5 | 72.4 | 49 | 4 | 6 | 2 | 0.55 | 100 |
| first study's weights, causal (delay 5), online beats | 399 | 61.0 | 70.0 | 63.8 | 22 | 21 | 13 | 4 | 0.84 | 86 |
| NEW: S, N=16, causal (delay 5), margin 1, link beats | 397 | 66.4 | 79.3 | 70.1 | 36 | 4 | 13 | 3 | 0.63 | 86 |
| NEW: S, N=16, causal (delay 5), margin 1, online beats | 399 | 57.6 | 64.9 | 60.7 | 18 | 43 | 15 | 3 | 0.60 | 86 |
| NEW: S, N=16, causal (delay 5), margin 1, ref beats | 399 | 79.6 | 99.4 | 80.2 | 62 | 4 | 6 | 1 | 0.36 | 100 |
| first study's weights, online beats, 169 test tracks where Traktor and madmom agree | 169 | 72.2 | 82.5 | 73.1 | 37 | 16 | 7 | 3 | 0.76 | 88 |
| NEW, Link beats, the same 169 agreeing test tracks | 169 | 76.8 | 89.4 | 78.4 | 49 | 5 | 5 | 3 | 0.61 | 89 |
| NEW, Link beats, reference = madmom offline (394 4/4 test tracks) | 394 | 51.3 | 54.2 | 50.5 | 24 | 8 | 21 | 3 | 0.63 | 86 |
| first study's weights, online beats, reference = madmom offline (same 394 tracks) | 394 | 49.4 | 49.3 | 48.6 | 17 | 26 | 16 | 4 | 0.80 | 88 |
| Traktor's own '1' scored against madmom offline on Link beats (same 394 tracks): see the agreement line above; mean agreement 56 % |
| NEW, Link beats, test tracks where the tracker's tempo = Traktor's (371) | 369 | 69.5 | 83.5 | 71.0 | 39 | 4 | 7 | 3 | 0.62 | 89 |

Splits, TEST, Link beats, pooled: by genre electro / breaks 64 % (68 tracks); house 76 % (46 tracks); other / untagged 71 % (31 tracks); pop / hyperpop 62 % (27 tracks); techno 71 % (227 tracks)
in a break 71 % (40851 beats); not in a break 70 % (193175 beats)

### Link's bar, TEST half, Link beats

| method | tracks | right (Link's bar as heard), mean % | median % | tracks >= 90 % | median s to first move | never moved % | first move right % | later moves/min | later moves wrong % |
|---|---|---|---|---|---|---|---|---|---|
| today: Link's arbitrary bar | - | 25.0 | 25.0 | 0 | - | 100 | 0 | - |
| NEW responsive: (1.0, 1.0, 8, 0) | 397 | 64.6 | 79.9 | 38 | 4 | 0 | 58 | 0.19 | 37 |
| NEW stable: (2.0, 1.0, 8, 0, 128, 2.0, 16) | 397 | 60.2 | 70.5 | 37 | 4 | 5 | 64 | 0.10 | 32 |

### Track change (simulated cut, same tempo), TEST half pairs, Link beats

- no reset (memory carries over): 199 cuts, re-lock median 36 s; within 15 s 24 %, 30 s 38 %, 60 s 53 %, never 17 %
- accumulators reset at the cut (a detected change): 199 cuts, re-lock median 8 s; within 15 s 57 %, 30 s 68 %, 60 s 71 %, never 15 %

Alternative with half the memory (S, N=8, causal (delay 5), margin 1.5, same training tracks), TEST:

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| S, N=8, causal (delay 5), margin 1.5, link beats | 397 | 66.1 | 77.6 | 69.9 | 37 | 4 | 14 | 2 | 0.51 | 86 |

### Track change (simulated cut, same tempo), TEST half pairs, Link beats

- no reset (memory carries over): 199 cuts, re-lock median 30 s; within 15 s 26 %, 30 s 41 %, 60 s 56 %, never 19 %
- accumulators reset at the cut (a detected change): 199 cuts, re-lock median 7 s; within 15 s 56 %, 30 s 65 %, 60 s 68 %, never 16 %

## GTZAN (human downbeats, 30 s clips), odd half (499)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study's weights, online beats | 484 | 42.8 | 27.5 | 46.9 | 24 | 5 | 58 | 1 | 2.84 | 79 |
| NEW (Traktor-trained), link beats | 420 | 45.9 | 30.0 | 50.8 | 28 | 5 | 58 | 2 | 5.50 | 79 |
| NEW (Traktor-trained), online beats | 484 | 38.3 | 25.0 | 41.9 | 22 | 6 | 64 | 1 | 3.43 | 79 |
| NEW structure trained on GTZAN even half (reference beats), link beats | 420 | 47.8 | 28.6 | 53.2 | 36 | 4 | 58 | 0 | 1.91 | 79 |
- NEW (Traktor-trained), Link beats, by genre: blues 43; classical 36; country 47; disco 59; hiphop 60; jazz 11; metal 48; pop 59; reggae 59; rock 44

88 weights. Run time 133 s.
