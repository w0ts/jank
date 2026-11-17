/* SPDX-License-Identifier: GPL-3.0-only */
/* Sound on the tracker's source, and the input meter (jank_live.h). */
#include "jank_live.h"
#include <math.h>
#include <string.h>

#define FULL_SCALE_SQ (32768.0 * 32768.0)

void jlv_init(struct jlv *l)
{
    memset(l, 0, sizeof *l);
    l->rms_db = l->min_db = l->max_db = JLV_FLOOR_DB;
}

int16_t jlv_db(double mean_square)
{
    double db;
    if (!(mean_square > 0.0))
        return JLV_FLOOR_DB;
    db = 10.0 * log10(mean_square / FULL_SCALE_SQ);
    if (db <= (double)JLV_FLOOR_DB)
        return JLV_FLOOR_DB;
    return db >= 0.0 ? 0 : (int16_t)lround(db);
}

/* the mean square of the newest n blocks (fewer at the start) */
static double window_ms(const struct jlv *l, unsigned n)
{
    uint64_t sum = 0;
    if (n > l->nblocks)
        n = l->nblocks;
    if (!n)
        return 0.0;
    for (unsigned i = 1; i <= n; i++)
        sum += l->block[(l->head + JLV_SOUND_BLOCKS - i) % JLV_SOUND_BLOCKS];
    return (double)sum / ((double)n * JLV_BLOCK);
}

static double level_ms(int db) { return FULL_SCALE_SQ * pow(10.0, db / 10.0); }

static void detect(struct jlv *l)
{
    double ms = window_ms(l, JLV_SOUND_BLOCKS);
    int loud = ms > level_ms(JLV_ON_DB), quiet = ms < level_ms(JLV_OFF_DB);
    if (!l->live) {
        l->loud_ms = loud ? l->loud_ms + JLV_BLOCK_MS : 0u;
        if (l->loud_ms >= JLV_ON_MS) {
            l->live = 1;
            l->lives++;
            l->quiet_ms = 0;
        }
    } else {
        l->quiet_ms = quiet ? l->quiet_ms + JLV_BLOCK_MS : 0u;
        if (l->quiet_ms >= JLV_OFF_MS) {
            l->live = 0;
            l->loud_ms = 0;
        }
    }
}

static void meter(struct jlv *l)
{
    int16_t lo, hi;
    l->rms_db = jlv_db(window_ms(l, JLV_METER_BLOCKS));
    l->span[l->span_head] = l->rms_db;
    l->span_head = (uint8_t)((l->span_head + 1u) % JLV_SPAN);
    if (l->nspan < JLV_SPAN)
        l->nspan++;
    lo = hi = l->rms_db;
    for (unsigned i = 0; i < l->nspan; i++) {
        int16_t v = l->span[(l->span_head + JLV_SPAN - 1u - i) % JLV_SPAN];
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    l->min_db = lo;
    l->max_db = hi;
}

/* a whole block: into the windows, then the detector, and the meter every JLV_METER_EVERY blocks */
static void block_done(struct jlv *l)
{
    l->block[l->head] = l->acc;
    l->head = (uint8_t)((l->head + 1u) % JLV_SOUND_BLOCKS);
    if (l->nblocks < JLV_SOUND_BLOCKS)
        l->nblocks++;
    l->acc = 0;
    l->acc_n = 0;
    l->blocks++;
    detect(l);
    if (l->blocks % JLV_METER_EVERY == 0u)
        meter(l);
}

void jlv_push(struct jlv *l, const int16_t *x, size_t n)
{
    while (n) {
        size_t take = JLV_BLOCK - l->acc_n;
        if (take > n)
            take = n;
        if (x) {
            for (size_t i = 0; i < take; i++)
                l->acc += (uint64_t)((int32_t)x[i] * (int32_t)x[i]);
            x += take;
        }
        l->acc_n += (uint32_t)take;
        n -= take;
        if (l->acc_n == JLV_BLOCK)
            block_done(l);
    }
}
