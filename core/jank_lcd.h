/* SPDX-License-Identifier: GPL-3.0-only */
/* The lcd2 layout's frame (jank_oled.h, jo_draw_lcd2: 320 x 240, 1 bit per pixel, and its ink plane) on the 320 x 240 panel as RGB565 rows:
 * one frame pixel per panel pixel, no centring, no bands. Unlit = the background, lit = the colour of its ink id (neutral
 * where there is no ink). */
#ifndef JANK_LCD_H
#define JANK_LCD_H
#include <stdint.h>
#include "jank_oled.h"

#define JLC_W 320
#define JLC_H 240
#define JLC_PAGES (JO_LCD2_H / 8)

/* The palette, exact 8-bit RGB: ui/lcd2.js's PALETTE and VU_RAMP (tests/test_lcd_palette.js reads these tables and compares them with it).
 * Colour only where it informs: neutral is every lit pixel without a meaning. */
#define JLC_PALETTE(X) X(bg, 8, 10, 14) X(neutral, 232, 236, 244) X(good, 72, 222, 128) X(warn, 255, 176, 32) X(bad, 255, 104, 92) X(dim, 150, 156, 168)
/* the colour of each ink id below JO_INK_VU0, in the order of enum jo_ink (ui/lcd2.js INK_PALETTE) */
#define JLC_INK_MAP(X) X(neutral) X(good) X(warn) X(bad) X(dim) X(warn)
/* the VU meter's ramp: the colour of ink id JO_INK_VU0 + i (ui/lcd2.js VU_RAMP) */
#define JLC_VU_RAMP(X) \
    X(72, 222, 128) X(72, 222, 128) X(72, 222, 128) X(72, 222, 128) \
    X(72, 222, 128) X(72, 222, 128) X(72, 222, 128) X(72, 222, 128) \
    X(72, 222, 128) X(72, 222, 128) X(72, 222, 128) X(82, 223, 122) \
    X(93, 224, 115) X(103, 224, 109) X(114, 225, 102) X(124, 226, 96) \
    X(134, 227, 90) X(145, 228, 83) X(155, 228, 77) X(166, 229, 70) \
    X(176, 230, 64) X(189, 221, 59) X(202, 212, 53) X(216, 203, 48) \
    X(229, 194, 43) X(242, 185, 37) X(255, 176, 32) X(255, 160, 43) \
    X(255, 144, 54) X(255, 128, 66) X(255, 112, 77) X(255, 96, 88)

/* the palette as the panel takes it: RGB565 for the background and each ink id. swap_bytes: the ST7789 wants the high
 * byte first, a little-endian CPU writes the low one: swapped once here, not per pixel. */
struct jlc_pal {
    uint16_t bg;
    uint16_t ink[JO_INK_IDS];
};
void jlc_palette(struct jlc_pal *pal, int swap_bytes);
uint16_t jlc_rgb565(uint8_t r, uint8_t g, uint8_t b);

/* destination row y (0 .. JLC_H - 1) -> row[JLC_W]. ink NULL: no plane, every lit pixel neutral; an id past the table: neutral */
void jlc_row(const uint8_t fb[JO_LCD2_FB_BYTES], const uint8_t *ink, const struct jlc_pal *pal, unsigned y, uint16_t row[JLC_W]);

/* the destination rows that frame page p (8 frame rows) covers: first row and count */
unsigned jlc_page_y(unsigned page);
unsigned jlc_page_rows(void);
#endif
