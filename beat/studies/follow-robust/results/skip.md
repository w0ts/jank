# Skip test

Skips counted only where the chain was locked, right in tempo and phase, before the skip. Beats = of the true period.

| case | config | skips | phase re-locked within 50 s | median phase re-lock (beats) | 90th pct (beats) | median wrong beats sent (30 s) | skips gated | median gate (s) | never gated | tracker alone: median re-sync (beats) |
|---|---|---|---|---|---|---|---|---|---|---|
| gap | all-d-f60 | 38 | 36 | 30.4 | 55 | 1.2 | 38 | 0.5 | 0 | 10.1 (36/38) |
| gap | fw-dbn | 36 | 34 | 30.1 | 47 | 1.2 | 36 | 0.5 | 0 | 10.1 (34/36) |
| jump | all-d-f60 | 76 | 73 | 5.8 | 28 | 8.5 | 31 | 7.0 | 45 | 1.8 (72/76) |
| jump | fw-dbn | 72 | 69 | 5.5 | 30 | 5.3 | 36 | 3.4 | 36 | 1.7 (68/72) |
| nudge | all-d-f60 | 38 | 37 | 4.5 | 22 | 5.6 | 14 | 6.9 | 24 | 0.9 (38/38) |
| nudge | fw-dbn | 36 | 36 | 4.4 | 26 | 4.8 | 15 | 2.9 | 21 | 0.9 (36/36) |
| other | all-d-f60 | 38 | 35 | 16.2 | 50 | 10.2 | 27 | 4.0 | 11 | 1.1 (37/38) |
| other | fw-dbn | 36 | 33 | 17.8 | 34 | 8.7 | 28 | 2.9 | 8 | 1.1 (35/36) |
