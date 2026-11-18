/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_bigfont.h"
#include "jank_oled_int.h"

#define GLYPH_MINUS 10
#define GLYPH_DOT 11

static int glyph_of(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    return ch == '.' ? GLYPH_DOT : GLYPH_MINUS;
}

/* a digit right after the '.' is the small one; anything else the font lacks is '-' */
static int is_small(const char *s, int i) { return i > 0 && s[i - 1] == '.' && s[i] >= '0' && s[i] <= '9'; }

static int glyph_width(const struct jbf_font *f, const char *s, int i)
{
    if (is_small(s, i))
        return f->small_width;
    return s[i] == '.' ? f->dot_width : f->width;
}

int jbf_width(const struct jbf_font *f, const char *s)
{
    int w = 0;
    for (int i = 0; s[i]; i++)
        w += glyph_width(f, s, i) + f->gap;
    return w > f->gap ? w - f->gap : 0;
}

static void draw_glyph(const struct jbf_font *f, uint8_t *fb, const char *s, int i, int x, int y)
{
    int small = is_small(s, i), g = glyph_of(s[i]), w = glyph_width(f, s, i), rows = small ? f->small_rows : f->rows;
    int dy = small ? f->rows - f->small_rows : 0;
    int rb = (w + 7) / 8;
    int big_rb = (f->width + 7) / 8;                        /* every glyph but the '.' has the font's width; the '.' is the last */
    const uint8_t *base = small ? f->small + (size_t)g * (size_t)f->small_rows * (size_t)rb : f->big + (size_t)g * (size_t)f->rows * (size_t)big_rb;
    for (int r = 0; r < rows; r++) {
        const uint8_t *bits = base + (size_t)r * (size_t)rb;
        for (int c = 0; c < w; c++)
            if (bits[c >> 3] >> (7 - (c & 7)) & 1u)
                jo_px(fb, x + c, y + dy + r, 1);
    }
}

void jbf_draw(const struct jbf_font *f, uint8_t *fb, const char *s, int x, int y)
{
    for (int i = 0; s[i]; i++) {
        draw_glyph(f, fb, s, i, x, y);
        x += glyph_width(f, s, i) + f->gap;
    }
}
