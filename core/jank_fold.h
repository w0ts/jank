/* SPDX-License-Identifier: GPL-3.0-only */
/* AUDIO mode's FOLD (HOME's UP x2 / DOWN /2, the control line's `fold`) and AUTO RANGE, as the Python player's engine
 * (beat/live/engine.py _folded, FOLDS 0.25..4): Link gets the tracked tempo times the fold, and a grid of that period.
 *
 *   x2, x4   every tracked beat stays a beat of the folded grid, with one (three) beats between: beat = the tracked
 *            beat, period / fold. The session's count goes on from the moment of the change (Link's own tempo change).
 *   /2, /4   every 2nd (4th) tracked beat is a beat. Which ones is chosen once, when the fold is set, against the output
 *            grid (the Link session's): the candidate whose place on it is nearest a multiple of K output beats, K the
 *            folded period in output beats. A user's /2 at the same tempo (K = 2): the beats counted 1 and 3 stay (the
 *            bar's 1 stays a beat). An AUTO RANGE /2 that undoes a tracker gone to double time (K = 1): the tracked
 *            beats that are on the output's beats, not between them, so the count goes on unchanged. The choice is kept
 *            from beat to beat (an anchor, moved along with each tracked beat, so nothing adds up).
 *   AUTO BAR makes no move while the fold is not 1 (the Python stage 2's rule): the downbeat estimator counts tracked
 *            beats, which are then not the session's; it keeps running (no reset) and moves again at fold 1.
 *   Not stored: 1 at a start, at a resync (Start after Stop, a new stream) and when AUDIO mode ends.
 *
 * AUTO RANGE: when a NEW tempo comes (a lock after no tempo, a track change, or a tempo jump of the tracker, an octave
 * flip within a track included), the fold becomes the one that brings the new tempo closest (in octaves) to the
 * reference, when that lands within JFO_RANGE_TOL of it: the Link session's tempo while it plays, else the last tempo jank
 * put out if that was less than JFO_RECENT_MS ago. Not within it (a real change: 128 -> 90 is not 180) or no reference:
 * the tempo is followed at fold 1 (a user's fold kept). So a tracker that flips 128 -> 64 within a track gets x2 and the
 * output stays 128, phase and count included (x2 keeps every tracked beat on the grid; /2 picks the on-beats above);
 * 128 -> 132 or 135 is followed as is. A user's UP / DOWN wins until the next new tempo, which is then ranged against
 * the tempo the user chose (it is the output). */
#ifndef JANK_FOLD_H
#define JANK_FOLD_H
#include <stdint.h>

#define JFO_EXP_MIN (-2)                /* 0.25 */
#define JFO_EXP_MAX 2                   /* 4 */
#define JFO_RECENT_MS 120000u           /* a last output tempo counts as the reference this long: a pause between tracks or a
                                           short break; after that the next music may be another style */
#define JFO_RANGE_TOL 0.06              /* an octave away within 6 %: a flip (the tracker's own error is far under 1 %) or the
                                           next track of a mix (a DJ's pitch range, mostly within +-6 %); 2^(+-1) x 1.06
                                           is never a plain change under ~40 %, which is followed */

struct jfo {
    int8_t exp;                         /* the fold is 2^exp */
    uint8_t manual;                     /* the user chose it (UP / DOWN) since the last new tempo */
    uint8_t anchored;                   /* /2, /4: the anchor is set */
    int64_t anchor_us;                  /* a tracked beat that is a beat of the folded grid */
    double out_bpm;                     /* the last tempo put out (folded), 0: none yet */
    uint32_t out_ms;
    uint8_t had_tempo;                  /* the tracker gave a tempo at the last step */
    int32_t jumps;                      /* the tracker's tempo jumps counted at the last step */
    uint32_t autos, manuals;
};

void jfo_init(struct jfo *f);
/* the user: dir +1 x2, -1 /2 (within 0.25..4), 0 back to 1. Returns the new exp */
int jfo_user(struct jfo *f, int dir);
/* a resync, or AUDIO mode ended: fold 1 (the memory of the last output tempo stays) */
void jfo_reset(struct jfo *f);
static inline double jfo_factor(const struct jfo *f) { return f->exp >= 0 ? (double)(1 << f->exp) : 1.0 / (double)(1 << -f->exp); }
/* "1" "2" "4" "0.5" "0.25" */
const char *jfo_name(int exp);

/* the exp in JFO_EXP_MIN..MAX that puts bpm x 2^exp closest to ref in octaves (ties: the smaller fold) */
int jfo_best(double bpm, double ref);
/* AUTO RANGE's choice: jfo_best when bpm x 2^best is within JFO_RANGE_TOL of ref, else 0 */
int jfo_range(double bpm, double ref);

/* each step of the tracker: have = it gives a tempo (bpm, tracked, unfolded), jumps = its tempo jumps so far;
 * session_bpm > 0 while the Link session plays. Applies AUTO RANGE on a new tempo and remembers the output tempo.
 * Returns 1 when the fold changed */
int jfo_step(struct jfo *f, int have, double bpm, int32_t jumps, double session_bpm, uint32_t now_ms);

/* the folded grid from a tracked beat t (us) and the tracked period p (us). For /2, /4 the first call after a change
 * anchors against the output grid: pos_at_t = its place in output beats at t, out_period its period (us); have_out 0
 * (no session): t itself */
void jfo_grid(struct jfo *f, int64_t t, double p, int have_out, double pos_at_t, double out_period, int64_t *beat, double *period);
#endif
