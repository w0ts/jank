/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_sysex.h"
#include <stdio.h>

static const uint8_t HEAD[] = {0xF0, 0x7D, 0x4A, 0x4B};
#define HEAD_LEN (sizeof HEAD)

size_t jx_frame(const char *text, size_t n, uint8_t *out)
{
    size_t at = 0;
    for (size_t i = 0; i < HEAD_LEN; ++i)
        out[at++] = HEAD[i];
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = (uint8_t)text[i];
        out[at++] = c < 0x80u ? c : (uint8_t)'?';
    }
    out[at++] = 0xF7;
    return at;
}

size_t jx_push_frame(uint8_t type, const char *text, size_t n, uint8_t *out)
{
    size_t at = 0;
    if (type >= 0x80u || n > JX_PUSH_TEXT_MAX)
        return 0;
    out[at++] = 0xF0;
    out[at++] = 0x7D;
    out[at++] = 0x4A;                  /* 'J' */
    out[at++] = 0x42;                  /* 'B' */
    out[at++] = type;
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = (uint8_t)text[i];
        out[at++] = c < 0x80u ? c : (uint8_t)'?';
    }
    out[at++] = 0xF7;
    return at;
}

static const char *const BEAT_MODES[] = {"stopped", "steady", "faster", "slower"};

static long clamp_field(int32_t v) { return v > JX_FIELD_MAX ? JX_FIELD_MAX : v < -JX_FIELD_MAX ? -JX_FIELD_MAX : (long)v; }

size_t jx_beat_line(char *out, size_t cap, char kind, uint32_t n, int32_t bpm_x100, unsigned mode, unsigned audio, int32_t extra,
                    int32_t extra2)
{
    int r = snprintf(out, cap, "%c %lu %ld %s %u", kind, (unsigned long)n, (long)(bpm_x100 > 0 ? bpm_x100 : 0),
                     BEAT_MODES[mode & 3u], audio > 2u ? 2u : audio);
    size_t at = r < 0 ? cap : (size_t)r;
    if (at < cap && extra != JX_NO_EXTRA) {
        r = snprintf(out + at, cap - at, " %ld", clamp_field(extra));
        at = r < 0 ? cap : at + (size_t)r;
    }
    if (at < cap && extra != JX_NO_EXTRA) {
        r = extra2 == JX_NO_EXTRA ? snprintf(out + at, cap - at, " -") : snprintf(out + at, cap - at, " %ld", clamp_field(extra2));
        at = r < 0 ? cap : at + (size_t)r;
    }
    return at >= cap ? 0u : at;
}

size_t jx_realtime_packets(const uint8_t *bytes, size_t n, uint8_t *out, size_t cap)
{
    size_t at = 0;
    for (size_t i = 0; i < n && at + 4u <= cap; ++i, at += 4u) {
        out[at] = 0x0F;
        out[at + 1u] = bytes[i];
        out[at + 2u] = 0;
        out[at + 3u] = 0;
    }
    return at;
}

void jx_init(struct jx_in *in) { in->state = 0; }

int jx_feed(struct jx_in *in, uint8_t byte)
{
    if (byte >= 0xF8u)                 /* real time: allowed anywhere, the clock included */
        return -1;
    if (byte == 0xF0u) {
        in->state = 1;
        return -1;
    }
    if (byte >= 0x80u) {               /* F7, or any other status: the frame is over */
        in->state = 0;
        return -1;
    }
    if (in->state == HEAD_LEN)
        return byte;
    if (in->state == 0)
        return -1;
    in->state = byte == HEAD[in->state] ? (uint8_t)(in->state + 1u) : 0;
    return -1;
}
