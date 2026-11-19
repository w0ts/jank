/* SPDX-License-Identifier: GPL-3.0-only */
#include "jb_lost.h"
#include <math.h>

void jl_init(struct jb_lost *l)
{
    l->losts = 0;
    jl_clear(l);
}

void jl_clear(struct jb_lost *l)
{
    l->next_line = -1.0;
    l->lost = 0;
    l->miss_run = l->ev_run = 0;
}

static int strong_near(const double *s, size_t n, double line, double margin)
{
    for (size_t i = n; i-- > 0;) {
        if (s[i] < line - margin)
            break;
        if (fabs(s[i] - line) <= margin)
            return 1;
    }
    return 0;
}

static uint8_t inc(uint8_t v) { return v < 255u ? (uint8_t)(v + 1u) : v; }

/* the lines whose frames (a period after them) are in */
static void judge(struct jb_lost *l, const double *strong, size_t ns, int has_grid, double t0, double p, double now)
{
    if (!has_grid || !(p > 0.0)) {
        l->next_line = -1.0;
        return;
    }
    if (l->next_line < 0.0 || fabs(remainder(l->next_line - t0, p)) > 0.25 * p) {
        /* a new grid (a lock, a jump): its first line after the last one judged */
        double k = ceil((fmax(l->next_line, now - JL_SEED_S) - t0) / p - 0.25);
        l->next_line = t0 + k * p;
    } else
        l->next_line = t0 + p * round((l->next_line - t0) / p);     /* the glide moved the grid a little */
    while (l->next_line + p + JL_LAG_S <= now) {
        if (strong_near(strong, ns, l->next_line, JL_TOL * p)) {
            l->ev_run = inc(l->ev_run);
            l->miss_run = 0;
        } else {
            l->miss_run = inc(l->miss_run);
            l->ev_run = 0;
        }
        l->next_line += p;
    }
}

int jl_step(struct jb_lost *l, const double *strong, size_t n_strong, int has_grid, double t0, double period, double now,
            int running, uint8_t *back)
{
    uint8_t b = 0;
    judge(l, strong, n_strong, has_grid, t0, period, now);
    if (l->lost && l->ev_run >= JL_BACK_LINES) {
        l->lost = 0;
        b = 1;
    } else if (!l->lost && running && l->miss_run >= JL_LOST_LINES) {
        l->lost = 1;
        l->losts++;
    }
    if (back)
        *back = b;
    return l->lost;
}
