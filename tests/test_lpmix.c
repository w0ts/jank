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
    jlmx_on_mixer(&s, &m, 0);
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
    jlmx_on_mixer(&s, &m, 0);
    CHECK(lit_in_column(&s, 0) == 0 && lit_in_column(&s, 1) == 1 && lit_in_column(&s, 2) == 8 && lit_in_column(&s, 3) == 5 && lit_in_column(&s, 4) == 8);
    CHECK(colour(&s, 2, 7) == JLMX_COLOR_FADER);                   /* the top pad of a full fader */
}

static void test_the_sends_have_their_own_views(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    jlmx_init(&s);
    jlmx_on_mixer(&s, &m, 0);
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
    jlmx_on_mixer(&s, &m, 0);
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
    jlmx_on_mixer(&s, &m, 0);
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
    jlmx_on_mixer(&s, &m, 0);
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

static void test_a_fader_press_writes_its_height_and_the_fader_moves_at_once(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    struct jlmx_write w;
    jlmx_init(&s);
    CHECK(!jlmx_press(&s, 0, 3, 0, &w));                           /* nothing known yet: nothing to write */
    jlmx_on_mixer(&s, &m, 0);
    CHECK(jlmx_press(&s, 2, 3, 100, &w) && w.kind == JLMX_WRITE_VALUE && w.track == 2 && w.param == JDL_MIX_VOL && w.value == 63);   /* pad 4 of 8: 4 * 127 / 8 */
    CHECK(lit_in_column(&s, 2) == 4 && colour(&s, 2, 3) == JLMX_COLOR_FADER && colour(&s, 2, 4) == 0);   /* the fader is 4 pads high now */
    CHECK(jlmx_press(&s, 2, 7, 110, &w) && w.value == 127 && lit_in_column(&s, 2) == 8);   /* the top pad: full */
    CHECK(jlmx_press(&s, 2, 0, 120, &w) && w.value == 15 && lit_in_column(&s, 2) == 1);    /* the lowest pad: one pad */
    CHECK(jlmx_press(&s, 2, 0, 130, &w) && w.value == 0 && lit_in_column(&s, 2) == 0);     /* the lowest again, at that step: nothing */
    CHECK(jlmx_press(&s, 2, 0, 140, &w) && w.value == 15);                                 /* and again: back to one pad */
    jlmx_bank_step(&s, 1);
    CHECK(jlmx_press(&s, 3, 5, 150, &w) && w.track == 11 && w.value == 95);                /* the second bank: column 3 is track 12 (index 11) */
}

static void test_every_fader_value_fills_exactly_the_pads_that_were_pressed(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    struct jlmx_write w;
    jlmx_init(&s);
    jlmx_on_mixer(&s, &m, 0);
    for (uint8_t view = JLMX_VOL; view <= JLMX_CHR; view++) {
        if (view == JLMX_PAN)
            continue;
        jlmx_scene(&s, view);
        for (uint8_t y = 1; y < 8; y++) {                          /* (the lowest pad has its own toggle rule, tested above) */
            CHECK(jlmx_press(&s, 0, y, 0, &w) && lit_in_column(&s, 0) == (unsigned)y + 1u);
            CHECK(w.param == (view == JLMX_VOL ? JDL_MIX_VOL : view == JLMX_DEL ? JDL_MIX_DEL : view == JLMX_REV ? JDL_MIX_REV : JDL_MIX_CHR));
        }
    }
}

static void test_a_pan_press_writes_the_position_and_the_lit_pad_is_the_pressed_one(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();
    struct jlmx_write w;
    static const uint8_t WANT[8] = {0, 23, 44, 64, 64, 84, 105, 127};
    jlmx_init(&s);
    jlmx_on_mixer(&s, &m, 0);
    jlmx_scene(&s, 1);
    for (uint8_t y = 0; y < 8; y++) {
        CHECK(jlmx_press(&s, 1, y, 0, &w) && w.param == JDL_MIX_PAN && w.value == WANT[y]);
        CHECK(colour(&s, 1, y == 4 ? 3 : y) == JLMX_COLOR_FADER);  /* the lit pad: the pressed one (the upper centre pad shows as the lower one: the same value) */
    }
}

static void test_mute_and_solo_presses_toggle_a_whole_track(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer();                                  /* mute: tracks 1 and 3; solo: track 16 */
    struct jlmx_write w;
    jlmx_init(&s);
    jlmx_on_mixer(&s, &m, 0);
    jlmx_scene(&s, 5);
    CHECK(jlmx_press(&s, 1, 4, 0, &w) && w.kind == JLMX_WRITE_MASK && w.which == 0 && w.mask == 0x0007u);     /* track 2 muted too: 0b111 */
    CHECK(colour(&s, 1, 0) == JLMX_COLOR_MUTED);
    CHECK(jlmx_press(&s, 0, 0, 0, &w) && w.mask == 0x0006u && colour(&s, 0, 0) == JLMX_COLOR_UNMUTED);        /* track 1 unmuted */
    jlmx_scene(&s, 6);
    jlmx_bank_step(&s, 1);
    CHECK(jlmx_press(&s, 7, 2, 0, &w) && w.which == 1 && w.mask == 0x0000u && colour(&s, 7, 0) == JLMX_COLOR_NOSOLO);   /* track 16 not soloed any more */
    CHECK(jlmx_press(&s, 0, 2, 0, &w) && w.which == 1 && w.mask == 0x0100u);                                  /* track 9 soloed: the whole mask */
}

static void test_the_digitones_older_reading_does_not_put_a_fader_back(void)
{
    struct jlmx s;
    struct jdl_mixer m = mixer(), old = mixer();
    struct jlmx_write w;
    jlmx_init(&s);
    jlmx_on_mixer(&s, &m, 0);
    jlmx_press(&s, 0, 7, 1000, &w);                                /* track 1 volume to full */
    jlmx_on_mixer(&s, &old, 1000 + JLMX_HOLD_MS - 1u);             /* a reading from before the write was applied */
    CHECK(lit_in_column(&s, 0) == 8);                              /* not taken */
    m.t[0].vol = 127;
    jlmx_on_mixer(&s, &m, 1000 + JLMX_HOLD_MS);                    /* later: taken (it agrees) */
    CHECK(lit_in_column(&s, 0) == 8);
    m.t[0].vol = 40;                                               /* the Digitone's own change, after the hold: followed */
    jlmx_on_mixer(&s, &m, 5000);
    CHECK(lit_in_column(&s, 0) == 3);
    jlmx_on_mixer(&s, &m, 5001);
    jlmx_scene(&s, 2);
    jlmx_press(&s, 4, 0, 6000, &w);
    jlmx_on_mixer(&s, &old, 6100);                                 /* a send fader: the same hold */
    CHECK(w.param == JDL_MIX_DEL && w.value == 15 && lit_in_column(&s, 4) == 1);   /* delay 127 -> 15 (one pad), and the old reading (127) did not put it back */
}

int main(void)
{
    test_a_fader_press_writes_its_height_and_the_fader_moves_at_once();
    test_every_fader_value_fills_exactly_the_pads_that_were_pressed();
    test_a_pan_press_writes_the_position_and_the_lit_pad_is_the_pressed_one();
    test_mute_and_solo_presses_toggle_a_whole_track();
    test_the_digitones_older_reading_does_not_put_a_fader_back();
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
