# Skip test

Skips counted only where the chain was locked, right in tempo and phase, before the skip. Beats = of the true period.

| case | config | skips | phase re-locked within 50 s | median phase re-lock (beats) | 90th pct (beats) | median wrong beats sent (30 s) | skips gated | median gate (s) | never gated | tracker alone: median re-sync (beats) |
|---|---|---|---|---|---|---|---|---|---|---|
| gap | all-d-f60 | 38 | 36 | 30.4 | 55 | 1.2 | 38 | 0.5 | 0 | 10.1 (36/38) |
| gap | all-rsonly | 35 | 33 | 30.2 | 46 | 1.2 | 35 | 0.5 | 0 | 10.1 (33/35) |
| gap | all-rsonly-s1 | 35 | 33 | 30.2 | 46 | 1.2 | 35 | 0.5 | 0 | 10.1 (33/35) |
| gap | fw-dbn | 36 | 34 | 30.1 | 47 | 1.2 | 36 | 0.5 | 0 | 10.1 (34/36) |
| gap | fw-rs-k8 | 35 | 33 | 30.0 | 46 | 1.2 | 35 | 0.5 | 0 | 10.1 (33/35) |
| gap | fw-rsonly | 35 | 33 | 30.0 | 46 | 1.2 | 35 | 0.5 | 0 | 10.1 (33/35) |
| gap | fw-rsonly-s1 | 35 | 33 | 30.0 | 46 | 1.2 | 35 | 0.5 | 0 | 10.1 (33/35) |
| jump | all-d-f60 | 76 | 73 | 5.8 | 28 | 8.5 | 31 | 7.0 | 45 | 1.8 (72/76) |
| jump | all-rsonly | 70 | 67 | 4.6 | 19 | 4.0 | 36 | 2.0 | 34 | 1.7 (66/70) |
| jump | all-rsonly-s1 | 70 | 67 | 4.8 | 19 | 3.1 | 51 | 1.3 | 19 | 1.7 (66/70) |
| jump | fw-dbn | 72 | 69 | 5.5 | 30 | 5.3 | 36 | 3.4 | 36 | 1.7 (68/72) |
| jump | fw-rs-k8 | 70 | 67 | 4.8 | 21 | 4.0 | 47 | 2.0 | 23 | 1.7 (66/70) |
| jump | fw-rsonly | 70 | 67 | 4.8 | 21 | 4.0 | 47 | 2.0 | 23 | 1.7 (66/70) |
| jump | fw-rsonly-s1 | 70 | 67 | 4.8 | 21 | 3.2 | 51 | 1.3 | 19 | 1.7 (66/70) |
| nudge | all-d-f60 | 38 | 37 | 4.5 | 22 | 5.6 | 14 | 6.9 | 24 | 0.9 (38/38) |
| nudge | all-rsonly | 35 | 35 | 4.0 | 17 | 3.8 | 18 | 6.0 | 17 | 0.9 (35/35) |
| nudge | all-rsonly-s1 | 35 | 35 | 4.5 | 17 | 2.9 | 25 | 1.5 | 10 | 0.9 (35/35) |
| nudge | fw-dbn | 36 | 36 | 4.4 | 26 | 4.8 | 15 | 2.9 | 21 | 0.9 (36/36) |
| nudge | fw-rs-k8 | 35 | 35 | 4.7 | 24 | 3.9 | 22 | 2.0 | 13 | 0.9 (35/35) |
| nudge | fw-rsonly | 35 | 35 | 4.7 | 24 | 3.9 | 22 | 2.0 | 13 | 0.9 (35/35) |
| nudge | fw-rsonly-s1 | 35 | 35 | 4.8 | 24 | 2.9 | 25 | 1.5 | 10 | 0.9 (35/35) |
| other | all-d-f60 | 38 | 35 | 16.2 | 50 | 10.2 | 27 | 4.0 | 11 | 1.1 (37/38) |
| other | all-rsonly | 35 | 31 | 16.2 | 41 | 5.7 | 33 | 1.7 | 2 | 1.3 (34/35) |
| other | all-rsonly-s1 | 35 | 31 | 16.1 | 37 | 3.2 | 35 | 1.1 | 0 | 1.3 (34/35) |
| other | fw-dbn | 36 | 33 | 17.8 | 34 | 8.7 | 28 | 2.9 | 8 | 1.1 (35/36) |
| other | fw-rs-k8 | 35 | 33 | 12.1 | 22 | 7.2 | 33 | 1.7 | 2 | 1.3 (34/35) |
| other | fw-rsonly | 35 | 32 | 17.9 | 31 | 6.2 | 33 | 1.7 | 2 | 1.3 (34/35) |
| other | fw-rsonly-s1 | 35 | 32 | 17.9 | 31 | 4.9 | 35 | 1.1 | 0 | 1.3 (34/35) |
