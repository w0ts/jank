/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_sysex.h"

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
