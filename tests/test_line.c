/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the LINE IN path (core/jank_line.c) and its resampler (core/jank_pcm.c jp_cr): the ADC framing, the
 * timestamp fit, DC / gain / level, the cell's average and percent, the cubic resampler, and the whole chain on a click
 * train whose times are known. Exit status: the failed checks. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_line.h"
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

#define LINE_CH 8u
#define CELL_CH 3

static uint32_t word(uint32_t data, uint32_t ch, uint32_t unit) { return (data & 0xFFFu) | (ch & 0xFu) << 13 | (unit & 1u) << 17; }

/* the ADC's rate: exact intervals (adc_hal.c's formula), so a line sample is 24 us with or without the cell */
static void test_rates(void)
{
    CHECK(jl_conv_hz(2) == 83333u && jl_interval(80000000u, jl_conv_hz(2)) == 30u);
    CHECK(jl_conv_hz(1) == 41666u && jl_interval(80000000u, jl_conv_hz(1)) == 60u);
    /* the rate jl_conv_hz must not be rounded up: 41,667 would give 59 (42,373 Hz), 83,334 would give 29 */
    CHECK(jl_interval(80000000u, 41667u) == 59u && jl_interval(80000000u, 83334u) == 29u);
    /* 80 MHz / 16 / 2 / 30 / 2 channels = one line sample every 24 us */
    CHECK(80000000.0 / 16 / 2 / 30 / 2 == (double)JL_RATE_NUM / JL_RATE_DEN);
    CHECK(1e6 * JL_RATE_DEN / JL_RATE_NUM == JL_SAMPLE_US && JL_FRAME_US == 4800);
}

static void test_parse(void)
{
    uint32_t conv[JL_CONV_MAX];
    uint16_t out[JL_FRAME];
    struct jl_parse r;
    /* [line, cell] x 200: lines 0..199, cells 1000 */
    for (unsigned i = 0; i < JL_FRAME; i++) {
        conv[2u * i] = word(i, LINE_CH, 0) | 0xABC00000u;   /* (the reserved bits are ignored) */
        conv[2u * i + 1u] = word(1000, CELL_CH, 0);
    }
    jl_parse(conv, JL_CONV_MAX, LINE_CH, CELL_CH, out, &r);
    CHECK(r.line_n == JL_FRAME && r.cell_n == JL_FRAME && r.cell_sum == 1000u * JL_FRAME && r.other == 0);
    CHECK(out[0] == 0 && out[199] == 199);
    /* a bad word (ADC2's unit bit, another channel) is not a line sample: the frame is padded with the last value */
    conv[10] = word(4095, LINE_CH, 1);
    conv[12] = word(4095, 5, 0);
    jl_parse(conv, JL_CONV_MAX, LINE_CH, CELL_CH, out, &r);
    CHECK(r.line_n == JL_FRAME - 2u && r.other == 2 && out[5] == 7 && out[197] == 199 && out[198] == 199 && out[199] == 199);
    /* no cell channel (the XIAO bench): [line] x 200, a cell-channel word is "other" */
    for (unsigned i = 0; i < JL_FRAME; i++)
        conv[i] = word(4000u - i, LINE_CH, 0);
    conv[0] = word(1, CELL_CH, 0);
    jl_parse(conv, JL_FRAME, LINE_CH, -1, out, &r);
    CHECK(r.line_n == JL_FRAME - 1u && r.cell_n == 0 && r.other == 1 && out[0] == 3999 && out[199] == 3801);
    /* too many line words: the frame keeps 200 */
    for (unsigned i = 0; i < JL_CONV_MAX; i++)
        conv[i] = word(i, LINE_CH, 0);
    jl_parse(conv, JL_CONV_MAX, LINE_CH, CELL_CH, out, &r);
    CHECK(r.line_n == JL_FRAME && r.other == JL_FRAME && out[199] == 199);
    /* an empty frame: 200 zeros */
    jl_parse(conv, 0, LINE_CH, CELL_CH, out, &r);
    CHECK(r.line_n == 0 && out[0] == 0 && out[199] == 0);
}

/* the mini 3 (lcd2): LINE_IN GPIO7 = ADC1 channel 6, the cell GPIO5 = channel 4; LINE_SENSE GPIO8 = channel 7 is in no pattern yet, so
 * a word of it (it should never come) is "other", never mixed into the line */
static void test_mini3_channels(void)
{
    uint32_t conv[JL_CONV_MAX];
    uint16_t out[JL_FRAME];
    struct jl_parse r;
    for (unsigned i = 0; i < JL_FRAME; i++) {
        conv[2u * i] = word(1900u + i % 7u, 6, 0);
        conv[2u * i + 1u] = word(1500, 4, 0);
    }
    jl_parse(conv, JL_CONV_MAX, 6, 4, out, &r);
    CHECK(r.line_n == JL_FRAME && r.cell_n == JL_FRAME && r.cell_sum == 1500u * JL_FRAME && r.other == 0 && out[8] == 1901);
    conv[20] = word(3000, 7, 0);
    jl_parse(conv, JL_CONV_MAX, 6, 4, out, &r);
    CHECK(r.line_n == JL_FRAME - 1u && r.other == 1 && out[10] == 1900u + 11u % 7u);
    /* the same two-channel pattern, so the same 24 us line sample as the mini 2.2 */
    CHECK(jl_interval(80000000u, jl_conv_hz(2)) == 30u && JL_SAMPLE_US == 24);
    /* the bias 1.467 V at the 12 dB range (nominal 3.1 V) is code ~1938: inside the 12-bit range with room both ways */
    CHECK(4095.0 * 1.467 / 3.1 > 1900 && 4095.0 * 1.467 / 3.1 < 2000);
}

/* stamps: frame k ends with sample 200 (k + 1) - 1 at T0 + 24 (200 (k + 1) - 1) + latency */
static double urand(void) { return rand() / (RAND_MAX + 1.0); }

static void test_fit(void)
{
    struct jl_fit f;
    const int64_t T0 = 123456789;
    int64_t n_end = 0;
    srand(7);
    jl_fit_init(&f);
    CHECK(!jl_fit_ready(&f));
    /* 10 s of frames, latency 3..8 us with a 40 us burst now and then: t0 = T0 + 3 (the lowest), the latency 0..5 (+40) above it */
    for (int k = 0; k < 2083; k++) {
        int64_t lat = 3 + (int64_t)(urand() * 6.0) + (k % 97 == 0 ? 40 : 0);
        n_end += JL_FRAME;
        jl_fit_add(&f, n_end, T0 + (n_end - 1) * JL_SAMPLE_US + lat);
    }
    CHECK(jl_fit_ready(&f));
    CHECK(llabs(jl_fit_t0(&f) - (T0 + 3)) <= 1);
    CHECK(jl_fit_time(&f, 1000) == jl_fit_t0(&f) + 24000);
    CHECK(f.lat_avg_us >= 1 && f.lat_avg_us <= 4 && f.lat_max_us >= 40 && f.lat_max_us <= 47 && f.lat_max_ever_us >= 40);
    CHECK(f.drift_ppb > -2000 && f.drift_ppb < 2000 && f.steps == 0 && f.blocks == 2083u / JL_FIT_BLOCK);
    /* 3 frames lost before the interrupt saw them: every later stamp is 14.4 ms later than its index says. The fit takes
     * it within two blocks, counted as a step */
    for (int k = 0; k < 3 * (int)JL_FIT_BLOCK; k++) {
        n_end += JL_FRAME;
        jl_fit_add(&f, n_end, T0 + 3 * JL_FRAME_US + (n_end - 1) * JL_SAMPLE_US + 3);
    }
    CHECK(jl_fit_t0(&f) == T0 + 3 * JL_FRAME_US + 3 && f.steps == 1);
    /* a step down is taken at once */
    n_end += JL_FRAME;
    jl_fit_add(&f, n_end, T0 + (n_end - 1) * JL_SAMPLE_US + 3);
    CHECK(jl_fit_t0(&f) == T0 + 3);
    /* a drift of +50 ppm is reported (in case the timer and the ADC ever ran from different clocks) */
    jl_fit_init(&f);
    n_end = 0;
    for (int k = 0; k < 5 * (int)JL_FIT_BLOCK; k++) {
        n_end += JL_FRAME;
        double t = (double)(n_end - 1) * JL_SAMPLE_US;
        jl_fit_add(&f, n_end, T0 + (int64_t)llround(t * 1.00005) + 2);
    }
    CHECK(f.drift_ppb > 48000 && f.drift_ppb < 52000);
}

static void test_cond(void)
{
    static uint16_t raw[41667];
    static int16_t out[41667];
    struct jl_cond c;
    struct jl_level l;
    /* 1 kHz at 1000 codes around 1900 (the board's 1.47 V bias): DC gone, 16000 peak, -6.2 dBFS peak, -9.2 dB RMS */
    for (size_t i = 0; i < 41667; i++)
        raw[i] = (uint16_t)lround(1900.0 + 1000.0 * sin(2.0 * M_PI * 1000.0 * i * JL_SAMPLE_US * 1e-6));
    jl_cond_init(&c);
    jl_cond_run(&c, raw, 41667, out);
    CHECK(abs(jl_cond_dc(&c) - 1900) <= 8);      /* (the one pole ripples +-6.5 codes at 1 kHz) */
    {
        double mean = 0.0, peak = 0.0;
        for (size_t i = 20833; i < 41667; i++) {
            mean += out[i];
            peak = fmax(peak, fabs((double)out[i]));
        }
        mean /= 20834.0;
        CHECK(fabs(mean) < 16.0);
        CHECK(peak > 15900.0 && peak < 16100.0);
    }
    jl_cond_window(&c, &l);
    CHECK(l.peak_db == -6 && l.rms_db == -9 && l.clips == 0);
    /* +12 dB: 4x, clipping at full scale, counted */
    jl_cond_gain(&c, 12);
    CHECK(c.gain_db == 12 && c.gain_q12 == 16306);
    jl_cond_run(&c, raw, 41667, out);
    jl_cond_window(&c, &l);
    CHECK(l.peak_db == 0 && l.clips > 1000);
    /* -6 dB and the range clamp */
    jl_cond_gain(&c, -6);
    CHECK(c.gain_q12 == 2053);
    jl_cond_gain(&c, -40);
    CHECK(c.gain_db == JL_GAIN_MIN_DB);
    jl_cond_gain(&c, 0);
    /* silence: -99 */
    for (size_t i = 0; i < 4167; i++)
        raw[i] = 1900;
    jl_cond_run(&c, raw, 4167, out);
    jl_cond_window(&c, &l);
    jl_cond_run(&c, raw, 4167, out);
    jl_cond_window(&c, &l);
    CHECK(l.peak_db <= -70 && l.rms_db <= -70);   /* (the DC estimate settles within 4 steps of int16: -78 dB) */
    jl_cond_window(&c, &l);              /* an empty window */
    CHECK(l.peak_db == -99 && l.rms_db == -99);
}

static void test_cell(void)
{
    struct jl_cell c;
    jl_cell_init(&c);
    /* 200 a frame, one second's block 41,600: the average of 2048 and 2049 alternating = 2048.5 -> x16 32776 */
    for (int k = 0; k < 207; k++)
        CHECK(jl_cell_add(&c, 2048u * 100u + 2049u * 100u, 200u, 41600u) == 0);
    CHECK(jl_cell_add(&c, 2048u * 100u + 2049u * 100u, 200u, 41600u) == 1);
    CHECK(c.avg_x16 == 32776u && c.blocks == 1u);
    CHECK(jl_cell_add(&c, 0, 0, 41600u) == 0);
    /* percent: no cell, the ends, the curve, the empty point moved */
    CHECK(jl_batt_pct(0, 3300) == -1 && jl_batt_pct(1999, 3300) == -1);
    CHECK(jl_batt_pct(3000, 3300) == 0 && jl_batt_pct(3300, 3300) == 0 && jl_batt_pct(4200, 3300) == 100 && jl_batt_pct(4350, 3300) == 100);
    CHECK(jl_batt_pct(3700, 3300) == 33 && jl_batt_pct(3725, 3300) == 39 && jl_batt_pct(4000, 3300) == 80);
    CHECK(jl_batt_pct(3500, 3500) == 0 && jl_batt_pct(4200, 3500) == 100 && jl_batt_pct(3600, 3500) > 0);
    CHECK(jl_batt_pct(3600, 3300) > jl_batt_pct(3550, 3300));
    /* the lcd2's divider (R19 200 k / R20 100 k, ratio 3): a full cell is 1400 mV at the pin, empty 1100 mV */
    CHECK(jl_cell_pin_pct(1400, 3, 3300) == 100 && jl_cell_pin_pct(1233, 3, 3300) == jl_batt_pct(3699, 3300));
    CHECK(jl_cell_pin_pct(1100, 3, 3300) == 0 && jl_cell_pin_pct(1000, 3, 3300) == 0);
    CHECK(jl_cell_pin_pct(1333, 3, 3300) == jl_batt_pct(3999, 3300) && jl_cell_pin_pct(1167, 3, 3300) == jl_batt_pct(3501, 3300));
    /* fitted (>= 0) only above 2000 mV at the cell = 667 mV at the pin: a grounded or floating-low pin is no cell, never 0 % */
    CHECK(jl_cell_pin_pct(0, 3, 3300) == -1 && jl_cell_pin_pct(5, 3, 3300) == -1 && jl_cell_pin_pct(666, 3, 3300) == -1);
    CHECK(jl_cell_pin_pct(667, 3, 3300) == 0 && jl_cell_pin_pct(-1, 3, 3300) == -1 && jl_cell_pin_pct(1400, 0, 3300) == -1);
    CHECK(jl_cell_pin_pct(1000, 2, 3300) == 0);
}

#define IN_N 125000u                   /* 3 s at 41,666.67 Hz */
static int16_t in_s[IN_N];
static int16_t out_s[132300u + 64u];

static size_t run_cr(size_t chunk)
{
    struct jp_cr r;
    size_t w = 0;
    CHECK(jp_cr_init(&r, JL_RATE_NUM, JL_RATE_DEN, 44100u) == 0);
    for (size_t i = 0; i < IN_N; i += chunk) {
        size_t n = IN_N - i < chunk ? IN_N - i : chunk;
        w += jp_cr_run(&r, in_s + i, n, out_s + w, sizeof out_s / sizeof out_s[0] - w);
    }
    return w;
}

static void test_cubic(void)
{
    struct jp_cr r;
    CHECK(jp_cr_init(&r, JL_RATE_NUM, JL_RATE_DEN, 44100u) == 0 && r.num == 1250u && r.den == 1323u);
    CHECK(jp_cr_init(&r, 0, 1, 44100u) == -1 && jp_cr_init(&r, 1, 0, 44100u) == -1);
    CHECK(jp_cr_init(&r, 100003u, 1, 44100u) == -1);            /* a prime rate: a denominator over 16 bits */
    /* a ramp: the cubic is exact on it, so output m is input position m x 1250 / 1323, i.e. the timing is exact */
    for (size_t i = 0; i < IN_N; i++)
        in_s[i] = (int16_t)((int32_t)(i % 4096u) * 8 - 16384);
    {
        size_t n = run_cr(200), bad = 0;
        /* inputs 0..124999 -> outputs at positions < 124998 (each needs two inputs past it): m <= 124998 x 1323 / 1250 */
        CHECK(n == 132298u);
        for (size_t m = 0; m < n; m++) {
            double pos = (double)m * 1250.0 / 1323.0;
            size_t k = (size_t)pos;
            if (k % 4096u > 4092u || k % 4096u < 2u)
                continue;              /* the ramp's wrap */
            if (fabs(out_s[m] - ((pos - (double)(k / 4096u) * 4096.0) * 8.0 - 16384.0)) > 1.0)
                bad++;
        }
        CHECK(bad == 0);
    }
    /* chunking does not change a single sample */
    {
        static int16_t a[132300u + 64u];
        size_t n1 = run_cr(200);
        memcpy(a, out_s, n1 * sizeof a[0]);
        size_t n2 = run_cr(7);
        CHECK(n1 == n2 && !memcmp(a, out_s, n1 * sizeof a[0]));
    }
    /* sines: the error against the ideal sine at the output's times. 1 kHz < -60 dB, 5 kHz < -35 dB (the cubic's own
     * response; LINE IN's front end has its 8.5 kHz pole before it) */
    {
        const double freqs[] = {1000.0, 5000.0}, limits[] = {-60.0, -35.0};
        for (size_t t = 0; t < 2; t++) {
            double err = 0.0, sig = 0.0;
            for (size_t i = 0; i < IN_N; i++)
                in_s[i] = (int16_t)lround(16000.0 * sin(2.0 * M_PI * freqs[t] * i / (125000.0 / 3.0)));
            size_t n = run_cr(200);
            for (size_t m = 100; m < n; m++) {
                double ideal = 16000.0 * sin(2.0 * M_PI * freqs[t] * m / 44100.0);
                err += (out_s[m] - ideal) * (out_s[m] - ideal);
                sig += ideal * ideal;
            }
            double db = 10.0 * log10(err / sig);
            printf("cubic %.0f Hz: error %.1f dB\n", freqs[t], db);
            CHECK(db < limits[t]);
        }
    }
}

/* The chain as linein.c runs it: a click train at known local times -> TYPE2 frames with the cell interleaved -> the
 * interrupt's stamps with latency -> parse, fit, condition, resample -> each output's time as usb_audio.c computes it
 * (origin + m x 10000 / 441, floor). The clicks found in the 44.1 kHz stream sit at their true times within 25 us. */
static void test_chain(void)
{
    enum { SECONDS = 6, FRAMES = SECONDS * 208 };
    const int64_t T0 = 5000000;        /* local us of line sample 0 */
    static int16_t res[SECONDS * 44100 + 64];
    struct jl_fit f;
    struct jl_cond c;
    struct jl_cell cell;
    struct jp_cr r;
    size_t w = 0, found = 0, bad = 0;
    double worst = 0.0;
    int64_t n_end = 0;
    double click_us[64];
    size_t clicks = 0;
    srand(3);
    jl_fit_init(&f);
    jl_cond_init(&c);
    jl_cell_init(&cell);
    CHECK(jp_cr_init(&r, JL_RATE_NUM, JL_RATE_DEN, 44100u) == 0);
    /* clicks every 0.5 s from 0.3 s, at times that fall between samples */
    for (double t = 300000.0 + 7.3; t < SECONDS * 1e6 - 100000.0; t += 500000.0 + 11.1)
        click_us[clicks++] = t;
    for (int k = 0; k < FRAMES; k++) {
        uint32_t conv[JL_CONV_MAX];
        uint16_t raw[JL_FRAME];
        int16_t x[JL_FRAME];
        struct jl_parse p;
        for (unsigned i = 0; i < JL_FRAME; i++) {
            double t = (double)(n_end + i) * JL_SAMPLE_US, v = 1900.0;
            for (size_t q = 0; q < clicks; q++) {
                double d = (t - click_us[q]) / 60.0;           /* a 60 us Gaussian pulse, band-limited enough */
                v += 1500.0 * exp(-0.5 * d * d);
            }
            conv[2u * i] = word((uint32_t)lround(v), LINE_CH, 0);
            conv[2u * i + 1u] = word(2100, CELL_CH, 0);
        }
        n_end += JL_FRAME;
        jl_fit_add(&f, n_end, T0 + (n_end - 1) * JL_SAMPLE_US + 2 + (int64_t)(urand() * 10.0));
        jl_parse(conv, JL_CONV_MAX, LINE_CH, CELL_CH, raw, &p);
        CHECK(p.line_n == JL_FRAME && p.cell_n == JL_FRAME);
        jl_cell_add(&cell, p.cell_sum, p.cell_n, 41600u);
        jl_cond_run(&c, raw, JL_FRAME, x);
        w += jp_cr_run(&r, x, JL_FRAME, res + w, sizeof res / sizeof res[0] - w);
    }
    CHECK(cell.avg_x16 == 2100u * 16u);
    {
        int64_t origin = jl_fit_time(&f, 0); /* the resampler started at line sample 0 */
        CHECK(llabs(origin - (T0 + 2)) <= 1);
        for (size_t q = 0; q < clicks; q++) {
            /* the peak near the click, refined by a parabola */
            int64_t guess = (int64_t)((click_us[q] + T0 - origin) * 0.0441);
            size_t best = (size_t)guess;
            for (int64_t m = guess - 20; m <= guess + 20; m++)
                if (m > 0 && (size_t)m + 1u < w && res[m] > res[best])
                    best = (size_t)m;
            if (best == 0 || best + 1u >= w)
                continue;
            found++;
            double a = res[best - 1u], b = res[best], cc = res[best + 1u];
            double frac = 0.5 * (a - cc) / (a - 2.0 * b + cc);
            int64_t t_best = origin + (int64_t)best * 10000 / 441;     /* usb_audio.c's sample time */
            double t_est = (double)t_best + frac * 1e6 / 44100.0;
            double err = t_est - (click_us[q] + T0);
            worst = fmax(worst, fabs(err));
            if (fabs(err) > 25.0) {
                printf("click %zu: %.1f us off\n", q, err);
                bad++;
            }
        }
    }
    printf("chain: %zu clicks, the worst %.1f us from its true time\n", found, worst);
    CHECK(found == clicks && bad == 0);
}

int main(void)
{
    test_rates();
    test_parse();
    test_mini3_channels();
    test_fit();
    test_cond();
    test_cell();
    test_cubic();
    test_chain();
    printf("line: %d of %d checks failed\n", fails, checks);
    return fails;
}
