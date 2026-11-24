/* SPDX-License-Identifier: GPL-3.0-only */
/* Updating the firmware over Wi-Fi: signed binary requests on the Wi-Fi service's UDP port (jank_wsvc.h), carrying the same transfer as the
 * control line's `ota` lines (jank_ota.h: begin, data chunks at an offset with their CRC, end), plus a confirm. The text service stays
 * read-only; these frames are the only thing that can change the unit over the network, and each one is signed:
 *
 *   key      32 bytes, set once over the USB control line (`otakey <64 hex>`), kept in flash, never shown, never sent. No key: every frame
 *            here is ignored without an answer.
 *   HELLO    01 01                          -> 01 81 <nonce 8>     a fresh random nonce, which starts a session
 *   request  01 02 <counter u32> <op> <args> <mac 16>
 *            mac = HMAC-SHA-256(key, nonce || every byte before the mac) cut to 16 bytes
 *   reply    01 82 <counter u32> <status> <offset u32> <chunk u16> <mac 16>   (same mac rule: the client knows it is the unit that answered)
 *   ops      1 BEGIN <size u32> <crc u32> <target u8>      2 DATA <offset u32> <crc u32> <n u16> <n bytes, at most 1024>
 *            3 END      4 ABORT      5 CONFIRM (the running image, still on probation after an update, is confirmed)
 *   status   an enum jo_err (0 ok, 1 old chunk again: also fine ...) or a JWO_S_*; the offset is where the sender goes on from
 *
 * A request with a wrong mac, a counter that is not above the last one, or outside a session (no HELLO, or 30 s since the last signed request)
 * is dropped without an answer: no oracle for guessing, no reflection. Every try, a resend included, carries a higher counter (the transfer
 * is idempotent: an old chunk again answers "ok, next offset"). A HELLO while a session is busy (a signed request in the last 5 s) is
 * ignored, so nobody on the network can knock a transfer off by sending one. Pure (no clock, no flash, no random source: they are passed
 * in). Host-tested (tests/test_wota.c), with the same vectors as scripts/jankwifi.py's tests. */
#ifndef JANK_WOTA_H
#define JANK_WOTA_H
#include <stddef.h>
#include <stdint.h>

#define JWO_MAGIC 0x01u
#define JWO_T_HELLO 0x01u
#define JWO_T_CMD 0x02u
#define JWO_T_HELLO_R 0x81u
#define JWO_T_CMD_R 0x82u

#define JWO_KEY 32u
#define JWO_NONCE 8u
#define JWO_MAC 16u
#define JWO_DATA_MAX 1024u                           /* bytes of image in one DATA request */
#define JWO_HELLO_LEN 2u
#define JWO_HELLO_R_LEN (2u + JWO_NONCE)
#define JWO_HEAD 7u                                  /* magic, type, counter, op */
#define JWO_FRAME_MAX (JWO_HEAD + 4u + 4u + 2u + JWO_DATA_MAX + JWO_MAC)   /* the biggest request: a full DATA */
#define JWO_REPLY_LEN (2u + 4u + 1u + 4u + 2u + JWO_MAC)
#define JWO_SESSION_MS 30000u                        /* a session ends this long after its last signed request */
#define JWO_HELLO_LOCK_MS 5000u                      /* a HELLO is ignored while the last signed request is younger than this */

enum jwo_op { JWO_OP_BEGIN = 1, JWO_OP_DATA, JWO_OP_END, JWO_OP_ABORT, JWO_OP_CONFIRM };

/* statuses beyond enum jo_err (0..11, jank_ota.h) */
#define JWO_S_BAD_OP 0x80u                           /* a signed request of an op this unit does not know */
#define JWO_S_NOT_PENDING 0x81u                      /* CONFIRM: the running image is not on probation */
#define JWO_S_TOO_EARLY 0x82u                        /* CONFIRM: it has not run long enough (20 s) */

struct jwo {
    uint8_t key[JWO_KEY];
    uint8_t key_set;
    uint8_t have;                                    /* a session is open: nonce, last_ctr, last_ms are valid */
    uint8_t nonce[JWO_NONCE];
    uint32_t last_ctr;
    uint32_t last_ms;
};

struct jwo_req {
    uint32_t counter;
    uint8_t op;                                      /* enum jwo_op, or whatever a signed frame said */
    uint8_t target;                                  /* BEGIN: JO_TARGET_* */
    uint32_t size, crc, offset;                      /* BEGIN: size, crc (the image's); DATA: offset, crc (the chunk's) */
    uint16_t n;                                      /* DATA: bytes at data */
    const uint8_t *data;                             /* DATA: inside the frame passed to jwo_input, valid only as long as that is */
};

enum jwo_in {
    JWO_NONE,                                        /* dropped: nothing to answer */
    JWO_HELLO,                                       /* answered: the reply is in out (JWO_HELLO_R_LEN bytes) */
    JWO_CMD                                          /* a request that checked out: in *req */
};

void jwo_init(struct jwo *s);
void jwo_set_key(struct jwo *s, const uint8_t key[JWO_KEY]);   /* a new key ends any session */
void jwo_clear_key(struct jwo *s);

/* a frame from the network (f, n bytes) at time now_ms. rnd: 8 fresh random bytes, used only if this is a HELLO that is accepted. A HELLO's
 * reply goes into out (cap bytes, JWO_HELLO_R_LEN needed) with *out_len its length. */
enum jwo_in jwo_input(struct jwo *s, const uint8_t *f, size_t n, uint32_t now_ms, const uint8_t rnd[JWO_NONCE], struct jwo_req *req, uint8_t *out, size_t cap,
                      size_t *out_len);

/* the reply to a request: its length (JWO_REPLY_LEN), 0 if there is no room or no session */
size_t jwo_reply(const struct jwo *s, uint32_t counter, uint8_t status, uint32_t offset, uint16_t chunk, uint8_t *out, size_t cap);

/* the sender's half (the host tests use it as the reference for scripts/jankwifi.py): a request frame, its length (0: no room, n over the
 * maximum) */
size_t jwo_build(const uint8_t key[JWO_KEY], const uint8_t nonce[JWO_NONCE], const struct jwo_req *r, uint8_t *out, size_t cap);
/* 1: f is a reply from the unit that has the key, to a request of any counter: the fields are filled */
int jwo_check_reply(const uint8_t key[JWO_KEY], const uint8_t nonce[JWO_NONCE], const uint8_t *f, size_t n, uint32_t *counter, uint8_t *status,
                    uint32_t *offset, uint16_t *chunk);
#endif
