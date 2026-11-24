/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the signed Wi-Fi update frames (core/jank_wota.c). The fixed vectors are the same as scripts/jankwifi.py's tests, made with
 * Python's hmac: the two implementations must agree byte for byte. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_wota.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static uint8_t KEY[JWO_KEY], NONCE[JWO_NONCE], OTHER_KEY[JWO_KEY];

static void setup(void)
{
    for (unsigned i = 0; i < JWO_KEY; i++) {
        KEY[i] = (uint8_t)i;                                 /* 00 01 .. 1f */
        OTHER_KEY[i] = (uint8_t)(i ^ 0x55);
    }
    for (unsigned i = 0; i < JWO_NONCE; i++)
        NONCE[i] = (uint8_t)(0xA0 + i);                      /* a0 .. a7 */
}

static int unhex(const char *h, uint8_t *out, size_t cap)
{
    size_t n = strlen(h) / 2;
    if (n > cap)
        return -1;
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        sscanf(h + 2 * i, "%2x", &v);
        out[i] = (uint8_t)v;
    }
    return (int)n;
}

/* a unit with the key and an open session on NONCE, at time t */
static struct jwo unit(uint32_t t)
{
    struct jwo s;
    struct jwo_req r;
    uint8_t out[16];
    size_t n;
    uint8_t hello[2] = {JWO_MAGIC, JWO_T_HELLO};
    jwo_init(&s);
    jwo_set_key(&s, KEY);
    CHECK(jwo_input(&s, hello, 2, t, NONCE, &r, out, sizeof out, &n) == JWO_HELLO);
    return s;
}

static size_t build(uint32_t ctr, uint8_t op, uint8_t *out, size_t cap)
{
    struct jwo_req r;
    memset(&r, 0, sizeof r);
    r.counter = ctr;
    r.op = op;
    return jwo_build(KEY, NONCE, &r, out, cap);
}

static void test_vectors_match_python(void)
{
    uint8_t want[JWO_FRAME_MAX], got[JWO_FRAME_MAX];
    struct jwo_req r;
    int w;
    memset(&r, 0, sizeof r);
    r.counter = 1;
    r.op = JWO_OP_BEGIN;
    r.size = 1514240;
    r.crc = 0x12345678u;
    r.target = 0;
    w = unhex("0102000000010100171b00123456780080dc27e23e1ddf3a060bebbf2fcab849", want, sizeof want);
    CHECK(jwo_build(KEY, NONCE, &r, got, sizeof got) == (size_t)w && !memcmp(got, want, (size_t)w));
    memset(&r, 0, sizeof r);
    r.counter = 2;
    r.op = JWO_OP_DATA;
    r.offset = 0;
    r.crc = 0xCAFEBABEu;
    r.n = 5;
    r.data = (const uint8_t *)"hello";
    w = unhex("0102000000020200000000cafebabe000568656c6c6f0e3dece4f6e38e99d8462556b0c6ec34", want, sizeof want);
    CHECK(jwo_build(KEY, NONCE, &r, got, sizeof got) == (size_t)w && !memcmp(got, want, (size_t)w));
    memset(&r, 0, sizeof r);
    r.counter = 3;
    r.op = JWO_OP_CONFIRM;
    w = unhex("010200000003057b8c2c6da2b5ffc6809c9c65443a6d6f", want, sizeof want);
    CHECK(jwo_build(KEY, NONCE, &r, got, sizeof got) == (size_t)w && !memcmp(got, want, (size_t)w));
    {
        struct jwo s = unit(1000);
        w = unhex("018200000002000000000504001e8bdb57ade9afac3579c3921e99d360", want, sizeof want);
        CHECK(jwo_reply(&s, 2, 0, 5, 1024, got, sizeof got) == JWO_REPLY_LEN && !memcmp(got, want, (size_t)w));
    }
}

static void test_a_transfer_end_to_end(void)
{
    struct jwo s = unit(1000);
    struct jwo_req r, in;
    uint8_t f[JWO_FRAME_MAX], out[16], rep[JWO_REPLY_LEN];
    uint8_t chunk[JWO_DATA_MAX];
    size_t n, ol;
    uint32_t c, off;
    uint8_t st;
    uint16_t ch;
    memset(chunk, 0xE9, sizeof chunk);
    memset(&r, 0, sizeof r);
    r.counter = 1;
    r.op = JWO_OP_BEGIN;
    r.size = 4096;
    r.crc = 0x01020304u;
    r.target = 1;
    n = jwo_build(KEY, NONCE, &r, f, sizeof f);
    CHECK(n == JWO_HEAD + 9 + JWO_MAC);
    CHECK(jwo_input(&s, f, n, 1100, NONCE, &in, out, sizeof out, &ol) == JWO_CMD);
    CHECK(in.counter == 1 && in.op == JWO_OP_BEGIN && in.size == 4096 && in.crc == 0x01020304u && in.target == 1);
    CHECK(jwo_reply(&s, in.counter, 0, 0, 1024, rep, sizeof rep) == JWO_REPLY_LEN);
    CHECK(jwo_check_reply(KEY, NONCE, rep, sizeof rep, &c, &st, &off, &ch) && c == 1 && st == 0 && off == 0 && ch == 1024);
    memset(&r, 0, sizeof r);
    r.counter = 2;
    r.op = JWO_OP_DATA;
    r.offset = 0;
    r.crc = 0xAABBCCDDu;
    r.n = JWO_DATA_MAX;
    r.data = chunk;
    n = jwo_build(KEY, NONCE, &r, f, sizeof f);
    CHECK(n == JWO_FRAME_MAX);                               /* the biggest frame */
    CHECK(jwo_input(&s, f, n, 1200, NONCE, &in, out, sizeof out, &ol) == JWO_CMD);
    CHECK(in.op == JWO_OP_DATA && in.offset == 0 && in.crc == 0xAABBCCDDu && in.n == JWO_DATA_MAX && !memcmp(in.data, chunk, JWO_DATA_MAX));
    CHECK(in.data >= f && in.data + in.n <= f + n);          /* a pointer into the frame, not a copy */
    n = build(3, JWO_OP_END, f, sizeof f);
    CHECK(n == JWO_HEAD + JWO_MAC && jwo_input(&s, f, n, 1300, NONCE, &in, out, sizeof out, &ol) == JWO_CMD && in.op == JWO_OP_END);
    n = build(4, JWO_OP_ABORT, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 1400, NONCE, &in, out, sizeof out, &ol) == JWO_CMD && in.op == JWO_OP_ABORT);
    n = build(5, JWO_OP_CONFIRM, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 1500, NONCE, &in, out, sizeof out, &ol) == JWO_CMD && in.op == JWO_OP_CONFIRM);
}

static void test_a_signed_request_of_an_unknown_op_is_answerable(void)
{
    struct jwo s = unit(0);
    struct jwo_req in;
    uint8_t f[JWO_FRAME_MAX], out[16];
    size_t n = build(1, 99, f, sizeof f), ol;
    CHECK(n == JWO_HEAD + JWO_MAC);
    CHECK(jwo_input(&s, f, n, 10, NONCE, &in, out, sizeof out, &ol) == JWO_CMD && in.op == 99);   /* the caller answers JWO_S_BAD_OP */
}

static void test_no_key_no_answer(void)
{
    struct jwo s;
    struct jwo_req in;
    uint8_t hello[2] = {JWO_MAGIC, JWO_T_HELLO}, out[16], f[JWO_FRAME_MAX];
    size_t ol, n;
    jwo_init(&s);
    CHECK(jwo_input(&s, hello, 2, 0, NONCE, &in, out, sizeof out, &ol) == JWO_NONE && ol == 0);   /* a unit with no key does not even say hello */
    n = build(1, JWO_OP_END, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 0, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);
    jwo_set_key(&s, KEY);
    CHECK(jwo_input(&s, hello, 2, 0, NONCE, &in, out, sizeof out, &ol) == JWO_HELLO && ol == JWO_HELLO_R_LEN);
    CHECK(out[0] == JWO_MAGIC && out[1] == JWO_T_HELLO_R && !memcmp(out + 2, NONCE, JWO_NONCE));
    jwo_clear_key(&s);                                       /* the key taken away ends the session */
    n = build(1, JWO_OP_END, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 1, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);
}

static void test_a_request_outside_a_session_is_dropped(void)
{
    struct jwo s;
    struct jwo_req in;
    uint8_t f[JWO_FRAME_MAX], out[16];
    size_t n, ol;
    jwo_init(&s);
    jwo_set_key(&s, KEY);                                    /* a key, but no HELLO yet */
    n = build(1, JWO_OP_END, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 0, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);
}

static void test_every_altered_byte_fails(void)
{
    struct jwo s;
    struct jwo_req r, in;
    uint8_t f[JWO_FRAME_MAX], out[16];
    uint8_t chunk[16];
    size_t n, ol;
    memset(chunk, 7, sizeof chunk);
    memset(&r, 0, sizeof r);
    r.counter = 1;
    r.op = JWO_OP_DATA;
    r.crc = 1;
    r.n = sizeof chunk;
    r.data = chunk;
    n = jwo_build(KEY, NONCE, &r, f, sizeof f);
    for (size_t i = 0; i < n; i++) {
        uint8_t keep = f[i];
        s = unit(0);
        f[i] ^= 0x01;
        CHECK(jwo_input(&s, f, n, 10, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);   /* a flipped bit anywhere: dropped */
        f[i] = keep;
    }
    s = unit(0);
    CHECK(jwo_input(&s, f, n, 10, NONCE, &in, out, sizeof out, &ol) == JWO_CMD);        /* and the untouched one goes through */
}

static void test_a_cut_or_padded_frame_is_dropped(void)
{
    struct jwo s = unit(0);
    struct jwo_req in;
    uint8_t f[JWO_FRAME_MAX + 8], out[16];
    size_t n = build(1, JWO_OP_END, f, sizeof f), ol;
    CHECK(jwo_input(&s, f, n - 1, 10, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);
    f[n] = 0;
    CHECK(jwo_input(&s, f, n + 1, 10, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);
    CHECK(jwo_input(&s, f, 0, 10, NONCE, &in, out, sizeof out, &ol) == JWO_NONE && jwo_input(&s, f, 1, 10, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);
    CHECK(jwo_input(&s, f, JWO_FRAME_MAX + 1u, 10, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);   /* over the biggest frame */
    f[0] = 0x02;                                             /* not our magic */
    CHECK(jwo_input(&s, f, n, 10, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);
}

static void test_the_wrong_key_or_nonce_fails(void)
{
    struct jwo s = unit(0);
    struct jwo_req r, in;
    uint8_t f[JWO_FRAME_MAX], out[16], other_nonce[JWO_NONCE];
    size_t n, ol;
    memset(&r, 0, sizeof r);
    r.counter = 1;
    r.op = JWO_OP_END;
    n = jwo_build(OTHER_KEY, NONCE, &r, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 10, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);     /* signed with another key */
    memcpy(other_nonce, NONCE, sizeof other_nonce);
    other_nonce[0] ^= 1;
    n = jwo_build(KEY, other_nonce, &r, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 10, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);     /* signed for another session */
}

static void test_replay_and_counters(void)
{
    struct jwo s = unit(0);
    struct jwo_req in;
    uint8_t f2[JWO_FRAME_MAX], f3[JWO_FRAME_MAX], f1[JWO_FRAME_MAX], out[16];
    size_t n1 = build(1, JWO_OP_END, f1, sizeof f1), n2 = build(2, JWO_OP_END, f2, sizeof f2), n3 = build(3, JWO_OP_END, f3, sizeof f3), ol;
    CHECK(jwo_input(&s, f2, n2, 10, NONCE, &in, out, sizeof out, &ol) == JWO_CMD);    /* a gap is fine: 2 after nothing */
    CHECK(jwo_input(&s, f2, n2, 20, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);   /* the same frame again: a replay */
    CHECK(jwo_input(&s, f1, n1, 30, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);   /* an older counter */
    CHECK(jwo_input(&s, f3, n3, 40, NONCE, &in, out, sizeof out, &ol) == JWO_CMD);
    CHECK(jwo_input(&s, f3, n3, 50, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);
}

static void test_a_session_ends_after_its_time(void)
{
    struct jwo s = unit(1000);
    struct jwo_req in;
    uint8_t f[JWO_FRAME_MAX], out[16];
    size_t n = build(1, JWO_OP_END, f, sizeof f), ol;
    CHECK(jwo_input(&s, f, n, 1000 + JWO_SESSION_MS, NONCE, &in, out, sizeof out, &ol) == JWO_CMD);        /* just inside it */
    n = build(2, JWO_OP_END, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 1000 + JWO_SESSION_MS + JWO_SESSION_MS + 1u, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);   /* too long after the last */
    n = build(3, JWO_OP_END, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 1000 + JWO_SESSION_MS + JWO_SESSION_MS + 2u, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);   /* the session is gone */
}

static void test_the_time_counter_wrapping(void)
{
    struct jwo s = unit(0xFFFFFF00u);                        /* the millisecond clock wraps after 49 days: a session spans it */
    struct jwo_req in;
    uint8_t f[JWO_FRAME_MAX], out[16];
    size_t n = build(1, JWO_OP_END, f, sizeof f), ol;
    CHECK(jwo_input(&s, f, n, 0x00000100u, NONCE, &in, out, sizeof out, &ol) == JWO_CMD);
}

static void test_hello_is_locked_while_a_session_is_busy(void)
{
    struct jwo s = unit(1000);
    struct jwo_req in;
    uint8_t hello[2] = {JWO_MAGIC, JWO_T_HELLO}, out[16], f[JWO_FRAME_MAX], fresh[JWO_NONCE];
    size_t n = build(1, JWO_OP_END, f, sizeof f), ol;
    memset(fresh, 0x77, sizeof fresh);
    CHECK(jwo_input(&s, f, n, 2000, NONCE, &in, out, sizeof out, &ol) == JWO_CMD);
    CHECK(jwo_input(&s, hello, 2, 2000 + JWO_HELLO_LOCK_MS - 1u, fresh, &in, out, sizeof out, &ol) == JWO_NONE && ol == 0);   /* busy: ignored */
    CHECK(!memcmp(s.nonce, NONCE, JWO_NONCE));               /* the session is as it was */
    n = build(2, JWO_OP_END, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 2000 + JWO_HELLO_LOCK_MS, NONCE, &in, out, sizeof out, &ol) == JWO_CMD);   /* and it still works */
    CHECK(jwo_input(&s, hello, 2, 2000 + 2u * JWO_HELLO_LOCK_MS, fresh, &in, out, sizeof out, &ol) == JWO_HELLO);   /* idle long enough: a new session */
    CHECK(!memcmp(s.nonce, fresh, JWO_NONCE) && s.last_ctr == 0);
    n = build(3, JWO_OP_END, f, sizeof f);
    CHECK(jwo_input(&s, f, n, 2000 + 2u * JWO_HELLO_LOCK_MS + 1u, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);   /* the old nonce means nothing now */
}

static void test_hello_shape(void)
{
    struct jwo s;
    struct jwo_req in;
    uint8_t out[16], long_hello[3] = {JWO_MAGIC, JWO_T_HELLO, 0};
    size_t ol;
    jwo_init(&s);
    jwo_set_key(&s, KEY);
    CHECK(jwo_input(&s, long_hello, 3, 0, NONCE, &in, out, sizeof out, &ol) == JWO_NONE);   /* a HELLO is exactly two bytes */
    CHECK(jwo_input(&s, long_hello, 2, 0, NONCE, &in, out, 4, &ol) == JWO_NONE);            /* no room for the answer: none given */
}

static void test_reply_checks(void)
{
    struct jwo s = unit(0);
    uint8_t rep[JWO_REPLY_LEN];
    uint32_t c, off;
    uint8_t st;
    uint16_t ch;
    CHECK(jwo_reply(&s, 9, JWO_S_TOO_EARLY, 12345, 0, rep, sizeof rep) == JWO_REPLY_LEN);
    CHECK(jwo_check_reply(KEY, NONCE, rep, sizeof rep, &c, &st, &off, &ch) && c == 9 && st == JWO_S_TOO_EARLY && off == 12345 && ch == 0);
    CHECK(!jwo_check_reply(OTHER_KEY, NONCE, rep, sizeof rep, &c, &st, &off, &ch));        /* another key's reply is not believed */
    for (size_t i = 0; i < sizeof rep; i++) {
        rep[i] ^= 0x40;
        CHECK(!jwo_check_reply(KEY, NONCE, rep, sizeof rep, &c, &st, &off, &ch));          /* nor an altered one */
        rep[i] ^= 0x40;
    }
    CHECK(!jwo_check_reply(KEY, NONCE, rep, sizeof rep - 1u, &c, &st, &off, &ch));
    CHECK(jwo_reply(&s, 1, 0, 0, 0, rep, sizeof rep - 1u) == 0);                           /* no room */
    jwo_clear_key(&s);
    CHECK(jwo_reply(&s, 1, 0, 0, 0, rep, sizeof rep) == 0);                                /* no key, no reply */
}

static void test_build_limits(void)
{
    struct jwo_req r;
    uint8_t f[JWO_FRAME_MAX], big[JWO_DATA_MAX + 1u];
    memset(&r, 0, sizeof r);
    memset(big, 0, sizeof big);
    r.counter = 1;
    r.op = JWO_OP_DATA;
    r.n = JWO_DATA_MAX + 1u;
    r.data = big;
    CHECK(jwo_build(KEY, NONCE, &r, f, sizeof f) == 0);                                    /* over the biggest chunk */
    r.n = 16;
    CHECK(jwo_build(KEY, NONCE, &r, f, JWO_HEAD + 10u + 16u + JWO_MAC - 1u) == 0);         /* no room */
    r.data = NULL;
    CHECK(jwo_build(KEY, NONCE, &r, f, sizeof f) == 0);                                    /* bytes promised, none given */
}

int main(void)
{
    setup();
    test_vectors_match_python();
    test_a_transfer_end_to_end();
    test_a_signed_request_of_an_unknown_op_is_answerable();
    test_no_key_no_answer();
    test_a_request_outside_a_session_is_dropped();
    test_every_altered_byte_fails();
    test_a_cut_or_padded_frame_is_dropped();
    test_the_wrong_key_or_nonce_fails();
    test_replay_and_counters();
    test_a_session_ends_after_its_time();
    test_the_time_counter_wrapping();
    test_hello_is_locked_while_a_session_is_busy();
    test_hello_shape();
    test_reply_checks();
    test_build_limits();
    printf("test_wota: %d checks, %d failed\n", checks, fails);
    return fails;
}
