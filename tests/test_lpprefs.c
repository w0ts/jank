/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the Launchpad part's remembered settings (core/jank_lpprefs.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpprefs.h"
#include "jank_lpscales.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static struct jlapp app(void)
{
    struct jlapp a;
    jlapp_init(&a, jls_get(JLS_MAJOR));
    return a;
}

static void test_the_defaults_are_the_apps_starting_state(void)
{
    struct jlapp a = app();
    struct jlprefs d, now;
    jlprefs_defaults(&d);
    jlprefs_from_app(&a, &now);
    CHECK(jlprefs_equal(&d, &now));                                /* a fresh app is the defaults: nothing to restore, nothing to write */
    CHECK(d.layout == JLN_DIATONIC && d.horizontal == 1 && d.absolute == 0 && d.octave == 3 && d.track_pads == 0 && d.step_kind == JLST_MELODIC && d.mix_view == JLMX_VOL &&
          d.follow == 0);
}

static void test_pack_and_unpack_round_trip(void)
{
    struct jlprefs p, q;
    uint8_t b[16];
    jlprefs_defaults(&p);
    p.layout = JLN_CHORDS;
    p.horizontal = 0;
    p.absolute = 1;
    p.octave = 5;
    p.track_pads = 1;
    p.step_kind = JLST_DRUM;
    p.mix_view = JLMX_SOLO;
    p.follow = 1;
    CHECK(jlprefs_pack(&p, b, sizeof b) == JLPREFS_SIZE && b[0] == JLPREFS_VERSION);
    CHECK(jlprefs_unpack(&q, b, JLPREFS_SIZE) && jlprefs_equal(&p, &q));
    CHECK(jlprefs_pack(&p, b, JLPREFS_SIZE - 1u) == 0);            /* no room */
}

static void test_a_blob_that_is_not_ours_is_not_used(void)
{
    struct jlprefs p, keep, q;
    uint8_t b[JLPREFS_SIZE], c[JLPREFS_SIZE];
    jlprefs_defaults(&p);
    p.track_pads = 1;
    jlprefs_pack(&p, b, sizeof b);
    jlprefs_defaults(&keep);
    q = keep;
    CHECK(!jlprefs_unpack(&q, b, JLPREFS_SIZE - 1u) && jlprefs_equal(&q, &keep));           /* short */
    CHECK(!jlprefs_unpack(&q, b, JLPREFS_SIZE + 1u) && jlprefs_equal(&q, &keep));           /* long */
    CHECK(!jlprefs_unpack(&q, b, 0) && jlprefs_equal(&q, &keep));
    memcpy(c, b, sizeof b);
    c[0] = 2;                                                      /* another version */
    CHECK(!jlprefs_unpack(&q, c, sizeof c) && jlprefs_equal(&q, &keep));
    for (unsigned i = 1; i < JLPREFS_SIZE; i++) {                  /* any one byte damaged: the checksum says */
        memcpy(c, b, sizeof b);
        c[i] ^= 0x10u;
        CHECK(!jlprefs_unpack(&q, c, sizeof c) && jlprefs_equal(&q, &keep));
    }
}

static void test_a_blob_with_a_value_out_of_range_is_not_used(void)
{
    static const uint8_t LIMIT[8] = {JLN_CHORDS, 1, 1, 6, 1, JLST_DRUM, JLMX_VIEWS - 1u, 1};
    struct jlprefs p, q;
    uint8_t b[JLPREFS_SIZE];
    for (unsigned field = 0; field < 8u; field++) {
        jlprefs_defaults(&p);
        jlprefs_pack(&p, b, sizeof b);
        b[1 + field] = (uint8_t)(LIMIT[field] + 1u);               /* one past the largest allowed (with a checksum that matches, as a buggy writer would make it) */
        {
            uint8_t x = 0xA5u;
            for (unsigned i = 0; i < 9u; i++)
                x = (uint8_t)((x << 1 | x >> 7) ^ b[i]);
            b[9] = x;
        }
        jlprefs_defaults(&q);
        CHECK(!jlprefs_unpack(&q, b, sizeof b));
        b[1 + field] = LIMIT[field];                               /* the largest allowed itself is fine */
        {
            uint8_t x = 0xA5u;
            for (unsigned i = 0; i < 9u; i++)
                x = (uint8_t)((x << 1 | x >> 7) ^ b[i]);
            b[9] = x;
        }
        CHECK(jlprefs_unpack(&q, b, sizeof b));
    }
}

static void test_the_apps_settings_survive_a_restart(void)
{
    struct jlapp a = app(), b = app();
    struct jlprefs p, q;
    uint8_t blob[JLPREFS_SIZE];
    jln_set_layout(&a.n, JLN_CHROMATIC);
    a.n.octave = 6;                                                /* chromatic has 7 octaves */
    a.n.absolute = 1;
    a.track_pads = 1;
    a.step_kind = JLST_DRUM;
    a.mix.view = JLMX_PAN;
    a.follow = 1;
    jlprefs_from_app(&a, &p);
    jlprefs_pack(&p, blob, sizeof blob);                           /* ... the unit restarts ... */
    CHECK(jlprefs_unpack(&q, blob, sizeof blob));
    jlprefs_apply(&b, &q);
    CHECK(b.n.layout == JLN_CHROMATIC && b.n.octave == 6 && b.n.absolute == 1 && b.track_pads == 1 && b.step_kind == JLST_DRUM && b.step.kind == JLST_DRUM &&
          b.mix.view == JLMX_PAN && b.follow == 1);
    jlprefs_from_app(&b, &q);
    CHECK(jlprefs_equal(&p, &q));                                  /* and nothing looks changed: no write for it */
}

static void test_chords_keep_their_own_orientation_and_the_diatonic_switch_is_restored(void)
{
    struct jlapp a = app(), b = app();
    struct jlprefs p;
    jln_set_layout(&a.n, JLN_CHORDS);
    jlprefs_from_app(&a, &p);
    jlprefs_apply(&b, &p);
    CHECK(b.n.layout == JLN_CHORDS && b.n.horizontal == 0);        /* chords: vertical */
    a = app();
    a.n.horizontal = 0;                                            /* diatonic with Horizontal switched off */
    jlprefs_from_app(&a, &p);
    jlprefs_apply(&b, &p);
    CHECK(b.n.layout == JLN_DIATONIC && b.n.horizontal == 0);
}

static void test_an_octave_a_layout_does_not_have_is_pulled_under_its_top(void)
{
    struct jlapp b = app();
    struct jlprefs p;
    jlprefs_defaults(&p);
    p.layout = JLN_DIATONIC;
    p.octave = 6;                                                  /* diatonic has octaves 0..5 (a hand-made blob: the app never writes it) */
    jlprefs_apply(&b, &p);
    CHECK(b.n.octave == 3);                                        /* the octave it already had stays: an impossible place changes nothing */
}

static void test_the_scale_and_the_key_are_not_remembered(void)
{
    struct jlapp a = app(), b = app();
    struct jlprefs p;
    a.scale = jls_get(11);
    a.n.key = 7;
    jlprefs_from_app(&a, &p);
    jlprefs_apply(&b, &p);
    CHECK(jls_index_of(b.scale) == JLS_MAJOR && b.n.key == 0);     /* they come from the Digitone */
}

int main(void)
{
    test_the_defaults_are_the_apps_starting_state();
    test_pack_and_unpack_round_trip();
    test_a_blob_that_is_not_ours_is_not_used();
    test_a_blob_with_a_value_out_of_range_is_not_used();
    test_the_apps_settings_survive_a_restart();
    test_chords_keep_their_own_orientation_and_the_diatonic_switch_is_restored();
    test_an_octave_a_layout_does_not_have_is_pulled_under_its_top();
    test_the_scale_and_the_key_are_not_remembered();
    printf("test_lpprefs: %d checks, %d failed\n", checks, fails);
    return fails;
}
