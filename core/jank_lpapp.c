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

void jlapp_tick(struct jlapp *a, uint32_t now_ms) { a->now_ms = now_ms; }

void jlapp_set_live(struct jlapp *a, int push, int can_play)
{
    a->push = push != 0;
    a->can_play = can_play != 0;
}

size_t jlapp_act_midi(const struct jlapp *a, const struct jlapp_act *act, uint8_t out[4])
{
    uint8_t cin;
    if (act->kind == JLAPP_ACT_NOTE_ON)
        cin = CIN_NOTE_ON;
    else if (act->kind == JLAPP_ACT_NOTE_OFF)
        cin = CIN_NOTE_OFF;
    else
        return 0;
    out[0] = (uint8_t)((a->dn2_cable << 4) | cin);
    out[1] = (uint8_t)((cin << 4) | act->chan);
    out[2] = act->note;
    out[3] = act->kind == JLAPP_ACT_NOTE_ON ? act->vel : 0;
    return 1;
}

static void set_act(struct jlapp_act *o, uint8_t kind, uint8_t track, uint8_t chan, uint8_t live, uint8_t note, uint8_t vel)
{
    o->kind = kind;
    o->track = track;
    o->chan = chan;
    o->live = live;
    o->note = note;
    o->vel = vel;
}

/* another pad that is down holds the same note on the same channel (and went the same way) */
static int held_elsewhere(const struct jlapp *a, uint32_t pad, const struct jlapp_held *h)
{
    for (uint32_t i = 0; i < JLAPP_PADS; i++)
        if (i != pad && a->held[i].note == h->note && a->held[i].chan == h->chan && a->held[i].live == h->live)
            return 1;
    return 0;
}

/* the rows the track pads take from the bottom of the layout */
static uint8_t note_shift(const struct jlapp *a) { return a->track_pads ? (uint8_t)JLAPP_TRACK_ROWS : 0u; }

static size_t pad_event(struct jlapp *a, const struct jlx_ev *ev, struct jlapp_act *out, size_t max)
{
    uint32_t i = ev->row * 8u + ev->col;
    struct jlapp_held *h = &a->held[i];
    if (ev->value) {
        struct jln_pad p;
        if (h->note >= 0 || max < 1 || ev->row < note_shift(a))
            return 0;
        jln_pad(&a->n, a->scale, ev->col, (uint8_t)(ev->row - note_shift(a)), &p);
        if (p.note < 0)
            return 0;
        h->note = p.note;
        h->track = a->n.track;
        h->chan = a->chan[a->n.track];
        h->live = a->can_play;
        set_act(out, JLAPP_ACT_NOTE_ON, h->track, h->chan, h->live, (uint8_t)p.note, ev->value);
        return 1;
    }
    if (h->note < 0)
        return 0;
    {
        struct jlapp_held was = *h;
        h->note = -1;
        if (held_elsewhere(a, i, &was) || max < 1)
            return 0;
        set_act(out, JLAPP_ACT_NOTE_OFF, was.track, was.chan, was.live, (uint8_t)was.note, 0);
        return 1;
    }
}

/* a pad of the two bottom rows while the track pads are on: it plays that track's own note */
static size_t track_pad_event(struct jlapp *a, const struct jlx_ev *ev, struct jlapp_act *out, size_t max)
{
    uint8_t t = (uint8_t)(ev->row * 8u + ev->col);
    uint16_t bitv = (uint16_t)(1u << t);
    if (ev->value) {
        if ((a->track_held & bitv) || max < 1 || !a->can_play)   /* without PLAY there is no way to say "that track's note": nothing */
            return 0;
        a->track_held |= bitv;
        set_act(out, JLAPP_ACT_TRACK_ON, t, 0, 1, 0, 0);
        return 1;
    }
    if (!(a->track_held & bitv))
        return 0;
    a->track_held &= (uint16_t)~bitv;
    if (max < 1)
        return 0;
    set_act(out, JLAPP_ACT_TRACK_OFF, t, 0, 1, 0, 0);
    return 1;
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
        else if (x == 7)
            a->track_pads ^= 1u;
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

static uint8_t on_off(int on, uint8_t on_colour, uint8_t off_colour) { return on ? on_colour : off_colour; }

/* the colour of the scale page's pad (x, y), 0 for none: Launchpad 95's (SkinMK2.py Scale.*; see jank_lpapp.h) */
static uint8_t page_colour(const struct jlapp *a, uint8_t x, uint8_t y)
{
    const struct jln *n = &a->n;
    uint8_t r = (uint8_t)PAGE_ROW(y);
    if (r == 0) {
        if (x == 0)                    /* AbsoluteRoot */
            return on_off(n->absolute, JLAPP_RED, JLAPP_RED_THIRD);
        if (x == 1)                    /* Horizontal (LP95 shows it in every layout, though only the diatonic one takes it) */
            return on_off(n->horizontal, JLAPP_GREEN, JLAPP_GREEN_THIRD);
        if (x == 4)                    /* Mode: the layouts */
            return on_off(n->layout == JLN_CHORDS, JLAPP_RED, JLAPP_RED_THIRD);
        if (x == 5)
            return on_off(n->layout == JLN_DIATONIC, JLAPP_RED, JLAPP_RED_THIRD);
        if (x == 6)
            return on_off(n->layout == JLN_CHROMATIC, JLAPP_RED, JLAPP_RED_THIRD);
        if (x == 7)                    /* the track pads (not Launchpad 95's: its drum rack pad is here) */
            return on_off(a->track_pads, JLAPP_GREEN, JLAPP_GREEN_THIRD);
        return 0;
    }
    if (r == 1) {
        if (is_black_key_pad(x))       /* Key */
            return on_off(n->key == WHITE_KEY[x] + 1u, JLAPP_GREEN, JLAPP_GREEN_THIRD);
        return x == 2 || x == 6 ? JLAPP_BLUE : 0;      /* RelativeScale, CircleOfFifths */
    }
    if (r == 2)
        return x < 7u ? on_off(n->key == WHITE_KEY[x], JLAPP_GREEN, JLAPP_GREEN_THIRD) : JLAPP_BLUE;
    if (r == 3)                        /* Octave */
        return x < jln_octaves(n) ? on_off(n->octave == x, JLAPP_RED, JLAPP_RED_THIRD) : 0;
    {
        unsigned idx = (unsigned)(r - FIRST_SCALE_ROW) * 8u + x;   /* Modus: the scales */
        return idx < jls_count() ? on_off((int)idx == jls_index_of(a->scale), JLAPP_BLUE, JLAPP_BLUE_THIRD) : 0;
    }
}

/* left / right: the previous / next track, shown on the bottom pads for a moment; a track that changed is the Digitone's active one too */
static size_t track_button(struct jlapp *a, int dir, struct jlapp_act *out, size_t max)
{
    uint8_t before = a->n.track;
    jln_track_step(&a->n, dir);
    a->select_on = 1;
    a->select_ms = a->now_ms;
    if (a->n.track == before || max < 1)
        return 0;
    set_act(out, JLAPP_ACT_SELECT, a->n.track, a->chan[a->n.track], 1, 0, 0);
    return 1;
}

static size_t button_event(struct jlapp *a, const struct jlx_ev *ev, struct jlapp_act *out, size_t max)
{
    if (ev->idx == BUTTON_NOTE) {      /* held: the scale page; let go: the notes again */
        a->page = ev->value != 0;
        return 0;
    }
    if (!ev->value)
        return 0;
    switch (ev->idx) {
    case BUTTON_UP: jln_octave_step(&a->n, 1); break;
    case BUTTON_DOWN: jln_octave_step(&a->n, -1); break;
    case BUTTON_LEFT: return track_button(a, -1, out, max);
    case BUTTON_RIGHT: return track_button(a, 1, out, max);
    default: break;                    /* the rest has no job in Note mode yet */
    }
    return 0;
}

size_t jlapp_on_lpx_acts(struct jlapp *a, const struct jlx_ev *ev, struct jlapp_act *out, size_t max)
{
    if (ev->kind == JLX_PAD && ev->row < 8u && ev->col < 8u) {
        int track_pad = a->track_pads && ev->row < JLAPP_TRACK_ROWS;
        if (a->page && ev->value) {    /* a press on the scale page edits it; a release still ends a note that was down before the page opened */
            page_pad(a, ev->col, ev->row);
            return 0;
        }
        if (track_pad && (ev->value || (a->track_held & (1u << (ev->row * 8u + ev->col)))))
            return track_pad_event(a, ev, out, max);
        return pad_event(a, ev, out, max);
    }
    if (ev->kind == JLX_TOP)
        return button_event(a, ev, out, max);
    return 0;
}

size_t jlapp_on_lpx(struct jlapp *a, const struct jlx_ev *ev, uint8_t *out, size_t max_packets)
{
    struct jlapp_act act;
    if (jlapp_on_lpx_acts(a, ev, &act, max_packets ? 1u : 0u) < 1)
        return 0;
    return jlapp_act_midi(a, &act, out);
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

void jlapp_on_push(struct jlapp *a, const struct jdl_note *notes, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        struct jlapp_recent *r = &a->recent[a->recent_head];
        if (notes[i].track >= JLN_TRACKS || notes[i].note > 127u)
            continue;
        r->at_ms = a->now_ms;
        r->track = notes[i].track;
        r->note = notes[i].note;
        r->used = 1;
        a->recent_head = (uint8_t)((a->recent_head + 1u) % JLAPP_RECENT);
    }
}

size_t jlapp_release_acts(struct jlapp *a, struct jlapp_act *out, size_t max)
{
    size_t n = 0;
    for (uint32_t i = 0; i < JLAPP_PADS && n < max; i++) {
        struct jlapp_held was = a->held[i];
        if (was.note < 0)
            continue;
        for (uint32_t j = i; j < JLAPP_PADS; j++)          /* one end for a note that several pads hold */
            if (a->held[j].note == was.note && a->held[j].chan == was.chan && a->held[j].live == was.live)
                a->held[j].note = -1;
        set_act(&out[n++], JLAPP_ACT_NOTE_OFF, was.track, was.chan, was.live, (uint8_t)was.note, 0);
    }
    for (uint8_t t = 0; t < JLN_TRACKS && n < max; t++)
        if (a->track_held & (1u << t)) {
            a->track_held &= (uint16_t)~(1u << t);
            set_act(&out[n++], JLAPP_ACT_TRACK_OFF, t, 0, 1, 0, 0);
        }
    return n;
}

size_t jlapp_release_all(struct jlapp *a, uint8_t *out, size_t max_packets)
{
    struct jlapp_act acts[JLAPP_PADS + JLN_TRACKS];
    size_t n = jlapp_release_acts(a, acts, max_packets < JLAPP_PADS + JLN_TRACKS ? max_packets : JLAPP_PADS + JLN_TRACKS), k = 0;
    for (size_t i = 0; i < n; i++)
        k += jlapp_act_midi(a, &acts[i], out + 4u * k);
    return k;
}

static int bit(const uint8_t *set, int note) { return (set[note >> 3] >> (note & 7)) & 1; }

/* a pushed note sounded in the last JLAPP_PUSH_LIGHT_MS on this track (and with this pitch, unless note < 0) */
static int pushed(const struct jlapp *a, uint8_t track, int note)
{
    for (uint32_t i = 0; i < JLAPP_RECENT; i++) {
        const struct jlapp_recent *r = &a->recent[i];
        if (r->used && r->track == track && (note < 0 || r->note == note) && (uint32_t)(a->now_ms - r->at_ms) < JLAPP_PUSH_LIGHT_MS)
            return 1;
    }
    return 0;
}

static int select_shown(const struct jlapp *a) { return a->select_on && (uint32_t)(a->now_ms - a->select_ms) < JLAPP_SELECT_MS; }

/* a track pad's colour: the chosen track white for a moment, a track that sounds (or whose pad is down) green, the current one blue, the rest dim */
static uint8_t track_colour(const struct jlapp *a, uint8_t t, int chosen_shown)
{
    int is_current = t == a->n.track;
    if (is_current && chosen_shown)
        return JLAPP_COLOR_TRACK_CHOSEN;
    if ((a->track_held & (1u << t)) || (a->push && pushed(a, t, -1)))
        return JLN_COLOR_PLAYING;
    return is_current ? JLAPP_COLOR_TRACK_CURRENT : JLAPP_COLOR_TRACK_IDLE;
}

void jlapp_frame(const struct jlapp *a, struct jlx_frame *f)
{
    uint8_t chan = a->chan[a->n.track], own[16], shift = note_shift(a);
    int chosen_shown = select_shown(a);
    memset(f, 0, sizeof *f);
    memset(own, 0, sizeof own);
    for (uint32_t i = 0; i < JLAPP_PADS; i++) {
        const struct jlapp_held *h = &a->held[i];
        if (h->note >= 0 && (h->live ? h->track == a->n.track : h->chan == chan))
            own[h->note >> 3] |= (uint8_t)(1u << (h->note & 7));
    }
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++) {
            struct jln_pad p;
            int playing;
            if (a->page) {
                f->led[JLX_PAD0 + y * 8u + x].color = page_colour(a, x, y);
                continue;
            }
            if (y < JLAPP_TRACK_ROWS && (shift || chosen_shown)) {
                f->led[JLX_PAD0 + y * 8u + x].color = track_colour(a, (uint8_t)(y * 8u + x), chosen_shown);
                continue;
            }
            jln_pad(&a->n, a->scale, x, (uint8_t)(y - shift), &p);
            playing = p.note >= 0 && (bit(own, p.note) || (a->push ? pushed(a, a->n.track, p.note) : bit(a->playing[chan], p.note)));
            f->led[JLX_PAD0 + y * 8u + x].color = jln_color(&p, playing);
        }
    /* the arrows are green, and dim at the end of what they do (Launchpad 95's Note.Octave): the octave's limits, the first and last track */
    f->led[JLX_TOP0 + BUTTON_UP].color = a->n.octave + 1u < jln_octaves(&a->n) ? JLAPP_COLOR_ARROW_ON : JLAPP_COLOR_ARROW_OFF;
    f->led[JLX_TOP0 + BUTTON_DOWN].color = a->n.octave > 0u ? JLAPP_COLOR_ARROW_ON : JLAPP_COLOR_ARROW_OFF;
    f->led[JLX_TOP0 + BUTTON_LEFT].color = a->n.track > 0u ? JLAPP_COLOR_ARROW_ON : JLAPP_COLOR_ARROW_OFF;
    f->led[JLX_TOP0 + BUTTON_RIGHT].color = a->n.track + 1u < JLN_TRACKS ? JLAPP_COLOR_ARROW_ON : JLAPP_COLOR_ARROW_OFF;
    f->led[JLX_TOP0 + BUTTON_NOTE].color = JLAPP_COLOR_MODE;
}
