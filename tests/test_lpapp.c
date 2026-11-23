/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of Launchpad Note mode against a Digitone II (core/jank_lpapp.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpapp.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const struct jln_scale MAJOR = {"Major", 7, {0, 2, 4, 5, 7, 9, 11}};

static struct jla fresh(void)
{
    struct jla a;
    jla_init(&a, &MAJOR, 0);
    return a;
}

static struct jlx_ev pad(uint8_t x, uint8_t y, uint8_t v)
{
    struct jlx_ev e = {JLX_PAD, y, x, 0, v};
    return e;
}

static struct jlx_ev top(uint8_t idx, uint8_t v)
{
    struct jlx_ev e = {JLX_TOP, 0, 0, idx, v};
    return e;
}

static size_t press(struct jla *a, struct jlx_ev e, uint8_t out[4])
{
    return jla_on_lpx(a, &e, out, 1);
}

static void dn2(struct jla *a, uint8_t status, uint8_t d1, uint8_t d2)
{
    const uint8_t p[4] = {(uint8_t)(status >> 4), status, d1, d2};
    jla_on_dn2(a, p);
}

static uint8_t colour(struct jla *a, uint8_t x, uint8_t y)
{
    struct jlx_frame f;
    jla_frame(a, &f);
    return f.led[y * 8u + x].color;
}

static void test_pad_plays_note_on_track_channel(void)
{
    struct jla a = fresh();
    uint8_t o[4];
    CHECK(press(&a, pad(0, 0, 100), o) == 1);                     /* diatonic C major, octave 3: 48 */
    CHECK(o[0] == 0x09 && o[1] == 0x90 && o[2] == 48 && o[3] == 100);
    CHECK(press(&a, pad(0, 0, 0), o) == 1);
    CHECK(o[0] == 0x08 && o[1] == 0x80 && o[2] == 48 && o[3] == 0);
    a.dn2_cable = 2;                                              /* the cable of the DN2's port */
    jla_on_lpx(&a, &(struct jlx_ev){JLX_TOP, 0, 0, 3, 127}, o, 1);   /* right: track 2, channel 2 */
    CHECK(press(&a, pad(1, 0, 64), o) == 1);
    CHECK(o[0] == 0x29 && o[1] == 0x91 && o[2] == 50 && o[3] == 64);
}

static void test_release_goes_to_the_note_it_started(void)
{
    struct jla a = fresh();
    uint8_t o[4];
    press(&a, pad(1, 0, 90), o);                                  /* 50 */
    press(&a, top(0, 127), o);                                    /* octave up while the pad is held */
    press(&a, top(0, 0), o);
    CHECK(a.n.octave == 4);
    CHECK(press(&a, pad(1, 0, 0), o) == 1 && o[2] == 50 && o[1] == 0x80);   /* not 62 */
}

static void test_release_goes_to_the_channel_it_started(void)
{
    struct jla a = fresh();
    uint8_t o[4];
    press(&a, pad(0, 0, 90), o);                                  /* track 1, channel 1 */
    press(&a, top(3, 127), o);                                    /* track 2 */
    CHECK(a.n.track == 1);
    CHECK(press(&a, pad(0, 0, 0), o) == 1 && o[1] == 0x80 && o[2] == 48);   /* still channel 1: no stuck note */
}

static void test_same_pitch_on_two_pads(void)
{
    struct jla a = fresh();
    uint8_t o[4];
    jln_set_layout(&a.n, JLN_CHROMATIC);                          /* (5,0) and (0,1) are both 53 */
    CHECK(press(&a, pad(5, 0, 80), o) == 1 && o[1] == 0x90);
    CHECK(press(&a, pad(0, 1, 80), o) == 1 && o[1] == 0x90);
    CHECK(press(&a, pad(5, 0, 0), o) == 0);                       /* the other pad still holds it */
    CHECK(press(&a, pad(0, 1, 0), o) == 1 && o[1] == 0x80 && o[2] == 53);
}

static void test_pad_outside_midi_range_is_silent(void)
{
    struct jla a = fresh();
    uint8_t o[4];
    jln_set_layout(&a.n, JLN_CHROMATIC);
    a.n.octave = 7;                                               /* base 96: the top rows pass 127 (the buttons stop at 6) */
    CHECK(press(&a, pad(0, 7, 90), o) == 0);
    CHECK(press(&a, pad(0, 7, 0), o) == 0);
}

static void test_repeated_press_and_stray_release(void)
{
    struct jla a = fresh();
    uint8_t o[4];
    CHECK(press(&a, pad(2, 2, 70), o) == 1);
    CHECK(press(&a, pad(2, 2, 70), o) == 0);                      /* a pad already down: nothing new */
    CHECK(press(&a, pad(2, 2, 0), o) == 1);
    CHECK(press(&a, pad(2, 2, 0), o) == 0);                       /* a release with no press: nothing */
    CHECK(press(&a, pad(3, 3, 0), o) == 0);
}

static void test_buttons(void)
{
    struct jla a = fresh();
    uint8_t o[4];
    CHECK(press(&a, top(0, 127), o) == 0 && a.n.octave == 4);     /* up: octave up, no MIDI */
    press(&a, top(1, 127), o);
    press(&a, top(1, 127), o);
    CHECK(a.n.octave == 2);
    press(&a, top(0, 0), o);                                      /* a release does nothing */
    press(&a, top(1, 0), o);
    CHECK(a.n.octave == 2);
    press(&a, top(2, 127), o);                                    /* left at the first track: stays */
    CHECK(a.n.track == 0);
    press(&a, top(3, 127), o);
    press(&a, top(3, 127), o);
    CHECK(a.n.track == 2);
    press(&a, top(2, 127), o);
    CHECK(a.n.track == 1);
    press(&a, top(5, 127), o);                                    /* the other buttons do nothing yet */
    press(&a, top(7, 127), o);
    CHECK(a.n.track == 1 && a.n.octave == 2);
    {
        struct jlx_ev side = {JLX_SIDE, 0, 0, 4, 127};
        CHECK(jla_on_lpx(&a, &side, o, 1) == 0);
    }
}

static void test_dn2_notes_light_the_pads(void)
{
    struct jla a = fresh();
    CHECK(colour(&a, 0, 0) == JLN_COLOR_ROOT);                    /* 48 is C: the root */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);                /* D */
    dn2(&a, 0x90, 50, 100);                                       /* the DN2 plays D (50) on channel 1 */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    CHECK(colour(&a, 0, 0) == JLN_COLOR_ROOT);
    dn2(&a, 0x80, 50, 0);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
    dn2(&a, 0x90, 50, 100);
    dn2(&a, 0x90, 50, 0);                                         /* a Note On with velocity 0 is an off */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_dn2_notes_per_channel(void)
{
    struct jla a = fresh();
    uint8_t o[4];
    dn2(&a, 0x91, 50, 100);                                       /* channel 2: not the current track */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
    press(&a, top(3, 127), o);                                    /* track 2: it is lit now */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    press(&a, top(2, 127), o);                                    /* back to track 1: dark again */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_dn2_same_pitch_lights_every_pad(void)
{
    struct jla a = fresh();
    jln_set_layout(&a.n, JLN_CHROMATIC);
    dn2(&a, 0x90, 53, 100);
    CHECK(colour(&a, 5, 0) == JLN_COLOR_PLAYING && colour(&a, 0, 1) == JLN_COLOR_PLAYING);
}

static void test_dn2_all_notes_off_and_noise(void)
{
    struct jla a = fresh();
    dn2(&a, 0x90, 50, 100);
    dn2(&a, 0x90, 52, 100);
    dn2(&a, 0xB0, 7, 100);                                        /* a volume CC: ignored */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    dn2(&a, 0xB0, 123, 0);                                        /* All Notes Off on channel 1 */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE && colour(&a, 2, 0) == JLN_COLOR_IN_SCALE);
    {
        const uint8_t clock[4] = {0x0F, 0xF8, 0, 0};              /* a clock byte */
        const uint8_t sysex[4] = {0x04, 0xF0, 0x7D, 0x01};
        const uint8_t pressure[4] = {0x0D, 0xD0, 50, 0};
        jla_on_dn2(&a, clock);
        jla_on_dn2(&a, sysex);
        jla_on_dn2(&a, pressure);
    }
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_own_press_lights_the_pad(void)
{
    struct jla a = fresh();
    uint8_t o[4];
    press(&a, pad(1, 0, 100), o);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    press(&a, pad(1, 0, 0), o);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_buttons_lit(void)
{
    struct jla a = fresh();
    struct jlx_frame f;
    jla_frame(&a, &f);
    for (uint32_t i = 0; i < 4; i++)
        CHECK(f.led[JLX_TOP0 + i].color == JLA_COLOR_ARROW);
    CHECK(f.led[JLX_TOP0 + 5].color == JLA_COLOR_MODE);          /* the Note button */
    CHECK(f.led[JLX_TOP0 + 4].color == 0 && f.led[JLX_TOP0 + 6].color == 0 && f.led[JLX_TOP0 + 7].color == 0);
    for (uint32_t i = 0; i < 8; i++)
        CHECK(f.led[JLX_SIDE0 + i].color == 0);
    CHECK(f.led[JLX_LOGO].color == 0);
    for (uint32_t i = 0; i < JLX_LEDS; i++)
        CHECK(f.led[i].mode == JLX_STATIC);
}

static void test_release_all(void)
{
    struct jla a = fresh();
    uint8_t o[4 * 8];
    uint8_t one[4];
    press(&a, pad(0, 0, 100), one);
    press(&a, pad(1, 0, 100), one);
    press(&a, top(3, 127), one);
    press(&a, pad(2, 0, 100), one);                               /* a third one, on channel 2 */
    CHECK(jla_release_all(&a, o, 8) == 3);
    CHECK(o[1] == 0x80 && o[2] == 48 && o[5] == 0x80 && o[6] == 50 && o[9] == 0x81 && o[10] == 52);
    CHECK(jla_release_all(&a, o, 8) == 0);                        /* all up now */
    CHECK(press(&a, pad(0, 0, 0), one) == 0);
}

static void test_release_all_respects_the_room(void)
{
    struct jla a = fresh();
    uint8_t o[4 * 2], one[4];
    press(&a, pad(0, 0, 100), one);
    press(&a, pad(1, 0, 100), one);
    press(&a, pad(2, 0, 100), one);
    CHECK(jla_release_all(&a, o, 2) == 2);
    CHECK(jla_release_all(&a, o, 2) == 1);                        /* the rest waits for the next call */
    CHECK(jla_release_all(&a, o, 2) == 0);
}

int main(void)
{
    test_pad_plays_note_on_track_channel();
    test_release_goes_to_the_note_it_started();
    test_release_goes_to_the_channel_it_started();
    test_same_pitch_on_two_pads();
    test_pad_outside_midi_range_is_silent();
    test_repeated_press_and_stray_release();
    test_buttons();
    test_dn2_notes_light_the_pads();
    test_dn2_notes_per_channel();
    test_dn2_same_pitch_lights_every_pad();
    test_dn2_all_notes_off_and_noise();
    test_own_press_lights_the_pad();
    test_buttons_lit();
    test_release_all();
    test_release_all_respects_the_room();
    printf("test_lpapp: %d checks, %d failed\n", checks, fails);
    return fails;
}
