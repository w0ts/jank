# Per track: does the follower hold the BPM the whole way? (tuned-l16-causal, M2j, the user's Traktor set)

Replay of the firmware's follower (`tuned-l16-causal`) and today's chain on every track of the set, scored per track against the audio reference (madmom offline beats cut into tempo sections, each change checked against Beat This!), output folded into the reference's octave, as `score.py`. Code: `pertrack.py`, `pertrack_report.py`. Per-track rows are in `beat/data/pertrack.jsonl` (git-ignored: derived from madmom).

Definitions (everything after the track's first lock, in the music from first reference beat + 10 s):
- first lock: the first tick with the outputs running;
- within 1 %: running with the folded tempo within 1 % of the reference; wrong: running and more than 2 % off;
- stop / wrong episode: a run of 2 s or more of stopped / wrong time;
- **held whole**: no wrong episode and no stop of 2 s or more after the first lock;
- **short stops**: no wrong episode, at least one stop of 2 s or more, all of them 10 s or shorter;
- **long stops**: no wrong episode, a stop longer than 10 s;
- **wrong tempo**: at least one wrong-tempo episode of 2 s or more (it may also have stops);
- **never locked**: the outputs never ran in the music.

## Headline

221 of 829 tracks held the BPM the whole way (tuned-l16-causal; 155 with today); 211 had only short stops (up to 10 s) and 208 longer stops, 179 had a wrong tempo for 2 s or more at some point, 10 never locked (today: 175, 375, 121, 3).

Caveats: the reference's transitions between tempo sections (segments under 16 beat intervals) are left out, the output is not judged there; the half-beat Traktor grid fixes (`eval/ref_phase.py`) are about the Traktor grid's phase, and this study's reference is madmom's own beats, so they are not applied; the signs in the worst-track table are heuristics.

## test half

| chain | tracks | held whole | short stops | long stops | wrong tempo | never locked | median within 1 % | median stopped | median wrong | stops >= 2 s / track | wrong episodes >= 2 s / track | within 1 % >= 95 % | 80-95 % | 50-80 % | < 50 % |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tuned-l16-causal | 414 | 104 (25 %) | 109 (26 %) | 112 (27 %) | 85 (21 %) | 4 (1 %) | 0.946 | 0.043 | 0.000 | 1.96 | 0.34 | 198 (48 %) | 160 (39 %) | 34 (8 %) | 22 (5 %) |
| today | 414 | 75 (18 %) | 85 (21 %) | 200 (48 %) | 51 (12 %) | 3 (1 %) | 0.914 | 0.085 | 0.000 | 2.78 | 0.25 | 139 (34 %) | 194 (47 %) | 54 (13 %) | 27 (7 %) |

## dev half

| chain | tracks | held whole | short stops | long stops | wrong tempo | never locked | median within 1 % | median stopped | median wrong | stops >= 2 s / track | wrong episodes >= 2 s / track | within 1 % >= 95 % | 80-95 % | 50-80 % | < 50 % |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tuned-l16-causal | 415 | 117 (28 %) | 102 (25 %) | 96 (23 %) | 94 (23 %) | 6 (1 %) | 0.946 | 0.044 | 0.000 | 2.00 | 0.43 | 200 (48 %) | 147 (35 %) | 40 (10 %) | 28 (7 %) |
| today | 415 | 80 (19 %) | 90 (22 %) | 175 (42 %) | 70 (17 %) | 0 (0 %) | 0.910 | 0.081 | 0.000 | 2.93 | 0.33 | 148 (36 %) | 175 (42 %) | 60 (14 %) | 32 (8 %) |

## all tracks

| chain | tracks | held whole | short stops | long stops | wrong tempo | never locked | median within 1 % | median stopped | median wrong | stops >= 2 s / track | wrong episodes >= 2 s / track | within 1 % >= 95 % | 80-95 % | 50-80 % | < 50 % |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tuned-l16-causal | 829 | 221 (27 %) | 211 (25 %) | 208 (25 %) | 179 (22 %) | 10 (1 %) | 0.946 | 0.043 | 0.000 | 1.98 | 0.38 | 398 (48 %) | 307 (37 %) | 74 (9 %) | 50 (6 %) |
| today | 829 | 155 (19 %) | 175 (21 %) | 375 (45 %) | 121 (15 %) | 3 (0 %) | 0.912 | 0.084 | 0.000 | 2.86 | 0.29 | 287 (35 %) | 369 (45 %) | 114 (14 %) | 59 (7 %) |

## How long are the longest episodes? (the 2 s threshold decides much)

Locked tracks by the length of their longest wrong-tempo episode and of their longest stop:

| half | chain | locked tracks | wrong < 2 s | wrong 2-5 s | wrong 5-10 s | wrong 10-30 s | wrong >= 30 s | stop < 2 s | stop 2-5 s | stop 5-10 s | stop 10-30 s | stop >= 30 s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| test half | tuned-l16-causal | 410 | 325 (79 %) | 36 (9 %) | 19 (5 %) | 26 (6 %) | 4 (1 %) | 108 (26 %) | 9 (2 %) | 125 (30 %) | 133 (32 %) | 35 (9 %) |
| test half | today | 411 | 360 (88 %) | 25 (6 %) | 15 (4 %) | 9 (2 %) | 2 (0 %) | 75 (18 %) | 8 (2 %) | 85 (21 %) | 186 (45 %) | 57 (14 %) |
| dev half | tuned-l16-causal | 409 | 315 (77 %) | 30 (7 %) | 26 (6 %) | 27 (7 %) | 11 (3 %) | 122 (30 %) | 10 (2 %) | 121 (30 %) | 121 (30 %) | 35 (9 %) |
| dev half | today | 415 | 345 (83 %) | 27 (7 %) | 22 (5 %) | 13 (3 %) | 8 (2 %) | 80 (19 %) | 3 (1 %) | 97 (23 %) | 180 (43 %) | 55 (13 %) |
| all tracks | tuned-l16-causal | 819 | 640 (78 %) | 66 (8 %) | 45 (5 %) | 53 (6 %) | 15 (2 %) | 230 (28 %) | 19 (2 %) | 246 (30 %) | 254 (31 %) | 70 (9 %) |
| all tracks | today | 826 | 705 (85 %) | 52 (6 %) | 37 (4 %) | 22 (3 %) | 10 (1 %) | 155 (19 %) | 11 (1 %) | 182 (22 %) | 366 (44 %) | 112 (14 %) |

Wrong episodes by how far off the tempo was (the output's folded tempo over the reference's, brought into one octave; 0.75 = 3:2 / 4:3 relations, 0.8 / 1.25 = 5:4, 0.9 / 1.1 = 10 %). Only the 6 longest episodes of each track are kept, all tracks:

tuned-l16-causal:

| output / reference | < 0.80 | 0.80-0.90 | 0.90-0.98 | ~1 | 1.02-1.10 | 1.10-1.20 | >= 1.20 | all | began after a stop |
|---|---|---|---|---|---|---|---|---|---|
| wrong episodes >= 2 s | 88 (36 %) | 14 (6 %) | 41 (17 %) | 2 (1 %) | 37 (15 %) | 18 (7 %) | 42 (17 %) | 242 | 71 (29 %) |

tuned-l16-causal:

| output / reference | < 0.80 | 0.80-0.90 | 0.90-0.98 | ~1 | 1.02-1.10 | 1.10-1.20 | >= 1.20 | all | began after a stop |
|---|---|---|---|---|---|---|---|---|---|
| wrong episodes >= 10 s | 49 (52 %) | 6 (6 %) | 4 (4 %) | 0 (0 %) | 5 (5 %) | 5 (5 %) | 25 (27 %) | 94 | 49 (52 %) |

today:

| output / reference | < 0.80 | 0.80-0.90 | 0.90-0.98 | ~1 | 1.02-1.10 | 1.10-1.20 | >= 1.20 | all | began after a stop |
|---|---|---|---|---|---|---|---|---|---|
| wrong episodes >= 2 s | 73 (52 %) | 8 (6 %) | 10 (7 %) | 0 (0 %) | 7 (5 %) | 8 (6 %) | 35 (25 %) | 141 | 76 (54 %) |

today:

| output / reference | < 0.80 | 0.80-0.90 | 0.90-0.98 | ~1 | 1.02-1.10 | 1.10-1.20 | >= 1.20 | all | began after a stop |
|---|---|---|---|---|---|---|---|---|---|
| wrong episodes >= 10 s | 33 (72 %) | 0 (0 %) | 0 (0 %) | 0 (0 %) | 0 (0 %) | 1 (2 %) | 12 (26 %) | 46 | 32 (70 %) |

### test half, tuned-l16-causal: by reference class

| class | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| steady | 351 | 103 (29 %) | 104 (30 %) | 93 (26 %) | 50 (14 %) | 1 (0 %) |
| disputed | 31 | 0 (0 %) | 3 (10 %) | 9 (29 %) | 18 (58 %) | 1 (3 %) |
| probable | 24 | 1 (4 %) | 2 (8 %) | 6 (25 %) | 14 (58 %) | 1 (4 %) |
| changing | 8 | 0 (0 %) | 0 (0 %) | 4 (50 %) | 3 (38 %) | 1 (12 %) |
| **all** | **414** | **104 (25 %)** | **109 (26 %)** | **112 (27 %)** | **85 (21 %)** | **4 (1 %)** |

### test half, tuned-l16-causal: by genre

| genre | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| techno | 230 | 69 (30 %) | 68 (30 %) | 56 (24 %) | 37 (16 %) | 0 (0 %) |
| house / tech house | 46 | 17 (37 %) | 11 (24 %) | 11 (24 %) | 7 (15 %) | 0 (0 %) |
| pop / hyperpop / other vocal | 35 | 6 (17 %) | 10 (29 %) | 10 (29 %) | 8 (23 %) | 1 (3 %) |
| electro / breaks / bass | 34 | 4 (12 %) | 5 (15 %) | 12 (35 %) | 12 (35 %) | 1 (3 %) |
| unknown | 30 | 4 (13 %) | 4 (13 %) | 12 (40 %) | 9 (30 %) | 1 (3 %) |
| ambient / downtempo | 20 | 2 (10 %) | 7 (35 %) | 3 (15 %) | 7 (35 %) | 1 (5 %) |
| IDM / experimental | 15 | 2 (13 %) | 2 (13 %) | 7 (47 %) | 4 (27 %) | 0 (0 %) |
| other | 3 | 0 (0 %) | 2 (67 %) | 0 (0 %) | 1 (33 %) | 0 (0 %) |
| trance | 1 | 0 (0 %) | 0 (0 %) | 1 (100 %) | 0 (0 %) | 0 (0 %) |
| **all** | **414** | **104 (25 %)** | **109 (26 %)** | **112 (27 %)** | **85 (21 %)** | **4 (1 %)** |

### test half, today: by reference class

| class | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| steady | 351 | 75 (21 %) | 82 (23 %) | 161 (46 %) | 32 (9 %) | 1 (0 %) |
| disputed | 31 | 0 (0 %) | 0 (0 %) | 16 (52 %) | 14 (45 %) | 1 (3 %) |
| probable | 24 | 0 (0 %) | 3 (12 %) | 17 (71 %) | 3 (12 %) | 1 (4 %) |
| changing | 8 | 0 (0 %) | 0 (0 %) | 6 (75 %) | 2 (25 %) | 0 (0 %) |
| **all** | **414** | **75 (18 %)** | **85 (21 %)** | **200 (48 %)** | **51 (12 %)** | **3 (1 %)** |

### test half, today: by genre

| genre | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| techno | 230 | 54 (23 %) | 52 (23 %) | 102 (44 %) | 22 (10 %) | 0 (0 %) |
| house / tech house | 46 | 8 (17 %) | 13 (28 %) | 22 (48 %) | 3 (7 %) | 0 (0 %) |
| pop / hyperpop / other vocal | 35 | 3 (9 %) | 6 (17 %) | 20 (57 %) | 5 (14 %) | 1 (3 %) |
| electro / breaks / bass | 34 | 4 (12 %) | 6 (18 %) | 14 (41 %) | 9 (26 %) | 1 (3 %) |
| unknown | 30 | 3 (10 %) | 4 (13 %) | 19 (63 %) | 4 (13 %) | 0 (0 %) |
| ambient / downtempo | 20 | 1 (5 %) | 2 (10 %) | 11 (55 %) | 5 (25 %) | 1 (5 %) |
| IDM / experimental | 15 | 2 (13 %) | 2 (13 %) | 8 (53 %) | 3 (20 %) | 0 (0 %) |
| other | 3 | 0 (0 %) | 0 (0 %) | 3 (100 %) | 0 (0 %) | 0 (0 %) |
| trance | 1 | 0 (0 %) | 0 (0 %) | 1 (100 %) | 0 (0 %) | 0 (0 %) |
| **all** | **414** | **75 (18 %)** | **85 (21 %)** | **200 (48 %)** | **51 (12 %)** | **3 (1 %)** |

### dev half, tuned-l16-causal: by reference class

| class | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| steady | 360 | 116 (32 %) | 100 (28 %) | 87 (24 %) | 55 (15 %) | 2 (1 %) |
| disputed | 25 | 0 (0 %) | 2 (8 %) | 2 (8 %) | 17 (68 %) | 4 (16 %) |
| probable | 22 | 1 (5 %) | 0 (0 %) | 6 (27 %) | 15 (68 %) | 0 (0 %) |
| changing | 8 | 0 (0 %) | 0 (0 %) | 1 (12 %) | 7 (88 %) | 0 (0 %) |
| **all** | **415** | **117 (28 %)** | **102 (25 %)** | **96 (23 %)** | **94 (23 %)** | **6 (1 %)** |

### dev half, tuned-l16-causal: by genre

| genre | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| techno | 237 | 79 (33 %) | 73 (31 %) | 43 (18 %) | 41 (17 %) | 1 (0 %) |
| house / tech house | 37 | 13 (35 %) | 6 (16 %) | 12 (32 %) | 6 (16 %) | 0 (0 %) |
| electro / breaks / bass | 34 | 7 (21 %) | 5 (15 %) | 8 (24 %) | 14 (41 %) | 0 (0 %) |
| pop / hyperpop / other vocal | 34 | 8 (24 %) | 5 (15 %) | 13 (38 %) | 8 (24 %) | 0 (0 %) |
| unknown | 27 | 3 (11 %) | 6 (22 %) | 6 (22 %) | 10 (37 %) | 2 (7 %) |
| ambient / downtempo | 22 | 4 (18 %) | 2 (9 %) | 8 (36 %) | 8 (36 %) | 0 (0 %) |
| IDM / experimental | 20 | 3 (15 %) | 2 (10 %) | 5 (25 %) | 7 (35 %) | 3 (15 %) |
| other | 2 | 0 (0 %) | 2 (100 %) | 0 (0 %) | 0 (0 %) | 0 (0 %) |
| trance | 2 | 0 (0 %) | 1 (50 %) | 1 (50 %) | 0 (0 %) | 0 (0 %) |
| **all** | **415** | **117 (28 %)** | **102 (25 %)** | **96 (23 %)** | **94 (23 %)** | **6 (1 %)** |

### dev half, today: by reference class

| class | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| steady | 360 | 79 (22 %) | 89 (25 %) | 156 (43 %) | 36 (10 %) | 0 (0 %) |
| disputed | 25 | 0 (0 %) | 1 (4 %) | 10 (40 %) | 14 (56 %) | 0 (0 %) |
| probable | 22 | 1 (5 %) | 0 (0 %) | 7 (32 %) | 14 (64 %) | 0 (0 %) |
| changing | 8 | 0 (0 %) | 0 (0 %) | 2 (25 %) | 6 (75 %) | 0 (0 %) |
| **all** | **415** | **80 (19 %)** | **90 (22 %)** | **175 (42 %)** | **70 (17 %)** | **0 (0 %)** |

### dev half, today: by genre

| genre | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| techno | 237 | 59 (25 %) | 58 (24 %) | 90 (38 %) | 30 (13 %) | 0 (0 %) |
| house / tech house | 37 | 8 (22 %) | 8 (22 %) | 15 (41 %) | 6 (16 %) | 0 (0 %) |
| electro / breaks / bass | 34 | 3 (9 %) | 6 (18 %) | 14 (41 %) | 11 (32 %) | 0 (0 %) |
| pop / hyperpop / other vocal | 34 | 4 (12 %) | 5 (15 %) | 21 (62 %) | 4 (12 %) | 0 (0 %) |
| unknown | 27 | 2 (7 %) | 4 (15 %) | 13 (48 %) | 8 (30 %) | 0 (0 %) |
| ambient / downtempo | 22 | 1 (5 %) | 6 (27 %) | 9 (41 %) | 6 (27 %) | 0 (0 %) |
| IDM / experimental | 20 | 3 (15 %) | 2 (10 %) | 10 (50 %) | 5 (25 %) | 0 (0 %) |
| other | 2 | 0 (0 %) | 1 (50 %) | 1 (50 %) | 0 (0 %) | 0 (0 %) |
| trance | 2 | 0 (0 %) | 0 (0 %) | 2 (100 %) | 0 (0 %) | 0 (0 %) |
| **all** | **415** | **80 (19 %)** | **90 (22 %)** | **175 (42 %)** | **70 (17 %)** | **0 (0 %)** |

### all tracks, tuned-l16-causal: by reference class

| class | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| steady | 711 | 219 (31 %) | 204 (29 %) | 180 (25 %) | 105 (15 %) | 3 (0 %) |
| disputed | 56 | 0 (0 %) | 5 (9 %) | 11 (20 %) | 35 (62 %) | 5 (9 %) |
| probable | 46 | 2 (4 %) | 2 (4 %) | 12 (26 %) | 29 (63 %) | 1 (2 %) |
| changing | 16 | 0 (0 %) | 0 (0 %) | 5 (31 %) | 10 (62 %) | 1 (6 %) |
| **all** | **829** | **221 (27 %)** | **211 (25 %)** | **208 (25 %)** | **179 (22 %)** | **10 (1 %)** |

### all tracks, tuned-l16-causal: by genre

| genre | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| techno | 467 | 148 (32 %) | 141 (30 %) | 99 (21 %) | 78 (17 %) | 1 (0 %) |
| house / tech house | 83 | 30 (36 %) | 17 (20 %) | 23 (28 %) | 13 (16 %) | 0 (0 %) |
| pop / hyperpop / other vocal | 69 | 14 (20 %) | 15 (22 %) | 23 (33 %) | 16 (23 %) | 1 (1 %) |
| electro / breaks / bass | 68 | 11 (16 %) | 10 (15 %) | 20 (29 %) | 26 (38 %) | 1 (1 %) |
| unknown | 57 | 7 (12 %) | 10 (18 %) | 18 (32 %) | 19 (33 %) | 3 (5 %) |
| ambient / downtempo | 42 | 6 (14 %) | 9 (21 %) | 11 (26 %) | 15 (36 %) | 1 (2 %) |
| IDM / experimental | 35 | 5 (14 %) | 4 (11 %) | 12 (34 %) | 11 (31 %) | 3 (9 %) |
| other | 5 | 0 (0 %) | 4 (80 %) | 0 (0 %) | 1 (20 %) | 0 (0 %) |
| trance | 3 | 0 (0 %) | 1 (33 %) | 2 (67 %) | 0 (0 %) | 0 (0 %) |
| **all** | **829** | **221 (27 %)** | **211 (25 %)** | **208 (25 %)** | **179 (22 %)** | **10 (1 %)** |

### all tracks, today: by reference class

| class | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| steady | 711 | 154 (22 %) | 171 (24 %) | 317 (45 %) | 68 (10 %) | 1 (0 %) |
| disputed | 56 | 0 (0 %) | 1 (2 %) | 26 (46 %) | 28 (50 %) | 1 (2 %) |
| probable | 46 | 1 (2 %) | 3 (7 %) | 24 (52 %) | 17 (37 %) | 1 (2 %) |
| changing | 16 | 0 (0 %) | 0 (0 %) | 8 (50 %) | 8 (50 %) | 0 (0 %) |
| **all** | **829** | **155 (19 %)** | **175 (21 %)** | **375 (45 %)** | **121 (15 %)** | **3 (0 %)** |

### all tracks, today: by genre

| genre | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| techno | 467 | 113 (24 %) | 110 (24 %) | 192 (41 %) | 52 (11 %) | 0 (0 %) |
| house / tech house | 83 | 16 (19 %) | 21 (25 %) | 37 (45 %) | 9 (11 %) | 0 (0 %) |
| pop / hyperpop / other vocal | 69 | 7 (10 %) | 11 (16 %) | 41 (59 %) | 9 (13 %) | 1 (1 %) |
| electro / breaks / bass | 68 | 7 (10 %) | 12 (18 %) | 28 (41 %) | 20 (29 %) | 1 (1 %) |
| unknown | 57 | 5 (9 %) | 8 (14 %) | 32 (56 %) | 12 (21 %) | 0 (0 %) |
| ambient / downtempo | 42 | 2 (5 %) | 8 (19 %) | 20 (48 %) | 11 (26 %) | 1 (2 %) |
| IDM / experimental | 35 | 5 (14 %) | 4 (11 %) | 18 (51 %) | 8 (23 %) | 0 (0 %) |
| other | 5 | 0 (0 %) | 1 (20 %) | 4 (80 %) | 0 (0 %) | 0 (0 %) |
| trance | 3 | 0 (0 %) | 0 (0 %) | 3 (100 %) | 0 (0 %) | 0 (0 %) |
| **all** | **829** | **155 (19 %)** | **175 (21 %)** | **375 (45 %)** | **121 (15 %)** | **3 (0 %)** |

### all tracks, tuned-l16-causal: tracks with a half-time / double-time section in the reference

| group | tracks | held whole | short stops | long stops | wrong tempo | never locked |
|---|---|---|---|---|---|---|
| half-time section | 43 | 0 (0 %) | 6 (14 %) | 16 (37 %) | 21 (49 %) | 0 (0 %) |
| **all** | **43** | **0 (0 %)** | **6 (14 %)** | **16 (37 %)** | **21 (49 %)** | **0 (0 %)** |

## The 25 worst tracks (tuned-l16-causal, all 829 tracks, by the share of time within 1 % after the first lock; never-locked tracks first)

Names are the file names of the user's set. The numbers in brackets are today's chain on the same track. Signs are heuristics: low activation = the live activation's median peak at the reference beats under 0.4; swing = the strongest off-beat onset at 0.58-0.75 of the beat period and at least 0.6 of the on-beat's; off-beat slides = 20 % or more of the running reference beats in the right tempo but half a beat out; intro / outro = 50 % or more of the off-tempo time in the first or last 40 s; tempo change / half-time = the reference's class.

| # | track | BPM | genre | class | held tuned-l16-causal (today) | stopped | wrong | first lock | what went wrong, when | signs |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Jam City, Aidan - Jam City Presents EFM - Be Mine | 125 | pop / hyperpop / other vocal | steady | never locked (never locked) | 100 % | 0 % | - | never locked | low activation |
| 2 | Burial - Burial - Broken Home | 140 | electro / breaks / bass | probable | never locked (never locked) | 100 % | 0 % | - | never locked | low activation, tempo change |
| 3 | Fennesz, David Sylvian - Venice - Transit (feat. David Sylvian) | 115 | unknown | changing | never locked (0.00) | 100 % | 0 % | - | never locked | low activation, tempo change |
| 4 | James Blake - The Colour In Anything - Meet You In The Maze | 117 | IDM / experimental | disputed | never locked (0.00) | 100 % | 0 % | - | never locked | low activation |
| 5 | K Wata - Dot Dot Dot - Lost My Focus | 129 | IDM / experimental | disputed | never locked (0.02) | 100 % | 0 % | - | never locked | low activation |
| 6 | Nathan Fake - Crystal Vision - Cmd | 130 | techno | steady | never locked (0.00) | 100 % | 0 % | - | never locked | low activation |
| 7 | Aaron Hibell, Felsmann - Tiley - Astral Projection - levitation | 134 | unknown | steady | never locked (0.06) | 100 % | 0 % | - | never locked | low activation |
| 8 | Clark - Steep Stims - Gift and Wound | 137 | IDM / experimental | disputed | never locked (0.16) | 100 % | 0 % | - | never locked | low activation |
| 9 |  Graintable - Herons - 39 | 110 | ambient / downtempo | disputed | never locked (never locked) | 100 % | 0 % | - | never locked | low activation |
| 10 | Sam Binga, Machinedrum, Cesco - Niche Flex EP - l00k | 138 | unknown | disputed | never locked (0.04) | 100 % | 0 % | - | never locked | low activation |
| 11 | Nazar - Guerrilla - Retaliation | 111 | pop / hyperpop / other vocal | steady | 0.00 (0.00) | 79 % | 21 % | 138 s | stopped 50 s at 151 s; wrong tempo (x0.74 of the reference) 13 s at 138 s | low activation, intro / outro |
| 12 | Jason Patrick - Interpretation (Original Mix) [Peer] | 118 | techno | probable | 0.00 (0.00) | 82 % | 18 % | 180 s | stopped 135 s at 209 s; wrong tempo (x0.75 of the reference) 29 s at 180 s | low activation, swing, tempo change |
| 13 | Kastil, Oscar Mulero - Diode - Diode - Oscar Mulero Remix | 132 | techno | steady | 0.00 (0.01) | 98 % | 2 % | 146 s | stopped 101 s at 152 s; stopped 78 s at 254 s; stopped 3 s at 148 s | low activation |
| 14 | Burial - Burial - Forgive | 110 | electro / breaks / bass | probable | 0.00 (0.00) | 100 % | 0 % | 88 s | stopped 62 s at 104 s | low activation, tempo change, intro / outro |
| 15 | Burial - Burial - U Hurt Me | 103 | electro / breaks / bass | steady | 0.00 (0.00) | 52 % | 48 % | 54 s | stopped 104 s at 73 s; wrong tempo (x0.75 of the reference) 60 s at 244 s; wrong tempo (x0.75 of the reference) 49 s at 177 s | low activation, half-time section |
| 16 | Burial - Untrue - Etched Headplate | 131 | ambient / downtempo | disputed | 0.00 (0.00) | 40 % | 60 % | 74 s | wrong tempo (x0.75 of the reference) 65 s at 167 s; wrong tempo (x0.75 of the reference) 38 s at 286 s; stopped 32 s at 232 s | low activation |
| 17 | mu tate - Faded - Faded | 128 | ambient / downtempo | probable | 0.00 (0.00) | 73 % | 27 % | 128 s | stopped 110 s at 130 s; stopped 45 s at 308 s; wrong tempo (x1.33 of the reference) 31 s at 240 s | low activation, tempo change |
| 18 | alva noto - Xerrox, Vol.5 - Xerrox Ascent II | 115 | ambient / downtempo | probable | 0.00 (0.00) | 100 % | 0 % | 266 s | stopped 80 s at 275 s | low activation, tempo change, intro / outro |
| 19 | Datassette - Offal 2, Pt. 1 - Let's Move To Belgium (version 1, 2021) | 84 | ambient / downtempo | changing | 0.00 (0.00) | 96 % | 4 % | 29 s | stopped 240 s at 111 s; stopped 45 s at 34 s; stopped 14 s at 82 s | low activation, tempo change |
| 20 |  Bot1500 - The Black Sea - Caves | 93 | IDM / experimental | probable | 0.00 (0.13) | 82 % | 12 % | 160 s | stopped 20 s at 165 s; wrong tempo (x0.98 of the reference) 3 s at 160 s | low activation, tempo change, intro / outro |
| 21 | Skrillex-Varg²™-Eurohead-SIIICKBRAIN-swedm® - Kora - Kora | 150 | IDM / experimental | disputed | 0.03 (0.02) | 70 % | 24 % | 60 s | stopped 44 s at 75 s; stopped 23 s at 123 s; wrong tempo (x0.75 of the reference) 21 s at 154 s | low activation |
| 22 | Thom Yorke - Anima - Dawn Chorus | 144 | unknown | changing | 0.04 (0.05) | 93 % | 1 % | 46 s | stopped 96 s at 48 s; stopped 82 s at 240 s; stopped 79 s at 158 s | low activation, tempo change |
| 23 | Daniel Herskedal - Slow Eastbound Train - The Mistral Noir | 120 | pop / hyperpop / other vocal | steady | 0.07 (0.08) | 77 % | 16 % | 39 s | stopped 76 s at 77 s; stopped 28 s at 156 s; stopped 22 s at 226 s | low activation |
| 24 | Etapp Kyle - Nolove - Nolove | 105 | unknown | disputed | 0.10 (0.12) | 43 % | 47 % | 36 s | stopped 29 s at 108 s; wrong tempo (x0.75 of the reference) 21 s at 167 s; wrong tempo (x0.87 of the reference) 18 s at 137 s | low activation, swing, half-time section |
| 25 | Bersarin Quartett - Systeme - Macht | 112 | ambient / downtempo | steady | 0.10 (0.14) | 67 % | 21 % | 102 s | stopped 40 s at 109 s; stopped 26 s at 217 s; stopped 20 s at 188 s | low activation, intro / outro |

## Do the worst share a cause?

Share of tracks with each sign: the worst tracks, every track with a wrong-tempo episode or never locked, and all tracks (the base rate).

| group | tracks | low activation | swing | off-beat slides | tempo change | intro / outro | half-time section | none |
|---|---|---|---|---|---|---|---|---|
| worst 25 | 25 | 25 (100 %) | 2 (8 %) | 0 (0 %) | 9 (36 %) | 5 (20 %) | 2 (8 %) | 0 (0 %) |
| wrong tempo or never locked | 189 | 87 (46 %) | 6 (3 %) | 19 (10 %) | 41 (22 %) | 45 (24 %) | 21 (11 %) | 55 (29 %) |
| held whole | 221 | 11 (5 %) | 1 (0 %) | 1 (0 %) | 2 (1 %) | 19 (9 %) | 0 (0 %) | 189 (86 %) |
| short stops | 211 | 26 (12 %) | 0 (0 %) | 30 (14 %) | 2 (1 %) | 39 (18 %) | 6 (3 %) | 139 (66 %) |
| long stops | 208 | 54 (26 %) | 1 (0 %) | 28 (13 %) | 17 (8 %) | 36 (17 %) | 16 (8 %) | 114 (55 %) |
| all tracks | 829 | 178 (21 %) | 8 (1 %) | 78 (9 %) | 62 (7 %) | 139 (17 %) | 43 (5 %) | 497 (60 %) |

