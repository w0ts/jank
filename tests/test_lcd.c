/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the lcd2 frame + ink plane + flash -> 320 x 240 RGB565 rows and the palette (core/jank_lcd.c). Exit status: the number of
 * failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lcd.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

/* RGB565 of the palette, worked out by hand from the RGB values (5 / 6 / 5 high bits), not by jlc_rgb565 */
#define BG 0x0841u
#define NEUTRAL 0xEF7Eu
#define GOOD 0x4EF0u
#define WARN 0xFD84u
#define BAD 0xFB4Bu
#define DIM 0x94F5u

static uint8_t fb[JO_LCD2_FB_BYTES], ink[JO_LCD2_INK_BYTES];
static uint16_t row[JLC_W];
static struct jlc_pal pal;

static void clear(void)
{
    memset(fb, 0, sizeof fb);
    memset(ink, 0, sizeof ink);
}

static void set_px(unsigned x, unsigned y, uint8_t id)
{
    fb[(y / 8u) * JO_LCD2_W + x] |= (uint8_t)(1u << (y % 8u));
    ink[y * JO_LCD2_W + x] = id;
}

static unsigned count(uint16_t v)
{
    unsigned n = 0;
    for (unsigned x = 0; x < JLC_W; x++)
        n += row[x] == v;
    return n;
}

static unsigned lit(void) { return JLC_W - count(BG); }

static void test_geometry(void)
{
    CHECK(JLC_W == 320 && JLC_H == 240 && JLC_SCALE == 2 && JO_LCD2_W * JLC_SCALE == JLC_W && JO_LCD2_H * JLC_SCALE == JLC_H);
    CHECK(JO_LCD2_FB_BYTES == 2400 && JO_LCD2_INK_BYTES == 19200 && JLC_PAGES == 15);
    CHECK(jlc_page_y(0) == 0 && jlc_page_y(14) == 224 && jlc_page_rows() == 16 && jlc_page_y(15) == JLC_H);
}

static void test_palette_values(void)
{
    struct jlc_pal swapped;
    jlc_palette(&pal, 0);
    jlc_palette(&swapped, 1);
    CHECK(pal.bg == BG && pal.ink[JO_INK_NONE] == NEUTRAL && pal.ink[JO_INK_GOOD] == GOOD && pal.ink[JO_INK_WARN] == WARN);
    CHECK(pal.ink[JO_INK_BAD] == BAD && pal.ink[JO_INK_DIM] == DIM && pal.ink[JO_INK_ONE] == WARN);
    CHECK(swapped.bg == 0x4108u && swapped.ink[JO_INK_NONE] == 0x7EEFu && swapped.ink[JO_INK_GOOD] == 0xF04Eu && swapped.ink[JO_INK_BAD] == 0x4BFBu);
    CHECK(jlc_rgb565(255, 255, 255) == 0xFFFFu && jlc_rgb565(0, 0, 0) == 0 && jlc_rgb565(255, 0, 0) == 0xF800u && jlc_rgb565(0, 255, 0) == 0x07E0u &&
          jlc_rgb565(0, 0, 255) == 0x001Fu);
    CHECK(pal.bg != pal.ink[JO_INK_NONE] && pal.ink[JO_INK_GOOD] != pal.ink[JO_INK_WARN] && pal.ink[JO_INK_WARN] != pal.ink[JO_INK_BAD]);
}

static void test_blank_frame_is_background_on_every_row(void)
{
    clear();
    for (unsigned y = 0; y < JLC_H; y++) {
        memset(row, 0xAA, sizeof row);
        jlc_row(fb, ink, &pal, y, row);
        CHECK(lit() == 0 && row[0] == BG && row[JLC_W - 1] == BG);
    }
}

static void test_full_frame_covers_the_whole_panel(void)
{
    memset(fb, 0xFF, sizeof fb);
    memset(ink, 0, sizeof ink);
    for (unsigned y = 0; y < JLC_H; y++) {
        jlc_row(fb, ink, &pal, y, row);
        CHECK(count(NEUTRAL) == JLC_W);              /* no bands, no centring */
    }
}

static void test_one_pixel_is_a_square(void)
{
    clear();
    set_px(5, 10, JO_INK_GOOD);          /* frame (5, 10) -> columns 10, 11; rows 20, 21 */
    for (unsigned y = 0; y < JLC_H; y++) {
        jlc_row(fb, ink, &pal, y, row);
        if (y == 20 || y == 21) {
            CHECK(lit() == 2 && row[10] == GOOD && row[11] == GOOD && row[9] == BG && row[12] == BG);
        } else {
            CHECK(lit() == 0);
        }
    }
}

static void test_corners_and_edges(void)
{
    clear();
    set_px(0, 0, 0);
    set_px(159, 0, 0);
    set_px(0, 119, 0);
    set_px(159, 119, 0);
    for (unsigned y = 0; y < JLC_H; y++) {
        jlc_row(fb, ink, &pal, y, row);
        if (y < 2 || y >= 238) {
            CHECK(lit() == 4 && row[0] == NEUTRAL && row[1] == NEUTRAL && row[318] == NEUTRAL && row[319] == NEUTRAL);
        } else {
            CHECK(lit() == 0);
        }
    }
}

static void test_page_layout_and_bit_order(void)
{
    clear();
    fb[3 * JO_LCD2_W + 7] = 0x81;        /* page 3: frame rows 24 and 31 at x = 7 -> rows 48, 49 and 62, 63; columns 14, 15 */
    for (unsigned y = 0; y < JLC_H; y++) {
        jlc_row(fb, NULL, &pal, y, row);
        int on = y == 48 || y == 49 || y == 62 || y == 63;
        CHECK(lit() == (on ? 2u : 0u) && (!on || (row[14] == NEUTRAL && row[15] == NEUTRAL)));
    }
    CHECK(jlc_page_y(3) == 48 && jlc_page_y(3) + jlc_page_rows() - 1 == 63);
}

/* the ink plane is indexed y * 160 + x, not by the page layout */
static void test_ink_picks_the_pixels_own_id(void)
{
    static const struct {
        uint8_t id;
        uint16_t want;
    } T[] = {{JO_INK_NONE, NEUTRAL}, {JO_INK_GOOD, GOOD}, {JO_INK_WARN, WARN}, {JO_INK_BAD, BAD}, {JO_INK_DIM, DIM}, {JO_INK_ONE, WARN}};
    clear();
    for (unsigned i = 0; i < 6; i++)
        set_px(10 + 3 * i, 31, T[i].id);    /* page 3, bit 7: frame row 31 -> rows 62, 63 */
    for (unsigned y = 62; y <= 63; y++) {
        jlc_row(fb, ink, &pal, y, row);
        CHECK(lit() == 12);
        for (unsigned i = 0; i < 6; i++) {
            unsigned x = (10 + 3 * i) * 2;
            CHECK(row[x] == T[i].want && row[x + 1] == T[i].want && row[x - 1] == BG && row[x + 2] == BG);
        }
    }
    set_px(40, 31, 6);                    /* an id past the table */
    set_px(41, 31, 255);
    jlc_row(fb, ink, &pal, 62, row);
    CHECK(row[80] == NEUTRAL && row[82] == NEUTRAL);
}

static void test_ink_on_unlit_pixels_is_ignored_and_no_plane_is_neutral(void)
{
    clear();
    ink[10 * JO_LCD2_W + 5] = JO_INK_BAD;      /* an id on a pixel that is not lit: the background */
    set_px(6, 10, JO_INK_GOOD);
    jlc_row(fb, ink, &pal, 20, row);
    CHECK(lit() == 2 && row[12] == GOOD && row[13] == GOOD && row[10] == BG && row[11] == BG);
    jlc_row(fb, NULL, &pal, 20, row);
    CHECK(lit() == 2 && row[12] == NEUTRAL && row[13] == NEUTRAL && row[10] == BG);
}

static void test_swapped_palette_row(void)
{
    struct jlc_pal swapped;
    jlc_palette(&swapped, 1);
    clear();
    set_px(0, 0, JO_INK_WARN);
    jlc_row(fb, ink, &swapped, 0, row);
    CHECK(row[0] == 0x84FDu && row[2] == 0x4108u && row[319] == 0x4108u);
}

int main(void)
{
    jlc_palette(&pal, 0);
    test_geometry();
    test_palette_values();
    test_blank_frame_is_background_on_every_row();
    test_full_frame_covers_the_whole_panel();
    test_one_pixel_is_a_square();
    test_corners_and_edges();
    test_page_layout_and_bit_order();
    test_ink_picks_the_pixels_own_id();
    test_ink_on_unlit_pixels_is_ignored_and_no_plane_is_neutral();
    test_swapped_palette_row();
    printf("lcd: %d checks, %d failed\n", checks, fails);
    return fails;
}
