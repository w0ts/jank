## Traktor: 829 tracks (dev = even 415, test = odd 414); madmom offline measured on 16, agreeing (>= 90 %) on 4

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| first study: linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), dev half (recomputed from this cache) | 414 | 59.1 | 68.8 | 63.9 | 22 | 24 | 14 | 4 | 0.80 | 83 |
| first study: linear [all 22] exp N=16 hyst0.5 (weights: traktor dev), test half (recomputed from this cache) | 411 | 61.0 | 69.6 | 64.6 | 24 | 21 | 14 | 4 | 0.82 | 82 |

### Per-beat models, dev half, online beats (2-fold cross-fitted; filter tuned on dev)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| lin P=4 R=1, T=0.1 eps=0.005 margin=0.2 | 414 | 47.4 | 45.1 | 50.2 | 13 | 30 | 17 | 4 | 0.71 | 83 |  (7 s)
| mlp H=32 P=4 R=1, T=0.1 eps=0.002 margin=0.4 | 414 | 49.2 | 51.1 | 52.7 | 11 | 29 | 16 | 3 | 0.66 | 83 |  (8 s)
| mlp H=32 P=8 R=1, T=0.1 eps=0.002 margin=0.6 | 414 | 49.6 | 51.9 | 53.0 | 11 | 34 | 15 | 3 | 0.63 | 83 |  (9 s)
| mlp H=64 P=8 R=2, T=0.1 eps=0.002 margin=0.8 | 414 | 48.8 | 51.0 | 52.3 | 8 | 46 | 14 | 3 | 0.68 | 83 |  (10 s)
| gru H=24 P=0 R=1, T=0.1 eps=0.002 margin=0.8 | 414 | 48.9 | 47.0 | 53.5 | 14 | 19 | 19 | 1 | 0.32 | 83 |  (30 s)
| gru H=32 P=0 R=2, T=0.1 eps=0.002 margin=0.4 | 414 | 50.0 | 52.2 | 53.8 | 10 | 26 | 15 | 3 | 0.58 | 83 |  (30 s)

Best on dev: gru H=32 P=0 R=2, T=0.1 eps=0.002 margin=0.4

### Ablation, dev half, gru H=32 P=0 R=2 (cross-fitted, filter T=0.1 eps=0.002 margin=0.4)

| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | never locked % | flips median | flips/min | beats matched % |
|---|---|---|---|---|---|---|---|---|---|---|
| no chroma change (cc1, cc2) | 414 | 54.2 | 57.8 | 58.4 | 13 | 11 | 13 | 3 | 0.60 | 83 |
Terminated
