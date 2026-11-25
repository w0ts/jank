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

static uint8_t colour_at(const struct jlmx *s, uint8_t x, uint8_t y, uint32_t now_ms)
{
    struct jlx_frame f;
    memset(&f, 0, sizeof f);
    jlmx_frame(s, now_ms, &f);
    return f.led[y * 8u + x].color;
}

static uint8_t colour(const struct jlmx *s, uint8_t x, uint8_t y) { return colour_at(s, x, y, 0); }

/* a fader pad tapped: pressed and let go at once (Launchpad 95's slow faders: a tap lands at once); 1 and *w the write */
static int tap(struct jlmx *s, uint8_t x, uint8_t y, uint32_t t, struct jlmx_write *w) { return !jlmx_press(s, x, y, t, w) && jlmx_release(s, x, y, t, w); }

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

/* Launchpad 95's overview: a row per thing, in the scene buttons' order, from the top: volume, pan, delay, reverb, chorus (the owner's colours), mute (YELLOW), solo (BLUE), nothing */
static void test_the_overview_has_a_row_per_thing(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_OVERVIEW);
    for (uint8_t x = 0; x < 8; x++) {
        for (uint8_t y = 3; y < 8; y++)
            CHECK(colour(&s, x, y) == (y == 7 ? JLMX_COLOR_VOL : y == 6 ? JLMX_COLOR_PAN : JLMX_COLOR_SEND));   /* rows 1-5 from the top (pad rows 7 .. 3), each in its fader's colour */
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
    CHECK(lit_in_column(&s, 1) == 2 && colour(&s, 1, 1) == JLMX_COLOR_VOL && colour(&s, 1, 2) == 0);   /* volume 8: up to step 1 (18) */
    CHECK(lit_in_column(&s, 4) == 3);                              /* 32: up to step 2 (39) */
    CHECK(lit_in_column(&s, 7) == 5);                              /* 56: up to step 4 (70) */
    jlmx_bank_step(&s, 1);
    CHECK(lit_in_column(&s, 0) == 2 && lit_in_column(&s, 1) == 2 && lit_in_column(&s, 2) == 3);   /* 1, 18: step 1 (18); 19: step 2 */
    CHECK(lit_in_column(&s, 3) == 8 && lit_in_column(&s, 4) == 8 && colour(&s, 3, 7) == JLMX_COLOR_VOL);
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
    CHECK(lit_in_column(&s, 0) == 4 && colour(&s, 0, 0) == JLMX_COLOR_PAN && colour(&s, 0, 3) == JLMX_COLOR_PAN && colour(&s, 0, 4) == 0);   /* hard left: pads 0 .. 3 */
    CHECK(lit_in_column(&s, 1) == 4 && colour(&s, 1, 7) == JLMX_COLOR_PAN && colour(&s, 1, 4) == JLMX_COLOR_PAN && colour(&s, 1, 3) == 0);   /* hard right: pads 4 .. 7 */
    CHECK(lit_in_column(&s, 2) == 2 && colour(&s, 2, 3) == JLMX_COLOR_PAN && colour(&s, 2, 4) == JLMX_COLOR_PAN);                         /* centre: the two middle pads */
    CHECK(lit_in_column(&s, 3) == 2 && colour(&s, 3, 2) == JLMX_COLOR_PAN && colour(&s, 3, 3) == JLMX_COLOR_PAN && colour(&s, 3, 1) == 0);   /* 30: pads 2 and 3 (the step 43 and the centre) */
    CHECK(lit_in_column(&s, 4) == 2 && colour(&s, 4, 3) == JLMX_COLOR_PAN && colour(&s, 4, 4) == JLMX_COLOR_PAN);                         /* 63 is centre too */
    CHECK(lit_in_column(&s, 5) == 2 && colour(&s, 5, 4) == JLMX_COLOR_PAN && colour(&s, 5, 5) == JLMX_COLOR_PAN && colour(&s, 5, 6) == 0);   /* 100: pads 4 and 5 */
    CHECK(lit_in_column(&s, 6) == 1 && colour(&s, 6, 3) == JLMX_COLOR_PAN && colour(&s, 6, 2) == 0);                                         /* 62: just left of centre: only the centre pad 3 (the step 43 is below it) */
    CHECK(lit_in_column(&s, 7) == 1 && colour(&s, 7, 4) == JLMX_COLOR_PAN && colour(&s, 7, 5) == 0);                                         /* 65: just right: only the centre pad 4 (the step 84 is above it) */
}

/* owner 2026-11-24: mute and solo are on / off buttons for all 16 tracks, 4 rows: the lower four pad rows, from the top mute 1-8, mute 9-16, solo 1-8, solo 9-16 */
static void test_the_mute_and_solo_screen_shows_16_tracks_in_four_rows(void)
{
    struct jdl_mixer m = mixer();                                  /* mute: tracks 1 and 3; solo: track 16 */
    for (uint8_t view = JLMX_MUTE; view <= JLMX_SOLO; view++) {     /* scene buttons 6 and 7 are the same screen */
        struct jlmx s = on_view(&m, view);
        for (unsigned bank = 0; bank < 2; bank++) {                 /* ... whatever the bank */
            s.bank = (uint8_t)bank;
            for (uint8_t y = 4; y < 8; y++)
                for (uint8_t x = 0; x < 8; x++)
                    CHECK(colour(&s, x, y) == 0);                   /* the top four rows are dark */
            CHECK(colour(&s, 0, 3) == JLMX_COLOR_MUTED && colour(&s, 1, 3) == JLMX_COLOR_UNMUTED && colour(&s, 2, 3) == JLMX_COLOR_MUTED && colour(&s, 3, 3) == JLMX_COLOR_UNMUTED);   /* mute 1-8 */
            for (uint8_t x = 0; x < 8; x++)
                CHECK(colour(&s, x, 2) == JLMX_COLOR_UNMUTED && colour(&s, x, 1) == JLMX_COLOR_NOSOLO);   /* mute 9-16: none; solo 1-8: none */
            CHECK(colour(&s, 7, 0) == JLMX_COLOR_SOLO && colour(&s, 6, 0) == JLMX_COLOR_NOSOLO && colour(&s, 0, 0) == JLMX_COLOR_NOSOLO);   /* solo 9-16: track 16 */
        }
    }
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
    for (uint8_t i = 0; i < 5; i++)
        CHECK(jlmx_scene_colour(&s, i) == JLMX_COLOR_SCENE_OFF);   /* on the overview no view is the one shown: all yellow, third */
    jlmx_scene(&s, 0);
    CHECK(jlmx_scene_colour(&s, 0) == JLMX_COLOR_SCENE_ON && jlmx_scene_colour(&s, 1) == JLMX_COLOR_SCENE_OFF);   /* the view shown: yellow full (owner) */
    jlmx_scene(&s, 1);
    CHECK(jlmx_scene_colour(&s, 1) == JLMX_COLOR_SCENE_ON && jlmx_scene_colour(&s, 0) == JLMX_COLOR_SCENE_OFF);
    jlmx_scene(&s, 4);
    CHECK(jlmx_scene_colour(&s, 4) == JLMX_COLOR_SCENE_ON && jlmx_scene_colour(&s, 3) == JLMX_COLOR_SCENE_OFF && jlmx_scene_colour(&s, 0) == JLMX_COLOR_SCENE_OFF);
    CHECK(jlmx_scene_colour(&s, 5) == JLMX_COLOR_MUTED && jlmx_scene_colour(&s, 6) == JLMX_COLOR_NOSOLO);
    jlmx_scene(&s, 5);                                             /* the mute / solo screen: scene buttons 6 and 7 are both on */
    CHECK(jlmx_scene_colour(&s, 5) == JLMX_COLOR_UNMUTED && jlmx_scene_colour(&s, 6) == JLMX_COLOR_SOLO && jlmx_scene_colour(&s, 0) == JLMX_COLOR_SCENE_OFF);
    jlmx_scene(&s, 6);
    CHECK(jlmx_scene_colour(&s, 6) == JLMX_COLOR_SOLO && jlmx_scene_colour(&s, 5) == JLMX_COLOR_UNMUTED);
    CHECK(jlmx_scene_colour(&s, 7) == 0);
    CHECK(JLMX_COLOR_UNMUTED == 13 && JLMX_COLOR_MUTED == 15 && JLMX_COLOR_SOLO == 45 && JLMX_COLOR_NOSOLO == 47);   /* LP95's YELLOW, YELLOW_THIRD, BLUE, BLUE_THIRD */
    CHECK(JLMX_COLOR_VOL == 25 && JLMX_COLOR_PAN == 17 && JLMX_COLOR_SEND == 60);   /* the owner's picks (2026-11-25) */
    CHECK(JLMX_COLOR_SCENE_ON == 13 && JLMX_COLOR_SCENE_OFF == 15 && JLMX_COLOR_BUTTON == 29 && JLMX_COLOR_BUTTON_DIM == 31);   /* YELLOW full / third; MINT, MINT_THIRD */
}

static void test_a_fader_tap_writes_its_step_and_the_fader_moves_at_once(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s;
    struct jlmx_write w;
    jlmx_init(&s);
    jlmx_scene(&s, JLMX_VOL);
    CHECK(!tap(&s, 0, 3, 0, &w));                           /* nothing known yet: nothing to write */
    jlmx_on_mixer(&s, &m, 0);
    CHECK(tap(&s, 2, 3, 100, &w) && w.kind == JLMX_WRITE_VALUE && w.track == 2 && w.param == JDL_MIX_VOL && w.value == VOL_STEP[3]);   /* pad 4 of 8 */
    CHECK(lit_in_column(&s, 2) == 4 && colour(&s, 2, 3) == JLMX_COLOR_VOL && colour(&s, 2, 4) == 0);   /* the fader is 4 pads high now */
    CHECK(tap(&s, 2, 7, 110, &w) && w.value == 127 && lit_in_column(&s, 2) == 8);   /* the top pad: full */
    CHECK(tap(&s, 2, 1, 120, &w) && w.value == VOL_STEP[1] && lit_in_column(&s, 2) == 2);
    CHECK(tap(&s, 2, 0, 130, &w) && w.value == 0 && lit_in_column(&s, 2) == 0);     /* the lowest pad: 0, nothing lit (as Launchpad 95) */
    CHECK(tap(&s, 2, 0, 140, &w) && w.value == 0);                                  /* and again: still 0 */
    jlmx_bank_step(&s, 1);
    CHECK(tap(&s, 3, 5, 150, &w) && w.track == 11 && w.value == VOL_STEP[5]);       /* the second bank: column 3 is track 12 (index 11) */
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
            CHECK(tap(&s, 0, y, 0, &w) && lit_in_column(&s, 0) == (unsigned)y + 1u && w.value == step[y]);
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
        CHECK(tap(&s, 1, y, 0, &w) && w.param == JDL_MIX_PAN && w.value == PAN_STEP[y]);
        CHECK(lit_in_column(&s, 1) == LIT[y]);
        CHECK(y > 3 ? colour(&s, 1, y == 4 ? 3 : y) == JLMX_COLOR_PAN : colour(&s, 1, y) == JLMX_COLOR_PAN);   /* the pressed pad is lit (the upper centre pad shows with the lower: the same value) */
    }
}

static void test_a_mute_or_solo_pad_toggles_its_track(void)
{
    struct jdl_mixer m = mixer();                                  /* mute: tracks 1 and 3; solo: track 16 */
    struct jlmx s = on_view(&m, JLMX_MUTE);
    struct jlmx_write w;
    CHECK(jlmx_press(&s, 1, 3, 0, &w) && w.kind == JLMX_WRITE_MASK && w.which == 0 && w.mask == 0x0007u);     /* mute 1-8, column 2: track 2 muted too: 0b111 */
    CHECK(colour(&s, 1, 3) == JLMX_COLOR_MUTED);
    CHECK(jlmx_press(&s, 0, 3, 0, &w) && w.mask == 0x0006u && colour(&s, 0, 3) == JLMX_COLOR_UNMUTED);        /* track 1 unmuted */
    CHECK(jlmx_press(&s, 1, 2, 0, &w) && w.which == 0 && w.mask == 0x0206u && colour(&s, 1, 2) == JLMX_COLOR_MUTED);   /* mute 9-16, column 2: track 10 */
    CHECK(jlmx_press(&s, 7, 0, 0, &w) && w.which == 1 && w.mask == 0x0000u && colour(&s, 7, 0) == JLMX_COLOR_NOSOLO); /* solo 9-16, column 8: track 16 not soloed any more */
    CHECK(jlmx_press(&s, 0, 1, 0, &w) && w.which == 1 && w.mask == 0x0001u && colour(&s, 0, 1) == JLMX_COLOR_SOLO);   /* solo 1-8, column 1: track 1 soloed */
    jlmx_scene(&s, 6);                                             /* the other scene button: the same screen */
    jlmx_bank_step(&s, 1);                                         /* and the bank does not matter */
    CHECK(jlmx_press(&s, 0, 3, 0, &w) && w.which == 0 && w.mask == 0x0207u);                                  /* still track 1 (muted again) */
    for (uint8_t y = 4; y < 8; y++)
        CHECK(!jlmx_press(&s, 0, y, 0, &w));                       /* the top four rows have no job */
}

static void test_the_digitones_older_reading_does_not_put_a_fader_back(void)
{
    struct jdl_mixer m = mixer(), old = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    struct jlmx_write w;
    tap(&s, 0, 7, 1000, &w);                                       /* track 1 volume to full */
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
    tap(&s, 4, 1, 6000, &w);
    jlmx_on_mixer(&s, &old, 6100);                                 /* a send fader: the same hold */
    CHECK(w.param == JDL_MIX_DEL && w.value == SEND_STEP[1] && lit_in_column(&s, 4) == 2);   /* delay 127 -> 13 (two pads), and the old reading (127) did not put it back */
}

/* owner 2026-11-25: Launchpad 95's slow faders (DeviceControllerStrip, ENABLE_TDC). A held fader pad writes nothing and steps through ten colours, one every 0.2 s */
static void test_a_held_fader_pad_steps_through_ten_colours(void)
{
    static const uint8_t SHADE[10] = {5, 9, 13, 21, 29, 37, 45, 53, 48, 3};   /* RED AMBER YELLOW GREEN MINT LIGHT_BLUE BLUE PINK PURPLE WHITE */
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    struct jlmx_write w;
    CHECK(!jlmx_press(&s, 0, 7, 1000, &w));                       /* held: nothing written yet */
    for (uint32_t i = 0; i < 10; i++)
        CHECK(colour_at(&s, 0, 7, 1000 + i * 200u) == SHADE[i] && colour_at(&s, 0, 7, 1000 + i * 200u + 199u) == SHADE[i]);
    CHECK(colour_at(&s, 0, 7, 9000) == 3);                         /* white from 1.8 s on */
    CHECK(colour_at(&s, 0, 6, 1500) == 0 && colour_at(&s, 1, 7, 1500) == 0 && colour_at(&s, 1, 0, 1500) == JLMX_COLOR_VOL);   /* the rest as before */
    CHECK(s.m.t[0].vol == 0);                                      /* the fader has not moved */
}

/* the time held picks how long a sweep of the whole range takes (TDC_MAP, 0 .. 2 s in ten steps); the slide writes as it moves, every 100 ms */
static void test_the_time_held_picks_the_speed(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    struct jlmx_write w[JLMX_SLIDES];
    jlmx_press(&s, 0, 7, 1000, w);
    CHECK(jlmx_release(&s, 0, 7, 2000, w) && w[0].kind == JLMX_WRITE_VALUE && w[0].track == 0 && w[0].param == JDL_MIX_VOL && w[0].value == 1);   /* 1 s: a 5 s sweep; its first 50 ms at once */
    CHECK(jlmx_slides(&s, 2050, w, JLMX_SLIDES) == 1 && w[0].value == 2);
    CHECK(jlmx_slides(&s, 2100, w, JLMX_SLIDES) == 0);            /* 100 ms between steps */
    CHECK(jlmx_slides(&s, 2150, w, JLMX_SLIDES) == 1 && w[0].value == 5);
    jlmx_scene(&s, JLMX_PAN);                                      /* another view: the slide goes on */
    CHECK(jlmx_slides(&s, 4450, w, JLMX_SLIDES) == 1 && w[0].value == 63);   /* half way after 2.5 s */
    jlmx_scene(&s, JLMX_VOL);
    CHECK(lit_in_column(&s, 0) == 5);                              /* the fader shows it (63: up to the step 70) */
    CHECK(jlmx_slides(&s, 6950, w, JLMX_SLIDES) == 1 && w[0].value == 127);   /* there after 5 s */
    CHECK(jlmx_slides(&s, 7050, w, JLMX_SLIDES) == 0 && lit_in_column(&s, 0) == 8);   /* done */
}

static void test_a_tap_lands_at_once_and_a_two_second_hold_takes_forty(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    struct jlmx_write w[JLMX_SLIDES];
    jlmx_press(&s, 0, 7, 0, w);
    CHECK(jlmx_release(&s, 0, 7, 221, w) && w[0].value == 127 && lit_in_column(&s, 0) == 8);   /* under 0.222 s: a tap, there at once */
    CHECK(jlmx_slides(&s, 300, w, JLMX_SLIDES) == 0);
    jlmx_press(&s, 1, 7, 1000, w);
    CHECK(jlmx_release(&s, 1, 7, 3000, w) && w[0].track == 1 && w[0].value == 8);   /* 2 s: a 40 s sweep (from 8; the first 50 ms moves less than 1) */
    CHECK(jlmx_slides(&s, 2950 + 20000, w, JLMX_SLIDES) == 1 && w[0].value == 8 + 63);   /* 20 s in: 63 more */
    CHECK(jlmx_slides(&s, 2950 + 37400, w, JLMX_SLIDES) == 1 && w[0].value == 126);
    CHECK(jlmx_slides(&s, 2950 + 37500, w, JLMX_SLIDES) == 1 && w[0].value == 127);
    CHECK(jlmx_slides(&s, 2950 + 40000, w, JLMX_SLIDES) == 0);
    jlmx_press(&s, 2, 0, 5000, w);
    CHECK(jlmx_release(&s, 2, 0, 5400, w) && w[0].value == 16 - 8);   /* 0.4 s: a 0.75 s sweep down from 16, its first 50 ms: 8 */
    CHECK(jlmx_slides(&s, 5450, w, JLMX_SLIDES) == 1 && w[0].value == 0);   /* 100 ms: 16 more, there */
}

/* Launchpad 95: a slide ends when its parameter was moved by something else. Here: the Digitone's reading is off the path the slide travelled (its knob) */
static void test_a_slide_stops_when_the_digitone_moves_the_fader_elsewhere(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    struct jlmx_write w[JLMX_SLIDES];
    jlmx_press(&s, 0, 7, 0, w);
    jlmx_release(&s, 0, 7, 1000, w);                               /* 0 -> 127 in 5 s */
    CHECK(jlmx_slides(&s, 2450, w, JLMX_SLIDES) == 1 && w[0].value == 38);
    m.t[0].vol = 20;                                               /* an older reading: on the path */
    jlmx_on_mixer(&s, &m, 2500);
    CHECK(s.m.t[0].vol == 38 && m.t[1].vol == s.m.t[1].vol);       /* the fader stays where the slide is; the other tracks follow the reading */
    CHECK(jlmx_slides(&s, 2550, w, JLMX_SLIDES) == 1 && w[0].value == 40);
    m.t[0].vol = 90;                                               /* off the path: the Digitone's own change */
    jlmx_on_mixer(&s, &m, 2600);
    CHECK(s.m.t[0].vol == 90 && jlmx_slides(&s, 2700, w, JLMX_SLIDES) == 0);   /* followed; the slide is over */
}

static void test_slides_run_together_and_a_new_one_replaces_the_old(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    struct jlmx_write w[JLMX_SLIDES];
    jlmx_press(&s, 0, 7, 0, w);
    jlmx_press(&s, 1, 0, 0, w);                                    /* two columns held at once */
    CHECK(jlmx_release(&s, 0, 7, 1000, w) && w[0].value == 1);    /* track 1: 0 -> 127 in 5 s */
    CHECK(jlmx_release(&s, 1, 0, 1000, w) && w[0].value == 7);    /* track 2: 8 -> 0 in 5 s */
    CHECK(jlmx_slides(&s, 1100, w, JLMX_SLIDES) == 2 && w[0].track == 0 && w[0].value == 3 && w[1].track == 1 && w[1].value == 5);
    jlmx_press(&s, 0, 0, 1100, w);
    CHECK(jlmx_release(&s, 0, 0, 2100, w) && w[0].track == 0 && w[0].value == 2);   /* track 1 again, down to 0: from where it is (3), the old slide gone */
    CHECK(jlmx_slides(&s, 2200, w, JLMX_SLIDES) == 2 && w[0].value == 0 && w[1].value == 0);   /* both there */
    CHECK(jlmx_slides(&s, 2300, w, JLMX_SLIDES) == 0);
    CHECK(jlmx_slides(&s, 2400, w, 1) == 0);
    jlmx_press(&s, 0, 7, 3000, w);
    jlmx_press(&s, 1, 7, 3000, w);
    jlmx_release(&s, 0, 7, 4000, w);
    jlmx_release(&s, 1, 7, 4000, w);
    CHECK(jlmx_slides(&s, 4100, w, 1) == 1);                       /* room for one write: one; the other comes at its next tick */
    CHECK(jlmx_slides(&s, 4200, w, JLMX_SLIDES) == 2);
    jlmx_stop_slides(&s);
    CHECK(jlmx_slides(&s, 4300, w, JLMX_SLIDES) == 0);
}

static void test_a_held_pad_does_nothing_when_the_screen_changes(void)
{
    struct jdl_mixer m = mixer();
    struct jlmx s = on_view(&m, JLMX_VOL);
    struct jlmx_write w;
    jlmx_press(&s, 0, 7, 0, &w);
    jlmx_scene(&s, JLMX_PAN);
    CHECK(!jlmx_release(&s, 0, 7, 500, &w));                       /* another view */
    jlmx_scene(&s, JLMX_VOL);
    jlmx_press(&s, 0, 7, 0, &w);
    jlmx_bank_step(&s, 1);
    CHECK(!jlmx_release(&s, 0, 7, 500, &w));                       /* another bank */
    jlmx_press(&s, 0, 7, 0, &w);
    jlmx_overview(&s);
    CHECK(!jlmx_release(&s, 0, 7, 500, &w));                       /* the overview */
    jlmx_scene(&s, JLMX_VOL);
    jlmx_press(&s, 0, 7, 0, &w);
    jlmx_press(&s, 0, 5, 100, &w);                                 /* a second pad in the same column replaces the first */
    CHECK(!jlmx_release(&s, 0, 7, 150, &w) && jlmx_release(&s, 0, 5, 150, &w) && w.value == VOL_STEP[5]);
    CHECK(!jlmx_release(&s, 0, 5, 160, &w));                       /* (released once) */
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
    test_the_mute_and_solo_screen_shows_16_tracks_in_four_rows();
    test_the_banks();
    test_scene_buttons();
    test_a_fader_tap_writes_its_step_and_the_fader_moves_at_once();
    test_every_fader_step_lights_exactly_the_pads_that_were_pressed();
    test_a_pan_press_writes_the_step_and_lights_the_bar();
    test_a_mute_or_solo_pad_toggles_its_track();
    test_the_digitones_older_reading_does_not_put_a_fader_back();
    test_a_held_fader_pad_steps_through_ten_colours();
    test_the_time_held_picks_the_speed();
    test_a_tap_lands_at_once_and_a_two_second_hold_takes_forty();
    test_a_slide_stops_when_the_digitone_moves_the_fader_elsewhere();
    test_slides_run_together_and_a_new_one_replaces_the_old();
    test_a_held_pad_does_nothing_when_the_screen_changes();
    printf("test_lpmix: %d checks, %d failed\n", checks, fails);
    return fails;
}
