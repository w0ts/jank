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
    CHECK(JLC_W == 320 && JLC_H == 240 && JO_LCD2_W == JLC_W && JO_LCD2_H == JLC_H);
    CHECK(JO_LCD2_FB_BYTES == 9600 && JO_LCD2_INK_BYTES == 76800 && JLC_PAGES == 30);
    CHECK(jlc_page_y(0) == 0 && jlc_page_y(29) == 232 && jlc_page_rows() == 8 && jlc_page_y(30) == JLC_H);
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

static void test_one_pixel_is_one_pixel(void)
{
    clear();
    set_px(5, 10, JO_INK_GOOD);          /* frame (5, 10) -> column 5, row 10, nothing else */
    for (unsigned y = 0; y < JLC_H; y++) {
        jlc_row(fb, ink, &pal, y, row);
        if (y == 10) {
            CHECK(lit() == 1 && row[5] == GOOD && row[4] == BG && row[6] == BG);
        } else {
            CHECK(lit() == 0);
        }
    }
}

static void test_corners_and_edges(void)
{
    clear();
    set_px(0, 0, 0);
    set_px(319, 0, 0);
    set_px(0, 239, 0);
    set_px(319, 239, 0);
    for (unsigned y = 0; y < JLC_H; y++) {
        jlc_row(fb, ink, &pal, y, row);
        if (y == 0 || y == 239) {
            CHECK(lit() == 2 && row[0] == NEUTRAL && row[319] == NEUTRAL);
        } else {
            CHECK(lit() == 0);
        }
    }
}

static void test_page_layout_and_bit_order(void)
{
    clear();
    fb[3 * JO_LCD2_W + 7] = 0x81;        /* page 3: frame rows 24 and 31 at x = 7 */
    for (unsigned y = 0; y < JLC_H; y++) {
        jlc_row(fb, NULL, &pal, y, row);
        int on = y == 24 || y == 31;
        CHECK(lit() == (on ? 1u : 0u) && (!on || row[7] == NEUTRAL));
    }
    CHECK(jlc_page_y(3) == 24 && jlc_page_y(3) + jlc_page_rows() - 1 == 31);
}

/* the ink plane is indexed y * 320 + x, not by the page layout */
static void test_ink_picks_the_pixels_own_id(void)
{
    static const struct {
        uint8_t id;
        uint16_t want;
    } T[] = {{JO_INK_NONE, NEUTRAL}, {JO_INK_GOOD, GOOD}, {JO_INK_WARN, WARN}, {JO_INK_BAD, BAD}, {JO_INK_DIM, DIM}, {JO_INK_ONE, WARN}};
    clear();
    for (unsigned i = 0; i < 6; i++)
        set_px(10 + 3 * i, 31, T[i].id);    /* page 3, bit 7: frame row 31 */
    for (unsigned y = 31; y <= 31; y++) {
        jlc_row(fb, ink, &pal, y, row);
        CHECK(lit() == 6);
        for (unsigned i = 0; i < 6; i++) {
            unsigned x = 10 + 3 * i;
            CHECK(row[x] == T[i].want && row[x - 1] == BG && row[x + 1] == BG);
        }
    }
    set_px(40, 31, JO_INK_IDS);           /* an id past the table */
    set_px(41, 31, 255);
    jlc_row(fb, ink, &pal, 31, row);
    CHECK(row[40] == NEUTRAL && row[41] == NEUTRAL);
}

/* the VU ramp: ids 6 .. 37, RGB565 worked out by hand from ui/lcd2.js VU_RAMP (steps 0, 16, 17, 23, 28 and 31), and the first id past it is neutral */
static void test_vu_ramp(void)
{
    static const struct {
        unsigned step;
        uint16_t want;
    } T[] = {{0, 0x4EF0u}, {16, 0x4EF0u}, {17, 0x56EEu}, {23, 0xB728u}, {28, 0xFD84u}, {31, 0xFB0Bu}};
    struct jlc_pal swapped;
    CHECK(JO_INK_VU0 == 6 && JO_VU_STEPS == 32 && JO_INK_IDS == 38);
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++)
        CHECK(pal.ink[JO_INK_VU0 + T[i].step] == T[i].want);
    jlc_palette(&swapped, 1);
    CHECK(swapped.ink[JO_INK_VU0 + 17] == 0xEE56u && swapped.ink[JO_INK_VU0 + 31] == 0x0BFBu);
    for (unsigned i = 1; i < JO_VU_STEPS; i++)
        CHECK(pal.ink[JO_INK_VU0 + i] != BG && pal.ink[JO_INK_VU0 + i] != NEUTRAL);
    clear();
    for (unsigned i = 0; i < JO_VU_STEPS; i++)
        set_px(i, 10, (uint8_t)(JO_INK_VU0 + i));
    set_px(40, 10, JO_INK_IDS);
    jlc_row(fb, ink, &pal, 10, row);
    for (unsigned i = 0; i < JO_VU_STEPS; i++)
        CHECK(row[i] == pal.ink[JO_INK_VU0 + i]);
    CHECK(row[40] == NEUTRAL);
}

static void test_ink_on_unlit_pixels_is_ignored_and_no_plane_is_neutral(void)
{
    clear();
    ink[10 * JO_LCD2_W + 5] = JO_INK_BAD;      /* an id on a pixel that is not lit: the background */
    set_px(6, 10, JO_INK_GOOD);
    jlc_row(fb, ink, &pal, 10, row);
    CHECK(lit() == 1 && row[6] == GOOD && row[5] == BG);
    jlc_row(fb, NULL, &pal, 10, row);
    CHECK(lit() == 1 && row[6] == NEUTRAL && row[5] == BG);
}

static void test_swapped_palette_row(void)
{
    struct jlc_pal swapped;
    jlc_palette(&swapped, 1);
    clear();
    set_px(0, 0, JO_INK_WARN);
    jlc_row(fb, ink, &swapped, 0, row);
    CHECK(row[0] == 0x84FDu && row[1] == 0x4108u && row[319] == 0x4108u);
}

int main(void)
{
    jlc_palette(&pal, 0);
    test_geometry();
    test_palette_values();
    test_blank_frame_is_background_on_every_row();
    test_full_frame_covers_the_whole_panel();
    test_one_pixel_is_one_pixel();
    test_corners_and_edges();
    test_page_layout_and_bit_order();
    test_ink_picks_the_pixels_own_id();
    test_vu_ramp();
    test_ink_on_unlit_pixels_is_ignored_and_no_plane_is_neutral();
    test_swapped_palette_row();
    printf("lcd: %d checks, %d failed\n", checks, fails);
    return fails;
}
