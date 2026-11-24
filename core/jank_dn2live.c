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

void jdl_init(struct jdl *d)
{
    memset(d, 0, sizeof *d);
    d->state = JDL_DOWN;
    d->play_ok = 1;
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
    if (d->state != JDL_LIVE || track > 15u)
        return 0;
    return send(d, now_ms, JDL_CMD_TRACK, &track, 1, out, cap);
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
    if (r.cmd != JDL_CMD_LIVE && r.cmd != JDL_CMD_TRACK && r.cmd != JDL_CMD_PLAY)
        return JDL_IN_IGNORED;
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
