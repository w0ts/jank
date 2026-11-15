/* SPDX-License-Identifier: GPL-3.0-only */
/* beat_latency per audio source (jank_lat.h). */
#include "jank_lat.h"
#include <stdio.h>
#include <string.h>

#define VERSION 1u
#define TRIM_VERSION 1u

static int clamp_ms(int ms) { return ms < JLA_MIN_MS ? JLA_MIN_MS : ms > JLA_MAX_MS ? JLA_MAX_MS : ms; }

void jla_init(struct jla *t)
{
    memset(t, 0, sizeof *t);
    t->computer_ms = JLA_COMPUTER_DEFAULT_MS;
}

static int find(const struct jla *t, uint16_t vid, uint16_t pid)
{
    for (uint32_t i = 0; i < t->ndev && i < JLA_DEVS; i++)
        if (t->dev[i].vid == vid && t->dev[i].pid == pid)
            return (int)i;
    return -1;
}

int16_t jla_base(const struct jla *t, const struct jla_key *k)
{
    int i;
    if (k->kind == JLA_LINE)
        return (int16_t)clamp_ms(JLA_LINE_PATH_MS + t->line_base_ms);
    if (k->kind != JLA_HOSTED)
        return t->computer_ms;
    i = find(t, k->vid, k->pid);
    return i < 0 ? (int16_t)JLA_HOSTED_DEFAULT_MS : t->dev[i].ms;
}

int16_t jla_trim(const struct jla *t, const struct jla_key *k)
{
    int i;
    if (k->kind == JLA_LINE)
        return t->line_trim;
    if (k->kind != JLA_HOSTED)
        return t->computer_trim;
    i = find(t, k->vid, k->pid);
    return i < 0 ? 0 : t->dev[i].trim;
}

int16_t jla_get(const struct jla *t, const struct jla_key *k) { return (int16_t)clamp_ms(jla_base(t, k) + jla_trim(t, k)); }

/* a device's entry, first in the table (added with the default base when new: the oldest leaves when full) */
static struct jla_dev *dev_first(struct jla *t, const struct jla_key *k)
{
    int i = find(t, k->vid, k->pid);
    struct jla_dev d = {k->vid, k->pid, (int16_t)JLA_HOSTED_DEFAULT_MS, 0};
    if (i < 0)
        i = (int)(t->ndev < JLA_DEVS ? t->ndev++ : JLA_DEVS - 1u);
    else
        d = t->dev[i];
    memmove(&t->dev[1], &t->dev[0], (size_t)i * sizeof d);
    t->dev[0] = d;
    return &t->dev[0];
}

void jla_set(struct jla *t, const struct jla_key *k, int ms)
{
    ms = clamp_ms(ms);
    if (k->kind == JLA_LINE)
        t->line_base_ms = (int16_t)(ms - JLA_LINE_PATH_MS);
    else if (k->kind != JLA_HOSTED)
        t->computer_ms = (int16_t)ms;
    else
        dev_first(t, k)->ms = (int16_t)ms;
}

void jla_set_trim(struct jla *t, const struct jla_key *k, int ms)
{
    int base = jla_base(t, k);
    ms = ms < JLA_MIN_MS - base ? JLA_MIN_MS - base : ms > JLA_MAX_MS - base ? JLA_MAX_MS - base : ms;
    if (k->kind == JLA_LINE)
        t->line_trim = (int16_t)ms;
    else if (k->kind != JLA_HOSTED)
        t->computer_trim = (int16_t)ms;
    else
        dev_first(t, k)->trim = (int16_t)ms;
}

int jla_same_key(const struct jla_key *a, const struct jla_key *b)
{
    return a->kind == b->kind && (a->kind != JLA_HOSTED || (a->vid == b->vid && a->pid == b->pid));
}

int jla_same(const struct jla *a, const struct jla *b)
{
    if (a->computer_ms != b->computer_ms || a->line_base_ms != b->line_base_ms || a->ndev != b->ndev
        || a->computer_trim != b->computer_trim || a->line_trim != b->line_trim)
        return 0;
    for (uint32_t i = 0; i < a->ndev && i < JLA_DEVS; i++)
        if (a->dev[i].vid != b->dev[i].vid || a->dev[i].pid != b->dev[i].pid || a->dev[i].ms != b->dev[i].ms
            || a->dev[i].trim != b->dev[i].trim)
            return 0;
    return 1;
}

const char *jla_name(const struct jla_key *k, char *out)
{
    if (k->kind == JLA_HOSTED)
        snprintf(out, 16, "host:%04x:%04x", k->vid, k->pid);
    else
        strcpy(out, k->kind == JLA_LINE ? "line" : "usb");
    return out;
}

static uint8_t *put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 255u);
    p[1] = (uint8_t)(v >> 8);
    return p + 2;
}

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)(p[1] << 8)); }

size_t jla_pack(const struct jla *t, uint8_t *out, size_t cap)
{
    uint8_t *p = out;
    if (cap < JLA_PACKED)
        return 0;
    memset(out, 0, JLA_PACKED);
    *p++ = VERSION;
    p = put16(p, (uint16_t)t->computer_ms);
    p = put16(p, (uint16_t)t->line_base_ms);
    *p++ = t->ndev;
    for (uint32_t i = 0; i < JLA_DEVS; i++) {
        p = put16(p, t->dev[i].vid);
        p = put16(p, t->dev[i].pid);
        p = put16(p, (uint16_t)t->dev[i].ms);
    }
    return JLA_PACKED;
}

static int in_range(int v) { return v >= JLA_MIN_MS && v <= JLA_MAX_MS; }

int jla_unpack(struct jla *t, const uint8_t *in, size_t n)
{
    struct jla r;
    const uint8_t *p = in + 6;
    if (n != JLA_PACKED || in[0] != VERSION || in[5] > JLA_DEVS)
        return -1;
    jla_init(&r);
    r.computer_ms = (int16_t)get16(in + 1);
    r.line_base_ms = (int16_t)get16(in + 3);
    r.ndev = in[5];
    if (!in_range(r.computer_ms) || !in_range(JLA_LINE_PATH_MS + r.line_base_ms))
        return -1;
    for (uint32_t i = 0; i < r.ndev; i++, p += 6) {
        r.dev[i] = (struct jla_dev){get16(p), get16(p + 2), (int16_t)get16(p + 4), 0};
        if (!in_range(r.dev[i].ms))
            return -1;
    }
    *t = r;
    return 0;
}

size_t jla_pack_trim(const struct jla *t, uint8_t *out, size_t cap)
{
    uint8_t *p = out;
    if (cap < JLA_TRIM_PACKED)
        return 0;
    memset(out, 0, JLA_TRIM_PACKED);
    *p++ = TRIM_VERSION;
    p = put16(p, (uint16_t)t->computer_trim);
    p = put16(p, (uint16_t)t->line_trim);
    for (uint32_t i = 0; i < t->ndev && i < JLA_DEVS; i++) {
        p = put16(p, t->dev[i].vid);
        p = put16(p, t->dev[i].pid);
        p = put16(p, (uint16_t)t->dev[i].trim);
    }
    return JLA_TRIM_PACKED;
}

static int trim_ok(int v) { return v >= JLA_MIN_MS - JLA_MAX_MS && v <= JLA_MAX_MS - JLA_MIN_MS; }

int jla_unpack_trim(struct jla *t, const uint8_t *in, size_t n)
{
    struct jla r = *t;
    const uint8_t *p = in + 5;
    if (n != JLA_TRIM_PACKED || in[0] != TRIM_VERSION)
        return -1;
    r.computer_trim = (int16_t)get16(in + 1);
    r.line_trim = (int16_t)get16(in + 3);
    if (!trim_ok(r.computer_trim) || !trim_ok(r.line_trim))
        return -1;
    for (uint32_t j = 0; j < JLA_DEVS; j++, p += 6) {
        int i = find(&r, get16(p), get16(p + 2));
        int16_t v = (int16_t)get16(p + 4);
        if (!trim_ok(v))
            return -1;
        if (i >= 0 && (get16(p) || get16(p + 2)))
            r.dev[i].trim = v;
    }
    *t = r;
    return 0;
}
