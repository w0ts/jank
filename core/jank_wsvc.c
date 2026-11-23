/* SPDX-License-Identifier: GPL-3.0-only */
/* The Wi-Fi diagnostics service's frames (jank_wsvc.h). */
#include "jank_wsvc.h"
#include <string.h>
#include "jank_net.h"

#define ETH_HDR 14u
#define IP_HDR 20u
#define UDP_HDR 8u
#define ARP_LEN 42u                    /* Ethernet header and a 28-byte ARP packet */
#define TYPE_IP 0x0800u
#define TYPE_ARP 0x0806u
#define PROTO_UDP 17u
#define IP_BROADCAST 0xFFFFFFFFu
#define IP_FRAGMENT_BITS 0x3FFFu       /* more-fragments and the offset: a datagram in pieces is not put together here */
#define IP_DONT_FRAGMENT 0x4000u
#define IP_TTL 64u
#define ARP_REQUEST 1u
#define ARP_REPLY 2u

static unsigned rd16(const uint8_t *p) { return (unsigned)(p[0] << 8 | p[1]); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)rd16(p) << 16 | rd16(p + 2); }

static void wr16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void wr32(uint8_t *p, uint32_t v)
{
    wr16(p, v >> 16);
    wr16(p + 2, v & 0xFFFFu);
}

/* a request for our address -> its reply in out */
static int arp_input(const uint8_t our_mac[6], uint32_t our_ip, const uint8_t *eth, uint8_t *out, size_t out_cap, size_t *out_len)
{
    const uint8_t *a = eth + ETH_HDR;
    if (rd16(a) != 1u || rd16(a + 2) != TYPE_IP || a[4] != 6u || a[5] != 4u || rd16(a + 6) != ARP_REQUEST || rd32(a + 24) != our_ip)
        return JW_IGNORE;
    if (out_cap < ARP_LEN)
        return JW_IGNORE;
    memcpy(out, a + 8, 6);                                     /* to the asker ... */
    memcpy(out + 6, our_mac, 6);                               /* ... from us */
    wr16(out + 12, TYPE_ARP);
    memcpy(out + 14, a, 6);                                    /* the hardware and protocol types and sizes, as asked */
    wr16(out + 20, ARP_REPLY);
    memcpy(out + 22, our_mac, 6);                              /* sender: our MAC at our address */
    wr32(out + 28, our_ip);
    memcpy(out + 32, a + 8, 6);                                /* target: the asker */
    wr32(out + 38, rd32(a + 14));
    *out_len = ARP_LEN;
    return JW_ARP_REPLIED;
}

static int printable(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (p[i] < 0x20u || p[i] > 0x7Eu)
            return 0;
    return 1;
}

/* where an answer may go: a host on our subnet (not the subnet's network or broadcast address, not us), a unicast MAC, a port */
static int sender_ok(uint32_t our_ip, uint32_t our_mask, const uint8_t *src_mac, uint32_t src_ip, unsigned src_port)
{
    uint32_t host = src_ip & ~our_mask;
    if (!our_mask || (src_mac[0] & 1u) || !src_port)
        return 0;
    if ((src_ip & our_mask) != (our_ip & our_mask) || !host || host == ~our_mask || src_ip == our_ip)
        return 0;
    return 1;
}

/* a clean UDP request to JW_PORT, addressed to us or to everyone -> *req */
static int udp_input(uint32_t our_ip, uint32_t our_mask, const uint8_t *eth, size_t len, struct jw_req *req)
{
    const uint8_t *ip = eth + ETH_HDR, *udp, *pl;
    size_t ihl = (size_t)(ip[0] & 0x0Fu) * 4u, total, ulen, n;
    uint32_t dst;
    if ((ip[0] >> 4) != 4u || ihl < IP_HDR || len < ETH_HDR + ihl + UDP_HDR || ip[9] != PROTO_UDP)
        return JW_IGNORE;
    total = rd16(ip + 2);
    if (total < ihl + UDP_HDR || total > len - ETH_HDR || jn_sum(ip, ihl) != 0 || (rd16(ip + 6) & IP_FRAGMENT_BITS))
        return JW_IGNORE;
    dst = rd32(ip + 16);
    if (dst != our_ip && dst != IP_BROADCAST)
        return JW_IGNORE;
    udp = ip + ihl;
    ulen = rd16(udp + 4);
    if (rd16(udp + 2) != JW_PORT || ulen < UDP_HDR || ulen > total - ihl)
        return JW_IGNORE;
    pl = udp + UDP_HDR;
    n = ulen - UDP_HDR;
    while (n && (pl[n - 1u] == '\n' || pl[n - 1u] == '\r'))
        n--;
    if (!n || n > JW_LINE_MAX || !printable(pl, n))
        return JW_IGNORE;
    if (!sender_ok(our_ip, our_mask, eth + 6, rd32(ip + 12), rd16(udp)))
        return JW_IGNORE;
    memcpy(req->src_mac, eth + 6, 6);
    req->src_ip = rd32(ip + 12);
    req->src_port = (uint16_t)rd16(udp);
    req->broadcast = (uint8_t)(dst == IP_BROADCAST);
    memcpy(req->line, pl, n);
    req->line[n] = 0;
    return JW_REQUEST;
}

int jw_input(const uint8_t our_mac[6], uint32_t our_ip, uint32_t our_mask, const uint8_t *eth, size_t len, struct jw_req *req, uint8_t *out, size_t out_cap,
             size_t *out_len)
{
    *out_len = 0;
    if (!our_ip || len < ARP_LEN)
        return JW_IGNORE;
    switch (rd16(eth + 12)) {
    case TYPE_ARP: return arp_input(our_mac, our_ip, eth, out, out_cap, out_len);
    case TYPE_IP: return udp_input(our_ip, our_mask, eth, len, req);
    default: return JW_IGNORE;
    }
}

unsigned jw_chunks(size_t len) { return len ? (unsigned)((len + JW_CHUNK - 1u) / JW_CHUNK) : 1u; }

size_t jw_reply_frame(const uint8_t our_mac[6], uint32_t our_ip, const struct jw_req *to, unsigned index, int last, const char *text, size_t n,
                      uint8_t *out, size_t cap)
{
    uint8_t *ip = out + ETH_HDR, *udp = ip + IP_HDR;
    size_t total = ETH_HDR + IP_HDR + UDP_HDR + 1u + n;
    if (n > JW_CHUNK || index > JW_CHUNKS_MAX || cap < total)
        return 0;
    memcpy(out, to->src_mac, 6);
    memcpy(out + 6, our_mac, 6);
    wr16(out + 12, TYPE_IP);
    memset(ip, 0, IP_HDR);
    ip[0] = 0x45u;
    wr16(ip + 2, IP_HDR + UDP_HDR + 1u + n);
    wr16(ip + 6, IP_DONT_FRAGMENT);
    ip[8] = IP_TTL;
    ip[9] = PROTO_UDP;
    wr32(ip + 12, our_ip);
    wr32(ip + 16, to->src_ip);
    wr16(ip + 10, jn_sum(ip, IP_HDR));
    wr16(udp, JW_PORT);
    wr16(udp + 2, to->src_port);
    wr16(udp + 4, UDP_HDR + 1u + n);
    wr16(udp + 6, 0);                                          /* no UDP checksum: allowed over IPv4, as jank_net's own frames */
    udp[UDP_HDR] = (uint8_t)(index | (last ? 0x80u : 0u));
    if (n)
        memcpy(udp + UDP_HDR + 1u, text, n);
    return total;
}

int jw_allowed_broadcast(const char *line) { return line && !strcmp(line, "get"); }

int jw_allowed(const char *line)
{
    static const char *const OK[] = {"get", "status", "about", "home", "beat", "diag", "line", "drive"};
    if (!line)
        return 0;
    for (size_t i = 0; i < sizeof OK / sizeof OK[0]; i++)
        if (!strcmp(line, OK[i]))
            return 1;
    return 0;
}
