/* SPDX-License-Identifier: GPL-3.0-only */
/* A hosted USB audio device's PCM into what the beat tracker takes: mono int16 at 44.1 kHz.
 *
 * - jp_to_mono16: interleaved little-endian PCM (USB Audio Type I: each sample in a subframe of 2, 3 or 4 bytes, its valid
 *   bits MSB-aligned, so the top 16 bits are the sample's top 16 bits) -> mono int16, (L + R) / 2 from the first two
 *   channels (what the player and the device stack's speaker get: the mid), mono as it is.
 * - jp_rs: a rational resampler in -> 44.1 kHz (48 kHz: 147/160), polyphase windowed sinc, JP_TAPS taps a phase, Q15
 *   coefficients, its cutoff 0.45 x the lower rate. 44.1 kHz in passes through untouched. */
#ifndef JANK_PCM_H
#define JANK_PCM_H
#include <stddef.h>
#include <stdint.h>

#define JP_OUT_RATE 44100u
#define JP_TAPS 24u                      /* per phase: ~1 M multiply-adds a second at 48 kHz [est: under 2 % of a core] */
#define JP_PHASES_MAX 147u               /* L of L/M: 147 for 48 and 96 kHz, 1 for 88.2 kHz; the table is 147 x 24 x 2 = 7 KiB */

/* frames converted (at most cap); 0 for a format it does not take (channels 0, subframe not 2..4) */
size_t jp_to_mono16(const uint8_t *in, size_t bytes, unsigned channels, unsigned subframe, int16_t *out, size_t cap);

struct jp_rs {
    uint32_t up, down;                   /* L, M: out = in x L / M */
    uint32_t phase;                      /* n x M - i x L of the next output, 0 .. L + M */
    uint32_t pos;                        /* where the next input goes in hist */
    int16_t hist[2u * JP_TAPS];          /* each sample twice (at pos and pos + TAPS): the last TAPS are one straight run */
    int16_t coef[JP_PHASES_MAX][JP_TAPS];
};

/* 0 ok; -1 a rate it cannot take (0, or L over JP_PHASES_MAX) */
int jp_rs_init(struct jp_rs *r, uint32_t in_rate);
void jp_rs_reset(struct jp_rs *r);                 /* history to silence, phase to 0: a new stream */
/* n input samples -> output samples written to out (at most cap; the rest of that input's outputs are lost, so give
 * cap >= n x L / M + 2) */
size_t jp_rs_run(struct jp_rs *r, const int16_t *in, size_t n, int16_t *out, size_t cap);
/* the filter's delay in microseconds (half its length at the input rate): an output sample stands for the input this
 * long before it */
uint32_t jp_rs_delay_us(const struct jp_rs *r, uint32_t in_rate);

/* jp_cr: a cubic (Catmull-Rom) resampler for a ratio jp_rs cannot take, the LINE IN's 125000/3 Hz -> 44.1 kHz (L = 1323).
 * Exact: output m is the input at position m x step_num / step_den (input samples), with no drift and no delay of its own
 * (the 4-point kernel is centred: output m waits only for input floor(pos) + 2). Its images are weak above ~10 kHz, which
 * the LINE IN's front end already cuts (an 8.5 kHz pole, hardware/mini2/README.md); the tracker's bands end at 16 kHz.
 * About 30 cycles an output [est]. */
struct jp_cr {
    uint32_t num, den;                  /* input samples per output: num / den (1250 / 1323 for 41,666.67 -> 44,100) */
    uint32_t ph;                        /* the next output's fraction past x[1], in 1/den; >= den: wait for more input */
    int16_t x[4];                       /* the last four inputs, oldest first */
};
/* in_rate_num / in_rate_den Hz -> out_rate Hz. 0 ok; -1 a ratio that does not fit (0, or a reduced denominator over 65535) */
int jp_cr_init(struct jp_cr *r, uint32_t in_rate_num, uint32_t in_rate_den, uint32_t out_rate);
void jp_cr_reset(struct jp_cr *r);                  /* a new stream: output 0 is input 0 */
/* n inputs -> outputs written (at most cap; give cap >= n x den / num + 2) */
size_t jp_cr_run(struct jp_cr *r, const int16_t *in, size_t n, int16_t *out, size_t cap);
#endif
