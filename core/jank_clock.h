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
    uint8_t want_usb_start;            /* a Start for the USB-hosted devices alone at the next bar line (jc_usb_start) */
    uint32_t sent;                     /* clocks */
    uint32_t resyncs;
};

struct jc_fire {
    uint8_t bytes[3];
    uint8_t n;
    uint8_t usb_start;                 /* a Start (0xFA) for the USB-hosted devices alone, before this tick's bytes (jc_usb_start); the other outputs get bytes only */
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
/* a Digitone asks to start while the clock already plays (LINK MODE, owner 2026-11-26 "only the Digitone"): a Start for the USB-hosted devices alone (jc_fire.usb_start)
 * on the next bar line, by a real Start's rules (not in a stop's tick, not on a tick scheduled before a jump of the session); the MIDI outs, the clock pins and USB
 * device mode go on untouched. Dropped when the clock does not play at that tick (or stops): a start of the clock's own (jc_want) reaches every output anyway */
void jc_usb_start(struct jc_ticker *c);
/* Link's Start Stop Sync, or with auto transport whether a session is joined; alone (M2l: the stick's own session in
 * LINK mode too), auto follows that session's start / stop, which only the user's Start / Stop set */
int jc_should_play(int auto_transport, int link_playing, uint32_t link_peers);

/* What the transport setting wants of the clock outputs at a bridge step (jank_hold.h jho_in.base, before AUDIO mode's
 * hold and LINK mode's own Stop):
 *   AUDIO mode   the session's start / stop, whatever the transport setting: the tracker starts it on a bar line once it
 *                locks (jank_beatlink.h), alone (a session of its own) or with peers. (Transport AUTO used to run them
 *                whenever a peer was there, locked or not, the session stopped or not.)
 *   LINK mode    jc_should_play: transport LINK the session's start / stop, AUTO whenever a peer is there.
 * In a session not settled yet, or with none (transport LINK): last, the wish of the step before, held (nothing started
 * or stopped on a guess). gone (no session and none to wait for: AUTO with the peer off, or a session of its own that
 * ended): 0. */
struct jc_tr {
    uint8_t audio;                     /* AUDIO mode */
    uint8_t automatic;                 /* transport AUTO (LINK TRANSPORT IGNORE) */
    uint8_t session;                   /* jank_peer.h jp_in_session */
    uint8_t settled;                   /* jp_settled */
    uint8_t playing;                   /* jp_playing */
    uint8_t gone;
    uint32_t peers;                    /* jp_peers */
};
int jc_base(int last, const struct jc_tr *t);
#endif
