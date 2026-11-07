/* SPDX-License-Identifier: GPL-3.0-only */
/* The serial test log of a board with no computer on it: a line a second for the first air tests. */
#ifndef JANK_LOG_H
#define JANK_LOG_H
#include <stddef.h>
#include <stdint.h>
#include "jank_core.h"

/* "# up=.. group=.. ch=.. heard=g1,..,g9 peers=.. rssi=.. sent=.. tx_fail=.. uni_ok=.. uni_fail=.. link=.. link_peers=.. bpm=.."
 * heard counts frames of each group on the channels the radio was on, since boot. Returns the length, 0 if it did not fit. */
size_t jk_log_line(char *out, size_t cap, const struct jk_snapshot *s, const uint32_t heard[JK_GROUP_MAX + 1u], uint32_t uni_ok, uint32_t uni_fail, uint32_t now_ms);
#endif
