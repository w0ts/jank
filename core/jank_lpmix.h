/* SPDX-License-Identifier: GPL-3.0-only */
/* The Launchpad's mixer screen on the Digitone (owner 2026-11-24), after Launchpad 95's mixer: each of the 8 pad columns is one track as a vertical fader 8 pads high
 * (column x, bottom pad = the lowest value); up / down switch the bank, tracks 1-8 / 9-16. The scene buttons (right side, top to bottom) pick the view:
 *
 *   1 volume   2 pan   3 delay send   4 reverb send   5 chorus send   6 mute   7 solo   (8: nothing yet)
 *
 * Volume and the sends fill from the bottom; pan is one pad (the centre pads marked); mute shows each track's column bright yellow, dim when it is muted; solo bright
 * blue when it is soloed, dim when not. Colours as Launchpad 95's (SkinMK2.py Mixer.*: MINT, YELLOW, BLUE). The values come from MIXER (jank_dn2live.h), so the faders
 * move when the Digitone's values do; a press writes (MIXER SET / MUTE SET, the Digitone's stage 6) and the fader moves at once. Pure; host-tested (tests/test_lpmix.c). */
#ifndef JANK_LPMIX_H
#define JANK_LPMIX_H
#include <stdint.h>
#include "jank_dn2live.h"
#include "jank_lpx.h"

#define JLMX_COLUMNS 8u
#define JLMX_BANKS 2u
#define JLMX_READ_MS 300u              /* the firmware reads the mixer this often while the screen is on (the answer is 106 bytes) */

#define JLMX_COLOR_FADER 29u           /* MINT: volume, pan and the sends; the empty part is dark */
#define JLMX_COLOR_FADER_THIRD 31u     /* MINT_THIRD: the pan's centre marks, and a scene button that is not the view */
#define JLMX_COLOR_UNMUTED 13u         /* YELLOW */
#define JLMX_COLOR_MUTED 15u           /* YELLOW_THIRD */
#define JLMX_COLOR_SOLO 45u            /* BLUE */
#define JLMX_COLOR_NOSOLO 47u          /* BLUE_THIRD */

enum jlmx_view { JLMX_VOL, JLMX_PAN, JLMX_DEL, JLMX_REV, JLMX_CHR, JLMX_MUTE, JLMX_SOLO, JLMX_VIEWS };

#define JLMX_HOLD_MS 400u              /* after a press the Digitone's older reading is not allowed to put the fader back (its write applies at its next UI pass) */

struct jlmx {
    uint8_t view;                      /* enum jlmx_view */
    uint8_t bank;                      /* 0: tracks 1-8, 1: tracks 9-16 */
    uint8_t have;                      /* a MIXER answer has come */
    uint8_t held;                      /* a press changed the local copy at hold_ms */
    uint32_t hold_ms;
    struct jdl_mixer m;
};

/* what a press asks the Digitone to write (jlmx_press) */
enum jlmx_write_kind { JLMX_WRITE_NONE, JLMX_WRITE_VALUE, JLMX_WRITE_MASK };
struct jlmx_write {
    uint8_t kind;                      /* enum jlmx_write_kind */
    uint8_t track;                     /* JLMX_WRITE_VALUE: the track, 0..15 */
    uint8_t param;                     /* ... enum jdl_mix_param */
    uint8_t value;                     /* ... 0..127 */
    uint8_t which;                     /* JLMX_WRITE_MASK: 0 mute, 1 solo */
    uint16_t mask;                     /* ... the whole new mask (bit t = track t + 1) */
};

void jlmx_init(struct jlmx *s);
/* the Digitone's answer (jdl_take_mixer); within JLMX_HOLD_MS of a press it is not taken (the fader would jump back and forth), the next one is */
void jlmx_on_mixer(struct jlmx *s, const struct jdl_mixer *m, uint32_t now_ms);
/* A pad (x 0..7, y 0..7 from the bottom) pressed: what to write, and the local copy is changed at once (the fader moves). Volume and the sends: the pad's height,
 * 8 steps, and the lowest pad again when the fader is already at that step sets 0; pan: the 8 positions (the two middle ones are the centre); mute / solo: the column's track
 * toggles. 1 and *w filled, 0 when nothing is to be written (no answer from the Digitone yet) */
int jlmx_press(struct jlmx *s, uint8_t x, uint8_t y, uint32_t now_ms, struct jlmx_write *w);
/* a scene button (index 0 = the top one): 1 and the view is picked if it is one of the views, else 0 */
int jlmx_scene(struct jlmx *s, uint8_t idx);
/* up (+1) / down (-1): the bank; 1 when it changed, 0 at the limit */
int jlmx_bank_step(struct jlmx *s, int dir);
/* the 64 pads of the screen (led[JLX_PAD0 ..]); nothing known yet: dark */
void jlmx_frame(const struct jlmx *s, struct jlx_frame *f);
/* the colour of scene button idx: the view MINT (bright when it is the one shown, dim otherwise), mute YELLOW, solo BLUE; 0 for a button with no job */
uint8_t jlmx_scene_colour(const struct jlmx *s, uint8_t idx);
#endif
