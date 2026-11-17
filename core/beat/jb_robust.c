/* SPDX-License-Identifier: GPL-3.0-only */
/* The robust follower (jb_robust.h): robust.py, rchain.py and dbn.py's names and order. */
#include "jb_robust.h"
#include "jank_conf.h"
#include "jb_skip.h"
#include <math.h>
#include <string.h>

void jrb_init(struct jb_robust *r, struct jrb_store *st)
{
    memset(r, 0, sizeof *r);
    r->st = st;
}

int jrb_on(const struct jb_robust *r) { return r->st != NULL; }

static uint32_t clk(const struct jb_robust *r) { return r->clock ? r->clock() : 0u; }

struct jrb_cycles jrb_take_cycles(struct jb_robust *r)
{
    struct jrb_cycles c = {r->refresh_cyc, r->refresh_max, r->gate_cyc, r->gate_max, r->tick_cyc};
    r->refresh_cyc = r->refresh_max = r->gate_cyc = r->gate_max = r->tick_cyc = 0;
    return c;
}

void jrb_reset(struct jb_robust *r) { r->refused_run = 0; }

void jrb_attach(struct jb_robust *r, struct jb_follow *f, struct jb_weak *w)
{
    r->f = f;
    r->w = w;
    f->m.gate = jrb_on(r) ? jrb_gate : NULL;
    f->m.gate_ctx = r;
}

void jrb_act(struct jb_robust *r, int64_t index, float act)
{
    if (!jrb_on(r) || index < r->act_n)
        return;
    if (index - r->act_n > JRB_ACT_RING)
        r->act_n = index - JRB_ACT_RING;
    while (r->act_n < index)
        r->st->ring[(uint64_t)r->act_n++ % JRB_ACT_RING] = 0.0f;
    r->st->ring[(uint64_t)index % JRB_ACT_RING] = act;
    r->act_n = index + 1;
}

/* dbn.fold_ratio: r brought into [1/sqrt 2, sqrt 2) by octaves */
static double fold_ratio(double r) { return r / pow(2.0, floor(log2(r) + 0.5)); }

int jrb_related(double bpm, double mem_bpm, double tol)
{
    double r = fold_ratio(bpm / mem_bpm);
    return fabs(r / 0.75 - 1.0) <= tol || fabs(r / (4.0 / 3.0) - 1.0) <= tol;
}

/* robust.consistent: the last n of the beats lie on one grid of `period` (90 % of them within a tenth of a period, no gap over
 * 2 beats, the fitted period within 2 % of it) */
int jrb_consistent(const double *beats, int n_all, double period, int n)
{
    if (n_all < n || n < 2 || n > JRB_MAX_ALIGN_BEATS)
        return 0;
    const double *b = beats + (n_all - n);
    double k[JRB_MAX_ALIGN_BEATS];
    int uniq = 0;
    for (int i = 0; i < n; i++) {
        k[i] = rint((b[i] - b[n - 1]) / period);
        if (i > 0 && k[i] - k[i - 1] > 2.0)
            return 0;
        if (i == 0 || k[i] != k[i - 1])
            uniq++;             /* (k ascends: the beats are in order, so distinct values are runs) */
    }
    if ((double)uniq < 0.9 * n)
        return 0;
    /* np.polyfit(k, b, 1): the least-squares line, b = slope k + icpt */
    double sk = 0.0, sb = 0.0, skk = 0.0, skb = 0.0;
    for (int i = 0; i < n; i++) {
        sk += k[i];
        sb += b[i];
        skk += k[i] * k[i];
        skb += k[i] * b[i];
    }
    double den = n * skk - sk * sk;
    if (!(fabs(den) > 0.0))
        return 0;
    double slope = (n * skb - sk * sb) / den, icpt = (sb - slope * sk) / n;
    if (fabs(slope / period - 1.0) > 0.02)
        return 0;
    int ok = 0;
    for (int i = 0; i < n; i++)
        if (fabs(b[i] - (slope * k[i] + icpt)) <= 0.1 * period)
            ok++;
    return (double)ok / n >= 0.9;
}

/* LockGate.allow */
int jrb_gate(void *ctx, int kind, double now, double period)
{
    struct jb_robust *r = ctx;
    const uint32_t c0 = clk(r);
    r->asked++;
    int ok = 1;
    if (r->has_mem && now - r->mem_t <= JRB_MEM_S && jrb_related(60.0 / period, r->mem_bpm, JRB_REL_TOL)) {
        const struct jb_weak *w = r->w;
        ok = kind == JB_GATE_LOCK ? r->f->m.in_a_row >= JRB_RELATED_BEATS
                                  : jrb_consistent(jw_seen(w), w->n_seen, period, JRB_RELATED_BEATS);
        if (!ok)
            r->refused_memory++;
    }
    if (ok && jrb_on(r)) {                /* the ratio check: the comb at P must beat its relatives' */
        struct jc_curve c;
        int64_t end = (int64_t)floor(now * JC_FPS + 1e-9);
        if (jc_curve(&c, r->st->x, r->st->ring, JRB_ACT_RING, r->act_n, end, JRB_RATIO_SPAN)) {
            double m = jc_ratio_margin(&c, period * JC_FPS);
            if (m == m && m < JRB_RATIO_MARGIN) {
                ok = 0;
                r->refused_ratio++;
            }
        }
    }
    r->refused_run = ok ? 0 : r->refused_run + 1;
    if (r->clock) {
        uint32_t c = clk(r) - c0;
        r->gate_cyc += c;
        r->gate_max = c > r->gate_max ? c : r->gate_max;
    }
    return ok;
}

/* dbn.tempo_prior: floor + (1 - floor) x a Gaussian in log2 tempo around the remembered tempo and its octaves */
void jrb_tempo_prior(const struct jb_dbn *d, double mem_bpm, double width, double floor_, float *w)
{
    for (int i = 0; i < d->n_intervals; i++) {
        double lr = log2((60.0 * d->fps / d->interval[i]) / mem_bpm), g = 0.0;
        for (int c = -1; c <= 1; c++) {
            double z = (lr - c) / width, e = exp(-0.5 * z * z);
            g = e > g ? e : g;
        }
        w[i] = (float)(floor_ + (1.0 - floor_) * g);
    }
}

/* Agreement._refresh */
static void refresh(struct jb_robust *r, struct jb_dbn *dbn, const struct jrb_in *in)
{
    if (!in->has_grid) {
        r->v_tempo = r->v_phase = r->v_dbn = 0.0;
        return;
    }
    const double period = in->period, now = in->now;
    const int64_t end = (int64_t)floor(now * JC_FPS + 1e-9);
    struct jc_curve c;
    r->v_tempo = jc_curve(&c, r->st->x, r->st->ring, JRB_ACT_RING, r->act_n, end, JRB_AGREE_SPAN)
                     ? jc_tempo_agreement(&c, period * JC_FPS) : 0.0;
    double ls[JCF_LINES];
    int n = 0;
    for (size_t i = 0; i < in->n_lines && n < JCF_LINES; i++)
        if (now - JRB_AGREE_SPAN / (double)JC_FPS <= in->lines[i] && in->lines[i] <= now - 0.1)
            ls[n++] = in->lines[i];
    r->v_phase = jc_phase_agreement(r->st->ring, JRB_ACT_RING, r->act_n, ls, n, period, end);
    r->v_dbn = (double)jb_dbn_mass_near(dbn, 60.0 / period, JRB_DBN_NEAR);
}

void jrb_tick(struct jb_robust *r, struct jb_dbn *dbn, const struct jrb_in *in, uint8_t *gated, struct jrb_out *out)
{
    const double now = in->now, threshold = (double)in->threshold_pct / 100.0;
    const uint32_t tick0 = clk(r), refresh0 = r->refresh_cyc;
    double g = in->conf;
    memset(out, 0, sizeof *out);
    if (!in->silent) {
        if (now >= r->next_t) {
            const uint32_t c0 = clk(r);
            r->next_t = now + JRB_AGREE_EVERY_S;
            refresh(r, dbn, in);
            if (r->clock) {
                uint32_t c = clk(r) - c0;
                r->refresh_cyc += c;
                r->refresh_max = c > r->refresh_max ? c : r->refresh_max;
            }
        }
        g = (in->conf + r->v_tempo + r->v_phase + r->v_dbn) / 4.0;     /* np.mean of the four parts, in this order */
    }
    out->gconf = g;
    if (threshold > 0.0 && !in->keep_running) {
        double start = threshold + JRB_HYST < 1.0 ? threshold + JRB_HYST : 1.0;
        int would = *gated ? g < start : g < threshold;
        int keep = in->was_running && !in->silent && in->has_bpm && !in->jumped && g >= JRB_COAST_FLOOR;
        *gated = keep ? 0 : (uint8_t)would;
        if (r->sk)                      /* SKIP: a re-synced grid opens the gate, a suspect one closes it (rchain's skip block) */
            jsk_gate(r->sk, in->has_bpm, g, threshold, gated);
    } else {
        *gated = 0;
    }
    out->gated = *gated;
    out->run = (uint8_t)(in->has_bpm && !*gated && !in->silent);
    out->coasting = (uint8_t)(out->run && g < threshold);
    out->confident = (uint8_t)(out->run && in->strict >= threshold && in->has_kf && !in->jumped);
    if (out->confident) {
        r->has_mem = 1;
        r->mem_bpm = in->bpm;
        r->mem_t = now;
    }
    if (r->has_mem && now - r->mem_t <= JRB_MEM_S && !out->confident) {
        if (!r->has_prior_cache || r->prior_for != r->mem_bpm) {
            jrb_tempo_prior(dbn, r->mem_bpm, JRB_PRIOR_WIDTH, JRB_PRIOR_FLOOR, r->prior);
            r->prior_for = r->mem_bpm;
            r->has_prior_cache = 1;
        }
        jb_dbn_set_prior(dbn, r->prior);
        out->prior_on = 1;
        r->prior_ticks++;
    } else {
        jb_dbn_set_prior(dbn, NULL);
    }
    r->coasting_ticks += out->coasting;
    if (r->clock)
        r->tick_cyc += (clk(r) - tick0) - (r->refresh_cyc - refresh0);      /* (the refresh counted apart) */
}
