/* SPDX-License-Identifier: GPL-3.0-only */
/* The control line in SysEx, on the S3's USB MIDI port: F0 7D 'J' 'K', 7-bit text, F7. 0x7D is MIDI's non-commercial ID.
 * Lines are not framed: text runs across frames and "\n" ends a line, as on a serial port. page/jank.html holds the same
 * frame. 30 characters make a frame that fits TinyUSB's 64 byte MIDI buffer (4 bytes for every 3 on USB). */
#ifndef JANK_SYSEX_H
#define JANK_SYSEX_H
#include <stddef.h>
#include <stdint.h>

#define JX_TEXT_MAX 30u
#define JX_FRAME_MAX (JX_TEXT_MAX + 5u)

struct jx_in {
    uint8_t state;
};

/* one frame of n <= JX_TEXT_MAX characters into out -> its length; a character with the high bit set goes as '?' */
size_t jx_frame(const char *text, size_t n, uint8_t *out);
void jx_init(struct jx_in *in);
/* one byte of the MIDI stream -> a character of text, or -1 */
int jx_feed(struct jx_in *in, uint8_t byte);

/* single-byte real time messages, the clock's, as USB-MIDI packets (cable 0, code F) -> bytes written, whole packets */
size_t jx_realtime_packets(const uint8_t *bytes, size_t n, uint8_t *out, size_t cap);

#endif
