/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the hosted audio device's descriptor parsing and rate requests (core/jank_uacdesc.c). The descriptors below
 * are written from audio10.pdf / Audio20.pdf's layouts, as a USB microphone (1.0) and an interface with a clock selector
 * (2.0) would give them. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_uacdesc.h"
#include "usb_audio.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define LE16(v) (uint8_t)((v) & 0xFF), (uint8_t)(((v) >> 8) & 0xFF)
#define LE24(v) (uint8_t)((v) & 0xFF), (uint8_t)(((v) >> 8) & 0xFF), (uint8_t)(((v) >> 16) & 0xFF)
#define LE32(v) LE24(v), (uint8_t)(((v) >> 24) & 0xFF)

/* UAC 1.0 microphone: control 0, streaming 1 (alt 1: 16-bit stereo 44.1 / 48 kHz; alt 2: 24-bit stereo 48 kHz only) */
static const uint8_t UAC1_MIC[] = {
    9, 0x02, LE16(0), 2, 1, 0, 0x80, 50,                               /* configuration (length not checked) */
    9, 0x04, 0, 0, 0, 0x01, 0x01, 0x00, 0,                             /* AC interface, protocol 0 */
    9, 0x24, 0x01, LE16(0x0100), LE16(30), 1, 1,                        /* header 1.00 */
    12, 0x24, 0x02, 1, LE16(0x0201), 0, 2, LE16(3), 0, 0,               /* input terminal: microphone */
    9, 0x24, 0x03, 2, LE16(0x0101), 0, 1, 0,                            /* output terminal: USB streaming, 9 bytes */
    9, 0x04, 1, 0, 0, 0x01, 0x02, 0x00, 0,                              /* AS alt 0 */
    9, 0x04, 1, 1, 1, 0x01, 0x02, 0x00, 0,                              /* AS alt 1 */
    7, 0x24, 0x01, 2, 1, LE16(0x0001),                                  /* AS_GENERAL: terminal 2, PCM */
    14, 0x24, 0x02, 0x01, 2, 2, 16, 2, LE24(44100), LE24(48000),        /* Type I: 2 ch, 2 bytes, 16 bits, 2 rates */
    9, 0x05, 0x81, 0x05, LE16(192), 1, 0, 0,                            /* iso async IN 192 */
    7, 0x25, 0x01, 0x01, 0, LE16(0),                                    /* EP_GENERAL: sampling frequency control */
    9, 0x04, 1, 2, 1, 0x01, 0x02, 0x00, 0,                              /* AS alt 2 */
    7, 0x24, 0x01, 2, 1, LE16(0x0001),
    11, 0x24, 0x02, 0x01, 2, 3, 24, 1, LE24(48000),
    9, 0x05, 0x81, 0x05, LE16(288), 1, 0, 0,
    7, 0x25, 0x01, 0x00, 0, LE16(0),
};

/* UAC 2.0: clock source 0x10 behind selector 0x11; capture streaming 1 (24-bit stereo, a feedback-free async IN) and a
 * playback streaming 2 (iso OUT: not ours), then a MIDI streaming interface (not ours) */
static const uint8_t UAC2_IF[] = {
    9, 0x02, LE16(0), 4, 1, 0, 0x80, 50,
    8, 0x0B, 0, 3, 0x01, 0x00, 0x20, 0,                                 /* IAD */
    9, 0x04, 0, 0, 0, 0x01, 0x01, 0x20, 0,                              /* AC, protocol 2.0 */
    9, 0x24, 0x01, LE16(0x0200), 0x08, LE16(64), 0,                     /* header 2.00 */
    8, 0x24, 0x0A, 0x10, 0x03, 0x07, 0, 0,                              /* clock source 0x10 */
    8, 0x24, 0x0B, 0x11, 1, 0x10, 0x03, 0,                              /* clock selector 0x11 <- 0x10 */
    17, 0x24, 0x02, 0x01, LE16(0x0603), 0, 0x11, 2, LE32(3), 0, LE16(0), 0,   /* input terminal: line connector */
    12, 0x24, 0x03, 0x05, LE16(0x0101), 0, 0x01, 0x11, LE16(0), 0,      /* output terminal: USB streaming, clock 0x11 */
    9, 0x04, 1, 0, 0, 0x01, 0x02, 0x20, 0,
    9, 0x04, 1, 1, 1, 0x01, 0x02, 0x20, 0,
    16, 0x24, 0x01, 0x05, 0x00, 0x01, LE32(1), 2, LE32(3), 0,           /* AS_GENERAL 2.0: terminal 5, PCM, 2 ch */
    6, 0x24, 0x02, 0x01, 3, 24,                                         /* Type I: 3-byte subslots, 24 bits */
    7, 0x05, 0x82, 0x05, LE16(294), 1,                                  /* iso async IN 294 */
    8, 0x25, 0x01, 0, 0, 0, LE16(0),
    9, 0x04, 2, 0, 0, 0x01, 0x02, 0x20, 0,
    9, 0x04, 2, 1, 1, 0x01, 0x02, 0x20, 0,
    16, 0x24, 0x01, 0x01, 0x00, 0x01, LE32(1), 2, LE32(3), 0,
    6, 0x24, 0x02, 0x01, 2, 16,
    7, 0x05, 0x03, 0x09, LE16(196), 1,                                  /* iso adaptive OUT: not audio coming out */
    9, 0x04, 3, 0, 2, 0x01, 0x03, 0x00, 0,                              /* MIDI streaming */
    7, 0x24, 0x01, LE16(0x0100), LE16(7),
    9, 0x05, 0x04, 0x02, LE16(64), 0, 0, 0,
};

/* jank's own speaker descriptor (usb_audio.h): an OUT stream only, so nothing to host */
static const uint8_t JANK_SPEAKER[] = {UAC_DESCRIPTOR(4, 7, 5)};

static void test_uac1(void)
{
    struct ju_dev d;
    struct ju_pick p;
    CHECK(ju_parse(UAC1_MIC, sizeof UAC1_MIC, &d) == 2);
    CHECK(!d.uac2 && d.ac_itf == 0);
    CHECK(d.alt[0].itf == 1 && d.alt[0].alt == 1 && d.alt[0].ep == 0x81 && d.alt[0].mps == 192);
    CHECK(d.alt[0].channels == 2 && d.alt[0].subframe == 2 && d.alt[0].bits == 16 && d.alt[0].freq_ctrl);
    CHECK(d.alt[0].nrates == 2 && d.alt[0].rates[0] == 44100u && d.alt[0].rates[1] == 48000u && !d.alt[0].continuous);
    CHECK(d.alt[1].alt == 2 && d.alt[1].subframe == 3 && d.alt[1].bits == 24 && d.alt[1].nrates == 1 && !d.alt[1].freq_ctrl);
    CHECK(ju_choose(&d, &p) == 0 && p.index == 0 && p.rate == 44100u);
    /* 44.1 kHz gone: 48 kHz, still the 16-bit one (fewer bytes) */
    d.alt[0].rates[0] = 32000;
    CHECK(ju_choose(&d, &p) == 0 && p.index == 0 && p.rate == 48000u);
    /* a packet too small for a millisecond of 48 kHz stereo 16-bit (192 bytes) */
    d.alt[0].mps = 188;
    CHECK(ju_choose(&d, &p) == 0 && p.index == 1 && p.rate == 48000u);
    d.alt[1].mps = 700;                    /* over the host's IN FIFO */
    CHECK(ju_choose(&d, &p) == -1);
}

static void test_uac1_continuous(void)
{
    uint8_t x[sizeof UAC1_MIC];
    struct ju_dev d;
    struct ju_pick p;
    size_t at = 0;
    memcpy(x, UAC1_MIC, sizeof x);
    /* alt 1's format: a continuous 8 .. 96 kHz (bSamFreqType 0, then min and max) */
    while (at < sizeof x && !(x[at] == 14 && x[at + 1] == 0x24 && x[at + 2] == 0x02))   /* the Type I format, not the AC input terminal */
        at += x[at];
    x[at + 7] = 0;
    x[at + 8] = 0x40, x[at + 9] = 0x1F, x[at + 10] = 0;                 /* 8000 */
    x[at + 11] = 0x00, x[at + 12] = 0x77, x[at + 13] = 0x01;            /* 96000 */
    CHECK(ju_parse(x, sizeof x, &d) == 2);
    CHECK(d.alt[0].continuous && d.alt[0].rates[0] == 8000u && d.alt[0].rates[1] == 96000u);
    CHECK(ju_choose(&d, &p) == 0 && p.index == 0 && p.rate == 44100u);
}

static void test_uac2(void)
{
    struct ju_dev d;
    struct ju_pick p;
    const uint8_t two[] = {LE16(2), LE32(48000), LE32(48000), LE32(0), LE32(96000), LE32(96000), LE32(0)};
    const uint8_t wide[] = {LE16(1), LE32(8000), LE32(192000), LE32(100)};
    const uint8_t odd[] = {LE16(1), LE32(44000), LE32(49000), LE32(1000)};   /* steps of 1 kHz from 44: 48 only */
    uint8_t s[8], data[4];
    CHECK(ju_parse(UAC2_IF, sizeof UAC2_IF, &d) == 1);
    CHECK(d.uac2 && d.ac_itf == 0 && d.alt[0].itf == 1 && d.alt[0].alt == 1);
    CHECK(d.alt[0].ep == 0x82 && d.alt[0].mps == 294 && d.alt[0].channels == 2 && d.alt[0].subframe == 3 && d.alt[0].bits == 24);
    CHECK(d.alt[0].clock == 0x10);         /* through the selector */
    CHECK(ju_choose(&d, &p) == -1);        /* no rates until the clock is asked */
    CHECK(ju_clock_rates(&d, 0x10, two, sizeof two) == 2 && d.alt[0].nrates == 2);
    CHECK(ju_choose(&d, &p) == 0 && p.rate == 48000u);
    CHECK(ju_clock_rates(&d, 0x10, wide, sizeof wide) == 4);
    CHECK(ju_choose(&d, &p) == 0 && p.rate == 44100u);
    CHECK(ju_clock_rates(&d, 0x10, odd, sizeof odd) == 1 && d.alt[0].rates[0] == 48000u);
    CHECK(ju_clock_rates(&d, 0x10, two, 25) == 1);     /* a short answer: the subranges that fit */
    CHECK(ju_clock_rates(&d, 0x10, two, 1) == 0);
    ju_req_uac2_rate(s, data, 0x10, 0, 48000);
    CHECK(s[0] == 0x21 && s[1] == 0x01 && s[2] == 0x00 && s[3] == 0x01 && s[4] == 0x00 && s[5] == 0x10 && s[6] == 4 && s[7] == 0);
    CHECK(data[0] == 0x80 && data[1] == 0xBB && data[2] == 0 && data[3] == 0);
    ju_req_uac2_range(s, 0x10, 0);
    CHECK(s[0] == 0xA1 && s[1] == 0x02 && s[3] == 0x01 && s[5] == 0x10 && s[6] == JU_RANGE_LEN);
}

static void test_requests(void)
{
    uint8_t s[8], data[3];
    ju_req_set_interface(s, 1, 2);
    CHECK(s[0] == 0x01 && s[1] == 0x0B && s[2] == 2 && s[3] == 0 && s[4] == 1 && s[5] == 0 && s[6] == 0 && s[7] == 0);
    ju_req_uac1_rate(s, data, 0x81, 44100);
    CHECK(s[0] == 0x22 && s[1] == 0x01 && s[2] == 0x00 && s[3] == 0x01 && s[4] == 0x81 && s[6] == 3);
    CHECK(data[0] == 0x44 && data[1] == 0xAC && data[2] == 0x00);
}

static void test_not_ours(void)
{
    struct ju_dev d;
    CHECK(ju_parse(JANK_SPEAKER, sizeof JANK_SPEAKER, &d) == 0);
    CHECK(ju_parse(UAC1_MIC, 40, &d) == 0);             /* cut before any streaming interface */
    {
        uint8_t bad[sizeof UAC1_MIC];
        memcpy(bad, UAC1_MIC, sizeof bad);
        bad[9] = 0;                                     /* a zero length: the walk stops there */
        CHECK(ju_parse(bad, sizeof bad, &d) == 0);
    }
}

static void test_string(void)
{
    const uint8_t s[] = {14, 0x03, 'O', 0, 'P', 0, '-', 0, 'Z', 0, 0xE9, 0, 'x', 0x30};
    char out[16];
    CHECK(ju_string_ascii(s, sizeof s, out, sizeof out) == 6 && !strcmp(out, "OP-Z??"));
    CHECK(ju_string_ascii(s, sizeof s, out, 3) == 2 && !strcmp(out, "OP"));
    CHECK(ju_string_ascii(s, 5, out, sizeof out) == 1 && !strcmp(out, "O"));   /* bLength over the buffer */
}

int main(void)
{
    test_uac1();
    test_uac1_continuous();
    test_uac2();
    test_requests();
    test_not_ours();
    test_string();
    printf("uacdesc: %d checks, %d failed\n", checks, fails);
    return fails;
}
