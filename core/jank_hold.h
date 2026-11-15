/* SPDX-License-Identifier: GPL-3.0-only */
/* AUDIO mode's Stop / Start (HOME's YES, the control line's `stop` / `play`): the HOLD of the outputs, as the emulator's
 * (player.html setHold, engine.py hold) and the user's rule "start stop should start stop the midi and modular clocks".
 *
 *   Stop   the hold on: the MIDI clock outputs stop at once (a MIDI Stop on the next tick). LINK TRANSPORT = FOLLOW+LEAD:
 *          the Link session stops too (once, at the press: jank_beatlink.h `held`), and the tracker does not start it
 *          again while held. IGNORE, FOLLOW: the session is left alone, the tracker keeps leading it. In every option the
 *          tracker keeps listening, and the hold stays through the tracker's own stops and through LINK mode.
 *   Start  the hold off: the outputs wait for the tracker to lock (a steady beat), then start on the session's next bar
 *          line (the MIDI clock's own start rule, jank_clock.h). FOLLOW+LEAD: the tracker starts the session on its next
 *          bar line (jank_start.h), and the outputs follow the session as always.
 *
 * Without a hold the outputs do what the transport setting says (jc_should_play: IGNORE = transport AUTO, the clocks run
 * whenever a session is there; FOLLOW / FOLLOW+LEAD = transport LINK, the clocks start and stop with the session).
 * LINK mode (no sound): the hold does not apply; `play` there starts the session as before, and also ends a hold.
 *
 * LOW CONFIDENCE (core/jank_conf.h): the gate (the grid-fit confidence under the THRESHOLD with STOP / START, or 0.5 s of
 * silence) is an automatic Stop that lifts itself. Gated: the outputs stop at once (a MIDI Stop on the next tick), and
 * with FOLLOW+LEAD the Link session stops (once, as the hold's press); IGNORE, FOLLOW: the session is left to the tracker.
 * Back above the threshold + JCF_HYST_PCT: the outputs start again on the session's next bar line (the MIDI clock's start
 * rule), the session too with FOLLOW+LEAD (jank_start.h). The user's hold wins: held, nothing starts whatever the gate
 * says; Start while gated: armed, the outputs wait for the gate as well as for the lock. LINK mode: no gate either. */
#ifndef JANK_HOLD_H
#define JANK_HOLD_H
#include <stdint.h>

enum jho_lt { JHO_IGNORE = 0, JHO_FOLLOW = 1, JHO_LEAD = 2 };          /* LINK TRANSPORT (the emulator's LT_MODES) */
enum jho_outputs { JHO_STOPPED = 0, JHO_ARMED = 1, JHO_RUNNING = 2 };  /* the transport icon: square, blinking, triangle */

struct jho {
    uint8_t hold;                       /* Stop was pressed in AUDIO mode, and no Start since */
    uint8_t arming;                     /* Start was pressed: the outputs wait for the tracker to lock */
    uint32_t holds, releases;
};

struct jho_in {
    uint8_t audio;                      /* AUDIO mode: the chosen source has sound (jank_live.h) */
    uint8_t lt;                         /* enum jho_lt */
    uint8_t base;                       /* the outputs as the transport setting wants them (jc_should_play) */
    uint8_t locked;                     /* the tracker follows a steady beat (jank_beatlink.h jbl_in.running) */
    uint8_t clock_running;              /* the MIDI clock outputs play (their Start went out) */
    uint8_t gated;                      /* LOW CONFIDENCE's gate or the silence (jank_conf.h): the outputs stopped by the tracker */
};

void jho_init(struct jho *h);
/* Stop (1) or Start (0) in AUDIO mode */
void jho_set(struct jho *h, int hold);
/* `play` in LINK mode: the session starts, and a hold from AUDIO mode ends with it (no surprise stop when sound returns) */
void jho_link_play(struct jho *h);
/* whether the MIDI clock outputs should play now (jank_clock.h jc_want); a lock while arming ends the arming */
int jho_outputs(struct jho *h, const struct jho_in *in);
/* FOLLOW+LEAD's part: the Link session is held stopped (jank_beatlink.h jbl_in.held), by the user's hold or the gate */
int jho_session_held(const struct jho *h, const struct jho_in *in);
/* the transport icon in AUDIO mode: held or gated -> stopped; the clocks playing -> running; otherwise a start that
 * resolves by itself (the tracker locks, the bar line comes) -> armed */
uint8_t jho_icon(const struct jho *h, const struct jho_in *in);
const char *jho_icon_name(uint8_t icon);   /* "stopped" "armed" "running" */
#endif
