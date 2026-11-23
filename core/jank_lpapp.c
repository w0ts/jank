/* SPDX-License-Identifier: GPL-3.0-only */
/* Launchpad X -> Digitone II, Note mode (jank_lpapp.h). */
#include "jank_lpapp.h"
#include <string.h>

#define CIN_NOTE_OFF 0x8u
#define CIN_NOTE_ON 0x9u
#define CIN_CC 0xBu
#define CC_ALL_NOTES_OFF 123u
#define BUTTON_UP 0u                   /* the top buttons, left to right */
#define BUTTON_DOWN 1u
#define BUTTON_LEFT 2u
#define BUTTON_RIGHT 3u
#define BUTTON_NOTE 5u

enum jla_role jla_role_from_name(const char *product)
{
    if (!product)
        return JLA_OTHER;
    if (strstr(product, "Launchpad X"))
        return JLA_LPX;
    if (strstr(product, "Digitone"))
        return JLA_DN2;
    return JLA_OTHER;
}

unsigned jla_midi_interface_index(enum jla_role r) { return r == JLA_LPX ? 1u : 0u; }

void jla_init(struct jla *a, const struct jln_scale *scale, uint8_t dn2_cable)
{
    memset(a, 0, sizeof *a);
    jln_init(&a->n);
    a->scale = scale;
    a->dn2_cable = dn2_cable;
    for (uint32_t t = 0; t < JLN_TRACKS; t++)
        a->chan[t] = (uint8_t)t;
    for (uint32_t i = 0; i < JLA_PADS; i++)
        a->held[i].note = -1;
}

static uint8_t *put(const struct jla *a, uint8_t *out, uint8_t cin, uint8_t chan, uint8_t note, uint8_t vel)
{
    *out++ = (uint8_t)((a->dn2_cable << 4) | cin);
    *out++ = (uint8_t)((cin << 4) | chan);
    *out++ = note;
    *out++ = vel;
    return out;
}

/* another pad that is down holds the same note on the same channel */
static int held_elsewhere(const struct jla *a, uint32_t pad, int16_t note, uint8_t chan)
{
    for (uint32_t i = 0; i < JLA_PADS; i++)
        if (i != pad && a->held[i].note == note && a->held[i].chan == chan)
            return 1;
    return 0;
}

static size_t pad_event(struct jla *a, const struct jlx_ev *ev, uint8_t *out, size_t max_packets)
{
    uint32_t i = ev->row * 8u + ev->col;
    struct jla_held *h = &a->held[i];
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

static void button_event(struct jla *a, const struct jlx_ev *ev)
{
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

size_t jla_on_lpx(struct jla *a, const struct jlx_ev *ev, uint8_t *out, size_t max_packets)
{
    if (ev->kind == JLX_PAD && ev->row < 8u && ev->col < 8u)
        return pad_event(a, ev, out, max_packets);
    if (ev->kind == JLX_TOP)
        button_event(a, ev);
    return 0;
}

void jla_on_dn2(struct jla *a, const uint8_t pkt[4])
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

size_t jla_release_all(struct jla *a, uint8_t *out, size_t max_packets)
{
    size_t n = 0;
    for (uint32_t i = 0; i < JLA_PADS && n < max_packets; i++) {
        int16_t note = a->held[i].note;
        uint8_t chan = a->held[i].chan;
        if (note < 0)
            continue;
        for (uint32_t j = i; j < JLA_PADS; j++)          /* one Note Off for a note that several pads hold */
            if (a->held[j].note == note && a->held[j].chan == chan)
                a->held[j].note = -1;
        out = put(a, out, CIN_NOTE_OFF, chan, (uint8_t)note, 0);
        n++;
    }
    return n;
}

static int bit(const uint8_t *set, int note) { return (set[note >> 3] >> (note & 7)) & 1; }

void jla_frame(const struct jla *a, struct jlx_frame *f)
{
    uint8_t chan = a->chan[a->n.track], own[16];
    memset(f, 0, sizeof *f);
    memset(own, 0, sizeof own);
    for (uint32_t i = 0; i < JLA_PADS; i++)
        if (a->held[i].note >= 0 && a->held[i].chan == chan)
            own[a->held[i].note >> 3] |= (uint8_t)(1u << (a->held[i].note & 7));
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++) {
            struct jln_pad p;
            int playing;
            jln_pad(&a->n, a->scale, x, y, &p);
            playing = p.note >= 0 && (bit(a->playing[chan], p.note) || bit(own, p.note));
            f->led[JLX_PAD0 + y * 8u + x].color = jln_color(&p, playing);
        }
    for (uint32_t b = BUTTON_UP; b <= BUTTON_RIGHT; b++)
        f->led[JLX_TOP0 + b].color = JLA_COLOR_ARROW;
    f->led[JLX_TOP0 + BUTTON_NOTE].color = JLA_COLOR_MODE;
}
