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
#define BEAT_STACK 8192
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

struct step_ctx {
    struct jbl_in in;
    uint32_t quantum;
};

/* the beat report on the MIDI port (jank_sysex.h JX_PUSH_BEAT): 'b' on each tracked beat, 't' once a second */
static void report(char kind, double at_s)
{
    struct uac_stats u;
    double bpm = 0.0;
    char line[JX_PUSH_TEXT_MAX + 1];
    enum jb_mode_kind mode = jb_mode_at(&s_mode, at_s);
    if (!jb_mode_bpm_at(&s_mode, at_s, &bpm))
        bpm = 0.0;
    uac_stats(&u);
    size_t n = jx_beat_line(line, sizeof line, kind, s_beat_n, (int32_t)llround(bpm * 100.0), (unsigned)mode, u.open);
    if (n)
        usb_beat_line(line, n);
}

static void on_frame(void *ctx, const struct jb_frame *f)
{
    (void)ctx;
    s_hops++;
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
    jbl_step(&s_jbl, &p->k, jp_joined(p), now, &c->in, c->quantum);
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
