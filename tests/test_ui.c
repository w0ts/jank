/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the button, the scan's bookkeeping and the LED events. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_core.h"
#include "jank_ui.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

/* feeds a level for ms, 5 ms a sample; returns the event seen, counting them */
static uint32_t now;
static int events;
static enum jb_event hold(struct jb_button *b, int level, uint32_t ms)
{
    enum jb_event seen = JB_NONE;
    for (uint32_t t = 0; t < ms; t += 5u) {
        enum jb_event e = jb_step(b, level, now);
        now += 5u;
        if (e != JB_NONE) {
            seen = e;
            events++;
        }
    }
    return seen;
}

static void reset(struct jb_button *b)
{
    now = 1000u;
    events = 0;
    jb_init(b);
    hold(b, 0, 100);
}

static void test_click(void)
{
    struct jb_button b;
    reset(&b);
    CHECK(hold(&b, 1, 100) == JB_NONE);
    CHECK(hold(&b, 0, JB_DOUBLE_MS - 30u) == JB_NONE);   /* not yet: a second press may come */
    CHECK(hold(&b, 0, 100) == JB_CLICK);
    CHECK(hold(&b, 0, 2000) == JB_NONE && events == 1);
}

static void test_double(void)
{
    struct jb_button b;
    reset(&b);
    hold(&b, 1, 80);
    hold(&b, 0, 150);
    CHECK(hold(&b, 1, 80) == JB_DOUBLE);                 /* said on the second press, no waiting */
    CHECK(hold(&b, 0, 1000) == JB_NONE && events == 1);
}

static void test_two_slow_clicks(void)
{
    struct jb_button b;
    reset(&b);
    hold(&b, 1, 80);
    CHECK(hold(&b, 0, JB_DOUBLE_MS + 50u) == JB_CLICK);
    hold(&b, 1, 80);
    CHECK(hold(&b, 0, JB_DOUBLE_MS + 50u) == JB_CLICK);
    CHECK(events == 2);
}

static void test_bounce(void)
{
    struct jb_button b;
    reset(&b);
    for (int i = 0; i < 5; i++) {                        /* contact bounce of 5 ms each: below the debounce */
        hold(&b, 1, 5);
        hold(&b, 0, 5);
    }
    CHECK(hold(&b, 1, 100) == JB_NONE);
    /* release, bouncing */
    for (int i = 0; i < 5; i++) {
        hold(&b, 0, 5);
        hold(&b, 1, 5);
    }
    CHECK(hold(&b, 0, JB_DOUBLE_MS + 100u) == JB_CLICK);
    CHECK(events == 1);                                  /* a bouncy click is not a double */
    /* a glitch shorter than the debounce alone: nothing */
    events = 0;
    hold(&b, 1, 10);
    hold(&b, 0, 1000);
    CHECK(events == 0);
}

static void test_long(void)
{
    struct jb_button b;
    reset(&b);
    CHECK(hold(&b, 1, JB_LONG_MS + 500u) == JB_NONE);    /* said at the release, as it could still become download */
    CHECK(hold(&b, 0, 100) == JB_LONG && events == 1);
    CHECK(hold(&b, 0, 2000) == JB_NONE && events == 1);  /* and no click after it */
    reset(&b);
    CHECK(hold(&b, 1, JB_LONG_MS - 100u) == JB_NONE);
    CHECK(hold(&b, 0, 1000) == JB_CLICK);                 /* short of long: a click */
}

static void test_download(void)
{
    struct jb_button b;
    reset(&b);
    CHECK(hold(&b, 1, JB_DOWNLOAD_MS - 100u) == JB_NONE);
    CHECK(hold(&b, 1, 200) == JB_DOWNLOAD);               /* while still held: the user lets go into download mode */
    CHECK(hold(&b, 1, 5000) == JB_NONE);
    CHECK(hold(&b, 0, 2000) == JB_NONE && events == 1);  /* no long press at the release */
}

static void test_wrapping_clock(void)
{
    struct jb_button b;
    jb_init(&b);
    now = 0xFFFFFF00u;
    events = 0;
    hold(&b, 0, 20);
    hold(&b, 1, 80);
    CHECK(hold(&b, 0, JB_DOUBLE_MS + 100u) == JB_CLICK);
}

static size_t frame(uint8_t *f, uint8_t fc0, uint8_t group, const uint8_t *sender)
{
    memset(f, 0, 32);
    f[0] = fc0;
    memcpy(f + 4, "\x01\x00\x5E\x4C\x4E\x4B", 6);
    memcpy(f + 10, sender, 6);
    memcpy(f + 16, "\x02\x66\x6D\x31\x6A", 5);
    f[21] = group;
    return 32;
}

static void test_group_of(void)
{
    static const uint8_t ME[6] = {2, 0, 0, 0, 0, 1}, HIM[6] = {0x8C, 0, 0, 0, 0, 2};
    uint8_t f[32];
    for (uint8_t g = 1; g <= 9; g++)
        CHECK(js_group_of(f, frame(f, 0x08, g, HIM), ME) == g);
    CHECK(js_group_of(f, frame(f, 0x08, 0, HIM), ME) == 0);     /* out of range */
    CHECK(js_group_of(f, frame(f, 0x08, 10, HIM), ME) == 0);
    CHECK(js_group_of(f, frame(f, 0x80, 3, HIM), ME) == 0);     /* a beacon */
    CHECK(js_group_of(f, frame(f, 0x08, 3, ME), ME) == 0);      /* ourselves */
    frame(f, 0x08, 3, HIM);
    CHECK(js_group_of(f, 23, ME) == 0);                         /* short */
    f[16] = 0x06;
    CHECK(js_group_of(f, 32, ME) == 0);                         /* another network's BSSID */
}

static void test_plan(void)
{
    uint8_t ch[2];
    CHECK(js_plan(1, ch) == 2 && ch[0] == 6 && ch[1] == 11);
    CHECK(js_plan(4, ch) == 2 && ch[0] == 6 && ch[1] == 11);   /* 1, 4, 7 share channel 1 */
    CHECK(js_plan(2, ch) == 2 && ch[0] == 1 && ch[1] == 11);
    CHECK(js_plan(9, ch) == 2 && ch[0] == 1 && ch[1] == 6);
}

/* the automatic scan: not before JS_ALONE_MS alone, then every JS_AUTO_EVERY_MS, never while anyone is heard */
static void test_auto(void)
{
    struct js_auto a;
    uint32_t t = 100000;
    js_auto_init(&a, t);
    CHECK(!js_auto_step(&a, 0, t + JS_ALONE_MS - 1));             /* alone, but not long enough */
    CHECK(js_auto_step(&a, 0, t + JS_ALONE_MS));                  /* alone long enough: sweep */
    js_auto_swept(&a, t + JS_ALONE_MS + 700);
    CHECK(!js_auto_step(&a, 0, t + JS_ALONE_MS + 700 + JS_AUTO_EVERY_MS - 1));
    CHECK(js_auto_step(&a, 0, t + JS_ALONE_MS + 700 + JS_AUTO_EVERY_MS));   /* and again */
    CHECK(!js_auto_step(&a, 2, t + 20000));                       /* someone heard: no sweep */
    CHECK(!js_auto_step(&a, 0, t + 20001));                       /* quiet again: the count restarts */
    CHECK(!js_auto_step(&a, 0, t + 20001 + JS_ALONE_MS - 1));
    CHECK(js_auto_step(&a, 0, t + 20001 + JS_ALONE_MS));          /* first sweep of this quiet spell, at once */
    js_auto_init(&a, 0xFFFFF000u);                                /* the ms counter wraps */
    CHECK(js_auto_step(&a, 0, 0xFFFFF000u + JS_ALONE_MS));
}

static void test_pick(void)
{
    struct js_scan s;
    js_clear(&s);
    CHECK(js_pick(&s, 3, 1000) == 0);                  /* nothing heard */
    js_note(&s, 3, 1000);
    CHECK(js_pick(&s, 3, 1000) == 0);                  /* only our own group: stay */
    js_note(&s, 7, 1000);
    js_note(&s, 2, 1000);
    CHECK(js_pick(&s, 3, 1000) == 7);                  /* the next after 3 */
    CHECK(js_pick(&s, 7, 1000) == 2);                  /* wrapping */
    CHECK(js_pick(&s, 2, 1000) == 3);
    CHECK(js_pick(&s, 9, 1000) == 2);                  /* current not heard itself */
    /* too old */
    CHECK(js_pick(&s, 3, 1000 + JS_HEARD_MS) == 0);
    /* a frame of a group outside 1..9 is ignored */
    js_note(&s, 0, 1000);
    js_note(&s, 10, 1000);
    CHECK(s.heard == ((1u << 2) | (1u << 3) | (1u << 7)));
}

static void test_table(void)
{
    static const uint8_t FM[6] = {0x8C, 0, 0, 0, 0, 1}, FM2[6] = {0x8C, 0, 0, 0, 0, 2}, JK[6] = {0x02, 0x6A, 0x6B, 0, 0, 3};
    struct js_table t;
    char out[512];
    uint8_t m[6] = {0x8C, 0, 0, 0, 1, 0};
    js_table_clear(&t);
    /* nothing heard: says so */
    CHECK(js_table_report(&t, out, sizeof out) > 0);
    CHECK(!strcmp(out, "scan=none\nok\n"));
    /* a sender counts once, its frames every time, the best signal wins */
    js_table_note(&t, 3, FM, -70);
    js_table_note(&t, 3, FM, -50);
    js_table_note(&t, 3, FM2, -60);
    js_table_note(&t, 3, JK, -80);
    js_table_note(&t, 7, JK, -65);
    CHECK(t.row[3].frames == 4 && t.row[3].units == 3 && t.row[3].janks == 1 && t.row[3].rssi == -50);
    /* only groups with traffic, in order, with their channel: group,channel,frames,units,janks,rssi */
    CHECK(js_table_report(&t, out, sizeof out) > 0);
    CHECK(!strcmp(out, "scan=3,11,4,3,1,-50\nscan=7,1,1,1,1,-65\nok\n"));
    /* outside 1..9: ignored */
    js_table_note(&t, 0, FM, -10);
    js_table_note(&t, 10, FM, -10);
    CHECK(t.row[0].frames == 0);
    /* more units than kept: the frames still count, the units stop at the limit */
    for (uint32_t i = 0; i < JS_UNITS + 3u; i++) {
        m[5] = (uint8_t)i;
        js_table_note(&t, 1, m, -40);
    }
    CHECK(t.row[1].units == JS_UNITS && t.row[1].frames == JS_UNITS + 3u);
    /* a full table, the longest numbers: fits the firmware's answer buffer, a short one is refused, not cut */
    js_table_clear(&t);
    for (uint32_t g = 1; g <= 9; g++) {
        js_table_note(&t, g, FM, -127);
        t.row[g].frames = 0xFFFFFFFFu;
    }
    CHECK(js_table_report(&t, out, 320) > 0);
    CHECK(js_table_report(&t, out, 30) == 0);
}

static int flashes(enum jl_event e, uint32_t group, uint32_t *total)
{
    uint8_t c[3];
    int n = 0, last = 0;
    uint32_t t;
    for (t = 0; jl_pattern(e, group, t, c); t += 5u) {
        int on = c[0] || c[1] || c[2];
        n += on && !last;
        last = on;
    }
    *total = t;
    return n;
}

static void test_led_events(void)
{
    uint8_t c[3];
    uint32_t total;
    for (uint32_t g = 1; g <= 9; g++) {
        CHECK(flashes(JL_JOINED, g, &total) == (int)g);
        CHECK(total >= g * JL_FLASH_MS);
    }
    CHECK(jl_pattern(JL_JOINED, 3, 0, c) == 1 && c[0] && c[1] && c[2]);    /* white */
    CHECK(flashes(JL_NO_GROUP, 1, &total) == 3);
    CHECK(jl_pattern(JL_NO_GROUP, 1, 0, c) == 1 && c[0] && !c[1] && !c[2]);  /* red: not a JL_JOINED */
    CHECK(jl_pattern(JL_SCANNING, 1, 100000, c) == 1 && c[0] && !c[1] && c[2]);  /* steady, until the event changes */
    CHECK(jl_pattern(JL_NONE, 1, 0, c) == 0);
}

int main(void)
{
    test_click();
    test_double();
    test_two_slow_clicks();
    test_bounce();
    test_long();
    test_download();
    test_wrapping_clock();
    test_group_of();
    test_plan();
    test_pick();
    test_auto();
    test_table();
    test_led_events();
    printf("ui: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
