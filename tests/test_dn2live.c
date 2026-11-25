/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of Jank's side of the Digitone's live control (core/jank_dn2live.c): what it sends and when, how the Digitone's answers move its state, the note push.
 * Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_dn2live.h"
#include "jank_dn2rpc.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const uint8_t HEADER[6] = {0xF0, 0x00, 0x20, 0x3C, 0x7D, 0x01};

/* a frame from the Digitone: F0 .. header, pack7(u16 seq, cmd byte, status, data), F7 */
static size_t frame(uint8_t *out, uint16_t seq, uint8_t cmd_byte, uint8_t status, const uint8_t *data, size_t n)
{
    uint8_t body[4 + 128];
    size_t k;
    body[0] = (uint8_t)(seq >> 8);
    body[1] = (uint8_t)seq;
    body[2] = cmd_byte;
    body[3] = status;
    if (n)
        memcpy(body + 4, data, n);
    memcpy(out, HEADER, sizeof HEADER);
    k = sizeof HEADER;
    k += jd2_pack7(body, 4 + n, out + k);
    out[k++] = 0xF7;
    return k;
}

/* what a request says: its seq, cmd and arguments */
static size_t request_body(const uint8_t *sx, size_t n, uint8_t *body)
{
    if (n < 8 || memcmp(sx, HEADER, 6) || sx[n - 1] != 0xF7)
        return 0;
    return jd2_unpack7(sx + 6, n - 7, body);
}

static void reply(struct jdl *d, uint8_t cmd, uint8_t status)
{
    uint8_t f[64], notes[JDL_NOTES_MAX * 6];
    size_t n = frame(f, d->seq, (uint8_t)(cmd | 0x80), status, NULL, 0), count;
    struct jdl_note nn[JDL_NOTES_MAX];
    (void)notes;
    CHECK(jdl_input(d, f, n, nn, &count) == JDL_IN_REPLY);
}

static struct jdl live(uint32_t t)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(jdl_poll(&d, t, out, sizeof out) > 0);
    reply(&d, JDL_CMD_LIVE, 0);
    CHECK(jdl_is_live(&d));
    return d;
}

static void test_asks_for_live_control_until_answered(void)
{
    struct jdl d;
    uint8_t out[64], body[32];
    size_t n;
    jdl_init(&d);
    CHECK(d.state == JDL_DOWN && !jdl_is_live(&d) && !jdl_can_play(&d));
    n = jdl_poll(&d, 1000, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 5 && body[2] == JDL_CMD_LIVE && body[3] == 1 && body[4] == 1);   /* LIVE ON, push the notes */
    CHECK(d.state == JDL_ASKING);
    CHECK(jdl_poll(&d, 1000 + JDL_ASK_MS - 1u, out, sizeof out) == 0);                      /* not again yet */
    n = jdl_poll(&d, 1000 + JDL_ASK_MS, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 5 && body[2] == JDL_CMD_LIVE && body[3] == 1);   /* asking again: nothing answered */
    CHECK(jdl_track(&d, 5000, 3, out, sizeof out) == 0 && jdl_play(&d, 5000, 3, JDL_PLAY_ON, 60, 100, out, sizeof out) == 0);   /* nothing else until live */
}

static void test_keepalive_while_live(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[32];
    size_t n;
    CHECK(jdl_poll(&d, JDL_KEEPALIVE_MS - 1u, out, sizeof out) == 0);
    n = jdl_poll(&d, JDL_KEEPALIVE_MS, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[2] == JDL_CMD_LIVE && body[3] == 0);   /* a LIVE STATUS: any live request keeps the lease */
    reply(&d, JDL_CMD_LIVE, 0);
    CHECK(jdl_poll(&d, JDL_KEEPALIVE_MS + 100u, out, sizeof out) == 0);
    CHECK(jdl_poll(&d, 2u * JDL_KEEPALIVE_MS, out, sizeof out) > 0);
}

static void test_track_and_play_are_requests_and_keep_the_lease(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[32];
    size_t n = jdl_track(&d, 3000, 7, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[2] == JDL_CMD_TRACK && body[3] == 7);
    CHECK(jdl_poll(&d, 3000 + JDL_KEEPALIVE_MS - 1u, out, sizeof out) == 0);                 /* the track request was a live request: the keepalive waits */
    CHECK(jdl_poll(&d, 3000 + JDL_KEEPALIVE_MS, out, sizeof out) > 0);
    CHECK(jdl_track(&d, 9000, 16, out, sizeof out) == 0);                                    /* only tracks 0..15 */
    n = jdl_play(&d, 9000, 255, JDL_PLAY_ON, 60, 100, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 7 && body[2] == JDL_CMD_PLAY && body[3] == 255 && body[4] == 1 && body[5] == 60 && body[6] == 100);
    n = jdl_play(&d, 9001, 0, JDL_PLAY_DEFAULT, 0, 0, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 7 && body[3] == 0 && body[4] == 2);         /* the track's own default note */
    n = jdl_play(&d, 9002, 15, JDL_PLAY_OFF, 255, 0, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 7 && body[3] == 15 && body[4] == 0 && body[5] == 255);
    n = jdl_play(&d, 9003, 255, JDL_PLAY_ALL_OFF, 0, 0, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 7 && body[4] == 3);
    CHECK(jdl_play(&d, 9004, 16, JDL_PLAY_ON, 60, 100, out, sizeof out) == 0);               /* bad track */
    CHECK(jdl_play(&d, 9004, 3, (enum jdl_play_op)4, 60, 100, out, sizeof out) == 0);        /* bad op */
}

static void test_play_refused_by_a_digitone_without_it(void)
{
    struct jdl d = live(0);
    uint8_t out[64];
    CHECK(jdl_can_play(&d));
    CHECK(jdl_play(&d, 10, 0, JDL_PLAY_ON, 60, 100, out, sizeof out) > 0);
    reply(&d, JDL_CMD_PLAY, 2);                                                              /* status 2: not understood */
    CHECK(jdl_is_live(&d) && !jdl_can_play(&d));                                             /* still live (track select, the push), but no more PLAY: MIDI then */
    CHECK(jdl_play(&d, 20, 0, JDL_PLAY_ON, 60, 100, out, sizeof out) == 0);
    CHECK(jdl_track(&d, 20, 1, out, sizeof out) > 0);
}

static void test_remote_control_off(void)
{
    struct jdl d;
    uint8_t out[64], body[32];
    size_t n;
    jdl_init(&d);
    jdl_poll(&d, 0, out, sizeof out);
    reply(&d, JDL_CMD_LIVE, 7);
    CHECK(d.state == JDL_REMOTE_OFF && !jdl_is_live(&d));
    CHECK(jdl_poll(&d, JDL_ASK_MS * 2u, out, sizeof out) == 0);                              /* not every second now */
    CHECK(jdl_poll(&d, JDL_IDLE_ASK_MS - 1u, out, sizeof out) == 0);
    n = jdl_poll(&d, JDL_IDLE_ASK_MS, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 5 && body[3] == 1);                         /* LIVE ON again: the owner may have switched it on */
    reply(&d, JDL_CMD_LIVE, 0);
    CHECK(jdl_is_live(&d));
}

static void test_a_digitone_without_live_control(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    jdl_poll(&d, 0, out, sizeof out);
    reply(&d, JDL_CMD_LIVE, 2);
    CHECK(d.state == JDL_NO_LIVE && !jdl_is_live(&d) && !jdl_can_play(&d));
    CHECK(jdl_poll(&d, 100, out, sizeof out) == 0 && jdl_poll(&d, JDL_IDLE_ASK_MS, out, sizeof out) > 0);   /* it asks again, seldom */
}

static void test_the_lease_running_out(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[32];
    size_t n;
    jdl_track(&d, 100, 2, out, sizeof out);
    reply(&d, JDL_CMD_TRACK, 4);                                                             /* status 4: live control is off */
    CHECK(d.state == JDL_ASKING && !jdl_is_live(&d));
    n = jdl_poll(&d, 101, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 5 && body[2] == JDL_CMD_LIVE && body[3] == 1);   /* LIVE ON at once, not a second later */
}

static void test_a_stranger_and_noise(void)
{
    struct jdl d = live(0);
    struct jdl_note nn[JDL_NOTES_MAX];
    size_t count = 99;
    uint8_t f[64];
    CHECK(jdl_input(&d, (const uint8_t *)"\xF0\x7E\x00\xF7", 4, nn, &count) == JDL_IN_IGNORED && count == 0);
    CHECK(jdl_input(&d, f, 0, nn, &count) == JDL_IN_IGNORED);
    CHECK(jdl_input(&d, f, sizeof f + 200u, nn, &count) == JDL_IN_IGNORED);                  /* too long to be ours */
    {
        size_t n = frame(f, 1, 35 | 0x80, 0, NULL, 0);                                        /* a PEEK reply: not live control */
        CHECK(jdl_input(&d, f, n, nn, &count) == JDL_IN_IGNORED && jdl_is_live(&d));
    }
}

static size_t push_frame(uint8_t *f, unsigned n, const struct jdl_note *notes, uint16_t lost, uint8_t status)
{
    uint8_t data[3 + JDL_NOTES_MAX * 6];
    size_t k = 0;
    data[k++] = (uint8_t)n;
    data[k++] = (uint8_t)(lost >> 8);
    data[k++] = (uint8_t)lost;
    for (unsigned i = 0; i < n && i < JDL_NOTES_MAX; i++) {
        data[k++] = notes[i].track;
        data[k++] = notes[i].step;
        data[k++] = notes[i].note;
        data[k++] = notes[i].vel;
        data[k++] = notes[i].length;
        data[k++] = notes[i].flags;
    }
    return frame(f, 0x1234, 0xAF, status, data, k);
}

static void test_the_note_push(void)
{
    struct jdl d = live(0);
    struct jdl_note in[3] = {{0, 4, 60, 100, 8, 0}, {5, 5, 64, 90, 9, 1}, {15, 127, 36, 127, 1, 12}}, out[JDL_NOTES_MAX];
    uint8_t f[256];
    size_t n, count = 0;
    n = push_frame(f, 3, in, 7, 0);
    CHECK(jdl_input(&d, f, n, out, &count) == JDL_IN_PUSH && count == 3);
    CHECK(out[0].track == 0 && out[0].step == 4 && out[0].note == 60 && out[0].vel == 100 && out[0].length == 8 && out[0].flags == 0);
    CHECK(out[1].track == 5 && out[1].note == 64 && out[1].flags == 1);
    CHECK(out[2].track == 15 && out[2].step == 127 && out[2].note == 36 && out[2].vel == 127 && out[2].flags == 12);
    CHECK(jdl_is_live(&d));                                                                  /* a push is not a reply: the state is as it was */
    n = push_frame(f, 0, in, 0, 0);
    CHECK(jdl_input(&d, f, n, out, &count) == JDL_IN_PUSH && count == 0);                    /* an empty push */
    {
        struct jdl_note full[JDL_NOTES_MAX];
        memset(full, 0, sizeof full);
        full[15].note = 99;
        n = push_frame(f, JDL_NOTES_MAX, full, 0, 0);
        CHECK(jdl_input(&d, f, n, out, &count) == JDL_IN_PUSH && count == JDL_NOTES_MAX && out[15].note == 99);   /* the most a push holds */
    }
    n = push_frame(f, 3, in, 0, 5);
    CHECK(jdl_input(&d, f, n, out, &count) == JDL_IN_IGNORED && count == 0);                 /* a push with a status is not one */
    {
        uint8_t data[3 + 6] = {2, 0, 0, 0, 0, 60, 100, 8, 0};                                /* says 2 notes, holds 1 */
        n = frame(f, 1, 0xAF, 0, data, sizeof data);
        CHECK(jdl_input(&d, f, n, out, &count) == JDL_IN_IGNORED && count == 0);
        data[0] = 17;                                                                          /* more than a push holds */
        n = frame(f, 1, 0xAF, 0, data, sizeof data);
        CHECK(jdl_input(&d, f, n, out, &count) == JDL_IN_IGNORED && count == 0);
        n = frame(f, 1, 0xAF, 0, data, 2);                                                     /* shorter than its own header */
        CHECK(jdl_input(&d, f, n, out, &count) == JDL_IN_IGNORED && count == 0);
    }
}

static void test_the_clock_wrapping(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(jdl_poll(&d, 0xFFFFFC00u, out, sizeof out) > 0);                                   /* a millisecond clock that is about to wrap */
    reply(&d, JDL_CMD_LIVE, 0);
    CHECK(jdl_poll(&d, 0xFFFFFC00u + JDL_KEEPALIVE_MS - 1u, out, sizeof out) == 0);          /* the wrap is inside the wait: 4 s are not up */
    CHECK(jdl_poll(&d, 0xFFFFFC00u + JDL_KEEPALIVE_MS, out, sizeof out) > 0);                /* and now they are, past the wrap */
}

/* a reply to request number `seq` of command `cmd` with data */
static void reply_to(struct jdl *d, uint16_t seq, uint8_t cmd, uint8_t status, const uint8_t *data, size_t n)
{
    uint8_t f[256];
    struct jdl_note nn[JDL_NOTES_MAX];
    size_t count, k = frame(f, seq, (uint8_t)(cmd | 0x80), status, data, n);
    CHECK(jdl_input(d, f, k, nn, &count) == JDL_IN_REPLY);
}

static void test_reading_the_active_track(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[32];
    const uint8_t five[1] = {5}, nine[1] = {9}, big[1] = {16};
    size_t n = jdl_track_read(&d, 100, out, sizeof out);
    CHECK(jdl_can_read_track(&d) && n > 0 && request_body(out, n, body) == 3 && body[2] == JDL_CMD_TRACK);   /* TRACK with no argument */
    CHECK(jdl_take_active(&d) == -1);                                                        /* nothing answered yet */
    reply_to(&d, (uint16_t)(d.seq + 1u), JDL_CMD_TRACK, 0, five, 1);                         /* an answer to another request: not ours */
    CHECK(jdl_take_active(&d) == -1);
    reply_to(&d, d.seq, JDL_CMD_TRACK, 0, five, 1);
    CHECK(jdl_take_active(&d) == 5 && jdl_take_active(&d) == -1);                            /* taken once */
    reply_to(&d, d.seq, JDL_CMD_TRACK, 0, big, 1);                                           /* not a track */
    reply_to(&d, d.seq, JDL_CMD_TRACK, 0, NULL, 0);                                          /* no data */
    CHECK(jdl_take_active(&d) == -1 && jdl_is_live(&d));
    reply_to(&d, d.seq, JDL_CMD_TRACK, 0, nine, 1);
    CHECK(jdl_take_active(&d) == 9);
}

static void test_a_read_sent_before_a_select_is_stale(void)
{
    struct jdl d = live(0);
    uint8_t out[64];
    const uint8_t old_track[1] = {2}, new_track[1] = {7};
    uint16_t read_seq;
    CHECK(jdl_track_read(&d, 100, out, sizeof out) > 0);
    read_seq = d.seq;
    CHECK(jdl_track(&d, 110, 7, out, sizeof out) > 0);                                       /* the Launchpad chose 7 after the read went out */
    reply_to(&d, read_seq, JDL_CMD_TRACK, 0, old_track, 1);                                  /* the read comes back with the track it had */
    CHECK(jdl_take_active(&d) == -1);
    CHECK(jdl_track_read(&d, 200, out, sizeof out) > 0);                                     /* a read sent after the select counts */
    reply_to(&d, d.seq, JDL_CMD_TRACK, 0, new_track, 1);
    CHECK(jdl_take_active(&d) == 7);
}

static void test_the_sequence_wrapping_in_the_stale_rule(void)
{
    struct jdl d = live(0);
    uint8_t out[64];
    const uint8_t t[1] = {4};
    d.seq = 0xFFFC;
    CHECK(jdl_track(&d, 10, 1, out, sizeof out) > 0);                                        /* select 0xFFFD */
    CHECK(jdl_track_read(&d, 20, out, sizeof out) > 0);                                      /* read 0xFFFE: after the select */
    reply_to(&d, d.seq, JDL_CMD_TRACK, 0, t, 1);
    CHECK(jdl_take_active(&d) == 4);
    d.seq = 0xFFFF;                                                                          /* the next requests wrap: read 0x0000 is still after select 0xFFFD */
    CHECK(jdl_track(&d, 30, 1, out, sizeof out) > 0);                                        /* select 0x0000 */
    CHECK(jdl_track_read(&d, 40, out, sizeof out) > 0);                                      /* read 0x0001 */
    reply_to(&d, d.seq, JDL_CMD_TRACK, 0, t, 1);
    CHECK(jdl_take_active(&d) == 4);
}

static void test_a_build_that_cannot_read_the_track(void)
{
    struct jdl d = live(0);
    uint8_t out[64];
    CHECK(jdl_track_read(&d, 10, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_TRACK, 2, NULL, 0);                                          /* status 2: not understood */
    CHECK(jdl_is_live(&d) && !jdl_can_read_track(&d) && jdl_track_read(&d, 20, out, sizeof out) == 0);   /* live control itself is fine: only the read is off */
    CHECK(jdl_can_play(&d) && jdl_track(&d, 30, 3, out, sizeof out) > 0);
}

static void test_a_refused_select_is_not_the_end_of_live_control(void)
{
    struct jdl d = live(0);
    uint8_t out[64];
    CHECK(jdl_track(&d, 10, 3, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_TRACK, 2, NULL, 0);                                          /* a select the Digitone did not like */
    CHECK(jdl_is_live(&d) && jdl_can_read_track(&d));
}

static void test_no_read_unless_live(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(!jdl_can_read_track(&d) && jdl_track_read(&d, 10, out, sizeof out) == 0);
}

static void test_the_digitones_own_counters(void)
{
    struct jdl d;
    uint8_t out[64];
    /* proto 3, push on, 9 s of lease, ring 64, 16 a push, then u32 made 100, sent 96, lost 4, dropped 2, push messages 30, waited 7, u16 next seq 0x0102 (big-endian) */
    const uint8_t st[31] = {3, 1, 9, 64, 16, 0, 0, 0, 100, 0, 0, 0, 96, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 30, 0, 0, 0, 7, 1, 2};
    const uint8_t big[34] = {3, 1, 9, 64, 16, 0, 0, 1, 0, 0, 0, 0, 96, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 30, 0, 0, 0, 7, 1, 2, 0xFF, 0xFF, 0xFF};
    jdl_init(&d);
    jdl_poll(&d, 0, out, sizeof out);
    CHECK(!d.st.valid);
    reply_to(&d, d.seq, JDL_CMD_LIVE, 0, st, sizeof st);
    CHECK(d.st.valid && d.st.proto == 3 && d.st.push == 1 && d.st.lease == 9 && d.st.made == 100 && d.st.sent == 96 && d.st.lost == 4 && d.st.dropped == 2 && d.st.msgs == 30 && d.st.waited == 7);
    CHECK(jdl_is_live(&d));
    reply_to(&d, d.seq, JDL_CMD_LIVE, 0, big, sizeof big);          /* a longer status (a later protocol): the first 31 bytes are read the same way */
    CHECK(d.st.made == 256 && d.st.lost == 4);
    reply_to(&d, d.seq, JDL_CMD_LIVE, 0, st, 30);                   /* a short one: not taken, the numbers stay */
    CHECK(d.st.made == 256);
}

static void test_play_is_asked_about_once_when_live(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[32];
    size_t n = jdl_probe_play(&d, 10, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 7 && body[2] == JDL_CMD_PLAY && body[3] == 255 && body[4] == 3);   /* all notes off, every track: harmless */
    CHECK(jdl_probe_play(&d, 20, out, sizeof out) == 0);                                     /* once per connection */
    reply_to(&d, d.seq, JDL_CMD_PLAY, 0, NULL, 0);                                           /* a Digitone with PLAY: it stays available */
    CHECK(jdl_can_play(&d));
}

static void test_a_digitone_without_play_is_known_before_the_first_press(void)
{
    struct jdl d = live(0);
    uint8_t out[64];
    CHECK(jdl_can_play(&d));                                                                 /* until asked, PLAY is assumed */
    CHECK(jdl_probe_play(&d, 10, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_PLAY, 2, NULL, 0);                                           /* not understood */
    CHECK(jdl_is_live(&d) && !jdl_can_play(&d));
    CHECK(jdl_play(&d, 20, 0, JDL_PLAY_ON, 48, 100, out, sizeof out) == 0);                  /* the first press never goes out */
}

static void test_play_is_asked_again_on_a_new_connection_and_not_before_live(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(jdl_probe_play(&d, 0, out, sizeof out) == 0);                                      /* not live: nothing */
    d = live(0);
    CHECK(jdl_probe_play(&d, 10, out, sizeof out) > 0);
    jdl_init(&d);                                                                            /* the Launchpad or Digitone came again */
    jdl_poll(&d, 100, out, sizeof out);
    reply_to(&d, d.seq, JDL_CMD_LIVE, 0, NULL, 0);
    CHECK(jdl_probe_play(&d, 110, out, sizeof out) > 0);
}

/* a STEPS answer for (track, page, pages, length): 16 steps, step i with flags 0x0381 (a note trig) when trigs[i], else 0x0300, note 40 + i, vel 100, len 14 */
static size_t steps_data(uint8_t *data, uint8_t track, uint8_t page, uint8_t pages, uint8_t length, const uint8_t *trigs)
{
    size_t k = 0;
    data[k++] = track;
    data[k++] = page;
    data[k++] = pages;
    data[k++] = length;
    data[k++] = 255;                                                                         /* playing step: not trusted */
    data[k++] = 0;
    data[k++] = track;
    data[k++] = 8;
    for (unsigned i = 0; i < 16; i++) {
        data[k++] = 0x03;
        data[k++] = trigs[i] ? 0x81 : 0x00;
        data[k++] = (uint8_t)(40 + i);
        data[k++] = 100;
        data[k++] = 14;
        data[k++] = trigs[i] ? 1 : 0;
    }
    return k;
}

static void test_reading_a_page_of_steps(void)
{
    struct jdl d = live(0);
    struct jdl_page pg;
    uint8_t out[64], body[32], data[104], trigs[16] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1};
    size_t n = jdl_steps_read(&d, 10, 3, 1, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 5 && body[2] == JDL_CMD_STEPS && body[3] == 3 && body[4] == 1);
    CHECK(jdl_steps_read(&d, 10, 16, 0, out, sizeof out) == 0 && jdl_steps_read(&d, 10, 0, 4, out, sizeof out) == 0);   /* only tracks 0..15, pages 0..3 */
    CHECK(!jdl_take_page(&d, &pg));
    reply_to(&d, d.seq, JDL_CMD_STEPS, 0, data, steps_data(data, 3, 1, 4, 64, trigs));
    CHECK(jdl_take_page(&d, &pg) && pg.track == 3 && pg.page == 1 && pg.pages == 4 && pg.length == 64);
    CHECK(pg.step[0].trig == 1 && pg.step[1].trig == 0 && pg.step[4].trig == 1 && pg.step[15].trig == 1);   /* trig = bit 0 of the u16 flags */
    CHECK(pg.step[0].note == 40 && pg.step[15].note == 55 && pg.step[2].vel == 100 && pg.step[2].len == 14 && pg.step[0].notes == 1 && pg.step[1].notes == 0);
    CHECK(!jdl_take_page(&d, &pg));                                                          /* taken once */
    CHECK(jdl_is_live(&d));
}

static void test_a_damaged_steps_answer_is_not_taken(void)
{
    struct jdl d = live(0);
    struct jdl_page pg;
    uint8_t data[104], trigs[16] = {0};
    size_t n = steps_data(data, 3, 1, 4, 64, trigs);
    reply_to(&d, d.seq, JDL_CMD_STEPS, 0, data, n - 1u);                                     /* one byte short */
    CHECK(!jdl_take_page(&d, &pg));
    data[0] = 16;                                                                            /* a track that does not exist */
    reply_to(&d, d.seq, JDL_CMD_STEPS, 0, data, n);
    CHECK(!jdl_take_page(&d, &pg));
    data[0] = 3;
    data[1] = 4;                                                                             /* a page past the last */
    reply_to(&d, d.seq, JDL_CMD_STEPS, 0, data, n);
    CHECK(!jdl_take_page(&d, &pg));
}

static void test_editing_a_step(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[32];
    size_t n = jdl_step_edit(&d, 10, 2, 17, JDL_STEP_NOTE, 60, 255, 255, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 9 && body[2] == JDL_CMD_STEP && body[3] == 2 && body[4] == 17 && body[5] == 2 && body[6] == 60 && body[7] == 255 && body[8] == 255);
    n = jdl_step_edit(&d, 11, 0, 0, JDL_STEP_TRIG, 0, 255, 255, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 9 && body[5] == 1);
    n = jdl_step_edit(&d, 12, 15, 127, JDL_STEP_CLEAR, 0, 255, 255, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 9 && body[3] == 15 && body[4] == 127 && body[5] == 0);
    CHECK(jdl_step_edit(&d, 13, 16, 0, JDL_STEP_TRIG, 0, 255, 255, out, sizeof out) == 0);       /* what the Digitone would refuse is not sent */
    CHECK(jdl_step_edit(&d, 13, 0, 128, JDL_STEP_TRIG, 0, 255, 255, out, sizeof out) == 0);
    CHECK(jdl_step_edit(&d, 13, 0, 0, (enum jdl_step_op)3, 0, 255, 255, out, sizeof out) == 0);
    CHECK(jdl_step_edit(&d, 13, 0, 0, JDL_STEP_NOTE, 128, 255, 255, out, sizeof out) == 0);
    CHECK(jdl_step_edit(&d, 13, 0, 0, JDL_STEP_NOTE, 60, 0, 255, out, sizeof out) == 0);         /* velocity 0 is refused by design */
    CHECK(jdl_step_edit(&d, 13, 0, 0, JDL_STEP_NOTE, 60, 128, 255, out, sizeof out) == 0);
    CHECK(jdl_step_edit(&d, 13, 0, 0, JDL_STEP_NOTE, 60, 100, 128, out, sizeof out) == 0);
    CHECK(jdl_step_edit(&d, 13, 0, 0, JDL_STEP_NOTE, 60, 100, 127, out, sizeof out) > 0);
}

static void test_steps_need_live_control(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(jdl_steps_read(&d, 0, 0, 0, out, sizeof out) == 0 && jdl_step_edit(&d, 0, 0, 0, JDL_STEP_TRIG, 0, 255, 255, out, sizeof out) == 0);
}

static void test_a_refused_or_full_step_request_changes_nothing_else(void)
{
    struct jdl d = live(0);
    uint8_t out[64];
    CHECK(jdl_step_edit(&d, 10, 0, 0, JDL_STEP_TRIG, 0, 255, 255, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_STEP, 5, NULL, 0);                                           /* 16 writes already waiting */
    CHECK(d.step_busy == 1 && jdl_is_live(&d));
    reply_to(&d, d.seq, JDL_CMD_STEP, 2, NULL, 0);                                           /* refused */
    CHECK(d.step_refused == 1 && jdl_is_live(&d) && jdl_can_play(&d));
    reply_to(&d, d.seq, JDL_CMD_STEPS, 2, NULL, 0);                                          /* a page that does not exist */
    CHECK(d.step_refused == 2 && jdl_is_live(&d));
    reply_to(&d, d.seq, JDL_CMD_STEP, 4, NULL, 0);                                           /* live control itself ended: that is the state's business */
    CHECK(!jdl_is_live(&d));
}

static void test_a_message_for_the_digitones_screen(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[40];
    size_t n = jdl_message(&d, 10, "KEY C#", 10, out, sizeof out);
    CHECK(jdl_can_message(&d) && n > 0 && request_body(out, n, body) == 3 + 1 + 6 && body[2] == JDL_CMD_MESSAGE && body[3] == 10 && memcmp(body + 4, "KEY C#", 6) == 0);
    n = jdl_message(&d, 11, "TWENTY CHARACTERS 20", 0, out, sizeof out);          /* the longest: 20 characters, duration 0 = the Digitone's default */
    CHECK(n > 0 && request_body(out, n, body) == 3 + 1 + 20 && body[3] == 0 && memcmp(body + 4, "TWENTY CHARACTERS 20", 20) == 0);
    CHECK(jdl_message(&d, 12, "TWENTY ONE CHARACTERS", 0, out, sizeof out) == 0);   /* 21: the Digitone would refuse it */
    CHECK(jdl_message(&d, 12, "", 0, out, sizeof out) == 0 && jdl_message(&d, 12, NULL, 0, out, sizeof out) == 0);
    CHECK(jdl_message(&d, 12, "TAB\tHERE", 0, out, sizeof out) == 0 && jdl_message(&d, 12, "\xC3\xA9", 0, out, sizeof out) == 0);   /* outside 0x20..0x7e */
    CHECK(jdl_message(&d, 12, " ~", 0, out, sizeof out) > 0);                      /* the edges of the range are fine */
    CHECK(jdl_message(&d, 13, "SHORT", 5, out, 8) == 0);                           /* no room: nothing */
}

static void test_a_message_needs_live_control_and_a_digitone_that_has_it(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(!jdl_can_message(&d) && jdl_message(&d, 0, "HI", 0, out, sizeof out) == 0);   /* not live */
    d = live(0);
    CHECK(jdl_message(&d, 10, "HI", 0, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_MESSAGE, 0, NULL, 0);                                  /* shown: nothing changes */
    CHECK(jdl_can_message(&d) && jdl_is_live(&d));
    CHECK(jdl_message(&d, 20, "HI", 0, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_MESSAGE, 2, NULL, 0);                                  /* a Digitone build without MESSAGE (stage 4): never again */
    CHECK(!jdl_can_message(&d) && jdl_is_live(&d) && jdl_can_play(&d) && jdl_message(&d, 30, "HI", 0, out, sizeof out) == 0);
    d = live(0);
    CHECK(jdl_message(&d, 10, "HI", 0, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_MESSAGE, 4, NULL, 0);                                  /* the lease ran out: live control asked for again, MESSAGE not given up */
    CHECK(!jdl_is_live(&d) && d.msg_ok);
}

/* a MIXER answer: mute tracks 1 and 3, solo track 16, active track 5; track i: VOL 10 + i, PAN 64 + i, DEL 20 + i, REV 30 + i, CHR 40 + i, LEVEL 50 + i */
static size_t mixer_data(uint8_t *data)
{
    size_t k = 0;
    const uint32_t mute = 0x5u, solo = 0x8000u;
    for (int s = 24; s >= 0; s -= 8)
        data[k++] = (uint8_t)(mute >> s);
    for (int s = 24; s >= 0; s -= 8)
        data[k++] = (uint8_t)(solo >> s);
    data[k++] = 5;
    data[k++] = 16;
    for (uint8_t i = 0; i < 16; i++) {
        data[k++] = (uint8_t)(10 + i);
        data[k++] = (uint8_t)(64 + i);
        data[k++] = (uint8_t)(20 + i);
        data[k++] = (uint8_t)(30 + i);
        data[k++] = (uint8_t)(40 + i);
        data[k++] = (uint8_t)(50 + i);
    }
    return k;
}

static void test_reading_the_mixer(void)
{
    struct jdl d = live(0);
    struct jdl_mixer m;
    uint8_t out[64], body[32], data[110];
    size_t n = jdl_mixer_read(&d, 10, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 3 && body[2] == JDL_CMD_MIXER);               /* no arguments */
    CHECK(!jdl_take_mixer(&d, &m));
    CHECK(mixer_data(data) == 106);
    reply_to(&d, d.seq, JDL_CMD_MIXER, 0, data, 106);
    CHECK(jdl_take_mixer(&d, &m) && m.mute == 5u && m.solo == 0x8000u && m.active == 5);
    CHECK(m.t[0].vol == 10 && m.t[0].pan == 64 && m.t[0].del == 20 && m.t[0].rev == 30 && m.t[0].chr == 40 && m.t[0].level == 50);
    CHECK(m.t[15].vol == 25 && m.t[15].pan == 79 && m.t[15].level == 65);
    CHECK(!jdl_take_mixer(&d, &m) && jdl_is_live(&d));                                         /* taken once */
}

static void test_a_damaged_or_refused_mixer_answer_is_not_taken(void)
{
    struct jdl d = live(0);
    struct jdl_mixer m;
    uint8_t data[110];
    mixer_data(data);
    reply_to(&d, d.seq, JDL_CMD_MIXER, 0, data, 105);                                          /* one byte short */
    CHECK(!jdl_take_mixer(&d, &m));
    data[8] = 16;                                                                              /* an active track that does not exist */
    reply_to(&d, d.seq, JDL_CMD_MIXER, 0, data, 106);
    CHECK(!jdl_take_mixer(&d, &m));
    data[8] = 5;
    data[9] = 8;                                                                               /* a different number of tracks: not what this reads */
    reply_to(&d, d.seq, JDL_CMD_MIXER, 0, data, 106);
    CHECK(!jdl_take_mixer(&d, &m));
    reply_to(&d, d.seq, JDL_CMD_MIXER, 2, NULL, 0);                                            /* refused: live control itself is fine */
    CHECK(!jdl_take_mixer(&d, &m) && jdl_is_live(&d) && jdl_can_play(&d));
    reply_to(&d, d.seq, JDL_CMD_MIXER, 7, NULL, 0);                                            /* REMOTE CONTROL turned off: that is the state's business */
    CHECK(!jdl_is_live(&d));
}

static void test_no_mixer_read_unless_live(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(jdl_mixer_read(&d, 0, out, sizeof out) == 0);
}

/* a LIVE STATUS of protocol `proto` (so that SCALE knows whether the root bytes are there) */
static void status_of_protocol(struct jdl *d, uint8_t proto)
{
    uint8_t st[31] = {0};
    st[0] = proto;
    st[1] = 1;
    st[2] = 10;
    st[3] = 64;
    st[4] = 16;
    jdl_poll(d, 1000000, (uint8_t[64]){0}, 64);
    reply_to(d, d->seq, JDL_CMD_LIVE, 0, st, sizeof st);
}

static void test_reading_the_scale_with_the_cheap_poll(void)
{
    struct jdl d = live(0);
    struct jdl_scale s;
    uint8_t out[64], body[32], data[24];
    size_t n = jdl_scale_read(&d, 10, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[2] == JDL_CMD_SCALE && body[3] == 255 && body[4] == 35 && body[5] == 1);   /* the active track, the last name, short */
    CHECK(!jdl_take_scale(&d, &s));
    status_of_protocol(&d, 4);                                                   /* stage 5: no root bytes */
    data[0] = 0; data[1] = 6; data[2] = 1; data[3] = 36; data[4] = 35; data[5] = 1;
    data[6] = 5; data[7] = 'L'; data[8] = 'o'; data[9] = 'c'; data[10] = 'r'; data[11] = '.';   /* names from data[6]: a length byte, not a root */
    reply_to(&d, d.seq, JDL_CMD_SCALE, 0, data, 12);
    CHECK(jdl_take_scale(&d, &s) && s.track == 0 && s.index == 6 && s.per_track == 1 && !s.has_root);
    CHECK(!jdl_take_scale(&d, &s) && jdl_is_live(&d));                           /* taken once */
}

static void test_the_root_comes_with_protocol_five(void)
{
    struct jdl d = live(0);
    struct jdl_scale s;
    uint8_t data[24];
    status_of_protocol(&d, 5);
    data[0] = 3; data[1] = 20; data[2] = 0; data[3] = 36; data[4] = 35; data[5] = 1;
    data[6] = 9; data[7] = 2;                                                    /* root A, key mode 2; the name follows from data[8] */
    data[8] = 4; data[9] = 'T'; data[10] = 'e'; data[11] = 's'; data[12] = 't';
    reply_to(&d, d.seq, JDL_CMD_SCALE, 0, data, 13);
    CHECK(jdl_take_scale(&d, &s) && s.index == 20 && s.per_track == 0 && s.has_root && s.root == 9 && s.key_mode == 2);
    data[6] = 12;                                                                /* a root that does not exist: not taken as one */
    reply_to(&d, d.seq, JDL_CMD_SCALE, 0, data, 13);
    CHECK(jdl_take_scale(&d, &s) && !s.has_root);
    reply_to(&d, d.seq, JDL_CMD_SCALE, 0, data, 7);                              /* the header only, no root bytes at all: the scale still counts */
    CHECK(jdl_take_scale(&d, &s) && s.index == 20 && !s.has_root);
}

static void test_a_damaged_or_refused_scale_answer(void)
{
    struct jdl d = live(0);
    struct jdl_scale s;
    uint8_t data[24] = {0, 36, 0, 36, 35, 1, 0, 0};
    reply_to(&d, d.seq, JDL_CMD_SCALE, 0, data, 5);                              /* shorter than its header */
    CHECK(!jdl_take_scale(&d, &s));
    reply_to(&d, d.seq, JDL_CMD_SCALE, 0, data, 8);                              /* a scale index of 36: not one */
    CHECK(!jdl_take_scale(&d, &s));
    reply_to(&d, d.seq, JDL_CMD_SCALE, 2, NULL, 0);                              /* a build without SCALE: it is not asked again, live control is fine */
    CHECK(!d.scale_ok && jdl_is_live(&d) && jdl_can_play(&d));
    CHECK(jdl_scale_read(&d, 1, (uint8_t[64]){0}, 64) == 0);
}

static void test_setting_the_scale(void)
{
    struct jdl d = live(0);
    struct jdl_scale s;
    uint8_t out[64], body[32], data[8];
    size_t n = jdl_scale_set(&d, 10, 11, 9, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[2] == JDL_CMD_SCALE_SET && body[3] == 255 && body[4] == 11 && body[5] == 9);   /* the active track, scale 11, root A */
    n = jdl_scale_set(&d, 11, 0, JDL_ROOT_UNCHANGED, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[4] == 0 && body[5] == 255);   /* Chromatic, the root as it is */
    CHECK(jdl_scale_set(&d, 12, 36, 0, out, sizeof out) == 0);                   /* what the Digitone would refuse is not sent */
    CHECK(jdl_scale_set(&d, 12, 5, 12, out, sizeof out) == 0);
    CHECK(jdl_scale_set(&d, 12, 5, 254, out, sizeof out) == 0);
    data[0] = 0; data[1] = 11; data[2] = 9;
    reply_to(&d, d.seq, JDL_CMD_SCALE_SET, 0, data, 3);                          /* its answer: scale 11, root 9 */
    CHECK(jdl_take_scale(&d, &s) && s.index == 11 && s.has_root && s.root == 9);
    reply_to(&d, d.seq, JDL_CMD_SCALE_SET, 2, NULL, 0);                          /* an older build: no more writes, nothing else changes */
    CHECK(!d.scale_set_ok && d.scale_ok && jdl_is_live(&d));
    CHECK(jdl_scale_set(&d, 20, 1, 0, out, sizeof out) == 0);
}

static void test_a_refused_write_on_a_build_that_has_scale_set_is_not_the_end_of_it(void)
{
    struct jdl d = live(0);
    uint8_t out[64];
    status_of_protocol(&d, 5);                                                   /* stage 6: SCALE SET exists */
    CHECK(jdl_scale_set(&d, 10, 3, 0, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_SCALE_SET, 2, NULL, 0);                          /* refused: the screen shows a pattern that is not the playing one */
    CHECK(d.scale_set_ok && d.scale_set_refused == 1 && jdl_is_live(&d));
    CHECK(jdl_scale_set(&d, 20, 4, 0, out, sizeof out) > 0);                     /* the next write may still go */
    reply_to(&d, d.seq, JDL_CMD_SCALE_SET, 2, NULL, 0);
    CHECK(d.scale_set_ok && d.scale_set_refused == 2);
    d = live(0);
    status_of_protocol(&d, 4);                                                   /* stage 5: no SCALE SET: status 2 means that */
    reply_to(&d, d.seq, JDL_CMD_SCALE_SET, 2, NULL, 0);
    CHECK(!d.scale_set_ok && d.scale_set_refused == 0);
}

static void test_setting_a_mixer_value_and_the_mute_mask(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[32];
    size_t n = jdl_mixer_set(&d, 10, 3, JDL_MIX_PAN, 100, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[2] == JDL_CMD_MIXER_SET && body[3] == 3 && body[4] == 1 && body[5] == 100);
    n = jdl_mixer_set(&d, 11, 255, JDL_MIX_LEVEL, 0, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[3] == 255 && body[4] == 5 && body[5] == 0);       /* the active track, LEVEL, 0 */
    CHECK(jdl_mixer_set(&d, 12, 16, JDL_MIX_VOL, 1, out, sizeof out) == 0);                                  /* what the Digitone would refuse is not sent */
    CHECK(jdl_mixer_set(&d, 12, 0, (enum jdl_mix_param)6, 1, out, sizeof out) == 0);
    CHECK(jdl_mixer_set(&d, 12, 0, JDL_MIX_VOL, 128, out, sizeof out) == 0);
    n = jdl_mute_set(&d, 13, 0, 0x8005u, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[2] == JDL_CMD_MUTE_SET && body[3] == 0 && body[4] == 0x80 && body[5] == 0x05);   /* mute, tracks 1, 3 and 16 */
    n = jdl_mute_set(&d, 14, 1, 0, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[3] == 1 && body[4] == 0 && body[5] == 0);          /* solo: no track */
    CHECK(jdl_mute_set(&d, 15, 2, 1, out, sizeof out) == 0);
    CHECK(jdl_mixer_set(&d, 16, 0, JDL_MIX_VOL, 5, out, 8) == 0);                                            /* no room: nothing */
    /* LIVE ERASE (44 sub 3, a u16 mask of tracks, big-endian; the Digitone's own answer to the Delete button, dn2-wishes) */
    n = jdl_erase(&d, 17, 1u << 5, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[2] == JDL_CMD_LIVE && body[3] == 3 && body[4] == 0x00 && body[5] == 0x20);   /* track 6 */
    n = jdl_erase(&d, 18, 1u << 15, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[3] == 3 && body[4] == 0x80 && body[5] == 0x00);   /* track 16 */
    n = jdl_erase(&d, 19, 0, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 6 && body[3] == 3 && body[4] == 0 && body[5] == 0);          /* let go: all off */
    CHECK(jdl_erase(&d, 20, 1, out, 8) == 0);                                                                /* no room: nothing */
}

static void test_erase_needs_live_control(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(jdl_erase(&d, 1, 1, out, sizeof out) == 0);              /* not live: nothing is sent */
}

static void test_the_mixer_writes_stop_on_a_build_without_them_but_not_on_one_that_has_them(void)
{
    struct jdl d = live(0);
    uint8_t out[64];
    status_of_protocol(&d, 4);                                                   /* stage 5: no mixer writes */
    CHECK(jdl_mixer_set(&d, 10, 0, JDL_MIX_VOL, 5, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_MIXER_SET, 2, NULL, 0);
    CHECK(!d.mix_set_ok && d.mix_set_refused == 0 && jdl_is_live(&d));
    CHECK(jdl_mixer_set(&d, 11, 0, JDL_MIX_VOL, 5, out, sizeof out) == 0 && jdl_mute_set(&d, 11, 0, 1, out, sizeof out) == 0);   /* neither goes any more */
    d = live(0);
    status_of_protocol(&d, 5);                                                   /* stage 6 */
    CHECK(jdl_mute_set(&d, 10, 0, 1, out, sizeof out) > 0);
    reply_to(&d, d.seq, JDL_CMD_MUTE_SET, 5, NULL, 0);                           /* 16 writes waiting: this one refused */
    CHECK(d.mix_set_ok && d.mix_set_refused == 1);
    reply_to(&d, d.seq, JDL_CMD_MIXER_SET, 2, NULL, 0);                          /* refused at the pass */
    CHECK(d.mix_set_ok && d.mix_set_refused == 2 && jdl_is_live(&d));
    reply_to(&d, d.seq, JDL_CMD_MIXER_SET, 0, NULL, 0);                          /* done */
    CHECK(d.mix_set_ok && d.mix_set_refused == 2);
}

/* RECORD (58, dn2-wishes stage 7): the request, the refusals that do not end live control, and the record byte of LIVE STATUS protocol 6 (73 bytes, byte 72) */
static void test_record(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[16], st[73] = {0};
    size_t n;
    n = jdl_record(&d, 10, 1, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[2] == JDL_CMD_RECORD && body[3] == 1);              /* arm */
    n = jdl_record(&d, 11, 0, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[3] == 0);                                           /* off */
    n = jdl_record(&d, 12, 255, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[3] == 255);                                         /* read */
    CHECK(jdl_record(&d, 13, 2, out, sizeof out) == 0);                                                        /* a value the Digitone would refuse */
    CHECK(jdl_record(&d, 14, 1, out, 4) == 0);                                                                 /* no room: nothing */
    reply_to(&d, d.seq, JDL_CMD_RECORD, 6, NULL, 0);                                                           /* GRID or STEP RECORDING is on */
    CHECK(d.record_ok && d.record_busy == 1 && jdl_is_live(&d));
    reply_to(&d, d.seq, JDL_CMD_RECORD, 0, (uint8_t[]){1}, 1);                                                 /* the value asked */
    CHECK(d.record_ok && d.record_busy == 1 && jdl_is_live(&d));
    reply_to(&d, d.seq, JDL_CMD_RECORD, 2, NULL, 0);                                                           /* a build without RECORD */
    CHECK(!d.record_ok && jdl_is_live(&d));                                                                    /* live control is fine */
    CHECK(jdl_record(&d, 20, 1, out, sizeof out) == 0);                                                        /* none is sent any more */
    d = live(0);
    status_of_protocol(&d, 5);
    CHECK(!d.st.rec_valid && d.st.rec_bits == 0);                                                              /* protocol 5: no record byte */
    st[0] = 6;
    st[1] = 1;
    st[2] = 10;
    st[72] = JDL_REC_ARMED | JDL_REC_GRID;
    jdl_poll(&d, 2000000, (uint8_t[64]){0}, 64);
    reply_to(&d, d.seq, JDL_CMD_LIVE, 0, st, sizeof st);
    CHECK(d.st.proto == 6 && d.st.rec_valid && d.st.rec_bits == (JDL_REC_ARMED | JDL_REC_GRID));
    st[72] = JDL_REC_RECORDING;
    jdl_poll(&d, 3000000, (uint8_t[64]){0}, 64);
    reply_to(&d, d.seq, JDL_CMD_LIVE, 0, st, sizeof st);
    CHECK(d.st.rec_valid && d.st.rec_bits == JDL_REC_RECORDING);
    st[0] = 6;
    jdl_poll(&d, 4000000, (uint8_t[64]){0}, 64);
    reply_to(&d, d.seq, JDL_CMD_LIVE, 0, st, 72);                                                              /* a protocol 6 answer too short for the byte */
    CHECK(!d.st.rec_valid);
    d = live(0);
    jdl_init(&d);
    CHECK(jdl_record(&d, 1, 1, out, sizeof out) == 0);                                                         /* not live: nothing */
}

/* PATTERNS (51, the Digitone's stage 4): the request per bank or for the playing pattern's bank, the answer, a damaged one, a build without it */
static void test_reading_the_patterns(void)
{
    struct jdl d = live(0);
    struct jdl_patterns p;
    uint8_t out[64], body[16];
    size_t n = jdl_patterns_read(&d, 10, 3, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[2] == JDL_CMD_PATTERNS && body[3] == 3);            /* bank D */
    n = jdl_patterns_read(&d, 11, JDL_BANK_PLAYING, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[3] == 255);                                         /* the playing pattern's bank */
    CHECK(jdl_patterns_read(&d, 12, 8, out, sizeof out) == 0);                                                 /* no bank I */
    CHECK(!jdl_take_patterns(&d, &p));
    reply_to(&d, d.seq, JDL_CMD_PATTERNS, 0, (uint8_t[]){2, 37, 0x80, 0x21}, 4);                                /* bank C, C06 playing, C01 C06 C16 hold trigs */
    CHECK(jdl_take_patterns(&d, &p) && p.bank == 2 && p.playing == 37 && p.mask == 0x8021u);
    CHECK(!jdl_take_patterns(&d, &p));                                                                         /* taken once */
    reply_to(&d, d.seq, JDL_CMD_PATTERNS, 0, (uint8_t[]){2, 37, 0x80}, 3);                                     /* one byte short */
    reply_to(&d, d.seq, JDL_CMD_PATTERNS, 0, (uint8_t[]){8, 37, 0, 0}, 4);                                     /* a bank that does not exist */
    reply_to(&d, d.seq, JDL_CMD_PATTERNS, 0, (uint8_t[]){2, 128, 0, 0}, 4);                                    /* a pattern that does not exist */
    CHECK(!jdl_take_patterns(&d, &p));
    reply_to(&d, d.seq, JDL_CMD_PATTERNS, 2, NULL, 0);                                                         /* refused: that read stops, live control is fine */
    CHECK(!d.patterns_ok && jdl_is_live(&d) && jdl_can_play(&d) && d.pattern_ok);
    CHECK(jdl_patterns_read(&d, 20, 0, out, sizeof out) == 0);
    jdl_init(&d);
    CHECK(jdl_patterns_read(&d, 1, 0, out, sizeof out) == 0);                                                  /* not live: nothing */
}

/* PATTERN (60, provisional): cue, jump, read; the answer; the refusal of a build without it (as RECORD's) */
static void test_selecting_a_pattern(void)
{
    struct jdl d = live(0);
    struct jdl_pattern p;
    uint8_t out[64], body[16];
    size_t n = jdl_pattern(&d, 10, 17, JDL_PATTERN_CUE, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 5 && body[2] == JDL_CMD_PATTERN && body[3] == 17 && body[4] == 0);   /* B02, cued */
    n = jdl_pattern(&d, 11, 127, JDL_PATTERN_JUMP, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 5 && body[3] == 127 && body[4] == 1);
    n = jdl_pattern(&d, 12, 99, JDL_PATTERN_READ, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 5 && body[3] == 0 && body[4] == 255);                         /* a read: the pattern byte is 0 */
    CHECK(jdl_pattern(&d, 13, 128, JDL_PATTERN_CUE, out, sizeof out) == 0 && jdl_pattern(&d, 13, 1, 2, out, sizeof out) == 0);   /* what the Digitone would refuse */
    reply_to(&d, d.seq, JDL_CMD_PATTERN, 0, (uint8_t[]){5, 17}, 2);
    CHECK(jdl_take_pattern(&d, &p) && p.playing == 5 && p.next == 17 && !jdl_take_pattern(&d, &p));
    reply_to(&d, d.seq, JDL_CMD_PATTERN, 0, (uint8_t[]){17, 255}, 2);                                          /* nothing cued */
    CHECK(jdl_take_pattern(&d, &p) && p.playing == 17 && p.next == JDL_NO_PATTERN);
    reply_to(&d, d.seq, JDL_CMD_PATTERN, 0, (uint8_t[]){17, 200}, 2);                                          /* damaged: not taken */
    reply_to(&d, d.seq, JDL_CMD_PATTERN, 0, (uint8_t[]){17}, 1);
    CHECK(!jdl_take_pattern(&d, &p));
    reply_to(&d, d.seq, JDL_CMD_PATTERN, 2, NULL, 0);                                                          /* a build without PATTERN */
    CHECK(!d.pattern_ok && d.patterns_ok && jdl_is_live(&d));                                                  /* live control and the reads go on */
    CHECK(jdl_pattern(&d, 20, 3, JDL_PATTERN_CUE, out, sizeof out) == 0 && jdl_patterns_read(&d, 20, 0, out, sizeof out) > 0);
    jdl_init(&d);                                                                                              /* live control asked for afresh: tried again */
    CHECK(d.pattern_ok && jdl_pattern(&d, 21, 3, JDL_PATTERN_CUE, out, sizeof out) == 0);                      /* (not live yet) */
}

/* TRANSPORT (61, stage 8, provisional): the ops, the answer, the probe once per connection, the refusal (as RECORD's) */
static void test_transport(void)
{
    struct jdl d = live(0);
    uint8_t out[64], body[16];
    size_t n = jdl_transport(&d, 10, JDL_TR_PLAY, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[2] == JDL_CMD_TRANSPORT && body[3] == 1);
    n = jdl_transport(&d, 11, JDL_TR_STOP, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[3] == 0);
    n = jdl_transport(&d, 12, JDL_TR_PAUSE, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[3] == 2);
    CHECK(jdl_transport(&d, 13, 3, out, sizeof out) == 0);                                                     /* no op 3 */
    CHECK(jdl_take_transport(&d) == -1);
    reply_to(&d, d.seq, JDL_CMD_TRANSPORT, 0, (uint8_t[]){JDL_TR_PAUSED}, 1);
    CHECK(jdl_take_transport(&d) == JDL_TR_PAUSED && jdl_take_transport(&d) == -1);                           /* once */
    reply_to(&d, d.seq, JDL_CMD_TRANSPORT, 0, (uint8_t[]){7}, 1);                                              /* not a state */
    CHECK(jdl_take_transport(&d) == -1);
    n = jdl_probe_transport(&d, 20, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[3] == 255 && jdl_probe_transport(&d, 21, out, sizeof out) == 0);   /* a read, once */
    reply_to(&d, d.seq, JDL_CMD_TRANSPORT, 2, NULL, 0);                                                        /* a build without it */
    CHECK(!d.transport_ok && jdl_is_live(&d) && jdl_transport(&d, 30, JDL_TR_PLAY, out, sizeof out) == 0);
    jdl_init(&d);
    CHECK(d.transport_ok && !d.transport_probed && jdl_probe_transport(&d, 1, out, sizeof out) == 0);          /* afresh, but not before live */
}

/* CHAIN (59, stage 8, provisional): SET's bytes (2..64 patterns, the longest request there is), CLEAR, READ, the answer, a full queue, a build without it */
static void test_chain(void)
{
    struct jdl d = live(0);
    struct jdl_chain c;
    uint8_t out[128], body[96], pats[JDL_CHAIN_MAX + 1], data[JDL_CHAIN_HEAD + 3];
    size_t n = jdl_chain_set(&d, 10, (const uint8_t[]){0, 17, 127}, 3, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 8 && body[2] == JDL_CMD_CHAIN && body[3] == 0 && body[4] == 3 && body[5] == 0 && body[6] == 17 && body[7] == 127);
    for (unsigned i = 0; i <= JDL_CHAIN_MAX; i++)
        pats[i] = (uint8_t)i;
    n = jdl_chain_set(&d, 11, pats, JDL_CHAIN_MAX, out, sizeof out);                                           /* 64: 66 argument bytes */
    CHECK(n > 0 && n <= 96u && request_body(out, n, body) == 3u + 2u + JDL_CHAIN_MAX && body[4] == 64 && body[3 + 2 + 63] == 63);
    CHECK(jdl_chain_set(&d, 12, pats, JDL_CHAIN_MAX + 1u, out, sizeof out) == 0 && jdl_chain_set(&d, 12, pats, 1, out, sizeof out) == 0);   /* 2..64 only */
    pats[1] = 128;
    CHECK(jdl_chain_set(&d, 12, pats, 2, out, sizeof out) == 0);                                               /* no pattern 128 */
    n = jdl_chain_clear(&d, 13, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 4 && body[3] == 1);
    n = jdl_chain_read(&d, 14, 8, out, sizeof out);
    CHECK(n > 0 && request_body(out, n, body) == 5 && body[3] == 255 && body[4] == 8);
    data[0] = 1;                                                                                               /* mode 1, 5 patterns, at the 2nd (position 1), from the 1st: 3 of them */
    data[1] = 0;
    data[2] = 5;
    data[3] = 0;
    data[4] = 1;
    data[5] = 0;
    data[6] = 3;
    data[7] = 4;
    data[8] = 9;
    data[9] = 120;
    reply_to(&d, d.seq, JDL_CMD_CHAIN, 0, data, sizeof data);
    CHECK(jdl_take_chain(&d, &c) && c.mode == 1 && c.count == 5 && c.pos == 1 && c.first == 0 && c.k == 3 && !jdl_take_chain(&d, &c));
    CHECK(c.member[0] == 0x10 && c.member[1] == 0x02 && c.member[15] == 0x01);                                  /* patterns 4, 9, 120 */
    reply_to(&d, d.seq, JDL_CMD_CHAIN, 0, data, sizeof data - 1u);                                             /* k says 3, 2 are there */
    CHECK(!jdl_take_chain(&d, &c));
    reply_to(&d, d.seq, JDL_CMD_CHAIN, 5, NULL, 0);                                                            /* the queue is full: that request only */
    CHECK(d.chain_ok && d.chain_busy == 1 && jdl_is_live(&d));
    reply_to(&d, d.seq, JDL_CMD_CHAIN, 2, NULL, 0);                                                            /* a build without CHAIN */
    CHECK(!d.chain_ok && jdl_is_live(&d) && jdl_chain_read(&d, 20, 0, out, sizeof out) == 0 && jdl_chain_clear(&d, 20, out, sizeof out) == 0);
}

/* PATTERN's stage-8 answer: playing, next (255 = the same), mode, running, jump armed */
static void test_the_pattern_answer_of_stage_8(void)
{
    struct jdl d = live(0);
    struct jdl_pattern p;
    reply_to(&d, d.seq, JDL_CMD_PATTERN, 0, (uint8_t[]){5, 255, 2, 1, 1}, 5);
    CHECK(jdl_take_pattern(&d, &p) && p.playing == 5 && p.next == JDL_NO_PATTERN && p.mode == 2 && p.running == 1 && p.armed == 1);
}

/* a LIVE STATUS of protocol `proto`, `len` bytes, with the protocol-7 bytes filled */
static void status7(struct jdl *d, uint32_t t, uint8_t proto, size_t len)
{
    uint8_t st[JDL_P7_COUNTERS_BYTES] = {0};
    st[0] = proto;
    st[1] = 1;
    st[2] = 10;
    st[72] = JDL_REC_ARMED;
    st[73] = JDL_TR_PLAYING;           /* transport */
    st[74] = 37;                       /* playing C06 */
    st[75] = 40;                       /* next C09 */
    st[76] = 1;                        /* mode */
    st[78] = 4;                        /* chain count 4 */
    st[80] = 2;                        /* chain position 2 */
    st[84] = 9;                        /* writes applied 9 */
    st[88] = 1;                        /* refused 1 */
    jdl_poll(d, t, (uint8_t[64]){0}, 64);
    reply_to(d, d->seq, JDL_CMD_LIVE, 0, st, len);
}

/* LIVE STATUS protocol 7 (stage 8, provisional): the 73 bytes, then transport, playing, next, mode, chain count and position, two counters; 5 and 6 as before; the status asked
 * every 250 ms once it says 7 */
static void test_the_status_of_protocol_7(void)
{
    struct jdl d = live(0);
    struct jdl_live7 s;
    uint8_t out[64];
    status7(&d, 5000, 7, JDL_P7_COUNTERS_BYTES);
    CHECK(d.st.proto == 7 && d.st.rec_valid && d.st.rec_bits == JDL_REC_ARMED);                               /* protocol 6's byte as before */
    CHECK(jdl_take_live7(&d, &s) && !jdl_take_live7(&d, &s));
    CHECK(s.transport == JDL_TR_PLAYING && s.playing == 37 && s.next == 40 && s.mode == 1 && s.chain_count == 4 && s.chain_pos == 2);
    CHECK(s.counters && s.applied == 9 && s.refused == 1);
    status7(&d, 10000, 7, JDL_P7_BYTES);                                                                       /* without the counters */
    CHECK(jdl_take_live7(&d, &s) && !s.counters && s.chain_count == 4);
    status7(&d, 15000, 7, JDL_P7_BYTES - 1u);                                                                  /* too short for the chain position: no protocol-7 part */
    CHECK(!d.st.p7_valid && !jdl_take_live7(&d, &s) && d.st.rec_valid);
    status7(&d, 20000, 6, JDL_P7_COUNTERS_BYTES);                                                              /* protocol 6, long: the bytes are not read */
    CHECK(!d.st.p7_valid && !jdl_take_live7(&d, &s) && d.st.rec_valid);
    CHECK(jdl_poll(&d, 20000 + JDL_STATUS_FAST_MS, out, sizeof out) == 0);                                     /* protocol 6: every 4 s */
    status7(&d, 30000, 7, JDL_P7_BYTES);
    CHECK(jdl_poll(&d, 30000 + JDL_STATUS_FAST_MS - 1u, out, sizeof out) == 0);
    CHECK(jdl_poll(&d, 30000 + JDL_STATUS_FAST_MS, out, sizeof out) > 0);                                      /* protocol 7: every 250 ms */
    jdl_pattern(&d, 30000 + 2u * JDL_STATUS_FAST_MS - 10u, 1, JDL_PATTERN_CUE, out, sizeof out);               /* (another request does not delay it) */
    CHECK(jdl_poll(&d, 30000 + 2u * JDL_STATUS_FAST_MS, out, sizeof out) > 0);
}

static void test_the_scale_needs_live_control(void)
{
    struct jdl d;
    uint8_t out[64];
    jdl_init(&d);
    CHECK(jdl_scale_read(&d, 0, out, sizeof out) == 0 && jdl_scale_set(&d, 0, 1, 0, out, sizeof out) == 0);
}

int main(void)
{
    test_erase_needs_live_control();
    test_record();
    test_reading_the_patterns();
    test_selecting_a_pattern();
    test_transport();
    test_chain();
    test_the_pattern_answer_of_stage_8();
    test_the_status_of_protocol_7();
    test_reading_the_scale_with_the_cheap_poll();
    test_the_root_comes_with_protocol_five();
    test_a_damaged_or_refused_scale_answer();
    test_setting_the_scale();
    test_a_refused_write_on_a_build_that_has_scale_set_is_not_the_end_of_it();
    test_setting_a_mixer_value_and_the_mute_mask();
    test_the_mixer_writes_stop_on_a_build_without_them_but_not_on_one_that_has_them();
    test_the_scale_needs_live_control();
    test_reading_the_mixer();
    test_a_damaged_or_refused_mixer_answer_is_not_taken();
    test_no_mixer_read_unless_live();
    test_a_message_for_the_digitones_screen();
    test_a_message_needs_live_control_and_a_digitone_that_has_it();
    test_reading_a_page_of_steps();
    test_a_damaged_steps_answer_is_not_taken();
    test_editing_a_step();
    test_steps_need_live_control();
    test_a_refused_or_full_step_request_changes_nothing_else();
    test_play_is_asked_about_once_when_live();
    test_a_digitone_without_play_is_known_before_the_first_press();
    test_play_is_asked_again_on_a_new_connection_and_not_before_live();
    test_the_digitones_own_counters();
    test_reading_the_active_track();
    test_a_read_sent_before_a_select_is_stale();
    test_the_sequence_wrapping_in_the_stale_rule();
    test_a_build_that_cannot_read_the_track();
    test_a_refused_select_is_not_the_end_of_live_control();
    test_no_read_unless_live();
    test_asks_for_live_control_until_answered();
    test_keepalive_while_live();
    test_track_and_play_are_requests_and_keep_the_lease();
    test_play_refused_by_a_digitone_without_it();
    test_remote_control_off();
    test_a_digitone_without_live_control();
    test_the_lease_running_out();
    test_a_stranger_and_noise();
    test_the_note_push();
    test_the_clock_wrapping();
    printf("test_dn2live: %d checks, %d failed\n", checks, fails);
    return fails;
}
