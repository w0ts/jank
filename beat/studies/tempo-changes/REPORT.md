# Tempo changes and weak signals: the M2i follower on the user's set (2026-11-16)

An offline Python study on the user's 829 Traktor tracks. No firmware was changed.
- Tuned on the dev half (even positions), reported on the test half (odd).
- Numbers are measured unless marked "estimate". Tables are in `results/`.
- What is scored is the folded output (AUTO RANGE), what Link / MIDI get. It is compared with the audio reference folded into the output's octave. An octave counts as an error only when the output itself jumps an octave ("range breaks").

## 1. Before → after (test half)

The chains:
- **today:** ModeBpm and the grid confidence gate (0.8).
- **m2i / m2i+rate:** the M2i follower as built, with the rate off / on.
- **tuned-l16 (the proposal):** M2i + rate with five changes (section 5).

Steady tracks (351):

| chain | BPM held (0.5 %) | stopped | wrong-tempo locks / track | range breaks / track | flips / track |
|---|---|---|---|---|---|
| today | 0.859 | 0.119 | 0.15 | 0.009 | 3.91 |
| m2i | 0.846 | 0.128 | 0.26 | 0.003 | 4.01 |
| m2i+rate | 0.832 | 0.129 | 0.31 | 0.003 | 4.60 |
| **tuned-l16** | **0.858** | **0.105** | **0.20** | **0.000** | **3.84** |

Beat-less passages: 139 passages where the live activation stays low for 4 s or more.

| chain | stopped in them | running at the first strong beat after | median phase error there |
|---|---|---|---|
| today | 0.60 | 0.45 | 26 ms |
| m2i+rate | 0.60 | 0.42 | 36 ms |
| **tuned-l16** | **0.39** | **0.62** | **18 ms** |

Pitch moves (synthetic; median beats to the new tempo, then the share of 30 s stopped):

| move | today | m2i+rate | tuned-l16 |
|---|---|---|---|
| +1 % step | 9.6 beats, 0.05 | 1.3, 0.05 | 1.3, 0.03 |
| +2 % step | 12.6, 0.09 | 2.6, 0.05 | 3.1, 0.04 |
| +3 % step | 21.1, 0.22 | 2.7, 0.05 | 3.1, 0.04 |
| +5 % step | 21.0, 0.24 | 2.7, 0.05 | 3.3, 0.04 |
| +8 % step | 16.8, 0.21 | 2.8, 0.05 | 4.3, 0.05 |
| +5 % over 8 s | 19.3, 0.30 | 1.2, 0.05 | 1.2, 0.04 |
| +8 % over 30 s | 12.0, 0.48 | 0, 0.08 | 0, 0.06 |

In short:
- tuned-l16 holds the BPM like today on steady tracks, stops less and never breaks the range.
- It locks on a wrong tempo less often than M2i (0.20 against 0.31 per track), but a little more often than today (0.15).
- In beat-less passages it stops a third less and is back on the beat sooner.
- It follows pitch moves in about 3 beats.
- Cost: on the tempo-changing tracks it is wrong 4.3 % of the time, against 1.7 % for today, because it holds the old tempo a little longer.

## 2. How many tracks really change tempo

The reference comes from the audio: madmom's offline beats are cut into flat or ramp sections by dynamic programming, and each change is checked against Beat This! final0.

| class | tracks | meaning |
|---|---|---|
| changing | 16 | both sources agree |
| probable | 46 | madmom only; Beat This has too few beats there |
| disputed | 56 | Beat This hears no change; mostly madmom switching to a 4:3 or 5:4 beat level |
| steady | 711 | |
| half-time / double-time section | 43 | an octave relation, which AUTO RANGE handles |

The real changes are section changes of 8-25 %, mostly in experimental, ambient and IDM tracks. Examples:
- Phara, Primrose Gold: 138 → 110 → 138;
- Drezo, Afterlife: 128.5 → 113.6 → 128.3;
- Rian Brazil, Hey-: 156 → 98 → 78;
- Life Sim, I.D.L: 135 → 160.

**The Charli XCX tracks (12)** are steady in tempo. What changes is the beat level:
- Everything is romantic: 75 / 150 / 75;
- forever: 175 / 88 / 175;
- party 4 u: 79, which Traktor calls 159.

There is one small intro ramp: Girl, so confusing, 124 → 126.

## 3. What goes wrong today

- **Pop and hyperpop:** the live activation is low (median 0.2 at the beats, against 0.9 in techno). The confidence sits at 0.7-0.75, just under the 0.8 gate, while the tempo is right. Everything is romantic is stopped 72 % of the time today. The weak-signal hold helps:
  - Talk talk: stopped 0.37 → 0.14;
  - forever: 0.31 → 0.11;
  - Speed Drive: 0.32 → 0.10.

  Half-time sections of pop are a limit of the tracker (DBN 90-180) and stay unfixed.
- **A regression, Sympathy is a knife:** stopped 0.13 → 0.36. The live tracker slides to the off-beat for about 40 s. The weak follower keeps the references' phase, but the strict gate stays shut against the tracker's beats.
- **Real section changes:** on Primrose Gold, M2i jumps about 7 s after the change, and the gate opens again about 7 s after that, because the old grid's lines stay in the confidence window. The fast jump and the cleared lines cut both.
- **The delay to follow a real change can't be measured on this set:** only 4 usable changes. The delay numbers come from the synthetic moves.

## 4. The proposal, part by part (dev half; `results/sweep_dev.md`)

| step | held | stopped | wrong locks | flips | beat-less stopped |
|---|---|---|---|---|---|
| m2i+rate | 0.820 | 0.136 | 0.36 | 4.28 | 0.570 |
| + rate CONFIRM 3 | 0.829 | 0.135 | 0.30 | 3.86 | 0.564 |
| + lines cleared on a jump | 0.838 | 0.121 | 0.34 | 4.13 | 0.513 |
| + fast jump (6 %) | 0.839 | 0.121 | 0.31 | 4.12 | 0.516 |
| + weak-signal hold | 0.851 | 0.109 | 0.27 | 4.03 | 0.361 |
| + LOCK_BEATS 16 (tuned-l16) | 0.850 | 0.112 | 0.21 | 3.41 | 0.373 |

`sweep_dev.md` was measured with three earlier versions of `weak.py`:
- sweep 1: weak beats filtered out of the lock;
- sweep 2: no filter;
- sweeps 3-4: a strict gate to open.

`final_*.md` uses the final version.

## 5. Proposed firmware changes

1. **`jb_rate.c`, CONFIRM 2 → 3:** fewer flips; a step is followed about 0.5 beat later.
2. **LOCK_BEATS 8 → 16** in jb_mode / jb_follow, the "hard to lock": wrong locks 0.33 → 0.20.
3. **A fast jump in jb_follow:** when 8 beats sit on one consistent grid more than 6 % away, and not an octave, jump at once.
4. **jank_conf clears the published lines on any jump.**
5. **The weak-signal hold** in jank_conf and jb_follow:
   - the gate opens strictly as today;
   - once open, lines with no evidence don't count against the grid;
   - weak beats become weak Kalman observations;
   - with no tracked beat, the follower coasts on salient activation peaks.
   - Cost (estimate): a max and a median over about 100 activation values per beat, plus a ring of about 200 values.

Not proposed: the adaptive beat jitter. It slows ramps (an 8 % ramp over 30 s is stopped 28 % of the time).

No gain was found in MANOEUVRE_SIGMA / RUN, TEMPO_REL, CHANGE_BEATS, the gate thresholds or a sticky gate, HOLD_S 60, or EVENT_TH.

## 6. Not verified

- Nothing ran on the device; the S3 costs are estimates.
- **A test-half leak:** the weak follower's last two fixes were found by tracing a test-half track. The dev half agrees in direction.
- The pitch moves are warps of the cached features, not re-rendered audio.
- The reference is two offline trackers, not hand labels.
- The beat-less passages come from the live activation, not from labels.
- madmom-derived per-track data is CC BY-NC-SA. `results/reference.md` lists per-track tempos: decide before any push.
