/* SPDX-License-Identifier: GPL-3.0-only */
/* The Launchpad's step sequencer modes (jank_lpstep.h). */
#include "jank_lpstep.h"
#include <string.h>

#define NO_TRACK 255u
#define VEL_OWN 255u                   /* STEP: the track's own velocity and length */
#define LEN_OWN 255u
#define QUAD 4u                        /* the quadrants are 4 x 4 */
#define STEP_ROWS_FROM 4u              /* pad rows 4..7 are the steps */
#define PAGE_ROW 3u                    /* the right quadrant's top row holds the page pads */

void jlst_init(struct jlst *s, enum jlst_kind kind)
{
    memset(s, 0, sizeof *s);
    s->kind = (uint8_t)kind;
    s->note = -1;
    s->cache_track = NO_TRACK;
    for (unsigned i = 0; i < JLST_KEYS; i++)
        s->key_note[i] = -1;
}

static int is_cached(const struct jlst *s, uint8_t track) { return s->cache_track == track; }

static void set_act(struct jlapp_act *o, uint8_t kind, uint8_t track, uint8_t note, uint8_t vel)
{
    memset(o, 0, sizeof *o);
    o->kind = kind;
    o->track = track;
    o->note = note;
    o->vel = vel;
}

/* the pages in the cache belong to the track of the last read: another track drops them */
static void sync_track(struct jlst *s, uint8_t track)
{
    if (s->cache_track == track)
        return;
    s->cache_track = track;
    s->pages_known = 0;
    s->have[0] = s->have[1] = 0;
    s->edit_on = 0;
}

/* ---- the steps ---- */

static size_t step_press(struct jlst *s, const struct jlst_ctx *c, unsigned k, struct jlapp_act *out, size_t max)
{
    unsigned p = k / 16u, idx = k % 16u, abs_page = s->first_page + p;
    uint8_t track = c->n->track, op, note = 0;
    struct jdl_step *st;
    if (max < 1 || !is_cached(s, track) || !s->have[p] || abs_page >= s->pages_known)
        return 0;                      /* the Digitone has not told this page yet: nothing to toggle */
    st = &s->pg[p].step[idx];
    if (s->kind == JLST_MELODIC && s->note >= 0) {
        if (st->trig && st->note == s->note) {
            op = JDL_STEP_CLEAR;
        } else {
            op = JDL_STEP_NOTE;
            note = (uint8_t)s->note;
        }
    } else {
        op = st->trig ? JDL_STEP_CLEAR : JDL_STEP_TRIG;
    }
    set_act(out, JLAPP_ACT_STEP, track, note, VEL_OWN);
    out->step = (uint8_t)(abs_page * 16u + idx);
    out->op = op;
    out->len = LEN_OWN;
    st->trig = op != JDL_STEP_CLEAR;   /* shown at once; the next read confirms it */
    if (op == JDL_STEP_NOTE)
        st->note = note;
    s->edit_on = 1;
    s->edit_ms = c->now_ms;
    return 1;
}

/* ---- the left quadrant: tracks (drums) or a keyboard (melodic) ---- */

static int auditions(const struct jlst_ctx *c) { return c->can_play && !c->transport_playing; }

static size_t drum_pad(struct jlst *s, const struct jlst_ctx *c, uint8_t x, uint8_t y, uint8_t value, struct jlapp_act *out, size_t max)
{
    uint8_t t = (uint8_t)((QUAD - 1u - y) * QUAD + x);     /* reading order: track 1 top left of the quadrant */
    uint16_t bitv = (uint16_t)(1u << t);
    size_t n = 0, need;
    int play = auditions(c);
    if (!value) {
        if (!(s->track_held & bitv) || max < 1)
            return 0;
        s->track_held &= (uint16_t)~bitv;
        set_act(out, JLAPP_ACT_TRACK_OFF, t, 0, 0);
        return 1;
    }
    if (s->track_held & bitv)
        return 0;
    need = (size_t)(t != c->n->track) + (size_t)play;
    if (need > max)
        return 0;
    if (t != c->n->track) {
        c->n->track = t;
        set_act(&out[n++], JLAPP_ACT_SELECT, t, 0, 0);
    }
    if (play) {
        s->track_held |= bitv;
        set_act(&out[n++], JLAPP_ACT_TRACK_ON, t, 0, 0);
    }
    return n;
}

static size_t key_pad(struct jlst *s, const struct jlst_ctx *c, uint8_t x, uint8_t y, uint8_t value, struct jlapp_act *out, size_t max)
{
    unsigned i = y * QUAD + x;
    struct jln_pad p;
    if (!value) {
        if (s->key_note[i] < 0 || max < 1)
            return 0;
        set_act(out, JLAPP_ACT_NOTE_OFF, s->key_track[i], (uint8_t)s->key_note[i], 0);
        s->key_note[i] = -1;
        return 1;
    }
    if (s->key_note[i] >= 0)
        return 0;
    jln_pad(c->n, c->scale, x, y, &p);
    if (p.note < 0)
        return 0;
    s->note = p.note;                  /* a step press places this note from now on */
    if (!auditions(c) || max < 1)
        return 0;
    s->key_note[i] = p.note;
    s->key_track[i] = c->n->track;
    set_act(out, JLAPP_ACT_NOTE_ON, c->n->track, (uint8_t)p.note, value);
    return 1;
}

size_t jlst_pad(struct jlst *s, const struct jlst_ctx *c, uint8_t x, uint8_t y, uint8_t value, struct jlapp_act *out, size_t max)
{
    if (x > 7u || y > 7u)
        return 0;
    if (y >= STEP_ROWS_FROM)
        return value ? step_press(s, c, (7u - y) * 8u + x, out, max) : 0;
    if (x < QUAD)
        return s->kind == JLST_DRUM ? drum_pad(s, c, x, y, value, out, max) : key_pad(s, c, x, y, value, out, max);
    if (y == PAGE_ROW && value) {      /* the page pads */
        unsigned page = x - QUAD;
        if (s->pages_known && page >= s->pages_known)
            return 0;
        if (page != s->first_page) {
            s->first_page = (uint8_t)page;
            s->have[0] = s->have[1] = 0;
            s->edit_on = 0;
        }
    }
    return 0;
}

/* ---- the Digitone's answers ---- */

size_t jlst_wanted(const struct jlst *s, uint8_t pages[2])
{
    size_t n = 0;
    if (!s->pages_known) {
        pages[0] = 0;
        return 1;
    }
    if (s->first_page < s->pages_known)
        pages[n++] = s->first_page;
    if (s->first_page + 1u < s->pages_known)
        pages[n++] = (uint8_t)(s->first_page + 1u);
    return n;
}

void jlst_on_page(struct jlst *s, uint8_t track, const struct jdl_page *pg, uint32_t now_ms)
{
    if (pg->track != track || pg->page >= JLST_PAGES || pg->pages == 0 || pg->pages > JLST_PAGES)
        return;
    sync_track(s, track);
    s->pages_known = pg->pages;
    if (s->first_page >= pg->pages) {  /* a shorter pattern: the last page */
        s->first_page = (uint8_t)(pg->pages - 1u);
        s->have[0] = s->have[1] = 0;
    }
    for (unsigned i = 0; i < 2u; i++) {
        if (pg->page != s->first_page + i)
            continue;
        if (s->edit_on && (uint32_t)(now_ms - s->edit_ms) < JLST_EDIT_HOLD_MS)
            continue;                  /* what a press just changed: the next read will agree */
        s->pg[i] = *pg;
        s->have[i] = 1;
    }
}

void jlst_on_push(struct jlst *s, uint8_t track, uint8_t step, uint32_t now_ms)
{
    s->head_on = 1;
    s->head_track = track;
    s->head_step = step;
    s->head_ms = now_ms;
}

size_t jlst_release(struct jlst *s, struct jlapp_act *out, size_t max)
{
    size_t n = 0;
    for (unsigned i = 0; i < JLST_KEYS && n < max; i++)
        if (s->key_note[i] >= 0) {
            set_act(&out[n++], JLAPP_ACT_NOTE_OFF, s->key_track[i], (uint8_t)s->key_note[i], 0);
            s->key_note[i] = -1;
        }
    for (uint8_t t = 0; t < JLST_KEYS && n < max; t++)
        if (s->track_held & (1u << t)) {
            set_act(&out[n++], JLAPP_ACT_TRACK_OFF, t, 0, 0);
            s->track_held &= (uint16_t)~(1u << t);
        }
    return n;
}

/* ---- lights ---- */

static uint8_t step_colour(const struct jlst *s, const struct jlst_ctx *c, unsigned k)
{
    unsigned p = k / 16u, idx = k % 16u, abs_page = s->first_page + p, abs_step = abs_page * 16u + idx;
    const struct jdl_step *st;
    if (!is_cached(s, c->n->track) || !s->have[p] || abs_page >= s->pages_known)
        return 0;
    if (s->pg[p].length && abs_step >= s->pg[p].length)
        return 0;                      /* past the pattern's length */
    if (s->head_on && s->head_track == c->n->track && s->head_step == abs_step && (uint32_t)(c->now_ms - s->head_ms) < JLST_HEAD_MS)
        return JLST_COLOR_HEAD;
    st = &s->pg[p].step[idx];
    return st->trig ? JLST_COLOR_ON : JLST_COLOR_OFF;
}

static uint8_t quadrant_colour(const struct jlst *s, const struct jlst_ctx *c, uint8_t x, uint8_t y)
{
    unsigned i = y * QUAD + x;
    if (s->kind == JLST_DRUM) {
        uint8_t t = (uint8_t)((QUAD - 1u - y) * QUAD + x);
        if (s->track_held & (1u << t))
            return JLST_COLOR_TRACK_PLAYING;
        return t == c->n->track ? JLST_COLOR_TRACK_CURRENT : JLST_COLOR_TRACK_IDLE;
    } else {
        struct jln_pad p;
        jln_pad(c->n, c->scale, x, y, &p);
        if (p.note < 0)
            return 0;
        if (s->key_note[i] >= 0)
            return JLN_COLOR_PLAYING;
        return p.note == s->note ? JLST_COLOR_HEAD : jln_color(&p, 0);
    }
}

void jlst_frame(const struct jlst *s, const struct jlst_ctx *c, struct jlx_frame *f)
{
    for (uint8_t y = 0; y < 8u; y++)
        for (uint8_t x = 0; x < 8u; x++) {
            uint8_t col = 0;
            if (y >= STEP_ROWS_FROM)
                col = step_colour(s, c, (7u - y) * 8u + x);
            else if (x < QUAD)
                col = quadrant_colour(s, c, x, y);
            else if (y == PAGE_ROW && (!s->pages_known || (x - QUAD) < s->pages_known))
                col = (x - QUAD) == s->first_page || (x - QUAD) == s->first_page + 1u ? JLST_COLOR_PAGE_ON : JLST_COLOR_PAGE_OFF;
            f->led[JLX_PAD0 + y * 8u + x].color = col;
        }
}
