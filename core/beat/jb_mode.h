/* SPDX-License-Identifier: GPL-3.0-only */
/* The BPM estimator with modes, a port of beat-listener's eval/mode_bpm.py (ModeBpm on LockedBpm): fed one tracked beat
 * time at a time, it keeps a growing least-squares fit of beat time against beat number while the tempo holds, restarts
 * it when the tempo changes, follows a ramp with a quadratic fit, and says "stopped" when no steady beat is left.
 *
 *   steady        the growing constant-tempo fit (precision improves the longer it holds)
 *   speeding_up   the tempo is moving: the tempo is the quadratic's slope at the latest beat
 *   slowing_down
 *   stopped       no steady beat to follow: the transport stops; it runs again once LOCK_BEATS consistent beats came
 *
 * Plain C with doubles (a few beats a second: the soft-float cost does not matter), no allocation: about 1 KB. Times
 * are seconds on any clock that does not jump. The Python and this agree to rounding (tests/test_beat.c). */
#ifndef JB_MODE_H
#define JB_MODE_H
#include <stdint.h>

#define JB_DQ_MAX 32
/* LOCK_BEATS: consistent beats in a new segment before the transport runs again, and the beats a jump or the follower's
 * fast jump needs on one grid (ModeBpm._recent_consistent). 16 since M2j (beat-listener studies/tempo-changes tuned-l16:
 * "hard to lock", wrong-tempo locks 0.33 -> 0.20 a track); M2i and the Python's default had 8. Per estimator (lock_beats)
 * so the golden vectors of either can be replayed; at most the recent beats kept (16). */
#define JB_LOCK_BEATS 16

enum jb_mode_kind { JB_STOPPED, JB_STEADY, JB_SPEEDING_UP, JB_SLOWING_DOWN };

struct jb_dq {                          /* a deque of times with a maximum length, oldest first */
    double v[JB_DQ_MAX];
    uint8_t n, cap;
};

struct jb_mode {
    /* the growing fit: running least squares of t = t0 + period k */
    int fit_n;
    double sk, st, skk, skt;
    struct jb_dq recent;                /* the last 16 beats */
    struct jb_dq history;               /* the last 32 beats, for the mode decision */
    double k_last, t_last;
    uint8_t has_t_last;
    int disagree, misses, restarts;
    int miss_limit;
    enum jb_mode_kind mode;
    double ramp_period, ramp_last;
    uint8_t has_ramp_period;
    int exit_run;
    double last_fit_time;
    uint8_t has_last_fit;
    int jumps, in_a_row, stuck;
    int last_jump_beat, last_jump_dir;
    uint8_t has_last_jump;
    int beats_seen;
    int lock_beats;                     /* LOCK_BEATS (JB_LOCK_BEATS by jb_mode_init) */
};

void jb_mode_init(struct jb_mode *m);
void jb_mode_update(struct jb_mode *m, double t);
/* The mode at time now: with no fitted beat for 30 s it is stopped. */
enum jb_mode_kind jb_mode_at(struct jb_mode *m, double now);
/* The published BPM at now: 0 while stopped or before a tempo is known, else 1. */
int jb_mode_bpm_at(struct jb_mode *m, double now, double *bpm);
/* The published beat grid: a beat at *t_beat, the next ones every *period seconds. 0 when there is none. */
int jb_mode_grid(const struct jb_mode *m, double *t_beat, double *period);

/* The pieces the follower (jb_follow.h, eval/follow_bpm.py) calls on its ModeBpm: */
void jb_dq_push(struct jb_dq *q, double t);            /* deque.append: the oldest goes when full */
double jb_short_period(const double *b, int n);        /* locked_bpm.short_period; < 0 for None */
void jb_mode_restart(struct jb_mode *m);                /* LockedBpm._restart: a new segment from the recent beats */
int jb_mode_recent_consistent(const struct jb_mode *m); /* ModeBpm._recent_consistent */
double jb_mode_period(const struct jb_mode *m);         /* ModeBpm.period; < 0 for None */
int jb_mode_line(const struct jb_mode *m, double *t_beat, double *period);   /* LockedBpm.grid: the fit's line */

#endif
