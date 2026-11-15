/* SPDX-License-Identifier: GPL-3.0-only */
/* A hosted USB audio device's descriptors and rate requests (jank_uacdesc.h). */
#include "jank_uacdesc.h"
#include <string.h>

#define DT_INTERFACE 0x04u
#define DT_ENDPOINT 0x05u
#define DT_CS_INTERFACE 0x24u
#define DT_CS_ENDPOINT 0x25u
#define CLASS_AUDIO 0x01u
#define SUB_CONTROL 0x01u
#define SUB_STREAMING 0x02u
#define PROTO_V2 0x20u
#define AC_OUTPUT_TERMINAL 0x03u
#define AC_CLOCK_SOURCE 0x0Au
#define AC_CLOCK_SELECTOR 0x0Bu
#define AC_CLOCK_MULTIPLIER 0x0Cu
#define AS_GENERAL 0x01u
#define AS_FORMAT_TYPE 0x02u
#define FORMAT_TYPE_I 0x01u
#define EP_GENERAL 0x01u
#define ENTITIES 16u
#define CLOCK_HOPS 4

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le24(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16; }
static uint32_t le32(const uint8_t *p) { return le24(p) | (uint32_t)p[3] << 24; }

/* the control interface's entities that lead to a clock: id -> the clock-ish entity it names (kind: 1 terminal,
 * 2 source, 3 selector, 4 multiplier) */
struct entity {
    uint8_t id, kind, next;
};

struct parse {
    struct ju_dev *d;
    struct entity ent[ENTITIES];
    uint8_t n_ent;
    uint8_t link[JU_ALTS_MAX];
    /* the alternate setting being read */
    uint8_t in_as, in_ac, uac2, pcm, format_ok, have_ep, link_cur;
    struct ju_alt cur;
};

static void add_entity(struct parse *p, uint8_t id, uint8_t kind, uint8_t next)
{
    if (p->n_ent < ENTITIES)
        p->ent[p->n_ent++] = (struct entity){id, kind, next};
}

static void finish_alt(struct parse *p)
{
    struct ju_dev *d = p->d;
    if (p->in_as && p->have_ep && p->pcm && p->format_ok && p->cur.channels && p->cur.subframe >= 2u && p->cur.subframe <= 4u
        && d->n < JU_ALTS_MAX) {
        p->link[d->n] = p->link_cur;
        d->alt[d->n++] = p->cur;
    }
    p->in_as = 0;
}

static void on_interface(struct parse *p, const uint8_t *x)
{
    finish_alt(p);
    p->in_ac = 0;
    if (x[5] != CLASS_AUDIO)
        return;
    if (x[6] == SUB_CONTROL) {
        p->in_ac = 1;
        p->d->ac_itf = x[2];
        p->d->uac2 = (uint8_t)(x[7] == PROTO_V2);
    } else if (x[6] == SUB_STREAMING && x[4] > 0u) {
        memset(&p->cur, 0, sizeof p->cur);
        p->cur.itf = x[2];
        p->cur.alt = x[3];
        p->in_as = 1;
        p->uac2 = (uint8_t)(x[7] == PROTO_V2);
        p->pcm = p->format_ok = p->have_ep = p->link_cur = 0;
    }
}

static void on_ac(struct parse *p, const uint8_t *x, uint8_t n)
{
    if (x[2] == AC_OUTPUT_TERMINAL && n >= 12u)        /* 2.0's output terminal has bCSourceID at 8 (1.0's is 9 long) */
        add_entity(p, x[3], 1, x[8]);
    else if (x[2] == AC_CLOCK_SOURCE && n >= 8u)
        add_entity(p, x[3], 2, 0);
    else if (x[2] == AC_CLOCK_SELECTOR && n >= 6u && x[4] > 0u)
        add_entity(p, x[3], 3, x[5]);                  /* its first input: the selector's state is not asked for */
    else if (x[2] == AC_CLOCK_MULTIPLIER && n >= 5u)
        add_entity(p, x[3], 4, x[4]);
}

static void on_as(struct parse *p, const uint8_t *x, uint8_t n)
{
    struct ju_alt *a = &p->cur;
    if (x[2] == AS_GENERAL) {
        if (!p->uac2 && n >= 7u) {
            p->link_cur = x[3];
            p->pcm = (uint8_t)(le16(x + 5) == 0x0001u);
        } else if (p->uac2 && n >= 16u) {
            p->link_cur = x[3];
            p->pcm = (uint8_t)(x[5] == FORMAT_TYPE_I && (x[6] & 1u));
            a->channels = x[10];
        }
    } else if (x[2] == AS_FORMAT_TYPE && n >= 4u && x[3] == FORMAT_TYPE_I) {
        if (!p->uac2 && n >= 8u) {
            uint8_t k = x[7];
            a->channels = x[4];
            a->subframe = x[5];
            a->bits = x[6];
            if (k == 0u && n >= 14u) {
                a->continuous = 1;
                a->rates[0] = le24(x + 8);
                a->rates[1] = le24(x + 11);
                a->nrates = 2;
            } else {
                for (uint8_t i = 0; i < k && a->nrates < JU_RATES_MAX && 8u + 3u * i + 3u <= n; i++)
                    a->rates[a->nrates++] = le24(x + 8 + 3 * i);
            }
            p->format_ok = 1;
        } else if (p->uac2 && n >= 6u) {
            a->subframe = x[4];
            a->bits = x[5];
            p->format_ok = 1;
        }
    }
}

static void on_endpoint(struct parse *p, const uint8_t *x)
{
    /* isochronous, IN, carrying audio: usage data (00) or implicit feedback data (10), not an explicit feedback endpoint
     * (01) or reserved (11) (USB 2.0 Table 9-13); the first one only */
    if (p->have_ep || (x[3] & 3u) != 1u || !(x[2] & 0x80u) || ((x[3] >> 4) & 1u) != 0u)
        return;
    p->cur.ep = x[2];
    p->cur.mps = (uint16_t)(le16(x + 4) & 0x7FFu);
    p->have_ep = 1;
}

/* a terminal's clock source, through selectors and multipliers */
static uint8_t clock_of(const struct parse *p, uint8_t terminal)
{
    uint8_t id = terminal;
    int want_terminal = 1;
    for (int hop = 0; hop < CLOCK_HOPS; hop++) {
        const struct entity *e = NULL;
        for (uint8_t i = 0; i < p->n_ent; i++)
            if (p->ent[i].id == id && (want_terminal ? p->ent[i].kind == 1u : p->ent[i].kind != 1u))
                e = &p->ent[i];
        if (!e)
            return 0;
        if (e->kind == 2u)
            return e->id;
        id = e->next;
        want_terminal = 0;
    }
    return 0;
}

uint8_t ju_parse(const uint8_t *cfg, size_t len, struct ju_dev *d)
{
    struct parse p;
    size_t at = 0;
    memset(d, 0, sizeof *d);
    memset(&p, 0, sizeof p);
    p.d = d;
    while (at + 2u <= len) {
        const uint8_t *x = cfg + at;
        uint8_t n = x[0];
        if (n < 2u || at + n > len)
            break;                         /* broken: what was read so far stands */
        if (x[1] == DT_INTERFACE && n >= 9u)
            on_interface(&p, x);
        else if (x[1] == DT_CS_INTERFACE && n >= 3u && p.in_ac)
            on_ac(&p, x, n);
        else if (x[1] == DT_CS_INTERFACE && n >= 3u && p.in_as)
            on_as(&p, x, n);
        else if (x[1] == DT_ENDPOINT && n >= 7u && p.in_as)
            on_endpoint(&p, x);
        else if (x[1] == DT_CS_ENDPOINT && n >= 4u && p.in_as && x[2] == EP_GENERAL && p.have_ep)
            p.cur.freq_ctrl = (uint8_t)(x[3] & 1u);
        at += n;
    }
    finish_alt(&p);
    for (uint8_t i = 0; i < d->n; i++)
        if (d->uac2)
            d->alt[i].clock = clock_of(&p, p.link[i]);
    return d->n;
}

static const uint32_t KNOWN[] = {44100u, 48000u, 88200u, 96000u};

static void add_rate(uint32_t *r, uint8_t *n, uint32_t hz)
{
    for (uint8_t i = 0; i < *n; i++)
        if (r[i] == hz)
            return;
    if (*n < JU_RATES_MAX)
        r[(*n)++] = hz;
}

uint8_t ju_clock_rates(struct ju_dev *d, uint8_t clock, const uint8_t *range, size_t len)
{
    uint32_t rates[JU_RATES_MAX];
    uint8_t n = 0;
    uint16_t sub;
    if (len < 2u)
        return 0;
    sub = le16(range);
    for (uint16_t s = 0; s < sub && 2u + 12u * (s + 1u) <= len; s++) {
        uint32_t lo = le32(range + 2 + 12 * s), hi = le32(range + 6 + 12 * s), res = le32(range + 10 + 12 * s);
        if (lo == hi) {
            add_rate(rates, &n, lo);
            continue;
        }
        for (size_t k = 0; k < sizeof KNOWN / sizeof KNOWN[0]; k++)
            if (KNOWN[k] >= lo && KNOWN[k] <= hi && (res == 0u || (KNOWN[k] - lo) % res == 0u))
                add_rate(rates, &n, KNOWN[k]);
    }
    for (uint8_t i = 0; i < d->n; i++)
        if (d->alt[i].clock == clock) {
            memcpy(d->alt[i].rates, rates, sizeof rates);
            d->alt[i].nrates = n;
            d->alt[i].continuous = 0;
        }
    return n;
}

static int has_rate(const struct ju_alt *a, uint32_t hz)
{
    if (a->continuous)
        return a->nrates == 2u && hz >= a->rates[0] && hz <= a->rates[1];
    for (uint8_t i = 0; i < a->nrates; i++)
        if (a->rates[i] == hz)
            return 1;
    return 0;
}

int ju_choose(const struct ju_dev *d, struct ju_pick *p)
{
    static const uint32_t WANT[] = {44100u, 48000u};
    uint64_t best = UINT64_MAX;
    for (uint8_t i = 0; i < d->n; i++) {
        const struct ju_alt *a = &d->alt[i];
        for (uint32_t w = 0; w < sizeof WANT / sizeof WANT[0]; w++) {
            uint32_t per_ms = (WANT[w] + 999u) / 1000u * a->channels * a->subframe;
            uint64_t score;
            if (!has_rate(a, WANT[w]) || a->channels > 8u || a->mps > JU_MPS_MAX || a->mps < per_ms)
                continue;
            score = (uint64_t)w << 32 | (uint64_t)WANT[w] * a->channels * a->subframe;
            if (score < best) {
                best = score;
                p->index = i;
                p->rate = WANT[w];
            }
        }
    }
    return best == UINT64_MAX ? -1 : 0;
}

static void setup(uint8_t s[8], uint8_t type, uint8_t req, uint16_t value, uint16_t index, uint16_t length)
{
    s[0] = type;
    s[1] = req;
    s[2] = (uint8_t)value;
    s[3] = (uint8_t)(value >> 8);
    s[4] = (uint8_t)index;
    s[5] = (uint8_t)(index >> 8);
    s[6] = (uint8_t)length;
    s[7] = (uint8_t)(length >> 8);
}

void ju_req_set_interface(uint8_t s[8], uint8_t itf, uint8_t alt) { setup(s, 0x01, 0x0B, alt, itf, 0); }

void ju_req_uac1_rate(uint8_t s[8], uint8_t data[3], uint8_t ep, uint32_t hz)
{
    setup(s, 0x22, 0x01, 0x0100, ep, 3);
    data[0] = (uint8_t)hz;
    data[1] = (uint8_t)(hz >> 8);
    data[2] = (uint8_t)(hz >> 16);
}

void ju_req_uac2_rate(uint8_t s[8], uint8_t data[4], uint8_t clock, uint8_t ac_itf, uint32_t hz)
{
    setup(s, 0x21, 0x01, 0x0100, (uint16_t)(clock << 8 | ac_itf), 4);
    data[0] = (uint8_t)hz;
    data[1] = (uint8_t)(hz >> 8);
    data[2] = (uint8_t)(hz >> 16);
    data[3] = (uint8_t)(hz >> 24);
}

void ju_req_uac2_range(uint8_t s[8], uint8_t clock, uint8_t ac_itf)
{
    setup(s, 0xA1, 0x02, 0x0100, (uint16_t)(clock << 8 | ac_itf), JU_RANGE_LEN);
}

size_t ju_string_ascii(const uint8_t *desc, size_t len, char *out, size_t cap)
{
    size_t w = 0, n;
    if (!cap)
        return 0;
    n = len >= 2u && desc[0] <= len ? desc[0] : len;
    for (size_t i = 2; i + 1u < n && w + 1u < cap; i += 2) {
        uint16_t c = le16(desc + i);
        out[w++] = c >= 0x20u && c < 0x7Fu ? (char)c : '?';
    }
    out[w] = 0;
    return w;
}
