# follow-robust: paused state (2026-11-16 ~08:05)

## Done
- Feature cache rebuilt on gpufarm: ~/optimist/worktrees/followrobust-cache (829 tracks, 365 MB). KEEP it.
- Reference rebuilt: ~/optimist/worktrees/followrobust-out/ref.json (same classes as before: changing 16, probable 46,
  disputed 56, steady 711).
- Farm code copy: ~/optimist/worktrees/followrobust-code (rsynced from this folder; data/traktor, data/torch, .venv
  linked to the farm's beat-listener checkout; data/mmdb_traktor copied).
- Code (this folder, all UNCOMMITTED, untracked): dbn.py (numba DBN, bit-exact with the cache's madmom beats: checked on
  3 real tracks with rdbn_check.py and in the tests), comb.py, robust.py, rchain.py, rconfigs.py, rtrack.py, report.py,
  farm_run.sh, sweep_dev.json, sweep_dev2.json, sweep_dev3.json, results/sweep_dev.md (dev1 + dev2 table);
  tests: beat/tests/test_follow_robust.py (18 pass). The replay with every flag off equals chain.replay (tested).
- Dev sweeps dev1 (30 chains) and dev2 (19 chains) done; rows on the Mac in beat/data/followrobust/dev1.jsonl, dev2.jsonl
  (git-ignored) and on the farm in followrobust-out/.

## Dev findings so far (415 dev tracks; fw = tuned-l16-causal: held whole 117, wrong tempo 94, long stops 96)
- Coast alone (on_jump follow): held 229, but wrong tempo 168, wrong eps >= 10 s 54 -> 138, 48 min coasting wrong (37 %
  of coasting); 53 of fw's "long stops" tracks become "wrong tempo" (the gate was rightly stopping them). Most coast-wrong
  time is on steady tracks, not tempo changes. HOLD 15/30/60 identical: the follower never stopped on its own on any dev
  track (follower_stops = 0 measured), so "stop on no evidence" never fires.
- coast30-gate (strict gate again after a jump): held 220, wrong 144, coast wrong 24 %.
- Memory (prior hold w 0.04 floor 0.1, related lock 48): small win (wrong 94 -> 85, eps >= 10 s 54 -> 39, 28 better / 9 worse).
  reseed is worse; once ~neutral; lock rule alone 6 better / 0 worse.
- Ratio check: small (m 0.05: wrong eps after a stop 55 -> 41, >= 10 s 54 -> 42; categories ~neutral).
- Agreement (lines+tempo+phase, thr 0.75-0.8): wrong tempo 94 -> 73-79, long stops 96 -> 71-91; with the DBN part 60.
- Best combination so far: all-d-f60 (coast gate-on-jump with floor 0.6 on the agreement, memory, ratio 0.05, agreement
  lines+tempo+phase+dbn at 0.8): held 117 -> 166, wrong tempo 94 -> 73, long stops 96 -> 51, eps >= 10 s 54 -> 39,
  126 better / 10 worse, coast wrong 8.5 min (16 %); real changes followed 6/10 -> 5/10 (a cost).

## Running when paused
- dev3 sweep (sweep_dev3.json: all-d-f60 variants and ablations), under the farm flock:
  log ~/optimist/worktrees/followrobust-out/dev3.log, rows ~/optimist/worktrees/followrobust-out/dev3.jsonl.

## Left
1. Fetch dev3.jsonl, add it to results/sweep_dev.md (report.py with dev1 dev2 dev3), pick the combination.
2. final.json: fw, coast30, coast30-gate, mem-w04-f10-l48, ratio-m05, agree-ltp-75, agree-ltpd-80, the chosen
   combination (+ agd80-mem-ratio, the no-coast option); run on ALL 829 (farm_run.sh with CONFIGS, no SPLIT, TAG=final);
   check fw reproduces per_track.md (221 held whole / 179 wrong tempo / 10 never locked).
3. results/final.md (test half + all, --groups for class/genre, --worst private list), REPORT.md (headline, costs,
   S3 CPU class: coast none; memory prior 35 mults/frame + lock rule: cheap; ratio check ~30 k MAC per lock attempt,
   4 KB activation ring: cheap; agreement tempo part ~170 k MAC per 0.5 s refresh: cheap-medium (estimates)).
4. Commit only follow-robust/ files + tests/test_follow_robust.py (explicit paths; not player.html).
5. Cleanup: farm followrobust-code and followrobust-out (copy rows first), Mac worktrees/followrobust-scratch; keep the cache.
