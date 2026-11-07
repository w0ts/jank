/* SPDX-License-Identifier: GPL-3.0-only */
/* jank's Link peer (jank_peer.h): ARP, IPv4 and UDP around link.c. */
#include "jank_peer.h"
#include <string.h>

#define ETH_HDR 14u
#define ARP_LEN 28u
#define IP_HDR 20u
#define UDP_HDR 8u
#define LINK_LOCAL 0xA9FE0000u

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) << 16 | rd16(p + 2); }
static void wr16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
static void wr32(uint8_t *p, uint32_t v)
{
    wr16(p, v >> 16);
    wr16(p + 2, v);
}

static uint32_t next_rand(struct jank_peer *p)
{
    uint32_t x = p->rnd;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    p->rnd = x;
    return x;
}

static uint32_t link_local(uint32_t b1, uint32_t b0) { return LINK_LOCAL | (1u + b1 % 254u) << 8 | (1u + b0 % 254u); }
static const uint8_t *our_mac(const struct jank_peer *p, uint32_t where) { return where == JP_TO_USB ? p->usb_mac : p->air_mac; }

void jp_init(struct jank_peer *p, const uint8_t usb_mac[6], const uint8_t air_mac[6], uint32_t seed, jp_out_fn out, void *ctx)
{
    memset(p, 0, sizeof *p);
    memcpy(p->usb_mac, usb_mac, 6);
    memcpy(p->air_mac, air_mac, 6);
    p->rnd = seed ? seed : 0x6A616E6Bu;
    p->out = out;
    p->ctx = ctx;
    p->ip = link_local(usb_mac[4], usb_mac[5]);
}

void jp_set_air_mac(struct jank_peer *p, const uint8_t mac[6]) { memcpy(p->air_mac, mac, 6); }

static const struct jp_host *find_host(const struct jank_peer *p, uint32_t ip)
{
    for (uint32_t i = 0; i < JP_HOSTS; i++)
        if (p->host[i].where && p->host[i].ip == ip)
            return &p->host[i];
    return NULL;
}

static void learn(struct jank_peer *p, uint32_t ip, const uint8_t *mac, uint32_t where)
{
    struct jp_host *h = (struct jp_host *)find_host(p, ip);
    if ((ip & 0xFFFF0000u) != LINK_LOCAL || ip == p->ip)
        return;
    for (uint32_t i = 0; !h && i < JP_HOSTS; i++)
        if (!p->host[i].where)
            h = &p->host[i];
    if (!h)
        h = &p->host[p->ip_id % JP_HOSTS];          /* full: any place, the next frame from it brings it back */
    h->ip = ip;
    h->where = (uint8_t)where;
    memcpy(h->mac, mac, 6);
}

static uint32_t ip_checksum(const uint8_t *h)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < IP_HDR; i += 2u)
        sum += rd16(h + i);
    while (sum >> 16)
        sum = (sum & 0xFFFFu) + (sum >> 16);
    return ~sum & 0xFFFFu;
}

/* the datagram in p->tx: Ethernet addresses left to emit */
static size_t build_udp(struct jank_peer *p, uint32_t dst_ip, uint16_t port, const uint8_t *data, uint32_t n)
{
    uint8_t *f = p->tx, *ip = f + ETH_HDR, *udp = ip + IP_HDR;
    int multicast = (dst_ip & 0xF0000000u) == 0xE0000000u;
    wr16(f + 12, 0x0800u);
    memset(ip, 0, IP_HDR);
    ip[0] = 0x45;
    wr16(ip + 2, IP_HDR + UDP_HDR + n);
    wr16(ip + 4, p->ip_id++);
    wr16(ip + 6, 0x4000u);
    ip[8] = multicast ? 1u : 64u;
    ip[9] = 17;
    wr32(ip + 12, p->ip);
    wr32(ip + 16, dst_ip);
    wr16(ip + 10, ip_checksum(ip));
    wr16(udp, LK_MCAST_PORT);
    wr16(udp + 2, port);
    wr16(udp + 4, UDP_HDR + n);
    wr16(udp + 6, 0);                               /* no checksum: allowed in IPv4, and what the FM-1 sends */
    memcpy(udp + UDP_HDR, data, n);
    return ETH_HDR + IP_HDR + UDP_HDR + n;
}

static void emit(struct jank_peer *p, uint32_t where, const uint8_t *dst, size_t len)
{
    memcpy(p->tx, dst, 6);
    memcpy(p->tx + 6, our_mac(p, where), 6);
    p->tx_frames++;
    p->out(p->ctx, where, p->tx, len);
}

static int link_send(void *ctx, uint32_t ip, uint16_t port, const uint8_t *data, uint32_t n)
{
    static const uint8_t group[6] = {0x01, 0x00, 0x5E, 0x4C, 0x4E, 0x4B};
    struct jank_peer *p = ctx;
    const struct jp_host *h;
    size_t len;
    if (n > LK_MSG_MAX)
        return -1;
    if (ip == LK_MCAST_IP) {
        len = build_udp(p, ip, port, data, n);
        emit(p, JP_TO_AIR, group, len);
        emit(p, JP_TO_USB, group, len);
        return 0;
    }
    h = find_host(p, ip);
    if (!h) {
        p->no_route++;                              /* (a peer's multicast came first, always, so this is rare) */
        return -1;
    }
    emit(p, h->where, h->mac, build_udp(p, ip, port, data, n));
    return 0;
}

static void start(struct jank_peer *p, int64_t now)
{
    struct lk_cfg c;
    c.ip = 0;
    c.port = LK_MCAST_PORT;
    c.send = link_send;
    c.ctx = p;
    lk_init(&p->k, &c, next_rand(p), JP_DEFAULT_MPB);
    lk_enable(&p->k, 1, now);
    lk_set_addr(&p->k, p->ip, LK_MCAST_PORT);
    p->on = 1;
    p->t_company = now;
}

static void stop(struct jank_peer *p, int64_t now)
{
    if (p->on)
        lk_enable(&p->k, 0, now);
    p->on = 0;
}

void jp_allow(struct jank_peer *p, int allowed, int64_t now_us)
{
    p->allowed = allowed != 0;
    if (!allowed)
        stop(p, now_us);
}

void jp_leave(struct jank_peer *p, int64_t now_us)
{
    stop(p, now_us);
    memset(p->host, 0, sizeof p->host);
}

static void redraw_address(struct jank_peer *p)
{
    uint32_t r = next_rand(p);
    p->ip_conflicts++;
    p->ip = link_local(r >> 8 & 255u, r & 255u);
    if (p->on)
        lk_set_addr(&p->k, p->ip, LK_MCAST_PORT);
}

static void reply_arp(struct jank_peer *p, const uint8_t *a, uint32_t where)
{
    uint8_t *f = p->tx, *r = f + ETH_HDR;
    const uint8_t *mine = our_mac(p, where);
    wr16(f + 12, 0x0806u);
    memcpy(r, a, 6);                                /* hardware and protocol types and sizes */
    wr16(r + 6, 2u);
    memcpy(r + 8, mine, 6);
    wr32(r + 14, p->ip);
    memcpy(r + 18, a + 8, 6);
    memcpy(r + 24, a + 14, 4);
    emit(p, where, a + 8, ETH_HDR + ARP_LEN);
}

static int input_arp(struct jank_peer *p, const uint8_t *a, size_t n, uint32_t from)
{
    uint32_t spa, tpa;
    if (n < ARP_LEN || rd16(a) != 1u || rd16(a + 2) != 0x0800u)
        return 1;
    spa = rd32(a + 14);
    tpa = rd32(a + 24);
    if (spa == p->ip && memcmp(a + 8, our_mac(p, from), 6)) {
        redraw_address(p);
        return 1;
    }
    learn(p, spa, a + 8, from);
    if (rd16(a + 6) != 1u || tpa != p->ip)
        return 1;
    reply_arp(p, a, from);
    return 0;
}

static void deliver(struct jank_peer *p, const uint8_t *ip, size_t ihl, size_t n, int multicast, int64_t now)
{
    const uint8_t *udp = ip + ihl;
    if (multicast && !p->on)
        start(p, now);
    if (p->on) {
        p->rx_frames++;
        lk_rx(&p->k, now, rd32(ip + 12), (uint16_t)rd16(udp), udp + UDP_HDR, (uint32_t)(n - ihl - UDP_HDR));
    }
}

/* 1 when this datagram is Link's to the peer */
static int link_for_us(const struct jank_peer *p, const uint8_t *ip, size_t ihl, size_t n, int *multicast)
{
    uint32_t dst = rd32(ip + 16);
    *multicast = dst == LK_MCAST_IP;
    return ip[9] == 17u && n >= ihl + UDP_HDR && rd16(ip + ihl + 2u) == LK_MCAST_PORT && (*multicast || dst == p->ip);
}

static int input_ipv4(struct jank_peer *p, const uint8_t *eth, size_t n, uint32_t from, int64_t now)
{
    const uint8_t *ip = eth + ETH_HDR;
    size_t ihl = (size_t)(ip[0] & 0x0Fu) * 4u;
    int multicast;
    n -= ETH_HDR;
    if (n < IP_HDR || (ip[0] >> 4) != 4u || ihl < IP_HDR || n < ihl)
        return 1;
    if (rd32(ip + 12) == p->ip && memcmp(eth + 6, our_mac(p, from), 6)) {
        redraw_address(p);
        return 1;
    }
    learn(p, rd32(ip + 12), eth + 6, from);
    if (link_for_us(p, ip, ihl, n, &multicast))
        deliver(p, ip, ihl, n, multicast, now);
    return rd32(ip + 16) != p->ip;
}

int jp_input(struct jank_peer *p, const uint8_t *eth, size_t len, uint32_t from, int64_t now_us)
{
    uint32_t type;
    int for_p;
    if (!p->allowed || len < ETH_HDR)
        return 1;
    for_p = from == JP_TO_USB && !memcmp(eth, p->usb_mac, 6);
    type = rd16(eth + 12);
    if (type == 0x0806u)
        return input_arp(p, eth + ETH_HDR, len - ETH_HDR, from) && !for_p;
    if (type == 0x0800u)
        return input_ipv4(p, eth, len, from, now_us) && !for_p;
    return !for_p;
}

void jp_poll(struct jank_peer *p, int64_t now_us)
{
    if (!p->on)
        return;
    lk_poll(&p->k, now_us);
    if (lk_peers(&p->k))
        p->t_company = now_us;
    else if (now_us - p->t_company > (int64_t)JP_ALONE_MS * 1000)
        stop(p, now_us);
}

int jp_joined(const struct jank_peer *p) { return p->on && lk_peers(&p->k) > 0u; }
int32_t jp_mpb(const struct jank_peer *p) { return lk_mpb(&p->k); }
int64_t jp_beat_at(const struct jank_peer *p, int64_t now_us) { return lk_beat_at(&p->k, now_us); }
int jp_playing(const struct jank_peer *p) { return p->on && lk_playing(&p->k); }
uint32_t jp_peers(const struct jank_peer *p) { return p->on ? lk_peers(&p->k) : 0u; }
