# follow-robust: keep the BPM through weak patches (2026-11-16)

Question: today's follower (tuned-l16-causal, firmware M2j) stops or loses the tempo on many tracks. Can four additions
(coast, tempo memory, ratio check, agreement confidence) make it hold the BPM longer without more wrong locks?

Data: 829 tracks, madmom features cached on gpufarm; DBN in numba is bit-exact with the cache's beats (tests). Dev half
(415) used for tuning, test half (414) never touched by tuning. Final run: 9 settings x 829 tracks, 14.8 min on 12 workers.
Today's follower reproduces `pertrack`'s numbers (221 held whole, 179 wrong tempo, 10 never locked).

## Headline (all 829 tracks): today -> all-d-f60

| | today | all-d-f60 |
|---|---|---|
| tempo held for the whole track | 221 (27 %) | 323 (39 %) |
| long stops | 208 | 100 |
| wrong tempo | 179 | 150 |
| never locked | 10 | 13 |
| real tempo changes followed | 11/24 | 12/24 |
| median stopped fraction | 0.043 | 0.016 |

Test half alone: held 104 -> 157, long stops 112 -> 49, wrong tempo 85 -> 77, changes followed 5/14 -> 7/14.
The wrong-tempo gain is smaller on unseen tracks than on dev (94 -> 73); the stop gain holds.

## Costs and risks
- It coasts (keeps playing the BPM with no evidence): 104 min over 829 tracks, 69 % of it right, 14 % wrong (14.9 min).
- Median delay to follow a real change 13.0 -> 16.8 s.
- Never locked 10 -> 13.
- Plain coast (coast30) backfires: held 458 but wrong tempo 344, 96 min of wrong coasting. The strict gate was right on
  many tracks. The no-coast variant (agreement + memory + ratio only) gives wrong tempo 100 (best) but held 224, long stops 185.
- Unmeasured: how it sounds; Sympathy-style off-beat slides were not re-listened.

## What it is (all-d-f60)
coast 30 s with a strict gate on jumps and floor 0.6 on the agreement; tempo memory (prior width 0.04, floor 0.1,
related-lock 48 beats); ratio check (margin 0.05 over 10 s); agreement of lines + tempo + phase + DBN at 0.8.
Ablations (dev): no memory held 145 (vs 166), no ratio ~same, no coast 111; floor 0.5 holds more (219) but 15 min wrong
coasting; floor 0.7 fewer wrong (63) but held 130.

## CPU on the S3: ESTIMATE ONLY, NOT MEASURED
M2j is at 42.6-43.0 % of a ~45 % ceiling (estimate), heap_min ~9.1 KB. Estimated adds: coast none; memory prior 35 mults
per frame + lock rule: cheap; ratio check ~30 k MAC per lock attempt, 4 KB activation ring; agreement tempo part ~170 k
MAC per 0.5 s refresh (cheap-medium); the DBN part of the agreement is NOT costed and is the unknown. A real number needs
a device build with cycle counters, which only the User can flash. The firmware port is not started.

## Files
results/sweep_dev.md (dev half, all settings), final_all.md, final_test.md; rows in beat/data/followrobust/ (git-ignored).
