/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the Digitone's live control, stage 9 (core/jank_dn2live9.c): the JANK PAGE's bytes (the placing bits, and plain pages after a build refuses them),
 * OPEN, LINK MODE's ops, the key, OPEN and LINK REQUEST pushes, LIVE STATUS protocol 8, the refusals. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_dn2live.h"
#include "jank_dn2rpc.h"
#include "jank_oled_text.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const uint8_t HEADER[6] = {0xF0, 0x00, 0x20, 0x3C, 0x7D, 0x01};

/* a frame from the Digitone: F0 .. header, pack7(u16 seq, cmd byte, status, data), F7 */
static size_t frame(uint8_t *out, uint16_t seq, uint8_t cmd_byte, uint8_t status, const uint8_t *data, size_t n)
{
    uint8_t body[4 + 128];
    size_t k;
    body[0] = (uint8_t)(seq >> 8);
    body[1] = (uint8_t)seq;
    body[2] = cmd_byte;
    body[3] = status;
    if (n)
        memcpy(body + 4, data, n);
    memcpy(out, HEADER, sizeof HEADER);
    k = sizeof HEADER;
    k += jd2_pack7(body, 4 + n, out + k);
    out[k++] = 0xF7;
    return k;
}

static size_t request_body(const uint8_t *sx, size_t n, uint8_t *body)
{
    if (n < 8 || memcmp(sx, HEADER, 6) || sx[n - 1] != 0xF7)
        return 0;
    return jd2_unpack7(sx + 6, n - 7, body);
}

static enum jdl_in input(struct jdl *d, uint16_t seq, uint8_t cmd_byte, uint8_t status, const uint8_t *data, size_t n)
{
    uint8_t f[JD2_SX_MAX];
    struct jdl_note notes[JDL_NOTES_MAX];
    size_t count;
    return jdl_input(d, f, frame(f, seq, cmd_byte, status, data, n), notes, &count);
}

static struct jdl live(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(jdl_poll(&d, 1000, out, sizeof out) > 0);
    CHECK(input(&d, d.seq, JDL_CMD_LIVE | 0x80u, 0, NULL, 0) == JDL_IN_REPLY && jdl_is_live(&d));
    return d;
}

/* a menu page: the title split (MENU  1/11), a plain row, a split row selected, a centred line */
static void a_page(struct jot_page *p)
{
    memset(p, 0, sizeof *p);
    p->flags = JOT_F_TITLE;
    p->sel = 2;
    p->cursor = JOT_NONE;
    p->n = 4;
    snprintf(p->line[0], sizeof p->line[0], "MENU  1/11");
    p->right[0] = 6;
    snprintf(p->line[1], sizeof p->line[1], "AUDIO");
    snprintf(p->line[2], sizeof p->line[2], "LINK TRANSPORT  BOTH");
    p->right[2] = 16;
    snprintf(p->line[3], sizeof p->line[3], "YES NEXT  NO DEL");
    p->centred = 1u << 3;
}

static void test_page_bytes(void)
{
    struct jdl d = live();
    struct jot_page p;
    uint8_t out[JDL_JANK_PAGE_SX_MAX], body[256];
    size_t n, k;
    a_page(&p);
    n = jdl_jank_page(&d, 2000, &p, out, sizeof out);
    k = request_body(out, n, body);
    CHECK(k == 3u + 4u + (1 + 10) + (1 + 5) + (1 + 20) + (1 + 16));
    CHECK(body[2] == JDL_CMD_JANK_PAGE && body[3] == JDL_PAGE_TITLE && body[4] == 2 && body[5] == 255 && body[6] == 4);
    CHECK(body[7] == (10u | JDL_LINE_SPLIT) && !memcmp(body + 8, "MENU  1/11", 10));
    CHECK(body[18] == 5u && !memcmp(body + 19, "AUDIO", 5));                      /* plain: no bit */
    CHECK(body[24] == (20u | JDL_LINE_SPLIT) && !memcmp(body + 25, "LINK TRANSPORT  BOTH", 20));
    CHECK(body[45] == (16u | JDL_LINE_CENTRED) && !memcmp(body + 46, "YES NEXT  NO DEL", 16));
    for (size_t i = 7; i < k;) {                                                  /* never bit 5, never 6 and 7 together */
        CHECK(!(body[i] & 0x20u) && (body[i] & 0xC0u) != 0xC0u);
        i += 1u + (body[i] & JDL_LINE_LEN);
    }
    p.cursor = 3;                                                                 /* the editor: the cursor column goes as it is */
    p.flags = JOT_F_TITLE | 0x80u;                                                /* flags the page does not have are not sent */
    n = jdl_jank_page(&d, 2000, &p, out, sizeof out);
    CHECK(request_body(out, n, body) > 6 && body[5] == 3 && body[3] == JDL_PAGE_TITLE);
    memset(&p, 0, sizeof p);                                                      /* an empty page (n 0): just the head */
    p.sel = p.cursor = JOT_NONE;
    n = jdl_jank_page(&d, 2000, &p, out, sizeof out);
    CHECK(request_body(out, n, body) == 7u && body[6] == 0);
}

/* the longest page fits the frame JDL_JANK_PAGE_SX_MAX says */
static void test_longest_page(void)
{
    struct jdl d = live();
    struct jot_page p;
    uint8_t out[JDL_JANK_PAGE_SX_MAX], body[256];
    size_t n;
    memset(&p, 0, sizeof p);
    p.sel = p.cursor = JOT_NONE;
    p.n = JOT_LINES;
    for (int i = 0; i < JOT_LINES; i++)
        memset(p.line[i], 'A' + i, JOT_LINE_MAX);
    n = jdl_jank_page(&d, 2000, &p, out, sizeof out);
    CHECK(n == JDL_JANK_PAGE_SX_MAX && request_body(out, n, body) == 3u + JDL_PAGE_ARGS_MAX);
    CHECK(body[7] == 31 && body[3u + JDL_PAGE_ARGS_MAX - 1u] == 'G');
    CHECK(jdl_jank_page(&d, 2000, &p, out, sizeof out - 1u) == 0);
}

/* a build that refuses the placing bits (lp9): status 2 to a page with them -> the page again, plain, and plain pages for the rest of the connection; then a plain
 * page refused: no stage 9 at all, no more pages. jdl_init starts over */
static void test_refused_bits(void)
{
    struct jdl d = live();
    struct jot_page p;
    uint8_t out[JDL_JANK_PAGE_SX_MAX], body[256];
    size_t n;
    uint16_t first, second;
    a_page(&p);
    CHECK(jdl_jank_page(&d, 2000, &p, out, sizeof out) > 0);
    first = d.seq;
    CHECK(jdl_jank_page(&d, 2001, &p, out, sizeof out) > 0);                     /* a second one on its way */
    second = d.seq;
    CHECK(!jdl_take_page_resend(&d));
    CHECK(input(&d, first, JDL_CMD_JANK_PAGE | 0x80u, 2, NULL, 0) == JDL_IN_REPLY);
    CHECK(jdl_is_live(&d) && d.jank_ok && !d.align_ok);                          /* live control and the page are fine */
    CHECK(jdl_take_page_resend(&d) && !jdl_take_page_resend(&d));                /* once */
    CHECK(input(&d, second, JDL_CMD_JANK_PAGE | 0x80u, 2, NULL, 0) == JDL_IN_REPLY);   /* the other one with bits: the same cause */
    CHECK(d.jank_ok && !jdl_take_page_resend(&d));
    n = jdl_jank_page(&d, 2002, &p, out, sizeof out);
    CHECK(request_body(out, n, body) > 8 && body[7] == 10u && body[24] == 20u && body[45] == 16u);   /* plain */
    CHECK(input(&d, d.seq, JDL_CMD_JANK_PAGE | 0x80u, 2, NULL, 0) == JDL_IN_REPLY);   /* a plain page refused: no stage 9 */
    CHECK(!d.jank_ok && jdl_is_live(&d) && jdl_jank_page(&d, 2003, &p, out, sizeof out) == 0);
    jdl_init(&d);
    CHECK(d.jank_ok && d.align_ok && d.link_ok && !d.page_bits);
    d = live();                                                                   /* a build without stage 9: the first page (with bits) refused, then the plain one */
    jdl_jank_page(&d, 2000, &p, out, sizeof out);
    input(&d, d.seq, JDL_CMD_JANK_PAGE | 0x80u, 2, NULL, 0);
    CHECK(jdl_take_page_resend(&d) && d.jank_ok);
    jdl_jank_page(&d, 2001, &p, out, sizeof out);
    input(&d, d.seq, JDL_CMD_JANK_PAGE | 0x80u, 2, NULL, 0);
    CHECK(!d.jank_ok);
    d = live();                                                                   /* a page with no bits at all refused: stage 9 is not there */
    memset(&p, 0, sizeof p);
    p.sel = p.cursor = JOT_NONE;
    p.n = 1;
    snprintf(p.line[0], sizeof p.line[0], "AUDIO");
    jdl_jank_page(&d, 2000, &p, out, sizeof out);
    input(&d, d.seq, JDL_CMD_JANK_PAGE | 0x80u, 2, NULL, 0);
    CHECK(!d.jank_ok);
    d = live();                                                                   /* the lease ran out (4): live control's business, not the page's */
    jdl_jank_page(&d, 2000, &p, out, sizeof out);
    CHECK(input(&d, d.seq, JDL_CMD_JANK_PAGE | 0x80u, 4, NULL, 0) == JDL_IN_REPLY && d.state == JDL_ASKING && d.jank_ok);
    CHECK(jdl_jank_page(&d, 2001, &p, out, sizeof out) == 0);                    /* not live: nothing */
}

static void test_open_and_probe(void)
{
    struct jdl d = live();
    struct jdl_open o;
    uint8_t out[64], body[32];
    const uint8_t open1[4] = {1, 0, 0x01, 0x02}, short_open[3] = {1, 0, 0};
    size_t n = jdl_probe_jank(&d, 2000, out, sizeof out);
    CHECK(request_body(out, n, body) == 3u && body[2] == JDL_CMD_JANK_OPEN);       /* a read: no arguments */
    CHECK(jdl_probe_jank(&d, 2001, out, sizeof out) == 0);                        /* once per connection */
    CHECK(!jdl_take_open(&d, &o));
    CHECK(input(&d, d.seq, JDL_CMD_JANK_OPEN | 0x80u, 0, open1, sizeof open1) == JDL_IN_JANK);   /* the read's answer */
    CHECK(jdl_take_open(&d, &o) && o.open == 1 && o.reason == 0 && o.drawn == 0x0102 && !jdl_take_open(&d, &o));
    CHECK(input(&d, 0x4000, 0xC0, 0, (const uint8_t[]){0, 1, 0, 9}, 4) == JDL_IN_JANK);   /* pushed: closed by a key */
    CHECK(jdl_take_open(&d, &o) && o.open == 0 && o.reason == JDL_OPEN_KEY && o.drawn == 9);
    CHECK(input(&d, 0x4001, 0xC0, 0, short_open, sizeof short_open) == JDL_IN_IGNORED && !jdl_take_open(&d, &o));   /* damaged */
    jdl_init(&d);                                                                 /* an OPEN never makes live control: it is pushed only while live */
    CHECK(input(&d, 0x4002, 0xC0, 0, open1, sizeof open1) == JDL_IN_JANK && !jdl_is_live(&d));
    d = live();
    jdl_probe_jank(&d, 2000, out, sizeof out);
    CHECK(input(&d, d.seq, JDL_CMD_JANK_OPEN | 0x80u, 2, NULL, 0) == JDL_IN_REPLY);   /* a build without stage 9 */
    CHECK(!d.jank_ok && jdl_is_live(&d) && d.state == JDL_LIVE);
}

static void test_keys(void)
{
    struct jdl d = live();
    struct jdl_keys k;
    const uint8_t two[7] = {2, JDL_KEY_PRESS, JDL_KC_UP, 0, JDL_KEY_TURN, 41, (uint8_t)-3};
    uint8_t nine[1 + 9 * 3] = {9}, cut[5] = {2, 1, 11, 0, 2};
    CHECK(input(&d, 0x0100, 0xBF, 0, two, sizeof two) == JDL_IN_JANK);
    CHECK(jdl_take_keys(&d, &k) && k.n == 2 && k.k[0].kind == JDL_KEY_PRESS && k.k[0].code == JDL_KC_UP && k.k[1].kind == JDL_KEY_TURN && k.k[1].delta == -3);
    CHECK(!jdl_take_keys(&d, &k));
    CHECK(input(&d, 0x0101, 0xBF, 0, nine, sizeof nine) == JDL_IN_IGNORED && !jdl_take_keys(&d, &k));   /* more than 8 */
    CHECK(input(&d, 0x0102, 0xBF, 0, cut, sizeof cut) == JDL_IN_IGNORED);       /* shorter than it says */
    CHECK(input(&d, 0x0103, 0xBF, 0, (const uint8_t[]){0}, 1) == JDL_IN_IGNORED);   /* none */
    CHECK(input(&d, 0x0104, 0xBF, 3, two, sizeof two) == JDL_IN_IGNORED);       /* a push is status 0 */
}

static void test_link_mode(void)
{
    struct jdl d = live();
    struct jdl_link_req r;
    uint8_t out[64], body[32];
    size_t n = jdl_link_mode(&d, 2000, JDL_LINK_ON, 0, 0, out, sizeof out);
    CHECK(request_body(out, n, body) == 4u && body[2] == JDL_CMD_LINK_MODE && body[3] == 1);
    n = jdl_link_mode(&d, 2000, JDL_LINK_PENDING, 1234, 7, out, sizeof out);
    CHECK(request_body(out, n, body) == 7u && body[3] == 2 && body[4] == 0x04 && body[5] == 0xD2 && body[6] == 7);
    n = jdl_link_mode(&d, 2000, JDL_LINK_ACK, 0, 9, out, sizeof out);
    CHECK(request_body(out, n, body) == 5u && body[3] == 4 && body[4] == 9);
    n = jdl_link_mode(&d, 2000, JDL_LINK_CANCEL, 0, 0, out, sizeof out);
    CHECK(request_body(out, n, body) == 4u && body[3] == 3);
    n = jdl_link_mode(&d, 2000, JDL_LINK_READ, 0, 0, out, sizeof out);
    CHECK(request_body(out, n, body) == 4u && body[3] == 255);
    CHECK(jdl_link_mode(&d, 2000, 5, 0, 0, out, sizeof out) == 0);             /* not an op */
    CHECK(input(&d, 0x0200, 0xC2, 0, (const uint8_t[]){1, JDL_LR_RECORD, 0, 42}, 4) == JDL_IN_JANK);
    CHECK(jdl_take_link_request(&d, &r) && r.op == JDL_LR_START && r.flags == JDL_LR_RECORD && r.id == 42 && !jdl_take_link_request(&d, &r));
    CHECK(input(&d, 0x0201, 0xC2, 0, (const uint8_t[]){1, 0, 0}, 3) == JDL_IN_IGNORED);   /* damaged */
    jdl_link_mode(&d, 2000, JDL_LINK_ON, 0, 0, out, sizeof out);
    CHECK(input(&d, d.seq, JDL_CMD_LINK_MODE | 0x80u, 2, NULL, 0) == JDL_IN_REPLY);   /* refused: no more, live control is fine */
    CHECK(!d.link_ok && jdl_is_live(&d) && d.jank_ok && jdl_link_mode(&d, 2001, JDL_LINK_OFF, 0, 0, out, sizeof out) == 0);
    jdl_init(&d);
    CHECK(jdl_link_mode(&d, 2000, JDL_LINK_ON, 0, 0, out, sizeof out) == 0);   /* not live */
}

/* LIVE STATUS protocol 8: the stage-9 bytes after protocol 7's 89 */
static void test_status_p8(void)
{
    struct jdl d = live();
    uint8_t s[JDL_P8_BYTES];
    memset(s, 0, sizeof s);
    s[0] = 8;
    s[89] = 1;
    s[90] = 3;
    s[91] = 2;
    s[92] = 0x01, s[93] = 0x2C;                                                   /* 300 pages drawn */
    s[97] = 41, s[101] = 17, s[105] = 2;                                          /* pages received, keys pushed, keys lost */
    s[106] = 1, s[107] = 1, s[108] = 1, s[109] = JDL_LR_RECORD, s[110] = 42, s[111] = 0;
    s[112] = 0x01, s[115] = 0x05;                                                 /* 16777221 requests */
    CHECK(input(&d, d.seq, JDL_CMD_LIVE | 0x80u, 0, s, sizeof s) == JDL_IN_REPLY);
    CHECK(d.p8_valid && d.p8.open == 1 && d.p8.reason == 3 && d.p8.sel == 2 && d.p8.drawn == 300 && d.p8.pages == 41 && d.p8.keys == 17 && d.p8.keys_lost == 2);
    CHECK(d.p8.link_on == 1 && d.p8.pending == 1 && d.p8.req_op == 1 && d.p8.req_flags == JDL_LR_RECORD && d.p8.req_id == 42 && !d.p8.acked);
    CHECK(d.p8.requests == 0x01000005u && d.st.p7_valid);
    {
        struct jdl_live8 l8;
        CHECK(jdl_take_live8(&d, &l8) && l8.open == 1 && l8.drawn == 300 && !jdl_take_live8(&d, &l8));   /* once per status */
    }
    CHECK(input(&d, d.seq, JDL_CMD_LIVE | 0x80u, 0, s, JDL_P8_BYTES - 1u) == JDL_IN_REPLY && !d.p8_valid);   /* short: not read */
    s[0] = 7;
    CHECK(input(&d, d.seq, JDL_CMD_LIVE | 0x80u, 0, s, sizeof s) == JDL_IN_REPLY && !d.p8_valid);   /* protocol 7: none */
}

/* the Digitone alone (owner 2026-11-26: Jank's menu and LINK MODE without a Launchpad): LIVE ON asks for no note push (what 0, "off" in dn2-wishes' table); with a
 * Launchpad it asks for the push as before; jdl_init asks for it */
static void test_push_choice(void)
{
    struct jdl d;
    uint8_t out[64], body[32];
    size_t n;
    jdl_init(&d);
    n = jdl_poll(&d, 1000, out, sizeof out);
    CHECK(request_body(out, n, body) == 5u && body[2] == JDL_CMD_LIVE && body[3] == 1 && body[4] == 1);   /* with a Launchpad: the push */
    jdl_init(&d);
    jdl_want_push(&d, 0);
    n = jdl_poll(&d, 1000, out, sizeof out);
    CHECK(request_body(out, n, body) == 5u && body[2] == JDL_CMD_LIVE && body[3] == 1 && body[4] == 0);   /* the Digitone alone: none */
    n = jdl_poll(&d, 1000 + JDL_ASK_MS, out, sizeof out);                        /* asked again: the same */
    CHECK(request_body(out, n, body) == 5u && body[4] == 0);
    jdl_want_push(&d, 1);                                                         /* a Launchpad came */
    n = jdl_poll(&d, 1000 + 2u * JDL_ASK_MS, out, sizeof out);
    CHECK(request_body(out, n, body) == 5u && body[4] == 1);
    CHECK(input(&d, d.seq, JDL_CMD_LIVE | 0x80u, 0, NULL, 0) == JDL_IN_REPLY && jdl_is_live(&d));
    jdl_want_push(&d, 0);                                                         /* live: the keep-alive is a STATUS, not a LIVE ON */
    n = jdl_poll(&d, 1000 + 2u * JDL_ASK_MS + JDL_KEEPALIVE_MS, out, sizeof out);
    CHECK(request_body(out, n, body) == 4u && body[3] == 0);
    jdl_init(&d);
    CHECK(!d.no_push);
}

/* LINK MODE's request lengths, exactly as dn2-wishes reads them (confirmed from its code 2026-11-26): ops 0, 1, 3, 255 no argument; 2 u16 ms + u8 id; 4 u8 id */
static void test_link_mode_lengths(void)
{
    struct jdl d = live();
    uint8_t out[64], body[32];
    static const uint8_t bare[4] = {JDL_LINK_OFF, JDL_LINK_ON, JDL_LINK_CANCEL, JDL_LINK_READ};
    for (int i = 0; i < 4; i++) {
        size_t n = jdl_link_mode(&d, 2000, bare[i], 1234, 7, out, sizeof out);   /* (ms and id given: not sent for these) */
        CHECK(request_body(out, n, body) == 4u && body[2] == JDL_CMD_LINK_MODE && body[3] == bare[i]);
    }
    CHECK(request_body(out, jdl_link_mode(&d, 2000, JDL_LINK_PENDING, 0xFFFF, 255, out, sizeof out), body) == 7u && body[4] == 0xFF && body[5] == 0xFF && body[6] == 255);
    CHECK(request_body(out, jdl_link_mode(&d, 2000, JDL_LINK_ACK, 9, 0, out, sizeof out), body) == 5u && body[4] == 0);
    for (unsigned op = 5; op < 255; op++)                                         /* anything else: not sent (the Digitone answers 2) */
        CHECK(jdl_link_mode(&d, 2000, (uint8_t)op, 0, 0, out, sizeof out) == 0);
}

int main(void)
{
    test_push_choice();
    test_link_mode_lengths();
    test_page_bytes();
    test_longest_page();
    test_refused_bits();
    test_open_and_probe();
    test_keys();
    test_link_mode();
    test_status_p8();
    printf("test_dn2live9: %d checks, %d failed\n", checks, fails);
    return fails;
}
