/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat tracker as the Link session's master: its tempo and beat grid, and its stop and start, go into the session.
 * No ESP-IDF, no tracker (core/beat/ is fed by the caller), no clock: times are the Link host's microseconds.
 *
 *   tempo       set when it differs from the session's by more than 0.02 BPM
 *   phase       the session's beat grid is moved only when it is more than 5 ms from the tracked beat
 *   transport   stopped -> Link stop at once; running again -> Link start on the next bar (quantum beats), sent
 *               JST_LEAD_US before it (jank_start.h); no sound on the source (LINK mode) -> the session left as it is;
 *               the user's Stop with FOLLOW+LEAD (held) -> Link stop at the press, and no start until the Start
 *
 * Called with the Link node's owner holding its lock, every few hundred ms and on each tracked beat. */
#ifndef JANK_BEATLINK_H
#define JANK_BEATLINK_H
#include <stdint.h>
#include "link.h"
#include "jank_start.h"

#define JBL_TEMPO_DEADBAND_X1000 20     /* 0.02 BPM */
#define JBL_PHASE_US 5000

struct jbl {
    uint8_t playing;                    /* we asked the session to play and have not asked it to stop */
    uint32_t tempo_sets, phase_forces, starts, stops, releases;
    int32_t err_us;                     /* the last grid error seen against the tracked beat, before any correction */
    uint32_t bar_seq;                   /* the downbeat estimator's last decision acted on */
    uint8_t bar_seen;                   /* bar_seq holds one */
    uint32_t bar_moves;                 /* whole-beat moves of the session's bar */
    uint8_t held;                       /* the last step was held (jbl_in.held): the press is acted on once */
};

/* the downbeat estimator (core/beat/jb_down.h) as the bar step wants it */
struct jbl_bar {
    uint8_t locked;                     /* it knows the bar's 1 */
    uint32_t seq;                       /* its decision (one a counted beat) */
    int32_t count;                      /* the count of that beat ... */
    int8_t phi;                         /* ... and the count position (mod 4) of the 1 */
    int64_t beat_us;                    /* that beat, Link host time (as jbl_in's beat_us) */
};

struct jbl_in {
    uint8_t running;                    /* the tracker follows a steady or moving beat (not stopped, audio flowing) */
    int64_t beat_us;                    /* a tracked beat, Link host time: any beat of the grid */
    double period_us;                   /* the beat grid's period */
    uint8_t passive;                    /* no sound on the source (LINK mode): the session is not the tracker's to start or stop */
    uint8_t held;                       /* AUDIO mode's Stop with LINK TRANSPORT = FOLLOW+LEAD (jank_hold.h): the session is
                                         * stopped once, at the press, then neither started nor stopped by the tracker */
};

void jbl_init(struct jbl *c);
/* settled: the node is in a session with peers and has its state (jp_settled). start: the peer's start, sent by its
 * owner's poll (jst_poll). now: host time. quantum: beats a bar. */
void jbl_step(struct jbl *c, struct lk_node *k, struct jst *start, int settled, int64_t now, const struct jbl_in *in, uint32_t quantum);
/* Where the session's nearest beat is from an instant, in us (+: the session's beat comes later), given the session's
 * beat position at that instant (micro-beats, lk_beat_at) and its microseconds a beat. For the beat reports: what Live
 * follows against the beat as the stick takes it to be heard. */
int32_t jbl_session_offset_us(int64_t ub_at, int32_t mpb);

/* AUTO BAR. The whole-beat move that puts the session's bar on the 1: the session's beat at the counted beat's moment
 * (ub_at, micro-beats; half a beat off after a half-beat nudge counts as the same beat) should sit at bar position
 * (count - phi) mod 4. Returns n in -1..2. */
int jbl_bar_shift(int64_t ub_at, int32_t count, int phi);
/* the session's beats jump n whole beats at t (a force; the tempo untouched); an armed start re-targets the next bar
 * line of the moved grid (jank_start.h). Called under the Link node owner's lock. */
void jbl_shift_bar(struct jbl *c, struct lk_node *k, struct jst *start, int n, int64_t t);
/* after jbl_step, with the same lock: at each new LOCKED decision, the bar onto the 1 if it is not there. Only while
 * the tracker leads (settled, not passive, running) and the phase is within JBL_PHASE_US (then the session's beat at
 * the counted beat is unambiguous; otherwise the decision waits for the next step). Returns the move made, 0 none. */
int jbl_bar_step(struct jbl *c, struct lk_node *k, struct jst *start, int settled, int64_t now, const struct jbl_in *in,
                 const struct jbl_bar *bar);
#endif
