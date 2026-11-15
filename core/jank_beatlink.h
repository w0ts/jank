/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat tracker as the Link session's master: its tempo and beat grid, and its stop and start, go into the session.
 * No ESP-IDF, no tracker (core/beat/ is fed by the caller), no clock: times are the Link host's microseconds.
 *
 *   tempo       set when it differs from the session's by more than 0.02 BPM
 *   phase       the session's beat grid is moved only when it is more than 5 ms from the tracked beat
 *   transport   stopped -> Link stop at once; running again -> Link start on the next bar (quantum beats), sent
 *               JST_LEAD_US before it (jank_start.h); the audio stream closed (LINK mode) -> the session left as it is
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
};

struct jbl_in {
    uint8_t running;                    /* the tracker follows a steady or moving beat (not stopped, audio flowing) */
    int64_t beat_us;                    /* a tracked beat, Link host time: any beat of the grid */
    double period_us;                   /* the beat grid's period */
    uint8_t passive;                    /* no audio stream (LINK mode): the session is not the tracker's to start or stop */
};

void jbl_init(struct jbl *c);
/* settled: the node is in a session with peers and has its state (jp_settled). start: the peer's start, sent by its
 * owner's poll (jst_poll). now: host time. quantum: beats a bar. */
void jbl_step(struct jbl *c, struct lk_node *k, struct jst *start, int settled, int64_t now, const struct jbl_in *in, uint32_t quantum);
/* Where the session's nearest beat is from an instant, in us (+: the session's beat comes later), given the session's
 * beat position at that instant (micro-beats, lk_beat_at) and its microseconds a beat. For the beat reports: what Live
 * follows against the beat as the stick takes it to be heard. */
int32_t jbl_session_offset_us(int64_t ub_at, int32_t mpb);
#endif
