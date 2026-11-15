/* SPDX-License-Identifier: GPL-3.0-only */
/* The tempo follower (advanced beat follow, M2i): a port of beat-listener's eval/follow_bpm.py FollowBpm, which sits on
 * ModeBpm (jb_mode.h) and glides its steady grid with the music.
 *
 * ModeBpm does the first lock, the hold through breaks, the stop and the ramps it decides on before a lock. Once it is
 * steady, a 3-state Kalman filter (the time of the last beat on the line, the period, the drift: the period's change a
 * beat) takes over the grid: each tracked beat within a quarter period of the prediction pulls it (missed beats bridged
 * by rounding); 3 innovations in a row on one side past 3 standard deviations are a manoeuvre (a pitch move: the
 * covariance opens, so the next beats pull the grid onto the new tempo in a few beats with the phase kept, no restart);
 * a second one the same way within 24 beats opens the drift too (a ramp). A run of 12 misses with the recent beats on a
 * grid of their own is a jump (a cut: a new segment), and the recent beats' own tempo 4 % away for 8 beats as well (the
 * guard against a lock on a 4:3 of the music, whose every third beat fits and never misses enough to jump).
 *
 * The playback rate measured from the spectrum (jb_rate.h) comes in as a proposal: held until the next tracked beat and
 * taken only if that beat lies closer to the grid re-timed by it than to the grid as it is (the spectral match fires on
 * chord changes too), and only on a trusted grid (16 of the last 32 beats or more, 80 % of them fitted).
 *
 * Plain C with doubles (a beat costs a few hundred double operations, ModeBpm's own work aside), no allocation:
 * sizeof struct jb_follow is about 1 KB, ModeBpm's included. Times are seconds on the tracker's stream clock. The
 * Python and this agree to rounding (tests/test_follow.c against tests/follow_ref, written by beat-listener's
 * scripts/follow_golden.py). FollowBpm's `sticky` (the same-music detector, dropped in stage 1) is not ported. */
#ifndef JB_FOLLOW_H
#define JB_FOLLOW_H
#include <stdint.h>
#include "jb_mode.h"

#define JF_TRUST_BEATS 32

enum jb_follow_state { JF_STOPPED = 0, JF_LOCK = 1, JF_GLIDE = 2 };   /* no tempo / ModeBpm's own grid / the Kalman glide */

struct jb_follow {
    struct jb_mode m;                   /* ModeBpm: frozen while the glide runs, but for the beats, misses and jumps */
    uint8_t has_kf;
    double x[3];                        /* the glide: time of the last beat on the line, period, drift (s a beat) */
    double c[3][3];                     /* its covariance */
    int run;                            /* innovations in a row past the manoeuvre threshold, signed by their side */
    int manoeuvres;                     /* since the last seed (the Python's kf.manoeuvres) */
    int last_side;                      /* the last manoeuvre's side ... */
    int since;                          /* ... and the beats since it */
    int follow_beats;                   /* beats the glide fitted */
    uint8_t out[JF_TRUST_BEATS];        /* the last beats: fitted 1 or missed 0 (a ring) */
    uint8_t out_n, out_head;
    uint8_t has_pending, pending_has_since;
    double pending_ratio, pending_t, pending_since;
    /* totals since the start, for the stats */
    uint32_t seeds, manoeuvres_total, ratios_taken, ratios_refused, ratios_ignored;
    double last_ratio;                  /* the last ratio taken; 1 before any */
};

void jb_follow_init(struct jb_follow *f);
void jb_follow_update(struct jb_follow *f, double t);
/* A playback-rate change measured at time t (the spectrum moved by ratio, a step estimated to begin at since when
 * has_since): held until the next beat (FollowBpm.propose_ratio). A newer proposal replaces one not yet judged. */
void jb_follow_propose(struct jb_follow *f, double ratio, double t, int has_since, double since);
/* The mode (jb_mode_at), the published BPM (0 while stopped or before a tempo), the published grid (0 when none). */
enum jb_mode_kind jb_follow_at(struct jb_follow *f, double now);
int jb_follow_bpm_at(struct jb_follow *f, double now, double *bpm);
int jb_follow_grid(const struct jb_follow *f, double *t_beat, double *period);
/* the state for the reports: stopped, lock (ModeBpm's grid) or glide */
enum jb_follow_state jb_follow_state(struct jb_follow *f, double now);
/* the glide's drift as parts per million of the period a beat (+: slowing down); 0 without the glide */
int32_t jb_follow_drift_ppm(const struct jb_follow *f);
/* FollowBpm.trusted: the grid fitted most of the last beats */
int jb_follow_trusted(const struct jb_follow *f);

#endif
