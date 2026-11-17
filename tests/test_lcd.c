/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the frame + ink plane -> landscape RGB565 rows and the palette (core/jank_lcd.c). Exit status: the number of
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

static uint8_t fb[JO_FB_BYTES], ink[JO_INK_BYTES];
static uint16_t row[JLC_W];
static struct jlc_pal pal;

static void clear(void)
{
    memset(fb, 0, sizeof fb);
    memset(ink, 0, sizeof ink);
}

static void set_px(unsigned x, unsigned y, uint8_t id)
{
    fb[(y / 8u) * JO_W + x] |= (uint8_t)(1u << (y % 8u));
    ink[y * JO_W + x] = id;
}

static unsigned lit(void)
{
    unsigned n = 0;
    for (unsigned x = 0; x < JLC_W; x++)
        n += row[x] != BG;
    return n;
}

static void test_geometry(void)
{
    CHECK(JLC_W == 320 && JLC_H == 240 && JLC_SCALE == 2);
    CHECK(JLC_X0 == 32 && JLC_Y0 == 56);
    CHECK(JLC_X0 * 2 + JO_W * JLC_SCALE == JLC_W && JLC_Y0 * 2 + JO_H * JLC_SCALE == JLC_H);
    CHECK(jlc_page_y(0) == 56 && jlc_page_y(7) == 56 + 7 * 16 && jlc_page_rows() == 16);
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
        CHECK(lit() == 0);
        CHECK(row[0] == BG && row[JLC_W - 1] == BG);
    }
}

static void test_full_frame_lights_exactly_the_picture(void)
{
    memset(fb, 0xFF, sizeof fb);
    memset(ink, 0, sizeof ink);
    for (unsigned y = 0; y < JLC_H; y++) {
        jlc_row(fb, ink, &pal, y, row);
        int inside = y >= 56 && y < 184;
        CHECK(lit() == (inside ? 256u : 0u));
        if (inside)
            CHECK(row[31] == BG && row[32] == NEUTRAL && row[287] == NEUTRAL && row[288] == BG);
    }
}

static void test_one_pixel_is_a_square(void)
{
    clear();
    set_px(5, 10, JO_INK_GOOD);          /* frame (5, 10) -> columns 42, 43; rows 76, 77 */
    for (unsigned y = 0; y < JLC_H; y++) {
        jlc_row(fb, ink, &pal, y, row);
        if (y == 76 || y == 77) {
            CHECK(lit() == 2 && row[42] == GOOD && row[43] == GOOD && row[41] == BG && row[44] == BG);
        } else {
            CHECK(lit() == 0);
        }
    }
}

static void test_corners_and_edges(void)
{
    clear();
    set_px(0, 0, 0);
    set_px(127, 0, 0);
    set_px(0, 63, 0);
    set_px(127, 63, 0);
    jlc_row(fb, ink, &pal, 56, row);
    CHECK(lit() == 4 && row[32] == NEUTRAL && row[33] == NEUTRAL && row[286] == NEUTRAL && row[287] == NEUTRAL);
    jlc_row(fb, ink, &pal, 57, row);
    CHECK(lit() == 4);
    jlc_row(fb, ink, &pal, 58, row);
    CHECK(lit() == 0);
    jlc_row(fb, ink, &pal, 182, row);
    CHECK(lit() == 4 && row[32] == NEUTRAL && row[287] == NEUTRAL);
    jlc_row(fb, ink, &pal, 183, row);
    CHECK(lit() == 4);
    jlc_row(fb, ink, &pal, 184, row);
    CHECK(lit() == 0);
    jlc_row(fb, ink, &pal, 55, row);
    CHECK(lit() == 0);
}

static void test_page_layout_and_bit_order(void)
{
    clear();
    fb[3 * JO_W + 7] = 0x81;             /* page 3: frame rows 24 and 31 at x = 7 */
    jlc_row(fb, NULL, &pal, jlc_page_y(3), row);
    CHECK(lit() == 2 && row[JLC_X0 + 14] == NEUTRAL);
    jlc_row(fb, NULL, &pal, jlc_page_y(3) + jlc_page_rows() - 1, row);
    CHECK(lit() == 2);
    jlc_row(fb, NULL, &pal, jlc_page_y(3) + 2, row);
    CHECK(lit() == 0);
    jlc_row(fb, NULL, &pal, jlc_page_y(3) + 14, row);
    CHECK(lit() == 2);
}

/* the ink plane is indexed y * 128 + x, not by the page layout: a pixel on page 3, bit 7 reads its own byte of the plane */
static void test_ink_picks_the_pixels_own_id(void)
{
    static const struct {
        uint8_t id;
        uint16_t want;
    } T[] = {{JO_INK_NONE, NEUTRAL}, {JO_INK_GOOD, GOOD}, {JO_INK_WARN, WARN}, {JO_INK_BAD, BAD}, {JO_INK_DIM, DIM}, {JO_INK_ONE, WARN}};
    clear();
    for (unsigned i = 0; i < 6; i++)
        set_px(10 + 3 * i, 31, T[i].id);    /* page 3, bit 7: frame row 31 -> rows 118, 119 */
    for (unsigned y = 118; y <= 119; y++) {
        jlc_row(fb, ink, &pal, y, row);
        CHECK(lit() == 12);
        for (unsigned i = 0; i < 6; i++) {
            unsigned x = JLC_X0 + (10 + 3 * i) * 2;
            CHECK(row[x] == T[i].want && row[x + 1] == T[i].want && row[x - 1] == BG && row[x + 2] == BG);
        }
    }
    set_px(40, 31, 6);                    /* an id past the table */
    set_px(41, 31, 255);
    jlc_row(fb, ink, &pal, 118, row);
    CHECK(row[JLC_X0 + 80] == NEUTRAL && row[JLC_X0 + 82] == NEUTRAL);
}

static void test_ink_on_unlit_pixels_is_ignored_and_no_plane_is_neutral(void)
{
    clear();
    ink[10 * JO_W + 5] = JO_INK_BAD;      /* an id on a pixel that is not lit: the background */
    set_px(6, 10, JO_INK_GOOD);
    jlc_row(fb, ink, &pal, 76, row);
    CHECK(lit() == 2 && row[44] == GOOD && row[45] == GOOD && row[42] == BG && row[43] == BG);
    jlc_row(fb, NULL, &pal, 76, row);
    CHECK(lit() == 2 && row[44] == NEUTRAL && row[45] == NEUTRAL && row[42] == BG);
}

static void test_swapped_palette_row(void)
{
    struct jlc_pal swapped;
    jlc_palette(&swapped, 1);
    clear();
    set_px(0, 0, JO_INK_WARN);
    jlc_row(fb, ink, &swapped, 56, row);
    CHECK(row[32] == 0x84FDu && row[34] == 0x4108u && row[0] == 0x4108u);
}

int main(void)
{
    jlc_palette(&pal, 0);
    test_geometry();
    test_palette_values();
    test_blank_frame_is_background_on_every_row();
    test_full_frame_lights_exactly_the_picture();
    test_one_pixel_is_a_square();
    test_corners_and_edges();
    test_page_layout_and_bit_order();
    test_ink_picks_the_pixels_own_id();
    test_ink_on_unlit_pixels_is_ignored_and_no_plane_is_neutral();
    test_swapped_palette_row();
    printf("lcd: %d checks, %d failed\n", checks, fails);
    return fails;
}
