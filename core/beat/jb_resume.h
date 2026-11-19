/* SPDX-License-Identifier: GPL-3.0-only */
/* RESUME: back on the beat at once when the music plays again after a pause (the user, 2026-11-19: "when the sound
 * resumes, even after a long break ... we remember the last played sound and if the song just resumed, with our cross
 * correlation, we resync almost instantly"; "a pause can resume the music in a completely different phase, so we need to
 * find the new phase ... one second, 10 seconds, 10 minutes or even 2 years").
 *
 *   remember  while the outputs run on a trusted glide: the period, and the SHAPE of the song's beat: the network's
 *             activation folded at the period from the published lines (JRE_BINS points a beat, JRE_LINES beats, a slow
 *             mean): which peak is the beat, and the off-beat's (hats, an off-beat bass) beside it. No time limit.
 *   arm       silence or near silence (jank_conf.h's gate, jb_lost.h's QUIET) stopped the outputs: the old phase is
 *             thrown away. While armed, neither the gate nor LOST's own return brings the old grid back (jre_holds),
 *             until RESUME finds the song or the follower locks or jumps on a grid of its own (the music changed).
 *   find      at each step the last JRE_LINES periods of activation, folded at the remembered period, are cross-correlated
 *             with the shape over all JRE_BINS shifts: the best shift is the new phase when its Pearson r reaches
 *             JRE_CORR (its beat looks like the song's) and JRE_SOUND of the periods, the newest among them, have a peak of
 *             JRE_PEAK. (The shape is a weak same-song check: songs of one genre look alike; the tempo must match too.)
 *   resync    the follower is seeded on the newest beat with the remembered period (jre_seed: ModeBpm locks on the
 *             remembered grid fed as beats, so the glide starts there as on any lock); the caller opens the gate, and
 *             holds the DBN's tempo prior on the remembered tempo for a while (after a pause its own tempo drifts).
 * A different song at nearly the same tempo whose beat looks alike is taken too: LOST's off-grid rule stops it within 2
 * bars.
 *
 * Portable C, no allocation. RAM: struct jb_resume, ~110 bytes. Cost: per step 80 activation reads (remember or find) and,
 * when finding, 20 correlations of 20 points. */
#ifndef JB_RESUME_H
#define JB_RESUME_H
#include <stdint.h>
#include "jb_follow.h"

#define JRE_BINS 20                    /* points of the beat's shape */
#define JRE_LINES 4                    /* periods folded */
#define JRE_LEARN 0.1                  /* the shape's mean, per step */
#define JRE_CORR 0.6                   /* Pearson r of the window against the shape */
#define JRE_PEAK 0.2                   /* a period's highest point to count as sound (the activation, 0..1) */
#define JRE_SOUND 4                    /* periods of the window with a peak, the newest among them */
#define JRE_FOLD_PEAK 0.15             /* the folded window's highest point: the peaks line up at one phase */
#define JRE_PRIOR_S 8.0                /* the DBN's tempo prior held this long after a resync */

struct jb_resume {
    double period;                     /* the remembered period, s; valid when has */
    float shape[JRE_BINS];             /* the beat's shape, bin 0 on the line */
    uint8_t has, armed;
    uint32_t mark;                     /* the follower's seeds + jumps when armed (jre_holds) */
    float last_r;                      /* the last correlation tried */
    uint32_t resumes;                  /* since the init */
};

void jre_init(struct jb_resume *r);
/* the outputs run on a trusted glide: its period and published line t_line (s); the activation ring (frame k at k % cap,
 * frames up to act_n - 1 in) for the shape */
void jre_remember(struct jb_resume *r, double period, double t_line, const float *ring, uint32_t cap, int64_t act_n);
/* silence or near silence stopped the outputs (nothing without a remembered tempo); mark: the follower's seeds + jumps */
void jre_arm(struct jb_resume *r, uint32_t mark);
/* 1 while armed and the follower has not locked or jumped since (mark unchanged): the old grid may not come back */
int jre_holds(const struct jb_resume *r, uint32_t mark);
/* the outputs run again (or the tracker restarted): stop watching */
void jre_disarm(struct jb_resume *r);
/* while armed: 1 when the song is back (then disarmed), *t_line its newest beat (s) */
int jre_find(struct jb_resume *r, const float *ring, uint32_t cap, int64_t act_n, double *t_line);
/* the follower seeded on t with the period: its ModeBpm started afresh (its gate, jumps and lock length kept) and fed the
 * grid ending at t as beats */
void jre_seed(struct jb_follow *f, double t, double period);

#endif
