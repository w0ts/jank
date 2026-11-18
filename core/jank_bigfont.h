/* SPDX-License-Identifier: GPL-3.0-only */
/* The big tempo digits of the lcd2 layout (ui/bigfont.js): two fonts of one face, NORMAL for AUDIO and LARGE for LINK, as const tables
 * (core/jank_bigfont_tables.c, generated). A text is digits, '.' and '-'; the digit right after a '.' is the small one, bottom-aligned
 * with the big digits. */
#ifndef JANK_BIGFONT_H
#define JANK_BIGFONT_H
#include <stdint.h>

struct jbf_font {
    uint8_t rows, small_rows, gap;        /* big glyph height, small digit height, columns between glyphs */
    uint8_t width, dot_width, small_width;
    const uint8_t *big;                   /* '0'..'9', '-', '.' (12 glyphs of `rows` rows); a row is ceil(width / 8) bytes, the leftmost column the highest bit of the first */
    const uint8_t *small;                 /* '0'..'9' (10 glyphs of `small_rows` rows) */
};
extern const struct jbf_font jbf_normal, jbf_large;

int jbf_width(const struct jbf_font *f, const char *s);
/* lit pixels only, the text's top left at (x, y) */
void jbf_draw(const struct jbf_font *f, uint8_t *fb, const char *s, int x, int y);
#endif
