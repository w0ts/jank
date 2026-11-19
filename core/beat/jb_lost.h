/* SPDX-License-Identifier: GPL-3.0-only */
/* LOST: stop when the follower does not know (the user, 2026-11-18: "it should stop sooner when it does not know").
 *
 * Why the gate alone keeps running: once the outputs run, the weak confidence (jank_conf.h jcf_weak_confidence) counts a
 * line as hit when any tracked beat is near it, and in a breakdown the online DBN keeps emitting beats from its own tempo
 * model, right on the lines: the confidence stays near 100 % with no beat in the music, and the follower gives up only
 * after 30 s without fits.
 *
 * The rule. Each line of the published grid, once the frames up to a period after it are in, has EVIDENCE when a strong
 * beat (jb_weak.h's strong test: the network's activation, not the DBN's tempo model) lies within JL_TOL of a period of
 * it. JL_LOST_LINES lines in a row without evidence while the outputs run: LOST, the gate closes. JL_BACK_LINES evidenced
 * lines in a row on the grid end LOST: the outputs run again at once (from the next bar line, as any gate opening), on the
 * grid the follower held.
 *
 * Measured offline (the firmware's C tracker and this chain on the user's 795 Traktor tracks, 43 h of clear beat, ~1900
 * passages of 8+ grid beats with no audible beat, median 9.7 s): median stop 29 s -> 2.7 s; stopped within 4 s 17 % ->
 * 63 %; never stopped before the beat came back 48 % -> 24 % (mostly breakdowns with an occasional kick on the grid);
 * back on the beat 2.6 s -> 3.3 s after it returns. Cost: stops in short fills (a 1-bar kick drop) 9.4 -> 14.0 an hour
 * of music; stops on a clear beat 6.0 -> 3.8 an hour. (Small salient peaks as evidence never fired: hats in breakdowns;
 * "at most 2 of the last 8 lines" doubled the stops on half-time beats.)
 *
 * Portable C, no allocation; per step a search of the strong beats for each line judged (one or two). */
#ifndef JB_LOST_H
#define JB_LOST_H
#include <stddef.h>
#include <stdint.h>

#define JL_LOST_LINES 4                /* one bar without a strong beat on the grid */
#define JL_BACK_LINES 4                /* strong beats on the grid in a row to run again */
#define JL_TOL 0.1                     /* periods (jank_conf.h JCF_TOL) */
#define JL_LAG_S 0.05                  /* (jb_weak.h JW_LAG_S) a line is judged once now >= line + period + this */
#define JL_SEED_S 2.0                  /* a new grid: its lines judged from this far back */

struct jb_lost {
    double next_line;                  /* the next line to judge; < 0: none (no grid) */
    uint8_t lost;
    uint8_t miss_run, ev_run;          /* lines in a row without / with evidence (capped) */
    uint32_t losts;                    /* times LOST began, since the init */
};

void jl_init(struct jb_lost *l);
/* One step. strong: the strong beats, ascending (jb_weak.h jw_strong, n_strong). has_grid / t0 / period: the published
 * grid (jb_follow_grid) while the follower gives a tempo. running: the outputs ran at the last step. Returns 1 while
 * LOST (the caller closes the gate); *back (may be NULL) is 1 at the step LOST ended (the caller opens the gate). */
int jl_step(struct jb_lost *l, const double *strong, size_t n_strong, int has_grid, double t0, double period, double now,
            int running, uint8_t *back);
/* silence, a restart: no lines, not lost */
void jl_clear(struct jb_lost *l);

#endif
