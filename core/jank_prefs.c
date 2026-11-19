/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_prefs.h"
#include <string.h>
#include "jank_core.h"

void jk_prefs_defaults(struct jk_prefs *p)
{
    *p = (struct jk_prefs){JK_PREFS_VERSION, 1, 100, 0, 2, 1, 80, 0, 0, 0};
}

size_t jk_prefs_pack(const struct jk_prefs *p, uint8_t *out, size_t cap)
{
    if (cap < JK_PREFS_SIZE)
        return 0;
    out[0] = p->version;
    out[1] = p->lead;
    out[2] = p->bright;
    out[3] = p->sleep;
    out[4] = p->rate;
    out[5] = p->conf_stop;
    out[6] = p->th;
    out[7] = (uint8_t)p->out1;
    out[8] = (uint8_t)p->out2;
    out[9] = (uint8_t)p->gain;
    return JK_PREFS_SIZE;
}

int jk_prefs_unpack(struct jk_prefs *p, const uint8_t *in, size_t n)
{
    struct jk_prefs q;
    if (n != JK_PREFS_SIZE)
        return 0;
    q = (struct jk_prefs){in[0], in[1], in[2], in[3], in[4], in[5], in[6], (int8_t)in[7], (int8_t)in[8], (int8_t)in[9]};
    if (q.version != JK_PREFS_VERSION || q.bright < 10 || q.bright > 100 || q.sleep >= 4 || q.rate >= 5 || q.th > 100)
        return 0;
    *p = q;
    return 1;
}

int jk_prefs_set_field(struct jk_prefs *p, enum jo_field f, int v)
{
    switch (f) {
    case JO_F_OUT1: p->out1 = (int8_t)v; return 1;
    case JO_F_OUT2: p->out2 = (int8_t)v; return 1;
    case JO_F_RATE: p->rate = (uint8_t)v; return 1;
    case JO_F_THRESHOLD: p->th = (uint8_t)v; return 1;
    case JO_F_GAIN: p->gain = (int8_t)v; return 1;
    case JO_F_BRIGHT: p->bright = (uint8_t)v; return 1;
    case JO_F_SLEEP: p->sleep = (uint8_t)v; return 1;
    case JO_F_LOWCONF: p->conf_stop = v == 1; return 1;
    default: return 0;
    }
}

void jk_prefs_changed(struct jk_prefs_save *s, uint32_t now_ms)
{
    s->pending = 1;
    s->changed_ms = now_ms;
}

int jk_prefs_due(struct jk_prefs_save *s, uint32_t now_ms, int playing, int force)
{
    if (!s->pending)
        return 0;
    if (force)
        playing = 0;
    else if ((uint32_t)(now_ms - s->changed_ms) < JK_PREFS_SETTLE_MS)
        return 0;
    return jk_save_due(&s->pending, playing);
}
