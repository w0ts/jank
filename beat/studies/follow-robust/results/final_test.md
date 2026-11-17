# Final: test half (414 tracks, untouched by tuning)

## test half

| chain | tracks | held whole | short stops | long stops | wrong tempo | never locked | median within 1 % | median stopped | median wrong | mean CMLc | median CMLc | wrong eps >= 2 s / track | of them after a stop | wrong eps >= 10 s | wrong locks / track | coasting (min) | coast right | coast wrong | coast wrong (min) | coast ends in/offbeat/off/wrong | real changes followed | median delay (s) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| fw | 414 | 104 (25 %) | 109 (26 %) | 112 (27 %) | 85 (21 %) | 4 (1 %) | 0.946 | 0.043 | 0.000 | 0.510 | 0.482 | 0.34 | 48 | 40 | 0.23 | 0 | - | - | 0.0 | - | 5/14 | 20.1 |
| agree-ltp-75 | 414 | 113 (27 %) | 151 (36 %) | 77 (19 %) | 69 (17 %) | 4 (1 %) | 0.960 | 0.036 | 0.000 | 0.517 | 0.482 | 0.29 | 59 | 28 | 0.17 | 0 | - | - | 0.0 | - | 7/14 | 20.5 |
| agree-ltpd-80 | 414 | 98 (24 %) | 160 (39 %) | 109 (26 %) | 43 (10 %) | 4 (1 %) | 0.950 | 0.048 | 0.000 | 0.491 | 0.450 | 0.19 | 48 | 13 | 0.13 | 0 | - | - | 0.0 | - | 6/14 | 21.0 |
| all-d-f60 | 414 | 157 (38 %) | 124 (30 %) | 49 (12 %) | 77 (19 %) | 7 (2 %) | 0.977 | 0.017 | 0.000 | 0.531 | 0.488 | 0.24 | 25 | 25 | 0.14 | 49 | 69 % | 13 % | 6.4 | 166/158/49/141 | 7/14 | 19.9 |
| all-d-f60-nocoast | 414 | 113 (27 %) | 156 (38 %) | 90 (22 %) | 48 (12 %) | 7 (2 %) | 0.956 | 0.041 | 0.000 | 0.491 | 0.450 | 0.19 | 45 | 4 | 0.12 | 0 | - | - | 0.0 | - | 7/14 | 16.5 |
| coast30 | 414 | 229 (55 %) | 2 (0 %) | 3 (1 %) | 176 (43 %) | 4 (1 %) | 0.990 | 0.000 | 0.001 | 0.536 | 0.489 | 0.78 | 8 | 112 | 1.11 | 132 | 49 % | 36 % | 48.0 | 308/205/67/292 | 8/14 | 17.7 |
| coast30-gate | 414 | 214 (52 %) | 31 (7 %) | 14 (3 %) | 151 (36 %) | 4 (1 %) | 0.985 | 0.000 | 0.000 | 0.535 | 0.489 | 0.62 | 31 | 83 | 0.57 | 94 | 61 % | 24 % | 22.4 | 273/204/68/235 | 8/14 | 17.7 |
| mem-w04-f10-l48 | 414 | 106 (26 %) | 120 (29 %) | 94 (23 %) | 90 (22 %) | 4 (1 %) | 0.946 | 0.043 | 0.000 | 0.510 | 0.481 | 0.33 | 37 | 39 | 0.26 | 0 | - | - | 0.0 | - | 5/14 | 20.1 |
| ratio-m05 | 414 | 106 (26 %) | 107 (26 %) | 112 (27 %) | 82 (20 %) | 7 (2 %) | 0.947 | 0.042 | 0.000 | 0.509 | 0.480 | 0.26 | 23 | 27 | 0.15 | 0 | - | - | 0.0 | - | 6/14 | 21.6 |

Tracks whose category moved against fw (held whole > short stops > long stops > wrong tempo / never locked):

- agree-ltp-75: 85 better, 19 worse
- agree-ltpd-80: 93 better, 27 worse
- all-d-f60: 123 better, 21 worse
- all-d-f60-nocoast: 109 better, 26 worse
- coast30: 125 better, 91 worse
- coast30-gate: 134 better, 66 worse
- mem-w04-f10-l48: 24 better, 16 worse
- ratio-m05: 10 better, 9 worse

