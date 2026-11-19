/* SPDX-License-Identifier: GPL-3.0-only */
#include "jb_lost.h"
#include <math.h>

void jl_init(struct jb_lost *l)
{
    for (int i = 0; i < 4; i++)
        l->losts[i] = 0;
    l->pause_db = JL_PAUSE_DB;
    jl_clear(l);
}

void jl_clear(struct jb_lost *l)
{
    l->next_line = -1.0;
    l->lost = l->cause = 0;
    l->fit_run = l->quiet_run = l->off_n = l->unfit_run = 0;
    l->hold_back = 0;
}

static uint8_t inc(uint8_t v) { return v < 255u ? (uint8_t)(v + 1u) : v; }

/* a strong beat within `margin` of the line (fit), or else between margin and half a period (off) */
static void near_line(const double *s, size_t n, double line, double p, int *fit, int *off)
{
    *fit = *off = 0;
    for (size_t i = n; i-- > 0;) {
        const double d = fabs(s[i] - line);
        if (s[i] < line - 0.5 * p)
            break;
        if (d <= JL_TOL * p) {
            *fit = 1;
            return;
        }
        if (d <= 0.5 * p)
            *off = 1;
    }
}

static void judge_line(struct jb_lost *l, const double *strong, size_t ns, const struct jb_kick *kick, double line, double p)
{
    int fit, off;
    near_line(strong, ns, line, p, &fit, &off);
    if (!fit && kick && jbk_on_line(kick, line, p, JL_TOL))
        fit = 1;
    const double lv = kick ? jbk_line_level(kick, line, p) : JBK_NONE;
    const int known = lv > JBK_NONE + 1.0;
    const int quiet = known && lv < l->pause_db;
    if (fit) {
        l->fit_run = inc(l->fit_run);
        l->off_n = 0;
        l->unfit_run = 0;
    } else {
        l->fit_run = 0;
        l->unfit_run = inc(l->unfit_run);
        if (off)
            l->off_n = inc(l->off_n);
    }
    l->quiet_run = quiet ? inc(l->quiet_run) : 0;
}

static void judge(struct jb_lost *l, const double *strong, size_t ns, const struct jb_kick *kick, int has_grid, double t0,
                  double p, double now)
{
    if (!has_grid || !(p > 0.0)) {
        l->next_line = -1.0;
        return;
    }
    if (l->next_line < 0.0 || fabs(remainder(l->next_line - t0, p)) > 0.25 * p) {
        /* a new grid (a lock, a jump): its first line after the last one judged, judged afresh */
        const double k = ceil((fmax(l->next_line, now - JL_SEED_S) - t0) / p - 0.25);
        l->next_line = t0 + k * p;
        l->fit_run = l->off_n = 0;
    } else
        l->next_line = t0 + p * round((l->next_line - t0) / p);     /* the glide moved the grid a little */
    while (l->next_line + p + JL_LAG_S <= now) {
        judge_line(l, strong, ns, kick, l->next_line, p);
        l->next_line += p;
    }
}

int jl_step(struct jb_lost *l, const double *strong, size_t n_strong, const struct jb_kick *kick, int has_grid, double t0,
            double period, double now, int running, int song_new, uint8_t *back)
{
    uint8_t b = 0;
    judge(l, strong, n_strong, kick, has_grid, t0, period, now);
    if (l->lost) {
        if (l->fit_run >= JL_BACK_LINES && l->quiet_run == 0 && !l->hold_back) {
            l->lost = 0;
            l->cause = JLO_NONE;
            b = 1;
        }
    } else if (running) {
        uint8_t c = JLO_NONE;
        if (l->quiet_run >= JL_QUIET_LINES)
            c = JLO_QUIET;
        else if (l->off_n >= JL_OFF_LINES)
            c = JLO_OFF;
        else if (song_new && l->unfit_run >= JL_SONG_LINES)
            c = JLO_SONG;
        if (c) {
            l->lost = 1;
            l->cause = c;
            l->losts[c]++;
        }
    }
    if (back)
        *back = b;
    return l->lost;
}
