/* SPDX-License-Identifier: GPL-3.0-only */
/* BEAT-FOLLOW BENCH BUILD. The beat tracker's task: the computer's audio (usb_audio.c) through madmom's online beat tracker
 * (core/beat/: front end, one LSTM network, DBN 90..180 BPM, ModeBpm) into the Link session as master (core/jank_beatlink.c).
 * The network's WEIGHTS ARE MADMOM'S (CC BY-NC-SA 4.0, core/beat/beat_model.c): for this bench only, never to be pushed or
 * shipped; our own network replaces them.
 *
 * Core 0, below the Wi-Fi and Link tasks (priority 3), woken every 5 ms: it takes what the ring holds, 441 samples (one
 * 10 ms hop) at a time, so one pass of work is one frame. Core 1 stays the MIDI clock's alone.
 *
 * AUTO BAR: the downbeat estimator (core/beat/jb_down.c, weights from the user's Traktor grids and audio) runs on the
 * same frames and on ModeBpm's grid; when it knows the 1, the bridge's step moves the session's bar onto it in whole
 * beats (jbl_bar_step). Its reset events here: ModeBpm stopped (30 s without a steady beat), a tempo jump
 * (DOWN_JUMP_RESET), a new audio stream (the tracker restarts). `set autobar off` leaves the bar where it is. */
#include <math.h>
#include <string.h>
#include "app.h"
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_beatlink.h"
#include "jank_peer.h"
#include "jank_sysex.h"
#include "jb_beat.h"
#include "jb_down.h"
#include "jb_mode.h"
#include "usb_audio.h"

#define BEAT_PRIO 3
#define BEAT_STACK 16384               /* measured in QEMU with a synthetic loop: 8192 left < 300 bytes free (ModeBpm, the reports) */
#define POLL_MS 5
#define STEP_EVERY_US 250000           /* the Link session is looked after this often, and at each tracked beat */
#define WINDOW_US 1000000              /* the CPU share is over this long */
#define DOWN_JUMP_RESET 1              /* a tempo jump (jb_mode's jumps) resets the bar, as designed; 0: the count goes on */

static const char *TAG = "beat";
static struct jb_beat s_b;
static struct jb_mode s_mode;
static struct jbl s_jbl;
static TaskHandle_t s_task;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static struct jk_beat s_pub;           /* published to beat_stats, under s_mux */
static struct jb_down s_down;          /* the downbeat estimator, the beat task's own */
static jd_seg_row *s_down_seg;         /* its segment ring (2.9 KB), in PSRAM when there is some */
static uint8_t s_autobar = 1, s_autobar_picks;     /* the setting as last applied */

/* the task's own */
static int64_t s_samples;              /* samples given to the tracker since its start */
static uint8_t s_beat_seen;
static uint32_t s_beat_n;              /* beats tracked since the start (the reports count them) */
static uint32_t s_hops, s_hop_max_us;
static uint64_t s_busy_us, s_busy_cycles, s_frames_timed;
static int64_t s_win_start, s_win_busy;

struct step_ctx {
    struct jbl_in in;
    struct jbl_bar bar;
    uint32_t quantum;
};

struct session_q {
    int64_t at;                        /* host us */
    int32_t us;                        /* the session's nearest beat from `at` */
    uint8_t ok;                        /* in a session that plays */
};

/* the bridge's task's lock held: the session's beat near q->at */
static void session_at(struct jank_peer *p, int64_t now, void *arg)
{
    struct session_q *q = arg;
    (void)now;
    q->ok = (uint8_t)(jp_joined(p) && jp_playing(p));
    if (q->ok)
        q->us = jbl_session_offset_us(jp_beat_at(p, q->at), jp_mpb(p));
}

/* the beat report on the MIDI port (jank_sysex.h JX_PUSH_BEAT): 'b' on each tracked beat, 't' once a second */
static void report(char kind, double at_s)
{
    struct uac_stats u;
    struct jk_cfg cfg;
    double bpm = 0.0;
    char line[JX_PUSH_TEXT_MAX + 1];
    int32_t extra = JX_NO_EXTRA, link = JX_NO_EXTRA;
    int64_t sample_us;
    enum jb_mode_kind mode = jb_mode_at(&s_mode, at_s);
    if (!jb_mode_bpm_at(&s_mode, at_s, &bpm))
        bpm = 0.0;
    uac_stats(&u);
    app_cfg_get(&cfg);
    if (kind == 't')
        extra = cfg.beat_latency_ms;
    else if (uac_sample_time((int64_t)llround(at_s * (double)UAC_RATE), &sample_us)) {
        /* the beat as the stick takes it to be heard (as the Link grid does: make_input) and how long ago that was */
        struct session_q q = {.at = sample_us + (int64_t)cfg.beat_latency_ms * 1000};
        int64_t lag = midi_now_us() - q.at;
        extra = lag > INT32_MAX ? INT32_MAX : lag <= INT32_MIN ? INT32_MIN + 1 : (int32_t)lag;
        bridge_with_peer(session_at, &q);          /* the Link session's beat (what Live follows) near that instant */
        if (q.ok)
            link = q.us;
    }
    size_t n = jx_beat_line(line, sizeof line, kind, s_beat_n, (int32_t)llround(bpm * 100.0), (unsigned)mode, u.open, extra, link);
    if (n)
        usb_beat_line(line, n);
}

/* a tracked beat to the downbeat estimator, after ModeBpm took it (live/downbeat.py Driver.tracked) */
static void down_tracked(double t, int jumps_before)
{
    double tb, period;
    int running;
    if (DOWN_JUMP_RESET && s_mode.jumps != jumps_before && jd_counting(&s_down))
        jd_reset(&s_down);              /* a tempo jump: a new bar to find */
    running = jb_mode_at(&s_mode, t) != JB_STOPPED;
    if (!jb_mode_grid(&s_mode, &tb, &period))
        jd_stop(&s_down);
    else if (!running && jd_counting(&s_down))
        jd_stop(&s_down);               /* ModeBpm stopped: a reset; shadow beats from its next grid on */
    else
        jd_grid(&s_down, t, tb, period, running);
}

static void on_frame(void *ctx, const struct jb_frame *f)
{
    (void)ctx;
    s_hops++;
    if (s_down_seg && jd_frame(&s_down, f->index, f->bands))
        s_beat_seen = 1;                /* a decision: the session's bar is looked after now */
    if (f->beat) {
        int jumps = s_mode.jumps;
        jb_mode_update(&s_mode, f->time);
        if (s_down_seg)
            down_tracked(f->time, jumps);
        s_beat_seen = 1;
        s_beat_n++;
        report('b', f->time);
    }
}

static void restart_down(void)
{
    if (!s_down_seg)
        return;
    jd_init(&s_down, s_down_seg);       /* the frame count starts over with the tracker */
    jd_set_auto(&s_down, s_autobar);
}

static void restart_tracker(void)
{
    uac_restart();
    jb_beat_reset(&s_b);
    jb_mode_init(&s_mode);
    restart_down();
    s_samples = 0;
    s_beat_seen = 0;
}

/* AUTO BAR: the setting, and `set autobar on` again = picked again (MANUAL ends) */
static void apply_autobar(const struct jk_cfg *cfg)
{
    if (!s_down_seg || (cfg->autobar == s_autobar && cfg->autobar_picks == s_autobar_picks))
        return;
    s_autobar = cfg->autobar;
    s_autobar_picks = cfg->autobar_picks;
    jd_set_auto(&s_down, s_autobar);
}

/* the downbeat estimator as the bridge's bar step wants it: a LOCKED decision and its beat in Link host time */
static void make_bar(const struct jk_cfg *cfg, struct jbl_bar *bar)
{
    struct jd_view v;
    int64_t us;
    memset(bar, 0, sizeof *bar);
    if (!s_down_seg)
        return;
    jd_view(&s_down, &v);
    if (v.state != JD_LOCKED || v.phi < 0 || !uac_sample_time((int64_t)llround(v.t_beat * (double)UAC_RATE), &us))
        return;
    bar->locked = 1;
    bar->seq = v.seq;
    bar->count = v.count;
    bar->phi = v.phi;
    bar->beat_us = us + (int64_t)cfg->beat_latency_ms * 1000;
}

/* the bridge's task, holding its lock */
static void step_link(struct jank_peer *p, int64_t now, void *arg)
{
    const struct step_ctx *c = arg;
    int settled = jp_settled(p, now);
    jbl_step(&s_jbl, &p->k, &p->start, settled, now, &c->in, c->quantum);
    jbl_bar_step(&s_jbl, &p->k, &p->start, settled, now, &c->in, &c->bar);
}

/* what the tracker says, as the session's master wants it: a tracked beat in Link host time and the grid's period */
static void make_input(const struct jk_cfg *cfg, struct jbl_in *in, enum jb_mode_kind *mode, double *bpm)
{
    double now_s = (double)s_samples / (double)UAC_RATE, tb, period;
    struct uac_stats u;
    int64_t us;
    uac_stats(&u);
    *mode = jb_mode_at(&s_mode, now_s);
    *bpm = 0.0;
    memset(in, 0, sizeof *in);
    in->passive = (uint8_t)!u.open;            /* LINK mode: the tracker neither starts nor stops the session */
    if (!u.open || *mode == JB_STOPPED || !jb_mode_bpm_at(&s_mode, now_s, bpm) || !jb_mode_grid(&s_mode, &tb, &period))
        return;
    if (!uac_sample_time((int64_t)llround(tb * (double)UAC_RATE), &us))
        return;
    in->running = 1;
    in->beat_us = us + (int64_t)cfg->beat_latency_ms * 1000;
    in->period_us = period * 1e6;
}

static void publish(enum jb_mode_kind mode, double bpm, const struct jk_cfg *cfg, uint32_t cpu_x10)
{
    struct uac_stats u;
    struct jk_beat b;
    uac_stats(&u);
    memset(&b, 0, sizeof b);
    b.on = cfg->beat;
    b.audio = u.open;
    b.mode = (uint8_t)mode;
    b.bpm_x100 = (int32_t)llround(bpm * 100.0);
    b.hops = s_hops;
    b.hop_avg_us = s_frames_timed ? (uint32_t)(s_busy_us / s_frames_timed) : 0u;
    b.hop_avg_cycles = s_frames_timed ? (uint32_t)(s_busy_cycles / s_frames_timed) : 0u;
    b.hop_max_us = s_hop_max_us;
    b.cpu_pct_x10 = cpu_x10;
    b.packets = u.packets;
    b.gaps = u.gaps;
    b.overflows = u.overflows;
    b.resets = u.resets;
    b.stack_free = (uint32_t)uxTaskGetStackHighWaterMark(NULL) * (uint32_t)sizeof(StackType_t);
    b.tempo_sets = s_jbl.tempo_sets;
    b.phase_forces = s_jbl.phase_forces;
    b.starts = s_jbl.starts;
    b.stops = s_jbl.stops;
    b.err_us = s_jbl.err_us;
    b.offset_us = uac_offset_us();
    b.latency_us = cfg->beat_latency_ms * 1000;
    if (s_down_seg) {
        struct jd_view v;
        jd_view(&s_down, &v);
        b.bar = v.state;
        b.bar_phi = v.phi;
        b.bar_lead_x100 = (int32_t)lroundf(v.lead * 100.0f);
    } else
        b.bar_phi = -1;
    b.bar_moves = s_jbl.bar_moves;
    portENTER_CRITICAL(&s_mux);
    s_pub = b;
    portEXIT_CRITICAL(&s_mux);
}

/* the tracker's work on one pass's samples; the time it took is the figure for the CPU */
static void work(const int16_t *x, size_t n)
{
    uint32_t h0 = s_hops;
    int64_t t0 = esp_timer_get_time();
    uint32_t c0 = esp_cpu_get_cycle_count();
    jb_beat_push_s16(&s_b, x, n, on_frame, NULL);
    uint32_t cycles = esp_cpu_get_cycle_count() - c0;
    int64_t us = esp_timer_get_time() - t0;
    uint32_t frames = s_hops - h0;
    s_samples += (int64_t)n;
    s_busy_us += (uint64_t)us;
    s_busy_cycles += cycles;
    s_win_busy += us;
    if (frames) {
        uint32_t per = (uint32_t)(us / (int64_t)frames);
        s_frames_timed += frames;
        if (per > s_hop_max_us)
            s_hop_max_us = per;
    }
}

static void beat_task(void *arg)
{
    int16_t x[JB_HOP];
    uint32_t epoch = uac_epoch();
    int64_t last_step = 0;
    uint32_t cpu_x10 = 0;
    enum jb_mode_kind mode = JB_STOPPED;
    double bpm = 0.0;
    (void)arg;
    restart_tracker();
    s_win_start = esp_timer_get_time();
    for (;;) {
        size_t n;
        int64_t now;
        struct jk_cfg cfg;
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        if (epoch != uac_epoch()) {
            epoch = uac_epoch();
            restart_tracker();
        }
        while ((n = uac_pull(x, JB_HOP)) != 0u)
            work(x, n);
        if (s_down_seg && jd_counting(&s_down) && jb_mode_at(&s_mode, (double)s_samples / (double)UAC_RATE) == JB_STOPPED)
            jd_stop(&s_down);           /* 30 s without a steady beat: the bar starts over */
        now = esp_timer_get_time();
        if (now - s_win_start >= WINDOW_US) {
            cpu_x10 = (uint32_t)(s_win_busy * 1000 / (now - s_win_start));
            s_win_start = now;
            s_win_busy = 0;
            report('t', (double)s_samples / (double)UAC_RATE);
        }
        if (!s_beat_seen && now - last_step < STEP_EVERY_US)
            continue;
        s_beat_seen = 0;
        last_step = now;
        app_cfg_get(&cfg);
        apply_autobar(&cfg);
        {
            struct step_ctx c;
            make_input(&cfg, &c.in, &mode, &bpm);
            make_bar(&cfg, &c.bar);
            c.quantum = cfg.quantum;
            if (cfg.beat)
                bridge_with_peer(step_link, &c);
        }
        publish(mode, bpm, &cfg, cpu_x10);
    }
}

void beat_stats(struct jk_beat *out)
{
    portENTER_CRITICAL(&s_mux);
    *out = s_pub;
    portEXIT_CRITICAL(&s_mux);
    out->heap_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    out->heap_min = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    out->heap_largest = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    out->psram_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out->psram_min = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
}

uint8_t beat_bar_state(void)
{
    uint8_t s;
    portENTER_CRITICAL(&s_mux);
    s = s_pub.bar;
    portEXIT_CRITICAL(&s_mux);
    return s;
}

esp_err_t beat_start(void)
{
    jbl_init(&s_jbl);
    if (jb_beat_init_q16(&s_b) != 0) {         /* the network's weights in RAM as int16: from flash they cost > a core */
        ESP_LOGE(TAG, "tracker setup failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "int16 network weights in RAM: %u bytes", (unsigned)jb_lstm_q16_ram());
    /* AUTO BAR's segment ring: PSRAM when there is some, else internal RAM; none at all: no AUTO BAR, the rest runs */
    s_down_seg = heap_caps_malloc(sizeof(jd_seg_row) * JD_SEGS, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_down_seg)
        s_down_seg = heap_caps_malloc(sizeof(jd_seg_row) * JD_SEGS, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (s_down_seg) {
        jd_init(&s_down, s_down_seg);
        ESP_LOGI(TAG, "AUTO BAR: %u bytes + a %u-byte segment ring", (unsigned)sizeof s_down, (unsigned)(sizeof(jd_seg_row) * JD_SEGS));
    } else
        ESP_LOGE(TAG, "AUTO BAR: no RAM for its segment ring, off");
    ESP_LOGW(TAG, "BEAT-FOLLOW BENCH BUILD: madmom's weights, CC BY-NC-SA 4.0, never ship");
    return xTaskCreatePinnedToCore(beat_task, "beat", BEAT_STACK, NULL, BEAT_PRIO, &s_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
