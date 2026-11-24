/* SPDX-License-Identifier: GPL-3.0-only */
/* The host enumeration rule and its sets (jank_hostfilter.h). */
#include "jank_hostfilter.h"
#include <string.h>

int jhf_set_has(const struct jhf_set *s, uint16_t vid, uint16_t pid)
{
    for (unsigned i = 0; i < s->n && i < JHF_MAX; i++)
        if (s->id[i].vid == vid && s->id[i].pid == pid)
            return 1;
    return 0;
}

int jhf_set_add(struct jhf_set *s, uint16_t vid, uint16_t pid)
{
    if (jhf_set_has(s, vid, pid) || s->n >= JHF_MAX)
        return 0;
    s->id[s->n].vid = vid;
    s->id[s->n].pid = pid;
    s->n++;
    return 1;
}

unsigned jhf_set_merge(struct jhf_set *to, const struct jhf_set *from)
{
    unsigned added = 0;
    for (unsigned i = 0; i < from->n && i < JHF_MAX; i++)
        added += (unsigned)jhf_set_add(to, from->id[i].vid, from->id[i].pid);
    return added;
}

int jhf_proceed(struct jhf *f, uint8_t dev_class, uint16_t vid, uint16_t pid)
{
    if (dev_class == JHF_CLASS_HUB) {
        f->hub_seen = 1;
        return 1;
    }
    if (!f->hub_seen || !jhf_set_has(&f->deny, vid, pid))
        return 1;
    f->skipped++;
    f->skip_vid = vid;
    f->skip_pid = pid;
    return 0;
}

size_t jhf_pack(const struct jhf_set *s, uint8_t out[JHF_PACK_MAX])
{
    unsigned n = s->n < JHF_MAX ? s->n : JHF_MAX;
    out[0] = (uint8_t)n;
    for (unsigned i = 0; i < n; i++) {
        out[1 + 4 * i] = (uint8_t)(s->id[i].vid & 0xFFu);
        out[2 + 4 * i] = (uint8_t)(s->id[i].vid >> 8);
        out[3 + 4 * i] = (uint8_t)(s->id[i].pid & 0xFFu);
        out[4 + 4 * i] = (uint8_t)(s->id[i].pid >> 8);
    }
    return 1u + 4u * n;
}

int jhf_unpack(struct jhf_set *s, const uint8_t *p, size_t n)
{
    memset(s, 0, sizeof *s);
    if (!p || n < 1u || p[0] > JHF_MAX || n != 1u + 4u * p[0])
        return 0;
    for (unsigned i = 0; i < p[0]; i++)
        jhf_set_add(s, (uint16_t)(p[1 + 4 * i] | (p[2 + 4 * i] << 8)), (uint16_t)(p[3 + 4 * i] | (p[4 + 4 * i] << 8)));
    return 1;
}
