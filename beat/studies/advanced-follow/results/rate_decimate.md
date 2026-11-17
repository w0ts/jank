# Rate decimation (stage 2, 2026-11-15)

`rate_decimate.py --cases 60 --changes 0 0.02 0.05 --jobs 3` (seed 11, the user's tracks longer than 150 s, any split). Each case's
frames go from t0 - 20 s to t0 + 4 s into live/rate.py RateProbe per variant: the frame step (1 in n frames into the 0.1 s
blocks) and the estimate spacing (every n-th block). jank M2i's default is 2 / 2.

| change | frame step / estimate every | cases | found in 3 s | delay s med | abs ratio err med / p90 | false events / h |
|---|---|---|---|---|---|---|
| none | 1 / 1 | 60 | - | - | - | 274 |
| none | 2 / 1 | 60 | - | - | - | 266 |
| none | 5 / 1 | 60 | - | - | - | 297 |
| none | 2 / 2 | 60 | - | - | - | 197 |
| 0.02 | 1 / 1 | 60 | 0.90 | 0.55 | 0.11 % / 1.13 % | 274 |
| 0.02 | 2 / 1 | 60 | 0.90 | 0.53 | 0.11 % / 1.49 % | 266 |
| 0.02 | 5 / 1 | 60 | 0.87 | 0.56 | 0.23 % / 4.37 % | 297 |
| 0.02 | 2 / 2 | 60 | 0.85 | 0.67 | 0.08 % / 0.80 % | 197 |
| 0.05 | 1 / 1 | 60 | 0.87 | 0.54 | 0.11 % / 0.78 % | 274 |
| 0.05 | 2 / 1 | 60 | 0.90 | 0.54 | 0.13 % / 4.57 % | 266 |
| 0.05 | 5 / 1 | 60 | 0.87 | 0.56 | 0.19 % / 4.56 % | 297 |
| 0.05 | 2 / 2 | 60 | 0.83 | 0.67 | 0.10 % / 0.54 % | 197 |
