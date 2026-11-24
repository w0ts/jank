/* SPDX-License-Identifier: GPL-3.0-only */
/* Launchpad X -> Digitone II, Note mode (jank_lpapp.h). */
#include "jank_lpapp.h"
#include <string.h>
#include "jank_lpscales.h"

#define CIN_NOTE_OFF 0x8u
#define CIN_NOTE_ON 0x9u
#define CIN_CC 0xBu
#define CC_ALL_NOTES_OFF 123u
#define BUTTON_UP 0u                   /* the top buttons, left to right */
#define BUTTON_DOWN 1u
#define BUTTON_LEFT 2u
#define BUTTON_RIGHT 3u
#define BUTTON_NOTE 5u

enum jlapp_role jlapp_role_from_name(const char *product)
{
    if (!product)
        return JLAPP_OTHER;
    if (strstr(product, "Launchpad X"))
        return JLAPP_LPX;
    if (strstr(product, "Digitone"))
        return JLAPP_DN2;
    return JLAPP_OTHER;
}

unsigned jlapp_midi_interface_index(enum jlapp_role r) { return r == JLAPP_LPX ? 1u : 0u; }

void jlapp_init(struct jlapp *a, const struct jln_scale *scale, uint8_t dn2_cable)
{
    memset(a, 0, sizeof *a);
    jln_init(&a->n);
    a->scale = scale;
    a->minor_idx = JLS_MINOR;
    a->dn2_cable = dn2_cable;
    for (uint32_t t = 0; t < JLN_TRACKS; t++)
        a->chan[t] = (uint8_t)t;
    for (uint32_t i = 0; i < JLAPP_PADS; i++)
        a->held[i].note = -1;
}

static uint8_t *put(const struct jlapp *a, uint8_t *out, uint8_t cin, uint8_t chan, uint8_t note, uint8_t vel)
{
    *out++ = (uint8_t)((a->dn2_cable << 4) | cin);
    *out++ = (uint8_t)((cin << 4) | chan);
    *out++ = note;
    *out++ = vel;
    return out;
}

/* another pad that is down holds the same note on the same channel */
static int held_elsewhere(const struct jlapp *a, uint32_t pad, int16_t note, uint8_t chan)
{
    for (uint32_t i = 0; i < JLAPP_PADS; i++)
        if (i != pad && a->held[i].note == note && a->held[i].chan == chan)
            return 1;
    return 0;
}

static size_t pad_event(struct jlapp *a, const struct jlx_ev *ev, uint8_t *out, size_t max_packets)
{
    uint32_t i = ev->row * 8u + ev->col;
    struct jlapp_held *h = &a->held[i];
    if (ev->value) {
        struct jln_pad p;
        if (h->note >= 0 || max_packets < 1)
            return 0;
        jln_pad(&a->n, a->scale, ev->col, ev->row, &p);
        if (p.note < 0)
            return 0;
        h->note = p.note;
        h->chan = a->chan[a->n.track];
        put(a, out, CIN_NOTE_ON, h->chan, (uint8_t)p.note, ev->value);
        return 1;
    }
    if (h->note < 0)
        return 0;
    {
        int16_t note = h->note;
        uint8_t chan = h->chan;
        h->note = -1;
        if (held_elsewhere(a, i, note, chan) || max_packets < 1)
            return 0;
        put(a, out, CIN_NOTE_OFF, chan, (uint8_t)note, 0);
        return 1;
    }
}

/* the scale page (jank_lpapp.h): rows are counted from the top, as Launchpad 95 does, row 0 being the top one (pad row 7) */
static const uint8_t WHITE_KEY[7] = {0, 2, 4, 5, 7, 9, 11};      /* C D E F G A B */
#define PAGE_ROW(y) (7u - (y))
#define FIRST_SCALE_ROW 4u
#define FIFTH_UP 7u                    /* the circle of fifths: a fifth up is 7 semitones, a fifth down 5 */
#define FIFTH_DOWN 5u
#define MINOR_THIRD_UP 3u
#define MAJOR_SIXTH_UP 9u              /* the relative major of a minor key is a minor third up, its relative minor a minor third down (9 up) */

static int is_black_key_pad(uint8_t x) { return x == 0 || x == 1 || x == 3 || x == 4 || x == 5; }

static void set_scale(struct jlapp *a, unsigned idx)
{
    const struct jln_scale *s = jls_get(idx);
    if (s)
        a->scale = s;
}

/* major <-> the minor last used, minor pentatonic <-> major pentatonic; another scale has no relative one */
static void relative_scale(struct jlapp *a)
{
    int idx = jls_index_of(a->scale);
    struct jln *n = &a->n;
    if (idx == (int)JLS_MAJOR) {
        set_scale(a, a->minor_idx);
        n->key = (uint8_t)((n->key + MAJOR_SIXTH_UP) % 12u);
    } else if (idx == (int)JLS_MINOR || idx == (int)JLS_HARMONIC_MINOR || idx == (int)JLS_MELODIC_MINOR) {
        a->minor_idx = (uint8_t)idx;
        set_scale(a, JLS_MAJOR);
        n->key = (uint8_t)((n->key + MINOR_THIRD_UP) % 12u);
    } else if (idx == (int)JLS_MINOR_PENTATONIC) {
        set_scale(a, JLS_MAJOR_PENTATONIC);
        n->key = (uint8_t)((n->key + MINOR_THIRD_UP) % 12u);
    } else if (idx == (int)JLS_MAJOR_PENTATONIC) {
        set_scale(a, JLS_MINOR_PENTATONIC);
        n->key = (uint8_t)((n->key + MAJOR_SIXTH_UP) % 12u);
    }
}

static void page_pad(struct jlapp *a, uint8_t x, uint8_t y)
{
    struct jln *n = &a->n;
    uint8_t r = (uint8_t)PAGE_ROW(y);
    if (r == 0) {
        if (x == 0)
            n->absolute ^= 1u;
        else if (x == 1 && n->layout == JLN_DIATONIC)
            n->horizontal ^= 1u;
        else if (x == 4)
            jln_set_layout(n, JLN_CHORDS);
        else if (x == 5)
            jln_set_layout(n, JLN_DIATONIC);
        else if (x == 6)
            jln_set_layout(n, JLN_CHROMATIC);
    } else if (r == 1) {
        if (is_black_key_pad(x))
            n->key = (uint8_t)(WHITE_KEY[x] + 1u);
        else if (x == 2)
            relative_scale(a);
        else if (x == 6)
            n->key = (uint8_t)((n->key + FIFTH_DOWN) % 12u);
    } else if (r == 2) {
        if (x < 7u)
            n->key = WHITE_KEY[x];
        else
            n->key = (uint8_t)((n->key + FIFTH_UP) % 12u);
    } else if (r == 3) {
        jln_set_octave(n, x);
    } else {
        set_scale(a, (unsigned)(r - FIRST_SCALE_ROW) * 8u + x);
    }
}

static uint8_t on_off(int on, uint8_t off_colour) { return on ? JLAPP_PAGE_ON : off_colour; }

/* the colour of the scale page's pad (x, y), 0 for none */
static uint8_t page_colour(const struct jlapp *a, uint8_t x, uint8_t y)
{
    const struct jln *n = &a->n;
    uint8_t r = (uint8_t)PAGE_ROW(y);
    if (r == 0) {
        if (x == 0)
            return on_off(n->absolute, JLAPP_PAGE_LAYOUT);
        if (x == 1)
            return n->layout == JLN_DIATONIC ? on_off(n->horizontal, JLAPP_PAGE_LAYOUT) : 0;
        if (x == 4)
            return on_off(n->layout == JLN_CHORDS, JLAPP_PAGE_LAYOUT);
        if (x == 5)
            return on_off(n->layout == JLN_DIATONIC, JLAPP_PAGE_LAYOUT);
        if (x == 6)
            return on_off(n->layout == JLN_CHROMATIC, JLAPP_PAGE_LAYOUT);
        return 0;
    }
    if (r == 1) {
        if (is_black_key_pad(x))
            return on_off(n->key == WHITE_KEY[x] + 1u, JLAPP_PAGE_BLACK_KEY);
        return x == 2 || x == 6 ? JLAPP_PAGE_SHIFT : 0;
    }
    if (r == 2)
        return x < 7u ? on_off(n->key == WHITE_KEY[x], JLAPP_PAGE_WHITE_KEY) : JLAPP_PAGE_SHIFT;
    if (r == 3)
        return x < jln_octaves(n) ? on_off(n->octave == x, JLAPP_PAGE_OCTAVE) : 0;
    {
        unsigned idx = (unsigned)(r - FIRST_SCALE_ROW) * 8u + x;
        return idx < jls_count() ? on_off((int)idx == jls_index_of(a->scale), JLAPP_PAGE_SCALE) : 0;
    }
}

static void button_event(struct jlapp *a, const struct jlx_ev *ev)
{
    if (ev->idx == BUTTON_NOTE) {      /* held: the scale page; let go: the notes again */
        a->page = ev->value != 0;
        return;
    }
    if (!ev->value)
        return;
    switch (ev->idx) {
    case BUTTON_UP: jln_octave_step(&a->n, 1); break;
    case BUTTON_DOWN: jln_octave_step(&a->n, -1); break;
    case BUTTON_LEFT: jln_track_step(&a->n, -1); break;
    case BUTTON_RIGHT: jln_track_step(&a->n, 1); break;
    default: break;                    /* the rest has no job in Note mode yet */
    }
}

size_t jlapp_on_lpx(struct jlapp *a, const struct jlx_ev *ev, uint8_t *out, size_t max_packets)
{
    if (ev->kind == JLX_PAD && ev->row < 8u && ev->col < 8u) {
        if (a->page && ev->value) {    /* a press on the scale page edits it; a release still ends a note that was down before the page opened */
            page_pad(a, ev->col, ev->row);
            return 0;
        }
        return pad_event(a, ev, out, max_packets);
    }
    if (ev->kind == JLX_TOP)
        button_event(a, ev);
    return 0;
}

void jlapp_on_dn2(struct jlapp *a, const uint8_t pkt[4])
{
    uint8_t cin = pkt[0] & 0x0Fu, chan = pkt[1] & 0x0Fu, note = pkt[2] & 0x7Fu;
    if ((pkt[1] >> 4) != cin)          /* not a channel message of that kind */
        return;
    if (cin == CIN_NOTE_ON && pkt[3])
        a->playing[chan][note >> 3] |= (uint8_t)(1u << (note & 7u));
    else if (cin == CIN_NOTE_ON || cin == CIN_NOTE_OFF)
        a->playing[chan][note >> 3] &= (uint8_t)~(1u << (note & 7u));
    else if (cin == CIN_CC && note == CC_ALL_NOTES_OFF)
        memset(a->playing[chan], 0, sizeof a->playing[chan]);
}

size_t jlapp_release_all(struct jlapp *a, uint8_t *out, size_t max_packets)
{
    size_t n = 0;
    for (uint32_t i = 0; i < JLAPP_PADS && n < max_packets; i++) {
        int16_t note = a->held[i].note;
        uint8_t chan = a->held[i].chan;
        if (note < 0)
            continue;
        for (uint32_t j = i; j < JLAPP_PADS; j++)          /* one Note Off for a note that several pads hold */
            if (a->held[j].note == note && a->held[j].chan == chan)
                a->held[j].note = -1;
        out = put(a, out, CIN_NOTE_OFF, chan, (uint8_t)note, 0);
        n++;
    }
    return n;
}

static int bit(const uint8_t *set, int note) { return (set[note >> 3] >> (note & 7)) & 1; }

void jlapp_frame(const struct jlapp *a, struct jlx_frame *f)
{
    uint8_t chan = a->chan[a->n.track], own[16];
    memset(f, 0, sizeof *f);
    memset(own, 0, sizeof own);
    for (uint32_t i = 0; i < JLAPP_PADS; i++)
        if (a->held[i].note >= 0 && a->held[i].chan == chan)
            own[a->held[i].note >> 3] |= (uint8_t)(1u << (a->held[i].note & 7));
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++) {
            struct jln_pad p;
            int playing;
            if (a->page) {
                f->led[JLX_PAD0 + y * 8u + x].color = page_colour(a, x, y);
                continue;
            }
            jln_pad(&a->n, a->scale, x, y, &p);
            playing = p.note >= 0 && (bit(a->playing[chan], p.note) || bit(own, p.note));
            f->led[JLX_PAD0 + y * 8u + x].color = jln_color(&p, playing);
        }
    for (uint32_t b = BUTTON_UP; b <= BUTTON_RIGHT; b++)
        f->led[JLX_TOP0 + b].color = JLAPP_COLOR_ARROW;
    f->led[JLX_TOP0 + BUTTON_NOTE].color = JLAPP_COLOR_MODE;
}
