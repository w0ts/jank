/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode: a Launchpad X plays a Digitone II (Note mode) through the Digitone's live control, SysEx only: no MIDI channel, and none of the Digitone's MIDI
 * is read. The logic is core/jank_lpapp.h's and core/jank_dn2live.h's (host-tested); this is the part that owns the one state, feeds it what the two devices
 * send and puts what it makes into their queues (usb_midi_host.c). Everything here runs in that file's client task (the USB Host Library's callbacks come from it), so the
 * state needs no lock; the queues take their own. Nothing here waits: the client task also serves the clock's transfers.
 *
 * The packets for the Digitone go through a FIFO (core/jank_lpout.h) that is drained as its queue has room, so a Note Off is
 * never lost to a full queue; the Launchpad's lights are only what changed (jlx_diff), a batch at a time.
 *
 * Not verified on hardware yet: the product names the roles are told apart by, that the Digitone's sequencer notes come out of
 * its USB port, its cable number (1: the Launchpad's "MIDI" port, measured), the Launchpad taking lights SETTLE_MS after the Programmer mode message. */
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app.h"
#include "jank_dn2live.h"
#include "jank_dn2rpc.h"
#include "jank_lpapp.h"
#include "jank_lpout.h"
#include "jank_lpscales.h"
#include "jank_lpx.h"
#include "uiscreen.h"
#include "usb_host_mode.h"

/* The Launchpad X has ONE USB-MIDI streaming interface (diag: ifs:1) with TWO cables, the high nibble of every USB-MIDI packet: 0 is the "DAW" port, 1 the
 * "MIDI" port (a Mac lists them as "LPX DAW" and "LPX MIDI"). Programmer mode's lights, and its setup messages, take effect on cable 1. Measured
 * 2026-11-24: the unit sent everything on cable 0 and the LEDs stayed dark although the Launchpad answered, took the messages without an error and
 * reported Programmer mode; the same messages on the Mac's "LPX MIDI" port (cable 1) lit the pads. */
#define LPX_CABLE 1u
#define DN2_CABLE 0u
#define RESERVE 4u                     /* packets left free in a queue that also carries the clock: the clock must not wait */
#define BATCH 16u                      /* lights per call: what fits one bulk packet */
#define SYSEX_PACKETS 3u               /* the Programmer mode message */
#define SETTLE_MS 400                  /* after the Launchpad's answer to the Programmer mode message, before the first lights: still a guess (it was 150,
                                          and the lights stayed dark: 2026-11-24, with the answer in and the packets right) */
#define REFRESH_MS 2000                /* every light again this often: the Launchpad may take none while it changes modes, and the diff would otherwise
                                          never send a light it believes delivered; the whole frame is about 330 bytes */
#define RETRY_MS 10                    /* the Programmer mode message waits for room this long between tries */
/* A Launchpad that was replugged stayed dark (measured 2026-11-24 over Wi-Fi diag: attached, claimed, answering the clock, no lights; the
 * message had gone out the moment it was claimed). The cause is not proven: a Launchpad whose firmware is not up yet may drop the message.
 * So: a wait before the first message, then the message again every ACK_WAIT_MS until the Launchpad answers it (it sends the same message
 * back: jlx_programmer_ack); lights wait for the answer. After ACK_TRIES messages with no answer the lights go anyway (a Launchpad that never
 * answers would otherwise stay dark for good). diag shows messages sent and answers. */
#define ATTACH_WAIT_MS 500
#define ACK_WAIT_MS 400
#define ACK_TRIES 4u
#define CIN_NOTE_OFF 0x8u
#define CIN_NOTE_ON 0x9u
#define CIN_CC 0xBu

static const char *TAG = "usb lp";

#define STEP_WAKE_MS 30                /* a step mode with the transport running */
#define LIVE_WAKE_MS 100              /* the client task wakes at least this often once the Launchpad is going: pushed notes' lights end after 150 ms, and the
                                          overlay after a second, and nothing else would wake the task to draw it (a clock transfer might, not always) */
#define REQUEST_MAX 48u               /* a live-control request is at most 6 + 7 + 1 + 1 bytes before its SysEx end: far under this */
#define REQUEST_PACKETS 16u            /* ... as USB-MIDI packets */

static struct jlapp s_app;
static struct jlx_frame s_shown;
static struct jlo s_out;               /* for the Digitone */
static struct jdl s_jdl;               /* the Digitone's live control: pads play and select the track by SysEx, not by MIDI channel (core/jank_dn2live.h) */
static struct jd2_sx s_sx;             /* the Digitone's SysEx, put together from its packets */
static uint8_t s_was_live;             /* live control was on at the last pump: a change to on sends the current track */
static uint8_t s_ready, s_lpx_up, s_dn2_up, s_programmer_pending;   /* pending: the Launchpad has not answered the Programmer mode message yet */
static uint8_t s_tries;                /* Programmer mode messages sent to this Launchpad */
static uint8_t s_setup_pending;        /* it is in Programmer mode: the LED-feedback and sleep-off messages (jlx_setup) are still to go, before any light */
static TickType_t s_settle_at;         /* no lights before this tick */
static TickType_t s_try_at;            /* the next Programmer mode message is due at this tick */
static TickType_t s_refresh_at;        /* every light is sent again at this tick (REFRESH_MS) */

static void ensure_init(void)
{
    if (s_ready)
        return;
    jlapp_init(&s_app, jls_get(JLS_MAJOR));   /* the starter list's major scale; the scale page (hold Note) changes it */
    jlo_init(&s_out);
    jdl_init(&s_jdl);
    jd2_sx_init(&s_sx);
    s_ready = 1;
}

static uint32_t now_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

/* the waiting packets go out as far as the Digitone's queue has room (beyond the clock's reserve) */
static void dn2_drain(void)
{
    uint8_t out[4 * BATCH];
    size_t room = usb_midi_host_room(JLAPP_DN2), n;
    if (!s_dn2_up || room <= RESERVE)
        return;
    n = jlo_drain(&s_out, room - RESERVE < BATCH ? room - RESERVE : BATCH, out);
    if (n)
        usb_midi_host_send_to(JLAPP_DN2, out, 4u * n);
}

/* bytes: whole packets, in order; with no Digitone there is nobody to play and they are dropped */
static void dn2_send(const uint8_t *packets, size_t bytes)
{
    if (!s_dn2_up)
        return;
    for (size_t i = 0; i + 4u <= bytes; i += 4u)
        if (!jlo_push(&s_out, packets + i))
            ESP_LOGW(TAG, "packets for the Digitone: FIFO full");
    dn2_drain();
}

/* a live-control request (a whole SysEx) for the Digitone, all of it or none: a cut SysEx would leave its parser waiting for an end. g_hostdiag.live_dropped counts
 * the ones that found no room */
static void dn2_send_sysex(const uint8_t *sx, size_t n)
{
    uint8_t pk[4 * REQUEST_PACKETS];
    size_t bytes;
    if (!s_dn2_up || !n)
        return;
    bytes = jd2_sysex_to_packets(DN2_CABLE, sx, n, pk, sizeof pk);
    if (!bytes || bytes / 4u > JLO_PACKETS - jlo_count(&s_out)) {
        g_hostdiag.live_dropped++;
        return;
    }
    g_hostdiag.live_asked++;
    dn2_send(pk, bytes);
}

static volatile uint8_t s_transport_req;   /* the Launchpad's start / stop scene button was pressed: the ui task acts on it */

int usb_lp_transport_request(void)
{
    if (!s_transport_req)
        return 0;
    s_transport_req = 0;
    return 1;
}

/* what a pad press or a button asks of the Digitone: always live control (SysEx), never a MIDI channel (jank_lpapp.h). A request that cannot be made (live
 * control is not on any more) is dropped: the Digitone ends the notes of a lapsed lease itself */
static void run_act(const struct jlapp_act *act)
{
    uint8_t sx[REQUEST_MAX];
    size_t n = 0;
    uint32_t t = now_ms();
    switch (act->kind) {
    case JLAPP_ACT_NOTE_ON:
    case JLAPP_ACT_NOTE_OFF:
        n = act->kind == JLAPP_ACT_NOTE_ON ? jdl_play(&s_jdl, t, act->track, JDL_PLAY_ON, act->note, act->vel, sx, sizeof sx)
                                           : jdl_play(&s_jdl, t, act->track, JDL_PLAY_OFF, act->note, 0, sx, sizeof sx);
        g_hostdiag.live_plays += n != 0;
        break;
    case JLAPP_ACT_TRACK_ON:           /* the track's own default note */
        n = jdl_play(&s_jdl, t, act->track, JDL_PLAY_DEFAULT, 0, 0, sx, sizeof sx);
        g_hostdiag.live_plays += n != 0;
        break;
    case JLAPP_ACT_TRACK_OFF:          /* note 255: the track's own default note again */
        n = jdl_play(&s_jdl, t, act->track, JDL_PLAY_OFF, 255, 0, sx, sizeof sx);
        g_hostdiag.live_plays += n != 0;
        break;
    case JLAPP_ACT_SELECT:
        n = jdl_track(&s_jdl, t, act->track, sx, sizeof sx);
        g_hostdiag.live_selects += n != 0;
        break;
    case JLAPP_ACT_STEP:               /* a trig set or cleared, a note set (the Digitone's STEP) */
        n = jdl_step_edit(&s_jdl, t, act->track, act->step, (enum jdl_step_op)act->op, act->note, act->vel, act->len, sx, sizeof sx);
        g_hostdiag.live_edits += n != 0;
        break;
    case JLAPP_ACT_TRANSPORT:          /* the ui task starts / stops the transport (it may wait for the bridge; this task must not) */
        s_transport_req = 1;
        return;
    default:
        return;
    }
    dn2_send_sysex(sx, n);
}

/* what live control is now, for the diag and for the pad logic (what a press can do, what the lights go by, whether FOLLOW can run) */
static void sync_live(void)
{
    g_hostdiag.live_state = s_jdl.state;
    g_hostdiag.live_play = s_jdl.play_ok;
    g_hostdiag.dn2_proto = s_jdl.st.valid ? s_jdl.st.proto : 0;   /* the Digitone's own counters, to tell where a missing push is lost */
    g_hostdiag.dn2_push = s_jdl.st.push;
    g_hostdiag.dn2_lease = s_jdl.st.lease;
    g_hostdiag.dn2_made = s_jdl.st.made;
    g_hostdiag.dn2_sent = s_jdl.st.sent;
    g_hostdiag.dn2_lost = s_jdl.st.lost;
    g_hostdiag.dn2_dropped = s_jdl.st.dropped;
    g_hostdiag.dn2_pushmsgs = s_jdl.st.msgs;
    g_hostdiag.head_rt_clocks = s_app.step.rt_clocks;   /* the playhead's source: the Digitone's own MIDI transport, or Jank's */
    g_hostdiag.head_rt_starts = s_app.step.rt_starts;
    g_hostdiag.head_rt_stops = s_app.step.rt_stops;
    g_hostdiag.head_rt_run = s_app.step.rt_run;
    g_hostdiag.head_beat = s_app.beat_ok;
    g_hostdiag.head_transport = s_app.transport;
    g_hostdiag.live_msg_ok = s_jdl.msg_ok;
    g_hostdiag.live_step_busy = s_jdl.step_busy;
    g_hostdiag.live_step_refused = s_jdl.step_refused;
    jlapp_set_live(&s_app, s_jdl.state, s_jdl.play_ok, jdl_can_read_track(&s_jdl));
}

/* the message the pad logic left (what Launchpad 95 writes in Live's status bar): on Jank's own screen when it has one; without a screen on the Digitone's (live control
 * MESSAGE, 54, a box of about a second over its page: stage 5; a build without it refuses once and nothing more is sent). A message made while live control is not up
 * (the "why the pads do not play" ones) has no way to be shown and is dropped. */
static void deliver_messages(void)
{
    char m[JLAPP_MSG_MAX];
    uint8_t sx[REQUEST_MAX];
    size_t n;
    if (!jlapp_take_message(&s_app, m, sizeof m))
        return;
    g_hostdiag.live_msgs++;
    if (uis_has_screen()) {
        uis_message(m);
        return;
    }
    n = jdl_message(&s_jdl, now_ms(), m, 0, sx, sizeof sx);   /* duration 0: the Digitone's own second */
    g_hostdiag.live_msgs_sent += n != 0;
    dn2_send_sysex(sx, n);
}

/* a whole SysEx from the Digitone: an answer to a live-control request (moves its state), or the push of the notes it is playing (lights) */
static void dn2_sysex(void)
{
    static struct jdl_note notes[JDL_NOTES_MAX];
    size_t count;
    int track;
    enum jdl_in k = jdl_input(&s_jdl, s_sx.buf, s_sx.n, notes, &count);
    if (k == JDL_IN_REPLY) {
        static struct jdl_page page;
        g_hostdiag.live_replies++;
        track = jdl_take_active(&s_jdl);           /* FOLLOW: the answer to an active-track read */
        if (track >= 0) {
            jlapp_tick(&s_app, now_ms());
            jlapp_on_active_track(&s_app, (unsigned)track);
        }
        if (jdl_take_page(&s_jdl, &page)) {        /* a step mode: a page of the current track */
            jlapp_tick(&s_app, now_ms());
            jlapp_on_page(&s_app, &page);
        }
    } else if (k == JDL_IN_IGNORED) {
        g_hostdiag.live_ignored++;
        g_hostdiag.live_ign_len = (uint16_t)s_sx.n;
    } else if (k == JDL_IN_PUSH) {
        g_hostdiag.live_pushes++;
        g_hostdiag.live_notes += (uint32_t)count;
        jlapp_tick(&s_app, now_ms());
        jlapp_on_push(&s_app, notes, count);
    }
    sync_live();
}

/* The session's beat, for the step modes' playhead. bridge_beat takes the bridge's lock, which this task must never wait on, so the ui task samples it and hands it over
 * here (a sequence counter makes the 64-bit read safe); between samples the position is extrapolated from the tempo. */
static volatile uint32_t s_beat_seq;
static int64_t s_beat_ub;
static int32_t s_beat_mpb;
static uint32_t s_beat_at_ms;
static uint8_t s_beat_known;

void usb_lp_beat_feed(int known, int64_t beat_ub, int32_t mpb)
{
    s_beat_seq++;                      /* odd: being written */
    __sync_synchronize();
    s_beat_known = known != 0;
    s_beat_ub = beat_ub;
    s_beat_mpb = mpb;
    s_beat_at_ms = now_ms();
    __sync_synchronize();
    s_beat_seq++;
}

/* the beat now in sixteenth notes; 0 when there is no session (or no tempo) */
static int beat_sixteenths(uint32_t t, uint32_t *tick16)
{
    int64_t beat, ub;
    int32_t mpb;
    uint32_t at, seq;
    uint8_t known;
    for (int tries = 0; tries < 3; tries++) {
        seq = s_beat_seq;
        __sync_synchronize();
        known = s_beat_known;
        ub = s_beat_ub;
        mpb = s_beat_mpb;
        at = s_beat_at_ms;
        __sync_synchronize();
        if (!(seq & 1u) && seq == s_beat_seq)
            break;
        known = 0;                     /* being written: this pass has no beat */
    }
    if (!known || mpb <= 0)
        return 0;
    beat = ub + (int64_t)(uint32_t)(t - at) * 1000 * 1000000 / mpb;   /* micro-beats: the elapsed microseconds over the microseconds a beat takes */
    if (beat < 0)
        return 0;
    *tick16 = (uint32_t)((beat * 4) / 1000000);
    return 1;
}

static uint32_t s_read_ms;             /* the last active-track read (FOLLOW) */
static uint32_t s_page_ms;             /* the last read of the step pages */
#define STEP_URGENT_MS 60u             /* ... right after a track or page switch, until its pages are in (at most STEP_URGENT_TRIES reads, then back to the slow rate) */
#define STEP_URGENT_TRIES 6u
static uint32_t s_seen_pages_ms;       /* the step modes' last page seen by the pump */
static uint8_t s_urgent_tries;         /* urgent page reads since a page last came */
#define STEP_READ_MS 400u             /* a step mode reads its two pages this often (each answer is 104 data bytes); a press shows at once, the next read confirms it */

/* live control is asked for, and kept up, while there is a Digitone and a Launchpad; once it answers, the Digitone's active track is made the one the pads are on;
 * while FOLLOW is on the Digitone's active track is read every JLAPP_FOLLOW_MS */
static void live_pump(void)
{
    uint8_t sx[REQUEST_MAX];
    size_t n;
    uint32_t t = now_ms();
    int live;
    if (!s_dn2_up || !s_lpx_up)
        return;
    dn2_send_sysex(sx, jdl_poll(&s_jdl, t, sx, sizeof sx));
    live = jdl_is_live(&s_jdl);
    sync_live();
    if (live && !s_was_live) {
        n = jdl_track(&s_jdl, t, s_app.n.track, sx, sizeof sx);
        g_hostdiag.live_selects += n != 0;
        dn2_send_sysex(sx, n);
    }
    if (live) {                        /* once per connection: does this Digitone have PLAY? known before the first press (the reply comes within a few ms) */
        n = jdl_probe_play(&s_jdl, t, sx, sizeof sx);
        g_hostdiag.live_plays += n != 0;
        dn2_send_sysex(sx, n);
    }
    s_was_live = (uint8_t)live;
    jlapp_set_transport(&s_app, bridge_is_playing());   /* (lock-free) the start / stop scene button's light, and the lower quadrant's rule */
    {
        uint32_t tick16 = 0;
        int known = beat_sixteenths(t, &tick16);
        jlapp_set_beat(&s_app, known, tick16);          /* the step modes' playhead */
    }
    if (s_app.step.pages_ms != s_seen_pages_ms) {   /* a page came: the urgent reads have worked */
        s_seen_pages_ms = s_app.step.pages_ms;
        s_urgent_tries = 0;
    }
    /* a step mode: the pages on the top rows, of the current track. Every STEP_READ_MS; at once (STEP_URGENT_MS) when the track or the page was just switched and its pages are
     * missing, a few times only: a Digitone that never answers STEPS must not be asked 12 times a second */
    if (live && t - s_page_ms >= (jlapp_step_urgent(&s_app) && s_urgent_tries < STEP_URGENT_TRIES ? STEP_URGENT_MS : STEP_READ_MS)) {
        uint8_t pages[2];
        size_t k = jlapp_step_wanted(&s_app, pages);
        if (jlapp_step_urgent(&s_app))
            s_urgent_tries++;
        for (size_t i = 0; i < k; i++) {
            n = jdl_steps_read(&s_jdl, t, s_app.n.track, pages[i], sx, sizeof sx);
            g_hostdiag.live_pagereads += n != 0;
            dn2_send_sysex(sx, n);
        }
        s_page_ms = t;
    }
    if (jlapp_following(&s_app) && t - s_read_ms >= JLAPP_FOLLOW_MS) {
        n = jdl_track_read(&s_jdl, t, sx, sizeof sx);
        s_read_ms = t;
        g_hostdiag.live_reads += n != 0;
        dn2_send_sysex(sx, n);
    }
}

void usb_lp_attach(enum jlapp_role r)
{
    ensure_init();
    if (r == JLAPP_LPX) {
        s_lpx_up = 1;
        g_hostdiag.lpx_up = 1;
        s_programmer_pending = 1;      /* sent by usb_lp_pump, after a wait and when its queue has room */
        s_setup_pending = 0;
        jdl_init(&s_jdl);              /* live control is only kept up while there is a Launchpad: ask afresh */
        s_was_live = 0;
        s_tries = 0;
        s_try_at = xTaskGetTickCount() + pdMS_TO_TICKS(ATTACH_WAIT_MS);
        ESP_LOGI(TAG, "Launchpad X");
    } else if (r == JLAPP_DN2) {
        s_dn2_up = 1;
        g_hostdiag.dn2_up = 1;
        jlo_clear(&s_out);
        jdl_init(&s_jdl);              /* live control is asked for afresh (usb_lp_pump) */
        jd2_sx_init(&s_sx);
        ESP_LOGI(TAG, "Digitone");
    }
}

void usb_lp_detach(enum jlapp_role r)
{
    ensure_init();
    if (r == JLAPP_LPX) {
        struct jlapp_act off[JLAPP_PADS + JLN_TRACKS];
        s_lpx_up = 0;
        g_hostdiag.lpx_up = 0;
        s_programmer_pending = 0;
        s_setup_pending = 0;
        /* its pads that were down end their notes: nothing stays sounding on the Digitone */
        for (size_t i = 0, n = jlapp_release_acts(&s_app, off, JLAPP_PADS + JLN_TRACKS); i < n; i++)
            run_act(&off[i]);
    } else if (r == JLAPP_DN2) {
        s_dn2_up = 0;
        g_hostdiag.dn2_up = 0;
        jlo_clear(&s_out);
        jdl_init(&s_jdl);
        sync_live();
        s_was_live = 0;
        memset(s_app.recent, 0, sizeof s_app.recent);
        s_app.track_held = 0;
    }
}

static void rx_lpx(const uint8_t p[4])
{
    uint8_t cin = p[0] & 0x0Fu;
    struct jlapp_act act;
    struct jlx_ev ev;
    const uint8_t msg[3] = {p[1], p[2], p[3]};
    int mode;
    if (jlx_programmer_ack(p, &mode)) {   /* its answer to the Programmer mode message, or to the mode query: the mode it is in now */
        g_hostdiag.lp_mode = (uint8_t)(mode ? 2 : 1);
        if (mode && s_programmer_pending) {   /* the setup messages, then the lights (usb_lp_pump) */
            s_programmer_pending = 0;
            s_setup_pending = 1;
            g_hostdiag.programmer_acked++;
        } else if (!mode && !s_programmer_pending) {   /* it is in Live mode (it took no lights there): enter Programmer mode again */
            if (g_hostdiag.lp_reentered < UINT8_MAX)
                g_hostdiag.lp_reentered++;
            s_programmer_pending = 1;
            s_tries = 0;
            s_try_at = xTaskGetTickCount();
        }
        return;
    }
    if (cin != CIN_NOTE_ON && cin != CIN_NOTE_OFF && cin != CIN_CC)
        return;
    if (!jlx_decode(msg, &ev))
        return;
    g_hostdiag.pads++;
    jlapp_tick(&s_app, now_ms());
    if (jlapp_on_lpx_acts(&s_app, &ev, &act, 1))
        run_act(&act);
    deliver_messages();
}

void usb_lp_rx(enum jlapp_role r, const uint8_t *packets, size_t n)
{
    ensure_init();
    for (size_t i = 0; i + 4u <= n; i += 4u) {
        if (r == JLAPP_LPX)
            rx_lpx(packets + i);
        else if (r == JLAPP_DN2) {
            g_hostdiag.dn2_msgs++;
            if (jd2_sx_feed(&s_sx, packets + i))   /* the Digitone's SysEx; of its MIDI only the real-time bytes below (no channel, no notes) */
                dn2_sysex();
            if ((packets[i] & 0x0Fu) == 0x0Fu) {   /* a single real-time byte: Start, Continue, Stop, clock: the step modes' playhead follows the Digitone's transport */
                jlapp_tick(&s_app, now_ms());
                jlapp_on_dn2_rt(&s_app, packets[i + 1]);
            }
        }
    }
}

void usb_lp_pump(void)
{
    struct jlx_frame want;
    uint8_t out[4 * BATCH];
    size_t room, n;
    live_pump();
    dn2_drain();
    if (!s_lpx_up)
        return;
    jlapp_tick(&s_app, now_ms());      /* the track overlay and the pushed notes' lights run out with it */
    room = usb_midi_host_room(JLAPP_LPX);
    if (s_programmer_pending) {        /* the whole message or none of it: a cut SysEx would leave the Launchpad in Live mode */
        if ((int32_t)(xTaskGetTickCount() - s_try_at) < 0 || room < SYSEX_PACKETS)
            return;
        usb_midi_host_send_to(JLAPP_LPX, out, jlx_programmer(LPX_CABLE, 1, out));
        g_hostdiag.programmer_sent++;
        s_tries++;
        s_try_at = xTaskGetTickCount() + pdMS_TO_TICKS(ACK_WAIT_MS);
        if (s_tries >= ACK_TRIES) {    /* it never answered: the setup and the lights go anyway */
            s_programmer_pending = 0;
            s_setup_pending = 1;
        }
        return;
    }
    if (s_setup_pending) {             /* Launchpad 95's two messages after Programmer mode: without them the Launchpad shows no light (jank_lpx.h) */
        if (room < JLX_SETUP_PACKETS + RESERVE)
            return;
        usb_midi_host_send_to(JLAPP_LPX, out, jlx_setup(LPX_CABLE, out));
        g_hostdiag.setup_sent++;
        s_setup_pending = 0;
        jlx_forget(&s_shown);          /* a Launchpad that just entered the mode shows nothing we know of */
        s_settle_at = xTaskGetTickCount() + pdMS_TO_TICKS(SETTLE_MS);
        s_refresh_at = s_settle_at + pdMS_TO_TICKS(REFRESH_MS);
        return;
    }
    if ((int32_t)(xTaskGetTickCount() - s_settle_at) < 0 || room <= RESERVE)
        return;
    if ((int32_t)(xTaskGetTickCount() - s_refresh_at) >= 0) {   /* all the lights again, in case one was lost; and which mode is it in? (a Launchpad in Live mode takes no lights) */
        jlx_forget(&s_shown);
        s_refresh_at = xTaskGetTickCount() + pdMS_TO_TICKS(REFRESH_MS);
        if (room >= SYSEX_PACKETS + RESERVE) {      /* the whole query, and the clock's reserve left: else it waits for the next refresh */
            usb_midi_host_send_to(JLAPP_LPX, out, jlx_mode_query(LPX_CABLE, out));
            return;                    /* the lights follow on the next pass: the queue has 3 packets fewer room now */
        }
    }
    jlapp_frame(&s_app, &want);
    n = jlx_diff(LPX_CABLE, &s_shown, &want, out, room - RESERVE < BATCH ? room - RESERVE : BATCH);
    if (n) {
        usb_midi_host_send_to(JLAPP_LPX, out, n * 4u);
        g_hostdiag.lights_sent += (uint32_t)n;
    }
}

/* how long the client task may sleep: until the lights may start, or the next try for room; else until an event */
uint32_t usb_lp_wait_ticks(void)
{
    int32_t left;
    if (!s_lpx_up)
        return portMAX_DELAY;
    if (s_programmer_pending) {        /* until the next message is due, then every RETRY_MS while it waits for room */
        left = (int32_t)(s_try_at - xTaskGetTickCount());
        if (left > 0)
            return (uint32_t)left;
        return pdMS_TO_TICKS(RETRY_MS) ? pdMS_TO_TICKS(RETRY_MS) : 1u;
    }
    if (s_setup_pending)               /* waiting for room for the setup messages */
        return pdMS_TO_TICKS(RETRY_MS) ? pdMS_TO_TICKS(RETRY_MS) : 1u;
    left = (int32_t)(s_settle_at - xTaskGetTickCount());
    if (left > 0)
        return (uint32_t)left;
    left = (int32_t)(s_refresh_at - xTaskGetTickCount());   /* settled: until the next refresh (the clock's transfers wake the task far oftener) */
    {
        /* ... but live control is asked every second, and the lights fade by the clock; a step mode's playhead moves every sixteenth (125 ms at 120 BPM): 30 ms */
        uint32_t cap = s_app.mode == JLAPP_MODE_STEP && s_app.transport ? STEP_WAKE_MS : LIVE_WAKE_MS;
        if (left > (int32_t)pdMS_TO_TICKS(cap))
            left = (int32_t)pdMS_TO_TICKS(cap);
    }
    return left > 0 ? (uint32_t)left : 1u;
}
