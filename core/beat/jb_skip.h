/* SPDX-License-Identifier: GPL-3.0-only */
/* SKIP: the follower finds its grid again when the music jumps (a skipped second, a cut, a bar the DJ nudged), without waiting for
 * the tracker's confidence to fall and rise again. A port of beat-listener's studies/follow-robust robust.py RobustBpm (update,
 * _skip_check, _phase_resync, coast, _act_resync, _tpl_update, _pattern_scores, _values) and rchain.py's `skip` blocks, on top of
 * all-d-f60 (jb_robust.h). Two settings, the configs "all-act2-c1" (ACT) and "a2-pat" (PAT) of skip.json:
 *
 *   beat path        a tracked beat further than 0.15 period off the follower's grid, `suspect` (1) beats in a row, gates the
 *                    output; two tracker beats in a row one follower period apart (+-6 %) and both off the grid by the same
 *                    amount (+-0.1 period) move the Kalman phase onto the last one (the period kept, its phase variance
 *                    (6 ms)^2, misses and outcomes cleared) and `reopen` the gate once one beat has fitted.
 *   activation path  every 50 ms while the grid is steady, not within 3 periods of the last re-sync: the activation's peak
 *                    (+-0.08 period) at the last m = 2 lines of each of 20 phase shifts of the follower's period. ACT takes the
 *                    best phase; PAT the correlation of that window with the remembered bar pattern (4 beats x 20 phases, folded
 *                    from the last bars while the grid is trusted, forgotten when the period moves 3 %), the best beat-in-bar
 *                    per phase. The grid's own phase scoring under half the best, the best a phase off it by more than 0.15 of a
 *                    period, for `confirm` ticks in a row (ACT 1, PAT 3): the Kalman phase goes to the best phase's beat (phase
 *                    variance (8 ms)^2), the gate reopens as above. `suspect` (act_run >= suspect_ticks: ACT 1, PAT 2) gates.
 *   gate             after a re-sync the gate opens once `reopen` beats have fitted, whatever the confidence; a suspect grid is
 *                    gated (jsk_gate, called from jrb_tick).
 *
 * The device sees the activation only up to now: the study's window of +-w frames around a point 30 ms before now reaches 1-2
 * frames past it at 90-130 BPM, here it ends at the newest frame (scripts/robust_golden.py CausalSkipBpm does the same, the
 * golden vectors are its). Not ported (no config uses them): the skip jump (k, ibi_tol), `clear`, `tempo_check`.
 * Cost (estimate): the activation path is 3200 multiply-adds per 50 ms in PAT (80 positions x 40 samples), a template fold per
 * bar (80 x 11 frames); the beat path a few double operations per beat. RAM: struct jb_skip about 450 bytes, internal. */
#ifndef JB_SKIP_H
#define JB_SKIP_H
#include <stdint.h>
#include "jb_follow.h"
#include "jb_weak.h"

#define JSK_STEPS 20                     /* phase shifts of a period */
#define JSK_BAR 4                        /* beats in the bar pattern */
#define JSK_MAX_M 3                      /* lines of the comb at most (the setting's m = 2) */

enum jsk_mode { JSK_OFF = 0, JSK_ACT = 1, JSK_PAT = 2 };

struct jb_robust;

struct jsk_cfg {                         /* skip.json's dict */
    int suspect;                         /* beats in a row off the grid that gate the output */
    int reopen;                          /* beats fitted after a re-sync that open the gate */
    int m;                               /* lines of the comb */
    int confirm;                         /* bad ticks in a row before the activation re-syncs */
    int suspect_ticks;                   /* act_run that gates */
    int tpl_bars;                        /* bars the pattern needs */
    uint8_t pattern;
    double win, ratio, floor, phase_tol, tempo_tol, cool, lag_s, every_s, decay, min_corr;
};

struct jb_skip {
    uint8_t mode;
    struct jsk_cfg c;
    const struct jb_robust *r;           /* the activation ring (jb_robust.h jrb_store) */
    double tracked[2];                   /* the last two beats fed (RobustBpm.tracked[-2:]) */
    uint8_t n_tracked;
    int off_run;                         /* tracked beats in a row off the follower's grid */
    uint8_t beat_suspect;
    int act_run;                         /* ticks the activation has put the best phase off the grid's */
    int reopen;                          /* beats that fitted since a re-sync; -1: not waiting */
    double act_next, act_last;
    float tpl[JSK_BAR * JSK_STEPS];      /* the bar pattern */
    uint8_t has_tpl, has_tpl_next;
    double tpl_p, tpl_next;
    int tpl_n;
    uint32_t resyncs, act_resyncs;       /* since the estimator began (the Python's) */
    uint32_t resyncs_total, act_resyncs_total, gated_ticks;    /* since boot, for `beat` */
};

void jsk_init(struct jb_skip *s, const struct jb_robust *r);
void jsk_set_mode(struct jb_skip *s, enum jsk_mode mode);       /* the dict of ACT / PAT; does not reset */
/* the estimator starts over (silence, a resync, a follower switch): the Python makes a new RobustBpm */
void jsk_reset(struct jb_skip *s);
int jsk_on(const struct jb_skip *s);
/* RobustBpm.update: a tracked beat into the weak follower, then the beat path. Returns what jw_update does. Off: jw_update. */
int jsk_update(struct jb_skip *s, struct jb_weak *w, struct jb_follow *f, double t);
/* RobustBpm.coast: the weak follower's, then the activation path while steady (act_run 0 otherwise) */
void jsk_coast(struct jb_skip *s, struct jb_weak *w, struct jb_follow *f, double now);
/* RobustBpm.suspect */
int jsk_suspect(const struct jb_skip *s);
/* rchain's skip block after the gate rule: a re-synced grid that fitted `reopen` beats opens the gate (and, with the gate's
 * confidence g at the threshold, stops waiting); a suspect grid closes it. threshold: 0..1. */
void jsk_gate(struct jb_skip *s, int has_bpm, double g, double threshold, uint8_t *gated);

#endif
