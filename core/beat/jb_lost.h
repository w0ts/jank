/* SPDX-License-Identifier: GPL-3.0-only */
/* LOST: when the tuned follower's outputs stop, once they run.
 *
 * The user's rule (2026-11-19, after the first LOST stopped his tracks with the kick plainly there): "if this looks like
 * the same song but there is no beat, try to stick to it" (drops of 16-32 beats); stop on silence or near silence for more
 * than 2 beats; beats at another tempo: match them (the follower's own jumps and rate) or stop; optionally a new song with
 * a beat-less intro stops too (jb_song.h). The first LOST (2026-11-18) stopped after a bar without a strong beat.
 *
 * Each line of the published grid is judged once the frames up to a period after it are in:
 *   FIT     a strong beat (jb_weak.h's strong test: the network's activation) or a kick (jb_kick.h: the band under
 *           120 Hz) within JL_TOL of a period of it
 *   OFF     no fit, and a strong beat between JL_TOL and half a period from it (beats off the grid)
 *   QUIET   the input's level over the line's period under JL_QUIET_ABS dBFS, or JL_QUIET_REL dB under the running
 *           level of the fitted lines
 *   (none of these: a beat-less line of the song: it HOLDS, nothing counts)
 * LOST (while the outputs run): JL_QUIET_LINES quiet lines in a row; or JL_OFF_LINES off lines since the last fit (the
 * follower did not take the new phase or tempo: its jumps and the spectral rate had those beats); or, with the new-song
 * check on, jb_song's NEW while the last JL_SONG_LINES lines had no fit. A new grid (a lock, a jump) is judged afresh.
 * Back: JL_BACK_LINES fits in a row with sound (the outputs run again at once, as any gate opening).
 *
 * Measured offline (the firmware's C tracker, these modules and beat.c's chain from the audio of the user's 795 Traktor
 * tracks, threshold 55, docs/beat.md): drops of the same song held through 25 % (the first LOST) -> 69 %; stops on a clear
 * beat 10.6 -> 2.4 an hour; clear-beat time stopped 4.5 % -> 0.3 %; back after a stop 4.2 -> 0.2 s (median); running off
 * Traktor's grid while the beat plays 3.8 % -> 5.9 % (LOST off: 5.0 %). Rhyw - Driptych: no stop in the music (6
 * before); Gary Beck - Mango Circus: one, where the follower's grid sits 0.17 s off the kick.
 *
 * Portable C, no allocation; per line judged a search of the strong beats and jb_kick's window. */
#ifndef JB_LOST_H
#define JB_LOST_H
#include <stddef.h>
#include <stdint.h>
#include "jb_kick.h"

#define JL_QUIET_LINES 2               /* the user: "more than n beats ... start with 2" */
#define JL_QUIET_ABS -55.0             /* dBFS */
#define JL_QUIET_REL 30.0              /* dB under the fitted lines' level */
#define JL_LEVEL_EMA 0.05              /* the fitted lines' running level */
#define JL_OFF_LINES 8                 /* two bars of beats off the grid */
#define JL_SONG_LINES 4                /* lines without a fit before a new song counts */
#define JL_BACK_LINES 3
#define JL_TOL 0.1                     /* periods (jank_conf.h JCF_TOL) */
#define JL_LAG_S 0.05                  /* (jb_weak.h JW_LAG_S) a line is judged once now >= line + period + this */
#define JL_SEED_S 2.0                  /* a new grid: its lines judged from this far back */

enum jl_cause { JLO_NONE = 0, JLO_QUIET = 1, JLO_OFF = 2, JLO_SONG = 3 };

struct jb_lost {
    double next_line;                  /* the next line to judge; < 0: none (no grid) */
    double level;                      /* the fitted lines' running level, dBFS; JBK_NONE: none yet */
    uint8_t lost, cause;               /* enum jl_cause of the current LOST */
    uint8_t fit_run, quiet_run, off_n, unfit_run;   /* capped */
    uint8_t hold_back;                 /* the caller holds the return (jl_hold_back) */
    uint32_t losts[4];                 /* LOST begun since the init, by cause */
};

void jl_init(struct jb_lost *l);
/* One step. strong: the strong beats, ascending (jb_weak.h jw_strong, n_strong). kick: the kick band and the level (NULL:
 * none: no kick evidence, nothing quiet). has_grid / t0 / period: the published grid (jb_follow_grid) while the follower
 * gives a tempo. running: the outputs ran at the last step. song_new: jb_song's NEW (0 when the option is off). Returns 1
 * while LOST (the caller closes the gate); *back (may be NULL) is 1 at the step LOST ended (the caller opens it). */
int jl_step(struct jb_lost *l, const double *strong, size_t n_strong, const struct jb_kick *kick, int has_grid, double t0,
            double period, double now, int running, int song_new, uint8_t *back);
/* LOST's own return held back (RESUME's match comes first, jb_resume.h jre_holds): 1 while held */
static inline void jl_hold_back(struct jb_lost *l, int hold) { l->hold_back = (uint8_t)(hold != 0); }
/* silence, a restart: no lines, not lost, the level forgotten */
void jl_clear(struct jb_lost *l);

#endif
