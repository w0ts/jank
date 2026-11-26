/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of jank's Launchpad core built as another unit builds it (docs/launchpad.md, "Using the core on another unit"): tests/lpunit_unity.c, the FM-1's file set as
 * one unit, its own scales (tests/lpunit/), the clips page. tests/run.sh builds it with the FM-1's configuration (JLN_TRACKS 15, JLCL_TRACKS 4, no Record, no Delete) and
 * with 4 tracks; the checks follow the configuration. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "lpunit_unity.c"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define TOP_UP 0u
#define TOP_DOWN 1u
#define TOP_RIGHT 3u
#define TOP_NOTE 5u

static struct jlapp t_fresh(void)
{
    struct jlapp a;
    jlapp_init(&a, jls_get(JLS_MAJOR));
    jlapp_set_live(&a, JDL_LIVE, 1, 1);
    jlapp_tick(&a, 1000);
    return a;
}

static size_t t_ev(struct jlapp *a, uint8_t kind, uint8_t x, uint8_t y, uint8_t idx, uint8_t v, struct jlapp_act out[2])
{
    struct jlx_ev e = {kind, y, x, idx, v};
    memset(out, 0, 2 * sizeof *out);
    return jlapp_on_lpx_acts(a, &e, out, 2);
}

static size_t t_pad(struct jlapp *a, uint8_t x, uint8_t y, uint8_t v, struct jlapp_act out[2]) { return t_ev(a, JLX_PAD, x, y, 0, v, out); }

static size_t t_button(struct jlapp *a, uint8_t kind, uint8_t idx, uint8_t v)
{
    struct jlapp_act o[2];
    return t_ev(a, kind, 0, 0, idx, v, o);
}

static uint8_t t_colour(const struct jlapp *a, uint8_t x, uint8_t y)
{
    struct jlx_frame f;
    jlapp_frame(a, &f);
    return f.led[JLX_PAD0 + y * 8u + x].color;
}

static const char *t_msg(struct jlapp *a)
{
    static char m[JLAPP_MSG_MAX];
    if (!jlapp_take_message(a, m, sizeof m))
        m[0] = 0;
    return m;
}

/* the track a pad of the bottom rows stands for: 1-8 in the upper of two rows (one row up to 8 tracks) */
static unsigned t_track(uint8_t x, uint8_t y) { return (JLAPP_TRACK_ROWS - 1u - y) * 8u + x; }

static void test_the_configuration(void)
{
    CHECK(JLAPP_CLIPS == 1 && JLCL_SLOTS == 16u && JLCL_TRACKS <= 7u && JLN_TRACKS <= 16u);
    CHECK(JLAPP_TRACK_ROWS == (JLN_TRACKS > 8u ? 2u : 1u) && JLMX_BANKS == JLAPP_TRACK_ROWS);
    CHECK(jls_count() == 16u && jls_get(JLS_MINOR) && !strcmp(jls_get(JLS_MINOR)->name, "MIN") && jls_mask(JLS_MAJOR) == 0xAB5u);
    CHECK(!JLN_NO_TRACK(JLN_TRACKS - 1u) && JLN_NO_TRACK(JLN_TRACKS) == (JLN_TRACKS < 16u));   /* (constant 0 with 16) */
}

/* the track pads (TRIGGER and SELECT) and the select overlay: the unit's tracks, the rest dark and doing nothing */
static void test_track_pads_stop_at_the_units_tracks(void)
{
    struct jlapp a = t_fresh();
    struct jlapp_act o[2];
    a.track_pads = JLAPP_TRACKS_TRIGGER;
    for (uint8_t y = 0; y < JLAPP_TRACK_ROWS; y++)
        for (uint8_t x = 0; x < 8u; x++) {
            unsigned t = t_track(x, y);
            size_t n = t_pad(&a, x, y, 100, o);
            if (t < JLN_TRACKS) {
                CHECK(n == 1 && o[0].kind == JLAPP_ACT_TRACK_ON && o[0].track == t);
                CHECK(t_colour(&a, x, y) == JLAPP_COLOR_TRIG_PLAYING);
                CHECK(t_pad(&a, x, y, 0, o) == 1 && o[0].kind == JLAPP_ACT_TRACK_OFF && o[0].track == t);
                CHECK(t_colour(&a, x, y) == (t == 0 ? JLAPP_COLOR_TRIG_CURRENT : JLAPP_COLOR_TRIG_IDLE));
            } else {
                CHECK(n == 0 && a.track_held == 0 && t_colour(&a, x, y) == 0);
                CHECK(t_pad(&a, x, y, 0, o) == 0);
            }
        }
    a.track_pads = JLAPP_TRACKS_SELECT;
    for (uint8_t y = 0; y < JLAPP_TRACK_ROWS; y++)
        for (uint8_t x = 0; x < 8u; x++) {
            unsigned t = t_track(x, y), before = a.n.track;
            size_t n = t_pad(&a, x, y, 100, o);
            if (t < JLN_TRACKS)
                CHECK(a.n.track == t && n == (t != before) && (!n || (o[0].kind == JLAPP_ACT_SELECT && o[0].track == t)));
            else
                CHECK(n == 0 && a.n.track == before && t_colour(&a, x, y) == 0);
            CHECK(t_pad(&a, x, y, 0, o) == 0);
        }
    /* right to the last track: the right arrow dims there; the overlay shows the tracks, dark past them */
    a = t_fresh();
    for (unsigned i = 0; i < 20u; i++)
        t_button(&a, JLX_TOP, TOP_RIGHT, 127), t_button(&a, JLX_TOP, TOP_RIGHT, 0);
    {
        struct jlx_frame f;
        jlapp_frame(&a, &f);
        CHECK(a.n.track == JLN_TRACKS - 1u && f.led[JLX_TOP0 + TOP_RIGHT].color == JLAPP_COLOR_ARROW_OFF);
        for (uint8_t y = 0; y < JLAPP_TRACK_ROWS; y++)
            for (uint8_t x = 0; x < 8u; x++)
                CHECK((f.led[JLX_PAD0 + y * 8u + x].color == 0) == (t_track(x, y) >= JLN_TRACKS));
    }
    CHECK(JLAPP_TRACK_ROWS == 1u || t_colour(&a, 0, JLAPP_TRACK_ROWS) != 0);   /* (above the overlay: notes) */
}

/* the scale page: its track rows select the unit's tracks; its scales are the unit's 16, one page pad */
static void test_the_scale_page(void)
{
    struct jlapp a = t_fresh();
    struct jlapp_act o[2];
    t_button(&a, JLX_TOP, TOP_NOTE, 127);
    for (uint8_t y = 0; y < JLAPP_TRACK_ROWS; y++)
        for (uint8_t x = 0; x < 8u; x++) {
            unsigned t = t_track(x, y), before = a.n.track;
            size_t n = t_pad(&a, x, y, 100, o);
            if (t < JLN_TRACKS) {
                CHECK(a.n.track == t && n == (t != before ? 1u : 0u) && t_colour(&a, x, y) == JLAPP_COLOR_TRACK_CURRENT);
            } else {
                CHECK(n == 0 && a.n.track == before && t_colour(&a, x, y) == 0);
            }
        }
    CHECK(t_colour(&a, 0, 4) == JLAPP_AMBER && t_colour(&a, 1, 4) == 0 && t_colour(&a, 2, 4) == 0);   /* one page pad: 16 scales */
    CHECK(t_colour(&a, 1, 3) == JLAPP_BLUE && t_colour(&a, 0, 3) == JLAPP_BLUE_THIRD);                 /* MAJ (1) is on, CHR (0) off */
    CHECK(t_pad(&a, 2, 6, 100, o) == 1 && o[0].kind == JLAPP_ACT_SCALE && o[0].note == JLS_MINOR && o[0].vel == 9);   /* the relative scale: A MIN, the unit's index */
    CHECK(!strcmp(t_msg(&a), "A MIN"));
    CHECK(t_pad(&a, 7, 2, 100, o) == 1 && o[0].note == 15 && !strcmp(t_msg(&a), "SCALE DIMWH"));
    CHECK(t_pad(&a, 1, 4, 100, o) == 0 && !strcmp(t_msg(&a), ""));             /* no second page */
}

/* the drum step quadrant: the unit's tracks, the rest dark and doing nothing */
static void test_the_drum_quadrant(void)
{
    struct jlapp a = t_fresh();
    struct jlapp_act o[2];
    t_button(&a, JLX_TOP, 6, 127);
    t_button(&a, JLX_TOP, 6, 127);
    CHECK(a.mode == JLAPP_MODE_STEP && a.step_kind == JLST_DRUM);
    for (uint8_t y = 0; y < 4u; y++)
        for (uint8_t x = 0; x < 4u; x++) {
            unsigned t = (3u - y) * 4u + x, before = a.n.track;
            size_t n = t_pad(&a, x, y, 100, o);
            if (t < JLN_TRACKS) {
                CHECK(a.n.track == t && n == (t != before ? 2u : 1u) && o[n - 1u].kind == JLAPP_ACT_TRACK_ON && o[n - 1u].track == t);
                CHECK(t_colour(&a, x, y) == JLST_COLOR_TRACK_PLAYING);
                CHECK(t_pad(&a, x, y, 0, o) == 1 && o[0].kind == JLAPP_ACT_TRACK_OFF);
                CHECK(t_colour(&a, x, y) == JLST_COLOR_TRACK_CURRENT);
            } else {
                CHECK(n == 0 && a.n.track == before && t_colour(&a, x, y) == 0 && t_pad(&a, x, y, 0, o) == 0);
            }
        }
}

static void t_mixer(struct jlapp *a)
{
    struct jdl_mixer m;
    memset(&m, 0, sizeof m);
    for (unsigned t = 0; t < JDL_TRACKS; t++)
        m.t[t].level = m.t[t].pan = 64;
    t_button(a, JLX_TOP, 7, 127);
    jlapp_on_mixer(a, &m);
}

/* the mixer: one bank of 8 per 8 tracks (8 + 7 for 15), columns and mute / solo pads past the last track dark and doing nothing */
static void test_the_mixer(void)
{
    struct jlapp a = t_fresh();
    struct jlapp_act o[2];
    struct jlx_frame f;
    t_mixer(&a);
    t_msg(&a);
    jlapp_frame(&a, &f);
    CHECK(a.mode == JLAPP_MODE_MIXER && f.led[JLX_TOP0 + TOP_UP].color == JLAPP_COLOR_ARROW_OFF);
    CHECK(f.led[JLX_TOP0 + TOP_DOWN].color == (JLMX_BANKS > 1u ? JLAPP_COLOR_ARROW_ON : JLAPP_COLOR_ARROW_OFF));
    for (unsigned b = 0; b < JLMX_BANKS; b++) {
        for (uint8_t x = 0; x < 8u; x++) {
            unsigned t = b * 8u + x;
            size_t n = t_pad(&a, x, 2, 100, o);                    /* the overview's mute row */
            if (t < JLN_TRACKS) {
                CHECK(n == 1 && o[0].kind == JLAPP_ACT_MUTE && (unsigned)(o[0].note | o[0].step << 8) == 1u << t && t_colour(&a, x, 7) == JLMX_COLOR_VOL);
                t_pad(&a, x, 2, 100, o);           /* back */
            } else {
                CHECK(n == 0 && t_colour(&a, x, 7) == 0 && t_colour(&a, x, 2) == 0);
            }
        }
        t_button(&a, JLX_TOP, TOP_DOWN, 127);
        if (b + 1u < JLMX_BANKS) {
            char want[JLAPP_MSG_MAX];
            snprintf(want, sizeof want, "TRACKS 9-%u", (unsigned)JLN_TRACKS);
            CHECK(!strcmp(t_msg(&a), want));
        } else {
            CHECK(!strcmp(t_msg(&a), ""));                         /* no bank further */
        }
    }
    t_button(&a, JLX_SIDE, 5, 127);                                /* scene 6: mute and solo, the 4 lower rows: mute 1-8, 9-16, solo 1-8, 9-16 */
    CHECK(a.mix.view == JLMX_MUTE);
    for (uint8_t y = 0; y < 4u; y++)
        for (uint8_t x = 0; x < 8u; x++) {
            unsigned t = (y & 1u ? 0u : 8u) + x;
            size_t n = t_pad(&a, x, y, 100, o);
            CHECK(n == (t < JLN_TRACKS ? 1u : 0u) && (t < JLN_TRACKS) == (t_colour(&a, x, y) != 0));
            if (n)
                CHECK(o[0].kind == JLAPP_ACT_MUTE && o[0].op == (y < 2u) && (unsigned)(o[0].note | o[0].step << 8) == 1u << t);
            if (n)
                t_pad(&a, x, y, 100, o);                           /* back */
        }
}

static struct jlcl_track t_clip(uint8_t playing, uint8_t queued, uint16_t filled)
{
    struct jlcl_track c = {playing, queued, filled};
    return c;
}

/* the clips page: the unit's tracks in columns, 16 slots in two halves of 8, the scenes on the right, lights from the unit's news */
static void test_the_clips_page(void)
{
    struct jlapp a = t_fresh();
    struct jlapp_act o[2];
    struct jlcl_track c[JLCL_TRACKS];
    struct jlx_frame f;
    memset(c, 0, sizeof c);
    for (unsigned t = 0; t < JLCL_TRACKS; t++)
        c[t] = t_clip(JLCL_NONE, JLCL_NONE, 0);
    jlapp_frame(&a, &f);
    t_button(&a, JLX_TOP, 4, 127);
    t_button(&a, JLX_TOP, 4, 0);                                   /* a short press: the page */
    CHECK(a.mode == JLAPP_MODE_SESSION && !strcmp(t_msg(&a), "SESSION SLOTS 1-8"));
    for (uint8_t y = 0; y < 8u; y++)
        for (uint8_t x = 0; x < 8u; x++)
            CHECK(t_colour(&a, x, y) == 0);                        /* nothing playing yet (jlapp_init: none) */
    c[0] = t_clip(2, JLCL_NONE, 1u << 0 | 1u << 2 | 1u << 9);
    if (JLCL_TRACKS > 1u)
        c[1] = t_clip(JLCL_NONE, 5, 1u << 5);
    if (JLCL_TRACKS > 2u)
        c[2] = t_clip(3, JLCL_STOP, 1u << 3);
    jlapp_on_clips(&a, c, 1u << 0 | 1u << 9);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_PAD0 + 5 * 8 + 0].color == JLCL_COLOR_PLAYING && f.led[JLX_PAD0 + 5 * 8 + 0].mode == JLX_PULSE);   /* slot 3 of track 1 plays */
    CHECK(f.led[JLX_PAD0 + 7 * 8 + 0].color == JLCL_COLOR_FILLED && f.led[JLX_PAD0 + 7 * 8 + 0].mode == JLX_STATIC);
    CHECK(f.led[JLX_PAD0 + 6 * 8 + 0].color == 0);
    CHECK(JLCL_TRACKS < 2u || (f.led[JLX_PAD0 + 2 * 8 + 1].color == JLCL_COLOR_PLAYING && f.led[JLX_PAD0 + 2 * 8 + 1].mode == JLX_FLASH && f.led[JLX_PAD0 + 2 * 8 + 1].alt == 0));
    CHECK(JLCL_TRACKS < 3u || (f.led[JLX_PAD0 + 4 * 8 + 2].color == JLCL_COLOR_PLAYING && f.led[JLX_PAD0 + 4 * 8 + 2].mode == JLX_FLASH));   /* a stop queued */
    CHECK(f.led[JLX_PAD0 + 7 * 8 + 7].color == JLCL_COLOR_SCENE && f.led[JLX_PAD0 + 6 * 8 + 7].color == 0);
    for (uint8_t y = 0; y < 8u; y++)
        for (uint8_t x = JLCL_TRACKS; x < 7u; x++)
            CHECK(f.led[JLX_PAD0 + y * 8u + x].color == 0);        /* no track there */
    CHECK(f.led[JLX_TOP0 + TOP_UP].color == JLAPP_COLOR_ARROW_OFF && f.led[JLX_TOP0 + TOP_DOWN].color == JLAPP_COLOR_ARROW_ON);
    /* presses: a slot, the playing slot (a stop), a scene, a column with no track, a release */
    CHECK(t_pad(&a, 0, 3, 100, o) == 1 && o[0].kind == JLAPP_ACT_CLIP && o[0].track == 0 && o[0].note == 4 && o[0].vel == 0 && !strcmp(t_msg(&a), "TRACK 1 CLIP 5"));
    CHECK(t_pad(&a, 0, 3, 0, o) == 0);
    CHECK(t_pad(&a, 0, 5, 100, o) == 1 && o[0].kind == JLAPP_ACT_CLIP && o[0].note == JLCL_NONE && !strcmp(t_msg(&a), "TRACK 1 STOP"));
    t_pad(&a, 0, 5, 0, o);
    CHECK(t_pad(&a, 7, 6, 100, o) == 1 && o[0].kind == JLAPP_ACT_SCENE && o[0].note == 1 && !strcmp(t_msg(&a), "SCENE 2"));
    t_pad(&a, 7, 6, 0, o);
    CHECK(JLCL_TRACKS > 6u || t_pad(&a, 6, 0, 100, o) == 0);
    CHECK(JLCL_TRACKS < 4u || (t_pad(&a, JLCL_TRACKS - 1u, 0, 100, o) == 1 && o[0].track == JLCL_TRACKS - 1u && o[0].note == 7));
    /* down: slots and scenes 9-16 */
    t_button(&a, JLX_TOP, TOP_DOWN, 127);
    CHECK(a.sess.half == 1 && !strcmp(t_msg(&a), "SLOTS 9-16"));
    t_button(&a, JLX_TOP, TOP_DOWN, 127);
    CHECK(!strcmp(t_msg(&a), ""));
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_PAD0 + 6 * 8 + 0].color == JLCL_COLOR_FILLED && f.led[JLX_PAD0 + 6 * 8 + 7].color == JLCL_COLOR_SCENE);   /* slot 10, scene 10 */
    CHECK(f.led[JLX_PAD0 + 5 * 8 + 0].color == 0 && f.led[JLX_PAD0 + 7 * 8 + 7].color == 0);
    CHECK(f.led[JLX_TOP0 + TOP_UP].color == JLAPP_COLOR_ARROW_ON && f.led[JLX_TOP0 + TOP_DOWN].color == JLAPP_COLOR_ARROW_OFF);
    CHECK(t_pad(&a, 0, 0, 100, o) == 1 && o[0].note == 15 && !strcmp(t_msg(&a), "TRACK 1 CLIP 16"));
    t_pad(&a, 0, 0, 0, o);
    CHECK(t_pad(&a, 7, 7, 100, o) == 1 && o[0].kind == JLAPP_ACT_SCENE && o[0].note == 8);
    t_pad(&a, 7, 7, 0, o);
    t_button(&a, JLX_TOP, TOP_UP, 127);
    CHECK(a.sess.half == 0 && !strcmp(t_msg(&a), "SLOTS 1-8"));
    /* the scene buttons on the page: start / stop only, the unit's transport */
    CHECK(t_ev(&a, JLX_SIDE, 0, 0, JLAPP_SIDE_START_STOP, 127, o) == 1 && o[0].kind == JLAPP_ACT_TRANSPORT && !strcmp(t_msg(&a), "START"));
    jlapp_set_transport(&a, 1);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_START_STOP].color == JLAPP_GREEN && f.led[JLX_SIDE0 + JLAPP_SIDE_DELETE].color == 0);
    t_button(&a, JLX_TOP, 4, 127);
    t_button(&a, JLX_TOP, 4, 0);
    CHECK(a.mode == JLAPP_MODE_NOTE && !strcmp(t_msg(&a), "SESSION OFF"));
}

/* a note held when the page opens still ends there */
static void test_a_note_held_into_the_clips_page_ends(void)
{
    struct jlapp a = t_fresh();
    struct jlapp_act o[2];
    CHECK(t_pad(&a, 0, 3, 100, o) == 1 && o[0].kind == JLAPP_ACT_NOTE_ON);
    t_button(&a, JLX_TOP, 4, 127);
    t_button(&a, JLX_TOP, 4, 0);
    CHECK(a.mode == JLAPP_MODE_SESSION && t_pad(&a, 0, 3, 0, o) == 1 && o[0].kind == JLAPP_ACT_NOTE_OFF);
}

/* no Record, no Delete (JLAPP_RECORD 0, JLAPP_ERASE 0): dark, doing nothing */
static void test_record_and_delete_absent(void)
{
    struct jlapp a = t_fresh();
    struct jlapp_act o[2];
    struct jlx_frame f;
    if (JLAPP_RECORD || JLAPP_ERASE)
        return;
    CHECK(t_ev(&a, JLX_SIDE, 0, 0, JLAPP_SIDE_DELETE, 127, o) == 0 && t_ev(&a, JLX_SIDE, 0, 0, JLAPP_SIDE_DELETE, 0, o) == 0 && !a.erase_on);
    CHECK(t_ev(&a, JLX_SIDE, 0, 0, JLAPP_SIDE_RECORD, 127, o) == 0 && !strcmp(t_msg(&a), ""));
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_DELETE].color == 0 && f.led[JLX_SIDE0 + JLAPP_SIDE_RECORD].color == 0);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_START_STOP].color == JLAPP_GREEN_THIRD);
}

/* the messages' own formatter (no stdio): %s, %u, upper case, 7-bit, cut at 20 */
static void test_the_formatter(void)
{
    struct jlapp a = t_fresh();
    char m[JLAPP_MSG_MAX];
    jlapp_say(&a, "ab %s=%u", "cD", 0u);
    CHECK(!strcmp(t_msg(&a), "AB CD=0"));
    jlapp_say(&a, "%u %u", 4294967295u, 10u);
    CHECK(!strcmp(t_msg(&a), "4294967295 10"));
    jlapp_say(&a, "%s", "a long message that is cut");
    CHECK(!strcmp(t_msg(&a), "A LONG MESSAGE THAT ") && strlen("A LONG MESSAGE THAT ") == JLAPP_MSG_MAX - 1u);
    jlapp_say(&a, "TAB\tHERE\x80");
    CHECK(!strcmp(t_msg(&a), "TAB?HERE?"));
    jlapp_say(&a, "x%uy", 123u);
    CHECK(jlapp_take_message(&a, m, 3) == 2 && !strcmp(m, "X1") && jlapp_take_message(&a, m, sizeof m) == 0);
    CHECK(jlx_has("Launchpad X MIDI", "Launchpad X") && !jlx_has("Launchpad", "Launchpad X") && jlx_has("", "") && jlx_has("ab", "") && !jlx_has("", "a"));
}

/* The FM-1 (Optimist 2026-11-26: "the X only", fixed velocity and the remembered settings out): the X alone is found, the other models are not; the scale page's pad 3 is
 * dark and does nothing, a pad plays at its press's velocity. The 4-track build keeps them (jank's defaults) */
static void test_x_only_and_no_fixed_velocity(void)
{
    struct jlapp a = t_fresh();
    struct jlapp_act o[2];
    CHECK(jlx_model_find("Launchpad X MIDI") == &jlx_models[0] && !jlx_model_find("Launchpad Pro MK3"));
    CHECK((jlx_model_find("Launchpad Mini MK3") == NULL) == JLX_ONLY_X && JLX_MODELS == (JLX_ONLY_X ? 1u : 4u));
    t_button(&a, JLX_TOP, TOP_NOTE, 127);
    t_msg(&a);
    CHECK(t_pad(&a, 2, 7, 100, o) == 0 && (t_colour(&a, 2, 7) == 0) == !JLAPP_FIXED_VEL && !strcmp(t_msg(&a), JLAPP_FIXED_VEL ? "FIXED VELOCITY ON" : ""));
    t_button(&a, JLX_TOP, TOP_NOTE, 0);
    a.no_vel = 1;                      /* (a Mini MK3's pads: fixed unless the X alone is built) */
    CHECK(t_pad(&a, 0, JLAPP_TRACK_ROWS + 1u, 77, o) == 1 && o[0].kind == JLAPP_ACT_NOTE_ON && o[0].vel == (JLAPP_FIXED_VEL || !JLX_ONLY_X ? 0 : 77));
}

int main(void)
{
    test_the_configuration();
    test_track_pads_stop_at_the_units_tracks();
    test_the_scale_page();
    test_the_drum_quadrant();
    test_the_mixer();
    test_the_clips_page();
    test_a_note_held_into_the_clips_page_ends();
    test_record_and_delete_absent();
    test_the_formatter();
    test_x_only_and_no_fixed_velocity();
    printf("test_lpunit (%u tracks, %u clip columns): %d checks, %d failed\n", (unsigned)JLN_TRACKS, (unsigned)JLCL_TRACKS, checks, fails);
    return fails;
}
