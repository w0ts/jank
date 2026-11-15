/* SPDX-License-Identifier: GPL-3.0-only */
/* BLE-MIDI packet framing of the control line's SysEx: split across packets, read back, and packets as other senders
 * (iOS, Android, Web Bluetooth pages) may write them. Exit status: non-zero on a failure. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "jank_blemidi.h"
#include "jank_sysex.h"

struct sink {
    uint8_t bytes[1024];
    size_t n;
};

static void collect(void *ctx, uint8_t b)
{
    struct sink *s = ctx;
    assert(s->n < sizeof s->bytes);
    s->bytes[s->n++] = b;
}

/* msg through packets of payload cap and back -> the packets used */
static size_t round_trip(const uint8_t *msg, size_t n, size_t cap, uint16_t ms)
{
    uint8_t pkt[600];
    struct jbm_in in;
    struct sink s = {.n = 0};
    size_t at = 0, len, packets = 0;
    jbm_init(&in);
    while ((len = jbm_sysex_packet(msg, n, &at, ms, pkt, cap)) != 0) {
        assert(len <= cap);
        assert((pkt[0] & 0xC0u) == 0x80u && (pkt[0] & 0x3Fu) == ((ms >> 7) & 0x3Fu));
        assert(jbm_decode(&in, pkt, len, collect, &s) == 0);
        ++packets;
    }
    assert(at == n);
    assert(s.n == n && !memcmp(s.bytes, msg, n));
    assert(!in.in_sysex);
    assert(packets == jbm_sysex_packet_count(n, cap));
    return packets;
}

static void test_control_line_every_payload(void)
{
    uint8_t frame[JX_FRAME_MAX];
    size_t n = jx_frame("set tempo 120.00 and some more\n", 30, frame);
    for (size_t cap = JBM_PAYLOAD_MIN; cap <= 64; ++cap)
        round_trip(frame, n, cap, (uint16_t)(cap * 97u));
    assert(round_trip(frame, n, 20, 0) == 2);        /* the default ATT MTU of 23 */
    assert(round_trip(frame, n, 182, 0) == 1);       /* iOS's usual MTU of 185 */
}

static void test_beat_report(void)
{
    uint8_t frame[JX_PUSH_FRAME_MAX];
    char text[64];
    size_t t = jx_beat_line(text, sizeof text, 'b', 123456, 12345, 1, 1, -1234, 56789);
    size_t n = jx_push_frame(JX_PUSH_BEAT, text, t, frame);
    assert(t && n);
    assert(round_trip(frame, n, 20, 8191) == 3);
}

static void test_packet_layout(void)
{
    const uint8_t msg[] = {0xF0, 0x7D, 0x4A, 0x4B, 'o', 'k', 0xF7};
    uint8_t pkt[16];
    size_t at = 0;
    /* ms 0x1ABC: header 0x80 | 0x35, timestamp 0x80 | 0x3C */
    assert(jbm_sysex_packet(msg, sizeof msg, &at, 0x1ABC, pkt, sizeof pkt) == 10);
    const uint8_t want[] = {0xB5, 0xBC, 0xF0, 0x7D, 0x4A, 0x4B, 'o', 'k', 0xBC, 0xF7};
    assert(!memcmp(pkt, want, sizeof want) && at == sizeof msg);
    assert(jbm_sysex_packet(msg, sizeof msg, &at, 0, pkt, sizeof pkt) == 0);
}

static void test_f7_never_split_from_its_timestamp(void)
{
    const uint8_t msg[] = {0xF0, 0x7D, 0x4A, 0x4B, 'a', 0xF7};
    uint8_t pkt[8];
    size_t at = 0;
    /* payload 6: header ts F0 7D 4A 4B, then header 'a' ts F7 */
    assert(jbm_sysex_packet(msg, sizeof msg, &at, 5, pkt, 6) == 6 && at == 4);
    assert(jbm_sysex_packet(msg, sizeof msg, &at, 5, pkt, 6) == 4);
    assert(pkt[1] == 'a' && pkt[2] == 0x85 && pkt[3] == 0xF7);
    /* payload 5, then 3: header ts F0 7D 4A | header 4B 'a' | header ts F7, the F7 alone in a third packet */
    at = 0;
    assert(jbm_sysex_packet(msg, sizeof msg, &at, 5, pkt, 5) == 5 && at == 3);
    assert(jbm_sysex_packet(msg, sizeof msg, &at, 5, pkt, 3) == 3 && at == 5);
    assert(jbm_sysex_packet(msg, sizeof msg, &at, 5, pkt, 3) == 3 && pkt[1] == 0x85 && pkt[2] == 0xF7);
}

static void test_no_sysex_no_packets(void)
{
    const uint8_t note[] = {0x90, 0x3C, 0x40};
    const uint8_t high[] = {0xF0, 0x7D, 0x80, 0xF7};
    uint8_t pkt[16];
    size_t at = 0;
    assert(jbm_sysex_packet(note, sizeof note, &at, 0, pkt, sizeof pkt) == 0);
    assert(jbm_sysex_packet(high, sizeof high, &at, 0, pkt, sizeof pkt) == 0);
    at = 0;
    assert(jbm_sysex_packet(high, 0, &at, 0, pkt, 2) == 0);
}

static void decode_ok(struct jbm_in *in, const uint8_t *pkt, size_t n, struct sink *s)
{
    assert(jbm_decode(in, pkt, n, collect, s) == 0);
}

/* as iOS writes: a clock byte inside a SysEx with its own timestamp, and running status with and without timestamps */
static void test_other_writers(void)
{
    const uint8_t p1[] = {0x80, 0x81, 0xF0, 0x7D, 0x4A, 0x82, 0xF8, 0x4B, 'g'};
    const uint8_t p2[] = {0x80, 'e', 't', 0x0A, 0x83, 0xF7, 0x84, 0x90, 0x3C, 0x40, 0x3E, 0x41, 0x85, 0x40, 0x00};
    const uint8_t want[] = {0xF0, 0x7D, 0x4A, 0xF8, 0x4B, 'g', 'e', 't', 0x0A, 0xF7, 0x90, 0x3C, 0x40, 0x90, 0x3E, 0x41, 0x90, 0x40, 0x00};
    struct jbm_in in;
    struct sink s = {.n = 0};
    struct jx_in x;
    char text[8];
    size_t got = 0;
    jbm_init(&in);
    decode_ok(&in, p1, sizeof p1, &s);
    assert(in.in_sysex);
    decode_ok(&in, p2, sizeof p2, &s);
    assert(s.n == sizeof want && !memcmp(s.bytes, want, sizeof want));
    jx_init(&x);
    for (size_t i = 0; i < s.n; ++i) {
        int c = jx_feed(&x, s.bytes[i]);
        if (c >= 0)
            text[got++] = (char)c;
    }
    assert(got == 4 && !memcmp(text, "get\n", 4));
}

static void test_short_messages(void)
{
    /* program change (1 data byte), pitch bend (2), song select (1, clears running status), tune request (0) */
    const uint8_t p[] = {0x80, 0x80, 0xC1, 0x05, 0x06, 0x81, 0xE0, 0x00, 0x40, 0x82, 0xF3, 0x02, 0x82, 0xF6};
    const uint8_t want[] = {0xC1, 0x05, 0xC1, 0x06, 0xE0, 0x00, 0x40, 0xF3, 0x02, 0xF6};
    struct jbm_in in;
    struct sink s = {.n = 0};
    jbm_init(&in);
    decode_ok(&in, p, sizeof p, &s);
    assert(s.n == sizeof want && !memcmp(s.bytes, want, sizeof want));
    assert(in.running == 0);
}

static void test_malformed(void)
{
    struct jbm_in in;
    struct sink s = {.n = 0};
    const uint8_t no_header[] = {0x40, 0x80, 0xF8};
    const uint8_t lone_ts[] = {0x80, 0x80};
    const uint8_t no_running[] = {0x80, 0x3C, 0x40};
    const uint8_t cut_note[] = {0x80, 0x80, 0x90, 0x3C};
    const uint8_t start[] = {0x80, 0x80, 0xF0, 0x7D};
    jbm_init(&in);
    assert(jbm_decode(&in, no_header, sizeof no_header, collect, &s) == -1 && s.n == 0);
    assert(jbm_decode(&in, lone_ts, sizeof lone_ts, collect, &s) == -1);
    assert(jbm_decode(&in, no_running, sizeof no_running, collect, &s) == -1);
    assert(jbm_decode(&in, cut_note, sizeof cut_note, collect, &s) == -1 && in.running == 0);
    assert(jbm_decode(&in, NULL, 0, collect, &s) == -1);
    /* a fault in the middle of a SysEx ends it: the next packet's data is not taken as SysEx */
    s.n = 0;
    decode_ok(&in, start, sizeof start, &s);
    assert(in.in_sysex);
    assert(jbm_decode(&in, no_header, sizeof no_header, collect, &s) == -1 && !in.in_sysex);
    /* a stray F7 is dropped; an empty packet (header only) is fine */
    {
        const uint8_t stray[] = {0x80, 0x80, 0xF7};
        const uint8_t empty[] = {0x80};
        s.n = 0;
        decode_ok(&in, stray, sizeof stray, &s);
        decode_ok(&in, empty, sizeof empty, &s);
        assert(s.n == 0);
    }
}

/* the sender's rule (firmware/main/ble_midi.c, page/jank.html bleSend): whole messages packed while they fit, a packet
 * sent when the next one does not, a message too big for an empty packet split on its own -> packets in out, their count */
struct packets {
    uint8_t p[64][600];
    size_t n[64];
    size_t count;
};

static void flush(struct packets *o, const uint8_t *pkt, size_t len)
{
    assert(o->count < 64 && len <= sizeof o->p[0]);
    memcpy(o->p[o->count], pkt, len);
    o->n[o->count++] = len;
}

static size_t send_all(const uint8_t *const *msgs, const size_t *lens, size_t count, size_t cap, uint16_t ms, struct packets *o)
{
    uint8_t pkt[600];
    size_t len = 0;
    o->count = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t at = 0, l;
        if (jbm_pack(msgs[i], lens[i], ms, pkt, &len, cap))
            continue;
        if (len) {
            flush(o, pkt, len);
            len = 0;
        }
        if (jbm_pack(msgs[i], lens[i], ms, pkt, &len, cap))
            continue;
        while ((l = jbm_sysex_packet(msgs[i], lens[i], &at, ms, pkt, cap)) != 0)
            flush(o, pkt, l);
    }
    if (len)
        flush(o, pkt, len);
    return o->count;
}

/* the text of a control line's frames, through packets and back */
static void check_lines(const char *text, size_t cap, size_t want_packets)
{
    static uint8_t frames[40][JX_FRAME_MAX];
    const uint8_t *msgs[40];
    size_t lens[40], count = 0, n = strlen(text);
    static struct packets o;
    struct jbm_in in;
    struct sink s = {.n = 0};
    struct jx_in x;
    char back[1024];
    size_t got = 0;
    for (size_t at = 0; at < n; at += JX_TEXT_MAX, ++count) {
        size_t k = n - at < JX_TEXT_MAX ? n - at : JX_TEXT_MAX;
        lens[count] = jx_frame(text + at, k, frames[count]);
        msgs[count] = frames[count];
    }
    assert(send_all(msgs, lens, count, cap, 77, &o) == want_packets);
    jbm_init(&in);
    jx_init(&x);
    for (size_t i = 0; i < o.count; ++i) {
        assert(o.n[i] <= cap);
        decode_ok(&in, o.p[i], o.n[i], &s);
    }
    for (size_t i = 0; i < s.n; ++i) {
        int c = jx_feed(&x, s.bytes[i]);
        if (c >= 0)
            back[got++] = (char)c;
    }
    assert(got == n && !memcmp(back, text, n));
}

static void test_pack(void)
{
    const uint8_t a[] = {0xF0, 0x7D, 0x4A, 0x4B, 'o', 'k', 0xF7};
    const uint8_t note[] = {0x90, 0x3C, 0x40};
    uint8_t pkt[32];
    size_t len = 0;
    /* two whole messages in one packet: header, then ts F0 .. ts F7 each */
    assert(jbm_pack(a, sizeof a, 0x1ABC, pkt, &len, sizeof pkt) == 1 && len == 10);
    assert(jbm_pack(a, sizeof a, 0x1ABC, pkt, &len, sizeof pkt) == 1 && len == 19);
    const uint8_t want[] = {0xB5, 0xBC, 0xF0, 0x7D, 0x4A, 0x4B, 'o', 'k', 0xBC, 0xF7, 0xBC, 0xF0, 0x7D, 0x4A, 0x4B, 'o', 'k', 0xBC, 0xF7};
    assert(!memcmp(pkt, want, sizeof want));
    assert(jbm_pack(a, sizeof a, 0x1ABC, pkt, &len, 27) == 0 && len == 19);   /* 19 + 9 > 27: unchanged */
    assert(jbm_pack(a, sizeof a, 0x1ABC, pkt, &len, 28) == 1 && len == 28);   /* exactly full */
    len = 0;
    assert(jbm_pack(a, sizeof a, 0, pkt, &len, 9) == 0 && len == 0);           /* 1 + 7 + 2 = 10 */
    assert(jbm_pack(a, sizeof a, 0, pkt, &len, 10) == 1);
    len = 0;
    assert(jbm_pack(note, sizeof note, 0, pkt, &len, sizeof pkt) == 0);        /* SysEx only */
    /* answers as they go out: `get` (about 250 characters) in 9 frames */
    check_lines("ok\n", 182, 1);
    check_lines("group=1\nchannel=1\nname=jank\nfilter=link\npeer=on\ntransport=link\nquantum=4\noffset=0.00\nbeat=on\n"
                "beat_latency=25\nble=pair\nnet=jank\nssid=\npass=none\nmac=02:6a:6b:aa:bb:cc\nversion=0.1.0\nok\n",
                182, 2);                                                       /* 7 frames of 37 = 4 a packet of 182 */
    check_lines("group=1\nchannel=1\nname=jank\nfilter=link\nok\n", 20, 4);  /* the default MTU: each frame split in two */
    check_lines("x\n", 10, 1);                                                 /* 1 + 7 + 2 */
}

static void hex_array(FILE *f, const uint8_t *b, size_t n)
{
    fputc('[', f);
    for (size_t i = 0; i < n; ++i)
        fprintf(f, "%s%u", i ? "," : "", b[i]);
    fputc(']', f);
}

/* vectors for the page's port of this framing (tests/test_page.js reads them: the same packets byte for byte) */
static void write_vectors(const char *path)
{
    static const char *const texts[] = {"get\n", "ok\n", "set name a jank with a long name\nset quantum 4\n", "status\n"};
    static const size_t caps[] = {9, 20, 23, 64, 182, 244};
    static struct packets o;
    FILE *f = fopen(path, "w");
    int first = 1;
    assert(f);
    fputs("{\"send\":[", f);
    for (size_t t = 0; t < sizeof texts / sizeof texts[0]; ++t)
        for (size_t c = 0; c < sizeof caps / sizeof caps[0]; ++c) {
            uint8_t frames[8][JX_FRAME_MAX];
            const uint8_t *msgs[8];
            size_t lens[8], count = 0, n = strlen(texts[t]);
            for (size_t at = 0; at < n; at += JX_TEXT_MAX, ++count) {
                lens[count] = jx_frame(texts[t] + at, n - at < JX_TEXT_MAX ? n - at : JX_TEXT_MAX, frames[count]);
                msgs[count] = frames[count];
            }
            send_all(msgs, lens, count, caps[c], (uint16_t)(1000u + 37u * c), &o);
            fprintf(f, "%s{\"text\":\"", first ? "" : ",");
            for (const char *p = texts[t]; *p; ++p)
                fputs(*p == '\n' ? "\\n" : (char[2]){*p, 0}, f);
            fprintf(f, "\",\"cap\":%zu,\"ms\":%u,\"packets\":[", caps[c], 1000u + 37u * (unsigned)c);
            for (size_t i = 0; i < o.count; ++i) {
                if (i)
                    fputc(',', f);
                hex_array(f, o.p[i], o.n[i]);
            }
            fputs("]}", f);
            first = 0;
        }
    /* packets as other senders write them: what the decoder makes of them, and whether it took each one */
    {
        static const uint8_t p1[] = {0x80, 0x81, 0xF0, 0x7D, 0x4A, 0x82, 0xF8, 0x4B, 'g'};
        static const uint8_t p2[] = {0x80, 'e', 't', 0x0A, 0x83, 0xF7, 0x84, 0x90, 0x3C, 0x40, 0x3E, 0x41, 0x85, 0x40, 0x00};
        static const uint8_t p3[] = {0x80, 0x80, 0xC1, 0x05, 0x06, 0x81, 0xE0, 0x00, 0x40, 0x82, 0xF3, 0x02, 0x82, 0xF6};
        static const uint8_t p4[] = {0x40, 0x80, 0xF8};
        static const uint8_t p5[] = {0x80, 0x80, 0xF0, 0x7D};
        static const uint8_t p6[] = {0x80, 0x80, 0xF7};
        static const uint8_t p7[] = {0x80, 0x3C, 0x40};
        static const uint8_t p8[] = {0x80};
        const uint8_t *ps[] = {p1, p2, p3, p5, p4, p6, p7, p8};
        const size_t ns[] = {sizeof p1, sizeof p2, sizeof p3, sizeof p5, sizeof p4, sizeof p6, sizeof p7, sizeof p8};
        struct jbm_in in;
        struct sink s = {.n = 0};
        jbm_init(&in);
        fputs("],\"decode\":{\"packets\":[", f);
        for (size_t i = 0; i < sizeof ps / sizeof ps[0]; ++i) {
            if (i)
                fputc(',', f);
            hex_array(f, ps[i], ns[i]);
        }
        fputs("],\"results\":[", f);
        for (size_t i = 0; i < sizeof ps / sizeof ps[0]; ++i)
            fprintf(f, "%s%d", i ? "," : "", jbm_decode(&in, ps[i], ns[i], collect, &s));
        fputs("],\"bytes\":", f);
        hex_array(f, s.bytes, s.n);
        fputs("}}\n", f);
    }
    assert(fclose(f) == 0);
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--vectors")) {
        write_vectors(argv[2]);
        return 0;
    }
    test_pack();
    test_control_line_every_payload();
    test_beat_report();
    test_packet_layout();
    test_f7_never_split_from_its_timestamp();
    test_no_sysex_no_packets();
    test_other_writers();
    test_short_messages();
    test_malformed();
    puts("blemidi: ok");
    return 0;
}
