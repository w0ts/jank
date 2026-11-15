/* SPDX-License-Identifier: GPL-3.0-only */
/* BLE-MIDI packet framing ("MIDI over Bluetooth Low Energy" 1.0, MIDI Association / Apple), for the control line's SysEx
 * (jank_sysex.h) on the BLE MIDI characteristic. A packet is one ATT value of at most ATT_MTU - 3 bytes:
 *   header:     1 0 t12..t7          (bit 7 set, bit 6 clear: the upper 6 bits of a 13-bit millisecond timestamp)
 *   timestamp:  1 t6..t0             (before each message's status byte; also before a SysEx's F7)
 * A SysEx longer than a packet goes on in the next packets as header + plain data bytes; its F7 comes after a timestamp
 * byte. Real time bytes may sit inside a SysEx, each after its own timestamp byte. Running status: a message's data bytes
 * may follow the previous message with no status, with or without a timestamp byte before them. */
#ifndef JANK_BLEMIDI_H
#define JANK_BLEMIDI_H
#include <stddef.h>
#include <stdint.h>

#define JBM_PAYLOAD_MIN 3u             /* header, timestamp, F0: the least a packet that starts a SysEx needs */

/* The next packet of a whole SysEx message msg (F0 .. F7, 7-bit data between) for a payload of at most cap bytes
 * (ATT_MTU - 3), with the 13-bit time ms. *at is the cursor into msg: 0 to start, advanced by each call.
 * -> the packet's length; 0 when the message is all sent, or msg is no SysEx, or cap < JBM_PAYLOAD_MIN. */
size_t jbm_sysex_packet(const uint8_t *msg, size_t n, size_t *at, uint16_t ms, uint8_t *out, size_t cap);

/* the packets a message of n bytes takes at payload cap (for buffers and tests) */
size_t jbm_sysex_packet_count(size_t n, size_t cap);

/* Several whole SysEx messages in one packet (a control line's 30-character frames, the HOME line): each goes in as
 * timestamp, F0 .. data .., timestamp, F7, n + 2 bytes, after the packet's one header. Appends msg to pkt, which holds *len
 * bytes (0: empty, the header goes in first). -> 1 appended; 0 it does not fit in cap or is no SysEx (pkt unchanged: send
 * what it holds and start the next; a message that does not fit an empty packet goes by jbm_sysex_packet). */
int jbm_pack(const uint8_t *msg, size_t n, uint16_t ms, uint8_t *pkt, size_t *len, size_t cap);

struct jbm_in {
    uint8_t in_sysex;                  /* a SysEx runs on into the next packet */
    uint8_t running;                   /* the running status, 0 none */
};

typedef void (*jbm_emit_fn)(void *ctx, uint8_t byte);

void jbm_init(struct jbm_in *in);
/* one packet -> the plain MIDI byte stream through emit (running status written out in full, real time bytes where they
 * came). -> 0, or -1 for a malformed packet: what came before the fault was emitted, the rest is dropped, and a SysEx
 * that was running is ended with no F7 (jx_feed then drops its unfinished line at the next F0). */
int jbm_decode(struct jbm_in *in, const uint8_t *pkt, size_t n, jbm_emit_fn emit, void *ctx);

#endif
