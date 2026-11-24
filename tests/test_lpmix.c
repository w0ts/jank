/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the mixer screen (core/jank_lpmix.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpmix.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static struct jdl_mixer mixer(void)
{
    struct jdl_mixer m;
    memset(&m, 0, sizeof m);
    for (unsigned i = 0; i < JDL_TRACKS; i++) {
        m.t[i].vol = (uint8_t)(i * 8u);        /* 0, 8, 16 .. 120 */
        m.t[i].pan = 64;
        m.t[i].del = 127;
        m.t[i].rev = 1;
        m.t[i].chr = 0;
        m.t[i].level = 100;
    }
    m.mute = 0x0005u;                          /* tracks 1 and 3 */
    m.solo = 0x8000u;                          /* track 16 */
    return m;
}

static uint8_t colour(const struct jlmx *s, uint8_t x, uint8_t y)
{
    struct jlx_frame f;
    memset(&f, 0, sizeof f);
    jlmx_frame(s, &f);
    return f.led[y * 8u + x].color;
}

static unsigned lit_in_column(const struct jlmx *s, uint8_t x)
{
    unsigned n = 0;
    for (uint8_t y = 0; y < 8; y++)
        n += colour(s, x, y) != 0;
    return n;
}

static void test_nothing_is_shown_until_the_digitone_answers(void)
{
    struct jlmx s;
    jlmx_init(&s);
    CHECK(s.view == JLMX_VOL && s.bank == 0 && !s.have);
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++)
            CHECK(colour(&s, x, y) == 0);
}

static void test_volume_faders_fill_from_the_bottom(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    jlmx_init(&s);
    jlmx_on_mixer(&s, &m);
    CHECK(lit_in_column(&s, 0) == 0);                              /* track 1: volume 0: nothing */
    CHECK(lit_in_column(&s, 1) == 1 && colour(&s, 1, 0) == JLMX_COLOR_FADER && colour(&s, 1, 1) == 0);   /* volume 8: the bottom pad only */
    CHECK(lit_in_column(&s, 4) == 3);                              /* track 5: 32 of 127: (8 * 32 + 126) / 127 = 3 pads */
    CHECK(lit_in_column(&s, 7) == 4);                              /* track 8: 56 of 127: (8 * 56 + 126) / 127 = 4 pads */
}

static void test_fader_heights_at_the_edges(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    jlmx_init(&s);
    m.t[0].vol = 0;
    m.t[1].vol = 1;
    m.t[2].vol = 127;
    m.t[3].vol = 64;
    m.t[4].vol = 200;                                              /* past the range: no more than the top */
    jlmx_on_mixer(&s, &m);
    CHECK(lit_in_column(&s, 0) == 0 && lit_in_column(&s, 1) == 1 && lit_in_column(&s, 2) == 8 && lit_in_column(&s, 3) == 5 && lit_in_column(&s, 4) == 8);
    CHECK(colour(&s, 2, 7) == JLMX_COLOR_FADER);                   /* the top pad of a full fader */
}

static void test_the_sends_have_their_own_views(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    jlmx_init(&s);
    jlmx_on_mixer(&s, &m);
    CHECK(jlmx_scene(&s, 2) && s.view == JLMX_DEL && lit_in_column(&s, 0) == 8);   /* delay 127: full */
    CHECK(jlmx_scene(&s, 3) && s.view == JLMX_REV && lit_in_column(&s, 0) == 1);   /* reverb 1: the bottom pad */
    CHECK(jlmx_scene(&s, 4) && s.view == JLMX_CHR && lit_in_column(&s, 0) == 0);   /* chorus 0: nothing */
    CHECK(jlmx_scene(&s, 0) && s.view == JLMX_VOL);
}

static void test_pan_is_one_pad_with_the_centre_marked(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    jlmx_init(&s);
    m.t[0].pan = 0;
    m.t[1].pan = 127;
    m.t[2].pan = 64;
    m.t[3].pan = 30;
    jlmx_on_mixer(&s, &m);
    jlmx_scene(&s, 1);
    CHECK(colour(&s, 0, 0) == JLMX_COLOR_FADER && colour(&s, 0, 7) == 0);                      /* hard left: the bottom pad */
    CHECK(colour(&s, 1, 7) == JLMX_COLOR_FADER && colour(&s, 1, 0) == 0);                      /* hard right: the top pad */
    CHECK(colour(&s, 2, 3) == JLMX_COLOR_FADER && colour(&s, 2, 4) == JLMX_COLOR_FADER_THIRD); /* the centre: pad 3 lit, its neighbour marked */
    CHECK(colour(&s, 3, 1) == JLMX_COLOR_FADER && colour(&s, 3, 3) == JLMX_COLOR_FADER_THIRD && colour(&s, 3, 4) == JLMX_COLOR_FADER_THIRD);   /* left of centre: the marks stay */
    CHECK(lit_in_column(&s, 0) == 3 && lit_in_column(&s, 2) == 2);                             /* the pad and the two marks (a mark under the pad is the pad) */
}

static void test_mute_and_solo_show_whole_columns(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    jlmx_init(&s);
    jlmx_on_mixer(&s, &m);
    jlmx_scene(&s, 5);
    CHECK(s.view == JLMX_MUTE);
    CHECK(colour(&s, 0, 0) == JLMX_COLOR_MUTED && colour(&s, 0, 7) == JLMX_COLOR_MUTED);       /* track 1: muted: dim */
    CHECK(colour(&s, 1, 3) == JLMX_COLOR_UNMUTED);                                             /* track 2: not muted: bright */
    CHECK(colour(&s, 2, 5) == JLMX_COLOR_MUTED && colour(&s, 3, 5) == JLMX_COLOR_UNMUTED);
    jlmx_scene(&s, 6);
    CHECK(s.view == JLMX_SOLO && colour(&s, 0, 0) == JLMX_COLOR_NOSOLO && colour(&s, 7, 7) == JLMX_COLOR_NOSOLO);
    jlmx_bank_step(&s, 1);
    CHECK(colour(&s, 7, 2) == JLMX_COLOR_SOLO && colour(&s, 6, 2) == JLMX_COLOR_NOSOLO);       /* track 16 is the soloed one */
}

static void test_the_banks(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    jlmx_init(&s);
    jlmx_on_mixer(&s, &m);
    CHECK(lit_in_column(&s, 7) == 4);                              /* track 8: volume 56 */
    CHECK(!jlmx_bank_step(&s, -1) && s.bank == 0);                 /* nothing below the first bank */
    CHECK(jlmx_bank_step(&s, 1) && s.bank == 1);
    CHECK(lit_in_column(&s, 0) == 5 && lit_in_column(&s, 7) == 8); /* track 9: 64; track 16: 120 */
    CHECK(!jlmx_bank_step(&s, 1) && s.bank == 1);                  /* nothing above the second */
    CHECK(jlmx_bank_step(&s, -1) && s.bank == 0);
    CHECK(!jlmx_bank_step(&s, 0));
}

static void test_scene_buttons(void)
{
    struct jlmx s;
    jlmx_init(&s);
    CHECK(!jlmx_scene(&s, 7) && !jlmx_scene(&s, 200) && s.view == JLMX_VOL);   /* the 8th has no job; a wild index changes nothing */
    CHECK(jlmx_scene_colour(&s, 0) == JLMX_COLOR_FADER && jlmx_scene_colour(&s, 1) == JLMX_COLOR_FADER_THIRD);   /* the view shown is bright, the others dim */
    jlmx_scene(&s, 4);
    CHECK(jlmx_scene_colour(&s, 4) == JLMX_COLOR_FADER && jlmx_scene_colour(&s, 0) == JLMX_COLOR_FADER_THIRD);
    CHECK(jlmx_scene_colour(&s, 5) == JLMX_COLOR_MUTED && jlmx_scene_colour(&s, 6) == JLMX_COLOR_NOSOLO);
    jlmx_scene(&s, 5);
    CHECK(jlmx_scene_colour(&s, 5) == JLMX_COLOR_UNMUTED);
    jlmx_scene(&s, 6);
    CHECK(jlmx_scene_colour(&s, 6) == JLMX_COLOR_SOLO);
    CHECK(jlmx_scene_colour(&s, 7) == 0);
    CHECK(JLMX_COLOR_FADER == 29 && JLMX_COLOR_FADER_THIRD == 31 && JLMX_COLOR_UNMUTED == 13 && JLMX_COLOR_MUTED == 15 && JLMX_COLOR_SOLO == 45 && JLMX_COLOR_NOSOLO == 47);   /* LP95's MINT, MINT_THIRD, YELLOW, YELLOW_THIRD, BLUE, BLUE_THIRD */
}

int main(void)
{
    test_nothing_is_shown_until_the_digitone_answers();
    test_volume_faders_fill_from_the_bottom();
    test_fader_heights_at_the_edges();
    test_the_sends_have_their_own_views();
    test_pan_is_one_pad_with_the_centre_marked();
    test_mute_and_solo_show_whole_columns();
    test_the_banks();
    test_scene_buttons();
    printf("test_lpmix: %d checks, %d failed\n", checks, fails);
    return fails;
}
