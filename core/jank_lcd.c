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

static const uint8_t VU_RGB[][3] = {
#define X(r, g, b) {r, g, b},
    JLC_VU_RAMP(X)
#undef X
};

_Static_assert(sizeof INK_COLOUR == JO_INK_VU0, "one palette colour for each ink id below the VU ramp");
_Static_assert(sizeof VU_RGB / sizeof VU_RGB[0] == JO_VU_STEPS, "one ramp colour for each VU step");
_Static_assert(JLC_W == JO_LCD2_W && JLC_H == JO_LCD2_H, "the frame fills the panel exactly");

uint16_t jlc_rgb565(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)((r & 0xF8u) << 8 | (g & 0xFCu) << 3 | b >> 3); }

static uint16_t swapped(uint16_t v, int swap) { return swap ? (uint16_t)(v << 8 | v >> 8) : v; }

static uint16_t colour565(enum colour c, int swap) { return swapped(jlc_rgb565(RGB[c][0], RGB[c][1], RGB[c][2]), swap); }

void jlc_palette(struct jlc_pal *pal, int swap_bytes)
{
    pal->bg = colour565(COLOUR_bg, swap_bytes);
    for (size_t i = 0; i < JO_INK_VU0; i++)
        pal->ink[i] = colour565(INK_COLOUR[i], swap_bytes);
    for (size_t i = 0; i < JO_VU_STEPS; i++)
        pal->ink[JO_INK_VU0 + i] = swapped(jlc_rgb565(VU_RGB[i][0], VU_RGB[i][1], VU_RGB[i][2]), swap_bytes);
}

void jlc_row(const uint8_t fb[JO_LCD2_FB_BYTES], const uint8_t *ink, const struct jlc_pal *pal, unsigned y, uint16_t row[JLC_W])
{
    unsigned bit = y % 8u;
    const uint8_t *page = fb + (y / 8u) * JO_LCD2_W;
    for (unsigned x = 0; x < JO_LCD2_W; x++) {
        uint16_t v = pal->bg;
        if (page[x] >> bit & 1u) {
            unsigned id = ink ? ink[y * JO_LCD2_W + x] : 0u;
            v = pal->ink[id < JO_INK_IDS ? id : 0u];
        }
        row[x] = v;
    }
}

unsigned jlc_page_y(unsigned page) { return page * 8u; }

unsigned jlc_page_rows(void) { return 8u; }
