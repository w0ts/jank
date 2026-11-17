# Skip test

Skips counted only where the chain was locked, right in tempo and phase, before the skip. Beats = of the true period.

| case | config | skips | phase re-locked within 50 s | median phase re-lock (beats) | 90th pct (beats) | median wrong beats sent (30 s) | skips gated | median gate (s) | never gated | tracker alone: median re-sync (beats) |
|---|---|---|---|---|---|---|---|---|---|---|
| gap | all-d-f60 | 38 | 36 | 30.4 | 55 | 1.2 | 38 | 0.5 | 0 | 10.1 (36/38) |
| gap | all-skip4 | 36 | 34 | 30.4 | 49 | 1.2 | 36 | 0.5 | 0 | 10.1 (34/36) |
| gap | fw-dbn | 36 | 34 | 30.1 | 47 | 1.2 | 36 | 0.5 | 0 | 10.1 (34/36) |
| gap | fw-skip3 | 35 | 33 | 30.0 | 46 | 1.2 | 35 | 0.5 | 0 | 10.1 (33/35) |
| gap | fw-skip4 | 35 | 33 | 30.0 | 46 | 1.2 | 35 | 0.5 | 0 | 10.1 (33/35) |
| gap | fw-skip5 | 35 | 33 | 30.0 | 46 | 1.2 | 35 | 0.5 | 0 | 10.1 (33/35) |
| jump | all-d-f60 | 76 | 73 | 5.8 | 28 | 8.5 | 31 | 7.0 | 45 | 1.8 (72/76) |
| jump | all-skip4 | 72 | 69 | 5.8 | 25 | 3.7 | 49 | 2.0 | 23 | 1.7 (68/72) |
| jump | fw-dbn | 72 | 69 | 5.5 | 30 | 5.3 | 36 | 3.4 | 36 | 1.7 (68/72) |
| jump | fw-skip3 | 70 | 66 | 12.7 | 26 | 3.4 | 48 | 1.8 | 22 | 1.7 (66/70) |
| jump | fw-skip4 | 70 | 66 | 5.9 | 27 | 3.6 | 46 | 2.0 | 24 | 1.7 (66/70) |
| jump | fw-skip5 | 70 | 66 | 5.9 | 28 | 3.5 | 46 | 2.0 | 24 | 1.7 (66/70) |
| nudge | all-d-f60 | 38 | 37 | 4.5 | 22 | 5.6 | 14 | 6.9 | 24 | 0.9 (38/38) |
| nudge | all-skip4 | 36 | 35 | 4.5 | 18 | 3.9 | 24 | 2.0 | 12 | 0.9 (36/36) |
| nudge | fw-dbn | 36 | 36 | 4.4 | 26 | 4.8 | 15 | 2.9 | 21 | 0.9 (36/36) |
| nudge | fw-skip3 | 35 | 35 | 10.3 | 24 | 3.8 | 23 | 2.0 | 12 | 0.9 (35/35) |
| nudge | fw-skip4 | 35 | 35 | 5.0 | 24 | 3.9 | 22 | 1.9 | 13 | 0.9 (35/35) |
| nudge | fw-skip5 | 35 | 35 | 5.0 | 23 | 4.0 | 22 | 1.9 | 13 | 0.9 (35/35) |
| other | all-d-f60 | 38 | 35 | 16.2 | 50 | 10.2 | 27 | 4.0 | 11 | 1.1 (37/38) |
| other | all-skip4 | 36 | 32 | 10.1 | 23 | 5.5 | 35 | 1.8 | 1 | 1.1 (35/36) |
| other | fw-dbn | 36 | 33 | 17.8 | 34 | 8.7 | 28 | 2.9 | 8 | 1.1 (35/36) |
| other | fw-skip3 | 35 | 32 | 17.1 | 24 | 4.7 | 35 | 1.5 | 0 | 1.3 (34/35) |
| other | fw-skip4 | 35 | 33 | 17.2 | 24 | 5.8 | 35 | 1.6 | 0 | 1.3 (34/35) |
| other | fw-skip5 | 35 | 32 | 16.8 | 24 | 5.8 | 34 | 1.7 | 1 | 1.3 (34/35) |
