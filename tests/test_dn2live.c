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
    uint8_t f[64];
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

int main(void)
{
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
