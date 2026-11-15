/* SPDX-License-Identifier: GPL-3.0-only */
/* AUDIO mode with no Link peer: the stick runs a session of its own, led by the beat tracker, and its MIDI clock starts on
 * that session's bar line. Then a computer joins and leaves, an older session turns up, and two sticks meet.
 *
 * A model of the firmware's loops around the core they run: firmware/main/bridge.c (bridge_task: jp_solo, jp_poll,
 * follow_transport, publish; bridge_play) and beat.c (step_link, every 250 ms) over jank_peer, jank_beatlink, jank_hold
 * and jank_clock. The tracker is a steady grid switched on and off by hand. Times and delays are this model's, not
 * measurements of anything. Exit status: the number of failed checks. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "jank_beatlink.h"
#include "jank_clock.h"
#include "jank_hold.h"
#include "jank_peer.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define STICKS 2
#define TO_PC (-1)
#define DELAY_US 1500
#define STEP_US 1000                   /* the bridge task's loop (2 ms idle wait, frames sooner) */
#define BEAT_STEP_US 250000            /* beat.c STEP_EVERY_US */
#define PUBLISH_US 5000                /* bridge.c PUBLISH_US */
#define QUANTUM 4u
#define BAR_TICKS ((int64_t)JC_PPQN * QUANTUM)
#define PC_IP 0xA9FE0A0Bu              /* 169.254.10.11 */

struct stick {
    uint8_t used;
    struct jank_peer jp;
    struct jbl jbl;
    struct jho hold;
    struct jho_in hin;
    struct jc_ticker tick;
    struct jc_map map;
    uint8_t base, want, ticking, was_settled;
    int64_t next_pub, next_beat;
    uint8_t audio, locked, automatic;  /* AUDIO mode, the tracker locked, transport AUTO (LINK TRANSPORT IGNORE) */
    uint8_t gated, silent, conf_open;  /* LOW CONFIDENCE's gate, the silence, the confidence at the threshold + 5 */
    int64_t beat0;                     /* a beat of the music, host us */
    double period_us;
    uint32_t starts, stops, clocks;    /* MIDI out */
    int64_t start_tick, start_at;      /* the last MIDI start's tick and time */
    uint32_t frames, unsettles;        /* frames the peer sent; steps in a session that lost its settled state */
};

static struct stick st[STICKS];
static int64_t sim_now;
static int air_linked;                 /* the sticks hear each other */
static int pc_at = -1;                 /* the stick the computer is on (USB); -1: not connected */
static int pc_alive;
static struct lk_node pc;
static uint32_t pc_arp_ip;
static uint8_t pc_arp_mac[6];
static int pc_arp_ok;

static const uint8_t BC[6] = {255, 255, 255, 255, 255, 255};
static const uint8_t ZERO[6] = {0};
static const uint8_t LINK_MAC[6] = {0x01, 0x00, 0x5E, 0x4C, 0x4E, 0x4B};

static uint8_t usb_mac(int i, int b) { return (uint8_t[6]){0x06, 0x11, 0x22, 0x33, 0x44, (uint8_t)(0x50 + i)}[b]; }
static void air_mac(int i, uint8_t m[6]) { memcpy(m, (uint8_t[6]){0x02, 0x11, 0x22, 0x33, 0x44, (uint8_t)(0x60 + i)}, 6); }

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static size_t arp(uint8_t *f, const uint8_t *src, uint32_t spa, uint32_t tpa)
{
    memcpy(f, BC, 6);
    memcpy(f + 6, src, 6);
    f[12] = 8;
    f[13] = 6;
    memcpy(f + 14, "\x00\x01\x08\x00\x06\x04\x00", 7);
    f[21] = 1;
    memcpy(f + 22, src, 6);
    wr32(f + 28, spa);
    memcpy(f + 32, ZERO, 6);
    wr32(f + 38, tpa);
    return 42;
}

static size_t udp(uint8_t *f, const uint8_t *dst, const uint8_t *src, uint32_t sip, uint32_t dip, uint16_t dport, const uint8_t *pl, size_t n)
{
    memcpy(f, dst, 6);
    memcpy(f + 6, src, 6);
    f[12] = 8;
    f[13] = 0;
    memset(f + 14, 0, 28);
    f[14] = 0x45;
    f[16] = (uint8_t)((28 + n) >> 8);
    f[17] = (uint8_t)(28 + n);
    f[22] = 1;
    f[23] = 17;
    wr32(f + 26, sip);
    wr32(f + 30, dip);
    f[34] = (uint8_t)(LK_MCAST_PORT >> 8);
    f[35] = (uint8_t)LK_MCAST_PORT;
    f[36] = (uint8_t)(dport >> 8);
    f[37] = (uint8_t)dport;
    f[38] = (uint8_t)((8 + n) >> 8);
    f[39] = (uint8_t)(8 + n);
    memcpy(f + 42, pl, n);
    return 42 + n;
}

/* ---- the wire: every frame takes DELAY_US, in order */
struct wire {
    int64_t at;
    int to;                            /* a stick, or TO_PC */
    uint32_t where;                    /* how it reaches that stick: JP_TO_AIR or JP_TO_USB */
    size_t len;
    uint8_t f[JP_FRAME_MAX];
};
#define WIRE 512u
static struct wire w[WIRE];
static uint32_t nw;

static void push(int to, uint32_t where, const uint8_t *f, size_t len)
{
    if (nw >= WIRE || len > JP_FRAME_MAX)
        return;
    w[nw].at = sim_now + DELAY_US;
    w[nw].to = to;
    w[nw].where = where;
    w[nw].len = len;
    memcpy(w[nw].f, f, len);
    nw++;
}

static void stick_out(void *ctx, uint32_t where, const uint8_t *eth, size_t len)
{
    struct stick *s = ctx;
    int i = (int)(s - st);
    s->frames++;
    if (where == JP_TO_AIR) {
        for (int j = 0; air_linked && j < STICKS; j++)
            if (j != i && st[j].used)
                push(j, JP_TO_AIR, eth, len);
    } else if (pc_at == i) {
        push(TO_PC, 0, eth, len);
    }
}

/* the computer: an ordinary Link node behind ARP (as tests/test_peer.c's), its MAC the stick's air M */
static int pc_send(void *ctx, uint32_t ip, uint16_t port, const uint8_t *p, uint32_t n)
{
    uint8_t f[JP_FRAME_MAX], m[6];
    (void)ctx;
    if (pc_at < 0)
        return 0;                      /* not connected: lost */
    air_mac(pc_at, m);
    if (ip == LK_MCAST_IP) {
        push(pc_at, JP_TO_USB, f, udp(f, LINK_MAC, m, PC_IP, ip, port, p, n));
        return 0;
    }
    if (!pc_arp_ok || pc_arp_ip != ip) {
        pc_arp_ip = ip;
        pc_arp_ok = 0;
        push(pc_at, JP_TO_USB, f, arp(f, m, PC_IP, ip));
        return 0;
    }
    push(pc_at, JP_TO_USB, f, udp(f, pc_arp_mac, m, PC_IP, ip, port, p, n));
    return 0;
}

static void pc_input(const uint8_t *f, size_t len)
{
    if (!pc_alive)
        return;
    if (f[12] == 8 && f[13] == 6) {
        if (f[21] == 2 && rd32(f + 28) == pc_arp_ip) {
            memcpy(pc_arp_mac, f + 22, 6);
            pc_arp_ok = 1;
        }
        return;
    }
    lk_rx(&pc, sim_now, rd32(f + 26), (uint16_t)(f[34] << 8 | f[35]), f + 42, (uint32_t)(len - 42));
}

/* the computer opens a Link app: its own session from now (at: the stick it is on, -1 none yet) */
static void pc_open(int at, int32_t mpb)
{
    struct lk_cfg c = {PC_IP, LK_MCAST_PORT, pc_send, NULL};
    pc_at = at;
    pc_arp_ok = 0;
    lk_init(&pc, &c, 77, mpb);
    lk_enable(&pc, 1, sim_now);
    lk_set_addr(&pc, PC_IP, LK_MCAST_PORT);
    pc_alive = 1;
}

static void pc_close(void)
{
    lk_enable(&pc, 0, sim_now);        /* a bye-bye */
    pc_alive = 0;
}

static void deliver(void)
{
    uint32_t i = 0;
    while (i < nw && w[i].at <= sim_now) {
        if (w[i].to == TO_PC)
            pc_input(w[i].f, w[i].len);
        else if (st[w[i].to].used)
            jp_input(&st[w[i].to].jp, w[i].f, w[i].len, w[i].where, sim_now);
        i++;
    }
    if (i) {
        nw -= i;
        memmove(w, w + i, nw * sizeof *w);
    }
}

/* ---- the stick */

/* beat.c step_link with make_input: the tracker's grid, the session's master */
static void beat_step(struct stick *s, int64_t now)
{
    struct jbl_in in;
    memset(&in, 0, sizeof in);
    in.passive = (uint8_t)!s->audio;
    if (s->audio && s->locked) {
        in.running = 1;
        in.beat_us = s->beat0;
        in.period_us = s->period_us;
    }
    in.held = (uint8_t)jho_session_held(&s->hold, &s->hin);
    in.bar_mode = JBL_BAR_OFF;
    jbl_step(&s->jbl, &s->jp.k, &s->jp.start, jp_settled(&s->jp, now), now, &in, QUANTUM);
}

/* the timer interrupt's ticks due by now */
static void ticks(struct stick *s, int64_t now)
{
    while (s->ticking && s->tick.t_due <= now) {
        struct jc_fire f;
        int64_t n = s->tick.n;
        jc_step(&s->tick, &s->map, now, &f);
        for (int i = 0; i < f.n; i++) {
            if (f.bytes[i] == JC_START) {
                s->starts++;
                s->start_tick = n;
                s->start_at = now;
            } else if (f.bytes[i] == JC_STOP) {
                s->stops++;
            } else if (f.bytes[i] == JC_CLOCK) {
                s->clocks++;
            }
        }
    }
}

/* bridge.c bridge_task's loop: jp_solo, jp_poll, follow_transport, publish */
static void bridge_step(struct stick *s, int64_t now)
{
    struct jc_tr t;
    int want, settled;
    if (jp_solo(&s->jp, s->audio, now))
        s->base = 0;
    jp_poll(&s->jp, now);
    settled = jp_settled(&s->jp, now);
    t = (struct jc_tr){.audio = s->audio, .automatic = s->automatic, .session = (uint8_t)jp_in_session(&s->jp),
                       .settled = (uint8_t)settled, .playing = (uint8_t)jp_playing(&s->jp), .gone = (uint8_t)(s->automatic && !s->jp.on),
                       .peers = jp_peers(&s->jp)};
    if (t.session && s->was_settled && !settled)
        s->unsettles++;
    s->was_settled = (uint8_t)(t.session && settled);
    s->base = (uint8_t)jc_base(s->base, &t);
    s->hin = (struct jho_in){.audio = s->audio, .lt = s->automatic ? JHO_IGNORE : JHO_LEAD, .base = s->base, .locked = s->locked,
                             .clock_running = (uint8_t)(s->tick.playing && !s->tick.want_start && !s->tick.resync),
                             .gated = s->gated, .silent = s->silent, .conf_open = s->conf_open};
    want = jho_outputs(&s->hold, &s->hin);
    if (want != s->want) {
        s->want = (uint8_t)want;
        jc_want(&s->tick, want);
    }
    if (now >= s->next_pub && settled) {
        jc_map_make(&s->map, now, jp_beat_at(&s->jp, now), jp_mpb(&s->jp), 0);
        s->next_pub = now + PUBLISH_US;
        if (!s->ticking)
            s->ticking = jc_begin(&s->tick, &s->map, now + 1000) == 0;
    }
    if (now >= s->next_beat) {
        beat_step(s, now);
        s->next_beat = now + BEAT_STEP_US;
    }
    ticks(s, now);
}

/* bridge.c bridge_play: AUDIO mode, the hold; LINK mode, the stick's own clocks, and with transport LINK the session */
static void play(struct stick *s, int on)
{
    if (s->audio) {
        jho_set(&s->hold, !on);
        return;
    }
    if (!s->automatic)
        jp_set_playing(&s->jp, on, QUANTUM, sim_now);
    if (on)
        jho_link_play(&s->hold);
    else
        jho_link_stop(&s->hold);
}

static struct stick *stick_on(int i, int audio, int automatic)
{
    struct stick *s = &st[i];
    uint8_t usb[6], air[6];
    memset(s, 0, sizeof *s);
    for (int b = 0; b < 6; b++)
        usb[b] = usb_mac(i, b);
    air_mac(i, air);
    jp_init(&s->jp, usb, air, 0x1234u + 77u * (uint32_t)i, stick_out, s);
    jp_allow(&s->jp, 1, sim_now);
    jbl_init(&s->jbl);
    jho_init(&s->hold);
    jc_ticker_init(&s->tick, QUANTUM);
    s->start_tick = -1;
    s->audio = (uint8_t)audio;
    s->automatic = (uint8_t)automatic;
    s->next_beat = sim_now;
    s->used = 1;
    return s;
}

static void music(struct stick *s, double bpm, int locked)
{
    s->period_us = 60e6 / bpm;
    s->beat0 = 1234567;                /* the same music for every stick */
    s->locked = (uint8_t)locked;
}

static void world(void)
{
    memset(st, 0, sizeof st);
    nw = 0;
    sim_now = 1000000;
    air_linked = 0;
    pc_at = -1;
    pc_alive = 0;
}

static void run_for(int64_t us)
{
    int64_t end = sim_now + us;
    while (sim_now < end) {
        deliver();
        if (pc_alive)
            lk_poll(&pc, sim_now);
        for (int i = 0; i < STICKS; i++)
            if (st[i].used)
                bridge_step(&st[i], sim_now);
        sim_now += STEP_US;
    }
}

static int64_t abs64(int64_t v) { return v < 0 ? -v : v; }

/* the session's grid against the music: us off at the nearest beat */
static int64_t grid_error_us(const struct stick *s)
{
    int64_t n = llround((double)(sim_now - s->beat0) / s->period_us);
    int64_t t = s->beat0 + llround((double)n * s->period_us);
    int64_t ub = jp_beat_at(&s->jp, t), r = ((ub % LK_UBEAT) + LK_UBEAT) % LK_UBEAT;
    if (r > LK_UBEAT / 2)
        r -= LK_UBEAT;
    return r * jp_mpb(&s->jp) / LK_UBEAT;
}

static int on_the_bar(const struct stick *s)
{
    return s->start_tick >= 0 && s->start_tick % BAR_TICKS == 0;
}

/* ---- a stick alone (the user's case, 2026-11-15: locked at 122.9 BPM, armed forever) */

static void test_alone_starts_on_the_bar(void)
{
    struct stick *a;
    int64_t t_lock, bar_us;
    world();
    a = stick_on(0, 1, 0);
    music(a, 122.9, 0);
    run_for(2000000);
    CHECK(a->starts == 0);                                    /* sound, no beat yet: nothing starts */
    a->locked = 1;
    t_lock = sim_now;
    run_for(8000000);
    bar_us = llround(a->period_us * QUANTUM);
    CHECK(a->jp.on && jp_peers(&a->jp) == 0 && jp_settled(&a->jp, sim_now));
    CHECK(jp_playing(&a->jp) && jp_mpb(&a->jp) == (int32_t)llround(a->period_us));
    CHECK(abs64(grid_error_us(a)) <= 5000);                   /* the session's beats on the music's */
    CHECK(a->starts == 1 && a->stops == 0 && on_the_bar(a));  /* one MIDI start, on a bar line ... */
    CHECK(a->start_tick >= 0 && jc_tick_ub(a->start_tick) == lk_play_beat(&a->jp.k));   /* ... the session's start */
    CHECK(a->start_at > t_lock && a->start_at - t_lock <= bar_us + BEAT_STEP_US + JST_LEAD_US);
    CHECK(a->clocks > 300);
    printf("solo: MIDI start %lld ms after the lock, on bar %lld\n", (long long)((a->start_at - t_lock) / 1000),
           (long long)(lk_play_beat(&a->jp.k) / LK_UBEAT / QUANTUM));
}

/* a computer opens Live while the stick plays alone: Live's session is the younger, it joins the stick's. The stick's
 * clock goes on: no MIDI Stop, no second Start, no jump */
static void test_peer_joins_mid_play(void)
{
    struct stick *a = &st[0];
    uint8_t sess[8];
    uint32_t resyncs = a->tick.resyncs, unsettles = a->unsettles;
    memcpy(sess, a->jp.k.sess, sizeof sess);
    pc_open(0, 500000);
    run_for(6000000);
    CHECK(jp_peers(&a->jp) == 1 && lk_peers(&pc) == 1);
    CHECK(!memcmp(a->jp.k.sess, sess, sizeof sess) && !memcmp(pc.sess, sess, sizeof sess));   /* the stick's session */
    CHECK(lk_mpb(&pc) == jp_mpb(&a->jp) && lk_playing(&pc));
    CHECK(abs64(lk_beat_at(&pc, sim_now) - jp_beat_at(&a->jp, sim_now)) < 4000);
    CHECK(a->starts == 1 && a->stops == 0 && a->tick.resyncs == resyncs && a->unsettles == unsettles);
}

/* the computer closes Live: the stick plays on, alone, still led by the tracker */
static void test_last_peer_leaves(void)
{
    struct stick *a = &st[0];
    uint32_t clocks;
    pc_close();
    run_for((int64_t)JP_ALONE_MS * 1000 + 2000000);
    CHECK(a->jp.on && jp_peers(&a->jp) == 0 && jp_settled(&a->jp, sim_now) && jp_playing(&a->jp));
    CHECK(a->starts == 1 && a->stops == 0);
    clocks = a->clocks;
    music(a, 126.0, 1);                                       /* the next track */
    run_for(1000000);
    CHECK(a->clocks > clocks + 40);
    CHECK(jp_mpb(&a->jp) == (int32_t)llround(a->period_us) && abs64(grid_error_us(a)) <= 5000);
    CHECK(a->starts == 1 && a->stops == 0);
}

/* AUDIO mode's Stop and Start alone (FOLLOW+LEAD): the outputs stop at once, the session too; Start: on the next bar */
static void test_hold_alone(void)
{
    struct stick *a = &st[0];
    play(a, 0);
    run_for(1000000);
    CHECK(a->stops == 1 && !jp_playing(&a->jp) && a->jp.on);
    play(a, 1);
    run_for(5000000);
    CHECK(a->starts == 2 && a->stops == 1 && on_the_bar(a) && jp_playing(&a->jp));
    CHECK(jc_tick_ub(a->start_tick) == lk_play_beat(&a->jp.k));
}

/* the sound goes (LINK mode) while alone: nothing to follow any more: the session ends, the outputs stop */
static void test_sound_goes_alone(void)
{
    struct stick *a = &st[0];
    a->audio = 0;
    run_for(500000);
    CHECK(!a->jp.on && !jp_in_session(&a->jp));
    CHECK(a->stops == 2 && a->starts == 2);
    run_for(10000000);
    CHECK(a->starts == 2);
}

/* LINK mode, alone: as before, nothing (no session, no frame, no clock); a play is refused */
static void test_link_mode_alone(void)
{
    for (int automatic = 0; automatic <= 1; automatic++) {
        struct stick *a;
        world();
        a = stick_on(0, 0, automatic);
        run_for(10000000);
        CHECK(!a->jp.on && a->frames == 0 && !a->ticking && a->starts == 0);
        CHECK(jp_set_playing(&a->jp, 1, QUANTUM, sim_now) == -1);
        play(a, 1);
        run_for(5000000);
        CHECK(!a->jp.on && !a->ticking && a->starts == 0);
    }
}

/* a session older than the stick's turns up (a computer whose Link app was open before, out of reach). The stick joins
 * it, as Link's rule says. It plays: the stick's clock goes on (a jump to the session's phase, no Stop / Start); the
 * tracker then leads it. It is stopped: the stick's outputs stop with it, and the tracker starts it on its next bar line */
static void test_older_session(int pc_playing)
{
    struct stick *a;
    world();
    pc_open(-1, 600000);
    run_for(100000);                   /* (a start / stop at the session's ghost time 0 is not newer than none: ignored) */
    if (pc_playing)
        lk_set_playing(&pc, 1, lk_beat_at(&pc, sim_now), sim_now);
    run_for(2000000);
    CHECK(lk_playing(&pc) == pc_playing);
    a = stick_on(0, 1, 0);
    music(a, 122.9, 1);
    run_for(6000000);
    CHECK(a->starts == 1 && jp_peers(&a->jp) == 0);
    pc_at = 0;
    run_for(8000000);
    CHECK(jp_peers(&a->jp) == 1 && !memcmp(a->jp.k.sess, pc.sess, sizeof pc.sess));   /* the stick joined */
    CHECK(lk_playing(&pc) && jp_playing(&a->jp));
    CHECK(lk_mpb(&pc) == (int32_t)llround(a->period_us) && abs64(grid_error_us(a)) <= 5000);   /* the tracker leads */
    if (pc_playing)
        CHECK(a->starts == 1 && a->stops == 0);
    else
        CHECK(a->starts == 2 && a->stops == 1 && on_the_bar(a) && jc_tick_ub(a->start_tick) == lk_play_beat(&a->jp.k));
    printf("solo: joined an older %s session: %u MIDI stop(s), %u clock jump(s)\n", pc_playing ? "playing" : "stopped",
           (unsigned)a->stops, (unsigned)a->tick.resyncs);
}

/* two sticks: A plays alone in AUDIO mode; B, in LINK mode, comes up next to it and joins A's session. B's own Stop
 * holds its clocks when the session starts again (A's tracker); with transport LINK it stops the session, with AUTO
 * (LINK TRANSPORT IGNORE) the session is not B's to stop. B's Start: its clocks on the session's next bar line */
static void test_two_sticks(int automatic)
{
    struct stick *a, *b;
    world();
    air_linked = 1;
    a = stick_on(0, 1, 0);
    music(a, 122.9, 1);
    run_for(6000000);
    b = stick_on(1, 0, automatic);
    run_for(6000000);
    CHECK(jp_peers(&b->jp) == 1 && !memcmp(a->jp.k.sess, b->jp.k.sess, sizeof a->jp.k.sess));
    CHECK(a->starts == 1 && a->stops == 0);                    /* B's arrival changes nothing on A */
    CHECK(b->starts == 1 && on_the_bar(b));
    play(b, 0);
    run_for(2000000);
    CHECK(b->stops == 1);
    if (automatic) {
        CHECK(jp_playing(&a->jp) && a->stops == 0);           /* IGNORE: the session plays on */
    } else {
        CHECK(!jp_playing(&a->jp) && a->stops == 1);          /* FOLLOW+LEAD: the session stops, A follows it */
        a->locked = 0;                                         /* A's tracker loses the beat, finds it again: it starts */
        run_for(1000000);
        a->locked = 1;
    }
    run_for(6000000);
    CHECK(jp_playing(&a->jp));
    CHECK(b->starts == 1 && b->stops == 1);                    /* B's Stop holds while it sees A */
    play(b, 1);
    run_for(4000000);
    CHECK(b->starts == 2 && on_the_bar(b) && jc_tick_ub(b->start_tick) % ((int64_t)QUANTUM * LK_UBEAT) == 0);
    CHECK(a->stops == (automatic ? 0u : 1u));                  /* B's Start restarts nothing on A */
    printf("solo: two sticks, B %s: its Stop held, its Start on bar line %lld\n", automatic ? "AUTO" : "LINK",
           (long long)(b->start_tick / BAR_TICKS));
}

/* two sticks playing alone on the same music come into range: the younger session joins the older. The older stick
 * notices nothing; the younger's clock goes on (no Stop / Start: the session it joins plays) */
static void test_two_solo_sticks_meet(void)
{
    struct stick *a, *b;
    uint8_t sess[8];
    world();
    a = stick_on(0, 1, 0);
    music(a, 122.9, 1);
    run_for(3000000);
    b = stick_on(1, 1, 0);
    music(b, 122.9, 1);
    run_for(6000000);
    CHECK(a->starts == 1 && b->starts == 1 && memcmp(a->jp.k.sess, b->jp.k.sess, sizeof sess));
    memcpy(sess, a->jp.k.sess, sizeof sess);
    air_linked = 1;
    run_for(8000000);
    CHECK(jp_peers(&a->jp) == 1 && jp_peers(&b->jp) == 1);
    CHECK(!memcmp(a->jp.k.sess, sess, sizeof sess) && !memcmp(b->jp.k.sess, sess, sizeof sess));   /* A's, the older */
    CHECK(a->starts == 1 && a->stops == 0 && b->starts == 1 && b->stops == 0);
    CHECK(jp_playing(&a->jp) && abs64(grid_error_us(a)) <= 5000 && abs64(grid_error_us(b)) <= 5000);
    printf("solo: two sticks met: the younger's clock jumped %u time(s), no Stop / Start\n", (unsigned)b->tick.resyncs);
}

/* M2g, the user's case (2026-11-15): alone, locked, conf 49 under the threshold 80: gated, stopped. The user's Start
 * wins: the outputs start on the session's next bar line (armed until then); a drop before the confidence reached the
 * threshold + 5 stops nothing; after it a drop stops, as before; silence stops at once, Start or not */
static void test_start_wins_alone(void)
{
    struct stick *a;
    int64_t t_play, delay;
    world();
    a = stick_on(0, 1, 0);
    music(a, 122.9, 1);
    a->gated = 1;
    run_for(6000000);
    CHECK(a->starts == 0 && !jp_playing(&a->jp) && jho_icon(&a->hold, &a->hin) == JHO_STOPPED);
    play(a, 1);
    t_play = sim_now;
    run_for(STEP_US);
    CHECK(jho_icon(&a->hold, &a->hin) == JHO_ARMED);
    run_for(4000000);
    CHECK(a->starts == 1 && a->stops == 0 && on_the_bar(a) && jc_tick_ub(a->start_tick) == lk_play_beat(&a->jp.k));
    delay = a->start_at - t_play;
    CHECK(delay <= llround(a->period_us * QUANTUM) + BEAT_STEP_US + JST_LEAD_US);
    CHECK(jho_icon(&a->hold, &a->hin) == JHO_RUNNING);
    a->gated = 0;                                              /* the threshold, not + 5 */
    run_for(1000000);
    a->gated = 1;                                              /* a drop before the re-arm */
    run_for(3000000);
    CHECK(a->starts == 1 && a->stops == 0 && jp_playing(&a->jp));
    a->gated = 0;
    a->conf_open = 1;                                          /* threshold + 5: re-armed */
    run_for(1000000);
    a->conf_open = 0;
    a->gated = 1;                                              /* a drop now: stops (FOLLOW+LEAD: the session too) */
    run_for(1000000);
    CHECK(a->starts == 1 && a->stops == 1 && !jp_playing(&a->jp));
    play(a, 1);                                                /* Start again, then silence: stops at once */
    run_for(4000000);
    CHECK(a->starts == 2 && on_the_bar(a));
    a->silent = 1;
    run_for(100000);
    CHECK(a->stops == 2 && jho_icon(&a->hold, &a->hin) == JHO_STOPPED);
    printf("solo: Start at low confidence: MIDI start %lld ms after it\n", (long long)(delay / 1000));
}

/* the two layers, alone (the user, 2026-11-15: "if I manually stopped, don't start again automatically"): after the
 * user's Stop nothing automatic starts the MIDI clock: the gate closing and opening, silence then sound, a re-lock, the
 * sound gone (LINK mode, the session of its own ended) and back (a new one), a computer joining and starting the session.
 * The user's Start does, on the bar; then an automatic stop (the gate, once re-armed) restarts automatically */
static void test_user_stop_holds_alone(void)
{
    struct stick *a;
    world();
    a = stick_on(0, 1, 0);
    music(a, 122.9, 1);
    run_for(6000000);
    CHECK(a->starts == 1);
    play(a, 0);
    run_for(1000000);
    CHECK(a->stops == 1 && !jp_playing(&a->jp));
    a->gated = 1;
    run_for(1000000);
    a->gated = 0;
    a->conf_open = 1;
    run_for(2000000);
    a->conf_open = 0;
    a->silent = 1;
    run_for(1000000);
    a->silent = 0;
    run_for(2000000);
    a->locked = 0;
    run_for(1000000);
    a->locked = 1;
    run_for(3000000);
    a->audio = 0;
    run_for(1000000);
    CHECK(!a->jp.on);
    a->audio = 1;
    run_for(3000000);
    CHECK(a->jp.on && !jp_playing(&a->jp));
    pc_open(0, 500000);
    run_for(4000000);
    lk_set_playing(&pc, 1, lk_beat_at(&pc, sim_now), sim_now);
    run_for(4000000);
    CHECK(jp_peers(&a->jp) == 1 && jp_playing(&a->jp));         /* the computer started the session ... */
    CHECK(a->starts == 1 && a->stops == 1 && jho_icon(&a->hold, &a->hin) == JHO_STOPPED);   /* ... the clock stays off */
    play(a, 1);
    run_for(5000000);
    CHECK(a->starts == 2 && on_the_bar(a) && jc_tick_ub(a->start_tick) % ((int64_t)QUANTUM * LK_UBEAT) == 0);
    a->conf_open = 1;                                          /* the gate re-armed */
    run_for(500000);
    a->conf_open = 0;
    a->gated = 1;                                              /* an automatic stop ... */
    run_for(2000000);
    CHECK(a->stops == 2);
    a->gated = 0;                                              /* ... and an automatic start, on the bar */
    a->conf_open = 1;
    run_for(5000000);
    CHECK(a->starts == 3 && on_the_bar(a));
}

int main(void)
{
    test_alone_starts_on_the_bar();
    test_peer_joins_mid_play();
    test_last_peer_leaves();
    test_hold_alone();
    test_sound_goes_alone();
    test_link_mode_alone();
    test_older_session(1);
    test_older_session(0);
    test_two_sticks(0);
    test_two_sticks(1);
    test_two_solo_sticks_meet();
    test_start_wins_alone();
    test_user_stop_holds_alone();
    printf("solo: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
