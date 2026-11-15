/* SPDX-License-Identifier: GPL-3.0-only */
/* A Link start on the session's next bar line (jank_start.h). */
#include "jank_start.h"

static int64_t bar_ub(uint32_t quantum) { return (int64_t)(quantum ? quantum : 1u) * LK_UBEAT; }

int64_t jst_bar_at_or_after(int64_t ub, uint32_t quantum)
{
    int64_t bar = bar_ub(quantum), ph = lk_phase(ub, bar);
    return ph ? ub - ph + bar : ub;
}

/* the bar line a start asked for now would take */
static int64_t next_bar(const struct lk_node *k, int64_t now, uint32_t quantum)
{
    return jst_bar_at_or_after(lk_beat_at(k, now + JST_LEAD_US), quantum);
}

void jst_arm(struct jst *s, const struct lk_node *k, int64_t now, uint32_t quantum)
{
    if (s->armed)
        return;
    s->armed = 1;
    s->quantum = quantum ? quantum : 1u;
    s->ub = next_bar(k, now, s->quantum);
}

void jst_cancel(struct jst *s) { s->armed = 0; }

int jst_poll(struct jst *s, struct lk_node *k, int64_t now, int allowed)
{
    int64_t t_bar, fresh;
    if (!s->armed)
        return 0;
    if (lk_playing(k)) {
        s->armed = 0;
        return 0;
    }
    fresh = next_bar(k, now, s->quantum);
    if (fresh < s->ub - bar_ub(s->quantum)) {
        s->ub = fresh;                  /* the session's beats moved back (a new session joined): its next bar */
        s->moves++;
    }
    t_bar = lk_time_at(k, s->ub);
    if (now < t_bar - JST_LEAD_US)
        return 0;
    if (!allowed || now > t_bar - JST_LATE_US) {
        s->ub = fresh > s->ub ? fresh : s->ub + bar_ub(s->quantum);
        s->moves++;
        return 0;
    }
    lk_set_playing(k, 1, s->ub, now);   /* playing from bar B, set now (jank_start.h) */
    s->armed = 0;
    s->commits++;
    return 1;
}
