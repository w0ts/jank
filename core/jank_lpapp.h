/* SPDX-License-Identifier: GPL-3.0-only */
/* Launchpad X -> Digitone II, Note mode, on stock MIDI (no change to the DN2): what a pad press sends, what the buttons do, and
 * what the pads show. Pure: it takes decoded Launchpad events and the DN2's USB-MIDI packets, and gives back USB-MIDI packets
 * and an LED frame (jank_lpx.h); the USB side is firmware/main. Host-tested (tests/test_lpapp.c).
 *
 * Buttons (Note mode, decided with the owner 2026-11-23): up / down = octave, left / right = the previous / next track (one at
 * a time, no wrap); the Note button is lit. A pad plays its note on the MIDI channel of the current track; the pad stays
 * lit while the DN2 sounds that note on that channel (a sequencer note or one of ours), and every pad with the same pitch
 * lights (the layouts overlap). The channel of each track is a table (default: track n on channel n). */
#ifndef JANK_LPAPP_H
#define JANK_LPAPP_H
#include <stddef.h>
#include <stdint.h>
#include "jank_lpnote.h"
#include "jank_lpx.h"

#define JLAPP_CHANNELS 16u
#define JLAPP_PADS 64u

/* the arrow buttons' and the mode button's palette indexes */
#define JLAPP_COLOR_ARROW 1u             /* dim grey */
#define JLAPP_COLOR_MODE 3u              /* white */

/* the hosted devices that matter here, told apart by their USB product string */
enum jlapp_role { JLAPP_OTHER, JLAPP_LPX, JLAPP_DN2 };
enum jlapp_role jlapp_role_from_name(const char *product);
/* which of a device's USB-MIDI streaming interfaces is the one to use (0 = the first): the Launchpad X's first is its DAW
 * interface, its MIDI one (Programmer mode, pads, lights) is the second */
unsigned jlapp_midi_interface_index(enum jlapp_role r);

struct jlapp_held {
    int16_t note;                      /* -1: the pad is up */
    uint8_t chan;                      /* the channel it was sent on: its Note Off goes there, whatever the track is by then */
};

struct jlapp {
    struct jln n;
    const struct jln_scale *scale;
    uint8_t chan[JLN_TRACKS];          /* each track's MIDI channel, 0..15 */
    uint8_t dn2_cable;                 /* the cable number in the packets to the DN2 */
    struct jlapp_held held[JLAPP_PADS];
    uint8_t playing[JLAPP_CHANNELS][16]; /* from the DN2: a bit a note, per channel */
};

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
