/* SPDX-License-Identifier: GPL-3.0-only */
/* The Link peer against a computer on the USB side: an ordinary Link node (link.c) behind a small test stack that does
 * what a computer does: ARP before unicast, no learning from multicast. Then session -> tick maths end to end.
 * Times and delays here are this model's, not measurements of anything. Exit status: the number of failed checks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_clock.h"
#include "jank_core.h"
#include "jank_peer.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const uint8_t M[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
static const uint8_t P[6] = {0x06, 0x11, 0x22, 0x33, 0x44, 0x55};
static const uint8_t UNIT[6] = {0x8C, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
#define COMPUTER_IP 0xA9FE0A0Bu       /* 169.254.10.11 */
#define UNIT_IP 0xA9FE1415u

/* frames the peer sent */
#define Q 64u
struct sent {
    uint32_t where;
    size_t len;
    uint8_t f[JP_FRAME_MAX];
};
static struct sent q[Q];
static uint32_t nq;
static void catch_out(void *ctx, uint32_t where, const uint8_t *eth, size_t len)
{
    (void)ctx;
    if (nq < Q) {
        q[nq].where = where;
        q[nq].len = len;
        memcpy(q[nq].f, eth, len);
        nq++;
    }
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static size_t arp(uint8_t *f, const uint8_t *dst, const uint8_t *src, uint16_t op, const uint8_t *sha, uint32_t spa, const uint8_t *tha, uint32_t tpa)
{
    memcpy(f, dst, 6);
    memcpy(f + 6, src, 6);
    f[12] = 8;
    f[13] = 6;
    memcpy(f + 14, "\x00\x01\x08\x00\x06\x04\x00", 7);
    f[21] = (uint8_t)op;
    memcpy(f + 22, sha, 6);
    wr32(f + 28, spa);
    memcpy(f + 32, tha, 6);
    wr32(f + 38, tpa);
    return 42;
}

static size_t udp(uint8_t *f, const uint8_t *dst, const uint8_t *src, uint32_t sip, uint32_t dip, uint16_t dport, const uint8_t *pl, size_t n)
{
    memcpy(f, dst, 6);
    memcpy(f + 6, src, 6);
    f[12] = 8;
    f[13] = 0;
    memset(f + 14, 0, 28);
    f[14] = 0x45;
    f[16] = (uint8_t)((28 + n) >> 8);
    f[17] = (uint8_t)(28 + n);
    f[22] = 1;
    f[23] = 17;
    wr32(f + 26, sip);
    wr32(f + 30, dip);
    f[34] = (uint8_t)(LK_MCAST_PORT >> 8);
    f[35] = (uint8_t)LK_MCAST_PORT;
    f[36] = (uint8_t)(dport >> 8);
    f[37] = (uint8_t)dport;
    f[39] = (uint8_t)(8 + n);
    f[38] = (uint8_t)((8 + n) >> 8);
    if (n)
        memcpy(f + 42, pl, n);
    return 42 + n;
}

static struct jank_peer jp;
static uint8_t fr[2048];
static const uint8_t BC[6] = {255, 255, 255, 255, 255, 255};
static const uint8_t ZERO[6] = {0};
static const uint8_t LINK_MAC[6] = {0x01, 0x00, 0x5E, 0x4C, 0x4E, 0x4B};

static void setup(void)
{
    jp_init(&jp, P, M, 0x1234, catch_out, NULL);
    jp_allow(&jp, 1, 0);
    nq = 0;
}

static void test_address(void)
{
    setup();
    CHECK((jp.ip >> 16) == 0xA9FEu);
    CHECK((jp.ip >> 8 & 255u) >= 1 && (jp.ip >> 8 & 255u) <= 254 && (jp.ip & 255u) >= 1 && (jp.ip & 255u) <= 254);
}

static void test_arp_usb(void)
{
    size_t l;
    setup();
    l = arp(fr, BC, M, 1, M, COMPUTER_IP, ZERO, jp.ip);
    CHECK(jp_input(&jp, fr, l, JP_TO_USB, 0) == 0);      /* answered here, no air time */
    CHECK(nq == 1 && q[0].where == JP_TO_USB);
    CHECK(!memcmp(q[0].f, M, 6) && !memcmp(q[0].f + 6, P, 6));
    CHECK(q[0].f[21] == 2 && !memcmp(q[0].f + 22, P, 6) && rd32(q[0].f + 28) == jp.ip);
    CHECK(!memcmp(q[0].f + 32, M, 6) && rd32(q[0].f + 38) == COMPUTER_IP);
}

static void test_arp_air(void)
{
    size_t l;
    setup();
    l = arp(fr, BC, UNIT, 1, UNIT, UNIT_IP, ZERO, jp.ip);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 0) == 0);
    CHECK(nq == 1 && q[0].where == JP_TO_AIR);
    CHECK(!memcmp(q[0].f + 6, M, 6) && !memcmp(q[0].f + 22, M, 6));      /* the air's address, not P */
}

static void test_arp_other(void)
{
    size_t l;
    setup();
    l = arp(fr, BC, M, 1, M, COMPUTER_IP, ZERO, UNIT_IP);
    CHECK(jp_input(&jp, fr, l, JP_TO_USB, 0) == 1 && nq == 0);           /* someone else's: goes on */
    /* a computer's probe for the peer's address: defended */
    l = arp(fr, BC, M, 1, M, 0, ZERO, jp.ip);
    CHECK(jp_input(&jp, fr, l, JP_TO_USB, 0) == 0 && nq == 1);
    /* short */
    nq = 0;
    CHECK(jp_input(&jp, fr, 20, JP_TO_USB, 0) == 1 && nq == 0);
}

static void test_conflict(void)
{
    size_t l;
    uint32_t was;
    setup();
    was = jp.ip;
    l = arp(fr, BC, UNIT, 2, UNIT, was, ZERO, 0);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 0) == 1);
    CHECK(jp.ip != was && jp.ip_conflicts == 1 && (jp.ip >> 16) == 0xA9FEu);
    was = jp.ip;
    l = udp(fr, BC, UNIT, was, 0xFFFFFFFFu, 9999, NULL, 0);
    jp_input(&jp, fr, l, JP_TO_AIR, 0);
    CHECK(jp.ip != was && jp.ip_conflicts == 2);
}

static void test_on_a_network(void)
{
    enum { MINE = 0xC0A80164, OTHER = 0xC0A80165, FAR = 0x0A000001, MASK24 = 0xFFFFFF00 };
    size_t l;
    setup();
    jp_set_network(&jp, 0, 0, 0);
    l = arp(fr, BC, UNIT, 1, UNIT, OTHER, ZERO, MINE);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 0) == 1 && nq == 0);            /* no address: silent, and everything passes */
    jp_set_network(&jp, MINE, MASK24, 0);
    CHECK(jp.ip == (uint32_t)MINE);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 0) == 0);                       /* answers for the DHCP address */
    CHECK(nq == 1 && !memcmp(q[0].f + 22, M, 6) && q[0].f[28] == 0xC0);
    CHECK(jp.host[0].where == JP_TO_AIR && jp.host[0].ip == (uint32_t)OTHER);   /* learnt, off link-local */
    l = arp(fr, BC, UNIT, 2, UNIT, FAR, ZERO, 0);
    jp_input(&jp, fr, l, JP_TO_AIR, 0);
    CHECK(jp.host[1].where == 0);                                         /* not on the subnet: no route to learn */
    l = arp(fr, BC, UNIT, 2, UNIT, MINE, ZERO, 0);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 0) == 1);
    CHECK(jp.ip == (uint32_t)MINE && jp.ip_conflicts == 1);               /* the network's address: counted, never redrawn */
    jp_set_network(&jp, 0, 0, 0);
    CHECK(jp.host[0].where == 0 && !jp.on);
}

static void test_silent_until_link_heard(void)
{
    static const uint8_t junk[8] = {'_', 'a', 's', 'd', 'p', '_', 'v', 1};
    size_t l;
    setup();
    for (int64_t t = 0; t < 3000000; t += 1000)
        jp_poll(&jp, t);
    CHECK(nq == 0 && !jp.on);                            /* alone: not a frame */
    /* other multicast, and Link's port to another group: not Link */
    l = udp(fr, LINK_MAC, UNIT, UNIT_IP, 0xE0000001u, LK_MCAST_PORT, junk, 8);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 0) == 1 && !jp.on);
    l = udp(fr, LINK_MAC, UNIT, UNIT_IP, LK_MCAST_IP, 9, junk, 8);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 0) == 1 && !jp.on);
    /* Link's multicast: on, and it still goes on to the computer */
    l = udp(fr, LINK_MAC, UNIT, UNIT_IP, LK_MCAST_IP, LK_MCAST_PORT, junk, 8);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 1000) == 1 && jp.on);
    /* the setting off: nothing */
    jp_allow(&jp, 0, 2000);
    CHECK(!jp.on);
    nq = 0;
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 3000) == 1 && !jp.on);
    l = arp(fr, BC, M, 1, M, COMPUTER_IP, ZERO, jp.ip);
    CHECK(jp_input(&jp, fr, l, JP_TO_USB, 3000) == 1 && nq == 0);
}

static void test_alone_leaves(void)
{
    static const uint8_t junk[8] = {'_', 'a', 's', 'd', 'p', '_', 'v', 1};
    size_t l;
    setup();
    l = udp(fr, LINK_MAC, UNIT, UNIT_IP, LK_MCAST_IP, LK_MCAST_PORT, junk, 8);
    jp_input(&jp, fr, l, JP_TO_AIR, 0);
    for (int64_t t = 0; t < (int64_t)JP_ALONE_MS * 1000 - 1000; t += 1000)
        jp_poll(&jp, t);
    CHECK(jp.on);
    jp_poll(&jp, (int64_t)JP_ALONE_MS * 1000 + 1000);
    CHECK(!jp.on);
}

static void test_for_the_peer_alone(void)
{
    size_t l;
    setup();
    /* a computer's frame to P never goes to the air */
    l = udp(fr, P, M, COMPUTER_IP, jp.ip, LK_MCAST_PORT, NULL, 0);
    CHECK(jp_input(&jp, fr, l, JP_TO_USB, 0) == 0);
    l = udp(fr, P, M, COMPUTER_IP, jp.ip, 1234, NULL, 0);
    CHECK(jp_input(&jp, fr, l, JP_TO_USB, 0) == 0);
    /* a unit's unicast to the peer's address, on M: not for the computer */
    l = udp(fr, M, UNIT, UNIT_IP, jp.ip, LK_MCAST_PORT, NULL, 0);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 0) == 0);
    /* a unit's unicast to the computer: is */
    l = udp(fr, M, UNIT, UNIT_IP, COMPUTER_IP, LK_MCAST_PORT, NULL, 0);
    CHECK(jp_input(&jp, fr, l, JP_TO_AIR, 0) == 1);
    /* cut short: no read past it */
    CHECK(jp_input(&jp, fr, 30, JP_TO_AIR, 0) == 1);
    fr[14] = 0x4F;                                       /* an IHL that points past the end */
    CHECK(jp_input(&jp, fr, 50, JP_TO_AIR, 0) == 1);
}

/* the computer: an ordinary Link node behind ARP */
#define DELAY_US 1500
struct wire {
    int64_t at;
    size_t len;
    uint8_t f[JP_FRAME_MAX];
};
#define WIRE 256u
static struct wire to_pc[WIRE], to_stick[WIRE];
static uint32_t n_pc, n_stick;
static struct lk_node pc;
static uint32_t pc_arp_ip;
static uint8_t pc_arp_mac[6];
static int pc_arp_ok;
static int64_t sim_now;

static void push(struct wire *w, uint32_t *n, const uint8_t *f, size_t len)
{
    if (*n < WIRE) {
        w[*n].at = sim_now + DELAY_US;
        w[*n].len = len;
        memcpy(w[*n].f, f, len);
        (*n)++;
    }
}

static void stick_out(void *ctx, uint32_t where, const uint8_t *eth, size_t len)
{
    (void)ctx;
    if (where == JP_TO_USB)
        push(to_pc, &n_pc, eth, len);
}

static int pc_send(void *ctx, uint32_t ip, uint16_t port, const uint8_t *p, uint32_t n)
{
    uint8_t f[JP_FRAME_MAX];
    (void)ctx;
    if (ip == LK_MCAST_IP) {
        push(to_stick, &n_stick, f, udp(f, LINK_MAC, M, COMPUTER_IP, ip, port, p, n));
        return 0;
    }
    if (!pc_arp_ok || pc_arp_ip != ip) {            /* who-has first, the datagram is lost, as it can be */
        pc_arp_ip = ip;
        push(to_stick, &n_stick, f, arp(f, BC, M, 1, M, COMPUTER_IP, ZERO, ip));
        return 0;
    }
    push(to_stick, &n_stick, f, udp(f, pc_arp_mac, M, COMPUTER_IP, ip, port, p, n));
    return 0;
}

static void pc_input(const uint8_t *f, size_t len)
{
    if (f[12] == 8 && f[13] == 6) {
        CHECK(f[21] == 2);
        memcpy(pc_arp_mac, f + 22, 6);
        pc_arp_ok = 1;
        return;
    }
    CHECK(rd32(f + 26) == jp.ip);
    lk_rx(&pc, sim_now, rd32(f + 26), (uint16_t)(f[34] << 8 | f[35]), f + 42, (uint32_t)(len - 42));
}

static void drain(struct wire *w, uint32_t *n, int to_stick_side)
{
    uint32_t i = 0;
    while (i < *n && w[i].at <= sim_now) {
        if (to_stick_side)
            jp_input(&jp, w[i].f, w[i].len, JP_TO_USB, sim_now);
        else
            pc_input(w[i].f, w[i].len);
        i++;
    }
    if (i) {
        *n -= i;
        memmove(w, w + i, *n * sizeof *w);
    }
}

static void run_for(int64_t us)
{
    int64_t end = sim_now + us;
    while (sim_now < end) {
        drain(to_stick, &n_stick, 1);
        drain(to_pc, &n_pc, 0);
        lk_poll(&pc, sim_now);
        jp_poll(&jp, sim_now);
        sim_now += 500;
    }
}

static void pc_start(int64_t age_us, int32_t mpb)
{
    struct lk_cfg c = {COMPUTER_IP, LK_MCAST_PORT, pc_send, NULL};
    n_pc = n_stick = 0;
    pc_arp_ok = 0;
    sim_now = 1000000;
    lk_init(&pc, &c, 77, mpb);
    lk_enable(&pc, 1, sim_now);
    lk_set_addr(&pc, COMPUTER_IP, LK_MCAST_PORT);
    jp_init(&jp, P, M, 0x1234, stick_out, NULL);
    jp_allow(&jp, 1, sim_now);
    run_for(age_us);
}

static int64_t abs64(int64_t v) { return v < 0 ? -v : v; }

static void test_joins_session(void)
{
    pc_start(100000, 433333);              /* 138.46 BPM, the computer's session is the older */
    run_for(6000000);
    CHECK(jp.on);
    CHECK(jp_joined(&jp));
    CHECK(lk_peers(&pc) == 1);
    CHECK(jp_mpb(&jp) == 433333);          /* jank took the tempo, it did not impose 120 */
    CHECK(lk_mpb(&pc) == 433333);
    int64_t a = lk_beat_at(&pc, sim_now), b = jp_beat_at(&jp, sim_now);
    CHECK(abs64(a - b) < 4000);            /* under 2 ms of beat at this tempo */
    printf("peer: session phase error %lld micro-beats\n", (long long)abs64(a - b));
}

static void test_follows(void)
{
    pc_start(100000, 500000);
    run_for(6000000);
    lk_set_tempo(&pc, 400000, sim_now);
    run_for(1000000);
    CHECK(jp_mpb(&jp) == 400000);
    lk_set_playing(&pc, 1, lk_beat_at(&pc, sim_now), sim_now);
    run_for(1000000);
    CHECK(jp_playing(&jp) == 1);
    lk_set_playing(&pc, 0, lk_beat_at(&pc, sim_now), sim_now);
    run_for(1000000);
    CHECK(jp_playing(&jp) == 0);
}

/* the whole way: the session -> a map every 5 ms -> ticks; every tick lands on the computer's own beat */
/* the stick sets the session's tempo and plays / stops it; the computer follows. Alone: refused */
static void test_leads(void)
{
    jp_init(&jp, P, M, 0x1234, stick_out, NULL);
    CHECK(jp_set_tempo(&jp, 400000, 0) == -1);
    CHECK(jp_set_playing(&jp, 1, 0) == -1);
    pc_start(100000, 500000);
    run_for(6000000);
    CHECK(jp_set_tempo(&jp, 400000, sim_now) == 0);
    run_for(1000000);
    CHECK(lk_mpb(&pc) == 400000);
    CHECK(jp_set_playing(&jp, 1, sim_now) == 0);
    run_for(1000000);
    CHECK(lk_playing(&pc) == 1 && jp_playing(&jp) == 1);
    CHECK(jp_set_playing(&jp, 0, sim_now) == 0);
    run_for(1000000);
    CHECK(lk_playing(&pc) == 0);
}

static void test_ticks_on_the_session(void)
{
    struct jc_map m = {0};
    struct jc_ticker c;
    struct jc_fire f;
    int64_t worst = 0, t_pub = 0;
    uint32_t ticks = 0;
    pc_start(100000, 451127);
    run_for(6000000);
    CHECK(jp_joined(&jp));
    jc_ticker_init(&c, 4);
    jc_map_make(&m, sim_now, jp_beat_at(&jp, sim_now), jp_mpb(&jp), 0);
    CHECK(jc_begin(&c, &m, sim_now) == 0);
    while (ticks < 2000) {
        if (sim_now >= t_pub) {
            jc_map_make(&m, sim_now, jp_beat_at(&jp, sim_now), jp_mpb(&jp), 0);
            t_pub = sim_now + 5000;
        }
        if (sim_now >= c.t_due) {
            int64_t n = c.n, due = c.t_due;
            jc_step(&c, &m, sim_now, &f);
            int64_t truth = lk_time_at(&pc, jc_tick_ub(n));
            int64_t e = abs64(truth - due);
            worst = e > worst ? e : worst;
            ticks++;
        }
        run_for(500);
    }
    CHECK(worst < 2500);
    CHECK(c.resyncs == 0);
    printf("peer: ticks against the session: worst %lld us of schedule error\n", (long long)worst);
}

static void test_leave(void)
{
    pc_start(100000, 500000);
    run_for(6000000);
    CHECK(jp_joined(&jp));
    jp_leave(&jp, sim_now);
    CHECK(!jp.on && !jp_joined(&jp));
    CHECK(jp.host[0].where == 0);
}

int main(void)
{
    test_address();
    test_arp_usb();
    test_arp_air();
    test_arp_other();
    test_conflict();
    test_on_a_network();
    test_silent_until_link_heard();
    test_alone_leaves();
    test_for_the_peer_alone();
    test_joins_session();
    test_follows();
    test_leads();
    test_ticks_on_the_session();
    test_leave();
    printf("peer: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
