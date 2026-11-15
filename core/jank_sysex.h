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

/* Reports the stick sends on its own (never answers to a line), with their own header F0 7D 'J' 'B' <type>, so that the
 * control line's readers (jx_feed, scripts/ota.py, page/jank.html: all match F0 7D 'J' 'K') never see them. */
/* the beat tracker: "b <n> <bpm x100> <mode> <audio> [<lag us> <link us>|-]" on each beat, "t <n> <bpm x100> <mode>
 * <audio> [<beat_latency ms> -]" a second. lag: how long after the instant the stick takes the beat to be HEARD (its
 * sample's arrival + beat_latency) the report was made; a reader that time-stamps the report on arrival subtracts it.
 * link: where the Link session's nearest beat is from that instant (+ = later; what Live follows), "-" with no session
 * playing. The numbers are clamped to +-JX_FIELD_MAX. */
#define JX_PUSH_BEAT 0x10u
/* the HOME line for a phone over BLE (jank_phone.h, jph_home_line): "h <bpm x100> <net> ...", a few times a second */
#define JX_PUSH_HOME 0x11u
#define JX_FIELD_MAX 9999999L
#define JX_PUSH_TEXT_MAX 48u
#define JX_PUSH_FRAME_MAX (JX_PUSH_TEXT_MAX + 6u)
/* one report of n <= JX_PUSH_TEXT_MAX characters into out -> its length (0: a bad type or too long) */
size_t jx_push_frame(uint8_t type, const char *text, size_t n, uint8_t *out);
#define JX_NO_EXTRA INT32_MIN           /* no such field */
/* the beat report's text: kind 'b' (a tracked beat) or 't' (the once-a-second state); mode 0..3 as jb_mode_kind; extra:
 * the lag (b) or the latency (t), JX_NO_EXTRA for none (and then no extra2 either); extra2: the link offset (b),
 * JX_NO_EXTRA prints "-" -> length (0: does not fit) */
size_t jx_beat_line(char *out, size_t cap, char kind, uint32_t n, int32_t bpm_x100, unsigned mode, unsigned audio, int32_t extra,
                    int32_t extra2);

/* single-byte real time messages, the clock's, as USB-MIDI packets (cable 0, code F) -> bytes written, whole packets */
size_t jx_realtime_packets(const uint8_t *bytes, size_t n, uint8_t *out, size_t cap);

#endif
