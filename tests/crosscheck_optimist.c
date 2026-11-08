/* SPDX-License-Identifier: GPL-3.0-only */
/* Test harness, never shipped: jank against the FM-1's own code. It compiles Optimist's wifi/wifi_jank.c and wifi/wifi_ip.c
 * here, with a model of device/wifi_own.c's jn_rx around them, and swaps real air frames with jank's bridge and peer.
 * What it proves: the two sides agree on the format, in both directions. What it does not: anything about radios.
 * Air delay and time steps are this model's. Exit status: the number of failed checks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_core.h"
#include "jank_peer.h"
#include "wifi_ip.c"
#include "wifi_jank.c"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const uint8_t FM_MAC[6] = {0x8C, 0x10, 0x20, 0x30, 0x40, 0x50};
static const uint8_t M[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
static const uint8_t P[6] = {0x06, 0x11, 0x22, 0x33, 0x44, 0x55};
#define FM_IP 0xA9FE0708u
#define PC_IP 0xA9FE0A0Bu
#define GROUP 5u

static int64_t now_us;
static uint32_t rng = 99;
static void fm_rand(uint8_t *o, uint8_t n)
{
    while (n--) {
        rng = rng * 1664525u + 1013904223u;
        *o++ = (uint8_t)(rng >> 16);
    }
}

/* the air */
#define AIR_DELAY_US 1000
#define QLEN 256u
struct frame {
    int64_t at;
    uint32_t len;
    uint8_t f[1600];
};
static struct frame q_to_fm[QLEN], q_to_stick[QLEN];
static uint32_t n_to_fm, n_to_stick;
static void push(struct frame *q, uint32_t *n, const uint8_t *f, uint32_t len)
{
    if (*n < QLEN) {
        q[*n].at = now_us + AIR_DELAY_US;
        q[*n].len = len;
        memcpy(q[*n].f, f, len);
        (*n)++;
    }
}

/* the FM-1 */
static struct jn_net fm_n;
static struct wifi_ip fm_ip;
static struct lk_node fm_k;
static uint8_t fm_tx[1600];
static uint32_t fm_kept, fm_arp_replies;

static void fm_udp_rx(void *ctx, int64_t t, uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport, const uint8_t *p, uint32_t n)
{
    (void)ctx, (void)dst, (void)dport;
    lk_rx(&fm_k, t, src, sport, p, n);
}

static int fm_send(void *ctx, uint32_t ip, uint16_t port, const uint8_t *p, uint32_t n)
{
    uint32_t len = wifi_ip_send_udp(&fm_ip, LK_MCAST_PORT, ip, port, p, n, fm_tx);
    (void)ctx;
    if (len)
        push(q_to_stick, &n_to_stick, fm_tx, len);
    return len ? 0 : -1;
}

/* device/wifi_own.c's jn_rx, step for step */
static void fm_rx(const uint8_t *f, uint32_t len)
{
    uint32_t tx_len = 0, ip;
    if (!jn_heard(&fm_n, f, len, -40, (uint32_t)(now_us / 1000)))
        return;
    ip = jn_sender_ip(f, len);
    CHECK(ip != fm_ip.ip);                      /* no address clash with anything jank sent */
    if (ip)
        wifi_ip_arp_learn(&fm_ip, ip, f + 10);
    if (!wifi_ip_early_filter(&fm_ip, f, len))
        return;
    fm_kept++;
    if (wifi_ip_rx_frame(&fm_ip, f, len, now_us, fm_tx, &tx_len) == 1 && tx_len) {
        fm_arp_replies++;
        push(q_to_stick, &n_to_stick, fm_tx, tx_len);
    }
}

/* jank */
static struct jk_bridge br;
static struct jank_peer jp;
static uint8_t air[JK_AIR_MAX + 8], eth[JK_ETH_MAX + 8];
static uint8_t last_pc[2048];
static size_t last_pc_len;
static uint32_t pc_frames;

static void to_air(const uint8_t *e, size_t len)
{
    size_t n = jk_eth_to_air(&br, e, len, (uint32_t)(now_us / 1000), air, sizeof air);
    if (n)
        push(q_to_fm, &n_to_fm, air, (uint32_t)n);
}

static void jp_out(void *ctx, uint32_t where, const uint8_t *e, size_t len)
{
    (void)ctx;
    if (where == JP_TO_AIR)
        to_air(e, len);
}

static void stick_rx(const uint8_t *f, uint32_t len)
{
    size_t n = jk_air_to_eth(&br, f, len, -40, (uint32_t)(now_us / 1000), eth, sizeof eth);
    if (n && jp_input(&jp, eth, n, JP_TO_AIR, now_us)) {
        memcpy(last_pc, eth, n);
        last_pc_len = n;
        pc_frames++;
    }
}

static void drain(struct frame *q, uint32_t *n, void (*rx)(const uint8_t *, uint32_t))
{
    uint32_t i = 0;
    while (i < *n && q[i].at <= now_us) {
        rx(q[i].f, q[i].len);
        i++;
    }
    if (i) {
        *n -= i;
        memmove(q, q + i, *n * sizeof *q);
    }
}

static void run_for(int64_t us)
{
    int64_t end = now_us + us;
    while (now_us < end) {
        drain(q_to_fm, &n_to_fm, fm_rx);
        drain(q_to_stick, &n_to_stick, stick_rx);
        lk_poll(&fm_k, now_us);
        jp_poll(&jp, now_us);
        now_us += 500;
    }
}

static void test_group_table(void)
{
    for (uint32_t g = 1; g <= 9; g++) {
        struct jn_net n;
        uint8_t b[6];
        jn_init(&n, FM_MAC, g);
        CHECK(jk_channel(g) == n.ch);
        CHECK(!jk_bssid(g, b) && !memcmp(b, n.bssid, 6));
    }
}

static void boot(int32_t mpb)
{
    struct lk_cfg c = {0, LK_MCAST_PORT, fm_send, NULL};
    uint8_t mac[6];
    now_us = 1000000;
    n_to_fm = n_to_stick = 0;
    pc_frames = 0;
    jn_init(&fm_n, FM_MAC, GROUP);
    wifi_ip_ibss_init(&fm_ip, FM_MAC, fm_n.bssid, FM_IP, fm_rand);
    fm_ip.udp_rx = fm_udp_rx;
    CHECK(wifi_ip_bind(&fm_ip, LK_MCAST_PORT) == 0);
    wifi_ip_join(&fm_ip, LK_MCAST_IP);
    lk_init(&fm_k, &c, 5, mpb);
    lk_enable(&fm_k, 1, now_us);
    lk_set_addr(&fm_k, FM_IP, LK_MCAST_PORT);
    memcpy(mac, M, 6);
    CHECK(jk_init(&br, mac, GROUP, JK_FILTER_LINK) == 0);
    jp_init(&jp, P, M, 0x77, jp_out, NULL);
    jp_allow(&jp, 1, now_us);
}

/* a real unit's frame is read by the bridge */
static void test_fm1_frame_to_computer(void)
{
    static const uint8_t pay[4] = {1, 2, 3, 4};
    uint32_t n;
    boot(500000);
    n = wifi_ip_send_udp(&fm_ip, LK_MCAST_PORT, LK_MCAST_IP, LK_MCAST_PORT, pay, 4, fm_tx);
    CHECK(n == 24 + 8 + 20 + 8 + 4);
    size_t e = jk_air_to_eth(&br, fm_tx, n, -40, 0, eth, sizeof eth);
    CHECK(e == 14 + 20 + 8 + 4);
    CHECK(!memcmp(eth, "\x01\x00\x5E\x4C\x4E\x4B", 6) && !memcmp(eth + 6, FM_MAC, 6) && eth[12] == 8 && eth[13] == 0);
    CHECK(!memcmp(eth + 42, pay, 4));
}

/* the computer's ARP goes through the bridge, the FM-1 answers, the answer comes back to the computer */
static void test_computer_arp(void)
{
    uint8_t e[42] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    boot(500000);
    memcpy(e + 6, M, 6);
    e[12] = 8, e[13] = 6;
    memcpy(e + 14, "\x00\x01\x08\x00\x06\x04\x00\x01", 8);
    memcpy(e + 22, M, 6);
    memcpy(e + 28, "\xA9\xFE\x0A\x0B", 4);
    memcpy(e + 38, "\xA9\xFE\x07\x08", 4);
    to_air(e, 42);
    run_for(5000);
    CHECK(fm_arp_replies == 1);
    CHECK(pc_frames >= 1);                      /* (the FM-1's Link announce may come first) */
    CHECK(!memcmp(last_pc, M, 6) && !memcmp(last_pc + 6, FM_MAC, 6) && last_pc[13] == 6);
    CHECK(last_pc[21] == 2 && !memcmp(last_pc + 22, FM_MAC, 6) && !memcmp(last_pc + 28, "\xA9\xFE\x07\x08", 4));
}

/* the computer's Link multicast and a unicast ping shape: read by the FM-1's layers as its own unit's would be */
static void test_computer_frames_pass_fm1_filters(void)
{
    uint8_t e[64] = {0x01, 0x00, 0x5E, 0x4C, 0x4E, 0x4B};
    boot(500000);
    memcpy(e + 6, M, 6);
    e[12] = 8;
    e[14] = 0x45;
    e[17] = 40;
    e[22] = 1, e[23] = 17;
    memcpy(e + 26, "\xA9\xFE\x0A\x0B\xE0\x4C\x4E\x4B", 8);
    e[34] = 0x51, e[35] = 0x48, e[36] = 0x51, e[37] = 0x48, e[39] = 20;
    size_t n = jk_eth_to_air(&br, e, 54, 0, air, sizeof air);
    CHECK(n == 24 + 8 + 40);
    CHECK(jn_heard(&fm_n, air, (uint32_t)n, -40, 0) == 1);
    CHECK(jn_sender_ip(air, (uint32_t)n) == PC_IP);
    CHECK(wifi_ip_early_filter(&fm_ip, air, (uint32_t)n) == 1);
    CHECK(jn_count(&fm_n, 0) == 1);
}

/* the FM-1's session, with no computer at all: jank joins it and is a peer in the FM-1's own Link */
static void test_jank_joins_an_fm1(void)
{
    boot(461538);                               /* 130 BPM */
    run_for(100000);
    run_for(8000000);
    CHECK(jp_joined(&jp));
    CHECK(jp_peers(&jp) == 1);
    CHECK(lk_peers(&fm_k) == 1);
    CHECK(jp_mpb(&jp) == 461538);
    CHECK(lk_mpb(&fm_k) == 461538);             /* the FM-1 keeps its tempo: jank did not push 120 */
    int64_t a = lk_beat_at(&fm_k, now_us), b = jp_beat_at(&jp, now_us);
    int64_t d = a > b ? a - b : b - a;
    CHECK(d < 4000);
    CHECK(jn_count(&fm_n, (uint32_t)(now_us / 1000)) == 1);
    CHECK(br.st.drop_bad == 0 && br.st.drop_own == 0);
    printf("crosscheck: jank in an FM-1's session, phase error %lld micro-beats in this model\n", (long long)d);
    lk_set_tempo(&fm_k, 400000, now_us);
    run_for(1000000);
    CHECK(jp_mpb(&jp) == 400000);
}

int main(void)
{
    test_group_table();
    test_fm1_frame_to_computer();
    test_computer_arp();
    test_computer_frames_pass_fm1_filters();
    test_jank_joins_an_fm1();
    printf("crosscheck: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
