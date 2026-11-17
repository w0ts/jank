/* SPDX-License-Identifier: GPL-3.0-only */
/* The 128 x 64 1-bit frame (jank_oled.h) and its ink plane (enum jo_ink) on a landscape 320 x 240 colour panel, as RGB565 rows.
 * Each frame pixel is a JLC_SCALE x JLC_SCALE square: unlit = the background, lit = the colour of its ink id (neutral where
 * there is no ink); the 256 x 128 picture is centred and the rest is the background. Whole-number scale: every pixel stays
 * square (2.5 would make the rows alternately 2 and 3 pixels). */
#ifndef JANK_LCD_H
#define JANK_LCD_H
#include <stdint.h>
#include "jank_oled.h"

#define JLC_W 320
#define JLC_H 240
#define JLC_SCALE 2
#define JLC_X0 ((JLC_W - JO_W * JLC_SCALE) / 2)
#define JLC_Y0 ((JLC_H - JO_H * JLC_SCALE) / 2)

/* The palette, exact 8-bit RGB: ui/lcd2.js's PALETTE (tests/test_lcd_palette.js reads these two tables and compares them with it).
 * Colour only where it informs: neutral is every lit pixel without a meaning. */
#define JLC_PALETTE(X) X(bg, 8, 10, 14) X(neutral, 232, 236, 244) X(good, 72, 222, 128) X(warn, 255, 176, 32) X(bad, 255, 104, 92) X(dim, 150, 156, 168)
/* the colour of each ink id, in the order of enum jo_ink (ui/lcd2.js INK_PALETTE) */
#define JLC_INK_MAP(X) X(neutral) X(good) X(warn) X(bad) X(dim) X(warn)

/* the palette as the panel takes it: RGB565 for the background and for each ink id. swap_bytes: the ST7789 wants the high byte
 * first, a little-endian CPU writes the low one: swapped once here, not per pixel. */
struct jlc_pal {
    uint16_t bg;
    uint16_t ink[JO_INK_IDS];
};
void jlc_palette(struct jlc_pal *pal, int swap_bytes);
uint16_t jlc_rgb565(uint8_t r, uint8_t g, uint8_t b);

/* destination row y (0 .. JLC_H - 1) -> row[JLC_W]. ink NULL: no plane, every lit pixel neutral; an id past the table: neutral */
void jlc_row(const uint8_t fb[JO_FB_BYTES], const uint8_t *ink, const struct jlc_pal *pal, unsigned y, uint16_t row[JLC_W]);

/* the destination rows that frame page p (8 frame rows) covers: first row and count */
unsigned jlc_page_y(unsigned page);
unsigned jlc_page_rows(void);
#endif
