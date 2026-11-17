# Where the 1 is when jank starts (AUTO BAR AT START, START WAIT)

80 Traktor test-half tracks (every 5th from #0), the live estimator (live/downbeat.py) streamed on FollowBpm's grid; the start's 1 against the Traktor reference (majority over the 32 beats after it). Run 2026-11-16, 185 s.

## LOCK_BEATS 8 (M2i)

| START WAIT (tracked beats) | placed % | right % of placed | one off % | **two off (the 3) %** | three off % | unjudged (tracks) | unplaced (arbitrary bar) % | median s tempo -> start |
|---|---|---|---|---|---|---|---|---|
| 8 | 80 | 67 | 16 | **10** | 8 | 12.2 | 20 | 4.6 |
| 12 | 96 | 67 | 16 | **10** | 8 | 14.0 | 4 | 3.9 |
| 16 | 97 | 67 | 16 | **10** | 8 | 14.5 | 3 | 3.9 |
| 32 | 98 | 67 | 16 | **10** | 8 | 15.0 | 3 | 3.9 |

The estimator's first lock (any time): 79 tracks, judged 64: right 43, one off 10, **two off 6**, three off 5; median 3.9 s after the tempo.

## LOCK_BEATS 16 (M2j)

| START WAIT (tracked beats) | placed % | right % of placed | one off % | **two off (the 3) %** | three off % | unjudged (tracks) | unplaced (arbitrary bar) % | median s tempo -> start |
|---|---|---|---|---|---|---|---|---|
| 8 | 80 | 69 | 11 | **9** | 11 | 8.2 | 20 | 4.7 |
| 12 | 96 | 70 | 10 | **9** | 10 | 10.0 | 4 | 4.0 |
| 16 | 96 | 70 | 10 | **9** | 10 | 10.0 | 4 | 4.0 |
| 32 | 96 | 70 | 10 | **9** | 10 | 10.0 | 4 | 4.0 |

The estimator's first lock (any time): 80 tracks, judged 69: right 49, one off 7, **two off 6**, three off 7; median 4.0 s after the tempo.
