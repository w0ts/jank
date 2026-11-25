/* SPDX-License-Identifier: GPL-3.0-only */
/* The Launchpad's Session page (jank_lpsess.h). */
#include "jank_lpsess.h"
#include <string.h>

#define ROWS_PER_BANK 2u
#define PADS_PER_ROW 8u

void jlss_init(struct jlss *s)
{
    memset(s, 0, sizeof *s);
    s->playing = JLSS_UNKNOWN;
    s->cued = JDL_NO_PATTERN;
    s->sweep = JDL_BANKS;              /* nothing to read until the page opens */
}

void jlss_open(struct jlss *s)
{
    s->sweep = 0;
    s->asked = 0;
    s->cue_due = 1;
    s->retry_due = 0;
}

void jlss_forget(struct jlss *s)
{
    uint8_t half = s->half, sweep = s->sweep;
    jlss_init(s);
    s->half = half;                    /* the page shown stays */
    s->sweep = sweep;
}

/* the first bank not read yet, -1 if every one has been */
static int missing_bank(const struct jlss *s)
{
    for (unsigned b = 0; b < JDL_BANKS; b++)
        if (!(s->have & (1u << b)))
            return (int)b;
    return -1;
}

static int bank_req(struct jlss *s, uint32_t now_ms, uint8_t bank, struct jlss_req *r)
{
    s->asked = 1;
    s->last_ms = now_ms;
    r->kind = JLSS_REQ_BANK;
    r->bank = bank;
    return 1;
}

int jlss_due(struct jlss *s, uint32_t now_ms, int reads_ok, int cue_ok, struct jlss_req *r)
{
    if (!reads_ok)
        s->sweep = JDL_BANKS;          /* a Digitone without PATTERNS: nothing to read but the cue */
    if (s->sweep < JDL_BANKS) {
        if (s->asked && (uint32_t)(now_ms - s->last_ms) < JLSS_OPEN_GAP_MS)
            return 0;
        return bank_req(s, now_ms, s->sweep++, r);
    }
    if (s->cue_due && cue_ok) {
        s->cue_due = 0;
        r->kind = JLSS_REQ_CUE_READ;
        r->bank = 0;
        return 1;
    }
    s->cue_due = 0;
    if (!reads_ok)
        return 0;
    if (s->retry_due) {                /* one bank whose answer never came (a request lost on the way), at most one per poll */
        int b = missing_bank(s);
        s->retry_due = 0;
        if (b >= 0) {
            r->kind = JLSS_REQ_BANK;
            r->bank = (uint8_t)b;
            return 1;
        }
    }
    if (!s->asked || (uint32_t)(now_ms - s->last_ms) < JLSS_POLL_MS)   /* (not open: nothing) */
        return 0;
    s->cue_due = 1;
    s->retry_due = 1;
    return bank_req(s, now_ms, JDL_BANK_PLAYING, r);
}

void jlss_on_patterns(struct jlss *s, const struct jdl_patterns *p)
{
    if (p->bank >= JDL_BANKS || p->playing >= JDL_PATTERNS)
        return;
    s->mask[p->bank] = p->mask;
    s->have |= (uint8_t)(1u << p->bank);
    s->playing = p->playing;
    if (s->cued == s->playing)         /* the cued pattern plays now */
        s->cued = JDL_NO_PATTERN;
}

void jlss_on_pattern(struct jlss *s, const struct jdl_pattern *p)
{
    if (p->playing >= JDL_PATTERNS || (p->cued >= JDL_PATTERNS && p->cued != JDL_NO_PATTERN))
        return;
    s->playing = p->playing;
    s->cued = p->cued == p->playing ? (uint8_t)JDL_NO_PATTERN : p->cued;
}

uint8_t jlss_pattern_of_pad(const struct jlss *s, uint8_t x, uint8_t y)
{
    unsigned r = 7u - (y & 7u), bank = s->half * JLSS_BANKS_SHOWN + r / ROWS_PER_BANK;
    return (uint8_t)(bank * JDL_BANK_PATTERNS + (r % ROWS_PER_BANK) * PADS_PER_ROW + (x & 7u));
}

void jlss_cue(struct jlss *s, uint8_t pattern)
{
    if (pattern < JDL_PATTERNS)
        s->cued = pattern;
}

int jlss_half_step(struct jlss *s, int dir)
{
    if (dir < 0 && s->half > 0u) {
        s->half--;
        return 1;
    }
    if (dir > 0 && s->half + 1u < JLSS_HALVES) {
        s->half++;
        return 1;
    }
    return 0;
}

void jlss_name(uint8_t pattern, char out[4])
{
    unsigned k = pattern % JDL_BANK_PATTERNS + 1u;
    out[0] = (char)('A' + (pattern / JDL_BANK_PATTERNS) % JDL_BANKS);
    out[1] = (char)('0' + k / 10u);
    out[2] = (char)('0' + k % 10u);
    out[3] = 0;
}

void jlss_frame(const struct jlss *s, struct jlx_frame *f)
{
    for (uint8_t y = 0; y < 8u; y++)
        for (uint8_t x = 0; x < 8u; x++) {
            struct jlx_led *l = &f->led[JLX_PAD0 + y * 8u + x];
            uint8_t p = jlss_pattern_of_pad(s, x, y), bank = p / JDL_BANK_PATTERNS;
            memset(l, 0, sizeof *l);
            if (p == s->playing) {
                l->color = JLSS_COLOR_PLAYING;
                l->mode = JLX_PULSE;
            } else if (p == s->cued) {
                l->color = JLSS_COLOR_CUED;
                l->alt = 0;                /* flashes with dark */
                l->mode = JLX_FLASH;
            } else if ((s->have & (1u << bank)) && (s->mask[bank] & (1u << (p % JDL_BANK_PATTERNS)))) {
                l->color = JLSS_COLOR_FILLED;
            }
        }
}
