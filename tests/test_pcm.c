/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the hosted device's PCM conversion and the resampler (core/jank_pcm.c). Exit status: the failed checks. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "jank_pcm.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void test_mono(void)
{
    /* 16-bit stereo: L 1000, R -3000 -> -1000; L -1, R 0 -> -1 (floor) */
    const uint8_t s16[] = {0xE8, 0x03, 0x48, 0xF4, 0xFF, 0xFF, 0x00, 0x00};
    /* 24-bit stereo in 3 bytes: the top 16 bits; 0x123456 -> 0x1234, 0xFEDCBA -> 0xFEDC */
    const uint8_t s24[] = {0x56, 0x34, 0x12, 0x56, 0x34, 0x12, 0xBA, 0xDC, 0xFE, 0xBA, 0xDC, 0xFE};
    /* 24 bits in 4-byte subframes, MSB-aligned: 0x12345600 -> 0x1234 */
    const uint8_t s32[] = {0x00, 0x56, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00};
    const uint8_t m16[] = {0x01, 0x80, 0xFF, 0x7F, 0x00};      /* mono; a trailing half sample is ignored */
    int16_t out[8];
    CHECK(jp_to_mono16(s16, sizeof s16, 2, 2, out, 8) == 2 && out[0] == -1000 && out[1] == -1);
    CHECK(jp_to_mono16(s24, sizeof s24, 2, 3, out, 8) == 2 && out[0] == 0x1234 && out[1] == (int16_t)0xFEDC);
    CHECK(jp_to_mono16(s32, sizeof s32, 2, 4, out, 8) == 1 && out[0] == 0x1234 / 2);
    CHECK(jp_to_mono16(s32, sizeof s32, 1, 4, out, 8) == 2 && out[0] == 0x1234 && out[1] == 0);
    CHECK(jp_to_mono16(m16, sizeof m16, 1, 2, out, 8) == 2 && out[0] == -32767 && out[1] == 32767);
    CHECK(jp_to_mono16(s16, sizeof s16, 2, 2, out, 1) == 1);  /* cap */
    CHECK(jp_to_mono16(s16, sizeof s16, 0, 2, out, 8) == 0);
    CHECK(jp_to_mono16(s16, sizeof s16, 2, 1, out, 8) == 0);
    CHECK(jp_to_mono16(s16, sizeof s16, 2, 5, out, 8) == 0);
}

#define SECS 1
static int16_t in_buf[96000 * SECS];
static int16_t out_buf[44100 * SECS + 64];

/* a sine of f Hz at amplitude a through the resampler, fed in 1 ms packets as USB gives them; the error against the ideal
 * sine at 44.1 kHz (delayed by the filter), RMS over the second half, relative to a */
static double run_sine(uint32_t rate, double f, double a, size_t *n_out, double *rms_out)
{
    static struct jp_rs r;
    size_t n_in = rate * SECS, w = 0;
    double err = 0.0, pow_out = 0.0, delay;
    int cnt = 0;
    CHECK(jp_rs_init(&r, rate) == 0);
    for (size_t i = 0; i < n_in; i++)
        in_buf[i] = (int16_t)lround(a * sin(2.0 * M_PI * f * (double)i / rate));
    for (size_t i = 0; i < n_in; i += rate / 1000u)
        w += jp_rs_run(&r, in_buf + i, rate / 1000u, out_buf + w, sizeof out_buf / sizeof out_buf[0] - w);
    delay = (r.up == 1u && r.down == 1u) ? 0.0 : (r.up * JP_TAPS - 1.0) / 2.0 / ((double)r.up * rate);   /* exact: delay_us rounds */
    CHECK(fabs(jp_rs_delay_us(&r, rate) * 1e-6 - delay) <= 0.5e-6);
    for (size_t n = w / 2; n < w; n++) {
        double t = (double)n / JP_OUT_RATE - delay;
        double d = out_buf[n] - a * sin(2.0 * M_PI * f * t);
        err += d * d;
        pow_out += (double)out_buf[n] * out_buf[n];
        cnt++;
    }
    *n_out = w;
    *rms_out = sqrt(pow_out / cnt);
    return sqrt(err / cnt) / a;
}

static void test_resample(void)
{
    size_t n;
    double rms, e;
    static struct jp_rs r;
    /* 48 kHz: 147 / 160; one second in -> one second out (+-1 sample) */
    e = run_sine(48000, 1000.0, 16000.0, &n, &rms);
    printf("48k -> 44.1k, 1 kHz: %zu samples, error %.5f of full\n", n, e);
    CHECK(n >= 44099 && n <= 44101);
    CHECK(e < 0.001);
    e = run_sine(48000, 100.0, 16000.0, &n, &rms);   /* the tracker's band: kick drums */
    printf("48k -> 44.1k, 100 Hz: error %.5f\n", e);
    CHECK(e < 0.001);
    e = run_sine(48000, 8000.0, 16000.0, &n, &rms);
    printf("48k -> 44.1k, 8 kHz: error %.5f\n", e);
    CHECK(e < 0.001);
    /* above the output's Nyquist frequency: 23 kHz would fold to 21.1 kHz; the filter takes it down */
    e = run_sine(48000, 23000.0, 16000.0, &n, &rms);
    printf("48k -> 44.1k, 23 kHz: %.1f dB left\n", 20.0 * log10(rms / (16000.0 / sqrt(2.0))));
    CHECK(rms < 16000.0 / sqrt(2.0) * 0.05);           /* at least 26 dB down */
    /* 96 kHz: 147 / 320 */
    e = run_sine(96000, 1000.0, 16000.0, &n, &rms);
    printf("96k -> 44.1k, 1 kHz: %zu samples, error %.5f\n", n, e);
    CHECK(n >= 44099 && n <= 44101 && e < 0.001);
    /* 44.1 kHz: untouched, no delay */
    CHECK(jp_rs_init(&r, 44100) == 0 && jp_rs_delay_us(&r, 44100) == 0);
    {
        int16_t x[5] = {1, -2, 3, -4, 32767}, y[5];
        CHECK(jp_rs_run(&r, x, 5, y, 5) == 5 && !memcmp(x, y, sizeof x));
        CHECK(jp_rs_run(&r, x, 5, y, 3) == 3);
    }
    /* full scale does not wrap: the rails held come out as the rails (each phase sums to 1: rounding could pass them) */
    CHECK(jp_rs_init(&r, 48000) == 0);
    {
        int16_t hi[480], lo[480], y[480];
        size_t got;
        int ok = 1;
        for (int i = 0; i < 480; i++) {
            hi[i] = 32767;
            lo[i] = -32768;
        }
        got = jp_rs_run(&r, hi, 480, y, 480);
        for (size_t i = 20; i < got; i++)
            ok &= y[i] >= 32760;
        CHECK(got >= 440 && got <= 442 && ok);
        got = jp_rs_run(&r, lo, 480, y, 480);
        for (size_t i = 20; i < got; i++)
            ok &= y[i] <= -32760;
        CHECK(got >= 440 && got <= 442 && ok);
    }
    /* reset: silence in, silence out */
    jp_rs_reset(&r);
    {
        int16_t z[48] = {0}, y[48];
        size_t got = jp_rs_run(&r, z, 48, y, 48);
        int quiet = 1;
        for (size_t i = 0; i < got; i++)
            quiet &= y[i] == 0;
        CHECK(got >= 44 && got <= 45 && quiet);
    }
    CHECK(jp_rs_init(&r, 0) == -1);
    CHECK(jp_rs_init(&r, 32000) == -1);   /* 441 / 320: L over JP_PHASES_MAX */
    CHECK(jp_rs_init(&r, 88200) == 0 && r.up == 1u && r.down == 2u);
    printf("delay at 48 kHz: %u us\n", (unsigned)(jp_rs_init(&r, 48000), jp_rs_delay_us(&r, 48000)));
}

int main(void)
{
    test_mono();
    test_resample();
    printf("pcm: %d checks, %d failed\n", checks, fails);
    return fails;
}
