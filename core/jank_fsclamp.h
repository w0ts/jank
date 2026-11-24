/* SPDX-License-Identifier: GPL-3.0-only */
/* Full-speed bulk endpoints carry at most 64 bytes (USB 2.0, 5.8.3), but a device behind a full-speed link can still hand out its
 * high-speed descriptor: the Digitone II declares bulk endpoints of 512 bytes there (measured 2026-11-24, hostdiag_dev mps:512/512).
 * The ESP-IDF host library refuses such an endpoint for good (its claim answers ESP_ERR_NOT_SUPPORTED: the controller's FIFO limit is
 * checked against the declared size, 64 bytes for bulk OUT in this build), so the device could never be used. On a full-speed link no
 * packet can be longer than 64 anyway: this clamps the declared size to what the wire allows, in the configuration descriptor the
 * library holds, before the claim. Pure: a walk over the descriptor bytes. Host-tested (tests/test_fsclamp.c). */
#ifndef JANK_FSCLAMP_H
#define JANK_FSCLAMP_H
#include <stddef.h>
#include <stdint.h>

#define JFC_FS_BULK_MAX 64u

/* every bulk endpoint descriptor in cfg (len bytes: wTotalLength) whose max packet size is over 64 gets 64; the high-bandwidth bits
 * (11-12, high speed only) are cleared with it. Returns how many endpoints were changed. A malformed descriptor (a length of 0 or 1, one
 * running past len) ends the walk where it is: what was read before it stays as it was. */
unsigned jfc_clamp_fs_bulk(uint8_t *cfg, size_t len);

#endif
