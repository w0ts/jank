/* SPDX-License-Identifier: GPL-3.0-only */
/* The Digitone's live control, stage 9 (jank_dn2live.h): Jank's page on its screen, its keys, OPEN, LINK MODE and its requests. */
#include <string.h>
#include "jank_dn2live.h"
#include "jank_dn2rpc.h"
#include "jank_oled_text.h"

#define ST_NOT_UNDERSTOOD 2u
#define PAGE_HEAD 4u                   /* flags, sel, cursor, n */
#define OPEN_BYTES 4u                  /* open, reason, u16 pages drawn */
#define LREQ_BYTES 4u                  /* op, flags, state, id */
#define KEY_BYTES 3u                   /* kind, code, delta */

static uint32_t u32be(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t u16be(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static size_t send(struct jdl *d, uint32_t now_ms, uint8_t cmd, const uint8_t *args, size_t nargs, uint8_t *out, size_t cap)
{
    size_t n = jd2_request(++d->seq, cmd, args, nargs, out, cap);
    if (n) {
        d->asked = 1;
        d->sent_ms = now_ms;
    }
    return n;
}

/* a line's length byte: the length, and how the OLED places it while the Digitone takes the bits (never both: a centred line has no right part) */
static uint8_t line_byte(const struct jot_page *p, unsigned i, size_t len, int bits)
{
    uint8_t b = (uint8_t)len;
    if (!bits)
        return b;
    if (p->right[i])
        b |= JDL_LINE_SPLIT;
    else if (p->centred >> i & 1u)
        b |= JDL_LINE_CENTRED;
    return b;
}

size_t jdl_jank_page(struct jdl *d, uint32_t now_ms, const struct jot_page *p, uint8_t *out, size_t cap)
{
    uint8_t args[JDL_PAGE_ARGS_MAX];
    size_t k = PAGE_HEAD, n;
    int bits = 0;
    if (d->state != JDL_LIVE || !d->jank_ok || !p)
        return 0;
    args[0] = p->flags & (JDL_PAGE_CLOSE | JDL_PAGE_TITLE);
    args[1] = p->sel;
    args[2] = p->cursor;
    args[3] = p->n <= JDL_PAGE_LINES ? p->n : JDL_PAGE_LINES;
    for (unsigned i = 0; i < args[3]; i++) {
        size_t len = 0;
        while (len < JDL_PAGE_COLS && p->line[i][len])
            len++;
        args[k] = line_byte(p, i, len, d->align_ok);
        bits |= (args[k] & (JDL_LINE_CENTRED | JDL_LINE_SPLIT)) != 0;
        for (size_t c = 0; c < len; c++)       /* (the page's characters are 0x20..0x7e already: anything else would be refused) */
            args[k + 1u + c] = (unsigned char)p->line[i][c] >= 0x20u && (unsigned char)p->line[i][c] <= 0x7eu ? (uint8_t)p->line[i][c] : (uint8_t)'?';
        k += 1u + len;
    }
    n = send(d, now_ms, JDL_CMD_JANK_PAGE, args, k, out, cap);
    if (n && bits) {
        d->page_seq = d->seq;
        d->page_bits = 1;
    }
    return n;
}

size_t jdl_probe_jank(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap)
{
    size_t n;
    if (d->state != JDL_LIVE || !d->jank_ok || d->jank_probed)
        return 0;
    n = send(d, now_ms, JDL_CMD_JANK_OPEN, NULL, 0, out, cap);
    if (n)
        d->jank_probed = 1;
    return n;
}

size_t jdl_link_mode(struct jdl *d, uint32_t now_ms, uint8_t op, uint16_t ms, uint8_t id, uint8_t *out, size_t cap)
{
    uint8_t args[4] = {op, 0, 0, 0};
    size_t k = 1;
    if (d->state != JDL_LIVE || !d->link_ok || (op > JDL_LINK_ACK && op != JDL_LINK_READ))
        return 0;
    if (op == JDL_LINK_PENDING) {
        args[1] = (uint8_t)(ms >> 8);
        args[2] = (uint8_t)ms;
        args[3] = id;
        k = 4;
    } else if (op == JDL_LINK_ACK) {
        args[1] = id;
        k = 2;
    }
    return send(d, now_ms, JDL_CMD_LINK_MODE, args, k, out, cap);
}

int jdl_take_keys(struct jdl *d, struct jdl_keys *out)
{
    if (!d->keys_fresh)
        return 0;
    *out = d->keys;
    d->keys_fresh = 0;
    return 1;
}

int jdl_take_open(struct jdl *d, struct jdl_open *out)
{
    if (!d->open_fresh)
        return 0;
    *out = d->open;
    d->open_fresh = 0;
    return 1;
}

int jdl_take_link_request(struct jdl *d, struct jdl_link_req *out)
{
    if (!d->lreq_fresh)
        return 0;
    *out = d->lreq;
    d->lreq_fresh = 0;
    return 1;
}

int jdl_take_page_resend(struct jdl *d)
{
    int r = d->page_resend;
    d->page_resend = 0;
    return r;
}

int jdl_take_live8(struct jdl *d, struct jdl_live8 *out)
{
    if (!d->p8_fresh)
        return 0;
    *out = d->p8;
    d->p8_fresh = 0;
    return 1;
}

/* JANK KEY's data: u8 n (1..8), n x {kind, code, delta} */
static int take_keys(struct jdl *d, const struct jd2_reply *r)
{
    uint8_t n;
    if (r->n < 1u)
        return 0;
    n = r->data[0];
    if (n < 1u || n > JDL_KEYS_MAX || r->n < 1u + (size_t)n * KEY_BYTES)
        return 0;
    d->keys.n = n;
    for (unsigned i = 0; i < n; i++) {
        const uint8_t *k = r->data + 1u + i * KEY_BYTES;
        d->keys.k[i] = (struct jdl_key){k[0], k[1], (int8_t)k[2]};
    }
    d->keys_fresh = 1;
    return 1;
}

static int take_open(struct jdl *d, const struct jd2_reply *r)
{
    if (r->n < OPEN_BYTES)
        return 0;
    d->open = (struct jdl_open){(uint8_t)(r->data[0] != 0), r->data[1], u16be(r->data + 2)};
    d->open_fresh = 1;
    return 1;
}

static int take_link_request(struct jdl *d, const struct jd2_reply *r)
{
    if (r->n < LREQ_BYTES)
        return 0;
    d->lreq = (struct jdl_link_req){r->data[0], r->data[1], r->data[2], r->data[3]};
    d->lreq_fresh = 1;
    return 1;
}

/* a refusal (2) of a page: the placing bits (a build that refuses them, lp9) when it is one of the pages sent with them (at or before the last such: several may be
 * on their way), else stage 9 itself */
static void page_refused(struct jdl *d, const struct jd2_reply *r)
{
    if (d->page_bits && (uint16_t)(d->page_seq - r->seq) < 0x8000u) {
        if (d->align_ok)
            d->page_resend = 1;
        d->align_ok = 0;
    } else
        d->jank_ok = 0;
}

int jdl9_input(struct jdl *d, const struct jd2_reply *r, enum jdl_in *kind)
{
    *kind = JDL_IN_JANK;
    switch (r->cmd) {
    case JDL_CMD_JANK_KEY:             /* pushes: status 0 always; a damaged one is not taken */
    case JDL_CMD_LINK_REQUEST:
        if (r->status != 0 || !(r->cmd == JDL_CMD_JANK_KEY ? take_keys(d, r) : take_link_request(d, r)))
            *kind = JDL_IN_IGNORED;
        return 1;
    case JDL_CMD_JANK_OPEN:            /* the push, or the reply to the read: the same data; it moves no state of live control's */
        if (r->status == ST_NOT_UNDERSTOOD) {
            d->jank_ok = 0;
            *kind = JDL_IN_REPLY;
            return 1;
        }
        if (r->status != 0)
            return 0;                  /* 4 / 7: live control's own, in jdl_input */
        if (!take_open(d, r))
            *kind = JDL_IN_IGNORED;
        return 1;
    case JDL_CMD_JANK_PAGE:
    case JDL_CMD_LINK_MODE:
        if (r->status != ST_NOT_UNDERSTOOD)
            return 0;
        if (r->cmd == JDL_CMD_JANK_PAGE)
            page_refused(d, r);
        else
            d->link_ok = 0;
        *kind = JDL_IN_REPLY;
        return 1;
    default:
        return 0;
    }
}

void jdl9_status(struct jdl *d, const uint8_t *s, size_t n)
{
    struct jdl_live8 *p = &d->p8;
    d->p8_valid = s[0] >= 8u && n >= JDL_P8_BYTES;
    if (!d->p8_valid)
        return;
    s += JDL_P8_BYTE;
    p->open = s[0];
    p->reason = s[1];
    p->sel = s[2];
    p->drawn = u16be(s + 3);
    p->pages = u32be(s + 5);
    p->keys = u32be(s + 9);
    p->keys_lost = u32be(s + 13);
    p->link_on = s[17];
    p->pending = s[18];
    p->req_op = s[19];
    p->req_flags = s[20];
    p->req_id = s[21];
    p->acked = s[22];
    p->requests = u32be(s + 23);
    d->p8_fresh = 1;
}
