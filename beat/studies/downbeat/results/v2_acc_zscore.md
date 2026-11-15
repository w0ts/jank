## Traktor: 829 tracks (dev = even 415, test = odd 414); madmom offline measured on 106 (dev 55), Traktor and madmom agree (>= 90 %) on 35 (dev 17, test 18)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study: linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), online beats | 411 | 61.0 | 69.6 | 64.6 | 24 | 21 | 14 | 4 | 0.82 | 82 |
| first study: linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), ref beats | 414 | 64.6 | 83.9 | 63.8 | 44 | 4 | 14 | 2 | 0.50 | 100 |

### Accumulator models, dev half, online beats (2-fold cross-fitted, margin chosen on dev)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| acc [S] N=16 bars, linear, margin 0.5 | 414 | 52.6 | 56.8 | 56.2 | 14 | 28 | 14 | 4 | 0.81 | 83 |  (6 s)
| acc [S] N=4/16/64 bars, linear, margin 0.5 | 414 | 51.9 | 54.2 | 55.8 | 16 | 27 | 13 | 6 | 1.22 | 83 |  (5 s)
| acc [D] N=4/16/64 bars, linear, margin 1 | 414 | 50.7 | 52.6 | 54.2 | 11 | 42 | 13 | 4 | 0.88 | 83 |  (8 s)
| acc [S+D] N=16 bars, linear, margin 0.5 | 414 | 53.2 | 56.3 | 56.4 | 13 | 30 | 13 | 4 | 0.91 | 83 |  (5 s)
| acc [S+D] N=4/16/64 bars, linear, margin 1 | 414 | 53.3 | 58.4 | 56.7 | 14 | 37 | 13 | 4 | 0.82 | 83 |  (8 s)
| acc [S+D] N=8/32/128 bars, linear, margin 0.5 | 414 | 53.3 | 56.3 | 56.9 | 14 | 29 | 13 | 5 | 1.11 | 83 |  (8 s)
| acc [S+D] N=4/16/64 bars, MLP H=16, margin 2 | 414 | 48.3 | 50.7 | 51.1 | 10 | 31 | 14 | 5 | 0.97 | 83 |  (9 s)
| acc [S+D] N=4/16/64 bars, MLP H=32, margin 2 | 414 | 50.0 | 50.2 | 53.0 | 10 | 28 | 13 | 5 | 1.10 | 83 |  (9 s)

Best on dev: acc [S+D] N=8/32/128 bars, linear, margin 0.5

