/* SPDX-License-Identifier: GPL-3.0-only */
/* jank's own address on a normal network, in plain Ethernet frames: DHCP, and the IGMP join without which some routers
 * drop Link's multicast. There is no IP stack on the stick.
 *
 * The computer shares jank's MAC, so one MAC holds two leases. jank asks with a client ID of its own (type 0, "jank") and
 * the broadcast flag, and picks its replies by transaction ID: anything else, the computer's DHCP included, passes on. A
 * router that keeps one lease per MAC whatever the client ID would give both the same address: not handled here. */
#ifndef JANK_NET_H
#define JANK_NET_H
#include <stddef.h>
#include <stdint.h>

#define JN_RETRY_MS 2000u
#define JN_RETRY_MAX_MS 16000u
#define JN_IGMP_MS 60000u              /* a router's querier forgets after about 260 s; this is the unsolicited report */
#define JN_FRAME_MAX 342u
#define JN_HOST_MAX 23u

typedef void (*jn_out_fn)(void *ctx, const uint8_t *eth, size_t len);

struct jank_net {
    uint8_t mac[6];
    uint8_t state;
    char host[JN_HOST_MAX + 1u];
    uint32_t group;
    uint32_t xid, rnd;
    uint32_t ip, mask, offered, server;
    uint32_t backoff_ms;
    uint32_t t_next, t_renew, t_expire, t_igmp;
    jn_out_fn out;
    void *ctx;
    uint8_t tx[JN_FRAME_MAX];
};

void jn_init(struct jank_net *n, const uint8_t mac[6], uint32_t seed, const char *name, uint32_t group, jn_out_fn out, void *ctx);
void jn_set_mac(struct jank_net *n, const uint8_t mac[6]);
void jn_link(struct jank_net *n, int up, uint32_t now_ms);   /* associated with the access point, or lost it */
void jn_poll(struct jank_net *n, uint32_t now_ms);
/* a frame from the network -> 1 when it was jank's own DHCP reply, and nobody else's business */
int jn_input(struct jank_net *n, const uint8_t *eth, size_t len, uint32_t now_ms);
/* a frame from the network on its way to the computer -> 0 drop it, 1 pass it (perhaps changed). The computer gets an
 * address on the network (all Link needs) but never a way out through the stick: the DHCP reply's gateway, DNS, domain
 * and routes are blanked, IPv6 router advertisements and DHCPv6 replies dropped. Else a Mac sends its internet here */
int jn_to_computer(uint8_t *eth, size_t len);
uint32_t jn_ip(const struct jank_net *n);                    /* host order; 0: none */
uint32_t jn_mask(const struct jank_net *n);
uint32_t jn_sum(const uint8_t *data, size_t n);              /* the one's complement sum; 0 over a checksummed block */

#endif
