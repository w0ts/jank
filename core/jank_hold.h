/* SPDX-License-Identifier: GPL-3.0-only */
/* AUDIO mode's Stop / Start (HOME's YES, the control line's `stop` / `play`): the HOLD of the outputs, as the emulator's
 * (player.html setHold, engine.py hold) and the user's rule "start stop should start stop the midi and modular clocks".
 *
 *   Stop   the hold on: the MIDI clock outputs stop at once (a MIDI Stop on the next tick). LINK TRANSPORT = FOLLOW+LEAD:
 *          the Link session stops too (once, at the press: jank_beatlink.h `held`), and the tracker does not start it
 *          again while held. IGNORE, FOLLOW: the session is left alone, the tracker keeps leading it. In every option the
 *          tracker keeps listening, and the hold stays through the tracker's own stops and through LINK mode (where the
 *          outputs stay stopped too).
 *   Start  the hold off: the outputs wait for the tracker to lock (a steady beat), then start on the session's next bar
 *          line (the MIDI clock's own start rule, jank_clock.h). FOLLOW+LEAD: the tracker starts the session on its next
 *          bar line (jank_start.h), and the outputs follow the session as always.
 *
 * Without a hold the outputs do what the transport setting says (jank_clock.h jc_base: in AUDIO mode the session's start /
 * stop, which the tracker leads; in LINK mode jc_should_play: IGNORE = transport AUTO, the clocks run whenever a session
 * is there; FOLLOW / FOLLOW+LEAD = transport LINK, the clocks start and stop with the session).
 *
 * TWO LAYERS (the user, 2026-11-15: "if I manually stopped, don't start again automatically"). The USER layer is the hold:
 * a Stop (AUDIO mode's, or LINK mode's `stop`) stays until the user's own Start (`play` in either mode), in both modes
 * (the mode itself is automatic: the sound coming and going), through everything automatic: the gate, silence and sound,
 * a re-lock, AUTO BAR, a fold, the session of its own ending and starting again, a peer joining or starting the session,
 * a USB audio stream restart. Nothing but jho_set / jho_link_play / jho_link_stop (the user's commands, bridge_play)
 * writes it. Power-on is stopped (M2h, the user: "come up with manual state: stopped"; jho_init) unless the user's
 * last press was Start: that press is stored (core/jank_prefs.h `started`) and bridge_start does it again at boot. The AUTOMATIC layer (LOW CONFIDENCE's gate, the silence) stops and starts the outputs only
 * while the user layer says started, and never changes the user layer. The outputs run only when user = started AND
 * automatic = open:
 *
 *   user      automatic                      outputs
 *   stopped   anything                       stopped (power-on, the user's Stop; only the user's Start ends it)
 *   started   open                           run (started on the session's bar line once the tracker is locked)
 *   started   gate closed (conf < threshold) stopped; open again at threshold + 5: run again, no new Start needed
 *   started   silence (0.5 s, -60 dBFS)      stopped at once; ends a Start's override (the next music waits for the gate)
 *   Start     gate closed                    the Start forces the gate open (override) until conf >= threshold + 5 once
 *
 * LOW CONFIDENCE (core/jank_conf.h): the gate (the grid-fit confidence under the THRESHOLD with STOP / START, or 0.5 s of
 * silence) is an automatic Stop that lifts itself. Gated: the outputs stop at once (a MIDI Stop on the next tick), and
 * with FOLLOW+LEAD the Link session stops (once, as the hold's press); IGNORE, FOLLOW: the session is left to the tracker.
 * Back above the threshold + JCF_HYST_PCT: the outputs start again on the session's next bar line (the MIDI clock's start
 * rule), the session too with FOLLOW+LEAD (jank_start.h). The user's hold wins: held, nothing starts whatever the gate
 * says. LINK mode: no gate either.
 *
 * The user's Start wins too (M2g, the user's rule 2026-11-15: "My Start should win"): a Start (held or not) disarms the
 * confidence gate. The outputs then start on the session's next bar line once the tracker is locked, whatever the
 * confidence (the icon armed until they run). The gate re-arms once the confidence has reached the threshold +
 * JCF_HYST_PCT (jho_in.conf_open), and from then on stops and starts as before. The silence stops the outputs at once,
 * Start or not, and ends the disarm (M2h: the automatic layer starts over; the user layer stays started). A Stop ends
 * the disarm. KEEP RUNNING: no gate, nothing changes. */
#ifndef JANK_HOLD_H
#define JANK_HOLD_H
#include <stdint.h>

enum jho_lt { JHO_IGNORE = 0, JHO_FOLLOW = 1, JHO_LEAD = 2 };          /* LINK TRANSPORT (the emulator's LT_MODES) */
enum jho_outputs { JHO_STOPPED = 0, JHO_ARMED = 1, JHO_RUNNING = 2 };  /* the transport icon: square, blinking, triangle */

struct jho {
    uint8_t hold;                       /* the USER layer: Stop was pressed (either mode), and no Start since */
    uint8_t arming;                     /* Start was pressed: the outputs wait for the tracker to lock */
    uint8_t gate_off;                   /* M2g: the user's Start disarmed LOW CONFIDENCE's gate, until the confidence reached
                                         * the threshold + JCF_HYST_PCT once (jho_in.conf_open) or a Stop */
    uint32_t holds, releases;
};

struct jho_in {
    uint8_t audio;                      /* AUDIO mode: the chosen source has sound (jank_live.h) */
    uint8_t lt;                         /* enum jho_lt */
    uint8_t base;                       /* the outputs as the transport setting wants them (jank_clock.h jc_base) */
    uint8_t locked;                     /* the tracker follows a steady beat (jank_beatlink.h jbl_in.running) */
    uint8_t clock_running;              /* the MIDI clock outputs play (their Start went out) */
    uint8_t gated;                      /* LOW CONFIDENCE's gate (jank_conf.h jcf_gate): the confidence under the threshold */
    uint8_t silent;                     /* the silence (0.5 s under -60 dBFS): stops the outputs whatever the Start */
    uint8_t conf_open;                  /* the confidence at the threshold + JCF_HYST_PCT: a Start's disarm ends */
    uint8_t bar_settling;               /* M2k: a Start's (or a fold's) bar placement is not made yet (the beat task's next
                                         * step): the clocks do not start before it, so they start on the placed 1 */
};

/* power-on: the user's layer stopped (M2h, the user: "come up with manual state: stopped"; a constant, nothing stored) */
void jho_init(struct jho *h);
/* Stop (1) or Start (0) in AUDIO mode. Start ends a hold (the outputs then wait for the lock) and disarms LOW
 * CONFIDENCE's gate (gate_off) whether held or not; Stop ends that disarm */
void jho_set(struct jho *h, int hold);
/* `play` in LINK mode, the user's Start: the hold of either mode ends (the clocks follow the transport again; with
 * transport LINK the session starts too, bridge.c), and the gate is disarmed for when the sound comes, as AUDIO mode's */
void jho_link_play(struct jho *h);
/* `stop` in LINK mode (player.html linkStartStop: "Stop is Stop in every option"): the user's Stop, the same hold as
 * AUDIO mode's: the stick's own clocks stop and stay stopped whoever starts the session again (another stick's tracker,
 * Live), whatever peer comes (transport AUTO) and when the sound comes (AUDIO mode), until the user's Start. With
 * transport LINK (FOLLOW+LEAD) the session stops too (bridge.c); with AUTO (IGNORE) it is left alone */
void jho_link_stop(struct jho *h);
/* whether the MIDI clock outputs should play now (jank_clock.h jc_want); a lock while arming ends the arming */
int jho_outputs(struct jho *h, const struct jho_in *in);
/* FOLLOW+LEAD's part: the Link session is held stopped (jank_beatlink.h jbl_in.held), by the user's hold or the gate */
int jho_session_held(const struct jho *h, const struct jho_in *in);
/* the transport icon in AUDIO mode: held or gated -> stopped; the clocks playing -> running; otherwise a start that
 * resolves by itself (the tracker locks, the bar line comes) -> armed */
uint8_t jho_icon(const struct jho *h, const struct jho_in *in);
const char *jho_icon_name(uint8_t icon);   /* "stopped" "armed" "running" */
#endif
