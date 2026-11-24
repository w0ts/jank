/* SPDX-License-Identifier: GPL-3.0-only */
/* Jank's side of the Digitone's live control (jank_dn2live.h). */
#include "jank_dn2live.h"
#include <string.h>
#include "jank_dn2rpc.h"

#define SUB_STATUS 0u
#define SUB_ON 1u
#define WHAT_PUSH_NOTES 1u
#define ST_NOT_UNDERSTOOD 2u
#define ST_NOT_LIVE 4u
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
}

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
        if (since >= JDL_KEEPALIVE_MS)
            return send(d, now_ms, JDL_CMD_LIVE, status, sizeof status, out, cap);
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

size_t jdl_mixer_read(struct jdl *d, uint32_t now_ms, uint8_t *out, size_t cap)
{
    if (d->state != JDL_LIVE)
        return 0;
    return send(d, now_ms, JDL_CMD_MIXER, NULL, 0, out, cap);
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
    if (r.cmd == JDL_CMD_PLAY && r.status == ST_NOT_UNDERSTOOD) {   /* a Digitone with live control but no PLAY: the pads play by MIDI channel then */
        d->play_ok = 0;
        return JDL_IN_REPLY;
    }
    if (r.cmd == JDL_CMD_MESSAGE && r.status == ST_NOT_UNDERSTOOD) {   /* a build without MESSAGE (our texts are always valid): none is sent from now on */
        d->msg_ok = 0;
        return JDL_IN_REPLY;
    }
    if (r.cmd != JDL_CMD_LIVE && r.cmd != JDL_CMD_TRACK && r.cmd != JDL_CMD_PLAY && r.cmd != JDL_CMD_STEPS && r.cmd != JDL_CMD_STEP && r.cmd != JDL_CMD_MESSAGE &&
        r.cmd != JDL_CMD_MIXER)
        return JDL_IN_IGNORED;
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
