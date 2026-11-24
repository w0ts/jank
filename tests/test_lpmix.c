/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the mixer screen (core/jank_lpmix.c): the overview and the fader sub modes after Launchpad 95's mixer (SubSelectorComponent.py, PreciseButtonSliderElement.py).
 * Exit status: the number of failed checks. */
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

/* Launchpad 95's steps as the Digitone's 0..127 (jank_lpmix.c) */
static const uint8_t VOL_STEP[8] = {0, 18, 39, 51, 70, 89, 108, 127};
static const uint8_t SEND_STEP[8] = {0, 13, 21, 30, 44, 70, 98, 127};
static const uint8_t PAN_STEP[8] = {0, 23, 43, 64, 64, 84, 104, 127};

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

/* a mixer that has the Digitone's answer, on a view */
static struct jlmx on_view(const struct jdl_mixer *m, uint8_t view)
{
    struct jlmx s;
    jlmx_init(&s);
    jlmx_on_mixer(&s, m, 0);
    if (view != JLMX_OVERVIEW)
        jlmx_scene(&s, view);
    return s;
}

static void test_it_starts_on_the_overview_and_shows_nothing_until_the_digitone_answers(void)
{
    struct jlmx s;
    jlmx_init(&s);
    CHECK(s.view == JLMX_OVERVIEW && s.bank == 0 && !s.have);
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++)
            CHECK(colour(&s, x, y) == 0);
}

/* Launchpad 95's overview: a row per thing, in the scene buttons' order, from the top: volume, pan, delay, reverb, chorus (MINT), mute (YELLOW), solo (BLUE), nothing */
static void test_the_overview_has_a_row_per_thing(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_OVERVIEW);
    for (uint8_t x = 0; x < 8; x++) {
        for (uint8_t y = 3; y < 8; y++)
            CHECK(colour(&s, x, y) == JLMX_COLOR_FADER);          /* rows 1-5 from the top (pad rows 7 .. 3) */
        CHECK(colour(&s, x, 0) == 0);                              /* row 8 */
    }
    CHECK(colour(&s, 0, 2) == JLMX_COLOR_MUTED && colour(&s, 1, 2) == JLMX_COLOR_UNMUTED && colour(&s, 2, 2) == JLMX_COLOR_MUTED && colour(&s, 7, 2) == JLMX_COLOR_UNMUTED);   /* row 6: mute */
    CHECK(colour(&s, 0, 1) == JLMX_COLOR_NOSOLO && colour(&s, 7, 1) == JLMX_COLOR_NOSOLO);   /* row 7: solo (track 16 is in the other bank) */
    jlmx_bank_step(&s, 1);
    CHECK(colour(&s, 7, 1) == JLMX_COLOR_SOLO && colour(&s, 6, 1) == JLMX_COLOR_NOSOLO && colour(&s, 0, 2) == JLMX_COLOR_UNMUTED);   /* the second bank: track 16 soloed */
}

static void test_the_overviews_value_rows_open_their_sub_mode(void)
{
    struct jdl_mixer m = mixer();
    for (uint8_t y = 3; y < 8; y++) {
        struct jlmx s = on_view(&m, JLMX_OVERVIEW);
        CHECK(jlmx_open(&s, y) && s.view == 7 - y);                /* pad row 7 = the top = volume (view 0) .. pad row 3 = chorus (view 4) */
    }
    for (uint8_t y = 0; y < 3; y++) {
        struct jlmx s = on_view(&m, JLMX_OVERVIEW);
        CHECK(!jlmx_open(&s, y) && s.view == JLMX_OVERVIEW);       /* mute, solo and the last row do not open anything */
    }
    {
        struct jlmx s = on_view(&m, JLMX_VOL);
        CHECK(!jlmx_open(&s, 6) && s.view == JLMX_VOL);            /* only the overview opens views by a pad */
        jlmx_overview(&s);
        CHECK(s.view == JLMX_OVERVIEW);
    }
}

static void test_the_overviews_mute_and_solo_rows_toggle_a_track(void)
{
    struct jdl_mixer m = mixer();                                  /* mute: tracks 1 and 3; solo: track 16 */
    struct jlmx s = on_view(&m, JLMX_OVERVIEW);
    struct jlmx_write w;
    CHECK(jlmx_press(&s, 1, 2, 0, &w) && w.kind == JLMX_WRITE_MASK && w.which == 0 && w.mask == 0x0007u);   /* row 6: track 2 muted too */
    CHECK(colour(&s, 1, 2) == JLMX_COLOR_MUTED);
    CHECK(jlmx_press(&s, 0, 2, 0, &w) && w.mask == 0x0006u && colour(&s, 0, 2) == JLMX_COLOR_UNMUTED);
    CHECK(jlmx_press(&s, 3, 1, 0, &w) && w.kind == JLMX_WRITE_MASK && w.which == 1 && w.mask == 0x8008u && colour(&s, 3, 1) == JLMX_COLOR_SOLO);   /* row 7: track 4 soloed */
    for (uint8_t y = 3; y < 8; y++)
        CHECK(!jlmx_press(&s, 0, y, 0, &w));                       /* the value rows write nothing (they open a view) */
    CHECK(!jlmx_press(&s, 0, 0, 0, &w));                           /* nor does the last row */
    jlmx_bank_step(&s, 1);
    CHECK(jlmx_press(&s, 7, 1, 0, &w) && w.which == 1 && w.mask == 0x0008u);                                /* the second bank: track 16 no longer soloed */
}

/* the volume fader: Launchpad 95 lights nothing for 0, else the pads up to the first step the value does not pass */
static void test_volume_faders_light_up_to_the_first_step_not_passed(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s;
    m.t[8].vol = 1;
    m.t[9].vol = 18;
    m.t[10].vol = 19;
    m.t[11].vol = 127;
    m.t[12].vol = 200;                                             /* past the range: the whole fader */
    s = on_view(&m, JLMX_VOL);
    CHECK(lit_in_column(&s, 0) == 0);                              /* track 1: volume 0: nothing */
    CHECK(lit_in_column(&s, 1) == 2 && colour(&s, 1, 1) == JLMX_COLOR_FADER && colour(&s, 1, 2) == 0);   /* volume 8: up to step 1 (18) */
    CHECK(lit_in_column(&s, 4) == 3);                              /* 32: up to step 2 (39) */
    CHECK(lit_in_column(&s, 7) == 5);                              /* 56: up to step 4 (70) */
    jlmx_bank_step(&s, 1);
    CHECK(lit_in_column(&s, 0) == 2 && lit_in_column(&s, 1) == 2 && lit_in_column(&s, 2) == 3);   /* 1, 18: step 1 (18); 19: step 2 */
    CHECK(lit_in_column(&s, 3) == 8 && lit_in_column(&s, 4) == 8 && colour(&s, 3, 7) == JLMX_COLOR_FADER);
    CHECK(lit_in_column(&s, 5) == 7 && lit_in_column(&s, 7) == 8);   /* track 14: 104 (up to step 6: 108 = 7 pads); track 16: 120 (step 7) */
}

static void test_the_sends_use_their_own_steps(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    CHECK(jlmx_scene(&s, 2) && s.view == JLMX_DEL && lit_in_column(&s, 0) == 8);   /* delay 127: full */
    CHECK(jlmx_scene(&s, 3) && s.view == JLMX_REV && lit_in_column(&s, 0) == 2);   /* reverb 1: up to the first step (13) */
    CHECK(jlmx_scene(&s, 4) && s.view == JLMX_CHR && lit_in_column(&s, 0) == 0);   /* chorus 0: nothing */
    m.t[0].chr = 44;
    m.t[1].chr = 45;
    jlmx_on_mixer(&s, &m, 0);
    CHECK(lit_in_column(&s, 0) == 5 && lit_in_column(&s, 1) == 6);                 /* 44 is step 4, 45 passes it */
    CHECK(jlmx_scene(&s, 0) && s.view == JLMX_VOL);
}

/* Launchpad 95's pan: the centre pair for 63 / 64, else a bar from the centre pair out to the value */
static void test_pan_is_a_bar_from_the_centre(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s;
    m.t[0].pan = 0;
    m.t[1].pan = 127;
    m.t[2].pan = 64;
    m.t[3].pan = 30;
    m.t[4].pan = 63;
    m.t[5].pan = 100;
    m.t[6].pan = 62;
    m.t[7].pan = 65;
    s = on_view(&m, JLMX_PAN);
    CHECK(lit_in_column(&s, 0) == 4 && colour(&s, 0, 0) == JLMX_COLOR_FADER && colour(&s, 0, 3) == JLMX_COLOR_FADER && colour(&s, 0, 4) == 0);   /* hard left: pads 0 .. 3 */
    CHECK(lit_in_column(&s, 1) == 4 && colour(&s, 1, 7) == JLMX_COLOR_FADER && colour(&s, 1, 4) == JLMX_COLOR_FADER && colour(&s, 1, 3) == 0);   /* hard right: pads 4 .. 7 */
    CHECK(lit_in_column(&s, 2) == 2 && colour(&s, 2, 3) == JLMX_COLOR_FADER && colour(&s, 2, 4) == JLMX_COLOR_FADER);                         /* centre: the two middle pads */
    CHECK(lit_in_column(&s, 3) == 2 && colour(&s, 3, 2) == JLMX_COLOR_FADER && colour(&s, 3, 3) == JLMX_COLOR_FADER && colour(&s, 3, 1) == 0);   /* 30: pads 2 and 3 (the step 43 and the centre) */
    CHECK(lit_in_column(&s, 4) == 2 && colour(&s, 4, 3) == JLMX_COLOR_FADER && colour(&s, 4, 4) == JLMX_COLOR_FADER);                         /* 63 is centre too */
    CHECK(lit_in_column(&s, 5) == 2 && colour(&s, 5, 4) == JLMX_COLOR_FADER && colour(&s, 5, 5) == JLMX_COLOR_FADER && colour(&s, 5, 6) == 0);   /* 100: pads 4 and 5 */
    CHECK(lit_in_column(&s, 6) == 1 && colour(&s, 6, 3) == JLMX_COLOR_FADER && colour(&s, 6, 2) == 0);                                         /* 62: just left of centre: only the centre pad 3 (the step 43 is below it) */
    CHECK(lit_in_column(&s, 7) == 1 && colour(&s, 7, 4) == JLMX_COLOR_FADER && colour(&s, 7, 5) == 0);                                         /* 65: just right: only the centre pad 4 (the step 84 is above it) */
}

static void test_mute_and_solo_show_whole_columns(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_MUTE);
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
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    CHECK(lit_in_column(&s, 7) == 5);                              /* track 8: volume 56 */
    CHECK(!jlmx_bank_step(&s, -1) && s.bank == 0);                 /* nothing below the first bank */
    CHECK(jlmx_bank_step(&s, 1) && s.bank == 1);
    CHECK(lit_in_column(&s, 0) == 5 && lit_in_column(&s, 7) == 8); /* track 9: 64 (step 4); track 16: 120 (step 7) */
    CHECK(!jlmx_bank_step(&s, 1) && s.bank == 1);                  /* nothing above the second */
    CHECK(jlmx_bank_step(&s, -1) && s.bank == 0);
    CHECK(!jlmx_bank_step(&s, 0));
}

static void test_scene_buttons(void)
{
    struct jlmx s;
    jlmx_init(&s);
    CHECK(!jlmx_scene(&s, 7) && !jlmx_scene(&s, 200) && s.view == JLMX_OVERVIEW);   /* the 8th has no job; a wild index changes nothing */
    CHECK(jlmx_scene_colour(&s, 0) == JLMX_COLOR_FADER_THIRD && jlmx_scene_colour(&s, 1) == JLMX_COLOR_FADER_THIRD);   /* on the overview no view is the one shown: all dim */
    jlmx_scene(&s, 0);
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

static void test_a_fader_press_writes_its_step_and_the_fader_moves_at_once(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s;
    struct jlmx_write w;
    jlmx_init(&s);
    jlmx_scene(&s, JLMX_VOL);
    CHECK(!jlmx_press(&s, 0, 3, 0, &w));                           /* nothing known yet: nothing to write */
    jlmx_on_mixer(&s, &m, 0);
    CHECK(jlmx_press(&s, 2, 3, 100, &w) && w.kind == JLMX_WRITE_VALUE && w.track == 2 && w.param == JDL_MIX_VOL && w.value == VOL_STEP[3]);   /* pad 4 of 8 */
    CHECK(lit_in_column(&s, 2) == 4 && colour(&s, 2, 3) == JLMX_COLOR_FADER && colour(&s, 2, 4) == 0);   /* the fader is 4 pads high now */
    CHECK(jlmx_press(&s, 2, 7, 110, &w) && w.value == 127 && lit_in_column(&s, 2) == 8);   /* the top pad: full */
    CHECK(jlmx_press(&s, 2, 1, 120, &w) && w.value == VOL_STEP[1] && lit_in_column(&s, 2) == 2);
    CHECK(jlmx_press(&s, 2, 0, 130, &w) && w.value == 0 && lit_in_column(&s, 2) == 0);     /* the lowest pad: 0, nothing lit (as Launchpad 95) */
    CHECK(jlmx_press(&s, 2, 0, 140, &w) && w.value == 0);                                  /* and again: still 0 */
    jlmx_bank_step(&s, 1);
    CHECK(jlmx_press(&s, 3, 5, 150, &w) && w.track == 11 && w.value == VOL_STEP[5]);       /* the second bank: column 3 is track 12 (index 11) */
}

static void test_every_fader_step_lights_exactly_the_pads_that_were_pressed(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx_write w;
    for (uint8_t view = JLMX_VOL; view <= JLMX_CHR; view++) {
        struct jlmx s = on_view(&m, view);
        const uint8_t *step = view == JLMX_VOL ? VOL_STEP : SEND_STEP;
        if (view == JLMX_PAN)
            continue;
        for (uint8_t y = 1; y < 8; y++) {
            CHECK(jlmx_press(&s, 0, y, 0, &w) && lit_in_column(&s, 0) == (unsigned)y + 1u && w.value == step[y]);
            CHECK(w.param == (view == JLMX_VOL ? JDL_MIX_VOL : view == JLMX_DEL ? JDL_MIX_DEL : view == JLMX_REV ? JDL_MIX_REV : JDL_MIX_CHR));
        }
    }
}

static void test_a_pan_press_writes_the_step_and_lights_the_bar(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_PAN);
    struct jlmx_write w;
    static const uint8_t LIT[8] = {4, 3, 2, 2, 2, 2, 3, 4};      /* the bar's pads for a press on pad y */
    for (uint8_t y = 0; y < 8; y++) {
        CHECK(jlmx_press(&s, 1, y, 0, &w) && w.param == JDL_MIX_PAN && w.value == PAN_STEP[y]);
        CHECK(lit_in_column(&s, 1) == LIT[y]);
        CHECK(y > 3 ? colour(&s, 1, y == 4 ? 3 : y) == JLMX_COLOR_FADER : colour(&s, 1, y) == JLMX_COLOR_FADER);   /* the pressed pad is lit (the upper centre pad shows with the lower: the same value) */
    }
}

static void test_mute_and_solo_views_press_toggle_a_whole_track(void)
{
    struct jdl_mixer m = mixer();                                  /* mute: tracks 1 and 3; solo: track 16 */
    struct jlmx s = on_view(&m, JLMX_MUTE);
    struct jlmx_write w;
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
    struct jdl_mixer m = mixer(), old = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    struct jlmx_write w;
    jlmx_press(&s, 0, 7, 1000, &w);                                /* track 1 volume to full */
    jlmx_on_mixer(&s, &old, 1000 + JLMX_HOLD_MS - 1u);             /* a reading from before the write was applied */
    CHECK(lit_in_column(&s, 0) == 8);                              /* not taken */
    m.t[0].vol = 127;
    jlmx_on_mixer(&s, &m, 1000 + JLMX_HOLD_MS);                    /* later: taken (it agrees) */
    CHECK(lit_in_column(&s, 0) == 8);
    m.t[0].vol = 40;                                               /* the Digitone's own change, after the hold: followed (step 3: 51) */
    jlmx_on_mixer(&s, &m, 5000);
    CHECK(lit_in_column(&s, 0) == 4);
    jlmx_on_mixer(&s, &m, 5001);
    jlmx_scene(&s, 2);
    jlmx_press(&s, 4, 1, 6000, &w);
    jlmx_on_mixer(&s, &old, 6100);                                 /* a send fader: the same hold */
    CHECK(w.param == JDL_MIX_DEL && w.value == SEND_STEP[1] && lit_in_column(&s, 4) == 2);   /* delay 127 -> 13 (two pads), and the old reading (127) did not put it back */
}

int main(void)
{
    test_it_starts_on_the_overview_and_shows_nothing_until_the_digitone_answers();
    test_the_overview_has_a_row_per_thing();
    test_the_overviews_value_rows_open_their_sub_mode();
    test_the_overviews_mute_and_solo_rows_toggle_a_track();
    test_volume_faders_light_up_to_the_first_step_not_passed();
    test_the_sends_use_their_own_steps();
    test_pan_is_a_bar_from_the_centre();
    test_mute_and_solo_show_whole_columns();
    test_the_banks();
    test_scene_buttons();
    test_a_fader_press_writes_its_step_and_the_fader_moves_at_once();
    test_every_fader_step_lights_exactly_the_pads_that_were_pressed();
    test_a_pan_press_writes_the_step_and_lights_the_bar();
    test_mute_and_solo_views_press_toggle_a_whole_track();
    test_the_digitones_older_reading_does_not_put_a_fader_back();
    printf("test_lpmix: %d checks, %d failed\n", checks, fails);
    return fails;
}
