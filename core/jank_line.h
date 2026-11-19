/* SPDX-License-Identifier: GPL-3.0-only */
/* The LINE IN path's portable parts (firmware/main/linein.c runs them; tests/test_line.c tests them on the host).
 *
 * The ESP32-S3's ADC1 runs in continuous mode (GDMA) over a pattern of [LINE IN] or [LINE IN, cell]: one conversion every
 * 12 us (83,333 Hz) with the cell, every 24 us (41,666 Hz) without, so the LINE IN channel is sampled exactly every 24 us
 * either way (worktrees/timing-scratch/TIMING.md trap 5: APB 80 MHz / 16 / 2 / interval, adc_hal.c). Each DMA frame holds
 * JL_FRAME line samples (4.8 ms).
 *
 * - jl_parse: one DMA frame's conversion words (the S3's TYPE2 format: data bits 0-11, channel bits 13-16, unit bit 17)
 *   -> the line samples in order (exactly JL_FRAME: a short frame is padded with its last value and counted) and the cell's
 *   sum. Runs in the ADC interrupt (linein.c puts it in IRAM with a linker fragment): no calls, no division.
 * - jl_fit: sample n's local time is t0 + 24 n us. t0 from the interrupts' timer stamps: each frame's stamp is late by the
 *   interrupt's latency (>= 0, a few us, more under critical sections), so t0 is the LOWER envelope of stamp - 24 n over the
 *   last two blocks of JL_FIT_BLOCK frames (~2 s). The envelope follows a step up (frames lost before the interrupt saw them)
 *   within two blocks and a step down at once. Also reported: the latency's mean and max, and the drift between blocks.
 * - jl_cond: raw 12-bit codes -> int16 (x 16: the ADC's full range is full scale), the DC removed (one pole, ~6.5 Hz), the
 *   gain (-12..+12 dB, Q12), clipping counted; the level's peak and mean square over a window.
 * - jl_cell: the cell channel's codes averaged per second (1 s blocks), and the percent from millivolts. */
#ifndef JANK_LINE_H
#define JANK_LINE_H
#include <stddef.h>
#include <stdint.h>

#define JL_SAMPLE_US 24                /* one LINE IN sample: 2 conversions at 12 us, or 1 at 24 us */
#define JL_RATE_NUM 125000u            /* the line's rate, 125000 / 3 = 41,666.67 Hz, as a fraction */
#define JL_RATE_DEN 3u
#define JL_FRAME 200u                  /* line samples a DMA frame: 4.8 ms */
#define JL_FRAME_US (JL_FRAME * JL_SAMPLE_US)
#define JL_CONV_MAX (2u * JL_FRAME)    /* conversions a frame at most (with the cell) */
#define JL_GAIN_MIN_DB (-12)
#define JL_GAIN_MAX_DB 12
#define JL_FIT_BLOCK 208u              /* frames a block of the t0 fit: ~1 s */

/* the ADC's conversion rate for a pattern of n channels (1 or 2) so that each channel is sampled every 24 us; and the trigger
 * interval ESP-IDF 5.5's adc_hal.c derives from it (clk_src_hz x B / ((NUM + 1) x B + A) / 2 / f, the S3: NUM 15, B 1, A 0),
 * which must come out exact: 30 for 2 channels, 60 for 1 */
uint32_t jl_conv_hz(unsigned channels);
uint32_t jl_interval(uint32_t clk_src_hz, uint32_t conv_hz);

struct jl_parse {
    uint16_t line_n;                   /* line samples found (before padding) */
    uint16_t cell_n;
    uint32_t cell_sum;
    uint16_t other;                    /* conversions of another channel or unit, or invalid */
};
/* conv: n conversion words; line_ch / cell_ch: ADC1 channels (cell_ch < 0: none). out gets exactly JL_FRAME samples. */
void jl_parse(const uint32_t *conv, size_t n, unsigned line_ch, int cell_ch, uint16_t *out, struct jl_parse *res);

struct jl_fit {
    int64_t cur_min, prev_min;         /* stamp - 24 n: the lowest in this block and in the last one */
    int64_t lat_sum;                   /* latency against the fit, this block */
    uint32_t lat_max_cur;
    uint32_t cur_n, blocks;            /* frames in this block; blocks finished */
    uint8_t have;                      /* a stamp seen */
    /* results */
    uint32_t lat_avg_us, lat_max_us;   /* the last full block's interrupt latency against the fit */
    uint32_t lat_max_ever_us;
    int32_t drift_ppb;                 /* t0's move from one block's end to the next, ppb of the time between (0: no drift) */
    uint32_t steps;                    /* t0 moved by more than half a frame between blocks (frames lost, or a restart) */
};
void jl_fit_init(struct jl_fit *f);
/* frame stamp: the timer at the interrupt for the frame whose LAST sample has index n_end - 1 */
void jl_fit_add(struct jl_fit *f, int64_t n_end, int64_t stamp_us);
int jl_fit_ready(const struct jl_fit *f);              /* 1 once a stamp was added */
int64_t jl_fit_t0(const struct jl_fit *f);             /* local us of sample 0 */
int64_t jl_fit_time(const struct jl_fit *f, int64_t n); /* local us of sample n */

struct jl_cond {
    int32_t dc_q8;                     /* the DC estimate in 1/256 of an int16 step (x 16 scale) */
    int32_t gain_q12;
    int8_t gain_db;
    uint8_t have_dc;
    /* the level window (jl_cond_window resets it) */
    int32_t peak;
    uint64_t sq_sum;
    uint32_t n, clips;
    uint32_t clips_total;
};
void jl_cond_init(struct jl_cond *c);
void jl_cond_gain(struct jl_cond *c, int db);          /* clamped to JL_GAIN_MIN_DB..MAX */
void jl_cond_run(struct jl_cond *c, const uint16_t *raw, size_t n, int16_t *out);
int32_t jl_cond_dc(const struct jl_cond *c);           /* the DC estimate in raw ADC codes (0..4095), for `line` */
struct jl_level {
    int16_t peak_db, rms_db;           /* dBFS, -99 .. 0 (silence -99) */
    uint32_t clips;
};
void jl_cond_window(struct jl_cond *c, struct jl_level *out);   /* the window's level, then a new window */

struct jl_cell {
    uint64_t sum;
    uint32_t n;
    uint32_t avg_x16;                  /* the last full second's average code x 16; 0: none yet */
    uint32_t blocks;
};
void jl_cell_init(struct jl_cell *c);
/* one frame's cell sum and count; returns 1 when a second's block was finished (avg_x16 is new) */
int jl_cell_add(struct jl_cell *c, uint32_t sum, uint32_t n, uint32_t block_n);
/* the cell's percent from its millivolts: a 1S Li-ion resting curve from empty_mv (0 %) to 4200 mV (100 %), -1 below
 * JL_NO_CELL_MV (no cell: the usb order holds the pin at 0 V) */
#define JL_NO_CELL_MV 2000
int jl_batt_pct(int32_t mv, int32_t empty_mv);
/* the same from the millivolts at the ADC pin and the board's divider (the cell's volts / the pin's): -1 = no cell (the pin
 * under JL_NO_CELL_MV / div: a floating or grounded pin never shows as a low cell) */
int jl_cell_pin_pct(int32_t pin_mv, int32_t div, int32_t empty_mv);
#endif
