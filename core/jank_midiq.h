/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode's MIDI out, one queue a device (firmware/main/usb_midi_host.c): the 4-byte USB-MIDI event packets that come
 * while the device's transfer is still out wait here, in order, and go in its next transfer. A clock (F8) is never thrown
 * away for a busy transfer: a missed F8 moves the receiver's count, and so its bar, by a tick for good; a late one costs
 * only the wait. Only packets past the queue's room are lost (counted). No ESP-IDF: host-tested (tests/test_midiq.c). */
#ifndef JANK_MIDIQ_H
#define JANK_MIDIQ_H
#include <stddef.h>
#include <stdint.h>

#define JMQ_BYTES 64u                  /* 16 packets: a full-speed bulk packet */

struct jmq {
    uint8_t n;                         /* bytes waiting, a multiple of 4 */
    uint8_t b[JMQ_BYTES];
};

/* appends whole packets (n rounded down to 4); returns the packets that did not fit */
uint32_t jmq_add(struct jmq *q, const uint8_t *packets, size_t n);
/* takes the oldest whole packets, at most max bytes, into out; returns the bytes taken */
size_t jmq_take(struct jmq *q, uint8_t *out, size_t max);
#endif
