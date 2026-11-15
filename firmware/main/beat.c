/* SPDX-License-Identifier: GPL-3.0-only */
/* BEAT-FOLLOW BENCH BUILD. The beat tracker's task: the computer's audio (usb_audio.c) through madmom's online beat tracker
 * (core/beat/: front end, one LSTM network, DBN 90..180 BPM, ModeBpm) into the Link session as master (core/jank_beatlink.c).
 * The network's WEIGHTS ARE MADMOM'S (CC BY-NC-SA 4.0, core/beat/beat_model.c): for this bench only, never to be pushed or
 * shipped; our own network replaces them.
 *
 * Core 0, below the Wi-Fi and Link tasks (priority 3), woken every 5 ms: it takes what the ring holds, 441 samples (one
 * 10 ms hop) at a time, so one pass of work is one frame. Core 1 stays the MIDI clock's alone. */
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
#include "jb_mode.h"
#include "usb_audio.h"

#define BEAT_PRIO 3
#define BEAT_STACK 16384               /* measured in QEMU with a synthetic loop: 8192 left < 300 bytes free (ModeBpm, the reports) */
#define POLL_MS 5
#define STEP_EVERY_US 250000           /* the Link session is looked after this often, and at each tracked beat */
#define WINDOW_US 1000000              /* the CPU share is over this long */

static const char *TAG = "beat";
static struct jb_beat s_b;
static struct jb_mode s_mode;
static struct jbl s_jbl;
static TaskHandle_t s_task;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static struct jk_beat s_pub;           /* published to beat_stats, under s_mux */

/* the task's own */
static int64_t s_samples;              /* samples given to the tracker since its start */
static uint8_t s_beat_seen;
static uint32_t s_beat_n;              /* beats tracked since the start (the reports count them) */
static uint32_t s_hops, s_hop_max_us;
static uint64_t s_busy_us, s_busy_cycles, s_frames_timed;
static int64_t s_win_start, s_win_busy;
static int32_t s_win_peak;             /* the input's largest |sample| in this window */
static float s_win_act;                /* the network's largest activation in this window */
static int16_t s_level_db = -99;       /* the last window's: dBFS, and the activation in percent */
static uint8_t s_conf_pct;

struct step_ctx {
    struct jbl_in in;
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
        usb_beat_line(line, n);        /* (not to a phone: it gets the HOME line, level and activation included) */
}

static void on_frame(void *ctx, const struct jb_frame *f)
{
    (void)ctx;
    s_hops++;
    if (f->act > s_win_act)
        s_win_act = f->act;
    if (f->beat) {
        jb_mode_update(&s_mode, f->time);
        s_beat_seen = 1;
        s_beat_n++;
        report('b', f->time);
    }
}

static void restart_tracker(void)
{
    uac_restart();
    jb_beat_reset(&s_b);
    jb_mode_init(&s_mode);
    s_samples = 0;
    s_beat_seen = 0;
}

/* the bridge's task, holding its lock */
static void step_link(struct jank_peer *p, int64_t now, void *arg)
{
    const struct step_ctx *c = arg;
    jbl_step(&s_jbl, &p->k, &p->start, jp_settled(p, now), now, &c->in, c->quantum);
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
    b.level_db = s_level_db;
    b.conf_pct = s_conf_pct;
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
    for (size_t i = 0; i < n; i++) {
        int32_t a = x[i] < 0 ? -(int32_t)x[i] : x[i];
        if (a > s_win_peak)
            s_win_peak = a;
    }
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
        now = esp_timer_get_time();
        if (now - s_win_start >= WINDOW_US) {
            cpu_x10 = (uint32_t)(s_win_busy * 1000 / (now - s_win_start));
            s_win_start = now;
            s_win_busy = 0;
            /* the second's input peak in dBFS (full scale 32768; silence and anything below -99 dB: -99) and activation */
            s_level_db = s_win_peak > 0 ? (int16_t)fmax(-99.0, floor(20.0 * log10((double)s_win_peak / 32768.0) + 0.5)) : -99;
            s_conf_pct = (uint8_t)fmin(100.0, fmax(0.0, floor((double)s_win_act * 100.0 + 0.5)));
            s_win_peak = 0;
            s_win_act = 0.0f;
            report('t', (double)s_samples / (double)UAC_RATE);
        }
        if (!s_beat_seen && now - last_step < STEP_EVERY_US)
            continue;
        s_beat_seen = 0;
        last_step = now;
        app_cfg_get(&cfg);
        {
            struct step_ctx c;
            make_input(&cfg, &c.in, &mode, &bpm);
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

esp_err_t beat_start(void)
{
    jbl_init(&s_jbl);
    if (jb_beat_init_q16(&s_b) != 0) {         /* the network's weights in RAM as int16: from flash they cost > a core */
        ESP_LOGE(TAG, "tracker setup failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "int16 network weights in RAM: %u bytes", (unsigned)jb_lstm_q16_ram());
    ESP_LOGW(TAG, "BEAT-FOLLOW BENCH BUILD: madmom's weights, CC BY-NC-SA 4.0, never ship");
    return xTaskCreatePinnedToCore(beat_task, "beat", BEAT_STACK, NULL, BEAT_PRIO, &s_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
