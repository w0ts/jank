/* SPDX-License-Identifier: GPL-3.0-only */
/* Launchpad Note mode's pad layouts (jank_lpnote.h): a port of Launchpad 95's MelodicPattern. */
#include "jank_lpnote.h"

#define TOP_OCTAVE_DIATONIC 6u         /* LP95's TOP_OCTAVE: octaves 0 .. top - 1 */
#define TOP_OCTAVE_OTHER 7u
#define DEFAULT_OCTAVE 3u
#define CHROMATIC_SIZE 12              /* a chromatic pattern runs over the 12 notes from the key */
#define CHROMATIC_ROW_STEP 5           /* a row up is a fourth */
#define DIATONIC_ROW_STEP 3
#define CHORDS_COLUMN_STEP 2

static uint8_t top_octave(uint8_t layout) { return layout == JLN_DIATONIC ? TOP_OCTAVE_DIATONIC : TOP_OCTAVE_OTHER; }

void jln_init(struct jln *s)
{
    s->key = 0;
    s->octave = DEFAULT_OCTAVE;
    s->absolute = 0;
    s->track = 0;
    s->layout = JLN_DIATONIC;
    s->horizontal = 1;
}

void jln_set_layout(struct jln *s, enum jln_layout l)
{
    s->layout = (uint8_t)l;
    s->horizontal = l != JLN_CHORDS;
    if (s->octave >= top_octave(s->layout))
        s->octave = (uint8_t)(top_octave(s->layout) - 1u);
}

void jln_octave_step(struct jln *s, int dir)
{
    if (dir > 0 && s->octave + 1u < top_octave(s->layout))
        s->octave++;
    else if (dir < 0 && s->octave > 0)
        s->octave--;
}

void jln_track_step(struct jln *s, int dir)
{
    if (dir > 0 && s->track + 1u < JLN_TRACKS)
        s->track++;
    else if (dir < 0 && s->track > 0)
        s->track--;
}

/* floored division and remainder: a negative index goes down an octave, as Python's // and % do */
static int floor_div(int a, int b) { return a / b - (a % b != 0 && ((a < 0) != (b < 0))); }

static int floor_mod(int a, int b) { return a - b * floor_div(a, b); }

/* the pad's index along the scale, LP95's steps[0] * (origin[0] + x) + steps[1] * (origin[1] + y) */
static int pad_index(const struct jln *s, const struct jln_scale *sc, uint8_t x, uint8_t y)
{
    int chromatic = s->layout == JLN_CHROMATIC, origin = 0, step_x, step_y, ox, oy;
    int first = s->key;                                        /* scale[0]: the key's note */
    if (s->absolute) {
        if (chromatic) {
            origin = -first;
        } else {
            for (int k = 0; k < sc->n; k++)
                if (first + sc->iv[k] >= 12) {
                    origin = k - sc->n;
                    break;
                }
        }
    }
    if (chromatic) {                                           /* orientation fixed: a row up is a fourth */
        step_x = 1;
        step_y = CHROMATIC_ROW_STEP;
        ox = origin;
        oy = 0;
    } else if (s->horizontal) {
        step_x = 1;
        step_y = DIATONIC_ROW_STEP;
        ox = origin;
        oy = 0;
    } else {
        step_x = CHORDS_COLUMN_STEP;
        step_y = 1;
        ox = 0;
        oy = origin;
    }
    return step_x * (ox + x) + step_y * (oy + y);
}

void jln_pad(const struct jln *s, const struct jln_scale *sc, uint8_t x, uint8_t y, struct jln_pad *out)
{
    int chromatic = s->layout == JLN_CHROMATIC;
    int size = chromatic ? CHROMATIC_SIZE : sc->n;
    int index = pad_index(s, sc, x, y);
    int k = floor_mod(index, size);
    int note_in_octave = chromatic ? s->key + k : s->key + sc->iv[k];
    int midi = (s->octave + 1) * 12 + 12 * floor_div(index, size) + note_in_octave;
    out->note = (int16_t)(midi >= 0 && midi < 128 ? midi : -1);
    out->root = note_in_octave == s->key;
    out->in_scale = 0;
    for (int i = 0; i < sc->n; i++)
        if (note_in_octave == s->key + sc->iv[i])
            out->in_scale = 1;
}

uint64_t jln_pads_with_note(const struct jln *s, const struct jln_scale *sc, int note)
{
    uint64_t mask = 0;
    if (note < 0 || note > 127)
        return 0;
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++) {
            struct jln_pad p;
            jln_pad(s, sc, x, y, &p);
            if (p.note == note)
                mask |= 1ull << (y * 8u + x);
        }
    return mask;
}

uint8_t jln_color(const struct jln_pad *p, int playing)
{
    if (p->note < 0)
        return 0;
    if (playing)
        return JLN_COLOR_PLAYING;
    if (p->root)
        return JLN_COLOR_ROOT;
    return p->in_scale ? JLN_COLOR_IN_SCALE : 0;
}
