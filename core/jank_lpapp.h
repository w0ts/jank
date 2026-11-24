/* SPDX-License-Identifier: GPL-3.0-only */
/* Launchpad X -> Digitone II, Note mode: what a pad press asks of the Digitone, what the buttons do, what the pads show, and the short messages that say what a
 * press did. Pure: it takes decoded Launchpad events and the Digitone's live-control news, and gives back acts (jlapp_act), an LED frame (jank_lpx.h) and
 * messages; the USB side is firmware/main. Host-tested (tests/test_lpapp.c).
 *
 * SysEx only (owner 2026-11-24): the Digitone is played and told what track to use through its live control (jank_dn2live.h, SETTINGS > SYSTEM > USB CONFIG >
 * REMOTE CONTROL ON), never through MIDI channels, which depend on how the Digitone is set up and would mix with the MIDI it gets otherwise. Without live
 * control, or without its PLAY command, the pads play nothing and say why (a message).
 *
 * Note mode: a pad plays its note on the current track (PLAY); up / down = octave; left / right = the previous / next track, one at a time, no wrap, and the
 * Digitone's active track follows. For JLAPP_SELECT_MS after a track press the 16 bottom pads show the track, laid out as the Digitone's trig keys: tracks 1..8 in the upper of the two rows, 9..16 in the lower, the chosen
 * one white. Lights come from the Digitone's own note push: a pad lights while its pitch on the current track (a track pad: its track) sounded in the last
 * JLAPP_PUSH_LIGHT_MS, and a pad that is down is lit; every pad with the same pitch lights (the layouts overlap).
 *
 * Left and right together (owner 2026-11-24) switch FOLLOW: the Launchpad's current track follows the Digitone's (read every JLAPP_FOLLOW_MS, jdl_track_read) as
 * well as the other way round; left and right then show in AMBER. The two presses make no track step. Not available (a message) while the Digitone cannot be asked.
 *
 * Scale page (hold Note): Launchpad 95's scale editor instead of notes, rows from the top: 1 the absolute-root and horizontal toggles (pads 1, 2), the layouts
 * (pads 5 chords, 6 diatonic, 7 chromatic) and the TRACK PADS switch (pad 8); 2 the black keys, the relative scale (pad 3) and the circle of fifths back (pad 7);
 * 3 the white keys C to B and the circle of fifths forward (pad 8); 4 the octaves; 5 to 8 the scales (jank_lpscales.h). Releasing Note goes back to playing.
 * With the track pads on, the two bottom rows play the 16 tracks on their own default note (PLAY's op 2) and the note layout moves up to the six rows left.
 *
 * Messages (what the Launchpad 95 shows in Live's status bar): each change of the scale page, each octave and track step, FOLLOW, and a press that cannot play
 * leave a message of at most JLAPP_MSG_MAX - 1 characters, upper case, 7-bit; the newest replaces an unread one. The firmware shows it on Jank's own screen if
 * it has one, else on the Digitone. */
#ifndef JANK_LPAPP_H
#define JANK_LPAPP_H
#include <stddef.h>
#include <stdint.h>
#include "jank_dn2live.h"
#include "jank_lpact.h"
#include "jank_lpnote.h"
#include "jank_lpstep.h"
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
#define JLAPP_AMBER 9u
#define JLAPP_AMBER_THIRD 11u

#define JLAPP_PADS 64u
#define JLAPP_SCALES_PER_PAGE 24u                    /* the scale page shows the Digitone's 36 scales three rows of eight at a time ... */
#define JLAPP_SCALE_PAGES 2u                         /* ... on two pages, picked by two pads on the row above them */
#define JLAPP_TRACK_ROWS 2u                          /* the track pads: the two bottom rows */
#define JLAPP_SELECT_MS 1000u                        /* the bottom 16 pads show the chosen track this long after left / right */
#define JLAPP_PUSH_LIGHT_MS 150u                     /* a pushed note lights its pad this long (the note's length is not mapped yet: not measured) */
#define JLAPP_RECENT 32u                             /* pushed notes remembered for the lights */
#define JLAPP_FOLLOW_MS 250u                         /* FOLLOW asks the Digitone for its active track this often */
#define JLAPP_FOLLOW_HOLD_MS 500u                    /* ... and does not trust an answer this long after the Launchpad chose a track (the Digitone takes a select at its next UI pass) */
#define JLAPP_MSG_MAX 21u                            /* a message, with its NUL: 20 characters (the Digitone's MESSAGE command takes that many) */
/* The track pads (the 16 bottom pads while the track is shown, and the track pads mode), owner 2026-11-24: not blue (it reads as the scale's root) and not green or purple (the
 * Digitone's own mute modes): every track WHITE, the selected one AMBER, a track that is playing (or held) RED. */
#define JLAPP_COLOR_TRACK_IDLE 3u                    /* WHITE */
#define JLAPP_COLOR_TRACK_CURRENT 9u                 /* AMBER */
#define JLAPP_COLOR_TRACK_PLAYING 5u                 /* RED */

/* the arrow buttons' and the mode button's palette indexes: Note.Octave On / Off, and Mode.Note On (LIGHT_BLUE) */
#define JLAPP_COLOR_ARROW_ON JLAPP_GREEN             /* a direction that can still be taken */
#define JLAPP_COLOR_ARROW_OFF JLAPP_GREEN_THIRD      /* at the limit: the lowest or highest octave, the first or last track */
#define JLAPP_COLOR_FOLLOW_ON JLAPP_AMBER            /* left / right while FOLLOW is on; dim at the first or last track */
#define JLAPP_COLOR_FOLLOW_OFF JLAPP_AMBER_THIRD
#define JLAPP_COLOR_MODE 37u
#define JLAPP_COLOR_MODE_DIM 39u            /* the Note button while a step mode is on: LIGHT_BLUE_THIRD */

/* the hosted devices that matter here, told apart by their USB product string */
enum jlapp_role { JLAPP_OTHER, JLAPP_LPX, JLAPP_DN2 };
enum jlapp_role jlapp_role_from_name(const char *product);
/* which of a device's USB-MIDI streaming interfaces is the one to use (0 = the first). The Launchpad X has only one (diag ifs:1: "DAW" and "MIDI" are its two
 * CABLES, not two interfaces), so 1 is clamped to that one; the cable matters, not the interface: usb_lp.c sends on cable 1, the MIDI port */
unsigned jlapp_midi_interface_index(enum jlapp_role r);

struct jlapp_held {
    int16_t note;                      /* -1: the pad is up */
    uint8_t track;                     /* the track it was played on: its end goes there, whatever the track is by then */
};

/* Modes (owner 2026-11-24): the Note button (a tap, shorter than JLAPP_TAP_MS) is Note mode; Custom goes to the step modes and cycles melodic <-> drum
 * (jank_lpstep.h); held, the Note button is the scale page in every mode. Scene button 5 (side index 4) starts / stops the transport, as Launchpad 95's Note mode.
 * Up / down are the octave in Note mode and in melodic step, nothing in drum step. */
enum jlapp_mode { JLAPP_MODE_NOTE, JLAPP_MODE_STEP };
#define JLAPP_TAP_MS 400u
#define JLAPP_SIDE_START_STOP 4u            /* the side buttons, top (scene 1) = 0: scene 5 */
#define BUTTON_CUSTOM_IDX 6u

struct jlapp {
    struct jln n;
    const struct jln_scale *scale;
    uint8_t page;                      /* the Note button is held: the pads are the scale page */
    uint8_t scale_page;                /* which 24 scales the page shows (0: 1-24, 1: 25-36); the page of the scale in use when the page opens or the scale changes */
    uint8_t dn2_seen;                  /* the Digitone's scale has been read since live control came up */
    uint8_t dn2_idx, dn2_root, dn2_has_root;   /* ... and what it said last: only a CHANGE of it is taken, so a scale chosen here is not undone by an unchanged reading */
    uint8_t minor_idx;                 /* the minor scale the relative pad returns to (jank_lpscales.h index) */
    struct jlapp_held held[JLAPP_PADS];
    uint8_t track_pads;                /* the two bottom rows are the 16 tracks (page switch) */
    uint8_t live_state;                /* enum jdl_state, as the firmware last told (jlapp_set_live) */
    uint8_t play_ok;                   /* PLAY has not been refused */
    uint8_t can_play;                  /* live control is on and PLAY is there: a press can play */
    uint8_t push;                      /* live control is on: the lights come from its note push */
    uint8_t follow;                    /* FOLLOW is on */
    uint8_t read_ok;                   /* the Digitone can be asked for its active track */
    uint8_t buttons;                   /* bit 0 left, bit 1 right: held (for the two together) */
    uint8_t before[2];                 /* the track before the left / right press (the pair puts it back) */
    uint8_t select_on;                 /* the bottom pads show the track since select_ms */
    uint8_t hold_on;                   /* the Launchpad itself chose a track at hold_ms */
    uint8_t why_on;                    /* "why the pads do not play" was said at why_ms */
    uint32_t hold_ms, why_ms;
    uint16_t track_held;               /* a bit for each track pad that is down */
    uint32_t now_ms, select_ms;
    struct jlapp_recent recent[JLAPP_RECENT];
    uint8_t recent_head;
    char msg[JLAPP_MSG_MAX];
    uint8_t msg_new;
    uint8_t mode;                      /* enum jlapp_mode */
    uint8_t step_kind;                 /* enum jlst_kind: the step mode Custom goes to (and the one last used) */
    uint8_t transport;                 /* the clock runs (set by the firmware) */
    uint8_t beat_ok;                   /* the session's beat is known (jlapp_set_beat) */
    uint32_t tick16;                   /* ... in sixteenth notes */
    uint8_t note_used;                 /* a page pad was pressed while the Note button was held: its release is not a tap */
    uint32_t note_down_ms;
    struct jlst step;
};

/* `scale` is one of jank_lpscales.h's (the page marks it) or any other */
void jlapp_init(struct jlapp *a, const struct jln_scale *scale);

/* the clock the select overlay and the push lights run on: call before the other calls with the current time (a wrap of the 32 bits is fine) */
void jlapp_tick(struct jlapp *a, uint32_t now_ms);

/* what live control is: `state` an enum jdl_state, `play_ok` PLAY has not been refused, `read_ok` the active track can be read (jdl_can_read_track). Live
 * and PLAY there: a press plays; live: the lights come from the push, FOLLOW can run */
void jlapp_set_live(struct jlapp *a, int state, int play_ok, int read_ok);

/* The Digitone's scale (owner 2026-11-24: synced both ways; jdl_take_scale): the first reading after live control came up, and then every CHANGE of its reading, becomes the
 * Launchpad's scale (and, when the Digitone says it, the root = the key); the scale page shows the page that holds it. A reading that has not changed leaves what the
 * Launchpad chose alone (a Digitone that refuses SCALE SET keeps its own and the Launchpad its own, until the Digitone changes). The other way round: a scale, key,
 * relative-scale or fifths press on the scale page comes out as JLAPP_ACT_SCALE. `idx` is 0..35; `has_root` 0 below protocol 5. */
void jlapp_on_dn2_scale(struct jlapp *a, unsigned idx, int has_root, unsigned root);

/* a Launchpad pad or button; what it asks of the Digitone comes out as acts, at most `max` (one per event); returns how many. A pad press that finds no
 * room (max 0) changes nothing */
size_t jlapp_on_lpx_acts(struct jlapp *a, const struct jlx_ev *ev, struct jlapp_act *out, size_t max);

/* the notes the Digitone pushed (jdl_input): they light for JLAPP_PUSH_LIGHT_MS */
void jlapp_on_push(struct jlapp *a, const struct jdl_note *notes, size_t n);

/* FOLLOW is on: the Digitone's active track (jdl_take_active). The Launchpad's current track becomes it, shown on the bottom pads, unless the Launchpad chose
 * a track in the last JLAPP_FOLLOW_HOLD_MS */
void jlapp_on_active_track(struct jlapp *a, unsigned track);
/* 1 while FOLLOW is on and can run: the firmware then asks the Digitone for its active track every JLAPP_FOLLOW_MS (jdl_track_read) */
int jlapp_following(const struct jlapp *a);

/* the clock runs (the step modes' lower quadrant then only selects instead of playing; the start / stop scene button lights) */
void jlapp_set_transport(struct jlapp *a, int playing);
/* the session's beat in sixteenth notes (beat x 4), `known` 0 outside a session: the step modes' playhead runs on it while the transport runs (jank_lpstep.h) */
void jlapp_set_beat(struct jlapp *a, int known, uint32_t tick16);
/* a real-time byte (Start / Continue / Stop / clock) from the Digitone's USB MIDI: the playhead follows its own transport while its clock ticks (jank_lpstep.h) */
void jlapp_on_dn2_rt(struct jlapp *a, uint8_t byte);
/* the step modes: the pages of the current track to read now (pages[0..n), 0 outside the step modes), and a page the Digitone answered (jdl_take_page) */
size_t jlapp_step_wanted(const struct jlapp *a, uint8_t pages[2]);
void jlapp_on_page(struct jlapp *a, const struct jdl_page *pg);
/* 1 while a step mode lacks the pages it shows (the track or the page was just switched): the firmware reads at once instead of at its next tick */
int jlapp_step_urgent(const struct jlapp *a);

/* an end for every pad that is down (the Launchpad went away): acts, at most max; returns how many */
size_t jlapp_release_acts(struct jlapp *a, struct jlapp_act *out, size_t max);

/* the message to show now, copied to out (cap bytes, NUL-terminated); returns its length, 0 if none. Takes it: the next call gives the next one */
size_t jlapp_take_message(struct jlapp *a, char *out, size_t cap);

/* what the Launchpad should show now */
void jlapp_frame(const struct jlapp *a, struct jlx_frame *f);
#endif
