/* SPDX-License-Identifier: GPL-3.0-only */
/* The Launchpad's mixer screen (jank_lpmix.h). */
#include "jank_lpmix.h"
#include <string.h>

#define PAD_ROWS 8u
#define PAN_CENTRE_LOW 3u              /* the centre pair of the pan's 8 positions */
#define PAN_CENTRE_HIGH 4u
#define VALUE_MAX 127u

void jlmx_init(struct jlmx *s)
{
    memset(s, 0, sizeof *s);
    s->view = JLMX_VOL;
}

void jlmx_on_mixer(struct jlmx *s, const struct jdl_mixer *m)
{
    s->m = *m;
    s->have = 1;
}

int jlmx_scene(struct jlmx *s, uint8_t idx)
{
    if (idx >= JLMX_VIEWS)
        return 0;
    s->view = idx;
    return 1;
}

int jlmx_bank_step(struct jlmx *s, int dir)
{
    if (dir > 0 && s->bank + 1u < JLMX_BANKS) {
        s->bank++;
        return 1;
    }
    if (dir < 0 && s->bank > 0u) {
        s->bank--;
        return 1;
    }
    return 0;
}

/* the value a view shows for a track */
static uint8_t value_of(const struct jdl_mix_track *t, uint8_t view)
{
    switch (view) {
    case JLMX_VOL: return t->vol;
    case JLMX_PAN: return t->pan;
    case JLMX_DEL: return t->del;
    case JLMX_REV: return t->rev;
    default: return t->chr;
    }
}

/* pads lit from the bottom for a value 0..127: 0 none, 1 for the smallest, 8 for the largest (every value above 0 shows at least the bottom pad) */
static unsigned fader_height(uint8_t v)
{
    if (v > VALUE_MAX)
        v = VALUE_MAX;
    return v ? ((unsigned)v * PAD_ROWS + VALUE_MAX - 1u) / VALUE_MAX : 0u;
}

/* the pan's one lit position, 0 (left) .. 7 (right) */
static unsigned pan_position(uint8_t v)
{
    if (v > VALUE_MAX)
        v = VALUE_MAX;
    return ((unsigned)v * (PAD_ROWS - 1u)) / VALUE_MAX;
}

static uint8_t column_colour(const struct jlmx *s, unsigned track, unsigned row)
{
    const struct jdl_mix_track *t = &s->m.t[track];
    if (s->view == JLMX_MUTE)
        return (s->m.mute >> track) & 1u ? JLMX_COLOR_MUTED : JLMX_COLOR_UNMUTED;
    if (s->view == JLMX_SOLO)
        return (s->m.solo >> track) & 1u ? JLMX_COLOR_SOLO : JLMX_COLOR_NOSOLO;
    if (s->view == JLMX_PAN) {
        unsigned p = pan_position(t->pan);
        if (row == p)
            return JLMX_COLOR_FADER;
        return row == PAN_CENTRE_LOW || row == PAN_CENTRE_HIGH ? JLMX_COLOR_FADER_THIRD : 0;
    }
    return row < fader_height(value_of(t, s->view)) ? JLMX_COLOR_FADER : 0;
}

void jlmx_frame(const struct jlmx *s, struct jlx_frame *f)
{
    for (uint8_t x = 0; x < JLMX_COLUMNS; x++)
        for (uint8_t y = 0; y < PAD_ROWS; y++)
            f->led[JLX_PAD0 + y * 8u + x].color = s->have ? column_colour(s, s->bank * JLMX_COLUMNS + x, y) : 0;
}

uint8_t jlmx_scene_colour(const struct jlmx *s, uint8_t idx)
{
    int on = s->view == idx;
    if (idx <= JLMX_CHR)
        return on ? JLMX_COLOR_FADER : JLMX_COLOR_FADER_THIRD;
    if (idx == JLMX_MUTE)
        return on ? JLMX_COLOR_UNMUTED : JLMX_COLOR_MUTED;
    if (idx == JLMX_SOLO)
        return on ? JLMX_COLOR_SOLO : JLMX_COLOR_NOSOLO;
    return 0;
}
