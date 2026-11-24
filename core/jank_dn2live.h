/* SPDX-License-Identifier: GPL-3.0-only */
/* Jank's side of the Digitone II's live control (docs/live-control.md in dn2-wishes, branches claude/live-ctl2 and claude/live-ctl4): the pads play and select
 * tracks by SysEx, independent of every MIDI channel setting and of the MIDI input stream, and the sequencer's notes come back by track for the lights.
 * Pure (the clock is passed in): it says what to send and when, and reads what comes back. Host-tested (tests/test_dn2live.c).
 *
 *   44 LIVE   sub 0 STATUS, 1 ON (what = 1: push the notes), 2 OFF       any live request within 10 s keeps live control on (the lease): jank asks at
 *                                                                        least every JDL_KEEPALIVE_MS
 *   49 TRACK  u8 track (0..15)                                           the active track, as [TRK]+[TRIG n] does
 *   53 PLAY   u8 track (0..15, 255 = the active one), u8 op, u8 note, u8 vel     op 0 note off, 1 note on (vel 0: the track's own), 2 the track's own default
 *                                                                        note on, 3 all notes off (track 255: every track); op 0 with note 255 ends the
 *                                                                        track's own default note (what op 2 started); PROPOSED by jank, accepted by
 *                                                                        the Digitone session (stage 4, not built yet): a Digitone without it answers status 2
 *   47 push   DN2 -> jank, unasked: u8 n, u16 lost, n x {track, step, note, velocity, length, flags}
 *   status    0 ok, 2 not understood (no live control in this build, or PLAY not there), 4 live control is off (the lease ran out), 7 REMOTE CONTROL is OFF
 * Needs REMOTE CONTROL ON (SETTINGS > SYSTEM > USB CONFIG). */
#ifndef JANK_DN2LIVE_H
#define JANK_DN2LIVE_H
#include <stddef.h>
#include <stdint.h>

#define JDL_CMD_LIVE 44u
#define JDL_CMD_TRACK 49u
#define JDL_CMD_PLAY 53u              /* not 50: the Digitone's stage 4 uses 50 SCALE, 51 PATTERNS, 52 SONG (renumbered 2026-11-24 at its request); MESSAGE will be 54 */
#define JDL_CMD_PUSH 47u               /* the push's command number (its cmd byte on the wire is 0xAF) */
#define JDL_NOTES_MAX 16u              /* notes in one push */

#define JDL_KEEPALIVE_MS 4000u         /* a live request at least this often while live (the lease is 10 s) */
#define JDL_ASK_MS 1000u               /* LIVE ON again this often while nothing answers */
#define JDL_IDLE_ASK_MS 5000u          /* ... and this often once the Digitone has said REMOTE CONTROL is off, or that it has no live control */

enum jdl_state {
    JDL_DOWN,                          /* nothing asked yet */
    JDL_ASKING,                        /* LIVE ON sent, no answer yet (or the lease ran out) */
    JDL_LIVE,                          /* live control is on */
    JDL_REMOTE_OFF,                    /* the Digitone answered 7: REMOTE CONTROL is off */
    JDL_NO_LIVE                        /* the Digitone answered 2 to LIVE: a build without live control */
};

enum jdl_play_op { JDL_PLAY_OFF = 0, JDL_PLAY_ON = 1, JDL_PLAY_DEFAULT = 2, JDL_PLAY_ALL_OFF = 3 };

struct jdl_note {
    uint8_t track, step, note, vel, length, flags;
};

struct jdl {
    uint16_t seq;
    uint8_t state;                     /* enum jdl_state */
    uint8_t play_ok;                   /* PLAY has not been refused (status 2): 1 until it is */
    uint8_t asked;                     /* a live request has been sent, at sent_ms */
    uint32_t sent_ms;
    uint16_t read_seq, select_seq;     /* the last active-track read and the last select sent */
    uint8_t has_read, has_select;
    uint8_t read_ok;                   /* the no-argument TRACK read has not been refused (status 2): 1 until it is */
    uint8_t fresh, active;             /* an answer to the last read, not yet taken: the Digitone's active track */
};

enum jdl_in {
    JDL_IN_IGNORED,                    /* not a frame of the channel */
    JDL_IN_REPLY,                      /* a reply: the state may have changed */
    JDL_IN_PUSH                        /* a note push: *count notes in the array given */
};

void jdl_init(struct jdl *d);
int jdl_is_live(const struct jdl *d);
int jdl_can_play(const struct jdl *d);                       /* live, and PLAY has not been refused */

/* the request to send now, F0 .. F7 into out, its length; 0 when there is nothing to send yet. LIVE ON while not live (every JDL_ASK_MS, every JDL_IDLE_ASK_MS
 * after a 7 or a 2), a LIVE STATUS every JDL_KEEPALIVE_MS while live */
size_t jdl_poll(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap);
/* a request, only while live (else 0): the active track, a PLAY. A request also keeps the lease */
size_t jdl_track(struct jdl *d, uint32_t now_ms, uint8_t track, uint8_t *out, size_t cap);
size_t jdl_play(struct jdl *d, uint32_t now_ms, uint8_t track, enum jdl_play_op op, uint8_t note, uint8_t vel, uint8_t *out, size_t cap);
/* TRACK with no argument asks for the active track (PROPOSED to the Digitone session 2026-11-24, not confirmed; a refusal, status 2, ends the asking). Only the
 * answer to the last read counts, and only when the read was sent after the last select: jdl_take_active gives that track (0..15) once, else -1 */
int jdl_can_read_track(const struct jdl *d);
size_t jdl_track_read(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap);
int jdl_take_active(struct jdl *d);

/* a SysEx from the Digitone (F0 .. F7, n bytes). A reply moves the state (0 ok on a live request: live; 7: REMOTE CONTROL off; 2: no live control, or no PLAY; 4: the
 * lease ran out, ask again); a push fills notes (room for JDL_NOTES_MAX) and *count */
enum jdl_in jdl_input(struct jdl *d, const uint8_t *sx, size_t n, struct jdl_note *notes, size_t *count);
#endif
