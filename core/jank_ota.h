/* SPDX-License-Identifier: GPL-3.0-only */
/* Firmware update over the control line (docs/usb.md, "Updating over USB MIDI"). The pure half: line parsing, base64, CRC-32
 * and the transfer's state machine. The flash writes, the restart and the clock are firmware/main/ota.c's.
 *
 *   ota begin <size> <crc32 as 8 hex>        -> ok 0 <chunk>          the image's size in bytes, its CRC-32 (zlib's)
 *   ota d <offset> <crc32 as 8 hex> <base64> -> ok <next offset>      one chunk of at most JO_CHUNK_MAX bytes; the CRC is the chunk's
 *   ota end                                  -> ok                    size and CRC checked, then the restart
 *   ota abort                                -> ok
 * An error is "err <name> <offset>": <offset> is where the sender goes on from. A chunk below the expected offset is an
 * old one sent again (the acknowledgement was lost): it is not written and is answered "ok <next offset>". */
#ifndef JANK_OTA_H
#define JANK_OTA_H
#include <stddef.h>
#include <stdint.h>

#define JO_CHUNK_MAX 120u               /* bytes per chunk: 160 base64 characters, a line of about 190 */
#define JO_LINE_MAX 256u
#define JO_IDLE_MS 20000u               /* no word from the sender for this long: the transfer is dropped */
#define JO_CHIP_ESP32S3 9u              /* esp_image_header_t.chip_id */
#define JO_HEADER_LEN 24u               /* esp_image_header_t */

enum jo_kind { JO_NOT_OTA = 0, JO_BEGIN, JO_DATA, JO_END, JO_ABORT, JO_BAD };

enum jo_err {
    JO_OK = 0,
    JO_OK_DUP,                           /* an old chunk again: answer ok, write nothing */
    JO_E_BUSY,                           /* a transfer is running */
    JO_E_SIZE,                           /* zero, over the slot, or the chunks run past the size */
    JO_E_ARG,                            /* a malformed line */
    JO_E_STATE,                          /* no transfer */
    JO_E_GAP,                            /* a chunk from beyond the expected offset */
    JO_E_CRC,                            /* a chunk's CRC, or the image's */
    JO_E_HEADER,                         /* not an ESP32-S3 image */
    JO_E_SHORT,                          /* end before the last byte */
    JO_E_IDLE                            /* dropped after JO_IDLE_MS */
};

struct jo_cmd {
    enum jo_kind kind;
    uint32_t size, crc, offset;
    size_t n;
    uint8_t data[JO_CHUNK_MAX];
};

struct jo {
    uint8_t active;
    uint32_t size, crc_want, offset, crc_run, last_ms;
};

/* zlib's crc32: pass the last result to go on, 0 to start */
uint32_t jo_crc32(uint32_t crc, const uint8_t *data, size_t n);
size_t jo_b64_encode(const uint8_t *in, size_t n, char *out, size_t cap);   /* with '=' padding, no NUL counted; 0 when cap is short */
int jo_b64_decode(const char *in, size_t len, uint8_t *out, size_t cap);    /* -> bytes, or -1 for anything but a whole, padded text */

/* the line -> its kind; JO_NOT_OTA for any other line, JO_BAD for an ota line that does not parse */
enum jo_kind jo_parse(const char *line, struct jo_cmd *c);
/* the first bytes of an image: 0xE9, 1..16 segments, the S3's chip id */
int jo_header_ok(const uint8_t *first, size_t n);

void jo_init(struct jo *j);
enum jo_err jo_begin(struct jo *j, uint32_t size, uint32_t crc, uint32_t slot_size, uint32_t now_ms);
enum jo_err jo_data(struct jo *j, const struct jo_cmd *c, uint32_t now_ms);   /* JO_OK: write c->data; the offset moves on */
enum jo_err jo_end(struct jo *j);                                              /* JO_OK: all of it, CRC right; the transfer is over */
void jo_abort(struct jo *j);
enum jo_err jo_idle(struct jo *j, uint32_t now_ms);                            /* JO_E_IDLE once, when it drops the transfer */

/* the answer line for e and the offset to go on from -> its length */
size_t jo_reply(enum jo_err e, uint32_t offset, char *out, size_t cap);

#endif
