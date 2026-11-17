Columns: time in correct phase = share of scored beats (tracked beats matched to a reference beat within 70 ms, inside 4-beat bars, after the first 2 bars) whose bar position the method gets right; mean and median over tracks, pooled over all beats; tracks >= 90 %; median seconds from the first tracked beat to the first run of 8 right beats; never locked; flips = changes of the chosen '1' after the first 2 bars.

## traktor: 829 tracks (dev = even, 415; test = odd, 414)

Reference bars: 148484, not 4 beats long: 0 (0.0 %), in 0 tracks; tracks with most bars not 4 beats: 0 (their beats are skipped where the bar is not 4)

### Single-feature rules, dev half, online beats: best 10 of 216 (all: grid_traktor.md)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| nov_low exp N=16 | 414 | 51.3 | 54.2 | 55.3 | 11 | 47 | 15 | 18 | 4.74 | 83 |
| nov_low exp N=8 | 414 | 49.6 | 51.7 | 52.9 | 8 | 38 | 13 | 31 | 7.13 | 83 |
| nov_low window N=16 | 414 | 46.9 | 47.1 | 49.9 | 6 | 40 | 13 | 21 | 5.04 | 83 |
| flux150+nov_all exp N=16 | 414 | 46.6 | 42.9 | 48.7 | 12 | 28 | 17 | 18 | 4.26 | 83 |
| nov_all exp N=16 | 414 | 46.3 | 46.5 | 49.3 | 10 | 40 | 17 | 24 | 5.26 | 83 |
| flux150+nov_all exp N=8 | 414 | 46.1 | 43.1 | 48.2 | 11 | 28 | 16 | 30 | 6.71 | 83 |
| e150+nov_all exp N=16 | 414 | 45.9 | 46.2 | 48.4 | 10 | 29 | 16 | 20 | 4.53 | 83 |
| nov_low exp N=4 | 414 | 45.9 | 44.6 | 48.4 | 6 | 33 | 12 | 52 | 11.17 | 83 |
| e150+nov_all exp N=8 | 414 | 45.9 | 45.2 | 47.8 | 9 | 29 | 15 | 31 | 6.81 | 83 |
| flux150+nov_all window N=16 | 414 | 45.7 | 42.9 | 47.9 | 11 | 31 | 16 | 21 | 4.84 | 83 |

### Learned linear rules (weights fitted on the dev half's online beats), dev half

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| linear [all 22] window N=4 (weights: traktor dev) | 414 | 49.1 | 48.1 | 51.4 | 9 | 21 | 11 | 62 | 13.53 | 83 |
| linear [all 22] exp N=4 (weights: traktor dev) | 414 | 54.2 | 57.8 | 57.5 | 13 | 24 | 12 | 52 | 11.43 | 83 |
| linear [all 22] window N=8 (weights: traktor dev) | 414 | 52.5 | 54.8 | 55.5 | 11 | 23 | 12 | 41 | 8.88 | 83 |
| linear [all 22] exp N=8 (weights: traktor dev) | 414 | 57.2 | 64.8 | 61.1 | 18 | 24 | 12 | 35 | 7.79 | 83 |
| linear [all 22] window N=16 (weights: traktor dev) | 414 | 55.6 | 62.9 | 59.4 | 14 | 21 | 11 | 26 | 5.99 | 83 |
| linear [all 22] exp N=16 (weights: traktor dev) | 414 | 58.7 | 68.6 | 63.2 | 20 | 23 | 12 | 21 | 5.29 | 83 |
| linear [6 band fluxes + nov_all + nov_bar_all] window N=4 (weights: traktor dev) | 414 | 47.5 | 46.4 | 49.6 | 6 | 20 | 11 | 69 | 14.57 | 83 |
| linear [6 band fluxes + nov_all + nov_bar_all] exp N=4 (weights: traktor dev) | 414 | 50.5 | 52.7 | 53.3 | 10 | 26 | 13 | 62 | 13.20 | 83 |
| linear [6 band fluxes + nov_all + nov_bar_all] window N=8 (weights: traktor dev) | 414 | 50.2 | 51.8 | 52.9 | 9 | 24 | 12 | 44 | 9.64 | 83 |
| linear [6 band fluxes + nov_all + nov_bar_all] exp N=8 (weights: traktor dev) | 414 | 52.9 | 55.8 | 55.9 | 15 | 27 | 13 | 42 | 9.19 | 83 |
| linear [6 band fluxes + nov_all + nov_bar_all] window N=16 (weights: traktor dev) | 414 | 52.5 | 56.1 | 55.6 | 13 | 26 | 13 | 27 | 6.47 | 83 |
| linear [6 band fluxes + nov_all + nov_bar_all] exp N=16 (weights: traktor dev) | 414 | 54.0 | 60.4 | 57.3 | 16 | 29 | 13 | 28 | 6.57 | 83 |
| linear [e150 + flux_g3 + nov_all] window N=4 (weights: traktor dev) | 414 | 44.9 | 43.3 | 46.5 | 6 | 23 | 11 | 61 | 13.05 | 83 |
| linear [e150 + flux_g3 + nov_all] exp N=4 (weights: traktor dev) | 414 | 47.1 | 47.0 | 49.3 | 9 | 27 | 12 | 56 | 11.72 | 83 |
| linear [e150 + flux_g3 + nov_all] window N=8 (weights: traktor dev) | 414 | 47.0 | 47.7 | 49.1 | 8 | 25 | 12 | 38 | 8.34 | 83 |
| linear [e150 + flux_g3 + nov_all] exp N=8 (weights: traktor dev) | 414 | 48.5 | 50.5 | 51.1 | 12 | 28 | 14 | 36 | 7.84 | 83 |
| linear [e150 + flux_g3 + nov_all] window N=16 (weights: traktor dev) | 414 | 48.2 | 50.0 | 50.8 | 10 | 29 | 13 | 23 | 5.44 | 83 |
| linear [e150 + flux_g3 + nov_all] exp N=16 (weights: traktor dev) | 414 | 48.8 | 51.3 | 51.7 | 12 | 30 | 16 | 26 | 5.74 | 83 |

### Hysteresis, dev half, online beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| nov_low exp N=16 hyst2% | 414 | 51.4 | 53.6 | 55.5 | 12 | 44 | 15 | 11 | 2.65 | 83 |
| nov_low exp N=16 hyst5% | 414 | 51.4 | 53.9 | 55.6 | 12 | 46 | 15 | 8 | 1.68 | 83 |
| nov_low exp N=16 hyst10% | 414 | 51.5 | 54.8 | 55.7 | 11 | 48 | 16 | 5 | 1.15 | 83 |
| nov_low exp N=16 hyst20% | 414 | 50.3 | 53.3 | 54.6 | 11 | 53 | 18 | 3 | 0.72 | 83 |
| linear [all 22] exp N=16 hyst0.25 (weights: traktor dev) | 414 | 59.2 | 68.4 | 63.8 | 22 | 23 | 13 | 6 | 1.35 | 83 |
| linear [all 22] exp N=16 hyst0.5 (weights: traktor dev) | 414 | 59.1 | 68.8 | 63.9 | 22 | 24 | 14 | 4 | 0.80 | 83 |
| linear [all 22] exp N=16 hyst1 (weights: traktor dev) | 414 | 56.5 | 64.1 | 61.4 | 20 | 28 | 18 | 2 | 0.40 | 83 |
| linear [all 22] exp N=16 hyst2 (weights: traktor dev) | 414 | 50.3 | 54.0 | 54.8 | 18 | 41 | 24 | 1 | 0.18 | 83 |

### TEST half

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| nov_low exp N=16, online beats | 411 | 54.1 | 58.3 | 58.5 | 13 | 42 | 13 | 19 | 4.56 | 82 |
| nov_low exp N=16 hyst20%, online beats | 411 | 53.1 | 56.5 | 57.9 | 16 | 43 | 17 | 4 | 0.76 | 82 |
| linear [all 22] exp N=16 (weights: traktor dev), online beats | 411 | 60.6 | 67.4 | 64.3 | 24 | 20 | 14 | 21 | 5.23 | 82 |
| linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), online beats | 411 | 61.0 | 69.6 | 64.6 | 24 | 21 | 14 | 4 | 0.81 | 82 |
| Link's arbitrary bar (today; mean of the 4 offsets), online beats | 411 | 24.8 | 25.0 | 25.0 | 0 | 51 | 7 | 0 | 0.00 | 82 |
| first tracked beat = 1, online beats | 411 | 15.3 | 0.3 | 13.2 | 5 | 110 | 72 | 0 | 0.00 | 82 |
| nov_low exp N=16, ref beats | 414 | 67.8 | 79.7 | 68.6 | 40 | 4 | 4 | 13 | 3.62 | 100 |
| nov_low exp N=16 hyst20%, ref beats | 414 | 71.7 | 90.7 | 73.1 | 51 | 4 | 5 | 2 | 0.51 | 100 |
| linear [all 22] exp N=16 (weights: traktor dev), ref beats | 414 | 68.9 | 83.2 | 68.5 | 42 | 4 | 5 | 14 | 4.13 | 100 |
| linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), ref beats | 414 | 71.7 | 88.4 | 71.4 | 48 | 4 | 5 | 2 | 0.55 | 100 |
| Link's arbitrary bar (today; mean of the 4 offsets), ref beats | 414 | 25.0 | 25.0 | 25.0 | 0 | 4 | 0 | 0 | 0.00 | 100 |
| first tracked beat = 1, ref beats | 414 | 81.2 | 100.0 | 83.1 | 81 | 4 | 19 | 0 | 0.00 | 100 |

### Splits, TEST half, pooled time in correct phase (beats scored)

- linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), online beats, by genre: electro / breaks 61 % (75 tracks); house 75 % (46 tracks); other / untagged 73 % (32 tracks); pop / hyperpop 55 % (28 tracks); techno 63 % (233 tracks)
- linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), online beats: in a break 66 % (37149 beats); not in a break 64 % (196787 beats)
- linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), ref beats, by genre: electro / breaks 67 % (75 tracks); house 79 % (46 tracks); other / untagged 78 % (32 tracks); pop / hyperpop 77 % (28 tracks); techno 70 % (233 tracks)
- linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), ref beats: in a break 70 % (54827 beats); not in a break 72 % (239828 beats)
- nov_low exp N=16 hyst20%, online beats, by genre: electro / breaks 50 % (75 tracks); house 62 % (46 tracks); other / untagged 67 % (32 tracks); pop / hyperpop 48 % (28 tracks); techno 58 % (233 tracks)
- nov_low exp N=16 hyst20%, online beats: in a break 61 % (37149 beats); not in a break 57 % (196787 beats)
- nov_low exp N=16 hyst20%, ref beats, by genre: electro / breaks 60 % (75 tracks); house 68 % (46 tracks); other / untagged 75 % (32 tracks); pop / hyperpop 72 % (28 tracks); techno 77 % (233 tracks)
- nov_low exp N=16 hyst20%, ref beats: in a break 70 % (54827 beats); not in a break 74 % (239828 beats)

### Downbeat models, ALL tracks, same scoring on their own beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| BeatNet online PF (causal; runs/ of 2026-11-13) | 827 | 30.3 | 27.6 | 31.8 | 0 | 62 | 51 | 77 | 16.06 | 64 |
| madmom RNNDownBeat + DBN offline (NOT causal), 208 tracks | 206 | 54.2 | 55.5 | 56.3 | 43 | 4 | 38 | 0 | 0.01 | 88 |
| linear [all 22] exp N=16 hyst0.5 (weights: traktor dev) on the same 208 tracks (dev+test), online beats | 208 | 61.1 | 70.3 | 65.9 | 23 | 25 | 12 | 4 | 0.80 | 83 |
| nov_low exp N=16 hyst20% on the same 208 tracks (dev+test), online beats | 208 | 50.6 | 53.9 | 54.4 | 12 | 53 | 18 | 3 | 0.68 | 83 |

madmom offline on these Traktor tracks: 0 of 37237 bars are 3 beats long.

## gtzan: 998 tracks (dev = even, 499; test = odd, 499)

Reference bars: 14150, not 4 beats long: 1144 (8.1 %), in 72 tracks; tracks with most bars not 4 beats: 63 (their beats are skipped where the bar is not 4)

### Single-feature rules, dev half, online beats: best 10 of 216 (all: grid_gtzan.md)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| e150+nov_all exp N=16 | 478 | 49.4 | 39.4 | 53.0 | 26 | 5 | 49 | 2 | 7.51 | 80 |
| e150+nov_all exp N=8 | 478 | 49.0 | 40.0 | 52.8 | 24 | 6 | 47 | 3 | 8.77 | 80 |
| e150+nov_all window N=16 | 478 | 48.6 | 34.9 | 52.6 | 27 | 5 | 52 | 2 | 5.94 | 80 |
| e150+nov_all exp N=4 | 478 | 48.4 | 41.4 | 52.4 | 21 | 6 | 47 | 4 | 11.29 | 80 |
| e150+nov_all window N=8 | 478 | 48.4 | 38.1 | 52.7 | 24 | 5 | 48 | 2 | 7.46 | 80 |
| e150+nov_all exp N=2 | 478 | 47.4 | 40.0 | 51.4 | 17 | 6 | 46 | 6 | 15.53 | 80 |
| e150+nov_all window N=4 | 478 | 46.9 | 41.4 | 51.2 | 16 | 5 | 44 | 4 | 11.52 | 80 |
| flux150+nov_all window N=16 | 478 | 46.4 | 31.5 | 50.2 | 23 | 5 | 51 | 2 | 6.79 | 80 |
| flux150+nov_all exp N=16 | 478 | 46.4 | 33.3 | 50.2 | 21 | 5 | 50 | 3 | 8.05 | 80 |
| flux150+nov_all window N=8 | 478 | 46.0 | 34.2 | 49.7 | 20 | 5 | 48 | 3 | 8.28 | 80 |

### Learned linear rules (weights fitted on the dev half's online beats), dev half

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| linear [all 22] window N=4 (weights: gtzan dev) | 478 | 51.7 | 45.3 | 57.6 | 23 | 5 | 41 | 5 | 13.21 | 80 |
| linear [all 22] exp N=4 (weights: gtzan dev) | 478 | 52.3 | 44.2 | 58.7 | 27 | 5 | 45 | 5 | 13.39 | 80 |
| linear [all 22] window N=8 (weights: gtzan dev) | 478 | 53.0 | 47.0 | 59.8 | 29 | 5 | 43 | 3 | 8.93 | 80 |
| linear [all 22] exp N=8 (weights: gtzan dev) | 478 | 52.6 | 44.4 | 59.3 | 28 | 5 | 46 | 4 | 11.10 | 80 |
| linear [all 22] window N=16 (weights: gtzan dev) | 478 | 53.2 | 45.3 | 60.3 | 34 | 5 | 46 | 2 | 7.15 | 80 |
| linear [all 22] exp N=16 (weights: gtzan dev) | 478 | 52.6 | 42.0 | 59.5 | 30 | 5 | 48 | 3 | 10.34 | 80 |
| linear [6 band fluxes + nov_all + nov_bar_all] window N=4 (weights: gtzan dev) | 478 | 50.1 | 42.1 | 55.9 | 18 | 5 | 45 | 5 | 13.70 | 80 |
| linear [6 band fluxes + nov_all + nov_bar_all] exp N=4 (weights: gtzan dev) | 478 | 50.6 | 41.4 | 56.5 | 23 | 7 | 48 | 5 | 15.28 | 80 |
| linear [6 band fluxes + nov_all + nov_bar_all] window N=8 (weights: gtzan dev) | 478 | 51.2 | 40.8 | 58.3 | 27 | 5 | 46 | 3 | 8.79 | 80 |
| linear [6 band fluxes + nov_all + nov_bar_all] exp N=8 (weights: gtzan dev) | 478 | 50.1 | 39.8 | 56.6 | 24 | 7 | 49 | 5 | 14.39 | 80 |
| linear [6 band fluxes + nov_all + nov_bar_all] window N=16 (weights: gtzan dev) | 478 | 51.6 | 38.3 | 58.9 | 31 | 5 | 49 | 2 | 7.29 | 80 |
| linear [6 band fluxes + nov_all + nov_bar_all] exp N=16 (weights: gtzan dev) | 478 | 49.7 | 37.3 | 56.1 | 25 | 7 | 51 | 5 | 15.05 | 80 |
| linear [e150 + flux_g3 + nov_all] window N=4 (weights: gtzan dev) | 478 | 47.0 | 39.2 | 52.2 | 15 | 5 | 45 | 5 | 13.55 | 80 |
| linear [e150 + flux_g3 + nov_all] exp N=4 (weights: gtzan dev) | 478 | 46.1 | 35.0 | 52.0 | 18 | 8 | 50 | 7 | 17.15 | 80 |
| linear [e150 + flux_g3 + nov_all] window N=8 (weights: gtzan dev) | 478 | 48.3 | 36.2 | 54.0 | 23 | 5 | 48 | 3 | 9.11 | 80 |
| linear [e150 + flux_g3 + nov_all] exp N=8 (weights: gtzan dev) | 478 | 46.2 | 32.6 | 51.9 | 19 | 8 | 53 | 7 | 17.97 | 80 |
| linear [e150 + flux_g3 + nov_all] window N=16 (weights: gtzan dev) | 478 | 47.5 | 32.8 | 53.2 | 26 | 5 | 52 | 2 | 7.49 | 80 |
| linear [e150 + flux_g3 + nov_all] exp N=16 (weights: gtzan dev) | 478 | 45.2 | 31.2 | 51.0 | 19 | 8 | 57 | 7 | 19.43 | 80 |

### Hysteresis, dev half, online beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| e150+nov_all exp N=16 hyst2% | 478 | 49.5 | 38.7 | 53.1 | 26 | 5 | 49 | 2 | 7.01 | 80 |
| e150+nov_all exp N=16 hyst5% | 478 | 49.4 | 38.5 | 53.0 | 26 | 5 | 49 | 2 | 6.42 | 80 |
| e150+nov_all exp N=16 hyst10% | 478 | 49.0 | 38.1 | 52.9 | 26 | 5 | 48 | 2 | 5.73 | 80 |
| e150+nov_all exp N=16 hyst20% | 478 | 48.8 | 38.5 | 52.5 | 26 | 5 | 49 | 1 | 4.51 | 80 |
| linear [all 22] window N=16 hyst0.25 (weights: gtzan dev) | 478 | 53.2 | 42.6 | 59.9 | 35 | 5 | 47 | 1 | 2.28 | 80 |
| linear [all 22] window N=16 hyst0.5 (weights: gtzan dev) | 478 | 51.5 | 38.3 | 58.0 | 34 | 5 | 49 | 0 | 1.44 | 80 |
| linear [all 22] window N=16 hyst1 (weights: gtzan dev) | 478 | 47.3 | 26.9 | 52.6 | 34 | 4 | 57 | 0 | 0.72 | 80 |
| linear [all 22] window N=16 hyst2 (weights: gtzan dev) | 478 | 41.1 | 24.5 | 45.0 | 32 | 4 | 66 | 0 | 0.26 | 80 |

### TEST half

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| e150+nov_all exp N=16, online beats | 484 | 47.9 | 33.6 | 53.0 | 24 | 6 | 53 | 2 | 7.55 | 79 |
| e150+nov_all exp N=16 hyst20%, online beats | 484 | 47.7 | 33.3 | 52.7 | 25 | 5 | 53 | 1 | 4.86 | 79 |
| linear [all 22] window N=16 (weights: gtzan dev), online beats | 484 | 50.0 | 38.2 | 56.4 | 30 | 5 | 52 | 2 | 7.76 | 79 |
| linear [all 22] window N=16 hyst0.25 (weights: gtzan dev), online beats | 484 | 49.3 | 33.3 | 55.2 | 31 | 5 | 53 | 1 | 2.37 | 79 |
| linear [all 22] exp N=16 hyst0.5 (weights: Traktor dev), online beats | 484 | 42.8 | 27.5 | 46.9 | 24 | 5 | 58 | 1 | 2.83 | 79 |
| Link's arbitrary bar (today; mean of the 4 offsets), online beats | 484 | 24.1 | 25.0 | 24.9 | 0 | 4 | 33 | 0 | 0.00 | 79 |
| first tracked beat = 1, online beats | 484 | 24.0 | 0.0 | 22.9 | 15 | 5 | 82 | 0 | 0.00 | 79 |
| e150+nov_all exp N=16, ref beats | 497 | 59.8 | 74.4 | 60.0 | 39 | 5 | 31 | 2 | 7.12 | 94 |
| e150+nov_all exp N=16 hyst20%, ref beats | 497 | 59.6 | 75.0 | 59.7 | 41 | 5 | 31 | 1 | 4.05 | 94 |
| linear [all 22] window N=16 (weights: gtzan dev), ref beats | 497 | 59.9 | 77.8 | 60.7 | 44 | 5 | 32 | 1 | 7.23 | 94 |
| linear [all 22] window N=16 hyst0.25 (weights: gtzan dev), ref beats | 497 | 58.0 | 77.1 | 59.2 | 45 | 5 | 33 | 0 | 1.70 | 94 |
| linear [all 22] exp N=16 hyst0.5 (weights: Traktor dev), ref beats | 497 | 49.7 | 52.7 | 50.1 | 36 | 5 | 43 | 1 | 2.24 | 94 |
| Link's arbitrary bar (today; mean of the 4 offsets), ref beats | 497 | 24.2 | 25.0 | 25.0 | 0 | 4 | 6 | 0 | 0.00 | 94 |
| first tracked beat = 1, ref beats | 497 | 23.0 | 0.0 | 23.0 | 23 | 4 | 78 | 0 | 0.00 | 94 |

### Splits, TEST half, pooled time in correct phase (beats scored)

- linear [all 22] window N=16 hyst0.25 (weights: gtzan dev), online beats, by genre: blues 43 % (50 tracks); classical 38 % (50 tracks); country 52 % (50 tracks); disco 68 % (50 tracks); hiphop 67 % (50 tracks); jazz 25 % (49 tracks); metal 58 % (50 tracks); pop 65 % (50 tracks); reggae 62 % (50 tracks); rock 47 % (50 tracks)
- linear [all 22] window N=16 hyst0.25 (weights: gtzan dev), ref beats, by genre: blues 43 % (50 tracks); classical 35 % (50 tracks); country 68 % (50 tracks); disco 67 % (50 tracks); hiphop 74 % (50 tracks); jazz 34 % (49 tracks); metal 64 % (50 tracks); pop 79 % (50 tracks); reggae 68 % (50 tracks); rock 56 % (50 tracks)
- e150+nov_all exp N=16 hyst20%, online beats, by genre: blues 42 % (50 tracks); classical 41 % (50 tracks); country 43 % (50 tracks); disco 52 % (50 tracks); hiphop 68 % (50 tracks); jazz 26 % (49 tracks); metal 57 % (50 tracks); pop 69 % (50 tracks); reggae 48 % (50 tracks); rock 63 % (50 tracks)
- e150+nov_all exp N=16 hyst20%, ref beats, by genre: blues 46 % (50 tracks); classical 62 % (50 tracks); country 63 % (50 tracks); disco 52 % (50 tracks); hiphop 77 % (50 tracks); jazz 44 % (49 tracks); metal 64 % (50 tracks); pop 83 % (50 tracks); reggae 50 % (50 tracks); rock 67 % (50 tracks)

### Downbeat models, ALL tracks, same scoring on their own beats

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| BeatNet online PF (causal) | 952 | 51.9 | 45.8 | 56.1 | 25 | 6 | 51 | 4 | 12.66 | 78 |
| madmom RNNDownBeat + DBN offline (NOT causal) | 971 | 64.7 | 100.0 | 67.1 | 58 | 4 | 43 | 0 | 2.03 | 82 |


## Traktor's '1' against madmom offline downbeats (208 tracks)

Tracks compared: 203; madmom chose 3/4 for 0. Share of matched beats where madmom's bar position = Traktor's: mean 55 %, median 57 %; tracks agreeing >= 90 %: 43 %. Most common offset (madmom - Traktor, beats) per track: 0: 115, 1: 36, 2: 38, 3: 14

Tracks where Traktor and madmom agree on >= 90 % of beats (the '1' is probably right): 88

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| linear [all 22] exp N=16 hyst0.5 (weights: Traktor test half), online beats, agreeing tracks | 88 | 69.6 | 76.7 | 71.7 | 30 | 15 | 5 | 4 | 0.77 | 90 |
| linear [all 22] exp N=16 hyst0.5 (weights: Traktor test half), ref beats, agreeing tracks | 88 | 81.3 | 94.1 | 81.8 | 60 | 4 | 5 | 2 | 0.47 | 100 |
| nov_low exp N=16 hyst20%, online beats, agreeing tracks | 88 | 60.2 | 70.8 | 62.0 | 18 | 47 | 8 | 3 | 0.66 | 90 |
| nov_low exp N=16 hyst20%, ref beats, agreeing tracks | 88 | 78.1 | 99.7 | 78.3 | 65 | 4 | 5 | 1 | 0.36 | 100 |
| first tracked beat = 1, online beats, agreeing tracks | 88 | 14.1 | 0.0 | 13.7 | 6 | 16 | 74 | 0 | 0.00 | 90 |
| Link's arbitrary bar, online beats, agreeing tracks | 88 | 25.0 | 25.0 | 25.0 | 0 | 39 | 1 | 0 | 0.00 | 90 |

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), online beats, reference = Traktor, 208 tracks | 208 | 61.1 | 70.3 | 65.9 | 23 | 25 | 12 | 4 | 0.80 | 83 |
| nov_low exp N=16 hyst20%, online beats, reference = Traktor, 208 tracks | 208 | 50.6 | 53.9 | 54.4 | 12 | 53 | 18 | 3 | 0.68 | 83 |
| linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), online beats, reference = madmom offline, 208 tracks | 208 | 46.9 | 42.7 | 47.0 | 16 | 31 | 17 | 4 | 0.80 | 88 |
| nov_low exp N=16 hyst20%, online beats, reference = madmom offline, 208 tracks | 208 | 41.1 | 34.1 | 41.1 | 10 | 53 | 22 | 3 | 0.68 | 88 |
