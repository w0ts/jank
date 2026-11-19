/* SPDX-License-Identifier: GPL-3.0-only */
/* The CLK / RST decision (jank_clockout.h). Runs in the clock interrupt: table lookups and shifts, no division. */
#include "jank_clockout.h"
#include "jank_clock.h"
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define JCO_ISR IRAM_ATTR
#else
#define JCO_ISR
#endif

#define JCO_DEFAULT_RATE 2
#define JCO_TICKS 24                       /* a beat */

/* bit p set: a step begins at tick p of the beat. Every step length divides 24, so a rate change keeps the grid */
static const uint32_t STEP_MASK[JCO_RATES] = {0x000001u, 0x001001u, 0x041041u, 0x249249u, 0xFFFFFFu};
static const uint8_t STEP_TICKS[JCO_RATES] = {24, 12, 6, 3, 1};

void jco_init(struct jco *s) { s->running = s->pos = s->reset = 0; }

unsigned jco_ticks_per_step(unsigned rate) { return STEP_TICKS[rate < JCO_RATES ? rate : JCO_DEFAULT_RATE]; }

struct jco_out JCO_ISR jco_fire(struct jco *s, const uint8_t *bytes, unsigned n, unsigned rate)
{
    struct jco_out o = {0, 0};
    if (rate >= JCO_RATES)
        rate = JCO_DEFAULT_RATE;
    for (unsigned i = 0; i < n; i++) {
        if (bytes[i] == JC_STOP) {
            s->running = 0;
            s->reset = 0;
        } else if (bytes[i] == JC_START) {
            s->running = 1;
            s->pos = 0;
            s->reset = 1;
        } else if (bytes[i] == JC_CLOCK && s->running) {
            if ((STEP_MASK[rate] >> s->pos) & 1u)
                o.clk = 1;
            o.rst = s->reset;
            s->reset = 0;
            s->pos = (uint8_t)(s->pos + 1u == JCO_TICKS ? 0u : s->pos + 1u);
        }
    }
    return o;
}
