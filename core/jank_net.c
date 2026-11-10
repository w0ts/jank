/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_net.h"
#include <string.h>

#define ETH_HDR 14u
#define IP_HDR 20u
#define UDP_HDR 8u
#define DHCP_AT (ETH_HDR + IP_HDR + UDP_HDR)
#define BOOTP_LEN 236u
#define COOKIE 0x63825363u
#define OPT_AT (DHCP_AT + BOOTP_LEN + 4u)
#define PORT_SERVER 67u
#define PORT_CLIENT 68u
#define LEASE_MAX_S 604800u            /* a week: it keeps the milliseconds in 32 bits, and a renewal costs nothing */
#define MASK_DEFAULT 0xFFFFFF00u
#define IGMP_IP_HDR 24u
#define IGMP_LEN 8u

enum { DOWN, SELECTING, REQUESTING, BOUND };
enum { DISCOVER = 1, OFFER, REQUEST, DECLINE, ACK, NAK };
enum { OPT_MASK = 1, OPT_HOST = 12, OPT_WANT_IP = 50, OPT_LEASE = 51, OPT_TYPE = 53, OPT_SERVER = 54, OPT_PARAMS = 55, OPT_CLIENT_ID = 61, OPT_END = 255 };

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

static int reached(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

static uint32_t next_rand(struct jank_net *n)
{
    uint32_t x = n->rnd;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    n->rnd = x;
    return x;
}

uint32_t jn_sum(const uint8_t *data, size_t n)
{
    uint32_t sum = 0;
    for (size_t i = 0; i + 1u < n; i += 2u)
        sum += rd16(data + i);
    if (n & 1u)
        sum += (uint32_t)data[n - 1u] << 8;
    while (sum >> 16)
        sum = (sum & 0xFFFFu) + (sum >> 16);
    return ~sum & 0xFFFFu;
}

void jn_init(struct jank_net *n, const uint8_t mac[6], uint32_t seed, const char *name, uint32_t group, jn_out_fn out, void *ctx)
{
    size_t i = 0;
    memset(n, 0, sizeof *n);
    memcpy(n->mac, mac, 6);
    n->rnd = seed ? seed : 0x6A616E6Bu;
    n->group = group;
    n->out = out;
    n->ctx = ctx;
    for (; name[i] && i < JN_HOST_MAX; i++)
        n->host[i] = name[i] == ' ' ? '-' : name[i];
}

void jn_set_mac(struct jank_net *n, const uint8_t mac[6]) { memcpy(n->mac, mac, 6); }

static size_t put_ip_option(uint8_t *o, uint8_t code, uint32_t ip)
{
    o[0] = code;
    o[1] = 4;
    wr32(o + 2, ip);
    return 6;
}

static size_t put_options(const struct jank_net *n, uint8_t type, uint8_t *o)
{
    static const uint8_t client[] = {0, 'j', 'a', 'n', 'k'};
    static const uint8_t params[] = {OPT_MASK};
    size_t at = 0, host = strlen(n->host);
    o[at++] = OPT_TYPE;
    o[at++] = 1;
    o[at++] = type;
    o[at++] = OPT_CLIENT_ID;
    o[at++] = (uint8_t)(sizeof client + 3u);
    memcpy(o + at, client, sizeof client);
    at += sizeof client;
    memcpy(o + at, n->mac + 3, 3);
    at += 3u;
    o[at++] = OPT_HOST;
    o[at++] = (uint8_t)host;
    memcpy(o + at, n->host, host);
    at += host;
    o[at++] = OPT_PARAMS;
    o[at++] = sizeof params;
    memcpy(o + at, params, sizeof params);
    at += sizeof params;
    if (type == REQUEST) {
        at += put_ip_option(o + at, OPT_WANT_IP, n->offered);
        at += put_ip_option(o + at, OPT_SERVER, n->server);
    }
    o[at++] = OPT_END;
    return at;
}

static void send_dhcp(struct jank_net *n, uint8_t type)
{
    uint8_t *f = n->tx, *ip = f + ETH_HDR, *udp = ip + IP_HDR, *b = f + DHCP_AT;
    size_t len;
    memset(f, 0, sizeof n->tx);
    memset(f, 0xFF, 6);
    memcpy(f + 6, n->mac, 6);
    wr16(f + 12, 0x0800u);
    b[0] = 1;
    b[1] = 1;
    b[2] = 6;
    wr32(b + 4, n->xid);
    b[10] = 0x80;                      /* broadcast: a reply to the MAC could be the computer's business */
    memcpy(b + 28, n->mac, 6);
    wr32(b + BOOTP_LEN, COOKIE);
    len = OPT_AT + put_options(n, type, f + OPT_AT);
    ip[0] = 0x45;
    wr16(ip + 2, len - ETH_HDR);
    ip[8] = 64;
    ip[9] = 17;
    memset(ip + 16, 0xFF, 4);
    wr16(ip + 10, jn_sum(ip, IP_HDR));
    wr16(udp, PORT_CLIENT);
    wr16(udp + 2, PORT_SERVER);
    wr16(udp + 4, len - ETH_HDR - IP_HDR);
    n->out(n->ctx, f, len);            /* no UDP checksum: allowed in IPv4, as the Link peer's frames */
}

static void send_join(struct jank_net *n)
{
    uint8_t *f = n->tx, *ip = f + ETH_HDR, *g = ip + IGMP_IP_HDR;
    memset(f, 0, ETH_HDR + IGMP_IP_HDR + IGMP_LEN);
    f[0] = 0x01;
    f[1] = 0x00;
    f[2] = 0x5E;
    f[3] = (uint8_t)(n->group >> 16 & 0x7Fu);
    f[4] = (uint8_t)(n->group >> 8);
    f[5] = (uint8_t)n->group;
    memcpy(f + 6, n->mac, 6);
    wr16(f + 12, 0x0800u);
    ip[0] = 0x46;
    ip[1] = 0xC0;
    wr16(ip + 2, IGMP_IP_HDR + IGMP_LEN);
    ip[8] = 1;
    ip[9] = 2;
    wr32(ip + 12, n->ip);
    wr32(ip + 16, n->group);
    ip[20] = 0x94;                     /* router alert */
    ip[21] = 0x04;
    wr16(ip + 10, jn_sum(ip, IGMP_IP_HDR));
    g[0] = 0x16;                       /* an IGMPv2 report */
    wr32(g + 4, n->group);
    wr16(g + 2, jn_sum(g, IGMP_LEN));
    n->out(n->ctx, f, ETH_HDR + IGMP_IP_HDR + IGMP_LEN);
}

static void restart(struct jank_net *n, uint32_t now_ms, uint32_t wait_ms)
{
    n->state = SELECTING;
    n->ip = 0;
    n->mask = 0;
    n->xid = next_rand(n);
    n->backoff_ms = JN_RETRY_MS;
    n->t_next = now_ms + wait_ms;
}

void jn_link(struct jank_net *n, int up, uint32_t now_ms)
{
    if (up)
        restart(n, now_ms, 0);
    else {
        n->state = DOWN;
        n->ip = 0;
        n->mask = 0;
    }
}

static void again_later(struct jank_net *n, uint32_t now_ms)
{
    n->t_next = now_ms + n->backoff_ms;
    n->backoff_ms = n->backoff_ms * 2u > JN_RETRY_MAX_MS ? JN_RETRY_MAX_MS : n->backoff_ms * 2u;
}

void jn_poll(struct jank_net *n, uint32_t now_ms)
{
    if (n->state == DOWN)
        return;
    if (n->state == BOUND) {
        if (reached(now_ms, n->t_expire)) {
            restart(n, now_ms, 0);
        } else {
            if (reached(now_ms, n->t_igmp)) {
                n->t_igmp = now_ms + JN_IGMP_MS;
                send_join(n);
            }
            if (reached(now_ms, n->t_renew) && reached(now_ms, n->t_next)) {
                send_dhcp(n, REQUEST);
                again_later(n, now_ms);
            }
            return;
        }
    }
    if (!reached(now_ms, n->t_next))
        return;
    send_dhcp(n, n->state == SELECTING ? DISCOVER : REQUEST);
    again_later(n, now_ms);
}

struct reply {
    uint8_t type;
    uint32_t server, mask, lease_s;
};

static void read_options(const uint8_t *o, size_t n, struct reply *r)
{
    size_t at = 0;
    while (at + 1u < n && o[at] != OPT_END) {
        size_t len;
        if (!o[at]) {
            at++;
            continue;
        }
        len = o[at + 1u];
        if (at + 2u + len > n)
            return;
        if (o[at] == OPT_TYPE && len == 1u)
            r->type = o[at + 2u];
        else if (o[at] == OPT_SERVER && len == 4u)
            r->server = rd32(o + at + 2u);
        else if (o[at] == OPT_MASK && len == 4u)
            r->mask = rd32(o + at + 2u);
        else if (o[at] == OPT_LEASE && len == 4u)
            r->lease_s = rd32(o + at + 2u);
        at += 2u + len;
    }
}

static void bound(struct jank_net *n, uint32_t yiaddr, const struct reply *r, uint32_t now_ms)
{
    uint32_t lease_ms = (r->lease_s > LEASE_MAX_S ? LEASE_MAX_S : r->lease_s) * 1000u;
    n->state = BOUND;
    n->ip = yiaddr;
    n->mask = r->mask ? r->mask : MASK_DEFAULT;
    n->t_expire = now_ms + lease_ms;
    n->t_renew = now_ms + lease_ms / 2u;
    n->t_next = n->t_renew;
    n->backoff_ms = JN_RETRY_MS;
    n->t_igmp = now_ms + JN_IGMP_MS;
    send_join(n);
}

int jn_input(struct jank_net *n, const uint8_t *eth, size_t len, uint32_t now_ms)
{
    const uint8_t *ip = eth + ETH_HDR, *b;
    struct reply r = {0, 0, 0, 0};
    size_t ihl;
    if (n->state == DOWN || len < ETH_HDR + IP_HDR || rd16(eth + 12) != 0x0800u || (ip[0] >> 4) != 4u || ip[9] != 17u)
        return 0;
    ihl = (size_t)(ip[0] & 0x0Fu) * 4u;
    if (ihl < IP_HDR || len < ETH_HDR + ihl + UDP_HDR + BOOTP_LEN + 4u)
        return 0;
    if (rd16(ip + ihl) != PORT_SERVER || rd16(ip + ihl + 2u) != PORT_CLIENT)
        return 0;
    b = ip + ihl + UDP_HDR;
    if (b[0] != 2u || rd32(b + 4) != n->xid || memcmp(b + 28, n->mac, 6) || rd32(b + BOOTP_LEN) != COOKIE)
        return 0;
    read_options(b + BOOTP_LEN + 4u, len - (size_t)(b + BOOTP_LEN + 4u - eth), &r);
    if (r.type == OFFER && n->state == SELECTING && r.server) {
        n->offered = rd32(b + 16);
        n->server = r.server;
        n->state = REQUESTING;
        n->backoff_ms = JN_RETRY_MS;
        n->t_next = now_ms + JN_RETRY_MS;
        send_dhcp(n, REQUEST);
    } else if (r.type == ACK && n->state != SELECTING) {
        bound(n, rd32(b + 16), &r, now_ms);
    } else if (r.type == NAK) {
        restart(n, now_ms, JN_RETRY_MS);
    }
    return 1;
}

uint32_t jn_ip(const struct jank_net *n) { return n->ip; }
uint32_t jn_mask(const struct jank_net *n) { return n->mask; }

/* DHCP options the computer must not get from the stick: router, DNS, domain, search, routes (classless, Microsoft's) */
static int a_way_out(uint8_t code) { return code == 3u || code == 6u || code == 15u || code == 119u || code == 121u || code == 249u; }

static void blank_way_out(uint8_t *eth, size_t len)
{
    size_t at = OPT_AT;
    while (at < len && eth[at] != 255u) {
        size_t n;
        if (eth[at] == 0u) {
            at++;
            continue;
        }
        if (at + 1u >= len || at + 2u + eth[at + 1u] > len)
            return;
        n = 2u + eth[at + 1u];
        if (a_way_out(eth[at]))
            memset(eth + at, 0, n);                  /* PAD bytes: the length stays, so the IP header does too */
        at += n;
    }
}

int jn_to_computer(uint8_t *eth, size_t len)
{
    if (len >= ETH_HDR + 40u + 8u && eth[12] == 0x86u && eth[13] == 0xDDu) {
        uint8_t next = eth[ETH_HDR + 6u];
        const uint8_t *l4 = eth + ETH_HDR + 40u;
        if (next == 58u && l4[0] == 134u)            /* ICMPv6 router advertisement */
            return 0;
        if (next == 17u && l4[0] == 0x02u && l4[1] == 0x23u)   /* from DHCPv6's server port, 547 */
            return 0;
        return 1;
    }
    if (len >= OPT_AT && eth[12] == 0x08u && eth[13] == 0x00u && (eth[ETH_HDR] & 0x0Fu) == 5u && eth[ETH_HDR + 9u] == 17u
        && eth[ETH_HDR + IP_HDR] == 0u && eth[ETH_HDR + IP_HDR + 1u] == 67u && eth[ETH_HDR + IP_HDR + 2u] == 0u
        && eth[ETH_HDR + IP_HDR + 3u] == 68u && rd32(eth + OPT_AT - 4u) == COOKIE) {
        blank_way_out(eth, len);
        eth[ETH_HDR + IP_HDR + 6u] = 0;              /* UDP checksum 0: none, allowed over IPv4 */
        eth[ETH_HDR + IP_HDR + 7u] = 0;
    }
    return 1;
}
