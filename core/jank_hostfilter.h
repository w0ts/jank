/* SPDX-License-Identifier: GPL-3.0-only */
/* What the USB host stack does not enumerate. The S3's controller has 8 host channels (usb_hostlib.c): every enumerated device's control
 * pipe takes one for as long as it is attached, used or not. A hub (control + interrupt: 2), a Launchpad X (control, OUT, IN: 3) and a
 * Digitone (3) are exactly 8. A multi-function dock adds a card reader and a video adapter with a control pipe each, and then the
 * Launchpad and the Digitone cannot claim their endpoints (measured 2026-11-24: claim 262 and 257 behind such a dock).
 *
 * The unit learns what is useless to it: a device with neither a MIDI nor an audio interface (usb_midi_host.c). When a Launchpad or a
 * Digitone then cannot be claimed for want of pipes, the useless devices seen are added to a short list kept in flash (hostskip.c) and the
 * unit restarts once; from then on the library's enumeration filter (enum_filter_cb) leaves the listed devices alone behind a hub, before
 * they hold a pipe. A device on the port itself is never skipped (no pipes shared with anyone). The list is bounded, so the restarts are:
 * each one adds an entry, and a full list adds none. Pure: the sets, the rule, the stored form. Host-tested (tests/test_hostfilter.c). */
#ifndef JANK_HOSTFILTER_H
#define JANK_HOSTFILTER_H
#include <stddef.h>
#include <stdint.h>

#define JHF_CLASS_HUB 9u               /* bDeviceClass of a hub */
#define JHF_MAX 8u                     /* devices in a set */
#define JHF_PACK_MAX (1u + 4u * JHF_MAX)

struct jhf_id {
    uint16_t vid, pid;
};

struct jhf_set {
    uint8_t n;
    struct jhf_id id[JHF_MAX];
};

struct jhf {
    uint8_t hub_seen;                  /* a hub came: devices after it share its pipes */
    uint32_t skipped;                  /* devices not enumerated */
    uint16_t skip_vid, skip_pid;       /* the last one skipped */
    struct jhf_set deny;               /* what is left alone behind a hub (the learned list) */
};

/* 1: v:p was added (it was not in the set and there was room) */
int jhf_set_add(struct jhf_set *s, uint16_t vid, uint16_t pid);
int jhf_set_has(const struct jhf_set *s, uint16_t vid, uint16_t pid);
/* adds what is in from and not in to; returns how many were added */
unsigned jhf_set_merge(struct jhf_set *to, const struct jhf_set *from);

/* 1: enumerate the device (the descriptor's class and IDs); 0: leave it alone (counted in f). A hub always is; anything is until a hub has come */
int jhf_proceed(struct jhf *f, uint8_t dev_class, uint16_t vid, uint16_t pid);

/* the set as stored: a count, then each vid and pid little-endian; returns the length (at most JHF_PACK_MAX) */
size_t jhf_pack(const struct jhf_set *s, uint8_t out[JHF_PACK_MAX]);
/* 1: p / n is a stored set (right length, count in range); s is then filled. Otherwise s is left empty and 0 is returned */
int jhf_unpack(struct jhf_set *s, const uint8_t *p, size_t n);

#endif
