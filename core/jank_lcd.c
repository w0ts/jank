/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_lcd.h"
#include <stddef.h>

enum colour {
#define X(name, r, g, b) COLOUR_##name,
    JLC_PALETTE(X)
#undef X
    COLOURS
};

static const uint8_t RGB[COLOURS][3] = {
#define X(name, r, g, b) {r, g, b},
    JLC_PALETTE(X)
#undef X
};

static const uint8_t INK_COLOUR[] = {
#define X(name) COLOUR_##name,
    JLC_INK_MAP(X)
#undef X
};

_Static_assert(sizeof INK_COLOUR == JO_INK_IDS, "one palette colour for each ink id");

uint16_t jlc_rgb565(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)((r & 0xF8u) << 8 | (g & 0xFCu) << 3 | b >> 3); }

static uint16_t colour565(enum colour c, int swap)
{
    uint16_t v = jlc_rgb565(RGB[c][0], RGB[c][1], RGB[c][2]);
    return swap ? (uint16_t)(v << 8 | v >> 8) : v;
}

void jlc_palette(struct jlc_pal *pal, int swap_bytes)
{
    pal->bg = colour565(COLOUR_bg, swap_bytes);
    for (size_t i = 0; i < JO_INK_IDS; i++)
        pal->ink[i] = colour565(INK_COLOUR[i], swap_bytes);
}

static int in_picture(unsigned y) { return y >= JLC_Y0 && y < JLC_Y0 + JO_H * JLC_SCALE; }

void jlc_row(const uint8_t fb[JO_FB_BYTES], const uint8_t *ink, const struct jlc_pal *pal, unsigned y, uint16_t row[JLC_W])
{
    for (unsigned x = 0; x < JLC_W; x++)
        row[x] = pal->bg;
    if (!in_picture(y))
        return;
    unsigned fy = (y - JLC_Y0) / JLC_SCALE;
    const uint8_t *page = fb + (fy / 8u) * JO_W;
    unsigned bit = fy % 8u;
    for (unsigned x = 0; x < JO_W; x++) {
        if (!(page[x] >> bit & 1u))
            continue;
        unsigned id = ink ? ink[fy * JO_W + x] : 0u;
        uint16_t v = pal->ink[id < JO_INK_IDS ? id : 0u];
        for (unsigned k = 0; k < JLC_SCALE; k++)
            row[JLC_X0 + x * JLC_SCALE + k] = v;
    }
}

unsigned jlc_page_y(unsigned page) { return JLC_Y0 + page * 8u * JLC_SCALE; }

unsigned jlc_page_rows(void) { return 8u * JLC_SCALE; }
