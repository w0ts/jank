/* SPDX-License-Identifier: GPL-3.0-only */
/* A Link start on the session's next bar line, held back until JST_LEAD_US before that bar.
 *
 * What a start means on the wire. Link's start / stop state is (isPlaying, beats, timestamp): the session beat the state
 * takes effect at, and when it was set (the later one wins). Ableton's library sends setIsPlaying(true, T) as
 * beats = the session beat at T (include/ableton/Link.ipp toIncomingClientState) and gives a receiver T back as the
 * time of that beat (Link.ipp toSessionState); LinkHut then puts its own beat 0 on the next bar line at or after T
 * (examples/linkaudio/AudioEngine.cpp: requestBeatAtStartPlayingTime(0, quantum); Link.ipp requestBeatAtTime,
 * nextPhaseMatch). So "play from bar B" is: isPlaying 1, beats B, timestamp now.
 *
 * Why it is held back. Optimist's FM-1 does not read the beats: when it sees the session playing it sends MIDI start on
 * the next bar line of the session it has not queued yet (optimist firmware/src/wifi/wifi_link.c wl_clock, k % 96, 25 ms
 * ahead; commits 9601cbd7 and 9e930e92), and so does jank's own MIDI clock (jank_clock.c jc_want). A start published a
 * bar ahead would start those a bar early. Published JST_LEAD_US before bar B, every reader agrees on B: the ones that
 * read the beats (Live, LinkHut) and the ones that take the next bar line after they hear it (the FM-1, the stick).
 *
 * No clock, no ESP-IDF: times are the Link host's microseconds; called with the Link node's owner holding its lock. */
#ifndef JANK_START_H
#define JANK_START_H
#include <stdint.h>
#include "link.h"

#define JST_LEAD_US 150000              /* the start goes out this long before its bar line */
#define JST_LATE_US 50000               /* later than this before the bar (a stall, an unsettled session): the next bar */

struct jst {
    uint8_t armed;                      /* a start waits for its bar line */
    uint32_t quantum;                   /* beats a bar, as armed */
    int64_t ub;                         /* that bar line, session micro-beats */
    uint32_t commits, moves;            /* starts sent; bars given up for the next one */
};

/* the first bar line (a multiple of quantum beats) at or after session beat ub */
int64_t jst_bar_at_or_after(int64_t ub, uint32_t quantum);
/* a start on the first bar line at least JST_LEAD_US ahead; armed already: kept as it is */
void jst_arm(struct jst *s, const struct lk_node *k, int64_t now, uint32_t quantum);
void jst_cancel(struct jst *s);
/* every few ms: sends the start when its time has come and allowed is set (1 sent). Not allowed, or too late: the next
 * bar line. The session playing already (another peer started it): nothing more to do */
int jst_poll(struct jst *s, struct lk_node *k, int64_t now, int allowed);
#endif
