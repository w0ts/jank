/* SPDX-License-Identifier: GPL-3.0-only */
/* MIDI clock from the Link session: the tick maths, with no ESP-IDF, no Link and no division on the path the
 * timer interrupt runs.
 *
 * A task turns the session into a jc_map every few ms: when tick n falls, as a straight line in the hardware timer's
 * microseconds. The timer interrupt calls jc_fire at each tick: it says what to send and when the next tick is due.
 * Tick n is session beat n/24, so n mod (24 x quantum) is Link's phase: a device started on such a tick is in phase. */
#ifndef JANK_CLOCK_H
#define JANK_CLOCK_H
#include <stdint.h>

#define JC_PPQN 24
#define JC_CLOCK 0xF8u
#define JC_START 0xFAu
#define JC_STOP 0xFCu
#define JC_RESYNC_TICKS 6              /* further from the session than this: jump, do not slew */

struct jc_map {
    int64_t t_ref;                     /* us of tick n_ref */
    int64_t n_ref;
    uint64_t q16;                      /* us a tick, 16 fractional bits */
    uint8_t valid;
};

/* the map at session beat ub (micro-beats) at time now, tempo mpb, the clock moved by offset_us */
void jc_map_make(struct jc_map *m, int64_t now, int64_t ub, int64_t mpb, int32_t offset_us);
/* the time of tick n on the map, no division */
int64_t jc_time(const struct jc_map *m, int64_t n);
/* session micro-beats of tick n */
int64_t jc_tick_ub(int64_t n);

struct jc_ticker {
    int64_t n;                         /* the next tick */
    int64_t t_due;                     /* when it is due */
    uint32_t bar_ticks;                /* 24 x quantum */
    uint8_t started;                   /* n and t_due hold a tick */
    uint8_t playing;                   /* a MIDI start went out, no stop since */
    uint8_t want_start, want_stop;
    uint8_t resync;                    /* the button: stop at the next tick, start at the next bar line after it */
    uint32_t sent;                     /* clocks */
    uint32_t resyncs;
};

struct jc_fire {
    uint8_t bytes[3];
    uint8_t n;
    int64_t next;                      /* due time of the next call */
};

void jc_ticker_init(struct jc_ticker *c, uint32_t quantum);
/* the first alarm, or another map after a gap: where to start. 0 ok, -1 no valid map */
int jc_begin(struct jc_ticker *c, const struct jc_map *m, int64_t now);
/* the interrupt's work at the due time; now is the time read in the interrupt, used only to notice a stall */
void jc_step(struct jc_ticker *c, const struct jc_map *m, int64_t now, struct jc_fire *o);

/* the session's wish, called from the task on a change only: a start at the next bar line, or a stop at the next tick */
void jc_want(struct jc_ticker *c, int play);
/* the button's click: MIDI stop (0xFC) before the next clock, always, then start (0xFA) just before the clock of the
 * first bar line after that tick, whatever the session's transport; clocks go on all the way */
void jc_resync(struct jc_ticker *c);
/* Link's Start Stop Sync, or with auto transport whether a session is joined */
int jc_should_play(int auto_transport, int link_playing, uint32_t link_peers);
#endif
