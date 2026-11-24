/* SPDX-License-Identifier: GPL-3.0-only */
/* The Launchpad's mixer screen (jank_lpmix.h). */
#include "jank_lpmix.h"
#include <string.h>

#define PAD_ROWS 8u
#define PAN_CENTRE_LOW 3u              /* the centre pair of the pan's 8 positions */
#define PAN_CENTRE_HIGH 4u
#define PAN_CENTRE_A 63u               /* Launchpad 95's _send_value_pan: 63 and 64 are the centre */
#define PAN_CENTRE_B 64u
#define OVERVIEW_LAST_VALUE_ROW 4u     /* the overview's rows from the top: 0 volume .. 4 chorus (the views), 5 mute, 6 solo, 7 nothing */
#define OVERVIEW_MUTE_ROW 5u
#define OVERVIEW_SOLO_ROW 6u

/* Launchpad 95's value maps (SubSelectorComponent.py), its 0..1 values on the Digitone's 0..127. Volume: 0 and the dB steps -42 -24 -18 -12 -6 0 +6 through its level_to_value
 * (0.1424 0.3066 0.40 0.55 0.70 0.85 1.0); the sends: SEND_VALUE_MAP; pan: PAN_VALUE_MAP (-1 -0.63 -0.32 0 0 0.32 0.63 1) as 0..127, the two middle pads the centre (64). The
 * Digitone's own curves are not measured: these are Launchpad 95's steps taken as fractions of its range. */
static const uint8_t VOL_MAP[PAD_ROWS] = {0, 18, 39, 51, 70, 89, 108, 127};
static const uint8_t SEND_MAP[PAD_ROWS] = {0, 13, 21, 30, 44, 70, 98, 127};
static const uint8_t PAN_MAP[PAD_ROWS] = {0, 23, 43, 64, 64, 84, 104, 127};

void jlmx_init(struct jlmx *s)
{
    memset(s, 0, sizeof *s);
    s->view = JLMX_OVERVIEW;
}

void jlmx_on_mixer(struct jlmx *s, const struct jdl_mixer *m, uint32_t now_ms)
{
    if (s->have && s->held && (uint32_t)(now_ms - s->hold_ms) < JLMX_HOLD_MS)
        return;                        /* a reading from before the press was applied */
    s->held = 0;
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

void jlmx_overview(struct jlmx *s) { s->view = JLMX_OVERVIEW; }

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

/* a pad row counted from the top (0 the top one), as Launchpad 95 counts them */
static unsigned row_from_top(uint8_t y) { return PAD_ROWS - 1u - y; }

int jlmx_open(struct jlmx *s, uint8_t y)
{
    unsigned r = row_from_top(y);
    if (s->view != JLMX_OVERVIEW || y >= PAD_ROWS || r > OVERVIEW_LAST_VALUE_ROW)
        return 0;
    s->view = (uint8_t)r;              /* the rows are in the scene buttons' order: volume, pan, delay, reverb, chorus */
    return 1;
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

static const uint8_t *map_of(uint8_t view)
{
    switch (view) {
    case JLMX_VOL: return VOL_MAP;
    case JLMX_PAN: return PAN_MAP;
    default: return SEND_MAP;
    }
}

/* Launchpad 95's PreciseButtonSliderElement._send_value_volume: nothing is lit for 0, else the pads up to the first step the value does not pass */
static int volume_lit(const uint8_t *map, uint8_t v, unsigned row)
{
    if (v == 0)
        return 0;
    for (unsigned i = 0; i < PAD_ROWS; i++)
        if (v <= map[i])
            return row <= i;
    return 1;                          /* above the top step (a value past 127): the whole fader */
}

/* ... and _send_value_pan: the centre pair for 63 / 64, else a bar from the centre pair out to the value */
static int pan_lit(uint8_t v, unsigned row)
{
    if (v == PAN_CENTRE_A || v == PAN_CENTRE_B)
        return row == PAN_CENTRE_LOW || row == PAN_CENTRE_HIGH;
    if (v < PAN_CENTRE_A)
        return row <= PAN_CENTRE_LOW && PAN_MAP[row] >= v;
    return row >= PAN_CENTRE_HIGH && PAN_MAP[row] <= v;
}

static uint8_t *value_ref(struct jdl_mix_track *t, uint8_t view)
{
    switch (view) {
    case JLMX_VOL: return &t->vol;
    case JLMX_PAN: return &t->pan;
    case JLMX_DEL: return &t->del;
    case JLMX_REV: return &t->rev;
    default: return &t->chr;
    }
}

static uint8_t param_of(uint8_t view)
{
    switch (view) {
    case JLMX_VOL: return JDL_MIX_VOL;
    case JLMX_PAN: return JDL_MIX_PAN;
    case JLMX_DEL: return JDL_MIX_DEL;
    case JLMX_REV: return JDL_MIX_REV;
    default: return JDL_MIX_CHR;
    }
}

/* mute or solo of a track toggled: the whole new mask (the view is MUTE or SOLO, or the overview's mute / solo row) */
static void toggle_mask(struct jlmx *s, unsigned track, int solo, struct jlmx_write *w)
{
    uint32_t *mask = solo ? &s->m.solo : &s->m.mute;
    *mask ^= 1u << track;
    w->kind = JLMX_WRITE_MASK;
    w->which = solo ? 1u : 0u;
    w->mask = (uint16_t)*mask;
}

int jlmx_press(struct jlmx *s, uint8_t x, uint8_t y, uint32_t now_ms, struct jlmx_write *w)
{
    unsigned track = s->bank * JLMX_COLUMNS + x;
    if (!s->have || x >= JLMX_COLUMNS || y >= PAD_ROWS)
        return 0;
    memset(w, 0, sizeof *w);
    if (s->view == JLMX_OVERVIEW) {
        unsigned r = row_from_top(y);
        if (r != OVERVIEW_MUTE_ROW && r != OVERVIEW_SOLO_ROW)
            return 0;                  /* the value rows open a view (jlmx_open); the last row has no job */
        toggle_mask(s, track, r == OVERVIEW_SOLO_ROW, w);
    } else if (s->view == JLMX_MUTE || s->view == JLMX_SOLO) {
        toggle_mask(s, track, s->view == JLMX_SOLO, w);
    } else {
        uint8_t nv = map_of(s->view)[y];   /* the pad's step: pressing the lowest sets 0 and darkens the fader, as Launchpad 95 does */
        *value_ref(&s->m.t[track], s->view) = nv;
        w->kind = JLMX_WRITE_VALUE;
        w->track = (uint8_t)track;
        w->param = param_of(s->view);
        w->value = nv;
    }
    s->held = 1;
    s->hold_ms = now_ms;
    return 1;
}

static uint8_t overview_colour(const struct jlmx *s, unsigned track, unsigned row)
{
    unsigned r = row_from_top((uint8_t)row);
    if (r <= OVERVIEW_LAST_VALUE_ROW)  /* Launchpad 95's Mixer.Volume / Pan / Sends: MINT, a pad that opens its fader screen */
        return JLMX_COLOR_FADER;
    if (r == OVERVIEW_MUTE_ROW)
        return (s->m.mute >> track) & 1u ? JLMX_COLOR_MUTED : JLMX_COLOR_UNMUTED;
    if (r == OVERVIEW_SOLO_ROW)
        return (s->m.solo >> track) & 1u ? JLMX_COLOR_SOLO : JLMX_COLOR_NOSOLO;
    return 0;
}

static uint8_t column_colour(const struct jlmx *s, unsigned track, unsigned row)
{
    const struct jdl_mix_track *t = &s->m.t[track];
    uint8_t v;
    if (s->view == JLMX_OVERVIEW)
        return overview_colour(s, track, row);
    if (s->view == JLMX_MUTE)
        return (s->m.mute >> track) & 1u ? JLMX_COLOR_MUTED : JLMX_COLOR_UNMUTED;
    if (s->view == JLMX_SOLO)
        return (s->m.solo >> track) & 1u ? JLMX_COLOR_SOLO : JLMX_COLOR_NOSOLO;
    v = value_of(t, s->view);
    if (s->view == JLMX_PAN)
        return pan_lit(v, row) ? JLMX_COLOR_FADER : 0;
    return volume_lit(map_of(s->view), v, row) ? JLMX_COLOR_FADER : 0;
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
