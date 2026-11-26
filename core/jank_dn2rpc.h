/* SPDX-License-Identifier: GPL-3.0-only */
/* The dn2-wishes test channel (docs/remote-ui-protocol.md in dn2-wishes), the part Jank needs to READ the Digitone's state with
 * PEEK (command 35): framing, pack7, and the USB-MIDI packets a SysEx travels in. Pure, host-tested (tests/test_dn2rpc.c) against
 * bytes made by the channel's own Python (tools/dn2tools protocol.py).
 *
 *   request  F0 00 20 3C 7D 01 <pack7( u16 seq, u8 cmd, args... )> F7
 *   reply    F0 00 20 3C 7D 01 <pack7( u16 seq, u8 cmd | 0x80, u8 status, data... )> F7
 *   PEEK     args u32 address, u8 length (1..128); data u32 address, then the bytes. Read only: whitelisted ranges.
 *   status   0 ok; 6 outside the whitelist; 7 REMOTE CONTROL is OFF on the unit.
 * USB only, big-endian. The unit answers each request once and never sends unasked. */
#ifndef JANK_DN2RPC_H
#define JANK_DN2RPC_H
#include <stddef.h>
#include <stdint.h>

#define JD2_CMD_PEEK 35u
#define JD2_PEEK_MAX 128u
#define JD2_STATUS_OK 0u
#define JD2_STATUS_REMOTE_OFF 7u
#define JD2_SX_MAX 256u                /* the longest frame: a 128-byte PEEK reply is 163 bytes */

/* 8-bit -> 7-bit: a header byte with the top bits of up to 7 bytes (bit 6 = the first), then the bytes with the top bit cleared.
 * out holds n + (n + 6) / 7 bytes; returns that count */
size_t jd2_pack7(const uint8_t *in, size_t n, uint8_t *out);
/* the reverse (a partial last group is allowed); out holds at most n bytes; returns the count */
size_t jd2_unpack7(const uint8_t *in, size_t n, uint8_t *out);

/* any request, F0 .. F7: `cmd` with nargs bytes of arguments (at most JD2_ARGS_MAX), into out (cap bytes); returns its length, 0 if it does not fit */
#define JD2_ARGS_MAX 228u              /* the longest: a JANK PAGE (live control 62): flags, sel, cursor, n and 7 lines of 1 + 31 (a CHAIN SET is 66, a MESSAGE 21) */
#define JD2_REQUEST_MAX(nargs) (6u + (3u + (nargs)) + (3u + (nargs) + 6u) / 7u + 1u)   /* the frame of a request with nargs arguments, F0 .. F7 */
size_t jd2_request(uint16_t seq, uint8_t cmd, const uint8_t *args, size_t nargs, uint8_t *out, size_t cap);

/* a PEEK request, F0 .. F7, into out (cap bytes); returns its length, 0 if len is not 1..JD2_PEEK_MAX or it does not fit */
size_t jd2_peek_request(uint16_t seq, uint32_t address, uint8_t len, uint8_t *out, size_t cap);

struct jd2_reply {
    uint16_t seq;
    uint8_t cmd;                       /* the request's, without the 0x80 */
    uint8_t status;
    const uint8_t *data;               /* points into the scratch buffer given to jd2_parse_reply */
    size_t n;
};
/* a reply, F0 .. F7 (n bytes) -> *r; scratch (cap bytes, at least n) holds the unpacked body. 1: a reply of the channel; 0: not */
int jd2_parse_reply(const uint8_t *sx, size_t n, uint8_t *scratch, size_t cap, struct jd2_reply *r);
/* a PEEK reply's data -> the address it starts at and the bytes after it; 0 if shorter than the address */
int jd2_peek_data(const struct jd2_reply *r, uint32_t *address, const uint8_t **bytes, size_t *n);

/* a SysEx (F0 .. F7) as USB-MIDI event packets on a cable: 4 bytes each, code 4 for three bytes with more to come, 5 / 6 / 7 for
 * the last 1 / 2 / 3. Returns the bytes written (a multiple of 4), 0 if it does not fit in cap */
size_t jd2_sysex_to_packets(uint8_t cable, const uint8_t *sx, size_t n, uint8_t *out, size_t cap);
/* the same one packet at a time: packet k (0 = the first) into out; 1, or 0 past the end. A long SysEx goes out without a buffer for all its packets */
#define JD2_SYSEX_PACKETS(n) (((n) + 2u) / 3u)
size_t jd2_sysex_packet(uint8_t cable, const uint8_t *sx, size_t n, size_t k, uint8_t out[4]);

/* the packets of a SysEx put together again */
struct jd2_sx {
    uint8_t buf[JD2_SX_MAX];
    size_t n;
    uint8_t active;                    /* inside a SysEx */
    uint8_t done;                      /* buf[0..n) is a whole SysEx: the next feed starts afresh */
};
void jd2_sx_init(struct jd2_sx *s);
/* one 4-byte USB-MIDI packet; 1 when it completes a SysEx (buf, n), else 0. Packets that are not part of a SysEx are ignored; a
 * SysEx longer than JD2_SX_MAX is dropped whole */
int jd2_sx_feed(struct jd2_sx *s, const uint8_t pkt[4]);
#endif
