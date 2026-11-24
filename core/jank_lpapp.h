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
 * LP95's other pads (its two guitar / not-staggered layouts, the drum rack, quick scale) have no job here. */
#ifndef JANK_LPAPP_H
#define JANK_LPAPP_H
#include <stddef.h>
#include <stdint.h>
#include "jank_lpnote.h"
#include "jank_lpx.h"

/* the scale page's palette indexes (manual p. 12) */
#define JLAPP_PAGE_ON 21u                /* green: what is selected */
#define JLAPP_PAGE_LAYOUT 37u            /* cyan: a layout or toggle that is off */
#define JLAPP_PAGE_WHITE_KEY 3u          /* white */
#define JLAPP_PAGE_BLACK_KEY 1u          /* dim grey */
#define JLAPP_PAGE_OCTAVE 13u            /* yellow */
#define JLAPP_PAGE_SCALE 45u             /* blue */
#define JLAPP_PAGE_SHIFT 9u              /* orange: the relative scale and the circle of fifths */

#define JLAPP_CHANNELS 16u
#define JLAPP_PADS 64u

/* the arrow buttons' and the mode button's palette indexes */
#define JLAPP_COLOR_ARROW 1u             /* dim grey */
#define JLAPP_COLOR_MODE 3u              /* white */

/* the hosted devices that matter here, told apart by their USB product string */
enum jlapp_role { JLAPP_OTHER, JLAPP_LPX, JLAPP_DN2 };
enum jlapp_role jlapp_role_from_name(const char *product);
/* which of a device's USB-MIDI streaming interfaces is the one to use (0 = the first). The Launchpad X has only one (diag ifs:1: "DAW" and "MIDI" are its two
 * CABLES, not two interfaces), so 1 is clamped to that one; the cable matters, not the interface: usb_lp.c sends on cable 1, the MIDI port */
unsigned jlapp_midi_interface_index(enum jlapp_role r);

struct jlapp_held {
    int16_t note;                      /* -1: the pad is up */
    uint8_t chan;                      /* the channel it was sent on: its Note Off goes there, whatever the track is by then */
};

struct jlapp {
    struct jln n;
    const struct jln_scale *scale;
    uint8_t page;                      /* the Note button is held: the pads are the scale page */
    uint8_t minor_idx;                 /* the minor scale the relative pad returns to (jank_lpscales.h index) */
    uint8_t chan[JLN_TRACKS];          /* each track's MIDI channel, 0..15 */
    uint8_t dn2_cable;                 /* the cable number in the packets to the DN2 */
    struct jlapp_held held[JLAPP_PADS];
    uint8_t playing[JLAPP_CHANNELS][16]; /* from the DN2: a bit a note, per channel */
};

/* `scale` is one of jank_lpscales.h's (the page marks it) or any other */
void jlapp_init(struct jlapp *a, const struct jln_scale *scale, uint8_t dn2_cable);

/* a Launchpad pad or button; the packets for the DN2 go to out (at most max_packets, one per event); returns how many */
size_t jlapp_on_lpx(struct jlapp *a, const struct jlx_ev *ev, uint8_t *out, size_t max_packets);

/* one 4-byte USB-MIDI packet from the DN2: Note On / Off and All Notes Off, the rest is ignored */
void jlapp_on_dn2(struct jlapp *a, const uint8_t pkt[4]);

/* a Note Off for every pad that is down (the Launchpad went away): packets for the DN2, at most max_packets; returns how many */
size_t jlapp_release_all(struct jlapp *a, uint8_t *out, size_t max_packets);

/* what the Launchpad should show now */
void jlapp_frame(const struct jlapp *a, struct jlx_frame *f);
#endif
