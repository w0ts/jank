/* SPDX-License-Identifier: GPL-3.0-only */
/* jank's own Link peer, on top of the bridge and separate from it: the bridge works the same with this module gone.
 * It runs Optimist's link/link.c (GPL-3.0-only, a C port of Ableton's Link protocol) over Ethernet frames, with the
 * smallest IP it can: ARP, IPv4, UDP to port 20808. No computer is needed: on a power bank the air side is all there is.
 *
 * Addresses. The air has one hardware address, the computer's adapter's M, which the radio acknowledges. The peer has its
 * own IPv4 address on the same M on the air (the units see one station with two addresses, as they would a computer with
 * two). Towards the computer it uses its own MAC P, because a computer cannot send frames to its own MAC.
 *
 * A late start. The peer stays silent until it hears Link's multicast, and leaves when it has been alone for
 * JP_ALONE_MS. A session's age decides who joins whom, so the peer's session is always the younger one: jank follows
 * a session and never pushes its default tempo on one. */
#ifndef JANK_PEER_H
#define JANK_PEER_H
#include <stddef.h>
#include <stdint.h>
#include "link.h"

#define JP_TO_AIR 1u
#define JP_TO_USB 2u
#define JP_HOSTS 8u
#define JP_ALONE_MS 8000u              /* longer than a peer's 5 s ttl */
#define JP_DEFAULT_MPB 500000          /* 120 BPM, only until the session's tempo is known */
#define JP_FRAME_MAX (14u + 20u + 8u + LK_MSG_MAX)

typedef void (*jp_out_fn)(void *ctx, uint32_t where, const uint8_t *eth, size_t len);

struct jp_host {
    uint32_t ip;
    uint8_t mac[6];
    uint8_t where;                     /* JP_TO_AIR or JP_TO_USB; 0: a free place */
};

struct jank_peer {
    struct lk_node k;
    uint8_t air_mac[6], usb_mac[6];
    uint32_t ip;
    uint32_t rnd;
    uint8_t allowed;                   /* the setting */
    uint8_t on;                        /* in the session's traffic */
    uint16_t ip_id;
    int64_t t_company;                 /* us it last had a peer, or it started */
    jp_out_fn out;
    void *ctx;
    struct jp_host host[JP_HOSTS];
    uint8_t tx[JP_FRAME_MAX];
    uint32_t rx_frames, tx_frames, ip_conflicts, no_route;
};

void jp_init(struct jank_peer *p, const uint8_t usb_mac[6], const uint8_t air_mac[6], uint32_t seed, jp_out_fn out, void *ctx);
void jp_allow(struct jank_peer *p, int allowed, int64_t now_us);          /* the setting: off leaves at once */
void jp_set_air_mac(struct jank_peer *p, const uint8_t mac[6]);           /* the bridge adopted another M */
/* a frame seen by the bridge, from JP_TO_AIR or JP_TO_USB: 1 pass it on, 0 it was for the peer alone */
int jp_input(struct jank_peer *p, const uint8_t *eth, size_t len, uint32_t from, int64_t now_us);
void jp_poll(struct jank_peer *p, int64_t now_us);                        /* every few ms */
void jp_leave(struct jank_peer *p, int64_t now_us);                       /* another GROUP: forget the session and the hosts */

/* the session; meaningful while jp_joined */
int jp_joined(const struct jank_peer *p);
int32_t jp_mpb(const struct jank_peer *p);
int64_t jp_beat_at(const struct jank_peer *p, int64_t now_us);
int jp_playing(const struct jank_peer *p);
uint32_t jp_peers(const struct jank_peer *p);
#endif
