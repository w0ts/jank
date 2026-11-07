/* SPDX-License-Identifier: GPL-3.0-only */
/* MIDI clock tick maths (jank_clock.h). jc_time and jc_step run in the timer interrupt: multiply and shift only. */
#include "jank_clock.h"
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define JC_ISR IRAM_ATTR
#else
#define JC_ISR
#endif

#define UB_BEAT 1000000

static int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

int64_t jc_tick_ub(int64_t n)
{
    int64_t scaled = n * UB_BEAT;
    return floor_div(scaled * 2 + JC_PPQN, JC_PPQN * 2);    /* rounded to the nearest */
}

void jc_map_make(struct jc_map *m, int64_t now, int64_t ub, int64_t mpb, int32_t offset_us)
{
    int64_t n = -floor_div(-ub * JC_PPQN, UB_BEAT);         /* the first tick at or after ub */
    int64_t ub_left = n * UB_BEAT - ub * JC_PPQN;           /* in 1/24 micro-beats, exact */
    m->n_ref = n;
    m->t_ref = now + floor_div(ub_left * mpb * 2 + UB_BEAT * JC_PPQN, UB_BEAT * JC_PPQN * 2) + offset_us;
    m->q16 = (((uint64_t)mpb << 16) + JC_PPQN / 2) / JC_PPQN;
    m->valid = 1;
}

int64_t JC_ISR jc_time(const struct jc_map *m, int64_t n)
{
    int64_t d = n - m->n_ref;
    uint64_t mag = (uint64_t)(d < 0 ? -d : d) * m->q16;
    int64_t us = (int64_t)((mag + 0x8000u) >> 16);
    return m->t_ref + (d < 0 ? -us : us);
}

void jc_ticker_init(struct jc_ticker *c, uint32_t quantum)
{
    memset(c, 0, sizeof *c);
    c->bar_ticks = (quantum ? quantum : 1u) * JC_PPQN;
}

/* the first tick of the map not before now */
static void JC_ISR seek(struct jc_ticker *c, const struct jc_map *m, int64_t now)
{
    int64_t n = m->n_ref;
    int64_t tick = (int64_t)(m->q16 >> 16);
    if (tick > 0 && now > m->t_ref) {             /* a shift instead of a division: a close estimate from below */
        int64_t behind = now - m->t_ref;
        while (behind >= tick << 8) {             /* (at most a few steps: maps are ms old, and a stall is rare) */
            n += 256;
            behind -= tick << 8;
        }
    }
    while (jc_time(m, n) < now)
        n++;
    while (jc_time(m, n - 1) >= now)
        n--;
    c->n = n;
    c->t_due = jc_time(m, n);
}

int jc_begin(struct jc_ticker *c, const struct jc_map *m, int64_t now)
{
    if (!m->valid)
        return -1;
    seek(c, m, now);
    c->started = 1;
    return 0;
}

/* n mod bar_ticks == 0, for a tick that may be negative */
static int JC_ISR on_bar(int64_t n, uint32_t bar)
{
    int64_t d = (n < 0 ? -n : n);
    while (d >= (int64_t)bar << 20)           /* no division in the interrupt */
        d -= (int64_t)bar << 20;
    for (int s = 19; s >= 0; s--)
        if (d >= (int64_t)bar << s)
            d -= (int64_t)bar << s;
    return d == 0;
}

static void JC_ISR collect(struct jc_ticker *c, struct jc_fire *o)
{
    int stop_now = c->resync || (c->want_stop && c->playing);
    o->n = 0;
    if (stop_now)
        o->bytes[o->n++] = JC_STOP;
    if (stop_now || c->want_stop)
        c->playing = 0;
    c->want_stop = 0;
    if (c->resync) {
        c->resync = 0;
        c->want_start = 1;
    }
    /* never in the tick of its stop: the receiver gets a whole bar line after it */
    if (c->want_start && !stop_now && on_bar(c->n, c->bar_ticks)) {
        o->bytes[o->n++] = JC_START;
        c->playing = 1;
        c->want_start = 0;
    }
    o->bytes[o->n++] = JC_CLOCK;
    c->sent++;
}

void JC_ISR jc_step(struct jc_ticker *c, const struct jc_map *m, int64_t now, struct jc_fire *o)
{
    int64_t tick = (int64_t)((m->q16 + 0x8000u) >> 16);
    int64_t scheduled = c->t_due, want, gap, limit = tick * JC_RESYNC_TICKS;
    collect(c, o);
    c->n++;
    want = jc_time(m, c->n);
    if (want - scheduled > limit || scheduled - want > limit || now - scheduled > limit) {
        seek(c, m, now > scheduled ? now : scheduled);
        c->resyncs++;
        o->next = c->t_due;
        return;
    }
    gap = want - scheduled;                   /* a small move of the session: slew by up to half a tick a tick */
    if (gap < tick / 2)
        gap = tick / 2;
    else if (gap > tick + tick / 2)
        gap = tick + tick / 2;
    c->t_due = scheduled + gap;
    o->next = c->t_due;
}

void jc_want(struct jc_ticker *c, int play)
{
    if (play && !c->playing)
        c->want_start = 1;
    if (!play) {
        c->want_start = 0;
        if (c->playing)
            c->want_stop = 1;
    }
}

void jc_resync(struct jc_ticker *c) { c->resync = 1; }

int jc_should_play(int auto_transport, int link_playing, uint32_t link_peers)
{
    return auto_transport ? link_peers > 0u : link_playing != 0;
}
