/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the Launchpads' Programmer mode core (core/jank_lpx.c): what pads and buttons send, and the packets that light them, for the
 * X, Mini MK3, MK2 and Pro. The numbers are those of their manuals. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpx.h"

static const struct jlx_model *const X = &jlx_models[0], *const MINI = &jlx_models[1], *const MK2 = &jlx_models[2], *const PRO = &jlx_models[3];
static struct jlx_model X0;              /* the X on cable 0: the packets below as the manual writes them */

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static struct jlx_ev dec(uint8_t s, uint8_t a, uint8_t b, int *ok)
{
    const uint8_t m[3] = {s, a, b};
    struct jlx_ev e;
    memset(&e, 0xEE, sizeof e);
    *ok = jlx_decode(m, &e);
    return e;
}

static void test_decode_pads(void)
{
    int ok;
    struct jlx_ev e = dec(0x90, 11, 127, &ok);                 /* lower left */
    CHECK(ok && e.kind == JLX_PAD && e.row == 0 && e.col == 0 && e.value == 127);
    e = dec(0x90, 88, 64, &ok);                                /* upper right */
    CHECK(ok && e.kind == JLX_PAD && e.row == 7 && e.col == 7 && e.value == 64);
    e = dec(0x90, 45, 0, &ok);                                 /* Note On velocity 0 is a release */
    CHECK(ok && e.kind == JLX_PAD && e.row == 3 && e.col == 4 && e.value == 0);
    e = dec(0x80, 45, 90, &ok);                                /* a Note Off is a release whatever its velocity */
    CHECK(ok && e.kind == JLX_PAD && e.value == 0);
    e = dec(0x93, 18, 100, &ok);                               /* any channel */
    CHECK(ok && e.kind == JLX_PAD && e.row == 0 && e.col == 7 && e.value == 100);
}

static void test_decode_not_pads(void)
{
    int ok;
    (void)dec(0x90, 10, 100, &ok);                              /* column 0 does not exist */
    CHECK(!ok);
    (void)dec(0x90, 9, 100, &ok);
    CHECK(!ok);
    (void)dec(0x90, 0, 100, &ok);
    CHECK(!ok);
    (void)dec(0xA0, 45, 100, &ok);                              /* polyphonic aftertouch */
    CHECK(!ok);
    (void)dec(0xD0, 45, 0, &ok);                                /* channel pressure */
    CHECK(!ok);
    (void)dec(0xE0, 0, 64, &ok);
    CHECK(!ok);
}

static void test_decode_buttons(void)
{
    int ok;
    struct jlx_ev e;
    for (uint8_t i = 0; i < 8; i++) {
        e = dec(0xB0, (uint8_t)(91u + i), 127, &ok);
        CHECK(ok && e.kind == JLX_TOP && e.idx == i && e.value == 127);
        e = dec(0xB0, (uint8_t)(91u + i), 0, &ok);
        CHECK(ok && e.kind == JLX_TOP && e.idx == i && e.value == 0);
    }
    for (uint8_t i = 0; i < 8; i++) {                           /* CC 89 is the top one, CC 19 the bottom one */
        e = dec(0xB0, (uint8_t)(89u - 10u * i), 127, &ok);
        CHECK(ok && e.kind == JLX_SIDE && e.idx == i && e.value == 127);
        e = dec(0x90, (uint8_t)(89u - 10u * i), 127, &ok);      /* the MK2's are Notes */
        CHECK(ok && e.kind == JLX_SIDE && e.idx == i && e.value == 127);
        e = dec(0x80, (uint8_t)(89u - 10u * i), 64, &ok);
        CHECK(ok && e.kind == JLX_SIDE && e.idx == i && e.value == 0);
        e = dec(0xB0, (uint8_t)(104u + i), 127, &ok);           /* the MK2's top row */
        CHECK(ok && e.kind == JLX_TOP && e.idx == i && e.value == 127);
    }
    (void)dec(0x90, 95, 127, &ok);                              /* top buttons are CCs on every model */
    CHECK(!ok);
    (void)dec(0xB0, 112, 127, &ok);
    CHECK(!ok);
    for (uint8_t i = 1; i <= 8; i++) {                          /* the Pro's left column and bottom row: not read */
        (void)dec(0xB0, (uint8_t)(10u * i), 127, &ok);
        CHECK(!ok);
        (void)dec(0xB0, i, 127, &ok);
        CHECK(!ok);
    }
    (void)dec(0xB0, 99, 127, &ok);                              /* the logo does not send */
    CHECK(!ok);
    (void)dec(0xB0, 90, 127, &ok);
    CHECK(!ok);
    (void)dec(0xB0, 29u + 1u, 127, &ok);                        /* 30 is no button */
    CHECK(!ok);
    (void)dec(0xB0, 7, 127, &ok);                               /* a CC that is not a button's */
    CHECK(!ok);
}

static void test_numbers_round_trip(void)
{
    for (uint32_t k = 0; k < JLX_MODELS; k++) {
        const struct jlx_model *m = &jlx_models[k];
        uint8_t seen[128];
        memset(seen, 0, sizeof seen);
        for (uint32_t i = 0; i < JLX_LEDS; i++) {
            uint8_t n = jlx_number(m, i);
            CHECK(n < 128 && !seen[n]);                         /* one number each */
            seen[n] = 1;
        }
        for (uint32_t i = 0; i < JLX_LOGO; i++) {               /* a light's number decodes back to the same pad or button */
            const int note = i < JLX_TOP0 || (m->side_note && i >= JLX_SIDE0);
            const uint8_t msg[3] = {note ? 0x90 : 0xB0, jlx_number(m, i), 100};
            struct jlx_ev e;
            CHECK(jlx_decode(msg, &e));
            CHECK(i >= JLX_TOP0 || (e.kind == JLX_PAD && e.row * 8u + e.col == i));
            CHECK(i < JLX_TOP0 || i >= JLX_SIDE0 || (e.kind == JLX_TOP && e.idx == i - JLX_TOP0));
            CHECK(i < JLX_SIDE0 || (e.kind == JLX_SIDE && e.idx == i - JLX_SIDE0));
        }
    }
    CHECK(jlx_number(X, JLX_PAD0 + 0) == 11 && jlx_number(X, JLX_PAD0 + 7) == 18 && jlx_number(X, JLX_PAD0 + 63) == 88);
    CHECK(jlx_number(X, JLX_TOP0) == 91 && jlx_number(X, JLX_TOP0 + 7) == 98);
    CHECK(jlx_number(X, JLX_SIDE0) == 89 && jlx_number(X, JLX_SIDE0 + 7) == 19);
    CHECK(jlx_number(X, JLX_LOGO) == 99);
    CHECK(jlx_number(MK2, JLX_TOP0) == 104 && jlx_number(MK2, JLX_TOP0 + 7) == 111);
    CHECK(jlx_number(PRO, JLX_TOP0) == 91 && jlx_number(MINI, JLX_SIDE0 + 7) == 19);
}

static void test_model_find(void)
{
    CHECK(jlx_model_find("Launchpad X") == X);
    CHECK(jlx_model_find("Launchpad Mini MK3") == MINI);
    CHECK(jlx_model_find("Launchpad MK2") == MK2);
    CHECK(jlx_model_find("Launchpad Pro") == PRO);
    CHECK(jlx_model_find("Launchpad Pro MK3") == NULL);         /* another protocol (header 0Eh) */
    CHECK(jlx_model_find("Launchpad Mini") == NULL);            /* the red / green ones */
    CHECK(jlx_model_find("Launchpad S") == NULL);
    CHECK(jlx_model_find("Launchpad") == NULL);
    CHECK(jlx_model_find("Digitone II") == NULL);
    CHECK(jlx_model_find("") == NULL && jlx_model_find(NULL) == NULL);
    CHECK(X->velocity && PRO->velocity && !MINI->velocity && !MK2->velocity);   /* the Mini MK3 and the MK2 send 127 */
}

/* F0 00 20 29 02 0C 0E 01 F7: Programmer mode, on the X's MIDI port (cable 1) */
static void test_programmer_sysex(void)
{
    uint8_t out[64];
    const uint8_t on[12] = {0x14, 0xF0, 0x00, 0x20, 0x14, 0x29, 0x02, 0x0C, 0x17, 0x0E, 0x01, 0xF7};
    const uint8_t on0[12] = {0x04, 0xF0, 0x00, 0x20, 0x04, 0x29, 0x02, 0x0C, 0x07, 0x0E, 0x01, 0xF7};
    CHECK(jlx_sysex(X, X->start, out) == 12 && !memcmp(out, on, 12));
    CHECK(jlx_sysex(&X0, X0.start, out) == 12 && !memcmp(out, on0, 12));
    CHECK(jlx_sysex(X, X->start, NULL) == 12);                     /* counted only */
    CHECK(jlx_sysex(X, NULL, out) == 0 && jlx_sysex(MK2, MK2->setup, out) == 0 && jlx_sysex(MK2, MK2->query, out) == 0);
}

static void test_other_models_start(void)
{
    uint8_t out[64];
    static const uint8_t mini[12] = {0x14, 0xF0, 0x00, 0x20, 0x14, 0x29, 0x02, 0x0D, 0x17, 0x0E, 0x01, 0xF7};
    static const uint8_t mk2[12] = {0x04, 0xF0, 0x00, 0x20, 0x04, 0x29, 0x02, 0x18, 0x07, 0x22, 0x00, 0xF7};   /* Session layout, its one port */
    static const uint8_t pro[36] = {0x14, 0xF0, 0x00, 0x20, 0x14, 0x29, 0x02, 0x10, 0x17, 0x21, 0x01, 0xF7,   /* Standalone mode */
                                    0x14, 0xF0, 0x00, 0x20, 0x14, 0x29, 0x02, 0x10, 0x17, 0x2C, 0x03, 0xF7,   /* Programmer layout */
                                    0x14, 0xF0, 0x00, 0x20, 0x14, 0x29, 0x02, 0x10, 0x16, 0x2F, 0xF7, 0x00};  /* which layout? */
    static const uint8_t pro_q[12] = {0x14, 0xF0, 0x00, 0x20, 0x14, 0x29, 0x02, 0x10, 0x16, 0x2F, 0xF7, 0x00};
    CHECK(jlx_sysex(MINI, MINI->start, out) == 12 && !memcmp(out, mini, 12));
    CHECK(jlx_sysex(MINI, MINI->setup, out) == 28 && out[7] == 0x0D && out[23] == 0x0D);   /* the X's setup, its own header */
    CHECK(jlx_sysex(MK2, MK2->start, out) == 12 && !memcmp(out, mk2, 12));
    CHECK(jlx_sysex(PRO, PRO->start, out) == 36 && !memcmp(out, pro, 36));
    CHECK(jlx_sysex(PRO, PRO->setup, out) == 0);
    CHECK(jlx_sysex(PRO, PRO->query, out) == 12 && !memcmp(out, pro_q, 12));
}

static struct jlx_frame blank(void)
{
    struct jlx_frame f;
    memset(&f, 0, sizeof f);
    return f;
}

static void test_diff_nothing_to_send(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[400];
    CHECK(jlx_diff(&X0, &shown, &want, out, 100) == 0);
}

static void test_diff_static_pad_and_button(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[16];
    const uint8_t pad[4] = {0x09, 0x90, 11, 5};
    const uint8_t top[4] = {0x1B, 0xB0, 91, 21};
    want.led[JLX_PAD0 + 0].color = 5;
    CHECK(jlx_diff(&X0, &shown, &want, out, 4) == 1 && !memcmp(out, pad, 4));
    CHECK(shown.led[0].color == 5);
    CHECK(jlx_diff(&X0, &shown, &want, out, 4) == 0);               /* now the same: nothing */
    want.led[JLX_TOP0].color = 21;
    CHECK(jlx_diff(X, &shown, &want, out, 4) == 1 && !memcmp(out, top, 4));   /* the X's cable 1 */
}

static void test_diff_off_is_static_zero(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[16];
    const uint8_t off[4] = {0x09, 0x90, 12, 0};
    shown.led[1].color = 9;
    shown.led[1].mode = JLX_PULSE;                                  /* a pulsing light is stopped by a static 0 */
    CHECK(jlx_diff(&X0, &shown, &want, out, 4) == 1 && !memcmp(out, off, 4));
    CHECK(shown.led[1].mode == JLX_STATIC && shown.led[1].color == 0);
}

static void test_diff_flash_and_pulse(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[16];
    const uint8_t a[4] = {0x09, 0x90, 11, 3};                       /* A, static */
    const uint8_t b[4] = {0x09, 0x91, 11, 5};                       /* then B, flashing */
    const uint8_t p[4] = {0x09, 0x92, 12, 45};
    want.led[0].mode = JLX_FLASH;
    want.led[0].alt = 3;
    want.led[0].color = 5;
    CHECK(jlx_diff(&X0, &shown, &want, out, 8) == 2 && !memcmp(out, a, 4) && !memcmp(out + 4, b, 4));
    CHECK(jlx_diff(&X0, &shown, &want, out, 8) == 0);
    want.led[1].mode = JLX_PULSE;
    want.led[1].color = 45;
    CHECK(jlx_diff(&X0, &shown, &want, out, 8) == 1 && !memcmp(out, p, 4));
    want.led[0].alt = 7;                                            /* a changed A sends the pair again */
    CHECK(jlx_diff(&X0, &shown, &want, out, 8) == 2 && out[3] == 7);
}

static void test_diff_limit_and_resume(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[4 * JLX_LEDS * 2];
    size_t total = 0, n;
    for (uint32_t i = 0; i < JLX_LEDS; i++)
        want.led[i].color = (uint8_t)(1 + i);
    n = jlx_diff(&X0, &shown, &want, out, 16);
    CHECK(n == 16);
    total += n;
    while ((n = jlx_diff(&X0, &shown, &want, out, 16)) != 0)
        total += n;
    CHECK(total == JLX_LEDS);                                       /* every light sent once, none twice */
    CHECK(!memcmp(&shown, &want, sizeof shown));
}

static void test_diff_flash_needs_two_slots(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[16];
    want.led[0].mode = JLX_FLASH;
    want.led[0].color = 5;
    CHECK(jlx_diff(&X0, &shown, &want, out, 1) == 0);               /* no half a flash: it waits for room */
    CHECK(shown.led[0].mode == JLX_STATIC && shown.led[0].color == 0);
    want.led[1].color = 9;                                          /* a later light that fits does not jump the queue */
    CHECK(jlx_diff(&X0, &shown, &want, out, 1) == 0);
    CHECK(jlx_diff(&X0, &shown, &want, out, 3) == 3);
}

static void test_forget_sends_all(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[4 * JLX_LEDS];
    CHECK(jlx_diff(&X0, &shown, &want, out, JLX_LEDS) == 0);
    jlx_forget(&shown);
    CHECK(jlx_diff(&X0, &shown, &want, out, JLX_LEDS) == JLX_LEDS);   /* all dark lights are sent once, to be sure */
    CHECK(jlx_diff(&X0, &shown, &want, out, JLX_LEDS) == 0);
    jlx_forget(&shown);
    CHECK(jlx_diff(MK2, &shown, &want, out, JLX_LEDS) == JLX_LEDS - 1u);   /* the MK2 has no logo */
}

/* the MK2: top row CC 104.., side buttons Notes, flash and pulse by channel, on cable 0 */
static void test_diff_mk2(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[32];
    const uint8_t top[4] = {0x0B, 0xB0, 104, 21}, side[4] = {0x09, 0x90, 89, 5}, side_b[4] = {0x09, 0x91, 19, 9};
    want.led[JLX_TOP0].color = 21;
    want.led[JLX_SIDE0].color = 5;
    want.led[JLX_LOGO].color = 3;                                   /* no logo: nothing sent, every time */
    CHECK(jlx_diff(MK2, &shown, &want, out, 8) == 2 && !memcmp(out, top, 4) && !memcmp(out + 4, side, 4));
    CHECK(jlx_diff(MK2, &shown, &want, out, 8) == 0);
    want.led[JLX_SIDE0 + 7].mode = JLX_FLASH;
    want.led[JLX_SIDE0 + 7].color = 9;
    CHECK(jlx_diff(MK2, &shown, &want, out, 8) == 2 && !memcmp(out + 4, side_b, 4));
}

/* the Pro in Programmer layout: flash = A static, then SysEx 23h <led> <B>; pulse = off, then SysEx 28h <led> <colour> */
static void test_diff_pro_flash_by_sysex(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[64];
    static const uint8_t flash[20] = {0x19, 0x90, 11, 3, 0x14, 0xF0, 0x00, 0x20, 0x14, 0x29, 0x02, 0x10, 0x14, 0x23, 11, 5, 0x15, 0xF7, 0x00, 0x00};
    static const uint8_t pulse[20] = {0x1B, 0xB0, 99, 0, 0x14, 0xF0, 0x00, 0x20, 0x14, 0x29, 0x02, 0x10, 0x14, 0x28, 99, 45, 0x15, 0xF7, 0x00, 0x00};
    want.led[0].mode = JLX_FLASH;
    want.led[0].alt = 3;
    want.led[0].color = 5;
    CHECK(jlx_diff(PRO, &shown, &want, out, 4) == 0);               /* 5 packets or none */
    CHECK(jlx_diff(PRO, &shown, &want, out, JLX_LIGHT_MAX) == 5 && !memcmp(out, flash, 20));
    want.led[JLX_LOGO].mode = JLX_PULSE;                            /* its side LED, 99 */
    want.led[JLX_LOGO].color = 45;
    CHECK(jlx_diff(PRO, &shown, &want, out, 16) == 5 && !memcmp(out, pulse, 20));
    want.led[JLX_LOGO].mode = JLX_STATIC;                           /* a static light is one CC, as on the X */
    CHECK(jlx_diff(PRO, &shown, &want, out, 16) == 1 && out[0] == 0x1B && out[2] == 99 && out[3] == 45);
}

/* the answer to the Programmer mode message: the same SysEx back, its last packet 07 0E 01 F7 (measured on the X); the Pro's to 2Fh: 2F <layout> F7 */
static void test_programmer_ack(void)
{
    const uint8_t on[4] = {0x07, 0x0E, 0x01, 0xF7}, off[4] = {0x07, 0x0E, 0x00, 0xF7}, cable2[4] = {0x27, 0x0E, 0x01, 0xF7};
    const uint8_t first[4] = {0x04, 0xF0, 0x00, 0x20}, bad_end[4] = {0x07, 0x0E, 0x01, 0x00};
    const uint8_t not_end[4] = {0x04, 0x0E, 0x01, 0xF7}, pad[4] = {0x09, 0x90, 0x18, 0x00}, other[4] = {0x07, 0x0F, 0x01, 0xF7};
    const uint8_t pro_on[4] = {0x17, 0x2F, 0x03, 0xF7}, pro_note[4] = {0x17, 0x2F, 0x00, 0xF7};
    uint8_t sent[64];
    int mode = -1;
    CHECK(jlx_programmer_ack(X, on, &mode) && mode == 1);
    CHECK(jlx_programmer_ack(X, off, &mode) && mode == 0);
    CHECK(jlx_programmer_ack(X, cable2, &mode) && mode == 1);       /* any cable */
    CHECK(jlx_programmer_ack(X, on, NULL));                         /* the mode is optional */
    CHECK(!jlx_programmer_ack(X, first, &mode));                    /* the first packet of the same SysEx is not its end */
    CHECK(!jlx_programmer_ack(X, bad_end, &mode) && !jlx_programmer_ack(X, not_end, &mode));
    CHECK(!jlx_programmer_ack(X, pad, &mode) && !jlx_programmer_ack(X, other, &mode));
    CHECK(jlx_sysex(X, X->start, sent) == 12 && jlx_programmer_ack(X, sent + 8, &mode) && mode == 1);   /* our own message ends the way its answer does */
    CHECK(jlx_programmer_ack(MINI, on, &mode) && mode == 1);
    CHECK(!jlx_programmer_ack(MK2, on, &mode));                     /* the MK2 answers nothing */
    CHECK(jlx_programmer_ack(PRO, pro_on, &mode) && mode == 1);     /* Programmer layout */
    CHECK(jlx_programmer_ack(PRO, pro_note, &mode) && mode == 0);   /* Note layout: start again */
    CHECK(!jlx_programmer_ack(PRO, on, &mode));
}

/* what Launchpad 95 sends after Programmer mode: LED feedback external on, sleep mode off */
static void test_setup(void)
{
    uint8_t out[28];
    static const uint8_t want[28] = {0x04, 0xF0, 0x00, 0x20, 0x04, 0x29, 0x02, 0x0C, 0x04, 0x0A, 0x00, 0x01, 0x05, 0xF7, 0x00, 0x00,
                                     0x04, 0xF0, 0x00, 0x20, 0x04, 0x29, 0x02, 0x0C, 0x07, 0x09, 0x01, 0xF7};
    CHECK(jlx_sysex(&X0, X0.setup, out) == 28 && !memcmp(out, want, 28));
    CHECK(jlx_sysex(X, X->setup, out) == 28 && out[0] == 0x14 && out[12] == 0x15 && out[24] == 0x17);   /* the cable in each packet's high nibble */
}

static void test_mode_query(void)
{
    uint8_t out[12];
    static const uint8_t want[12] = {0x04, 0xF0, 0x00, 0x20, 0x04, 0x29, 0x02, 0x0C, 0x06, 0x0E, 0xF7, 0x00};   /* F0 00 20 29 02 0C 0E F7 */
    CHECK(jlx_sysex(&X0, X0.query, out) == 12 && !memcmp(out, want, 12));
    CHECK(jlx_sysex(X, X->query, out) == 12 && out[0] == 0x14 && out[4] == 0x14 && out[8] == 0x16);
}

int main(void)
{
    X0 = *X;
    X0.cable = 0;
    test_model_find();
    test_setup();
    test_mode_query();
    test_programmer_ack();
    test_decode_pads();
    test_decode_not_pads();
    test_decode_buttons();
    test_numbers_round_trip();
    test_programmer_sysex();
    test_other_models_start();
    test_diff_nothing_to_send();
    test_diff_static_pad_and_button();
    test_diff_off_is_static_zero();
    test_diff_flash_and_pulse();
    test_diff_limit_and_resume();
    test_diff_flash_needs_two_slots();
    test_forget_sends_all();
    test_diff_mk2();
    test_diff_pro_flash_by_sysex();
    printf("test_lpx: %d checks, %d failed\n", checks, fails);
    return fails;
}
