/* SPDX-License-Identifier: GPL-3.0-only */
/* The HOME line (jank_home.h). */
#include "jank_home.h"
#include "jank_hold.h"
#include <stdio.h>

/* "-" or the number */
static void field(char *out, size_t cap, int have, long v)
{
    if (have)
        snprintf(out, cap, "%ld", v);
    else
        snprintf(out, cap, "-");
}

size_t jhm_home_line(const struct jk_cfg *c, const struct jk_snapshot *s, uint32_t now_ms, char *out, size_t cap)
{
    char net[8], bar[12], batt[8], level[8], conf[8], lo[8], hi[8], mode[4];
    uint32_t q = c->quantum >= 1u && c->quantum <= JK_QUANTUM_MAX ? c->quantum : 4u;
    uint32_t bpm_x100 = s->mpb > 0 ? (uint32_t)((6000000000ull + (uint64_t)s->mpb / 2u) / (uint32_t)s->mpb) : 0u;
    /* AUDIO mode: the outputs (jank_hold.h: held = stopped); LINK mode: the session */
    unsigned transport = s->beat.audio_live ? (s->outputs == JHO_RUNNING ? 1u : s->outputs == JHO_ARMED ? 2u : 0u)
                                            : s->armed ? 2u : s->playing ? 1u : 0u;
    int r;
    if (c->net == JK_NET_WIFI)
        snprintf(net, sizeof net, "w%u", s->wifi_up ? 1u : 0u);
    else
        snprintf(net, sizeof net, "j%u", s->b.group);
    if (s->mpb > 0) {
        /* the beat in hundredths, floored, then into the bar (Link's phase: beat mod quantum, negative beats too) */
        int64_t hb = s->beat_ub >= 0 ? s->beat_ub / 10000 : -((-s->beat_ub + 9999) / 10000);
        int64_t m = (int64_t)q * 100;
        field(bar, sizeof bar, 1, (long)(((hb % m) + m) % m));
    } else {
        field(bar, sizeof bar, 0, 0);
    }
    field(batt, sizeof batt, s->batt_pct >= 0, s->batt_pct);
    field(level, sizeof level, s->beat.audio, s->beat.level_db);
    field(conf, sizeof conf, s->beat.audio, s->beat.conf_pct);
    if (s->beat.audio && s->beat.conf_hold)    /* M2k: the hold, "<pct>h", 99 at most: no wider than "100" */
        snprintf(conf, sizeof conf, "%uh", s->beat.conf_pct > 99u ? 99u : (unsigned)s->beat.conf_pct);
    else if (s->beat.audio && s->beat.coast)   /* the coast (robust follower: the outputs run under the threshold): "<pct>c" */
        snprintf(conf, sizeof conf, "%uc", s->beat.conf_pct > 99u ? 99u : (unsigned)s->beat.conf_pct);
    field(lo, sizeof lo, s->beat.audio, s->beat.level_min_db);
    field(hi, sizeof hi, s->beat.audio, s->beat.level_max_db);
    /* the mode: 0 LINK, 1 AUDIO, or AUDIO folded: x2 x4 /2 /4 (core/jank_fold.h) */
    if (!s->beat.audio_live || !s->beat.fold_exp)
        snprintf(mode, sizeof mode, "%u", s->beat.audio_live ? 1u : 0u);
    else
        snprintf(mode, sizeof mode, "%c%d", s->beat.fold_exp > 0 ? 'x' : '/', s->beat.fold_exp == 2 || s->beat.fold_exp == -2 ? 4 : 2);
    r = snprintf(out, cap, "h %lu %s %u %s %lu %lu %lu %s %s %s %s %s %s", (unsigned long)bpm_x100, net, transport, bar, (unsigned long)q,
                 (unsigned long)s->link_peers, (unsigned long)(c->net == JK_NET_WIFI ? 0u : jk_peers_heard(&s->b, now_ms)), batt, level, conf,
                 lo, hi, mode);
    return r < 0 || (size_t)r >= cap ? 0u : (size_t)r;
}
