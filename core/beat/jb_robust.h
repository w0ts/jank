/* SPDX-License-Identifier: GPL-3.0-only */
/* The robust follower, all-d-f60 (M2m): a port of beat-listener's studies/follow-robust (rchain.py, robust.py, comb.py,
 * dbn.py's tempo prior) on top of the tuned follower (jb_weak.h). Its four additions, each as the Python:
 *
 *   coast       once the outputs run, a confidence under the gate no longer stops them while the follower holds a tempo, the
 *               agreement is at least JRB_COAST_FLOOR (0.6) and no jump of the follower came in this step (a jump needs the
 *               strict gate again). (The 30 s hold is the weak follower's own: JW hold_s 30 as M2j.)
 *   memory      the last confident tempo (the outputs running, the strict line confidence at the threshold, the follower
 *               gliding, no jump this step) is remembered for JRB_MEM_S (120 s). While the outputs are not confident the DBN
 *               carries a tempo prior around it and its octaves (jb_dbn_set_prior), and a lock or a jump to a tempo at 3:4 /
 *               4:3 / 2:3 / 3:2 of it waits for JRB_RELATED_BEATS (48) consistent beats.
 *   ratio       before any lock or jump the activation's comb at the new period must beat its relatives' by JRB_RATIO_MARGIN
 *               (0.05) over the last 10 s.
 *   agreement   the gate's confidence is the mean of the line confidence, the comb's tempo agreement, the phase agreement and
 *               the DBN's mass at the follower's tempo (the last three refreshed every 0.5 s); the gate opens at the threshold
 *               + 5 points and closes under the threshold.
 *
 * The activation of the last 1024 frames is kept (jrb_store, 8 KB with the curve's scratch: PSRAM on the device). The lock and
 * jump gate is jb_mode's `gate` hook (jb_robust_attach). Doubles for the decisions, float for the activation sums. */
#ifndef JB_ROBUST_H
#define JB_ROBUST_H
#include <stddef.h>
#include <stdint.h>
#include "jb_comb.h"
#include "jb_dbn.h"
#include "jb_follow.h"
#include "jb_weak.h"

struct jb_skip;

#define JRB_ACT_RING 1024
#define JRB_COAST_FLOOR 0.6
#define JRB_MEM_S 120.0
#define JRB_PRIOR_WIDTH 0.04
#define JRB_PRIOR_FLOOR 0.1
#define JRB_RELATED_BEATS 48
#define JRB_REL_TOL 0.04
#define JRB_MAX_ALIGN_BEATS 64          /* the beats the consistency check reads at most (JW seen: 256) */
#define JRB_RATIO_MARGIN 0.05
#define JRB_RATIO_SPAN 1000             /* frames (10 s) */
#define JRB_AGREE_SPAN 800              /* frames (8 s) */
#define JRB_AGREE_EVERY_S 0.5
#define JRB_DBN_NEAR 0.04
#define JRB_HYST 0.05                   /* the gate opens again this far above the threshold (engine.HYSTERESIS) */

struct jrb_store {                      /* PSRAM on the device */
    float ring[JRB_ACT_RING];           /* frame k at k % JRB_ACT_RING */
    float x[JRB_ACT_RING];              /* the curve's mean-removed window */
};

struct jb_robust {
    struct jrb_store *st;               /* NULL: off */
    int64_t act_n;                     /* frames pushed */
    struct jb_follow *f;               /* the gate's context */
    struct jb_weak *w;
    struct jb_skip *sk;                /* SKIP (jb_skip.h): its gate rule runs inside jrb_tick; NULL: off */
    uint8_t has_mem;
    double mem_bpm, mem_t;
    double next_t;                     /* the agreement's next refresh */
    double v_tempo, v_phase, v_dbn;
    int refused_run;                   /* refusals in a row (LockGate.refused_run; none given up on: ratio give_up None) */
    float prior[JB_DBN_MAX_INTERVALS]; /* the tempo prior of prior_for */
    double prior_for;
    uint8_t has_prior_cache;
    /* totals since the start, for `beat` */
    uint32_t asked, refused_memory, refused_ratio, coasting_ticks, prior_ticks;
    /* the cost counters (the device sets `clock`: a cycle counter; NULL: none): the work since the last jrb_take_cycles */
    uint32_t (*clock)(void);
    uint32_t refresh_cyc, refresh_max, gate_cyc, gate_max, tick_cyc;
};
/* the cycles spent since the last call, and the longest refresh / gate question in them; clears them */
struct jrb_cycles { uint32_t refresh, refresh_max, gate, gate_max, tick; };
struct jrb_cycles jrb_take_cycles(struct jb_robust *r);

struct jrb_in {
    double now;
    int silent;
    int has_bpm;
    double bpm;                        /* the follower's tempo (unfolded) */
    int has_grid;
    double period;                     /* the published grid's period (s) */
    double conf;                       /* the gate's input as M2j: jcf_weak_pick(strict, weak, gated, was_running) */
    double strict;                     /* the strict line confidence */
    int has_kf;                        /* the follower glides */
    int jumped;                        /* a jump of the follower came in this step (its beats, its fast jump) */
    int was_running;
    const double *lines;               /* the published lines, ascending (jank_conf.h) */
    size_t n_lines;
    uint8_t threshold_pct;             /* the user's THRESHOLD; 0 or keep_running: never gated */
    int keep_running;
};

struct jrb_out {
    double gconf;                      /* the agreement confidence */
    uint8_t gated, run, coasting, confident, prior_on;
};

void jrb_init(struct jb_robust *r, struct jrb_store *st);
int jrb_on(const struct jb_robust *r);
/* the estimator starts over (silence): the gate's run of refusals goes; the memory stays */
void jrb_reset(struct jb_robust *r);
/* hook the lock and jump gate into the follower (f->m.gate); call after every jb_follow_init */
void jrb_attach(struct jb_robust *r, struct jb_follow *f, struct jb_weak *w);
/* the network's activation of frame index (in order) */
void jrb_act(struct jb_robust *r, int64_t index, float act);
/* The gate (jb_mode's hook): 1 allows. kind JB_GATE_LOCK / JB_GATE_JUMP. */
int jrb_gate(void *ctx, int kind, double now, double period);
/* The tick after the beats and the coast (rchain._replay's: the agreement, the gate, the memory, the DBN's prior). *gated: the
 * gate's state, in and out. */
void jrb_tick(struct jb_robust *r, struct jb_dbn *dbn, const struct jrb_in *in, uint8_t *gated, struct jrb_out *out);

/* the comb's tempo agreement now (the ring's last JRB_AGREE_SPAN frames against period, s): 0 with no ring or too little activation */
double jrb_tempo_agreement(const struct jb_robust *r, double now, double period);

/* the pieces, for the tests */
int jrb_related(double bpm, double mem_bpm, double tol);                 /* robust.related */
int jrb_consistent(const double *beats, int n_all, double period, int n); /* robust.consistent */
void jrb_tempo_prior(const struct jb_dbn *d, double mem_bpm, double width, double floor, float *w);

#endif
