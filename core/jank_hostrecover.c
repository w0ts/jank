/* SPDX-License-Identifier: GPL-3.0-only */
/* The software replug (jank_hostrecover.h). */
#include "jank_hostrecover.h"
#include <string.h>

void jhr_init(struct jhr *s) { memset(s, 0, sizeof *s); }

enum jhr_act jhr_step(struct jhr *s, unsigned fails, uint32_t now_ms)
{
    if (s->off) {
        if ((uint32_t)(now_ms - s->off_ms) < JHR_OFF_MS)
            return JHR_NONE;
        s->off = 0;
        return JHR_POWER_ON;
    }
    if (fails < JHR_AFTER_FAILS || s->count >= JHR_MAX || (s->count && (uint32_t)(now_ms - s->last_ms) < JHR_GAP_MS))
        return JHR_NONE;
    s->count++;
    s->off = 1;
    s->off_ms = s->last_ms = now_ms;
    return JHR_POWER_OFF;
}

int jhr_hold_over(uint32_t since_start_ms, int confirmed, uint32_t since_request_ms)
{
    if (since_start_ms >= JHR_HOLD_MAX_MS)
        return 1;
    return since_start_ms >= JHR_HOLD_MIN_MS && confirmed && since_request_ms >= JHR_HOLD_QUIET_MS;
}

uint32_t jhr_wait_ms(const struct jhr *s, uint32_t now_ms)
{
    uint32_t gone;
    if (!s->off)
        return UINT32_MAX;
    gone = now_ms - s->off_ms;
    return gone >= JHR_OFF_MS ? 0u : JHR_OFF_MS - gone;
}
