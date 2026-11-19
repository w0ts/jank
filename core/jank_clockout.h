/* SPDX-License-Identifier: GPL-3.0-only */
/* The CLK and RST outputs (the mini 3's jacks): short pulses, like Mutable Instruments Grids. The decision only, no ESP-IDF:
 * given the bytes of one MIDI tick (jc_fire: FC, FA, F8) it says whether CLK and RST go high now. Both come down again
 * JCO_PULSE_US later (the firmware arms a one-shot timer for it); velocity and accents play no part.
 *   CLK  one pulse per clock step: the CLK RATE setting says how many MIDI ticks a step is (24 a beat)
 *   RST  one pulse at step 0, the first clock after a MIDI Start
 * Nothing moves while stopped (before the first Start, and after a Stop). The steps are counted from the Start, which the
 * clock sends on a bar line, so they stay on the beat grid. */
#ifndef JANK_CLOCKOUT_H
#define JANK_CLOCKOUT_H
#include <stdint.h>

#define JCO_PULSE_US 1000                  /* how long a pulse stays high */
#define JCO_RATES 5                        /* the CLK RATE setting 0..4: 1/4, 1/8, 1/16, 1/32, 24 PPQN */

struct jco {
    uint8_t running;                       /* a Start went by, no Stop since */
    uint8_t pos;                           /* the tick in the beat, 0..23 */
    uint8_t reset;                         /* the Start's reset pulse is still to come (with the next clock) */
};

struct jco_out {
    uint8_t clk, rst;                      /* 1: goes high now */
};

void jco_init(struct jco *s);
/* MIDI ticks in a step for a rate setting: 24, 12, 6, 3, 1 (a rate out of range counts as the default, 1/16) */
unsigned jco_ticks_per_step(unsigned rate);
/* the bytes of one tick, in the order the clock sends them */
struct jco_out jco_fire(struct jco *s, const uint8_t *bytes, unsigned n, unsigned rate);
#endif
