/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the dn2-wishes test channel's framing (core/jank_dn2rpc.c). The golden bytes were made by the channel's own
 * Python (dn2-wishes tools/dn2tools, protocol.py encode / reply / pack7). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_dn2rpc.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static size_t unhex(const char *h, uint8_t *out)
{
    size_t n = 0;
    for (; h[0] && h[1]; h += 2, n++) {
        unsigned v;
        sscanf(h, "%2x", &v);
        out[n] = (uint8_t)v;
    }
    return n;
}

/* P.encode(0x0102, 35, struct.pack('>IB', 0x42432a61, 128)) */
static const char REQ[] = "f000203c7d010001022342432a614000f7";
/* P.reply(0x0102, 35, 0, data) with data = DATA */
static const char REP[] = "f000203c7d01100102230042432a0661000102007f071008092a0bf7";
static const char DATA[] = "42432a6100010280ff070809aa0b";

static void test_pack7_golden_and_round_trip(void)
{
    const uint8_t in[8] = {0x80, 0x01, 0xFF, 0x03, 0x04, 0x05, 0x06, 0x81};   /* P.pack7 -> 5000017f030405064001 */
    uint8_t want[16], out[32], back[64];
    size_t n = unhex("5000017f030405064001", want);
    CHECK(jd2_pack7(in, 8, out) == n && !memcmp(out, want, n));
    for (size_t len = 0; len < 60; len++) {
        uint8_t src[60];
        for (size_t k = 0; k < len; k++)
            src[k] = (uint8_t)(k * 37u + len);
        uint8_t packed[80];
        size_t pn = jd2_pack7(src, len, packed);
        CHECK(pn == len + (len + 6) / 7);
        for (size_t k = 0; k < pn; k++)
            CHECK(packed[k] < 0x80);                                  /* 7-bit clean */
        CHECK(jd2_unpack7(packed, pn, back) == len && !memcmp(back, src, len));
    }
}

static void test_peek_request_golden(void)
{
    uint8_t want[32], out[32];
    size_t n = unhex(REQ, want);
    CHECK(jd2_peek_request(0x0102, 0x42432a61u, 128, out, sizeof out) == n && !memcmp(out, want, n));
    CHECK(jd2_peek_request(1, 0, 0, out, sizeof out) == 0);              /* length 0 and over 128 are refused */
    CHECK(jd2_peek_request(1, 0, 129, out, sizeof out) == 0);
    CHECK(jd2_peek_request(1, 0, 4, out, 10) == 0);                      /* does not fit */
    CHECK(jd2_peek_request(1, 0, 4, out, sizeof out) != 0);
}

static void test_parse_reply_golden(void)
{
    uint8_t sx[JD2_SX_MAX], scratch[JD2_SX_MAX], data[32];
    struct jd2_reply r;
    uint32_t addr;
    const uint8_t *b;
    size_t n = unhex(REP, sx), dn = unhex(DATA, data), bn;
    CHECK(jd2_parse_reply(sx, n, scratch, sizeof scratch, &r));
    CHECK(r.seq == 0x0102 && r.cmd == JD2_CMD_PEEK && r.status == JD2_STATUS_OK && r.n == dn && !memcmp(r.data, data, dn));
    CHECK(jd2_peek_data(&r, &addr, &b, &bn) && addr == 0x42432a61u && bn == dn - 4 && !memcmp(b, data + 4, bn));
}

static void test_parse_reply_refusals(void)
{
    uint8_t sx[JD2_SX_MAX], scratch[JD2_SX_MAX];
    struct jd2_reply r;
    size_t n = unhex(REP, sx);
    CHECK(!jd2_parse_reply(sx, n - 1, scratch, sizeof scratch, &r));      /* no F7 */
    sx[3] ^= 1;
    CHECK(!jd2_parse_reply(sx, n, scratch, sizeof scratch, &r));          /* not our header */
    sx[3] ^= 1;
    CHECK(!jd2_parse_reply(sx, 8, scratch, sizeof scratch, &r));          /* too short for seq, cmd, status */
    CHECK(!jd2_parse_reply(sx, n, scratch, 4, &r));                       /* scratch too small */
    {
        const uint8_t status7[] = {0xF0, 0x00, 0x20, 0x3C, 0x7D, 0x01, 0x10, 0x00, 0x01, 0x23, 0x07, 0xF7};   /* seq 0, cmd 35|0x80, status 7 */
        CHECK(jd2_parse_reply(status7, sizeof status7, scratch, sizeof scratch, &r) && r.status == JD2_STATUS_REMOTE_OFF && r.n == 0);
        CHECK(!jd2_peek_data(&r, &(uint32_t){0}, &(const uint8_t *){0}, &(size_t){0}));   /* no data, no address */
    }
}

static void test_sysex_packets_golden(void)
{
    uint8_t sx[JD2_SX_MAX], out[64];
    const uint8_t want[24] = {0x04, 0xF0, 0x00, 0x20, 0x04, 0x3C, 0x7D, 0x01, 0x04, 0x00, 0x01, 0x02,
                              0x04, 0x23, 0x42, 0x43, 0x04, 0x2A, 0x61, 0x40, 0x06, 0x00, 0xF7, 0x00};
    size_t n = unhex(REQ, sx);
    CHECK(jd2_sysex_to_packets(0, sx, n, out, sizeof out) == 24 && !memcmp(out, want, 24));
    CHECK(jd2_sysex_to_packets(3, sx, n, out, sizeof out) == 24 && out[0] == 0x34 && out[20] == 0x36);   /* the cable in the high nibble */
    CHECK(jd2_sysex_to_packets(0, sx, n, out, 20) == 0);                    /* does not fit */
    {
        const uint8_t three[3] = {0xF0, 0x7D, 0xF7};                         /* exactly three bytes: one packet, end with 3 */
        const uint8_t one_end[4] = {0xF0, 0x01, 0x02, 0xF7};                 /* 4 bytes: 3 + an end of 1 */
        CHECK(jd2_sysex_to_packets(0, three, 3, out, sizeof out) == 4 && out[0] == 0x07 && out[1] == 0xF0 && out[3] == 0xF7);
        CHECK(jd2_sysex_to_packets(0, one_end, 4, out, sizeof out) == 8 && out[0] == 0x04 && out[4] == 0x05 && out[5] == 0xF7);
    }
}

static int feed_all(struct jd2_sx *s, const uint8_t *packets, size_t bytes)
{
    int done = 0;
    for (size_t i = 0; i + 4 <= bytes; i += 4)
        done = jd2_sx_feed(s, packets + i);
    return done;
}

static void test_reassemble_round_trip(void)
{
    struct jd2_sx s;
    uint8_t sx[JD2_SX_MAX], pk[4 * JD2_SX_MAX / 3 + 8];
    for (size_t len = 3; len < 100; len++) {                                 /* every length: all four end codes */
        sx[0] = 0xF0;
        for (size_t k = 1; k + 1 < len; k++)
            sx[k] = (uint8_t)(k & 0x7F);
        sx[len - 1] = 0xF7;
        size_t pn = jd2_sysex_to_packets(1, sx, len, pk, sizeof pk);
        jd2_sx_init(&s);
        CHECK(feed_all(&s, pk, pn) == 1 && s.n == len && !memcmp(s.buf, sx, len));
    }
}

static void test_reassemble_real_reply(void)
{
    struct jd2_sx s;
    uint8_t sx[JD2_SX_MAX], pk[JD2_SX_MAX * 2], scratch[JD2_SX_MAX];
    struct jd2_reply r;
    size_t n = unhex(REP, sx), pn = jd2_sysex_to_packets(0, sx, n, pk, sizeof pk);
    jd2_sx_init(&s);
    CHECK(feed_all(&s, pk, pn) == 1);
    CHECK(jd2_parse_reply(s.buf, s.n, scratch, sizeof scratch, &r) && r.seq == 0x0102);
}

static void test_reassemble_noise_and_recovery(void)
{
    struct jd2_sx s;
    const uint8_t clock[4] = {0x0F, 0xF8, 0, 0}, note[4] = {0x09, 0x90, 60, 100}, cont[4] = {0x04, 1, 2, 3}, endp[4] = {0x05, 0xF7, 0, 0};
    const uint8_t start[4] = {0x04, 0xF0, 0x7D, 0x01}, fin[4] = {0x05, 0xF7, 0, 0};
    jd2_sx_init(&s);
    CHECK(!jd2_sx_feed(&s, clock) && !jd2_sx_feed(&s, note));               /* not SysEx: ignored */
    CHECK(!jd2_sx_feed(&s, cont) && !jd2_sx_feed(&s, endp));                /* a middle or an end with no start: ignored */
    CHECK(!jd2_sx_feed(&s, start) && !jd2_sx_feed(&s, clock));              /* realtime inside a SysEx: ignored, it goes on */
    CHECK(jd2_sx_feed(&s, fin) && s.n == 4 && s.buf[0] == 0xF0 && s.buf[3] == 0xF7);
    CHECK(!jd2_sx_feed(&s, start));                                         /* done: the next feed starts afresh */
    CHECK(!jd2_sx_feed(&s, start));                                         /* an F0 inside a SysEx restarts it */
    CHECK(jd2_sx_feed(&s, fin) && s.n == 4);
}

static void test_reassemble_overflow(void)
{
    struct jd2_sx s;
    const uint8_t start[4] = {0x04, 0xF0, 1, 2}, mid[4] = {0x04, 3, 4, 5}, fin[4] = {0x05, 0xF7, 0, 0};
    jd2_sx_init(&s);
    jd2_sx_feed(&s, start);
    for (size_t i = 0; i < JD2_SX_MAX; i++)                                 /* far more than fits: never completes, nothing overruns */
        CHECK(!jd2_sx_feed(&s, mid));
    CHECK(!jd2_sx_feed(&s, fin));                                           /* the end of a dropped SysEx is not a message */
    CHECK(!jd2_sx_feed(&s, start));                                         /* and a new one starts cleanly */
    CHECK(jd2_sx_feed(&s, fin) && s.n == 4);
}

int main(void)
{
    test_pack7_golden_and_round_trip();
    test_peek_request_golden();
    test_parse_reply_golden();
    test_parse_reply_refusals();
    test_sysex_packets_golden();
    test_reassemble_round_trip();
    test_reassemble_real_reply();
    test_reassemble_noise_and_recovery();
    test_reassemble_overflow();
    printf("test_dn2rpc: %d checks, %d failed\n", checks, fails);
    return fails;
}
