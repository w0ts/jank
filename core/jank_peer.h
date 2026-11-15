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
 * a session and never pushes its default tempo on one.
 *
 * AUDIO mode is the exception (jp_solo): the beat tracker leads, so the stick needs no one to follow. Alone, it runs a
 * session of its own (as any Link app does, on its own tempo, beat and start / stop, which the tracker sets), sent as
 * Link's (silent with the setting off or no address). A peer coming later joins it, as Link's rule says: the older
 * session wins, so the stick's tempo, phase and transport carry on and the newcomer takes them. A session older than the
 * stick's (an app that was there first, out of reach) is joined as before: the stick takes its timeline and start / stop
 * state, then the tracker leads it. The last peer leaving changes nothing in AUDIO mode. The sound gone (LINK mode) while
 * alone: the session ends at once (nothing to follow, as before). */
#ifndef JANK_PEER_H
#define JANK_PEER_H
#include <stddef.h>
#include <stdint.h>
#include "link.h"
#include "jank_start.h"

#define JP_TO_AIR 1u
#define JP_TO_USB 2u
#define JP_HOSTS 8u
#define JP_ALONE_MS 8000u              /* longer than a peer's 5 s ttl */
#define JP_DEFAULT_MPB 500000          /* 120 BPM, only until the session's tempo is known */
#define JP_SETTLE_US 1000000           /* in the same session with peers this long (4 of their broadcasts: their tempo, beat
                                        * and start / stop are in) and no other session waiting to be measured: settled */
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
    uint32_t net_mask;                 /* a normal network's: 0 on the air of MODE JANK */
    uint8_t on_network;
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
    struct jst start;                  /* a start waiting for its bar line (the play command, the beat tracker) */
    uint8_t sess_seen[8];              /* the session the settle hold counts for */
    int64_t t_settle;                  /* host us the hold started: joined, or the session changed */
    uint8_t was_settled;               /* settled once in this session */
    uint8_t solo;                      /* AUDIO mode: a session of its own while alone (jp_solo) */
};

void jp_init(struct jank_peer *p, const uint8_t usb_mac[6], const uint8_t air_mac[6], uint32_t seed, jp_out_fn out, void *ctx);
void jp_allow(struct jank_peer *p, int allowed, int64_t now_us);          /* the setting: off leaves at once */
void jp_set_air_mac(struct jank_peer *p, const uint8_t mac[6]);           /* the bridge adopted another M */
/* a normal network: the address is the network's, 0 until there is one, and the peer is silent without it. Called once
 * before anything is sent; never back to link-local */
void jp_set_network(struct jank_peer *p, uint32_t ip, uint32_t mask, int64_t now_us);
/* a frame seen by the bridge, from JP_TO_AIR or JP_TO_USB: 1 pass it on, 0 it was for the peer alone */
int jp_input(struct jank_peer *p, const uint8_t *eth, size_t len, uint32_t from, int64_t now_us);
void jp_poll(struct jank_peer *p, int64_t now_us);                        /* every few ms */
void jp_leave(struct jank_peer *p, int64_t now_us);                       /* another GROUP: forget the session and the hosts */

/* in a session with peers */
int jp_joined(const struct jank_peer *p);
/* AUDIO mode (1) or not, every bridge step: 1 runs a session of its own while alone (started at the next jp_poll). Set
 * back to 0 while alone: that session ends (a bye-bye), and 1 is returned (the outputs have nothing to follow) */
int jp_solo(struct jank_peer *p, int solo, int64_t now_us);
/* the session; meaningful while jp_in_session: joined, or alone in a session of its own (AUDIO mode) */
int jp_in_session(const struct jank_peer *p);
/* joined for JP_SETTLE_US in the same session, no other session waiting to be measured before that: the session's tempo,
 * beat and start / stop are the peers'. Until then the stick starts nothing, sends no clock and sets no tempo. Alone in a
 * session of its own: settled (the state is the stick's); a peer joining that session keeps it settled */
int jp_settled(const struct jank_peer *p, int64_t now_us);
int32_t jp_mpb(const struct jank_peer *p);
int64_t jp_beat_at(const struct jank_peer *p, int64_t now_us);
int jp_playing(const struct jank_peer *p);
uint32_t jp_peers(const struct jank_peer *p);
/* the stick changes the session, as any Link app may: 0 done, -1 not in a settled session (alone in LINK mode it has none
 * to change) */
int jp_set_tempo(struct jank_peer *p, int32_t mpb, int64_t now_us);
/* play: a start on the next bar line (quantum beats), sent JST_LEAD_US before it by jp_poll (jank_start.h); nothing when
 * the session plays already. Stop: at once */
int jp_set_playing(struct jank_peer *p, int play, uint32_t quantum, int64_t now_us);
/* a start waits for its bar line: 1 and the bar's session micro-beats */
int jp_start_armed(const struct jank_peer *p, int64_t *bar_ub);
#endif
