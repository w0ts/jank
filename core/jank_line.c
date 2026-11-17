/* SPDX-License-Identifier: GPL-3.0-only */
/* The LINE IN path's portable parts (jank_line.h). */
#include "jank_line.h"
#include <math.h>
#include <string.h>

/* ESP-IDF 5.5 hal/esp32s3/include/hal/adc_ll.h: ADC_LL_CLKM_DIV_NUM_DEFAULT 15, _B_ 1, _A_ 0 */
#define CLKM_NUM 15u
#define CLKM_B 1u
#define CLKM_A 0u

uint32_t jl_conv_hz(unsigned channels)
{
    /* 83,333 = SOC_ADC_SAMPLE_FREQ_THRES_HIGH; one channel at half of it: 41,666 (41,667 would round the interval to 59) */
    return channels >= 2u ? 83333u : 41666u;
}

uint32_t jl_interval(uint32_t clk_src_hz, uint32_t conv_hz)
{
    uint64_t denom = ((uint64_t)(CLKM_NUM + 1u) * CLKM_B + CLKM_A) * 2u * conv_hz;
    return conv_hz ? (uint32_t)((uint64_t)clk_src_hz * CLKM_B / denom) : 0u;
}

/* the S3's TYPE2 word (hal/adc_types.h, adc_digi_output_data_t): data 0-11, channel 13-16, unit 17 */
void jl_parse(const uint32_t *conv, size_t n, unsigned line_ch, int cell_ch, uint16_t *out, struct jl_parse *res)
{
    uint16_t last = 0;
    uint32_t got = 0, cell_n = 0, cell_sum = 0, other = 0;
    for (size_t i = 0; i < n; i++) {
        uint32_t w = conv[i];
        uint32_t data = w & 0xFFFu, ch = (w >> 13) & 0xFu, unit = (w >> 17) & 1u;
        if (unit) {
            other++;
        } else if (ch == line_ch) {
            if (got < JL_FRAME) {
                last = (uint16_t)data;
                out[got++] = last;
            } else {
                other++;               /* more than a frame's worth: the extra is dropped, the frame keeps its length */
            }
        } else if (cell_ch >= 0 && ch == (uint32_t)cell_ch) {
            cell_sum += data;
            cell_n++;
        } else {
            other++;
        }
    }
    res->line_n = (uint16_t)got;
    while (got < JL_FRAME)
        out[got++] = last;             /* a short frame: held, so that every frame is 4.8 ms and the times stay right */
    res->cell_n = (uint16_t)cell_n;
    res->cell_sum = cell_sum;
    res->other = (uint16_t)other;
}

void jl_fit_init(struct jl_fit *f) { memset(f, 0, sizeof *f); }

int jl_fit_ready(const struct jl_fit *f) { return f->have; }

int64_t jl_fit_t0(const struct jl_fit *f) { return f->cur_min < f->prev_min ? f->cur_min : f->prev_min; }

int64_t jl_fit_time(const struct jl_fit *f, int64_t n) { return jl_fit_t0(f) + n * JL_SAMPLE_US; }

static void block_end(struct jl_fit *f)
{
    int64_t step = f->cur_min - f->prev_min;
    f->lat_avg_us = (uint32_t)(f->lat_sum / (int64_t)f->cur_n);
    f->lat_max_us = f->lat_max_cur;
    if (f->blocks) {                   /* (the first block's "previous" is its own first stamp) */
        if (step > (int64_t)JL_FRAME_US / 2 || step < -(int64_t)JL_FRAME_US / 2)
            f->steps++;
        else
            f->drift_ppb = (int32_t)(step * 1000000000 / ((int64_t)JL_FIT_BLOCK * JL_FRAME_US));
    }
    f->blocks++;
    f->prev_min = f->cur_min;
    f->cur_min = INT64_MAX;
    f->cur_n = 0;
    f->lat_sum = 0;
    f->lat_max_cur = 0;
}

void jl_fit_add(struct jl_fit *f, int64_t n_end, int64_t stamp_us)
{
    int64_t r = stamp_us - (n_end - 1) * JL_SAMPLE_US, lat;
    if (!f->have) {
        f->cur_min = f->prev_min = r;
        f->have = 1;
    }
    if (r < f->cur_min)
        f->cur_min = r;
    lat = r - jl_fit_t0(f);
    f->lat_sum += lat;
    if (lat > (int64_t)f->lat_max_cur)
        f->lat_max_cur = lat > (int64_t)UINT32_MAX ? UINT32_MAX : (uint32_t)lat;
    if (f->lat_max_cur > f->lat_max_ever_us)
        f->lat_max_ever_us = f->lat_max_cur;
    if (++f->cur_n == JL_FIT_BLOCK)
        block_end(f);
}

#define DC_SHIFT 10                    /* one pole at 41,667 / (2 pi 1024) = 6.5 Hz */

void jl_cond_init(struct jl_cond *c)
{
    memset(c, 0, sizeof *c);
    c->gain_q12 = 4096;
}

void jl_cond_gain(struct jl_cond *c, int db)
{
    if (db < JL_GAIN_MIN_DB)
        db = JL_GAIN_MIN_DB;
    if (db > JL_GAIN_MAX_DB)
        db = JL_GAIN_MAX_DB;
    c->gain_db = (int8_t)db;
    c->gain_q12 = (int32_t)lround(4096.0 * pow(10.0, db / 20.0));
}

void jl_cond_run(struct jl_cond *c, const uint16_t *raw, size_t n, int16_t *out)
{
    for (size_t i = 0; i < n; i++) {
        int32_t x = (int32_t)(raw[i] & 0xFFFu) * 16, y, a;
        if (!c->have_dc) {
            c->dc_q8 = x * 256;        /* the first sample is the DC: no long rise from 0 */
            c->have_dc = 1;
        }
        c->dc_q8 += (x * 256 - c->dc_q8) / (1 << DC_SHIFT);
        y = x - c->dc_q8 / 256;
        y = (int32_t)(((int64_t)y * c->gain_q12 + 2048) >> 12);
        if (y > 32767 || y < -32768) {
            y = y > 0 ? 32767 : -32768;
            c->clips++;
            c->clips_total++;
        }
        out[i] = (int16_t)y;
        a = y < 0 ? -y : y;
        if (a > c->peak)
            c->peak = a;
        c->sq_sum += (uint64_t)((int64_t)y * y);
    }
    c->n += (uint32_t)n;
}

int32_t jl_cond_dc(const struct jl_cond *c) { return c->dc_q8 / 256 / 16; }

static int16_t db_of(double ratio)
{
    double db = ratio > 0.0 ? 20.0 * log10(ratio) : -99.0;
    return (int16_t)(db < -99.0 ? -99 : db > 0.0 ? 0 : floor(db + 0.5));
}

void jl_cond_window(struct jl_cond *c, struct jl_level *out)
{
    out->peak_db = db_of((double)c->peak / 32768.0);
    out->rms_db = c->n ? db_of(sqrt((double)c->sq_sum / c->n) / 32768.0) : -99;
    out->clips = c->clips;
    c->peak = 0;
    c->sq_sum = 0;
    c->n = 0;
    c->clips = 0;
}

void jl_cell_init(struct jl_cell *c) { memset(c, 0, sizeof *c); }

int jl_cell_add(struct jl_cell *c, uint32_t sum, uint32_t n, uint32_t block_n)
{
    c->sum += sum;
    c->n += n;
    if (!c->n || c->n < block_n)
        return 0;
    c->avg_x16 = (uint32_t)((c->sum * 16u + c->n / 2u) / c->n);
    c->sum = 0;
    c->n = 0;
    c->blocks++;
    return 1;
}

/* a 1S Li-ion cell resting (or at a light load) from its empty point to full [est: a generic curve, not this cell's] */
static const int16_t CURVE_MV[] = {3300, 3400, 3500, 3600, 3650, 3700, 3750, 3800, 3900, 4000, 4100, 4200};
static const int8_t CURVE_PCT[] = {0, 3, 7, 14, 22, 33, 45, 55, 68, 80, 90, 100};

int jl_batt_pct(int32_t mv, int32_t empty_mv)
{
    const size_t n = sizeof CURVE_MV / sizeof CURVE_MV[0];
    int32_t v;
    if (mv < JL_NO_CELL_MV)
        return -1;
    if (empty_mv < 3000 || empty_mv >= 4000)
        empty_mv = CURVE_MV[0];
    /* the curve's 3300 .. 4200 stretched onto empty_mv .. 4200 */
    v = CURVE_MV[0] + (mv - empty_mv) * (CURVE_MV[n - 1u] - CURVE_MV[0]) / (CURVE_MV[n - 1u] - empty_mv);
    if (v <= CURVE_MV[0])
        return 0;
    if (v >= CURVE_MV[n - 1u])
        return 100;
    for (size_t i = 1; i < n; i++)
        if (v < CURVE_MV[i])
            return CURVE_PCT[i - 1u] + (v - CURVE_MV[i - 1u]) * (CURVE_PCT[i] - CURVE_PCT[i - 1u]) / (CURVE_MV[i] - CURVE_MV[i - 1u]);
    return 100;
}
