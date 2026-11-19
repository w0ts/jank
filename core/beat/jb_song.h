/* SPDX-License-Identifier: GPL-3.0-only */
/* NEW SONG (an option, off by default; the user, 2026-11-19: "a different song with no tempo" should stop the outputs
 * while a drop of the same song holds them): the harmony of the last seconds against the song's.
 *
 * The tracker's own 81 log bands (jb_beat's frame tap: no second FFT), the bands centred 400 Hz-4 kHz (one a semitone
 * there) folded into 12 pitch classes, each frame's mean removed; averaged per JS_BLOCK frames (1 s) into a ring of
 * JS_BLOCKS blocks. Each block: the last JS_RECENT blocks' mean against the mean of the JS_REF before them (JS_GAP
 * blocks earlier), distance = 1 - their correlation; NEW when it reaches JS_TH and the reference holds at least
 * JS_MIN_REF blocks. jb_lost.h stops on it only while the grid's lines have no evidence (a beat-less passage).
 *
 * Measured offline (the user's tracks, a 0.1 s sliding form of this: recent 6 s, reference 90 s, gap 2 s, 1 - r >= 0.8):
 * 80 synthetic cuts from one track to another (12 of them to a beat-less intro) and 411 drops of the songs themselves:
 * the cut seen within 8 s in 57 % of the beat-less intros, but 14 % of the drops trip it too (a breakdown on new chords).
 * Spectral shape alone (no folding) does not tell the two apart (39 % against 34 %): hence an option.
 *
 * Cost: per frame ~40 adds; per block ~100 multiply-adds over the ring (12 x 100 floats). RAM: struct jb_song ~5 KB
 * (the caller's: PSRAM on the device). Portable C, no allocation. */
#ifndef JB_SONG_H
#define JB_SONG_H
#include <stdint.h>

#define JS_BANDS 81
#define JS_BLOCK 100                   /* frames a block (1 s) */
#define JS_BLOCKS 100                  /* blocks kept */
#define JS_RECENT 6
#define JS_GAP 2
#define JS_REF 90
#define JS_MIN_REF 20
#define JS_TH 0.8

struct jb_song {
    int8_t pc[JS_BANDS];               /* each band's pitch class, -1: not used */
    float acc[12];                     /* the block being filled */
    uint16_t n_acc;
    uint16_t n_blocks, head;
    uint8_t is_new;                    /* the last block's decision */
    float last_dist;
    uint32_t news;                     /* blocks decided NEW since the init (for `beat`) */
    float blocks[JS_BLOCKS][12];       /* oldest overwritten */
};

/* the bands' pitch classes from beat_model.c's filterbank (a band's centre: its bins' middle, 2048-point FFT) */
void js_init(struct jb_song *s);
/* new music (silence, a restart): forget the song */
void js_reset(struct jb_song *s);
/* one frame's 81 log bands (jb_frame.bands) */
void js_frame(struct jb_song *s, const float *bands);
/* the last decision: 1 when the last seconds' harmony is not the song's */
static inline int js_new(const struct jb_song *s) { return s->is_new; }

#endif
