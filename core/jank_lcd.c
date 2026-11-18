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

static const uint8_t FIELD[][3] = {
#define X(name, r, g, b) {r, g, b},
    JLC_FLASH_FIELD(X)
#undef X
};

_Static_assert(sizeof INK_COLOUR == JO_INK_IDS, "one palette colour for each ink id");
_Static_assert(sizeof FIELD / sizeof FIELD[0] == 2, "a field for the beat and for the bar's first beat");
_Static_assert(JLC_W == JO_LCD2_W * JLC_SCALE && JLC_H == JO_LCD2_H * JLC_SCALE, "the frame fills the panel exactly");

uint16_t jlc_rgb565(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)((r & 0xF8u) << 8 | (g & 0xFCu) << 3 | b >> 3); }

static uint16_t swapped(uint16_t v, int swap) { return swap ? (uint16_t)(v << 8 | v >> 8) : v; }

static uint16_t colour565(enum colour c, int swap) { return swapped(jlc_rgb565(RGB[c][0], RGB[c][1], RGB[c][2]), swap); }

void jlc_palette(struct jlc_pal *pal, int swap_bytes)
{
    pal->bg = colour565(COLOUR_bg, swap_bytes);
    for (size_t i = 0; i < JO_INK_IDS; i++)
        pal->ink[i] = colour565(INK_COLOUR[i], swap_bytes);
    pal->field[JLC_FLASH_NONE] = pal->bg;
    for (size_t i = 0; i < sizeof FIELD / sizeof FIELD[0]; i++)
        pal->field[i + 1] = swapped(jlc_rgb565(FIELD[i][0], FIELD[i][1], FIELD[i][2]), swap_bytes);
}

void jlc_row(const uint8_t fb[JO_LCD2_FB_BYTES], const uint8_t *ink, const struct jlc_pal *pal, int flash, unsigned y, uint16_t row[JLC_W])
{
    unsigned fy = y / JLC_SCALE, bit = fy % 8u;
    const uint8_t *page = fb + (fy / 8u) * JO_LCD2_W;
    int flashing = flash == JLC_FLASH_BEAT || flash == JLC_FLASH_BAR;
    uint16_t field = pal->field[flashing ? flash : JLC_FLASH_NONE];
    for (unsigned x = 0; x < JO_LCD2_W; x++) {
        uint16_t v = field;
        if (page[x] >> bit & 1u) {
            unsigned id = ink ? ink[fy * JO_LCD2_W + x] : 0u;
            v = flashing ? pal->bg : pal->ink[id < JO_INK_IDS ? id : 0u];
        }
        row[JLC_SCALE * x] = v;
        row[JLC_SCALE * x + 1] = v;
    }
}

unsigned jlc_page_y(unsigned page) { return page * 8u * JLC_SCALE; }

unsigned jlc_page_rows(void) { return 8u * JLC_SCALE; }
