/* SPDX-License-Identifier: GPL-3.0-only */
/* The lcd2 layout's frame (jank_oled.h, jo_draw_lcd2: 160 x 120, 1 bit per pixel, and its ink plane) on the 320 x 240 panel as RGB565 rows:
 * exactly x2, every source pixel a square of 2 x 2, no centring, no bands. Unlit = the background, lit = the colour of its ink id (neutral
 * where there is no ink). A frame can carry a FLASH (the BEAT FLASH setting): the field turns to the flash colour and every lit pixel to
 * near-black (the background colour), whatever its ink. */
#ifndef JANK_LCD_H
#define JANK_LCD_H
#include <stdint.h>
#include "jank_oled.h"

#define JLC_W 320
#define JLC_H 240
#define JLC_SCALE 2
#define JLC_PAGES (JO_LCD2_H / 8)

/* The palette, exact 8-bit RGB: ui/lcd2.js's PALETTE and FLASH_FIELD (tests/test_lcd_palette.js reads these tables and compares them with it).
 * Colour only where it informs: neutral is every lit pixel without a meaning. */
#define JLC_PALETTE(X) X(bg, 8, 10, 14) X(neutral, 232, 236, 244) X(good, 72, 222, 128) X(warn, 255, 176, 32) X(bad, 255, 104, 92) X(dim, 150, 156, 168)
/* the colour of each ink id, in the order of enum jo_ink (ui/lcd2.js INK_PALETTE) */
#define JLC_INK_MAP(X) X(neutral) X(good) X(warn) X(bad) X(dim) X(warn)
/* the field of a flash (ui/lcd2.js FLASH_FIELD): an ordinary beat, and the bar's first beat; the lit pixels of a flash are JLC_PALETTE's bg */
#define JLC_FLASH_FIELD(X) X(beat, 214, 255, 226) X(bar, 255, 176, 32)

enum jlc_flash { JLC_FLASH_NONE = 0, JLC_FLASH_BEAT = 1, JLC_FLASH_BAR = 2 };      /* the numbers of core/jank_core.h enum jk_flash */

/* the palette as the panel takes it: RGB565 for the background, each ink id and each flash field. swap_bytes: the ST7789 wants the high
 * byte first, a little-endian CPU writes the low one: swapped once here, not per pixel. */
struct jlc_pal {
    uint16_t bg;
    uint16_t ink[JO_INK_IDS];
    uint16_t field[3];                    /* by enum jlc_flash; [0] is bg */
};
void jlc_palette(struct jlc_pal *pal, int swap_bytes);
uint16_t jlc_rgb565(uint8_t r, uint8_t g, uint8_t b);

/* destination row y (0 .. JLC_H - 1) -> row[JLC_W]. ink NULL: no plane, every lit pixel neutral; an id past the table: neutral */
void jlc_row(const uint8_t fb[JO_LCD2_FB_BYTES], const uint8_t *ink, const struct jlc_pal *pal, int flash, unsigned y, uint16_t row[JLC_W]);

/* the destination rows that frame page p (8 frame rows) covers: first row and count */
unsigned jlc_page_y(unsigned page);
unsigned jlc_page_rows(void);
#endif
