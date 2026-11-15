/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_blemidi.h"

#define BAD ((size_t)-1)

static int is_sysex(const uint8_t *msg, size_t n)
{
    if (n < 2u || msg[0] != 0xF0u || msg[n - 1u] != 0xF7u)
        return 0;
    for (size_t i = 1; i + 1u < n; ++i)
        if (msg[i] >= 0x80u)
            return 0;
    return 1;
}

/* one packet's worth from pos: the same walk for counting (out NULL) and for writing */
static size_t walk(const uint8_t *msg, size_t n, size_t *at, uint16_t ms, uint8_t *out, size_t cap)
{
    const uint8_t ts = (uint8_t)(0x80u | (ms & 0x7Fu));
    size_t k = 0, pos = *at;
    if (out)
        out[k] = (uint8_t)(0x80u | ((ms >> 7) & 0x3Fu));
    ++k;
    if (pos == 0) {
        if (out) {
            out[k] = ts;
            out[k + 1u] = 0xF0;
        }
        k += 2u;
        pos = 1;
    }
    while (pos < n) {
        if (pos == n - 1u) {           /* the F7, after its own timestamp byte, both in one packet */
            if (k + 2u > cap)
                break;
            if (out) {
                out[k] = ts;
                out[k + 1u] = 0xF7;
            }
            k += 2u;
        } else {
            if (k + 1u > cap)
                break;
            if (out)
                out[k] = msg[pos];
            ++k;
        }
        ++pos;
    }
    *at = pos;
    return k;
}

size_t jbm_sysex_packet(const uint8_t *msg, size_t n, size_t *at, uint16_t ms, uint8_t *out, size_t cap)
{
    if (cap < JBM_PAYLOAD_MIN || *at >= n || !is_sysex(msg, n))
        return 0;
    return walk(msg, n, at, ms, out, cap);
}

size_t jbm_sysex_packet_count(size_t n, size_t cap)
{
    size_t at = 0, count = 0;
    if (cap < JBM_PAYLOAD_MIN || n < 2u)
        return 0;
    while (at < n) {
        walk(NULL, n, &at, 0, NULL, cap);
        ++count;
    }
    return count;
}

void jbm_init(struct jbm_in *in)
{
    in->in_sysex = 0;
    in->running = 0;
}

/* the data bytes of a status (MIDI 1.0's lengths); SysEx and real time have none here */
static unsigned data_len(uint8_t status)
{
    if (status < 0xF0u)
        return (status & 0xE0u) == 0xC0u ? 1u : 2u;   /* program change and channel pressure: 1 */
    if (status == 0xF1u || status == 0xF3u)
        return 1u;
    return status == 0xF2u ? 2u : 0u;
}

static size_t data_bytes(const uint8_t *p, size_t n, size_t i, unsigned count, jbm_emit_fn emit, void *ctx)
{
    for (unsigned j = 0; j < count; ++j, ++i) {
        if (i >= n || p[i] >= 0x80u)
            return BAD;
        emit(ctx, p[i]);
    }
    return i;
}

static size_t sysex_data(const uint8_t *p, size_t n, size_t i, jbm_emit_fn emit, void *ctx)
{
    while (i < n && p[i] < 0x80u)
        emit(ctx, p[i++]);
    return i;
}

/* a status byte at p[i], after its timestamp byte */
static size_t status(struct jbm_in *in, const uint8_t *p, size_t n, size_t i, jbm_emit_fn emit, void *ctx)
{
    const uint8_t s = p[i++];
    if (s >= 0xF8u) {                  /* real time: also inside a SysEx, which then goes on */
        emit(ctx, s);
        return in->in_sysex ? sysex_data(p, n, i, emit, ctx) : i;
    }
    if (in->in_sysex) {
        in->in_sysex = 0;
        if (s == 0xF7u) {
            emit(ctx, s);
            return i;
        }
    }
    if (s == 0xF7u)                    /* an F7 with no SysEx running: nothing to end */
        return i;
    emit(ctx, s);
    if (s == 0xF0u) {
        in->running = 0;
        in->in_sysex = 1;
        return sysex_data(p, n, i, emit, ctx);
    }
    in->running = s < 0xF0u ? s : 0;   /* system common clears the running status */
    return data_bytes(p, n, i, data_len(s), emit, ctx);
}

static size_t running(struct jbm_in *in, const uint8_t *p, size_t n, size_t i, jbm_emit_fn emit, void *ctx)
{
    if (!in->running)
        return BAD;
    emit(ctx, in->running);
    return data_bytes(p, n, i, data_len(in->running), emit, ctx);
}

int jbm_decode(struct jbm_in *in, const uint8_t *pkt, size_t n, jbm_emit_fn emit, void *ctx)
{
    size_t i = 1;
    if (n < 1u || (pkt[0] & 0xC0u) != 0x80u) {
        in->in_sysex = 0;
        return -1;
    }
    if (in->in_sysex)                  /* a SysEx goes on: its data follows the header directly */
        i = sysex_data(pkt, n, i, emit, ctx);
    while (i < n && i != BAD) {
        if (pkt[i] < 0x80u) {          /* running status with no timestamp byte */
            i = running(in, pkt, n, i, emit, ctx);
            continue;
        }
        if (++i >= n) {                /* a timestamp byte with nothing after it */
            i = BAD;
            break;
        }
        i = pkt[i] < 0x80u ? running(in, pkt, n, i, emit, ctx) : status(in, pkt, n, i, emit, ctx);
    }
    if (i == BAD) {
        in->in_sysex = 0;
        in->running = 0;
        return -1;
    }
    return 0;
}
