/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the Wi-Fi diagnostics service's frames (core/jank_wsvc.c): ARP for jank's address, a UDP request on JW_PORT, the
 * reply datagrams. The frames are built here by hand, bytes as a router and a laptop would send them. Exit status: the number of
 * failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_net.h"
#include "jank_wota.h"
#include "jank_wsvc.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const uint8_t OUR_MAC[6] = {0x02, 0x6A, 0x6B, 0xA7, 0x6D, 0xE0};
static const uint8_t PC_MAC[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x11, 0x22};
#define OUR_IP 0xC0A8010Au             /* 192.168.1.10 */
#define PC_IP 0xC0A80164u              /* 192.168.1.100 */
#define MASK 0xFFFFFF00u               /* 255.255.255.0 */

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

static unsigned rd16(const uint8_t *p) { return (unsigned)(p[0] << 8 | p[1]); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)rd16(p) << 16 | rd16(p + 2); }

/* an ARP request: who has tpa, tell spa */
static size_t arp_frame(uint8_t *f, unsigned oper, uint32_t tpa)
{
    memset(f, 0xFF, 6);                                          /* broadcast */
    memcpy(f + 6, PC_MAC, 6);
    wr16(f + 12, 0x0806u);
    wr16(f + 14, 1);                                             /* Ethernet */
    wr16(f + 16, 0x0800u);
    f[18] = 6;
    f[19] = 4;
    wr16(f + 20, oper);
    memcpy(f + 22, PC_MAC, 6);
    wr32(f + 28, PC_IP);
    memset(f + 32, 0, 6);
    wr32(f + 38, tpa);
    return 42;
}

/* a UDP datagram from the laptop; ihl_words 5 = no IP options */
static size_t udp_frame(uint8_t *f, uint32_t dst_ip, unsigned dst_port, const char *payload, size_t n, unsigned ihl_words)
{
    uint8_t *ip = f + 14, *udp = ip + 4u * ihl_words;
    unsigned ihl = 4u * ihl_words;
    memcpy(f, OUR_MAC, 6);
    memcpy(f + 6, PC_MAC, 6);
    wr16(f + 12, 0x0800u);
    memset(ip, 0, ihl);
    ip[0] = (uint8_t)(0x40u | ihl_words);
    wr16(ip + 2, ihl + 8u + n);
    ip[8] = 64;
    ip[9] = 17;
    wr32(ip + 12, PC_IP);
    wr32(ip + 16, dst_ip);
    wr16(ip + 10, 0);
    wr16(ip + 10, (uint16_t)jn_sum(ip, ihl));
    wr16(udp, 51000);
    wr16(udp + 2, dst_port);
    wr16(udp + 4, 8u + n);
    wr16(udp + 6, 0);
    memcpy(udp + 8, payload, n);
    return 14u + ihl + 8u + n;
}

static int input(const uint8_t *f, size_t n, struct jw_req *rq, uint8_t *out, size_t *olen)
{
    *olen = 0;
    return jw_input(OUR_MAC, OUR_IP, MASK, f, n, rq, out, 128, olen);
}

static void test_arp(void)
{
    uint8_t f[64], out[128];
    struct jw_req rq;
    size_t olen;
    size_t n = arp_frame(f, 1, OUR_IP);
    CHECK(input(f, n, &rq, out, &olen) == JW_ARP_REPLIED && olen == 42);
    CHECK(!memcmp(out, PC_MAC, 6) && !memcmp(out + 6, OUR_MAC, 6) && rd16(out + 12) == 0x0806u);   /* to the asker, from us */
    CHECK(rd16(out + 20) == 2 && !memcmp(out + 22, OUR_MAC, 6) && rd32(out + 28) == OUR_IP);       /* is-at: our MAC for our IP */
    CHECK(!memcmp(out + 32, PC_MAC, 6) && rd32(out + 38) == PC_IP);
    n = arp_frame(f, 1, OUR_IP + 1);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE && olen == 0);        /* somebody else's address */
    n = arp_frame(f, 2, OUR_IP);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* a reply, not a request */
    n = arp_frame(f, 1, OUR_IP);
    CHECK(input(f, n - 1, &rq, out, &olen) == JW_IGNORE);                  /* cut short */
    CHECK(jw_input(OUR_MAC, 0, MASK, f, n, &rq, out, 128, &olen) == JW_IGNORE);  /* no address yet: never answers for 0.0.0.0 */
    CHECK(jw_input(OUR_MAC, OUR_IP, MASK, f, n, &rq, out, 41, &olen) == JW_IGNORE);   /* no room for the reply */
}

static void test_request(void)
{
    uint8_t f[300], out[128];
    struct jw_req rq;
    size_t olen, n = udp_frame(f, OUR_IP, JW_PORT, "get", 3, 5);
    memset(&rq, 0, sizeof rq);
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST);
    CHECK(!strcmp(rq.line, "get") && rq.src_ip == PC_IP && rq.src_port == 51000 && !memcmp(rq.src_mac, PC_MAC, 6));
    n = udp_frame(f, OUR_IP, JW_PORT, "status\r\n", 8, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST && !strcmp(rq.line, "status"));            /* the line end is not part of it */
    n = udp_frame(f, 0xFFFFFFFFu, JW_PORT, "get", 3, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST && !strcmp(rq.line, "get"));               /* a broadcast finds every jank */
    n = udp_frame(f, OUR_IP, JW_PORT, "get", 3, 6);                                              /* IP options: the UDP header moves */
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST && !strcmp(rq.line, "get"));
}

static void test_request_refusals(void)
{
    uint8_t f[300], out[128];
    struct jw_req rq;
    char big[200];
    size_t olen, n;
    n = udp_frame(f, OUR_IP, JW_PORT + 1, "get", 3, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* another port: not ours, it passes on */
    n = udp_frame(f, OUR_IP + 1, JW_PORT, "get", 3, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* another host */
    n = udp_frame(f, OUR_IP, JW_PORT, "get", 3, 5);
    f[14 + 10] ^= 1;
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* a bad IP checksum */
    n = udp_frame(f, OUR_IP, JW_PORT, "get", 3, 5);
    wr16(f + 14 + 6, 0x2000u);                                              /* more fragments: not reassembled here */
    wr16(f + 14 + 10, 0);
    wr16(f + 14 + 10, (uint16_t)jn_sum(f + 14, 20));
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);
    n = udp_frame(f, OUR_IP, JW_PORT, "", 0, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* nothing to run */
    memset(big, 'a', sizeof big);
    n = udp_frame(f, OUR_IP, JW_PORT, big, JW_LINE_MAX + 1u, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* too long: no answer, nothing to amplify */
    n = udp_frame(f, OUR_IP, JW_PORT, big, JW_LINE_MAX, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST && strlen(rq.line) == JW_LINE_MAX);
    n = udp_frame(f, OUR_IP, JW_PORT, "ge\x01t", 4, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* a control character */
    n = udp_frame(f, OUR_IP, JW_PORT, "g\xC3\xA9t", 4, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* not ASCII */
    n = udp_frame(f, OUR_IP, JW_PORT, "get", 3, 5);
    CHECK(jw_input(OUR_MAC, 0, MASK, f, n, &rq, out, 128, &olen) == JW_IGNORE);  /* no address yet */
    CHECK(input(f, 30, &rq, out, &olen) == JW_IGNORE);                     /* cut short */
    wr16(f + 14 + 2, 20u + 8u + 100u);                                      /* the IP length claims more than the frame holds */
    wr16(f + 14 + 10, 0);
    wr16(f + 14 + 10, (uint16_t)jn_sum(f + 14, 20));
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);
    {
        uint8_t other[60];
        memset(other, 0, sizeof other);
        wr16(other + 12, 0x86DDu);                                          /* IPv6 */
        CHECK(input(other, sizeof other, &rq, out, &olen) == JW_IGNORE);
    }
}

/* the frame's source IP replaced, the header checksum made again */
static void set_src_ip(uint8_t *f, uint32_t ip)
{
    wr32(f + 14 + 12, ip);
    wr16(f + 14 + 10, 0);
    wr16(f + 14 + 10, (uint16_t)jn_sum(f + 14, 20));
}

static void test_source_must_be_a_neighbour(void)
{
    uint8_t f[300], out[128];
    struct jw_req rq;
    size_t olen, n = udp_frame(f, OUR_IP, JW_PORT, "get", 3, 5);
    /* an answer goes to whoever the datagram says it is from: only a host on our own subnet, with an ordinary address, gets one
     * (else a spoofed request aims our answer at a stranger) */
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST);
    set_src_ip(f, 0xC0A80105u);                                              /* another host on the subnet: fine */
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST && rq.src_ip == 0xC0A80105u);
    set_src_ip(f, 0x0A000005u);                                              /* 10.0.0.5: not our subnet */
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);
    set_src_ip(f, 0);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                        /* 0.0.0.0 */
    set_src_ip(f, 0xFFFFFFFFu);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                        /* the broadcast address */
    set_src_ip(f, 0xC0A801FFu);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                        /* the subnet's broadcast address */
    set_src_ip(f, 0xE0000001u);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                        /* multicast */
    set_src_ip(f, OUR_IP);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                        /* our own address */
    set_src_ip(f, 0xC0A80100u);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                        /* the subnet's network address */
    set_src_ip(f, PC_IP);
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST);
    f[6] |= 1;                                                               /* a multicast source MAC */
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);
    f[6] &= (uint8_t)~1u;
    wr16(f + 14 + 20, 0);                                                    /* source port 0 */
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);
    wr16(f + 14 + 20, 51000);
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST);
    CHECK(jw_input(OUR_MAC, OUR_IP, 0, f, n, &rq, out, 128, &olen) == JW_IGNORE);   /* no mask yet: no subnet to check against */
}

static void test_broadcast_flag(void)
{
    uint8_t f[300], out[128];
    struct jw_req rq;
    size_t olen, n = udp_frame(f, OUR_IP, JW_PORT, "get", 3, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST && rq.broadcast == 0);
    n = udp_frame(f, 0xFFFFFFFFu, JW_PORT, "get", 3, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST && rq.broadcast == 1);  /* the answer to everyone's question is kept short */
    CHECK(jw_allowed_broadcast("get") && !jw_allowed_broadcast("status") && !jw_allowed_broadcast("diag") && !jw_allowed_broadcast(""));
    CHECK(!jw_allowed_broadcast(NULL) && !jw_allowed_broadcast("get "));
}

static void test_reply_frame(void)
{
    struct jw_req to;
    uint8_t out[300];
    size_t n;
    memcpy(to.src_mac, PC_MAC, 6);
    to.src_ip = PC_IP;
    to.src_port = 51000;
    to.line[0] = 0;
    n = jw_reply_frame(OUR_MAC, OUR_IP, &to, 2, 0, "name=jank\n", 10, out, sizeof out);
    CHECK(n == 14u + 20u + 8u + 1u + 10u);
    CHECK(!memcmp(out, PC_MAC, 6) && !memcmp(out + 6, OUR_MAC, 6) && rd16(out + 12) == 0x0800u);
    CHECK(out[14] == 0x45 && out[23] == 17 && rd16(out + 16) == 20u + 8u + 1u + 10u && rd32(out + 26) == OUR_IP && rd32(out + 30) == PC_IP);
    CHECK(jn_sum(out + 14, 20) == 0);                                      /* the IP header checksum holds */
    CHECK(rd16(out + 34) == JW_PORT && rd16(out + 36) == 51000 && rd16(out + 38) == 8u + 1u + 10u && rd16(out + 40) == 0);
    CHECK(out[42] == 2 && !memcmp(out + 43, "name=jank\n", 10));            /* chunk 2, not the last */
    n = jw_reply_frame(OUR_MAC, OUR_IP, &to, 5, 1, "ok\n", 3, out, sizeof out);
    CHECK(n > 0 && out[42] == (0x80 | 5) && !memcmp(out + 43, "ok\n", 3));  /* the last: bit 7 */
    n = jw_reply_frame(OUR_MAC, OUR_IP, &to, 0, 1, "", 0, out, sizeof out);
    CHECK(n == 14u + 20u + 8u + 1u && out[42] == 0x80);                     /* an empty last chunk is a whole datagram */
    CHECK(jw_reply_frame(OUR_MAC, OUR_IP, &to, 0, 0, "x", JW_CHUNK + 1u, out, sizeof out) == 0);   /* too long for one datagram */
    CHECK(jw_reply_frame(OUR_MAC, OUR_IP, &to, 128, 0, "x", 1, out, sizeof out) == 0);              /* index past 127 */
    CHECK(jw_reply_frame(OUR_MAC, OUR_IP, &to, 0, 0, "x", 1, out, 20) == 0);                        /* no room */
    {
        char chunk[JW_CHUNK];
        memset(chunk, 'z', sizeof chunk);
        n = jw_reply_frame(OUR_MAC, OUR_IP, &to, 0, 0, chunk, sizeof chunk, out, sizeof out);
        CHECK(n == JW_FRAME_MAX && out[43 + JW_CHUNK - 1u] == 'z');         /* the biggest chunk, whole */
    }
}

/* the signed update's datagrams (jank_wota.h) ride the same port: a first byte of 0x01 is never a text line, so they are handed up whole */
static void test_binary_update_frames(void)
{
    static uint8_t f[1400], payload[JWO_FRAME_MAX + 8u];
    uint8_t out[128];
    struct jw_req rq;
    size_t n, olen;
    memset(payload, 0xAB, sizeof payload);
    payload[0] = JWO_MAGIC;
    payload[1] = JWO_T_CMD;
    n = udp_frame(f, OUR_IP, JW_PORT, (const char *)payload, 40, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_BINARY);
    CHECK(rq.bin_len == 40 && !memcmp(rq.bin, payload, 40));
    CHECK(rq.bin >= f && rq.bin + rq.bin_len <= f + n);                    /* a pointer into the frame: nothing copied */
    CHECK(rq.src_ip == PC_IP && rq.src_port == 51000 && !rq.broadcast && !memcmp(rq.src_mac, PC_MAC, 6));
    payload[38] = '\r';                                                    /* line ends at the end are data in a binary frame, not trimmed */
    payload[39] = '\n';
    n = udp_frame(f, OUR_IP, JW_PORT, (const char *)payload, 40, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_BINARY && rq.bin_len == 40 && rq.bin[39] == '\n');
    n = udp_frame(f, OUR_IP, JW_PORT, (const char *)payload, JWO_FRAME_MAX, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_BINARY && rq.bin_len == JWO_FRAME_MAX);                 /* the biggest request */
    n = udp_frame(f, OUR_IP, JW_PORT, (const char *)payload, JWO_FRAME_MAX + 1u, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* over it */
    n = udp_frame(f, OUR_IP, JW_PORT, (const char *)payload, 2, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_BINARY && rq.bin_len == 2);   /* a HELLO is two bytes */
    n = udp_frame(f, 0xFFFFFFFFu, JW_PORT, (const char *)payload, 2, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* never to everyone: nothing answers a broadcast with a signed anything */
    n = udp_frame(f, OUR_IP + 1u, JW_PORT, (const char *)payload, 2, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* not our address */
    n = udp_frame(f, OUR_IP, JW_PORT + 1u, (const char *)payload, 2, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* not our port */
    n = udp_frame(f, OUR_IP, JW_PORT, (const char *)payload, 40, 5);
    set_src_ip(f, 0x0A000001u);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* from another network: no answer goes there */
    n = udp_frame(f, OUR_IP, JW_PORT, (const char *)payload, 1, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_IGNORE);                      /* one byte of 0x01: neither a frame nor a printable line */
    n = udp_frame(f, OUR_IP, JW_PORT, "status", 6, 5);
    CHECK(input(f, n, &rq, out, &olen) == JW_REQUEST && !strcmp(rq.line, "status") && rq.bin == NULL && rq.bin_len == 0);   /* the text service is as it was */
}

static void test_raw_frame(void)
{
    struct jw_req to;
    uint8_t out[300], payload[JWO_REPLY_LEN];
    size_t n;
    memcpy(to.src_mac, PC_MAC, 6);
    to.src_ip = PC_IP;
    to.src_port = 51000;
    to.line[0] = 0;
    for (size_t i = 0; i < sizeof payload; i++)
        payload[i] = (uint8_t)(0x90 + i);
    n = jw_raw_frame(OUR_MAC, OUR_IP, &to, payload, sizeof payload, out, sizeof out);
    CHECK(n == 14u + 20u + 8u + JWO_REPLY_LEN);
    CHECK(!memcmp(out, PC_MAC, 6) && !memcmp(out + 6, OUR_MAC, 6) && rd16(out + 12) == 0x0800u);
    CHECK(rd16(out + 16) == 20u + 8u + JWO_REPLY_LEN && rd32(out + 26) == OUR_IP && rd32(out + 30) == PC_IP && jn_sum(out + 14, 20) == 0);
    CHECK(rd16(out + 34) == JW_PORT && rd16(out + 36) == 51000 && rd16(out + 38) == 8u + JWO_REPLY_LEN);
    CHECK(!memcmp(out + 42, payload, sizeof payload));                     /* the payload as it is: no index byte in front */
    CHECK(jw_raw_frame(OUR_MAC, OUR_IP, &to, payload, JW_CHUNK + 1u, out, sizeof out) == 0);
    CHECK(jw_raw_frame(OUR_MAC, OUR_IP, &to, payload, sizeof payload, out, 20) == 0);
}

static void test_chunks(void)
{
    CHECK(jw_chunks(0) == 1 && jw_chunks(1) == 1 && jw_chunks(JW_CHUNK) == 1 && jw_chunks(JW_CHUNK + 1u) == 2);
    CHECK(jw_chunks(2040) == (2040u + JW_CHUNK - 1u) / JW_CHUNK && jw_chunks(2040) <= JW_CHUNKS_MAX);   /* the longest control answer fits */
}

static void test_allowed(void)
{
    static const char *const ok[] = {"get", "status", "about", "home", "beat", "diag", "line", "drive"};
    static const char *const no[] = {"set usb host", "set pass x", "reboot", "bootloader", "ota begin 1 2", "scan", "play", "stop", "tempo 120",
                                     "nudge beat 1", "fold 1", "diag intr", "get ", " get", "GET", "", "get\n", "statusx", "set", "wipe settings"};
    for (size_t i = 0; i < sizeof ok / sizeof ok[0]; i++)
        CHECK(jw_allowed(ok[i]));
    for (size_t i = 0; i < sizeof no / sizeof no[0]; i++)
        CHECK(!jw_allowed(no[i]));
    CHECK(!jw_allowed(NULL));
}

int main(void)
{
    test_arp();
    test_request();
    test_request_refusals();
    test_source_must_be_a_neighbour();
    test_broadcast_flag();
    test_reply_frame();
    test_binary_update_frames();
    test_raw_frame();
    test_chunks();
    test_allowed();
    printf("test_wsvc: %d checks, %d failed\n", checks, fails);
    return fails;
}
