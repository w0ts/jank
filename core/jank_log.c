/* SPDX-License-Identifier: GPL-3.0-only */
/* The serial test log (jank_log.h). */
#include "jank_log.h"
#include <stdio.h>

static size_t put_heard(char *out, size_t cap, const uint32_t heard[JK_GROUP_MAX + 1u])
{
    size_t n = 0;
    for (uint32_t g = JK_GROUP_MIN; g <= JK_GROUP_MAX; g++) {
        int r = snprintf(out + n, cap - n, "%s%lu", g == JK_GROUP_MIN ? "" : ",", (unsigned long)heard[g]);
        if (r < 0 || (size_t)r >= cap - n)
            return 0;
        n += (size_t)r;
    }
    return n;
}

size_t jk_log_line(char *out, size_t cap, const struct jk_snapshot *s, const uint32_t heard[JK_GROUP_MAX + 1u], uint32_t uni_ok, uint32_t uni_fail, uint32_t now_ms)
{
    const struct jk_bridge *b = &s->b;
    char list[110];
    uint32_t bpm_x100 = s->mpb > 0 ? (uint32_t)((6000000000ull + (uint64_t)s->mpb / 2u) / (uint32_t)s->mpb) : 0u;
    int r;
    if (!put_heard(list, sizeof list, heard))
        return 0;
    r = snprintf(out, cap, "# up=%lu group=%u ch=%u heard=%s peers=%lu rssi=%d sent=%lu tx_fail=%lu uni_ok=%lu uni_fail=%lu link=%u link_peers=%lu bpm=%lu.%02lu\n",
                 (unsigned long)s->uptime_s, b->group, b->channel, list, (unsigned long)jk_peers_heard(b, now_ms), jk_best_rssi(b, now_ms),
                 (unsigned long)b->st.air_out, (unsigned long)s->tx_fail, (unsigned long)uni_ok, (unsigned long)uni_fail, s->link_on,
                 (unsigned long)s->link_peers, (unsigned long)(bpm_x100 / 100u), (unsigned long)(bpm_x100 % 100u));
    return r > 0 && (size_t)r < cap ? (size_t)r : 0u;
}
