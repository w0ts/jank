/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the step sequencer modes (core/jank_lpstep.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpstep.h"

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

struct rig {
    struct jlst s;
    struct jln n;
    struct jlst_ctx c;
    struct jlapp_recent recent[8];
};

static void rig_init(struct rig *r, enum jlst_kind kind)
{
    jlst_init(&r->s, kind);
    jln_init(&r->n);
    r->c.n = &r->n;
    r->c.scale = &MAJOR;
    r->c.can_play = 1;
    r->c.transport_playing = 0;
    r->c.now_ms = 1000;
    r->c.recent = r->recent;
    r->c.recent_n = 8;
    memset(r->recent, 0, sizeof r->recent);
}

/* the Digitone played `note` on `track` at `now` (what jlapp_on_push records) */
static void played(struct rig *r, uint8_t slot, uint8_t track, uint8_t note, uint32_t now)
{
    r->recent[slot].used = 1;
    r->recent[slot].track = track;
    r->recent[slot].note = note;
    r->recent[slot].at_ms = now;
}

/* the Digitone's answer for `track`, `page` of a pattern `pages` long: trig where (step + page * 16) % 4 == 0 when `every4`, note 40 + step */
static struct jdl_page page_of(uint8_t track, uint8_t page, uint8_t pages, int every4)
{
    struct jdl_page pg;
    memset(&pg, 0, sizeof pg);
    pg.track = track;
    pg.page = page;
    pg.pages = pages;
    pg.length = (uint8_t)(pages * 16u);
    for (unsigned i = 0; i < 16; i++) {
        pg.step[i].trig = every4 && i % 4 == 0;
        pg.step[i].note = (uint8_t)(40 + i);
        pg.step[i].vel = 100;
        pg.step[i].len = 14;
    }
    return pg;
}

static void feed(struct rig *r, uint8_t page, uint8_t pages, int every4)
{
    struct jdl_page pg = page_of(r->n.track, page, pages, every4);
    jlst_on_page(&r->s, r->n.track, &pg, r->c.now_ms);
}

static size_t press(struct rig *r, uint8_t x, uint8_t y, uint8_t v, struct jlapp_act *o, size_t max)
{
    return jlst_pad(&r->s, &r->c, x, y, v, o, max);
}

static uint8_t colour(struct rig *r, uint8_t x, uint8_t y)
{
    struct jlx_frame f;
    memset(&f, 0, sizeof f);
    jlst_frame(&r->s, &r->c, &f);
    return f.led[y * 8u + x].color;
}

static void test_a_step_does_nothing_until_its_page_is_read(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_MELODIC);
    CHECK(press(&r, 0, 7, 100, o, 2) == 0);                       /* nothing read yet: nothing to toggle */
    CHECK(colour(&r, 0, 7) == 0);                                 /* and the pad is dark */
    feed(&r, 0, 4, 1);
    CHECK(press(&r, 0, 7, 100, o, 2) == 1);
}

static void test_a_step_press_toggles_the_trig(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    feed(&r, 0, 4, 1);                                            /* trigs on steps 0, 4, 8, 12 */
    CHECK(press(&r, 1, 7, 100, o, 2) == 1 && o[0].kind == JLAPP_ACT_STEP && o[0].track == 0 && o[0].step == 1 && o[0].op == JDL_STEP_TRIG && o[0].vel == 255 && o[0].len == 255);
    CHECK(colour(&r, 1, 7) == JLST_COLOR_ON);                     /* shown at once */
    CHECK(press(&r, 1, 7, 0, o, 2) == 0);                         /* a release is nothing */
    CHECK(press(&r, 1, 7, 100, o, 2) == 1 && o[0].op == JDL_STEP_CLEAR && o[0].step == 1);
    CHECK(colour(&r, 1, 7) == JLST_COLOR_OFF);
    CHECK(press(&r, 0, 7, 100, o, 2) == 1 && o[0].op == JDL_STEP_CLEAR && o[0].step == 0);   /* a step that had a trig */
    CHECK(press(&r, 0, 7, 100, o, 0) == 0);                       /* no room: nothing changes */
    CHECK(colour(&r, 0, 7) == JLST_COLOR_OFF);
}

static void test_the_step_pads_are_the_digitones_layout(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    feed(&r, 0, 4, 0);
    feed(&r, 1, 4, 0);
    CHECK(press(&r, 7, 7, 100, o, 2) == 1 && o[0].step == 7);     /* the top row: steps 1..8 */
    CHECK(press(&r, 0, 6, 100, o, 2) == 1 && o[0].step == 8);     /* the second row: 9..16 */
    CHECK(press(&r, 7, 6, 100, o, 2) == 1 && o[0].step == 15);
    CHECK(press(&r, 0, 5, 100, o, 2) == 1 && o[0].step == 16);    /* then the next page */
    CHECK(press(&r, 7, 4, 100, o, 2) == 1 && o[0].step == 31);
}

static void test_a_melodic_step_with_a_key_picked(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_MELODIC);
    r.c.can_play = 0;                                             /* no sound from the keys: just picking */
    feed(&r, 0, 4, 1);
    CHECK(press(&r, 0, 0, 100, o, 2) == 0 && r.s.note == 48);     /* the bottom left key: C (octave 3), picked, silent */
    CHECK(press(&r, 0, 0, 0, o, 2) == 0);
    CHECK(press(&r, 1, 7, 100, o, 2) == 1 && o[0].op == JDL_STEP_NOTE && o[0].note == 48 && o[0].step == 1);   /* an empty step gets the trig with the note */
    CHECK(press(&r, 1, 7, 100, o, 2) == 1 && o[0].op == JDL_STEP_CLEAR);   /* the same note again: off */
    CHECK(press(&r, 0, 7, 100, o, 2) == 1 && o[0].op == JDL_STEP_NOTE && o[0].note == 48 && o[0].step == 0);   /* a step with another note (40): the picked note replaces it */
    CHECK(press(&r, 0, 7, 100, o, 2) == 1 && o[0].op == JDL_STEP_CLEAR);   /* now it is 48: off */
    press(&r, 1, 0, 100, o, 2);                                   /* the next key, D (50) */
    CHECK(r.s.note == 50);
    CHECK(press(&r, 2, 7, 100, o, 2) == 1 && o[0].op == JDL_STEP_NOTE && o[0].note == 50);
}

static void test_the_keys_play_only_while_the_transport_is_stopped(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_MELODIC);
    CHECK(press(&r, 1, 0, 90, o, 2) == 1 && o[0].kind == JLAPP_ACT_NOTE_ON && o[0].track == 0 && o[0].note == 50 && o[0].vel == 90);   /* stopped: it plays */
    CHECK(colour(&r, 1, 0) == JLST_COLOR_TRACK_PLAYING);          /* a key that is down: RED, as in the drum quadrant */
    CHECK(press(&r, 1, 0, 0, o, 2) == 1 && o[0].kind == JLAPP_ACT_NOTE_OFF && o[0].note == 50);
    r.c.transport_playing = 1;
    CHECK(press(&r, 2, 0, 90, o, 2) == 0 && r.s.note == 52);      /* playing: only picks the note */
    CHECK(press(&r, 2, 0, 0, o, 2) == 0);
    r.c.transport_playing = 0;
    press(&r, 3, 0, 90, o, 2);                                    /* down while stopped ... */
    r.c.transport_playing = 1;
    CHECK(press(&r, 3, 0, 0, o, 2) == 1 && o[0].kind == JLAPP_ACT_NOTE_OFF);   /* ... the transport starts: its end still goes */
}

static void test_the_drum_quadrant_selects_the_track(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    CHECK(press(&r, 0, 3, 100, o, 2) == 1 && o[0].kind == JLAPP_ACT_TRACK_ON && o[0].track == 0 && r.n.track == 0);   /* track 1: already the track: only the sound */
    CHECK(press(&r, 0, 3, 0, o, 2) == 1 && o[0].kind == JLAPP_ACT_TRACK_OFF && o[0].track == 0);
    CHECK(press(&r, 3, 0, 100, o, 2) == 2 && o[0].kind == JLAPP_ACT_SELECT && o[0].track == 15 && o[1].kind == JLAPP_ACT_TRACK_ON && o[1].track == 15 && r.n.track == 15);   /* the corner: 16 */
    CHECK(press(&r, 3, 0, 0, o, 2) == 1 && o[0].kind == JLAPP_ACT_TRACK_OFF && o[0].track == 15);
    CHECK(press(&r, 1, 2, 100, o, 2) == 2 && r.n.track == 5);     /* row 2 of the quadrant, column 2: track 6 (5 + 1: reading order 4 + 1) */
    press(&r, 1, 2, 0, o, 2);
    CHECK(press(&r, 0, 0, 100, o, 1) == 0 && r.n.track == 5);     /* no room for both: nothing changes */
    CHECK(press(&r, 0, 0, 100, o, 2) == 2 && r.n.track == 12);
}

static void test_the_drum_quadrant_only_selects_while_the_transport_runs(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    r.c.transport_playing = 1;
    CHECK(press(&r, 2, 3, 100, o, 2) == 1 && o[0].kind == JLAPP_ACT_SELECT && o[0].track == 2 && r.n.track == 2);
    CHECK(press(&r, 2, 3, 0, o, 2) == 0);
    r.c.transport_playing = 0;
    r.c.can_play = 0;                                             /* no PLAY: still selects */
    CHECK(press(&r, 3, 3, 100, o, 2) == 1 && o[0].kind == JLAPP_ACT_SELECT && o[0].track == 3);
}

static void test_the_drum_quadrant_lights(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    CHECK(colour(&r, 0, 3) == JLST_COLOR_TRACK_CURRENT && colour(&r, 1, 3) == JLST_COLOR_TRACK_IDLE && colour(&r, 3, 0) == JLST_COLOR_TRACK_IDLE);
    press(&r, 3, 0, 100, o, 2);
    CHECK(colour(&r, 3, 0) == JLST_COLOR_TRACK_PLAYING);          /* held: it sounds, RED as Launchpad 95's playing note */
    press(&r, 3, 0, 0, o, 2);
    CHECK(colour(&r, 3, 0) == JLST_COLOR_TRACK_CURRENT && colour(&r, 0, 3) == JLST_COLOR_TRACK_IDLE);
    CHECK(JLST_COLOR_TRACK_IDLE == 13 && JLST_COLOR_TRACK_CURRENT == 21 && JLST_COLOR_TRACK_PLAYING == 5);   /* YELLOW, GREEN, RED */
}

static void test_the_melodic_keyboard_lights(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_MELODIC);
    CHECK(colour(&r, 0, 0) == JLN_COLOR_ROOT && colour(&r, 1, 0) == JLN_COLOR_IN_SCALE);    /* the Note mode's colours: C blue, D light blue */
    r.c.can_play = 0;
    press(&r, 1, 0, 100, o, 2);
    CHECK(colour(&r, 1, 0) == JLST_COLOR_HEAD);                   /* the picked key: white */
    CHECK(colour(&r, 2, 0) == JLN_COLOR_IN_SCALE);
}

static void test_the_page_pads(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    CHECK(colour(&r, 4, 3) == JLST_COLOR_PAGE_ON && colour(&r, 5, 3) == JLST_COLOR_PAGE_ON && colour(&r, 6, 3) == JLST_COLOR_PAGE_OFF);   /* before the read: all four, 1 and 2 shown */
    feed(&r, 0, 3, 1);                                            /* a pattern of 3 pages */
    CHECK(colour(&r, 7, 3) == 0 && colour(&r, 6, 3) == JLST_COLOR_PAGE_OFF);   /* page 4 does not exist */
    CHECK(press(&r, 7, 3, 100, o, 2) == 0 && r.s.first_page == 0);            /* and cannot be picked */
    CHECK(press(&r, 6, 3, 100, o, 2) == 0 && r.s.first_page == 2);
    CHECK(colour(&r, 6, 3) == JLST_COLOR_PAGE_ON && colour(&r, 4, 3) == JLST_COLOR_PAGE_OFF);
    CHECK(colour(&r, 0, 7) == 0);                                 /* the page changed: its steps are not read yet */
    CHECK(colour(&r, 4, 2) == 0 && colour(&r, 7, 0) == 0);        /* the rest of the quadrant is free */
}

static void test_which_pages_to_read(void)
{
    struct rig r;
    uint8_t pg[2];
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    CHECK(jlst_wanted(&r.s, pg) == 1 && pg[0] == 0);              /* the length is not known: page 0 says it */
    feed(&r, 0, 4, 0);
    CHECK(jlst_wanted(&r.s, pg) == 2 && pg[0] == 0 && pg[1] == 1);
    press(&r, 6, 3, 100, o, 2);
    CHECK(jlst_wanted(&r.s, pg) == 2 && pg[0] == 2 && pg[1] == 3);
    press(&r, 7, 3, 100, o, 2);
    CHECK(jlst_wanted(&r.s, pg) == 1 && pg[0] == 3);              /* the last page: no second one */
}

static void test_pages_of_another_track_are_dropped(void)
{
    struct rig r;
    struct jdl_page other = page_of(5, 0, 4, 1);
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    feed(&r, 0, 4, 1);
    jlst_on_page(&r.s, r.n.track, &other, r.c.now_ms);            /* a late answer for track 6 while the track is 1 */
    CHECK(r.s.pg[0].track == 0 && press(&r, 0, 7, 100, o, 2) == 1);
    r.n.track = 5;                                                /* the track changed: what was read is of track 1 */
    CHECK(press(&r, 0, 7, 100, o, 2) == 0 && colour(&r, 0, 7) == 0);
    jlst_on_page(&r.s, 5, &other, r.c.now_ms);
    CHECK(press(&r, 0, 7, 100, o, 2) == 1 && o[0].track == 5);
}

static void test_a_read_does_not_undo_a_press_for_a_moment(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    feed(&r, 0, 4, 0);                                            /* no trigs */
    r.c.now_ms = 5000;
    press(&r, 2, 7, 100, o, 2);                                   /* trig on step 2 */
    feed(&r, 0, 4, 0);                                            /* an older read arrives: still no trigs */
    CHECK(colour(&r, 2, 7) == JLST_COLOR_ON);
    r.c.now_ms = 5000 + JLST_EDIT_HOLD_MS;
    feed(&r, 0, 4, 0);                                            /* the hold is over: the Digitone's word counts */
    CHECK(colour(&r, 2, 7) == JLST_COLOR_OFF);
}

static void test_a_shorter_pattern_pulls_the_window_back(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    feed(&r, 0, 4, 0);
    press(&r, 7, 3, 100, o, 2);                                   /* page 4 */
    CHECK(r.s.first_page == 3);
    feed(&r, 0, 2, 0);                                            /* the pattern is 2 pages now */
    CHECK(r.s.first_page == 1 && r.s.pages_known == 2);
}

static void test_the_steps_past_the_length_are_dark(void)
{
    struct rig r;
    struct jdl_page pg = page_of(0, 0, 1, 1);
    rig_init(&r, JLST_DRUM);
    pg.length = 12;                                               /* a pattern of 12 steps */
    jlst_on_page(&r.s, 0, &pg, 1000);
    CHECK(colour(&r, 3, 7) == JLST_COLOR_OFF && colour(&r, 4, 6) == 0 && colour(&r, 7, 6) == 0);   /* steps 1..12 shown; 13..16 dark */
    CHECK(colour(&r, 0, 5) == 0);                                 /* a page that is not there */
}

static void test_the_step_that_played_is_lit_for_a_moment(void)
{
    struct rig r;
    rig_init(&r, JLST_DRUM);
    feed(&r, 0, 4, 1);
    r.c.now_ms = 2000;
    jlst_on_push(&r.s, 0, 4, 2000);
    CHECK(colour(&r, 4, 7) == JLST_COLOR_HEAD);
    r.c.now_ms = 2000 + JLST_HEAD_MS - 1u;
    CHECK(colour(&r, 4, 7) == JLST_COLOR_HEAD);
    r.c.now_ms = 2000 + JLST_HEAD_MS;
    CHECK(colour(&r, 4, 7) == JLST_COLOR_ON);
    jlst_on_push(&r.s, 3, 4, r.c.now_ms);                         /* another track's note: not shown for this one */
    CHECK(colour(&r, 4, 7) == JLST_COLOR_ON);
    jlst_on_push(&r.s, 0, 20, r.c.now_ms);                        /* a step on the other page of the window */
    feed(&r, 1, 4, 1);
    CHECK(colour(&r, 4, 5) == JLST_COLOR_HEAD);                   /* step 20 is the 5th of page 2: row 5, column 4 */
}

static void test_release_ends_what_is_held(void)
{
    struct rig r;
    struct jlapp_act o[4];
    rig_init(&r, JLST_MELODIC);
    press(&r, 1, 0, 100, o, 2);
    press(&r, 2, 0, 100, o, 2);
    CHECK(jlst_release(&r.s, o, 1) == 1 && o[0].kind == JLAPP_ACT_NOTE_OFF);
    CHECK(jlst_release(&r.s, o, 4) == 1 && o[0].kind == JLAPP_ACT_NOTE_OFF);
    CHECK(jlst_release(&r.s, o, 4) == 0);
    rig_init(&r, JLST_DRUM);
    press(&r, 1, 3, 100, o, 2);
    CHECK(jlst_release(&r.s, o, 4) == 1 && o[0].kind == JLAPP_ACT_TRACK_OFF && o[0].track == 1);
    CHECK(jlst_release(&r.s, o, 4) == 0);
}

static void test_pads_off_the_grid_and_no_page_pad_hold(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_DRUM);
    CHECK(press(&r, 8, 0, 100, o, 2) == 0 && press(&r, 0, 8, 100, o, 2) == 0);
    CHECK(press(&r, 4, 3, 0, o, 2) == 0 && r.s.first_page == 0);  /* a release of a page pad */
}

static void test_the_drum_quadrant_flashes_the_tracks_that_play(void)
{
    struct rig r;
    rig_init(&r, JLST_DRUM);
    r.c.now_ms = 5000;
    played(&r, 0, 2, 60, 5000);                                   /* track 3 played */
    played(&r, 1, 15, 36, 4900);                                  /* track 16 played 100 ms ago */
    CHECK(colour(&r, 2, 3) == JLST_COLOR_TRACK_PLAYING);          /* track 3: row 0 of the quadrant (top), column 2 */
    CHECK(colour(&r, 3, 0) == JLST_COLOR_TRACK_PLAYING);          /* track 16: the corner */
    CHECK(colour(&r, 1, 3) == JLST_COLOR_TRACK_IDLE);             /* a track that did not */
    CHECK(colour(&r, 0, 3) == JLST_COLOR_TRACK_CURRENT);          /* the current one stays green when silent */
    r.c.now_ms = 5000 + JLST_HEAD_MS - 1u;
    CHECK(colour(&r, 2, 3) == JLST_COLOR_TRACK_PLAYING && colour(&r, 3, 0) == JLST_COLOR_TRACK_IDLE);   /* the older one has faded: 150 ms */
    r.c.now_ms = 5000 + JLST_HEAD_MS;
    CHECK(colour(&r, 2, 3) == JLST_COLOR_TRACK_IDLE);
    played(&r, 2, 0, 48, r.c.now_ms);                             /* the current track plays: it flashes too, then is green again */
    CHECK(colour(&r, 0, 3) == JLST_COLOR_TRACK_PLAYING);
    r.c.now_ms += JLST_HEAD_MS;
    CHECK(colour(&r, 0, 3) == JLST_COLOR_TRACK_CURRENT);
}

static void test_the_melodic_keyboard_has_sixteen_different_notes(void)
{
    struct rig r;
    struct jlapp_act o[2];
    const enum jln_layout layouts[3] = {JLN_DIATONIC, JLN_CHROMATIC, JLN_CHORDS};
    rig_init(&r, JLST_MELODIC);
    r.c.can_play = 1;
    for (unsigned l = 0; l < 3; l++)
        for (uint8_t key = 0; key < 12; key += 5)
            for (uint8_t absolute = 0; absolute < 2; absolute++) {
                int seen[128] = {0}, n = 0;
                jln_set_layout(&r.n, layouts[l]);
                r.n.key = key;
                r.n.absolute = absolute;
                for (uint8_t y = 0; y < 4; y++)
                    for (uint8_t x = 0; x < 4; x++) {
                        struct jlapp_act on;
                        size_t k = press(&r, x, y, 100, o, 2);
                        CHECK(k == 1 && o[0].kind == JLAPP_ACT_NOTE_ON);
                        on = o[0];
                        CHECK(on.note < 128 && !seen[on.note]);    /* every key plays a note no other key plays */
                        seen[on.note] = 1;
                        n++;
                        press(&r, x, y, 0, o, 2);
                    }
                CHECK(n == 16);
            }
}

static void test_the_keyboard_rows_follow_the_scale_in_order(void)
{
    struct rig r;
    struct jlapp_act o[2];
    rig_init(&r, JLST_MELODIC);                                   /* C major, octave 3, relative: 48 50 52 53 | 55 57 59 60 | 62 64 65 67 | 69 71 72 74 */
    static const uint8_t want[16] = {48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72, 74};
    for (unsigned i = 0; i < 16; i++) {
        CHECK(press(&r, (uint8_t)(i % 4), (uint8_t)(i / 4), 100, o, 2) == 1 && o[0].note == want[i]);
        press(&r, (uint8_t)(i % 4), (uint8_t)(i / 4), 0, o, 2);
    }
}

static void test_the_melodic_keyboard_flashes_the_pitches_that_play(void)
{
    struct rig r;
    rig_init(&r, JLST_MELODIC);
    r.c.now_ms = 3000;
    played(&r, 0, 0, 50, 3000);                                   /* the current track plays D (50): the key (1,0) */
    played(&r, 1, 4, 52, 3000);                                   /* another track plays E: not this keyboard */
    CHECK(colour(&r, 1, 0) == JLST_COLOR_TRACK_PLAYING);
    CHECK(colour(&r, 2, 0) == JLN_COLOR_IN_SCALE);                /* E on the current track's keyboard stays as it was */
    CHECK(colour(&r, 0, 0) == JLN_COLOR_ROOT);
    r.c.now_ms = 3000 + JLST_HEAD_MS;
    CHECK(colour(&r, 1, 0) == JLN_COLOR_IN_SCALE);
    r.n.layout = JLN_CHROMATIC;                                   /* the 16 keys are 16 different notes: 48 + 0..15 semitones, a row up is +4 */
    r.c.now_ms = 3200;
    played(&r, 2, 0, 53, 3200);                                   /* 53 is key 5: (1,1), and no other key */
    CHECK(colour(&r, 1, 1) == JLST_COLOR_TRACK_PLAYING);
    {
        unsigned lit = 0;
        for (uint8_t y = 0; y < 4; y++)
            for (uint8_t x = 0; x < 4; x++)
                lit += colour(&r, x, y) == JLST_COLOR_TRACK_PLAYING;
        CHECK(lit == 1);
    }
    played(&r, 3, 0, 70, 3200);                                   /* a pitch that is not on the 4x4 (48..63): nothing more lights */
    {
        unsigned lit = 0;
        for (uint8_t y = 0; y < 4; y++)
            for (uint8_t x = 0; x < 4; x++)
                lit += colour(&r, x, y) == JLST_COLOR_TRACK_PLAYING;
        CHECK(lit == 1);
    }
}

static void test_the_playhead_runs_on_the_clock(void)
{
    struct rig r;
    rig_init(&r, JLST_DRUM);
    feed(&r, 0, 4, 1);                                            /* trigs on 0, 4, 8, 12 of page 1 */
    feed(&r, 1, 4, 0);
    jlst_set_clock(&r.s, 1, 0);
    CHECK(colour(&r, 0, 7) == JLST_COLOR_PLAYHEAD_NOTE);          /* step 0 has a trig: RED */
    jlst_set_clock(&r.s, 1, 1);
    CHECK(colour(&r, 1, 7) == JLST_COLOR_PLAYHEAD && colour(&r, 0, 7) == JLST_COLOR_ON);   /* step 1: BLUE on an empty step, step 0 as it is */
    jlst_set_clock(&r.s, 1, 9);
    CHECK(colour(&r, 1, 6) == JLST_COLOR_PLAYHEAD);               /* step 9 is the second row */
    jlst_set_clock(&r.s, 1, 17);
    CHECK(colour(&r, 1, 5) == JLST_COLOR_PLAYHEAD);               /* 17: the next page, third row */
    jlst_set_clock(&r.s, 1, 64 + 2);
    CHECK(colour(&r, 2, 7) == JLST_COLOR_PLAYHEAD);               /* a pattern of 64 steps wraps */
    jlst_set_clock(&r.s, 1, 40);
    {
        unsigned lit = 0;
        for (uint8_t y = 4; y < 8; y++)
            for (uint8_t x = 0; x < 8; x++)
                lit += colour(&r, x, y) == JLST_COLOR_PLAYHEAD || colour(&r, x, y) == JLST_COLOR_PLAYHEAD_NOTE;
        CHECK(lit == 0);                                          /* step 40 is not on the two pages shown: no head on them */
    }
    jlst_set_clock(&r.s, 0, 5);                                   /* stopped: no playhead */
    CHECK(colour(&r, 5, 7) == JLST_COLOR_OFF);
}

static void test_a_pushed_note_corrects_the_playhead(void)
{
    struct rig r;
    rig_init(&r, JLST_DRUM);
    feed(&r, 0, 2, 0);                                            /* 2 pages: 32 steps */
    feed(&r, 1, 2, 0);
    jlst_set_clock(&r.s, 1, 100);                                 /* the session's beat says step 100 % 32 = 4 */
    CHECK(colour(&r, 4, 7) == JLST_COLOR_PLAYHEAD);
    jlst_on_push(&r.s, 0, 7, r.c.now_ms);                         /* but the Digitone just played step 7: its start was 3 steps later */
    CHECK(colour(&r, 7, 7) == JLST_COLOR_PLAYHEAD && colour(&r, 4, 7) == JLST_COLOR_OFF);
    jlst_set_clock(&r.s, 1, 101);                                 /* and it goes on from there */
    CHECK(colour(&r, 0, 6) == JLST_COLOR_PLAYHEAD);               /* step 8 */
}

static void test_without_a_clock_only_the_note_that_played_shows(void)
{
    struct rig r;
    rig_init(&r, JLST_DRUM);
    feed(&r, 0, 4, 1);
    r.c.now_ms = 2000;
    jlst_on_push(&r.s, 0, 4, 2000);                               /* transport not known to run */
    CHECK(colour(&r, 4, 7) == JLST_COLOR_HEAD);
    r.c.now_ms = 2000 + JLST_HEAD_MS;
    CHECK(colour(&r, 4, 7) == JLST_COLOR_ON);
}

int main(void)
{
    test_the_melodic_keyboard_has_sixteen_different_notes();
    test_the_keyboard_rows_follow_the_scale_in_order();
    test_the_drum_quadrant_flashes_the_tracks_that_play();
    test_the_melodic_keyboard_flashes_the_pitches_that_play();
    test_the_playhead_runs_on_the_clock();
    test_a_pushed_note_corrects_the_playhead();
    test_without_a_clock_only_the_note_that_played_shows();
    test_a_step_does_nothing_until_its_page_is_read();
    test_a_step_press_toggles_the_trig();
    test_the_step_pads_are_the_digitones_layout();
    test_a_melodic_step_with_a_key_picked();
    test_the_keys_play_only_while_the_transport_is_stopped();
    test_the_drum_quadrant_selects_the_track();
    test_the_drum_quadrant_only_selects_while_the_transport_runs();
    test_the_drum_quadrant_lights();
    test_the_melodic_keyboard_lights();
    test_the_page_pads();
    test_which_pages_to_read();
    test_pages_of_another_track_are_dropped();
    test_a_read_does_not_undo_a_press_for_a_moment();
    test_a_shorter_pattern_pulls_the_window_back();
    test_the_steps_past_the_length_are_dark();
    test_the_step_that_played_is_lit_for_a_moment();
    test_release_ends_what_is_held();
    test_pads_off_the_grid_and_no_page_pad_hold();
    printf("test_lpstep: %d checks, %d failed\n", checks, fails);
    return fails;
}
