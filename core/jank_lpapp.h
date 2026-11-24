/* SPDX-License-Identifier: GPL-3.0-only */
/* Launchpad X -> Digitone II, Note mode, on stock MIDI (no change to the DN2): what a pad press sends, what the buttons do, and
 * what the pads show. Pure: it takes decoded Launchpad events and the DN2's USB-MIDI packets, and gives back USB-MIDI packets
 * and an LED frame (jank_lpx.h); the USB side is firmware/main. Host-tested (tests/test_lpapp.c).
 *
 * Buttons (Note mode, decided with the owner 2026-11-23): up / down = octave, left / right = the previous / next track (one at
 * a time, no wrap); the Note button is lit. A pad plays its note on the MIDI channel of the current track; the pad stays
 * lit while the DN2 sounds that note on that channel (a sequencer note or one of ours), and every pad with the same pitch
 * lights (the layouts overlap). The channel of each track is a table (default: track n on channel n).
 *
 * Scale page (decided with the owner 2026-11-23: hold Note): while the Note button is held the pads are Launchpad 95's scale editor
 * instead of notes, rows from the top: 1 the absolute-root and horizontal toggles (pads 1, 2) and the layouts (pads 5 chords, 6
 * diatonic, 7 chromatic); 2 the black keys, the relative scale (pad 3) and the circle of fifths back (pad 7); 3 the white keys C to B
 * and the circle of fifths forward (pad 8); 4 the octaves; 5 to 8 the scales (jank_lpscales.h). Releasing Note goes back to playing.
 * LP95's other pads (its two guitar / not-staggered layouts, the drum rack, quick scale) have no job here, except the top row's last pad (pad 8):
 * the track pads switch (green: on).
 *
 * Independent of the Digitone's MIDI channels (owner 2026-11-24, jank_dn2live.h): while its live control answers, a pad plays its note on the current
 * track by the live-control PLAY command, not by a MIDI channel, left / right also make the track the Digitone's active one (and for JLAPP_SELECT_MS the 16
 * bottom pads show which track that is: row 1 tracks 1..8, row 2 tracks 9..16, the chosen one white), and the lights come from the Digitone's own note push:
 * a pad lights while its track (the track pads) or its pitch on the current track (the note pads) sounded in the last JLAPP_PUSH_LIGHT_MS. With the track
 * pads switch on, those two bottom rows play the 16 tracks on their own default note (PLAY's op 2) and the note layout moves up to the six rows left.
 * Without live control the pads play by the MIDI-channel table, as before, and the track pads do nothing.
 * What a press means comes out as `struct jlapp_act`: the firmware sends it by SysEx (live) or as a MIDI packet (jlapp_act_midi). */
#ifndef JANK_LPAPP_H
#define JANK_LPAPP_H
#include <stddef.h>
#include <stdint.h>
#include "jank_dn2live.h"
#include "jank_lpnote.h"
#include "jank_lpx.h"

/* The colours are Launchpad 95's (SkinMK2.py), as palette indexes (manual p. 12) under the names its ColorsMK2.py gives them; "on" is the bright one and "off"
 * its THIRD: the scale page's Horizontal and Key are green, AbsoluteRoot, Mode (the layouts) and Octave are red, Modus (the scales) is blue, and the relative
 * scale and the circle of fifths are plain BLUE; Note mode's arrows (Note.Octave) are green, dim at a limit, and the Note mode button (Mode.Note) LIGHT_BLUE. */
#define JLAPP_GREEN 21u
#define JLAPP_GREEN_THIRD 23u
#define JLAPP_RED 5u
#define JLAPP_RED_THIRD 7u
#define JLAPP_BLUE 45u
#define JLAPP_BLUE_THIRD 47u

#define JLAPP_CHANNELS 16u
#define JLAPP_PADS 64u
#define JLAPP_TRACK_ROWS 2u                          /* the track pads: the two bottom rows */
#define JLAPP_SELECT_MS 1000u                        /* the bottom 16 pads show the chosen track this long after left / right */
#define JLAPP_PUSH_LIGHT_MS 150u                     /* a pushed note lights its pad this long (the note's length is not mapped yet: not measured) */
#define JLAPP_RECENT 32u                             /* pushed notes remembered for the lights */
#define JLAPP_COLOR_TRACK_IDLE 39u                   /* a track pad: LIGHT_BLUE_THIRD; the current track 45 (the scale root's blue), playing 21, just chosen 3 (WHITE) */
#define JLAPP_COLOR_TRACK_CURRENT JLN_COLOR_ROOT
#define JLAPP_COLOR_TRACK_CHOSEN 3u

/* the arrow buttons' and the mode button's palette indexes: Note.Octave On / Off, and Mode.Note On (LIGHT_BLUE) */
#define JLAPP_COLOR_ARROW_ON JLAPP_GREEN             /* a direction that can still be taken */
#define JLAPP_COLOR_ARROW_OFF JLAPP_GREEN_THIRD      /* at the limit: the lowest or highest octave, the first or last track */
#define JLAPP_COLOR_MODE 37u

/* the hosted devices that matter here, told apart by their USB product string */
enum jlapp_role { JLAPP_OTHER, JLAPP_LPX, JLAPP_DN2 };
enum jlapp_role jlapp_role_from_name(const char *product);
/* which of a device's USB-MIDI streaming interfaces is the one to use (0 = the first). The Launchpad X has only one (diag ifs:1: "DAW" and "MIDI" are its two
 * CABLES, not two interfaces), so 1 is clamped to that one; the cable matters, not the interface: usb_lp.c sends on cable 1, the MIDI port */
unsigned jlapp_midi_interface_index(enum jlapp_role r);

struct jlapp_held {
    int16_t note;                      /* -1: the pad is up */
    uint8_t chan;                      /* the channel it was sent on: its Note Off goes there, whatever the track is by then */
    uint8_t track;                     /* the track it was played on */
    uint8_t live;                      /* it was played by live control (PLAY): its Note Off goes the same way */
};

enum jlapp_act_kind {
    JLAPP_ACT_NONE,
    JLAPP_ACT_NOTE_ON,                 /* track, chan, note, vel */
    JLAPP_ACT_NOTE_OFF,                /* track, chan, note */
    JLAPP_ACT_TRACK_ON,                /* a track pad: the track's own default note (live control only) */
    JLAPP_ACT_TRACK_OFF,
    JLAPP_ACT_SELECT                   /* the Digitone's active track becomes `track` (live control only) */
};

/* `live`: the pad was pressed while PLAY worked (jlapp_set_live): send it by live control (jank_dn2live.h); else as a MIDI packet on `chan` */
struct jlapp_act {
    uint8_t kind, track, chan, live, note, vel;
};

struct jlapp_recent {
    uint32_t at_ms;
    uint8_t track, note, used;
};

struct jlapp {
    struct jln n;
    const struct jln_scale *scale;
    uint8_t page;                      /* the Note button is held: the pads are the scale page */
    uint8_t minor_idx;                 /* the minor scale the relative pad returns to (jank_lpscales.h index) */
    uint8_t chan[JLN_TRACKS];          /* each track's MIDI channel, 0..15 */
    uint8_t dn2_cable;                 /* the cable number in the packets to the DN2 */
    struct jlapp_held held[JLAPP_PADS];
    uint8_t playing[JLAPP_CHANNELS][16]; /* from the DN2: a bit a note, per channel (MIDI) */
    uint8_t track_pads;                /* the two bottom rows are the 16 tracks (page switch) */
    uint8_t can_play;                  /* live control answers and PLAY is there: presses go out as live acts */
    uint8_t push;                      /* live control is on: the lights come from its note push, not from the MIDI the DN2 sends */
    uint8_t select_on;                 /* left / right chose a track at select_ms */
    uint16_t track_held;               /* a bit for each track pad that is down */
    uint32_t now_ms, select_ms;
    struct jlapp_recent recent[JLAPP_RECENT];
    uint8_t recent_head;
};

/* `scale` is one of jank_lpscales.h's (the page marks it) or any other */
void jlapp_init(struct jlapp *a, const struct jln_scale *scale, uint8_t dn2_cable);

/* the clock the select overlay and the push lights run on: call before the other calls with the current time (a wrap of the 32 bits is fine) */
void jlapp_tick(struct jlapp *a, uint32_t now_ms);

/* what live control can do now (jdl_is_live, jdl_can_play): `push`, the lights come from the note push (and the MIDI channels are not read for them);
 * `can_play`, a press is a live act */
void jlapp_set_live(struct jlapp *a, int push, int can_play);

/* a Launchpad pad or button; what it asks of the Digitone comes out as acts, at most `max` (one per event); returns how many. A pad press that finds no
 * room (max 0) changes nothing */
size_t jlapp_on_lpx_acts(struct jlapp *a, const struct jlx_ev *ev, struct jlapp_act *out, size_t max);

/* the same as MIDI only (the fallback, and the form the older tests use): the packets go to out (at most max_packets, one per event); returns how many */
size_t jlapp_on_lpx(struct jlapp *a, const struct jlx_ev *ev, uint8_t *out, size_t max_packets);

/* a NOTE_ON / NOTE_OFF act as its USB-MIDI packet (4 bytes): returns 1, or 0 for any other act */
size_t jlapp_act_midi(const struct jlapp *a, const struct jlapp_act *act, uint8_t out[4]);

/* one 4-byte USB-MIDI packet from the DN2: Note On / Off and All Notes Off, the rest is ignored */
void jlapp_on_dn2(struct jlapp *a, const uint8_t pkt[4]);

/* the notes the Digitone pushed (jdl_input): they light for JLAPP_PUSH_LIGHT_MS */
void jlapp_on_push(struct jlapp *a, const struct jdl_note *notes, size_t n);

/* an end for every pad that is down (the Launchpad went away): acts, at most max; returns how many */
size_t jlapp_release_acts(struct jlapp *a, struct jlapp_act *out, size_t max);

/* the same as MIDI Note Offs: packets for the DN2, at most max_packets; returns how many */
size_t jlapp_release_all(struct jlapp *a, uint8_t *out, size_t max_packets);

/* what the Launchpad should show now */
void jlapp_frame(const struct jlapp *a, struct jlx_frame *f);
#endif
