/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat network's weights, kept OUT of the firmware image: a blob in their own flash partition ("beatw"), written by
 * the Install page's builder at the user's click (madmom's model, converted in the browser) or by
 * beat/scripts/export_beat_c.py (our own net). With no valid blob the beat tracker does not start; the rest runs.
 *
 * Blob, little endian:  "JBW1"  u32 count (BEAT_WEIGHTS_COUNT)  u32 crc32 (zlib's, of the float bytes)  u32 0
 *                       float32[count] in beat_model.h's order: per LSTM layer w, r, b, p; then out_w, out_b. */
#ifndef BEAT_WEIGHTS_H
#define BEAT_WEIGHTS_H
#include <stddef.h>
#include <stdint.h>

#define BEAT_WEIGHTS_MAGIC "JBW1"
#define BEAT_WEIGHTS_HEADER 16u
/* l0_w [4][162][25]; l0_r, l1_w, l1_r, l2_w, l2_r [4][25][25]; b [4][25] and p [3][25] per layer; out_w [25], out_b [1] */
#define BEAT_WEIGHTS_COUNT (4u * 162u * 25u + 5u * 4u * 25u * 25u + 3u * (4u * 25u + 3u * 25u) + 25u + 1u)   /* 29251 */

enum beat_weights_err { BW_OK = 0, BW_SHORT, BW_MAGIC, BW_COUNT, BW_CRC, BW_ALIGN };

uint32_t beat_weights_crc32(const void *data, size_t n);   /* zlib's CRC-32 */
/* Checks the blob and points beat_model.h's weight pointers into it (no copy: blob must stay mapped). BW_OK or why not;
 * on failure the pointers are left NULL. */
enum beat_weights_err beat_weights_bind(const void *blob, size_t n);
int beat_weights_ready(void);
const char *beat_weights_why(enum beat_weights_err e);
void beat_weights_unbind(void);                           /* the pointers back to NULL (tests) */

#endif
