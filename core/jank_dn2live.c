/* SPDX-License-Identifier: GPL-3.0-only */
/* Jank's side of the Digitone's live control (jank_dn2live.h). */
#include "jank_dn2live.h"
#include <string.h>
#include "jank_dn2rpc.h"

#define SUB_STATUS 0u
#define SUB_ON 1u
#define SUB_ERASE 3u
#define WHAT_PUSH_NOTES 1u
#define ST_NOT_UNDERSTOOD 2u
#define ST_NOT_LIVE 4u
#define ST_RECORD_BUSY 6u              /* RECORD: GRID or STEP RECORDING is on */
#define RECORD_PROTO 6u                /* the LIVE STATUS protocol that has the record byte */
#define NOTE_BYTES 6u                  /* track, step, note, velocity, length, flags */
#define PUSH_HEAD 3u                   /* n, u16 lost */

static uint32_t u32be(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

void jdl_init(struct jdl *d)
{
    memset(d, 0, sizeof *d);
    d->state = JDL_DOWN;
    d->play_ok = 1;
    d->read_ok = 1;
    d->msg_ok = 1;
    d->scale_ok = 1;
    d->scale_set_ok = 1;
    d->mix_set_ok = 1;
    d->record_ok = 1;
    d->patterns_ok = 1;
    d->pattern_ok = 1;
    d->transport_ok = 1;
    d->chain_ok = 1;
    d->jank_ok = 1;
    d->align_ok = 1;
    d->link_ok = 1;
}

static int proto7(const struct jdl *d) { return d->st.valid && d->st.proto >= 7u; }

int jdl_is_live(const struct jdl *d) { return d->state == JDL_LIVE; }

int jdl_can_play(const struct jdl *d) { return d->state == JDL_LIVE && d->play_ok; }

static size_t send(struct jdl *d, uint32_t now_ms, uint8_t cmd, const uint8_t *args, size_t nargs, uint8_t *out, size_t cap)
{
    size_t n = jd2_request(++d->seq, cmd, args, nargs, out, cap);
    if (n) {
        d->asked = 1;
        d->sent_ms = now_ms;
    }
    return n;
}

size_t jdl_poll(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap)
{
    uint32_t since = now_ms - d->sent_ms;
    static const uint8_t on[2] = {SUB_ON, WHAT_PUSH_NOTES}, status[1] = {SUB_STATUS};
    switch (d->state) {
    case JDL_LIVE:
        if (since >= JDL_KEEPALIVE_MS || (proto7(d) && (uint32_t)(now_ms - d->status_ms) >= JDL_STATUS_FAST_MS)) {
            d->status_ms = now_ms;
            return send(d, now_ms, JDL_CMD_LIVE, status, sizeof status, out, cap);
        }
        return 0;
    case JDL_REMOTE_OFF:
    case JDL_NO_LIVE:
        if (!d->asked || since >= JDL_IDLE_ASK_MS)
            return send(d, now_ms, JDL_CMD_LIVE, on, sizeof on, out, cap);
        return 0;
    default:                           /* down, or asking: until something answers */
        if (!d->asked || since >= JDL_ASK_MS) {
            d->state = JDL_ASKING;
            return send(d, now_ms, JDL_CMD_LIVE, on, sizeof on, out, cap);
        }
        return 0;
    }
}

size_t jdl_track(struct jdl *d, uint32_t now_ms, uint8_t track, uint8_t *out, size_t cap)
{
    size_t n;
    if (d->state != JDL_LIVE || track > 15u)
        return 0;
    n = send(d, now_ms, JDL_CMD_TRACK, &track, 1, out, cap);
    if (n) {
        d->select_seq = d->seq;        /* a read sent before this one may answer with the track it had */
        d->has_select = 1;
    }
    return n;
}

size_t jdl_steps_read(struct jdl *d, uint32_t now_ms, uint8_t track, uint8_t page, uint8_t *out, size_t cap)
{
    uint8_t args[2];
    if (d->state != JDL_LIVE || track > 15u || page > 3u)
        return 0;
    args[0] = track;
    args[1] = page;
    return send(d, now_ms, JDL_CMD_STEPS, args, sizeof args, out, cap);
}

size_t jdl_step_edit(struct jdl *d, uint32_t now_ms, uint8_t track, uint8_t step, enum jdl_step_op op, uint8_t note, uint8_t vel, uint8_t len, uint8_t *out, size_t cap)
{
    uint8_t args[6];
    if (d->state != JDL_LIVE || track > 15u || step > 127u || (unsigned)op > (unsigned)JDL_STEP_NOTE || note > 127u || (vel != 255u && (vel < 1u || vel > 127u)) ||
        (len != 255u && len > 127u))
        return 0;
    args[0] = track;
    args[1] = step;
    args[2] = (uint8_t)op;
    args[3] = note;
    args[4] = vel;
    args[5] = len;
    return send(d, now_ms, JDL_CMD_STEP, args, sizeof args, out, cap);
}

size_t jdl_scale_read(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap)
{
    static const uint8_t args[3] = {255u, (uint8_t)(JDL_SCALES - 1u), 1u};   /* the active track; the last name only (one short name follows the header); short names */
    if (d->state != JDL_LIVE || !d->scale_ok)
        return 0;
    return send(d, now_ms, JDL_CMD_SCALE, args, sizeof args, out, cap);
}

size_t jdl_scale_set(struct jdl *d, uint32_t now_ms, uint8_t scale, uint8_t root, uint8_t *out, size_t cap)
{
    uint8_t args[3];
    if (d->state != JDL_LIVE || !d->scale_set_ok || scale >= JDL_SCALES || (root >= 12u && root != JDL_ROOT_UNCHANGED))
        return 0;
    args[0] = 255u;
    args[1] = scale;
    args[2] = root;
    return send(d, now_ms, JDL_CMD_SCALE_SET, args, sizeof args, out, cap);
}

int jdl_take_scale(struct jdl *d, struct jdl_scale *out)
{
    if (!d->scale_fresh)
        return 0;
    *out = d->sc;
    d->scale_fresh = 0;
    return 1;
}

#define SCALE_HEAD 6u                  /* track, scale, per track, names, first, n */
#define SCALE_PROTO_ROOT 5u            /* from this LIVE STATUS protocol the header is followed by the root and the key mode */

static void take_scale(struct jdl *d, const struct jd2_reply *r)
{
    struct jdl_scale *s = &d->sc;
    if (r->n < SCALE_HEAD || r->data[1] >= JDL_SCALES)
        return;
    s->track = r->data[0];
    s->index = r->data[1];
    s->per_track = r->data[2];
    s->has_root = 0;
    s->root = 0;
    s->key_mode = 0;
    if (d->st.valid && d->st.proto >= SCALE_PROTO_ROOT && r->n >= SCALE_HEAD + 2u && r->data[SCALE_HEAD] < 12u) {
        s->has_root = 1;
        s->root = r->data[SCALE_HEAD];
        s->key_mode = r->data[SCALE_HEAD + 1u];
    }
    d->scale_fresh = 1;
}

/* SCALE SET's answer: `u8 track, u8 scale, u8 root` */
static void take_scale_set(struct jdl *d, const struct jd2_reply *r)
{
    struct jdl_scale *s = &d->sc;
    if (r->n < 3u || r->data[1] >= JDL_SCALES)
        return;
    s->track = r->data[0];
    s->index = r->data[1];
    s->has_root = r->data[2] < 12u;    /* (per-track flag and key mode: as the last read said) */
    s->root = r->data[2] < 12u ? r->data[2] : 0u;
    d->scale_fresh = 1;
}

size_t jdl_mixer_read(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap)
{
    if (d->state != JDL_LIVE)
        return 0;
    return send(d, now_ms, JDL_CMD_MIXER, NULL, 0, out, cap);
}

size_t jdl_mixer_set(struct jdl *d, uint32_t now_ms, uint8_t track, enum jdl_mix_param param, uint8_t value, uint8_t *out, size_t cap)
{
    uint8_t args[3];
    if (d->state != JDL_LIVE || !d->mix_set_ok || (track > 15u && track != 255u) || (unsigned)param > (unsigned)JDL_MIX_LEVEL || value > 127u)
        return 0;
    args[0] = track;
    args[1] = (uint8_t)param;
    args[2] = value;
    return send(d, now_ms, JDL_CMD_MIXER_SET, args, sizeof args, out, cap);
}

size_t jdl_mute_set(struct jdl *d, uint32_t now_ms, uint8_t which, uint16_t mask, uint8_t *out, size_t cap)
{
    uint8_t args[3];
    if (d->state != JDL_LIVE || !d->mix_set_ok || which > 1u)
        return 0;
    args[0] = which;
    args[1] = (uint8_t)(mask >> 8);
    args[2] = (uint8_t)mask;
    return send(d, now_ms, JDL_CMD_MUTE_SET, args, sizeof args, out, cap);
}

size_t jdl_erase(struct jdl *d, uint32_t now_ms, uint16_t tracks, uint8_t *out, size_t cap)
{
    uint8_t args[3];
    if (d->state != JDL_LIVE)
        return 0;
    args[0] = SUB_ERASE;
    args[1] = (uint8_t)(tracks >> 8);
    args[2] = (uint8_t)tracks;
    return send(d, now_ms, JDL_CMD_LIVE, args, sizeof args, out, cap);
}

size_t jdl_record(struct jdl *d, uint32_t now_ms, uint8_t on, uint8_t *out, size_t cap)
{
    if (d->state != JDL_LIVE || !d->record_ok || (on > 1u && on != 255u))
        return 0;
    return send(d, now_ms, JDL_CMD_RECORD, &on, 1, out, cap);
}

size_t jdl_patterns_read(struct jdl *d, uint32_t now_ms, uint8_t bank, uint8_t *out, size_t cap)
{
    if (d->state != JDL_LIVE || !d->patterns_ok || (bank >= JDL_BANKS && bank != JDL_BANK_PLAYING))
        return 0;
    return send(d, now_ms, JDL_CMD_PATTERNS, &bank, 1, out, cap);
}

size_t jdl_pattern(struct jdl *d, uint32_t now_ms, uint8_t pattern, uint8_t how, uint8_t *out, size_t cap)
{
    uint8_t args[2];
    if (d->state != JDL_LIVE || !d->pattern_ok || pattern >= JDL_PATTERNS || (how > JDL_PATTERN_JUMP && how != JDL_PATTERN_READ))
        return 0;
    args[0] = how == JDL_PATTERN_READ ? 0u : pattern;
    args[1] = how;
    return send(d, now_ms, JDL_CMD_PATTERN, args, sizeof args, out, cap);
}

int jdl_take_patterns(struct jdl *d, struct jdl_patterns *out)
{
    if (!d->patterns_fresh)
        return 0;
    *out = d->pb;
    d->patterns_fresh = 0;
    return 1;
}

int jdl_take_pattern(struct jdl *d, struct jdl_pattern *out)
{
    if (!d->pattern_fresh)
        return 0;
    *out = d->pc;
    d->pattern_fresh = 0;
    return 1;
}

/* PATTERNS' data: `u8 bank, u8 playing, u16 mask`; PATTERN's: `u8 playing, u8 cued` */
static void take_patterns(struct jdl *d, const struct jd2_reply *r)
{
    if (r->n < 4u || r->data[0] >= JDL_BANKS || r->data[1] >= JDL_PATTERNS)
        return;
    d->pb.bank = r->data[0];
    d->pb.playing = r->data[1];
    d->pb.mask = (uint16_t)((r->data[2] << 8) | r->data[3]);
    d->patterns_fresh = 1;
}

static void take_pattern(struct jdl *d, const struct jd2_reply *r)
{
    if (r->n < 2u || r->data[0] >= JDL_PATTERNS || (r->data[1] >= JDL_PATTERNS && r->data[1] != JDL_NO_PATTERN))
        return;
    d->pc.playing = r->data[0];
    d->pc.next = r->data[1];
    d->pc.mode = r->n > 2u ? r->data[2] : 0u;
    d->pc.running = r->n > 3u ? r->data[3] : 0u;
    d->pc.armed = r->n > 4u ? r->data[4] : 0u;
    d->pattern_fresh = 1;
}

size_t jdl_transport(struct jdl *d, uint32_t now_ms, uint8_t op, uint8_t *out, size_t cap)
{
    if (d->state != JDL_LIVE || !d->transport_ok || (op > JDL_TR_PAUSE && op != JDL_TR_READ))
        return 0;
    return send(d, now_ms, JDL_CMD_TRANSPORT, &op, 1, out, cap);
}

size_t jdl_probe_transport(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap)
{
    size_t n;
    if (d->transport_probed)
        return 0;
    n = jdl_transport(d, now_ms, JDL_TR_READ, out, cap);
    if (n)
        d->transport_probed = 1;
    return n;
}

int jdl_take_transport(struct jdl *d)
{
    if (!d->transport_fresh)
        return -1;
    d->transport_fresh = 0;
    return d->transport_state;
}

size_t jdl_chain_set(struct jdl *d, uint32_t now_ms, const uint8_t *patterns, size_t n, uint8_t *out, size_t cap)
{
    uint8_t args[2u + JDL_CHAIN_MAX];
    if (d->state != JDL_LIVE || !d->chain_ok || !patterns || n < JDL_CHAIN_MIN || n > JDL_CHAIN_MAX)
        return 0;
    for (size_t i = 0; i < n; i++) {
        if (patterns[i] >= JDL_PATTERNS)
            return 0;
        args[2u + i] = patterns[i];
    }
    args[0] = JDL_CHAIN_SET;
    args[1] = (uint8_t)n;
    return send(d, now_ms, JDL_CMD_CHAIN, args, 2u + n, out, cap);
}

size_t jdl_chain_clear(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap)
{
    static const uint8_t op = JDL_CHAIN_CLEAR;
    if (d->state != JDL_LIVE || !d->chain_ok)
        return 0;
    return send(d, now_ms, JDL_CMD_CHAIN, &op, 1, out, cap);
}

size_t jdl_chain_read(struct jdl *d, uint32_t now_ms, uint8_t first, uint8_t *out, size_t cap)
{
    uint8_t args[2] = {JDL_CHAIN_READ, first};
    if (d->state != JDL_LIVE || !d->chain_ok || first >= JDL_CHAIN_MAX)
        return 0;
    return send(d, now_ms, JDL_CMD_CHAIN, args, sizeof args, out, cap);
}

int jdl_take_chain(struct jdl *d, struct jdl_chain *out)
{
    if (!d->chain_fresh)
        return 0;
    *out = d->ch;
    d->chain_fresh = 0;
    return 1;
}

int jdl_take_live7(struct jdl *d, struct jdl_live7 *out)
{
    if (!d->live7_fresh)
        return 0;
    *out = d->st.p7;
    d->live7_fresh = 0;
    return 1;
}

static uint16_t u16be(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

/* CHAIN's answer: `u8 mode, u16 count, u16 position, u8 first, u8 k, k x pattern` (a SET's or a CLEAR's answer of that shape is taken too) */
static void take_chain(struct jdl *d, const struct jd2_reply *r)
{
    struct jdl_chain *c = &d->ch;
    uint8_t k;
    if (r->n < JDL_CHAIN_HEAD)
        return;
    k = r->data[6];
    if (r->n < JDL_CHAIN_HEAD + (size_t)k || k > JDL_CHAIN_MAX)
        return;
    memset(c, 0, sizeof *c);
    c->mode = r->data[0];
    c->count = u16be(r->data + 1);
    c->pos = u16be(r->data + 3);
    c->first = r->data[5];
    c->k = k;
    for (unsigned i = 0; i < k; i++) {
        uint8_t p = r->data[JDL_CHAIN_HEAD + i];
        if (p < JDL_PATTERNS)
            c->member[p >> 3] |= (uint8_t)(1u << (p & 7u));
    }
    d->chain_fresh = 1;
}

/* the protocol-7 bytes after the 73 of protocol 6 */
static void take_live7(struct jdl *d, const uint8_t *s, size_t n)
{
    struct jdl_live7 *p = &d->st.p7;
    d->st.p7_valid = d->st.proto >= 7u && n >= JDL_P7_BYTES;
    if (!d->st.p7_valid)
        return;
    s += JDL_P7_BYTE;
    p->transport = s[0];
    p->playing = s[1];
    p->next = s[2];
    p->mode = s[3];
    p->chain_count = u16be(s + 4);
    p->chain_pos = u16be(s + 6);
    p->counters = n >= JDL_P7_COUNTERS_BYTES;
    p->applied = p->counters ? u32be(s + 8) : 0u;
    p->refused = p->counters ? u32be(s + 12) : 0u;
    d->live7_fresh = 1;
}

int jdl_take_mixer(struct jdl *d, struct jdl_mixer *out)
{
    if (!d->mixer_fresh)
        return 0;
    *out = d->mx;
    d->mixer_fresh = 0;
    return 1;
}

/* the MIXER data: 10 bytes, then 16 tracks of 6 */
#define MIXER_HEAD 10u
#define MIXER_TRACK_BYTES 6u
#define MIXER_BYTES (MIXER_HEAD + JDL_TRACKS * MIXER_TRACK_BYTES)

static void take_mixer(struct jdl *d, const struct jd2_reply *r)
{
    struct jdl_mixer *m = &d->mx;
    if (r->n < MIXER_BYTES || r->data[8] > 15u || r->data[9] != JDL_TRACKS)
        return;
    m->mute = u32be(r->data);
    m->solo = u32be(r->data + 4);
    m->active = r->data[8];
    for (unsigned i = 0; i < JDL_TRACKS; i++) {
        const uint8_t *s = r->data + MIXER_HEAD + i * MIXER_TRACK_BYTES;
        m->t[i].vol = s[0];
        m->t[i].pan = s[1];
        m->t[i].del = s[2];
        m->t[i].rev = s[3];
        m->t[i].chr = s[4];
        m->t[i].level = s[5];
    }
    d->mixer_fresh = 1;
}

int jdl_take_page(struct jdl *d, struct jdl_page *out)
{
    if (!d->page_fresh)
        return 0;
    *out = d->pg;
    d->page_fresh = 0;
    return 1;
}

/* the STEPS data: 8 bytes, then 16 steps of 6 */
#define STEPS_HEAD 8u
#define STEPS_STEP_BYTES 6u
#define STEPS_BYTES (STEPS_HEAD + JDL_PAGE_STEPS * STEPS_STEP_BYTES)
#define ST_BUSY 5u

static void take_steps(struct jdl *d, const struct jd2_reply *r)
{
    struct jdl_page *p = &d->pg;
    if (r->n < STEPS_BYTES || r->data[0] > 15u || r->data[1] > 3u)
        return;
    p->track = r->data[0];
    p->page = r->data[1];
    p->pages = r->data[2];
    p->length = r->data[3];
    for (unsigned i = 0; i < JDL_PAGE_STEPS; i++) {
        const uint8_t *s = r->data + STEPS_HEAD + i * STEPS_STEP_BYTES;
        p->step[i].trig = s[1] & 1u;              /* the u16 flags, big-endian: bit 0 is in the second byte */
        p->step[i].note = s[2];
        p->step[i].vel = s[3];
        p->step[i].len = s[4];
        p->step[i].notes = s[5];
    }
    d->page_fresh = 1;
}

int jdl_can_message(const struct jdl *d) { return d->state == JDL_LIVE && d->msg_ok; }

size_t jdl_message(struct jdl *d, uint32_t now_ms, const char *text, uint8_t tenths, uint8_t *out, size_t cap)
{
    uint8_t args[1u + JDL_MSG_MAX];
    size_t n = 0;
    if (!jdl_can_message(d) || !text)
        return 0;
    args[0] = tenths;
    for (; text[n]; n++) {
        if (n >= JDL_MSG_MAX || (unsigned char)text[n] < 0x20u || (unsigned char)text[n] > 0x7Eu)
            return 0;                  /* the Digitone refuses it: say nothing instead */
        args[1u + n] = (uint8_t)text[n];
    }
    if (!n)
        return 0;
    return send(d, now_ms, JDL_CMD_MESSAGE, args, 1u + n, out, cap);
}

size_t jdl_probe_play(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap)
{
    size_t n;
    if (d->state != JDL_LIVE || d->play_probed)
        return 0;
    n = jdl_play(d, now_ms, 255u, JDL_PLAY_ALL_OFF, 0, 0, out, cap);
    if (n)
        d->play_probed = 1;
    return n;
}

int jdl_can_read_track(const struct jdl *d) { return d->state == JDL_LIVE && d->read_ok; }

size_t jdl_track_read(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap)
{
    size_t n;
    if (!jdl_can_read_track(d))
        return 0;
    n = send(d, now_ms, JDL_CMD_TRACK, NULL, 0, out, cap);
    if (n) {
        d->read_seq = d->seq;
        d->has_read = 1;
    }
    return n;
}

int jdl_take_active(struct jdl *d)
{
    int t = d->fresh ? d->active : -1;
    d->fresh = 0;
    return t;
}

size_t jdl_play(struct jdl *d, uint32_t now_ms, uint8_t track, enum jdl_play_op op, uint8_t note, uint8_t vel, uint8_t *out, size_t cap)
{
    uint8_t args[4];
    if (!jdl_can_play(d) || (track > 15u && track != 255u) || (unsigned)op > (unsigned)JDL_PLAY_ALL_OFF)
        return 0;
    args[0] = track;
    args[1] = (uint8_t)op;
    args[2] = note;
    args[3] = vel;
    return send(d, now_ms, JDL_CMD_PLAY, args, sizeof args, out, cap);
}

enum jdl_in jdl_input(struct jdl *d, const uint8_t *sx, size_t n, struct jdl_note *notes, size_t *count)
{
    uint8_t scratch[JD2_SX_MAX];
    struct jd2_reply r;
    *count = 0;
    if (n > sizeof scratch || !jd2_parse_reply(sx, n, scratch, sizeof scratch, &r))
        return JDL_IN_IGNORED;
    if (r.cmd == JDL_CMD_PUSH) {       /* the push: u8 n, u16 lost, n notes; it has the reply's shape (cmd | 0x80), status 0, and a sequence of its own */
        size_t k;
        if (r.status != 0 || r.n < PUSH_HEAD)
            return JDL_IN_IGNORED;
        k = r.data[0];
        if (k > JDL_NOTES_MAX || r.n < PUSH_HEAD + k * NOTE_BYTES)
            return JDL_IN_IGNORED;
        for (size_t i = 0; i < k; i++) {
            const uint8_t *p = r.data + PUSH_HEAD + i * NOTE_BYTES;
            notes[i].track = p[0];
            notes[i].step = p[1];
            notes[i].note = p[2];
            notes[i].vel = p[3];
            notes[i].length = p[4];
            notes[i].flags = p[5];
        }
        *count = k;
        return JDL_IN_PUSH;
    }
    {
        enum jdl_in kind;
        if (jdl9_input(d, &r, &kind))  /* stage 9: the keys, OPEN, a LINK REQUEST; a refusal of the page, OPEN or LINK MODE */
            return kind;
    }
    if (r.cmd == JDL_CMD_PLAY && r.status == ST_NOT_UNDERSTOOD) {   /* a Digitone with live control but no PLAY: the pads play by MIDI channel then */
        d->play_ok = 0;
        return JDL_IN_REPLY;
    }
    if (r.cmd == JDL_CMD_MESSAGE && r.status == ST_NOT_UNDERSTOOD) {   /* a build without MESSAGE (our texts are always valid): none is sent from now on */
        d->msg_ok = 0;
        return JDL_IN_REPLY;
    }
    if (r.cmd != JDL_CMD_LIVE && r.cmd != JDL_CMD_TRACK && r.cmd != JDL_CMD_PLAY && r.cmd != JDL_CMD_STEPS && r.cmd != JDL_CMD_STEP && r.cmd != JDL_CMD_MESSAGE &&
        r.cmd != JDL_CMD_MIXER && r.cmd != JDL_CMD_SCALE && r.cmd != JDL_CMD_SCALE_SET && r.cmd != JDL_CMD_MIXER_SET && r.cmd != JDL_CMD_MUTE_SET && r.cmd != JDL_CMD_RECORD &&
        r.cmd != JDL_CMD_PATTERNS && r.cmd != JDL_CMD_PATTERN && r.cmd != JDL_CMD_TRANSPORT && r.cmd != JDL_CMD_CHAIN && r.cmd != JDL_CMD_JANK_PAGE &&
        r.cmd != JDL_CMD_JANK_OPEN && r.cmd != JDL_CMD_LINK_MODE)
        return JDL_IN_IGNORED;
    if (r.cmd == JDL_CMD_TRANSPORT || r.cmd == JDL_CMD_CHAIN) {   /* stage 8: 2 = a build without it (our arguments are always valid); 5 = CHAIN's queue is full */
        if (r.status == ST_NOT_UNDERSTOOD) {
            if (r.cmd == JDL_CMD_TRANSPORT)
                d->transport_ok = 0;
            else
                d->chain_ok = 0;
            return JDL_IN_REPLY;
        }
        if (r.status == ST_BUSY) {
            d->chain_busy += r.cmd == JDL_CMD_CHAIN;
            return JDL_IN_REPLY;
        }
        if (r.status == 0 && r.cmd == JDL_CMD_TRANSPORT && r.n >= 1u && r.data[0] <= JDL_TR_PAUSED) {
            d->transport_state = r.data[0];
            d->transport_fresh = 1;
        }
        if (r.status == 0 && r.cmd == JDL_CMD_CHAIN)
            take_chain(d, &r);
    }
    if (r.cmd == JDL_CMD_PATTERNS || r.cmd == JDL_CMD_PATTERN) {   /* 2: a build without it (our arguments are always valid): that command stops, live control is fine */
        if (r.status == ST_NOT_UNDERSTOOD) {
            if (r.cmd == JDL_CMD_PATTERNS)
                d->patterns_ok = 0;
            else
                d->pattern_ok = 0;
            return JDL_IN_REPLY;
        }
        if (r.status == 0) {
            if (r.cmd == JDL_CMD_PATTERNS)
                take_patterns(d, &r);
            else
                take_pattern(d, &r);
        }
    }
    if (r.cmd == JDL_CMD_RECORD) {     /* 2: a build without RECORD (live control itself is fine); 6: GRID or STEP RECORDING is on, that request only */
        if (r.status == ST_NOT_UNDERSTOOD) {
            d->record_ok = 0;
            return JDL_IN_REPLY;
        }
        if (r.status == ST_RECORD_BUSY) {
            d->record_busy++;
            return JDL_IN_REPLY;
        }
    }
    if (r.cmd == JDL_CMD_MIXER_SET || r.cmd == JDL_CMD_MUTE_SET) {   /* 2: not understood (an older build) or refused; 5: 16 writes already waiting: that write only */
        if (r.status == ST_NOT_UNDERSTOOD || r.status == ST_BUSY) {
            if (r.status == ST_NOT_UNDERSTOOD && !(d->st.valid && d->st.proto >= SCALE_PROTO_ROOT))
                d->mix_set_ok = 0;     /* a build without the mixer writes */
            else
                d->mix_set_refused++;
            return JDL_IN_REPLY;
        }
    }
    if (r.cmd == JDL_CMD_SCALE || r.cmd == JDL_CMD_SCALE_SET) {   /* a refusal (2) ends that command only: live control is fine */
        if (r.status == ST_NOT_UNDERSTOOD) {
            if (r.cmd == JDL_CMD_SCALE)
                d->scale_ok = 0;
            else if (d->st.valid && d->st.proto >= SCALE_PROTO_ROOT)
                d->scale_set_refused++;    /* a build that HAS SCALE SET: status 2 is that one write refused (the screen shows a pattern that is not the playing one) */
            else
                d->scale_set_ok = 0;       /* a build without it */
            return JDL_IN_REPLY;
        }
        if (r.status == 0) {
            if (r.cmd == JDL_CMD_SCALE)
                take_scale(d, &r);
            else
                take_scale_set(d, &r);
        }
    }
    if (r.cmd == JDL_CMD_MIXER) {      /* a refused read (2) changes nothing else: live control is fine */
        if (r.status == ST_NOT_UNDERSTOOD)
            return JDL_IN_REPLY;
        if (r.status == 0)
            take_mixer(d, &r);
    }
    if (r.cmd == JDL_CMD_STEPS || r.cmd == JDL_CMD_STEP) {   /* a refused read or write (2), a full queue (5): that request only, live control is fine */
        if (r.status == ST_NOT_UNDERSTOOD) {
            d->step_refused++;
            return JDL_IN_REPLY;
        }
        if (r.status == ST_BUSY) {
            d->step_busy++;
            return JDL_IN_REPLY;
        }
        if (r.status == 0 && r.cmd == JDL_CMD_STEPS)
            take_steps(d, &r);
    }
    if (r.cmd == JDL_CMD_TRACK) {
        int mine = d->has_read && r.seq == d->read_seq;
        if (r.status == ST_NOT_UNDERSTOOD) {   /* this command only: a build whose TRACK cannot be read (or a bad track); live control itself is fine */
            if (mine)
                d->read_ok = 0;
            return JDL_IN_REPLY;
        }
        if (r.status == 0 && mine && r.n >= 1u && r.data[0] <= 15u) {
            unsigned after = (uint16_t)(d->read_seq - d->select_seq);   /* the read must have been sent after the last select */
            if (!d->has_select || (after >= 1u && after <= 0x7FFFu)) {
                d->active = r.data[0];
                d->fresh = 1;
            }
        }
    }
    if (r.cmd == JDL_CMD_LIVE && r.status == 0 && r.n >= JDL_STATUS_BYTES) {   /* the Digitone's own counters: what it made, sent and lost of the push */
        const uint8_t *s = r.data;
        d->st.valid = 1;
        d->st.proto = s[0];
        d->st.push = s[1];
        d->st.lease = s[2];
        d->st.made = u32be(s + 5);
        d->st.sent = u32be(s + 9);
        d->st.lost = u32be(s + 13);
        d->st.dropped = u32be(s + 17);
        d->st.msgs = u32be(s + 21);
        d->st.waited = u32be(s + 25);
        d->st.rec_valid = s[0] >= RECORD_PROTO && r.n > JDL_REC_BYTE;   /* protocol 6: the record byte at the end */
        d->st.rec_bits = d->st.rec_valid ? s[JDL_REC_BYTE] : 0u;
        take_live7(d, s, r.n);         /* protocol 7: the transport, the patterns, the chain */
        jdl9_status(d, s, r.n);        /* protocol 8: the JANK page, LINK MODE */
    }
    if (r.status == 0) {
        if (d->state != JDL_LIVE)
            d->state = JDL_LIVE;
    } else if (r.status == JD2_STATUS_REMOTE_OFF) {
        d->state = JDL_REMOTE_OFF;
    } else if (r.status == ST_NOT_UNDERSTOOD) {
        d->state = JDL_NO_LIVE;
    } else if (r.status == ST_NOT_LIVE) {
        d->state = JDL_ASKING;         /* the lease ran out: LIVE ON again at the next poll */
        d->asked = 0;
    }
    return JDL_IN_REPLY;
}
