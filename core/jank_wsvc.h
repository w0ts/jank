/* SPDX-License-Identifier: GPL-3.0-only */
/* Talking to a jank over Wi-Fi: a read-only diagnostics service in UDP, in jank's own Ethernet frames (there is no IP stack on
 * the stick: core/jank_net.h). The unit answers ARP for its DHCP address and takes UDP datagrams on JW_PORT; the datagram's text
 * is one control line (`get`, `status`, `diag`, ...), only the read-only ones (jw_allowed). Its answer goes back to the sender in
 * datagrams of JW_CHUNK bytes of text at most, each led by one byte: the chunk's index (0..127) and, in bit 7, "the last". A client
 * puts the text together by index (scripts/jankwifi.py). Nothing here can change a setting, update the firmware or restart the unit,
 * and a datagram that is not a clean request is not answered (nothing to amplify).
 *
 * Unauthenticated, for a network one trusts: read-only is the whole defence. Pure (no clock, no radio), host-tested
 * (tests/test_wsvc.c) with frames built by hand. */
#ifndef JANK_WSVC_H
#define JANK_WSVC_H
#include <stddef.h>
#include <stdint.h>

#define JW_PORT 7447u
#define JW_LINE_MAX 120u               /* the longest request line */
#define JW_CHUNK 200u                  /* text bytes in one reply datagram, at most */
#define JW_CHUNKS_MAX 127u             /* the chunk index has 7 bits */
#define JW_FRAME_MAX (14u + 20u + 8u + 1u + JW_CHUNK)   /* the biggest frame this sends: Ethernet, IPv4, UDP, index byte, text */

enum jw_in {
    JW_IGNORE,                         /* not for this service: the frame goes on as before */
    JW_ARP_REPLIED,                    /* an ARP request for our address: the reply is in out; the frame is consumed */
    JW_REQUEST                         /* a UDP request: in *req; the frame is consumed */
};

struct jw_req {
    uint8_t src_mac[6];
    uint32_t src_ip;                   /* host order */
    uint16_t src_port;
    char line[JW_LINE_MAX + 1u];       /* the control line, no line end */
};

/* a frame from the network (eth, len); our_ip is the DHCP address in host order, 0: none yet (then nothing is answered). out takes
 * an ARP reply (out_cap bytes; 42 needed) and *out_len its length */
int jw_input(const uint8_t our_mac[6], uint32_t our_ip, const uint8_t *eth, size_t len, struct jw_req *req, uint8_t *out, size_t out_cap, size_t *out_len);

/* chunks needed for an answer of `len` text bytes (an empty answer is one chunk) */
unsigned jw_chunks(size_t len);

/* the datagram carrying chunk `index` of an answer, to the sender of `to`: its frame length, 0 if n > JW_CHUNK, index > 127 or no room */
size_t jw_reply_frame(const uint8_t our_mac[6], uint32_t our_ip, const struct jw_req *to, unsigned index, int last, const char *text, size_t n,
                      uint8_t *out, size_t cap);

/* 1: a control line the Wi-Fi service may run (exact words, all read-only) */
int jw_allowed(const char *line);
#endif
