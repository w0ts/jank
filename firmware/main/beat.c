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
 * beats (jbl_bar_step). Its reset events here: ModeBpm stopped (30 s without a steady beat), a new audio
 * stream (the tracker restarts); a tempo jump only with DOWN_JUMP_RESET. `set autobar off` leaves the bar where it is. */
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
#include "jank_fold.h"
#include "jank_live.h"
#include "usb_audio.h"

#define BEAT_PRIO 3
#define BEAT_STACK 16384               /* measured in QEMU with a synthetic loop: 8192 left < 300 bytes free (ModeBpm, the reports) */
#define POLL_MS 5
#define STEP_EVERY_US 250000           /* the Link session is looked after this often, and at each tracked beat */
#define WINDOW_US 1000000              /* the CPU share is over this long */
#define SILENT_GAP_US 200000         /* the source open and no samples this long: that time counts as silence (jank_live.h) */
#define DOWN_JUMP_RESET 0              /* 1: a tempo jump (jb_mode's jumps) resets the bar; off: those are mostly short excursions
                                        * in breaks, and resetting cost 3 points and 2.4x the moves (beat-listener
                                        * studies/downbeat/results/v2_stream_check.md) */

static const char *TAG = "beat";
static struct jb_beat s_b;
static struct jb_mode s_mode;
static struct jbl s_jbl;
static TaskHandle_t s_task;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static struct jk_beat s_pub;           /* published to beat_stats, under s_mux */
static struct jb_down s_down;          /* the downbeat estimator, the beat task's own */
static jd_seg_row *s_down_seg;         /* its segment ring (2.9 KB), in PSRAM when there is some */
static uint8_t s_autobar = JK_AUTOBAR_START, s_autobar_picks;     /* the setting as last applied (enum jk_autobar) */
static uint8_t s_bar_now;              /* AT START: a new start for the bar (a resync, a fold change, a new lock) */
static uint8_t s_was_running;          /* the tracker gave a tempo at the last step ... */
static uint32_t s_run_from;            /* ... from this tracked beat on (START WAIT counts from it) */

/* the task's own */
static int64_t s_samples;              /* samples given to the tracker since its start */
static uint8_t s_beat_seen;
static uint32_t s_beat_n;              /* beats tracked since the start (the reports count them) */
static uint32_t s_hops, s_hop_max_us;
static uint64_t s_busy_us, s_busy_cycles, s_frames_timed;
static int64_t s_win_start, s_win_busy;
static int32_t s_win_peak;             /* the input's largest |sample| in this window */
static float s_win_act;                /* the network's largest activation in this window */
static int16_t s_peak_db = -99;        /* the last window's: dBFS, and the activation in percent */
static uint8_t s_conf_pct;
static struct jlv s_live;              /* sound on the source (AUDIO mode) and the input meter: the tracker's own samples */
static int64_t s_fed_us;               /* when samples last came ... */
static int64_t s_fed_samples;          /* ... s_samples then */
static uint8_t s_audio_live;           /* the source open and sounding: AUDIO mode, as last decided by this task */

struct step_ctx {
    struct jbl_in in;
    struct jbl_bar bar;
    uint32_t quantum;
    double bpm;                        /* the tracker's own (unfolded) tempo, 0: none */
};

/* AUDIO mode's fold and AUTO RANGE (jank_fold.h): the beat task's; the control line asks through s_fold_req */
enum { FOLD_REQ_NONE = 0, FOLD_REQ_UP, FOLD_REQ_DOWN, FOLD_REQ_ONE, FOLD_REQ_RESET };
static struct jfo s_fold;
static volatile uint8_t s_fold_req;

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
    bpm *= jfo_factor(&s_fold);        /* the tempo jank puts out */
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
    size_t n = jx_beat_line(line, sizeof line, kind, s_beat_n, (int32_t)llround(bpm * 100.0), (unsigned)mode,
                            u.open ? (s_audio_live ? 2u : 1u) : 0u, extra, link);
    if (n)
        usb_beat_line(line, n);        /* (not to a phone: it gets the HOME line, level and activation included) */
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
    if (f->act > s_win_act)
        s_win_act = f->act;
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
    jd_set_auto(&s_down, s_autobar != JK_AUTOBAR_OFF);
}

static void restart_tracker(void)
{
    uac_restart();
    jfo_reset(&s_fold);                /* a resync: fold 1 (Python: resync clears the fold) */
    s_bar_now = 1;
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
    jd_set_auto(&s_down, s_autobar != JK_AUTOBAR_OFF);
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

/* the fold onto what Link gets: AUTO RANGE on a new tempo (against the session's tempo while it plays), then the folded
 * grid (for /2, /4 anchored on the session's grid); AUTO BAR makes no move while folded (jank_fold.h). The bridge's lock
 * held, in the beat task */
static void fold_step(struct jank_peer *p, int64_t now, int settled, struct step_ctx *c)
{
    int on = settled && jp_joined(p);
    double session_bpm = on && jp_playing(p) && jp_mpb(p) > 0 ? 60000000.0 / (double)jp_mpb(p) : 0.0;
    if (c->in.running && !s_fold.had_tempo)
        s_bar_now = 1;                 /* a new lock (a new track): a new start for the bar */
    if (jfo_step(&s_fold, c->in.running, c->bpm, s_mode.jumps, session_bpm, (uint32_t)(now / 1000)))
        s_bar_now = 1;
    if (c->in.running) {
        int64_t beat;
        double period;
        double pos = on ? (double)lk_beat_at(&p->k, c->in.beat_us) / (double)LK_UBEAT : 0.0;
        jfo_grid(&s_fold, c->in.beat_us, c->in.period_us, on, pos, on ? (double)jp_mpb(p) : 0.0, &beat, &period);
        c->in.beat_us = beat;
        c->in.period_us = period;
    }
    if (s_fold.exp)
        c->bar.locked = 0;
    c->in.bar_now = s_bar_now;
}

/* AUTO BAR's setting for the bar step, and START WAIT: with AT START the tracker's start waits up to JBL_BAR_WAIT_BEATS
 * tracked beats after its lock for the estimator to find the 1 (jank_beatlink.h) */
static void bar_rules(struct step_ctx *c)
{
    struct jd_view v;
    if (c->in.running && !s_was_running)
        s_run_from = s_beat_n;
    s_was_running = c->in.running;
    c->in.bar_mode = s_autobar == JK_AUTOBAR_OFF ? JBL_BAR_OFF : s_autobar == JK_AUTOBAR_ALWAYS ? JBL_BAR_ALWAYS : JBL_BAR_START;
    if (c->in.bar_mode != JBL_BAR_START || !s_down_seg || !c->in.running)
        return;
    jd_view(&s_down, &v);
    c->in.bar_wait = (uint8_t)(v.state != JD_LOCKED && s_beat_n - s_run_from < JBL_BAR_WAIT_BEATS);
}

/* the bridge's task, holding its lock */
static void step_link(struct jank_peer *p, int64_t now, void *arg)
{
    struct step_ctx *c = arg;
    int settled = jp_settled(p, now);
    c->in.held = (uint8_t)bridge_session_held();   /* AUDIO mode's Stop with FOLLOW+LEAD (jank_hold.h) */
    fold_step(p, now, settled, c);
    jbl_step(&s_jbl, &p->k, &p->start, settled, now, &c->in, c->quantum);
    jbl_bar_step(&s_jbl, &p->k, &p->start, settled, now, &c->in, &c->bar);
    s_bar_now = 0;                     /* (one step: a decision not there yet waits for the next start) */
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
    in->passive = (uint8_t)!s_audio_live;      /* LINK mode (no sound): the tracker neither starts nor stops the session */
    if (!s_audio_live || *mode == JB_STOPPED || !jb_mode_bpm_at(&s_mode, now_s, bpm) || !jb_mode_grid(&s_mode, &tb, &period))
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
    b.audio_live = s_audio_live;
    b.fold_exp = s_fold.exp;
    b.fold_manual = s_fold.manual;
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
    b.level_db = s_live.rms_db;
    b.level_min_db = s_live.min_db;
    b.level_max_db = s_live.max_db;
    b.peak_db = s_peak_db;
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
    jlv_push(&s_live, x, n);
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

/* AUDIO mode: the source open and sounding (jank_live.h). Closed: the detector starts over (no sound at once); open with
 * no samples for a while (a stalled stream): that time is silence */
static void follow_sound(int64_t now)
{
    struct uac_stats u;
    uac_stats(&u);
    if (!u.open) {
        if (s_live.blocks || s_live.acc_n)
            jlv_init(&s_live);
        s_fed_us = now;
    } else if (s_samples != s_fed_samples) {
        s_fed_samples = s_samples;
        s_fed_us = now;
    } else if (now - s_fed_us >= SILENT_GAP_US) {
        jlv_push(&s_live, NULL, (size_t)((now - s_fed_us) * (int64_t)UAC_RATE / 1000000));
        s_fed_us = now;
    }
    if (s_audio_live && !(u.open && jlv_live(&s_live)))
        jfo_reset(&s_fold);            /* AUDIO mode ended: fold 1 */
    s_audio_live = (uint8_t)(u.open && jlv_live(&s_live));
}

/* the control line's fold, taken in the beat task */
static void take_fold_req(void)
{
    uint8_t r = __atomic_exchange_n(&s_fold_req, (uint8_t)FOLD_REQ_NONE, __ATOMIC_ACQ_REL);
    if (r == FOLD_REQ_UP || r == FOLD_REQ_DOWN || r == FOLD_REQ_ONE)
        jfo_user(&s_fold, r == FOLD_REQ_UP ? 1 : r == FOLD_REQ_DOWN ? -1 : 0);
    else if (r == FOLD_REQ_RESET)
        jfo_reset(&s_fold);
    if (r != FOLD_REQ_NONE)
        s_bar_now = 1;                 /* a fold change or a Start: a new start for the bar */
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
    jlv_init(&s_live);
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
        follow_sound(esp_timer_get_time());
        take_fold_req();
        if (s_down_seg && jd_counting(&s_down) && jb_mode_at(&s_mode, (double)s_samples / (double)UAC_RATE) == JB_STOPPED)
            jd_stop(&s_down);           /* 30 s without a steady beat: the bar starts over */
        now = esp_timer_get_time();
        if (now - s_win_start >= WINDOW_US) {
            cpu_x10 = (uint32_t)(s_win_busy * 1000 / (now - s_win_start));
            s_win_start = now;
            s_win_busy = 0;
            /* the second's input peak in dBFS (full scale 32768; silence and anything below -99 dB: -99) and activation */
            s_peak_db = s_win_peak > 0 ? (int16_t)fmax(-99.0, floor(20.0 * log10((double)s_win_peak / 32768.0) + 0.5)) : -99;
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
        apply_autobar(&cfg);
        {
            struct step_ctx c;
            make_input(&cfg, &c.in, &mode, &bpm);
            c.bpm = c.in.running ? bpm : 0.0;
            make_bar(&cfg, &c.bar);
            bar_rules(&c);
            c.quantum = cfg.quantum;
            if (cfg.beat)
                bridge_with_peer(step_link, &c);
        }
        publish(mode, bpm * jfo_factor(&s_fold), &cfg, cpu_x10);
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

int beat_fold(int dir)
{
    uint8_t audio, locked;
    int exp = s_fold.exp;
    beat_state(&audio, &locked);
    if (!audio)
        return -99;                    /* LINK mode: no fold (UP / DOWN are the session's tempo there) */
    exp = dir > 0 ? exp + 1 : dir < 0 ? exp - 1 : 0;
    exp = exp < JFO_EXP_MIN ? JFO_EXP_MIN : exp > JFO_EXP_MAX ? JFO_EXP_MAX : exp;
    __atomic_store_n(&s_fold_req, (uint8_t)(dir > 0 ? FOLD_REQ_UP : dir < 0 ? FOLD_REQ_DOWN : FOLD_REQ_ONE), __ATOMIC_RELEASE);
    return exp;
}

void beat_fold_resync(void) { __atomic_store_n(&s_fold_req, (uint8_t)FOLD_REQ_RESET, __ATOMIC_RELEASE); }

void beat_state(uint8_t *audio, uint8_t *locked)
{
    portENTER_CRITICAL(&s_mux);
    *audio = (uint8_t)(s_pub.on && s_pub.audio_live);
    *locked = (uint8_t)(s_pub.mode != JB_STOPPED && s_pub.bpm_x100 > 0);
    portEXIT_CRITICAL(&s_mux);
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
