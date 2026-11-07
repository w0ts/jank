/* SPDX-License-Identifier: GPL-3.0-only */
/* jank's own address on a normal network: DHCP with a client ID of its own, and the IGMP join. Exit status: non-zero on a failure. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "jank_net.h"

static const uint8_t M[6] = {0x02, 0x6A, 0x6B, 0x01, 0x02, 0x03};
#define GROUP 0xE04C4E4Bu
#define SERVER 0xC0A80101u
#define GIVEN 0xC0A80164u
#define MASK 0xFFFFFF00u
#define DHCP_AT 42u                    /* Ethernet + IP + UDP */
#define COOKIE_AT (DHCP_AT + 236u)
#define OPT_AT (COOKIE_AT + 4u)

static uint8_t sent[8][JN_FRAME_MAX];
static size_t sent_len[8];
static unsigned nsent;

static void catch_out(void *ctx, const uint8_t *eth, size_t len)
{
    (void)ctx;
    assert(nsent < 8 && len <= JN_FRAME_MAX);
    memcpy(sent[nsent], eth, len);
    sent_len[nsent++] = len;
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static const uint8_t *option(const uint8_t *f, size_t len, uint8_t code)
{
    for (size_t at = OPT_AT; at + 1u < len && f[at] != 255u; at += 2u + f[at + 1u])
        if (f[at] == code)
            return f + at;
    return NULL;
}

static size_t reply(uint8_t *f, uint32_t xid, const uint8_t *mac, uint8_t type, uint32_t yiaddr, uint32_t lease)
{
    size_t at = OPT_AT;
    memset(f, 0, 400);
    memset(f, 0xFF, 6);
    f[12] = 0x08;
    f[14] = 0x45;
    f[23] = 17;
    f[35] = 67;
    f[37] = 68;
    f[DHCP_AT] = 2;
    wr32(f + DHCP_AT + 4, xid);
    wr32(f + DHCP_AT + 16, yiaddr);
    memcpy(f + DHCP_AT + 28, mac, 6);
    wr32(f + COOKIE_AT, 0x63825363u);
    f[at++] = 53, f[at++] = 1, f[at++] = type;
    f[at++] = 54, f[at++] = 4, wr32(f + at, SERVER), at += 4;
    f[at++] = 1, f[at++] = 4, wr32(f + at, MASK), at += 4;
    f[at++] = 51, f[at++] = 4, wr32(f + at, lease), at += 4;
    f[at++] = 255;
    return at;
}

static struct jank_net n;
static void start(void)
{
    nsent = 0;
    jn_init(&n, M, 1234, "front of house", GROUP, catch_out, NULL);
}

static uint32_t xid_of(unsigned i) { return rd32(sent[i] + DHCP_AT + 4); }

static void bind_it(uint32_t lease)
{
    uint8_t f[400];
    size_t len;
    jn_link(&n, 1, 0);
    jn_poll(&n, 0);
    len = reply(f, xid_of(0), M, 2, GIVEN, lease);
    assert(jn_input(&n, f, len, 10) == 1);
    len = reply(f, xid_of(1), M, 5, GIVEN, lease);
    assert(jn_input(&n, f, len, 20) == 1);
}

static void test_silent_until_the_link_is_up(void)
{
    start();
    jn_poll(&n, 100000);
    assert(nsent == 0 && jn_ip(&n) == 0);
}

static void test_discover(void)
{
    const uint8_t *id, *name;
    start();
    jn_link(&n, 1, 0);
    jn_poll(&n, 0);
    assert(nsent == 1);
    assert(!memcmp(sent[0], "\xFF\xFF\xFF\xFF\xFF\xFF", 6) && !memcmp(sent[0] + 6, M, 6));
    assert(rd32(sent[0] + 26) == 0 && rd32(sent[0] + 30) == 0xFFFFFFFFu);
    assert(sent[0][DHCP_AT] == 1 && sent[0][DHCP_AT + 10] == 0x80);      /* broadcast flag: the computer's lease is another's */
    assert(!memcmp(sent[0] + DHCP_AT + 28, M, 6));
    assert(option(sent[0], sent_len[0], 53)[2] == 1);
    id = option(sent[0], sent_len[0], 61);
    assert(id && id[2] == 0 && !memcmp(id + 3, "jank", 4));              /* not the computer's client ID */
    name = option(sent[0], sent_len[0], 12);
    assert(name && !memcmp(name + 2, "front-of-house", 14));             /* a host name has no spaces */
}

static void test_retries_back_off(void)
{
    start();
    jn_link(&n, 1, 0);
    jn_poll(&n, 0);
    jn_poll(&n, JN_RETRY_MS - 1u);
    assert(nsent == 1);
    jn_poll(&n, JN_RETRY_MS);
    assert(nsent == 2);
    jn_poll(&n, JN_RETRY_MS * 2u);
    assert(nsent == 2);
    jn_poll(&n, JN_RETRY_MS * 3u);
    assert(nsent == 3);
}

static void test_offer_to_bound_and_igmp(void)
{
    const uint8_t *req;
    start();
    bind_it(3600);
    assert(jn_ip(&n) == GIVEN && jn_mask(&n) == MASK);
    assert(nsent == 3);
    req = option(sent[1], sent_len[1], 53);
    assert(req[2] == 3);
    assert(rd32(option(sent[1], sent_len[1], 50) + 2) == GIVEN);
    assert(rd32(option(sent[1], sent_len[1], 54) + 2) == SERVER);
    /* the join: to the group's MAC, source our address, TTL 1, router alert, right checksums */
    assert(!memcmp(sent[2], "\x01\x00\x5E\x4C\x4E\x4B", 6));
    assert(sent[2][14] == 0x46 && sent[2][22] == 1 && sent[2][23] == 2);
    assert(rd32(sent[2] + 26) == GIVEN && rd32(sent[2] + 30) == GROUP);
    assert(sent[2][38] == 0x16 && rd32(sent[2] + 42) == GROUP);
    assert(jn_sum(sent[2] + 14, 24) == 0 && jn_sum(sent[2] + 38, 8) == 0);
}

static void test_the_join_repeats(void)
{
    start();
    bind_it(86400);
    nsent = 0;
    jn_poll(&n, 20u + JN_IGMP_MS - 1u);
    assert(nsent == 0);
    jn_poll(&n, 20u + JN_IGMP_MS);
    assert(nsent == 1 && sent[0][23] == 2);
}

static void test_someone_elses_reply_passes_through(void)
{
    static const uint8_t other[6] = {1, 2, 3, 4, 5, 6};
    uint8_t f[400];
    size_t len;
    start();
    jn_link(&n, 1, 0);
    jn_poll(&n, 0);
    len = reply(f, xid_of(0) + 1u, M, 2, GIVEN, 3600);                   /* the computer's, on the same MAC */
    assert(jn_input(&n, f, len, 5) == 0 && jn_ip(&n) == 0);
    len = reply(f, xid_of(0), other, 2, GIVEN, 3600);
    assert(jn_input(&n, f, len, 5) == 0);
    f[12] = 0x08, f[13] = 0x06;
    assert(jn_input(&n, f, 60, 5) == 0);
    assert(jn_input(&n, f, 10, 5) == 0);
}

static void test_renew_keeps_the_address(void)
{
    uint8_t f[400];
    size_t len;
    start();
    bind_it(100);
    nsent = 0;
    jn_poll(&n, 20u + 49999u);
    assert(nsent == 0);
    jn_poll(&n, 20u + 50000u);
    assert(nsent == 1 && option(sent[0], sent_len[0], 53)[2] == 3 && jn_ip(&n) == GIVEN);
    len = reply(f, xid_of(0), M, 5, GIVEN, 100);
    assert(jn_input(&n, f, len, 50100) == 1 && jn_ip(&n) == GIVEN);
    nsent = 0;
    jn_poll(&n, 50100u + 49999u);
    assert(nsent == 0);
}

static void test_a_lease_runs_out(void)
{
    start();
    bind_it(100);
    jn_poll(&n, 20u + 99999u);
    assert(jn_ip(&n) == GIVEN);
    jn_poll(&n, 20u + 100000u);
    assert(jn_ip(&n) == 0);
}

static void test_nak_starts_again(void)
{
    uint8_t f[400];
    size_t len;
    start();
    jn_link(&n, 1, 0);
    jn_poll(&n, 0);
    len = reply(f, xid_of(0), M, 2, GIVEN, 3600);
    jn_input(&n, f, len, 10);
    len = reply(f, xid_of(1), M, 6, 0, 0);
    assert(jn_input(&n, f, len, 20) == 1 && jn_ip(&n) == 0);
    nsent = 0;
    jn_poll(&n, 20u + JN_RETRY_MS);
    assert(nsent == 1 && option(sent[0], sent_len[0], 53)[2] == 1);
}

static void test_link_down_forgets(void)
{
    start();
    bind_it(3600);
    jn_link(&n, 0, 100);
    assert(jn_ip(&n) == 0);
    nsent = 0;
    jn_poll(&n, 200000);
    assert(nsent == 0);
    jn_link(&n, 1, 300000);
    jn_poll(&n, 300000);
    assert(nsent == 1);
}

static void test_a_lease_of_for_ever_fits(void)
{
    start();
    bind_it(0xFFFFFFFFu);
    jn_poll(&n, 604800019u);           /* capped at a week, which fits 32 bits of milliseconds */
    assert(jn_ip(&n) == GIVEN);
    jn_poll(&n, 604800020u);
    assert(jn_ip(&n) == 0);
}

static void test_bad_options_do_not_read_past_the_frame(void)
{
    uint8_t f[400];
    size_t len;
    start();
    jn_link(&n, 1, 0);
    jn_poll(&n, 0);
    len = reply(f, xid_of(0), M, 2, GIVEN, 3600);
    f[OPT_AT + 1u] = 250;
    assert(jn_input(&n, f, len, 5) == 1 && jn_ip(&n) == 0);
}

int main(void)
{
    test_silent_until_the_link_is_up();
    test_discover();
    test_retries_back_off();
    test_offer_to_bound_and_igmp();
    test_the_join_repeats();
    test_someone_elses_reply_passes_through();
    test_renew_keeps_the_address();
    test_a_lease_runs_out();
    test_nak_starts_again();
    test_link_down_forgets();
    test_a_lease_of_for_ever_fits();
    test_bad_options_do_not_read_past_the_frame();
    puts("net: ok");
    return 0;
}
