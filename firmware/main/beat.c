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
 * stream (the tracker restarts); a tempo jump only with DOWN_JUMP_RESET. `set autobar off` leaves the bar where it is.
 *
 * LOW CONFIDENCE (core/jank_conf.h, the player's live/confidence.py and engine.py): the tracked beats of the last 8 s
 * against the grid lines published (M2i: live/follow.py line_confidence) make the confidence (an F-measure), looked at
 * each step; with STOP / START under the THRESHOLD the gate closes (it opens again 5 points above), and 0.5 s of silence
 * closes it whatever the setting. The bridge stops the outputs while it is closed (core/jank_hold.h), and with
 * FOLLOW+LEAD the session (jbl_in.held).
 *
 * ADVANCED FOLLOW (M2i, beat-listener studies/advanced-follow): the estimator is the follower (core/beat/jb_follow.h:
 * ModeBpm's lock, then a Kalman glide that takes a pitch move in a few beats with the phase kept), and while it glides
 * the spectral rate (core/beat/jb_rate.h, `set rate on|off`) reads the playback rate off the tracker's own |FFT| (the
 * front end's tap: no second FFT) and proposes it to the follower, which takes it if the next beat backs it. All of its
 * data (59 KB) is in PSRAM, a hundred bytes aside (internal RAM is short); without PSRAM the rate is off and the rest runs.
 *
 * THE TUNED FOLLOW (M2j, beat-listener studies/tempo-changes, the proposal tuned-l16): hard to lock (LOCK_BEATS 16), the
 * rate's median of three, a fast jump to a new grid 16 beats agree on (jb_follow_fast_jump), the published lines cleared on
 * any jump, and the weak-signal hold (core/beat/jb_weak.h): the network's activation kept for 2.56 s, each tracked beat
 * judged strong or weak by it (fed to the follower 3 frames late, when its frames after are in), weak beats and salient
 * activation peaks on beat-less lines as gentle corrections, and the gate held open by the weak confidence once the
 * strict one opened it. Its 7.3 KB are PSRAM; without, M2i's follow. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "sdkconfig.h"
#include "jank_beatlink.h"
#include "jank_conf.h"
#include "jank_peer.h"
#include "jank_sysex.h"
#include "jb_beat.h"
#include "jb_down.h"
#include "jb_follow.h"
#include "jb_graded.h"
#include "jb_kick.h"
#include "jb_lost.h"
#include "jb_resume.h"
#include "jb_song.h"
#include "jb_robust.h"
#include "jb_skip.h"
#include "jb_mode.h"
#include "jb_rate.h"
#include "jb_weak.h"
#include "jank_fold.h"
#include "jank_live.h"
#include "usb_audio.h"

#define BEAT_PRIO 3
#define BEAT_STACK 16384               /* measured in QEMU with a synthetic loop: 8192 left < 300 bytes free (ModeBpm, the reports) */
#define POLL_MS 5
#define STEP_EVERY_US 250000           /* the Link session is looked after this often, and at each tracked beat */
#define WINDOW_US 1000000              /* the CPU share is over this long */
#define SILENT_GAP_US 200000         /* the source open and no samples this long: that time counts as silence (jank_live.h) */
#define BEAT_DEFER JW_STRONG_HALF       /* frames a tracked beat waits for its strength (jb_weak.h) */
#define RATE_QUEUE 4                   /* rate events waiting for the next tracked beat */
#define DOWN_JUMP_RESET 0              /* 1: a tempo jump (jb_mode's jumps) resets the bar; off: those are mostly short excursions
                                        * in breaks, and resetting cost 3 points and 2.4x the moves (beat-listener
                                        * studies/downbeat/results/v2_stream_check.md) */

static const char *TAG = "beat";
static struct jb_beat s_b;
static struct jb_follow s_follow;      /* the estimator: ModeBpm and the glide (core/beat/jb_follow.h) */
static struct jb_rate s_rate;          /* the spectral rate, fed by the front end's tap (core/beat/jb_rate.h) */
static struct jr_store *s_rate_store;  /* its tables, ring and work (59 KB, PSRAM); NULL: no rate */
static uint8_t s_rate_cfg = 1;         /* the setting as last read (`set rate`) */
static uint32_t s_rate_cycles, s_rate_max_cycles;   /* its work this second (the tap's calls) */
static uint32_t s_rate_us, s_rate_max_us;           /* ... over the last second, published */
static struct jb_weak s_weak;          /* the weak-signal hold (core/beat/jb_weak.h, M2j) */
static struct jw_store *s_weak_store;  /* its activation ring and beat lists (7.3 KB, PSRAM); NULL: off, M2i's follow */
static struct jb_robust s_robust;      /* the robust follower, all-d-f60 (core/beat/jb_robust.h, M2m): the A/B against the tuned one */
static struct jrb_store *s_robust_store;   /* its activation ring (8 KB, PSRAM); NULL: no robust follower, `set follower robust` stays tuned */
static uint8_t s_follower;             /* the one running: 0 tuned (M2j, today's code path), 1 robust */
static struct jb_skip s_skip;          /* SKIP (core/beat/jb_skip.h): off with the tuned follower; ACT / PAT on the robust one */
static uint8_t s_skip_mode;            /* enum jsk_mode running */
static struct jg_tuned s_gtuned;       /* the tuned follower's graded confidence (jb_graded.h jg_tuned_step) */
static uint8_t s_conf_graded;          /* the shown confidence at the last step is the graded one (the gate value is s_conf_gate_pct) */
static uint8_t s_conf_gate_pct;        /* the gate's confidence at the last step, percent: what STOP / START, COAST, the LED and the notch judge */
static uint8_t s_graded, s_coast;      /* the shown confidence is the graded one (display only); the outputs run under the threshold */
static int s_last_jumps;               /* the follower's jumps at the last confidence step (a jump since is `jumped`) */
static uint32_t s_conf_cycles;         /* follow_conf's work this second ... */
static uint32_t s_conf_us;             /* ... over the last second, published */
static struct {
    uint32_t refresh_us, refresh_max_us, gate_us, gate_max_us, tick_us;
} s_rb_pub;                            /* the robust counters over the last second */
static uint8_t s_pend;                 /* a tracked beat waiting BEAT_DEFER frames for its strength: */
static int64_t s_pend_index;           /* ... its frame */
static double s_pend_t;                /* ... its time */
static struct {
    double t, ratio;
} s_rate_q[RATE_QUEUE];                /* rate events decided, proposed before the first tracked beat after them (chain.py) */
static uint8_t s_rate_qn;
static uint8_t s_conf_ran;             /* the automatic layer ran at the last step (a tempo, the gate open, no silence): the
                                        * gate is then judged on the weak confidence */
static uint32_t s_weak_holds;          /* steps the weak confidence held the gate open with the strict one under it */
static struct jb_lost s_lost;          /* LOST (core/beat/jb_lost.h): when the tuned follower's outputs stop once they run */
static struct jb_kick *s_kick;         /* the kick band and the input level per tracker frame, LOST's evidence (core/beat/jb_kick.h;
                                          PSRAM); NULL: no RAM, LOST on strong beats alone */
static struct jb_song *s_song;         /* NEW SONG (core/beat/jb_song.h, `set newsong`; PSRAM); NULL: no RAM, off */
static uint8_t s_song_on;              /* the setting as last read (the beat task) */
static struct jb_resume s_resume;      /* RESUME (core/beat/jb_resume.h): a paused song back on the beat at once */
static uint8_t s_resume_now;           /* RESUME seeded the follower at s_resume_at: the next step opens the gate */
static double s_resume_at, s_resume_fired = -1e9;
static float s_resume_prior[JB_DBN_MAX_INTERVALS];   /* the DBN's tempo prior on the remembered tempo (jb_dbn.h) */
static uint8_t s_resume_prior_on;
static uint8_t s_conf_hold;            /* M2k: the weak confidence holds its last value (no evidence): shown eased */
static double s_conf_shown, s_conf_at; /* the confidence shown (jcf_show_step) and the stream time of its step */
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
static uint8_t s_act_pct;
static struct jcf s_conf;              /* LOW CONFIDENCE: the tracked beats kept and the silence (core/jank_conf.h) */
static uint8_t s_conf_pct;             /* the confidence at the last step, percent */
static uint8_t s_conf_open;            /* ... at the threshold + 5, where the gate opens (core/jank_hold.h jho_in.conf_open) */
static uint8_t s_gated;                /* its gate (STOP / START under the threshold), at the last step */
static struct jlv s_live;              /* sound on the source (AUDIO mode) and the input meter: the tracker's own samples */
static int64_t s_fed_us;               /* when samples last came ... */
static int64_t s_fed_samples;          /* ... s_samples then */
static uint8_t s_audio_live;           /* the source open and sounding: AUDIO mode, as last decided by this task */

struct step_ctx {
    struct jbl_in in;
    struct jbl_bar bar;
    uint32_t quantum;
    double bpm;                        /* the tracker's own (unfolded) tempo, 0: none */
    int8_t nudge;                      /* NUDGE BEAT asked for at this step, half beats (0 none) ... */
    int8_t nudge_rc;                   /* ... its result (enum beat_nudge_rc) */
};

/* NUDGE BEAT (core/jank_beatlink.h jbl_nudge): asked by the control line or the device's buttons (beat_nudge, which waits
 * for the answer), taken by the beat task at its next pass (5 ms) and applied in that pass's step under the bridge's
 * lock. Under s_mux */
static struct {
    int8_t halves, rc, total;
    uint32_t asked, done;
    char view[96];                     /* the downbeat estimator before it, for the answer (the refit's hand label) */
} s_nudge;

/* AUDIO mode's fold and AUTO RANGE (jank_fold.h): the beat task's; the control line asks through s_fold_req */
enum { FOLD_REQ_NONE = 0, FOLD_REQ_UP, FOLD_REQ_DOWN, FOLD_REQ_ONE, FOLD_REQ_RESET };
static struct jfo s_fold;
static volatile uint8_t s_fold_req;
/* M2k: a fold change or a Start asks for a new start for the bar (bar_now); the clocks wait until the beat task's step
 * made that placement (beat_bar_settling, bridge.c's jho_in.bar_settling), so they start on the placed 1. Asked: s_bar_req
 * counts up; done: the step that took it sets s_bar_done to it */
static uint32_t s_bar_req, s_bar_done;

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
    q->ok = (uint8_t)(jp_in_session(p) && jp_playing(p));
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
    enum jb_mode_kind mode = jb_follow_at(&s_follow, at_s);
    if (!jb_follow_bpm_at(&s_follow, at_s, &bpm))
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
        usb_beat_line(line, n);
}

/* a tracked beat to the downbeat estimator, after ModeBpm took it (live/downbeat.py Driver.tracked) */
static void down_tracked(double t, int jumps_before)
{
    double tb, period;
    int running;
    if (DOWN_JUMP_RESET && s_follow.m.jumps != jumps_before && jd_counting(&s_down))
        jd_reset(&s_down);              /* a tempo jump: a new bar to find */
    running = jb_follow_at(&s_follow, t) != JB_STOPPED;
    if (!jb_follow_grid(&s_follow, &tb, &period))
        jd_stop(&s_down);
    else if (!running && jd_counting(&s_down))
        jd_stop(&s_down);               /* ModeBpm stopped: a reset; shadow beats from its next grid on */
    else
        jd_grid(&s_down, t, tb, period, running);
}

/* the front end's tap: each frame's |FFT| to the spectral rate (it returns at once while inactive), its cycles counted */
static uint32_t cycles_now(void) { return esp_cpu_get_cycle_count(); }

/* the profile (M2m): the tracker task's cycles per stage this second, and core 0's idle share */
enum { P_PULL, P_METER, P_FE, P_RATE, P_NET, P_DBN, P_DOWN, P_ACT, P_FEED, P_COAST, P_RSTEP, P_CONF, P_STEP, P_BURN, P_N };
static uint32_t s_prof[P_N], s_prof_pub[P_N], s_pmax[P_N], s_pmax_pub[P_N];
static uint32_t s_hop_win_max_us, s_hop_over10, s_hop_win_max_pub, s_hop_over10_pub;
static uint32_t s_busy1_last, s_busy1_pub, s_busy1_ok, s_busy1_started;   /* core 1: the MIDI clock interrupt's cycles (midi_busy_cycles) at the last window, its share published */
static uint32_t s_idle_cyc, s_idle_last, s_idle_pub;     /* core 0's idle task: cycles spent spinning in it */
static uint16_t s_burn_us;                               /* BENCH: extra work a frame (set burn) */
static int64_t s_burn_until_us;                          /* ... only until then (60 s after it was set): a forgotten burn must not stay */
#define BURN_MAX_S 60
#define PROF_BEGIN uint32_t prof_c0 = esp_cpu_get_cycle_count()
#define PROF_END(k) do { uint32_t c_ = esp_cpu_get_cycle_count() - prof_c0; s_prof[k] += c_; if (c_ > s_pmax[k]) s_pmax[k] = c_; } while (0)

/* core 0's idle task (it spins here instead of waiting for an interrupt, so that its time can be counted: a gap between two
 * calls longer than a spin is time some other task or interrupt had) */
static bool idle_hook(void)
{
    uint32_t n = esp_cpu_get_cycle_count(), d = n - s_idle_last;
    s_idle_last = n;
    if (d < 2000u)
        s_idle_cyc += d;
    return false;
}

static void rate_tap(void *ctx, int64_t index, const float *mag)
{
    (void)ctx;
    if (!s_rate.active)
        return;
    uint32_t c0 = esp_cpu_get_cycle_count();
    jb_rate_frame(&s_rate, index, mag);
    uint32_t c = esp_cpu_get_cycle_count() - c0;
    s_rate_cycles += c;
    if (c > s_rate_max_cycles)
        s_rate_max_cycles = c;
}

/* after a frame's beat: a rate step decided in this frame waits for the next tracked beat (then proposed to the follower,
 * which that beat judges: FollowBpm.propose_ratio), and the rate runs only while the follower glides, the setting on and
 * sound */
static void rate_step(double now)
{
    double t, ratio;
    if (!s_rate_store)
        return;
    if (jb_rate_take(&s_rate, &t, &ratio)) {
        if (s_rate_qn == RATE_QUEUE) {          /* (never: 4.5 s apart) the oldest goes; the follower keeps the last anyway */
            memmove(s_rate_q, s_rate_q + 1, sizeof s_rate_q[0] * (RATE_QUEUE - 1u));
            s_rate_qn--;
        }
        s_rate_q[s_rate_qn].t = t;
        s_rate_q[s_rate_qn++].ratio = ratio;
    }
    jb_rate_set_active(&s_rate, s_rate_cfg && s_audio_live && jb_follow_state(&s_follow, now) == JF_GLIDE);
}

/* a tracked beat into the follower (chain.py's order): the rate events decided before it, the weak follower (strong:
 * the follower's update; weak: a gentle observation), the fast jump, the lines cleared on any jump */
static void feed_beat_inner(double t)
{
    int jumps = s_follow.m.jumps;
    uint8_t i = 0;
    for (; i < s_rate_qn && s_rate_q[i].t <= t; i++)
        jb_follow_propose(&s_follow, s_rate_q[i].ratio, s_rate_q[i].t, 1, s_rate_q[i].t - JR_LAG_S);
    if (i) {
        memmove(s_rate_q, s_rate_q + i, sizeof s_rate_q[0] * (size_t)(s_rate_qn - i));
        s_rate_qn = (uint8_t)(s_rate_qn - i);
    }
    const uint32_t resyncs = s_skip.resyncs;
    jsk_update(&s_skip, &s_weak, &s_follow, t);    /* (SKIP off: jw_update) */
    jb_follow_fast_jump(&s_follow, t, JF_FAST_JUMP_REL);
    if (s_follow.m.jumps != jumps || s_skip.resyncs != resyncs)
        jcf_clear_lines(&s_conf);       /* a new grid (a jump, or SKIP's phase re-sync): the old one's lines judge no beat */
    if (s_down_seg)
        down_tracked(t, jumps);
    jcf_beat(&s_conf, t);              /* (not while silent: what comes after silence is new music) */
    s_beat_seen = 1;
    s_beat_n++;
    report('b', t);
}

static void feed_beat(double t)
{
    PROF_BEGIN;
    feed_beat_inner(t);
    PROF_END(P_FEED);
}

static void on_frame(void *ctx, const struct jb_frame *f)
{
    double now = (double)f->index / (double)JB_FPS;
    (void)ctx;
    s_hops++;
    {
        PROF_BEGIN;
        if (s_down_seg && jd_frame(&s_down, f->index, f->bands))
            s_beat_seen = 1;            /* a decision: the session's bar is looked after now */
        PROF_END(P_DOWN);
    }
    if (s_song && s_song_on)
        js_frame(s_song, f->bands);
    if (f->act > s_win_act)
        s_win_act = f->act;
    {
        PROF_BEGIN;
        jw_act(&s_weak, f->index, f->act);
        jrb_act(&s_robust, f->index, f->act);   /* (the robust follower's agreement and the tuned follower's graded tempo part read it) */
        PROF_END(P_ACT);
    }
    if (f->beat) {
        if (s_pend)
            feed_beat(s_pend_t);        /* (never: tracked beats are 0.33 s apart at least) */
        s_pend = 0;
        if (jw_on(&s_weak)) {
            s_pend = 1;                 /* its strength needs the frames after it */
            s_pend_index = f->index;
            s_pend_t = f->time;
        } else
            feed_beat(f->time);
    }
    if (s_pend && f->index >= s_pend_index + BEAT_DEFER) {
        s_pend = 0;
        feed_beat(s_pend_t);
    }
    {
        PROF_BEGIN;
        jsk_coast(&s_skip, &s_weak, &s_follow, now);  /* the beat-less lines whose evidence is in; SKIP's activation path (off: jw_coast) */
        PROF_END(P_COAST);
    }
    {
        PROF_BEGIN;
        rate_step(now);
        PROF_END(P_RSTEP);
    }
    if (s_burn_us && esp_timer_get_time() < s_burn_until_us) {   /* BENCH: busy for s_burn_us more, to find where the core overloads */
        PROF_BEGIN;
        const uint32_t span = (uint32_t)s_burn_us * (uint32_t)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
        while (esp_cpu_get_cycle_count() - prof_c0 < span)
            ;
        PROF_END(P_BURN);
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
    /* (the user's PHASE stays: the automatic layer never changes it, M2h) */
    s_bar_now = 1;
    jb_beat_reset(&s_b);
    jre_disarm(&s_resume);             /* (the tempo stays remembered) */
    s_resume_now = 0;
    s_resume_prior_on = 0;             /* (jb_beat_reset: the DBN without a prior) */
    jb_dbn_set_prior(&s_b.dbn, NULL);
    if (s_kick)
        jbk_reset(s_kick);             /* (its hops are the tracker's frames) */
    if (s_song)
        js_reset(s_song);
    jb_follow_init(&s_follow);
    jw_init(&s_weak, s_weak_store, NULL, 0, 1);    /* the frame count starts over with the tracker */
    jl_init(&s_lost);
    jrb_init(&s_robust, s_robust_store);           /* ... and the robust follower's: no memory, no ring */
    s_robust.clock = cycles_now;
    jsk_init(&s_skip, &s_robust);                  /* SKIP starts over with the estimator */
    jsk_set_mode(&s_skip, s_follower ? (enum jsk_mode)s_skip_mode : JSK_OFF);
    s_robust.sk = jsk_on(&s_skip) ? &s_skip : NULL;
    jb_dbn_set_prior(&s_b.dbn, NULL);
    jg_tuned_reset(&s_gtuned);
    if (s_follower)
        jrb_attach(&s_robust, &s_follow, &s_weak);  /* the lock and jump gate (tuned: none, today's follower bit for bit) */
    s_last_jumps = 0;
    s_pend = 0;
    s_rate_qn = 0;
    s_conf_ran = 0;
    s_conf_hold = 0;
    s_coast = 0;
    s_conf_shown = s_conf_at = 0.0;
    jb_rate_set_active(&s_rate, 0);    /* (its history starts over at the next glide) */
    restart_down();
    jcf_init(&s_conf);                 /* a resync: no beats, no gate (engine.py resync) */
    s_gated = 0;
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
    int on = settled;                  /* (settled: in a session, joined or AUDIO mode's own) */
    double session_bpm = on && jp_playing(p) && jp_mpb(p) > 0 ? 60000000.0 / (double)jp_mpb(p) : 0.0;
    if (c->in.running && !s_fold.had_tempo)
        s_bar_now = 1;                 /* a new lock (a new track): a new start for the bar */
    if (jfo_step(&s_fold, c->in.running, c->bpm, s_follow.m.jumps, session_bpm, (uint32_t)(now / 1000)))
        s_bar_now = 1;
    if (c->in.running) {
        int64_t beat;
        double period;
        /* the user's nudge (jbl_nudge_us): the session's beats sit this long before the folded grid's. A /2 or /4 anchors
         * on the session's grid as it would be without it (an integer beat there), so the choice is never a tie */
        int64_t off = jbl_nudge_us(&s_jbl, c->in.period_us, s_fold.exp);
        double pos = on ? (double)lk_beat_at(&p->k, c->in.beat_us - off) / (double)LK_UBEAT : 0.0;
        jfo_grid(&s_fold, c->in.beat_us, c->in.period_us, on, pos, on ? (double)jp_mpb(p) : 0.0, &beat, &period);
        c->in.beat_us = beat - off;
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
    if (c->nudge)                      /* first: the follower below then finds the session where the nudge put it */
        c->nudge_rc = (int8_t)(jbl_nudge(&s_jbl, &p->k, &p->start, settled, &c->in, c->nudge, s_fold.exp, now) ? BEAT_NUDGE_LIMIT : BEAT_NUDGE_OK);
    fold_step(p, now, settled, c);
    c->in.clock_playing = (uint8_t)(midi_is_playing() && !midi_start_pending());   /* AT START: no move under it (M2k) */
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
    *mode = jb_follow_at(&s_follow, now_s);
    *bpm = 0.0;
    memset(in, 0, sizeof *in);
    in->passive = (uint8_t)!s_audio_live;      /* LINK mode (no sound): the tracker neither starts nor stops the session */
    if (!s_audio_live || *mode == JB_STOPPED || !jb_follow_bpm_at(&s_follow, now_s, bpm) || !jb_follow_grid(&s_follow, &tb, &period))
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
    b.nudge_halves = s_jbl.nudge;
    b.level_db = s_live.rms_db;
    b.level_min_db = s_live.min_db;
    b.level_max_db = s_live.max_db;
    b.peak_db = s_peak_db;
    b.act_pct = s_act_pct;
    b.conf_pct = s_conf_pct;
    b.conf_gate_pct = s_conf_gate_pct;
    b.conf_graded = s_conf_graded;
    b.conf_hold = s_conf_hold;
    b.gated = s_gated;
    b.silent = s_conf.silent;
    b.conf_open = s_conf_open;
    b.follow = (uint8_t)jb_follow_state(&s_follow, (double)s_samples / (double)UAC_RATE);
    b.drift_ppm = jb_follow_drift_ppm(&s_follow);
    b.manoeuvres = s_follow.manoeuvres_total;
    b.rate_on = (uint8_t)(s_rate_store != NULL);
    b.rate_active = s_rate.active;
    b.rate_x10000 = (int32_t)lround(s_follow.last_ratio * 10000.0);
    b.rate_events = s_rate.events;
    b.rate_taken = s_follow.ratios_taken;
    b.rate_refused = s_follow.ratios_refused;
    b.rate_ignored = s_follow.ratios_ignored;
    b.rate_us = s_rate_us;
    b.rate_max_us = s_rate_max_us;
    b.weak_on = (uint8_t)jw_on(&s_weak);
    b.weak_holds = s_weak_holds;
    b.coasts = s_weak.coasted;
    b.weak_beats = s_weak.weak_beats;
    b.fast_jumps = s_follow.fast_jumps;
    b.follower = s_follower;
    b.skip = s_skip_mode;
    b.graded = s_graded;
    b.coast = s_coast;
    b.skip_resyncs = (uint16_t)(s_skip.resyncs_total > 65535u ? 65535u : s_skip.resyncs_total);
    b.skip_act_resyncs = (uint16_t)(s_skip.act_resyncs_total > 65535u ? 65535u : s_skip.act_resyncs_total);
    b.skip_gated = (uint16_t)(s_skip.gated_ticks > 65535u ? 65535u : s_skip.gated_ticks);
    memcpy(b.prof_us, s_prof_pub, sizeof b.prof_us);
    memcpy(b.prof_max_us, s_pmax_pub, sizeof b.prof_max_us);
    b.hop_win_max_us = s_hop_win_max_pub;
    b.hop_over10 = s_hop_over10_pub;
    b.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    memcpy(b.gap_at_ms, u.gap_at_ms, sizeof b.gap_at_ms);
    memcpy(b.gap_len_ms, u.gap_len_ms, sizeof b.gap_len_ms);
    b.core0_idle_x10 = s_idle_pub;
    b.core1_busy_x10 = s_busy1_pub;
    b.core1_ok = (uint8_t)s_busy1_ok;
    b.burn_us = s_burn_us;
    b.conf_us = s_conf_us;
    b.robust_refresh_us = s_rb_pub.refresh_us;
    b.robust_refresh_max_us = s_rb_pub.refresh_max_us;
    b.robust_gate_us = s_rb_pub.gate_us;
    b.robust_gate_max_us = s_rb_pub.gate_max_us;
    b.robust_tick_us = s_rb_pub.tick_us;
    b.robust_asked = s_robust.asked;
    b.robust_refused_memory = s_robust.refused_memory;
    b.robust_refused_ratio = s_robust.refused_ratio;
    b.robust_coast_ticks = s_robust.coasting_ticks;
    b.robust_prior_ticks = s_robust.prior_ticks;
    portENTER_CRITICAL(&s_mux);
    s_pub = b;
    portEXIT_CRITICAL(&s_mux);
}

/* LOW CONFIDENCE at a step: the confidence of the tracked beats against the lines the follower's grid published (0 in
 * LINK mode or silence; the grid records its lines while it gives a tempo: studies/advanced-follow replay.py) and the
 * gate's next state (core/jank_conf.h; engine.py status: conf under the threshold, back at threshold + 5). M2j
 * (studies/tempo-changes chain.py conf "weak"): the strict line confidence opens the gate; once the outputs run, the weak
 * one (jcf_weak_confidence: lines with no evidence do not count) holds it */
static void follow_conf(const struct jk_cfg *cfg)
{
    double tb = 0.0, period = 0.0, strict = 0.0, conf = 0.0, bpm, now = (double)s_samples / (double)UAC_RATE;
    double conf_shown = 0.0;
    int has_bpm = 0, has = 0, use_graded = 0;
    uint8_t weak_picked = 0, held = 0, lost = 0;
    const uint32_t mark = s_follow.seeds + (uint32_t)s_follow.m.jumps;    /* (RESUME: has the follower locked anew?) */
    if (!s_follower && s_resume.has) {
        /* the DBN held on the remembered tempo while RESUME waits and JRE_PRIOR_S after it resynced: after a pause its own
         * tempo drifts (measured: 140 -> 93 BPM after 10 min of -40 dB noise) and its beats would drag the grid off */
        const uint8_t want = (uint8_t)(s_resume.armed || now - s_resume_fired < JRE_PRIOR_S);
        if (want && !s_resume_prior_on)
            jrb_tempo_prior(&s_b.dbn, 60.0 / s_resume.period, JRB_PRIOR_WIDTH, JRB_PRIOR_FLOOR, s_resume_prior);
        if (want != s_resume_prior_on)
            jb_dbn_set_prior(&s_b.dbn, want ? s_resume_prior : NULL);
        s_resume_prior_on = want;
    }
    if (s_audio_live && !s_conf.silent) {
        double t_res;
        if (!s_follower && s_resume.armed
            && jre_find(&s_resume, s_robust_store ? s_robust_store->ring : NULL, JRB_ACT_RING, s_robust.act_n, &t_res)) {
            jre_seed(&s_follow, t_res, s_resume.period);  /* the song again, at its remembered tempo, on the new phase */
            jcf_clear_lines(&s_conf);
            s_resume_now = 1;
            s_resume_at = t_res;
            s_resume_fired = now;
        }
        has = jb_follow_grid(&s_follow, &tb, &period);
        has_bpm = jb_follow_bpm_at(&s_follow, now, &bpm);
        jcf_line_tick(&s_conf, has_bpm ? has : 0, tb, period, now);
        strict = jcf_lines_confidence(s_conf.t, s_conf.n, s_conf.line, s_conf.lines, has, period, now, JCF_WINDOW_S,
                                      JCF_DELAY_S, JCF_TOL);
        conf = strict;
        if (jw_on(&s_weak)) {
            double weak = jcf_weak_confidence(jw_strong(&s_weak), s_weak.n_strong, jw_seen(&s_weak), s_weak.n_seen,
                                              jw_weak(&s_weak), s_weak.n_weak, s_conf.line, s_conf.lines, has, period, now,
                                              JW_MIN_OBS, &s_weak.last_conf, &held);
            /* LOST (the tuned follower with STOP / START): near silence, beats off the grid or (option) a new song close
             * the gate; a drop of the same song holds it; beats or kicks on the held grid again open it at once */
            if (!s_follower && cfg->lowconf == JCF_STOP && cfg->threshold) {
                uint8_t back = 0;
                if (s_resume_now) {     /* RESUME: run at once, LOST judging the lines from the resumed beat on */
                    s_resume_now = 0;
                    jl_clear(&s_lost);
                    s_lost.next_line = s_resume_at;
                    s_gated = 0;
                    s_conf_ran = 1;
                }
                if (!s_lost.quiet_run)
                    jre_sound(&s_resume, now);
                jl_hold_back(&s_lost, jre_holds(&s_resume, mark, now));
                jl_set_pause_db(&s_lost, (double)cfg->pause_db);
                lost = (uint8_t)jl_step(&s_lost, jw_strong(&s_weak), s_weak.n_strong, s_kick, has_bpm && has, tb, period,
                                        now, s_conf_ran, s_song && cfg->newsong && js_new(s_song), &back);
                if (back) {
                    s_gated = 0;
                    s_conf_ran = 1;
                }
                if (lost && s_lost.cause == JLO_QUIET)
                    jre_arm(&s_resume, mark);
            } else
                jl_clear(&s_lost);
            weak_picked = (uint8_t)(!s_gated && s_conf_ran);
            conf = jcf_weak_pick(strict, weak, s_gated, s_conf_ran);
        }
    } else {
        jl_clear(&s_lost);              /* silence or LINK mode: what comes next is new music */
        if (s_song)
            js_reset(s_song);
        if (s_audio_live && !s_follower)
            jre_arm(&s_resume, mark);   /* (silence: RESUME watches the beats that come back) */
    }
    /* shown: a hold eases toward the threshold instead of a frozen value (M2k); the gate below takes the real one */
    s_conf_hold = (uint8_t)(weak_picked && held);
    if (s_follower) {
        /* M2m, the robust follower: the gate judges the agreement of the lines, the comb's tempo, the phase and the DBN's
         * mass (jrb_tick: also the coast, the tempo memory and the DBN's prior); that is the confidence shown */
        struct jrb_in in = {.now = now, .silent = !(s_audio_live && !s_conf.silent), .has_bpm = has_bpm, .bpm = has_bpm ? bpm : 0.0,
                            .has_grid = has, .period = period,
                            .conf = conf, .strict = strict, .has_kf = s_follow.has_kf,
                            .jumped = s_follow.m.jumps != s_last_jumps, .was_running = s_conf_ran, .lines = s_conf.line,
                            .n_lines = s_conf.lines, .threshold_pct = cfg->threshold, .keep_running = cfg->lowconf != JCF_STOP};
        struct jrb_out o;
        if (in.silent)
            in.conf = in.strict = 0.0;
        jrb_tick(&s_robust, &s_b.dbn, &in, &s_gated, &o);
        s_last_jumps = s_follow.m.jumps;
        conf = o.gconf;
        s_coast = o.coasting;
        if (s_graded && !in.silent) {
            /* the graded confidence (jb_graded.h) is shown instead; the gate above keeps judging the agreement */
            conf_shown = jg_graded(has ? jg_soft_f(jw_seen(&s_weak), s_weak.n_seen, s_conf.line, s_conf.lines, period, now) : 0.0,
                                   jg_jitter(jw_seen(&s_weak), s_weak.n_seen), s_robust.v_tempo, s_robust.v_dbn);
            use_graded = 1;
        }
    } else {
        const uint8_t was = s_gated;
        s_gated = jcf_gate(s_gated, conf, cfg->lowconf, cfg->threshold);
        /* the user (2026-11-19): once the outputs run, a drop of the same song holds them: the confidence no longer stops
         * them, LOST does (and silence, the follower giving up); the threshold decides the start */
        if (cfg->lowconf == JCF_STOP && cfg->threshold && !was && s_conf_ran)
            s_gated = 0;
        if (jre_holds(&s_resume, mark, now))
            s_gated = 1;                /* after a pause the old grid does not come back by itself (RESUME) */
        if (lost)
            s_gated = 1;
        s_coast = 0;
        if (s_graded && s_audio_live && !s_conf.silent) {
            /* the tuned follower's graded confidence (jb_graded.h) is shown instead; the gate above is unchanged */
            conf_shown = jg_tuned_step(&s_gtuned, &s_robust, &s_b.dbn, jw_seen(&s_weak), s_weak.n_seen, s_conf.line, s_conf.lines,
                                       has, period, now);
            use_graded = 1;
        }
    }
    if (use_graded) {
        s_conf_shown = conf_shown;      /* a measurement: no HOLD easing */
        s_conf_hold = 0;
    } else {
        s_conf_shown = jcf_show_step(s_conf_shown, conf, s_conf_hold, (double)cfg->threshold / 100.0,
                                     now > s_conf_at ? now - s_conf_at : 0.0);
        if (s_conf_hold)
            s_coast = 0;                /* (a HOLD is not a COAST) */
    }
    s_conf_at = now;
    s_conf_pct = jcf_pct(s_conf_shown);
    s_conf_gate_pct = jcf_pct(conf);
    s_conf_graded = (uint8_t)use_graded;
    /* (a closed gate's test: open at threshold + 5; never while LOST, whose confidence the DBN's own beats can fill: a
     * Start keeps the outputs running until strong beats are back) */
    s_conf_open = (uint8_t)(!lost && !jcf_gate(1, conf, JCF_STOP, cfg->threshold));
    if (weak_picked && !s_gated && cfg->lowconf == JCF_STOP && strict < (double)cfg->threshold / 100.0)
        s_weak_holds++;
    s_conf_ran = (uint8_t)(s_audio_live && has_bpm && !s_gated && !s_conf.silent);
    if (s_conf_ran) {
        jre_disarm(&s_resume);
        if (!s_follower && jb_follow_state(&s_follow, now) == JF_GLIDE && jb_follow_trusted(&s_follow))
            jre_remember(&s_resume, period, tb, s_robust_store ? s_robust_store->ring : NULL, JRB_ACT_RING, s_robust.act_n);
    }
}

/* the tracker's work on one pass's samples; the time it took is the figure for the CPU */
static void work(const int16_t *x, size_t n)
{
    uint32_t h0 = s_hops;
    int64_t t0 = esp_timer_get_time();
    uint32_t c0 = esp_cpu_get_cycle_count();
    uint64_t sumsq = 0;
    uint32_t m0 = esp_cpu_get_cycle_count();
    for (size_t i = 0; i < n; i++) {
        int32_t a = x[i] < 0 ? -(int32_t)x[i] : x[i];
        if (a > s_win_peak)
            s_win_peak = a;
        sumsq += (uint32_t)a * (uint32_t)a;     /* (a <= 32768: fits 32 bits; the 64-bit multiply was a library call) */
    }
    jlv_push(&s_live, x, n);
    jcf_samples(&s_conf, sumsq, n);    /* the silence first: a block that begins it keeps none of its beats (engine.py) */
    s_prof[P_METER] += esp_cpu_get_cycle_count() - m0;
    if (s_kick)
        jbk_push_s16(s_kick, x, n);    /* the same samples as the tracker: its hop k is the tracker's frame k */
    jb_beat_push_s16(&s_b, x, n, on_frame, NULL);
    uint32_t cycles = esp_cpu_get_cycle_count() - c0;
    int64_t us = esp_timer_get_time() - t0;
    uint32_t frames = s_hops - h0;
    if ((uint32_t)us > s_hop_win_max_us)
        s_hop_win_max_us = (uint32_t)us;
    if (us > 10000)
        s_hop_over10++;
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
        size_t gap = (size_t)((now - s_fed_us) * (int64_t)UAC_RATE / 1000000);
        jlv_push(&s_live, NULL, gap);
        jcf_samples(&s_conf, 0, gap);  /* a stalled stream is silence for LOW CONFIDENCE too */
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

/* a nudge asked for and not answered yet: its half beats (0 none). The step runs in this pass */
static int8_t take_nudge(void)
{
    int8_t h = 0;
    portENTER_CRITICAL(&s_mux);
    if (s_nudge.asked != s_nudge.done)
        h = s_nudge.halves;
    portEXIT_CRITICAL(&s_mux);
    return h;
}

static const char *const BAR_NAME[] = {"off", "listening", "locked", "manual"};

/* the nudge's answer; the estimator's view before it is logged and answered (the Python player logs each nudge with it, a
 * hand label for a refit: here the serial log and the answer carry it). AUTO BAR stays in charge: the PHASE rides on its
 * bar (M2h, jank_beatlink.h jbl_bar_step) */
static void finish_nudge(int8_t halves, int8_t rc)
{
    char view[sizeof s_nudge.view] = "";
    if (s_down_seg) {
        struct jd_view v;
        jd_view(&s_down, &v);
        snprintf(view, sizeof view, "bar=%s\nbar_phi=%d\nbar_count=%ld\nbar_scores=%.2f,%.2f,%.2f,%.2f\n", BAR_NAME[v.state & 3u],
                 v.phi, (long)v.count, (double)v.scores[0], (double)v.scores[1], (double)v.scores[2], (double)v.scores[3]);
    }
    if (rc == BEAT_NUDGE_OK)
        ESP_LOGI(TAG, "nudge %+d/2 beat: total %+d/2, %u nudges; before it %s", halves, s_jbl.nudge, (unsigned)s_jbl.nudges, view);
    portENTER_CRITICAL(&s_mux);
    s_nudge.rc = rc;
    s_nudge.total = s_jbl.nudge;
    memcpy(s_nudge.view, view, sizeof view);
    s_nudge.done = s_nudge.asked;
    s_pub.nudge_halves = s_jbl.nudge;  /* (status says it at once, not at the next publish) */
    portEXIT_CRITICAL(&s_mux);
}

static volatile uint32_t s_loop_max_us;     /* the longest pass of the task since boot (ABOUT screen) */
uint32_t beat_loop_max_us(void) { return s_loop_max_us; }

static void beat_task(void *arg)
{
    int16_t x[JB_HOP];
    uint32_t epoch = uac_epoch();
    int64_t last_step = 0;
    uint32_t cpu_x10 = 0;
    enum jb_mode_kind mode = JB_STOPPED;
    double bpm = 0.0;
    int8_t nudge;
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
        const int64_t pass_start = esp_timer_get_time();
        for (;;) {
            uint32_t p0 = esp_cpu_get_cycle_count();
            n = uac_pull(x, JB_HOP);
            s_prof[P_PULL] += esp_cpu_get_cycle_count() - p0;
            if (n == 0u)
                break;
            work(x, n);
            if (esp_timer_get_time() - pass_start > 12000)
                break;                  /* behind the audio: the step (settings, publish) still runs, the ring may overflow */
        }
        follow_sound(esp_timer_get_time());
        uint32_t bar_req = __atomic_load_n(&s_bar_req, __ATOMIC_ACQUIRE);
        take_fold_req();
        if (bar_req != s_bar_done)
            s_beat_seen = 1;            /* a Start / a fold: the step now, the clocks wait for it (M2k) */
        nudge = take_nudge();
        if (jb_follow_at(&s_follow, (double)s_samples / (double)UAC_RATE) == JB_STOPPED) {
            if (s_down_seg && jd_counting(&s_down))
                jd_stop(&s_down);       /* 30 s without a steady beat: the bar starts over */
        }
        now = esp_timer_get_time();
        if (now - pass_start > (int64_t)s_loop_max_us)
            s_loop_max_us = (uint32_t)(now - pass_start);
        if (now - s_win_start >= WINDOW_US) {
            cpu_x10 = (uint32_t)(s_win_busy * 1000 / (now - s_win_start));
            s_rate_us = s_rate_cycles / (uint32_t)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;      /* (cycles at the CPU's MHz: us) */
            s_rate_max_us = s_rate_max_cycles / (uint32_t)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
            {
                const uint32_t mhz = (uint32_t)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
                s_prof[P_FE] = s_b.cyc_fe > s_rate_cycles ? s_b.cyc_fe - s_rate_cycles : 0u;   /* (the rate's tap is inside the front end) */
                s_prof[P_RATE] = s_rate_cycles;
                s_pmax[P_FE] = s_b.max_fe;
                s_pmax[P_NET] = s_b.max_net;
                s_pmax[P_DBN] = s_b.max_dbn;
                s_b.max_fe = s_b.max_net = s_b.max_dbn = 0;
                s_prof[P_NET] = s_b.cyc_net;
                s_prof[P_DBN] = s_b.cyc_dbn;
                s_b.cyc_fe = s_b.cyc_net = s_b.cyc_dbn = 0;
                s_prof[P_CONF] = s_conf_cycles;
                for (int k = 0; k < P_N; k++) {
                    s_prof_pub[k] = s_prof[k] / mhz;
                    s_pmax_pub[k] = s_pmax[k] / mhz;
                    s_prof[k] = 0;
                    s_pmax[k] = 0;
                }
                s_hop_win_max_pub = s_hop_win_max_us;
                s_hop_over10_pub = s_hop_over10;
                s_hop_win_max_us = s_hop_over10 = 0;
                s_idle_pub = (uint32_t)((uint64_t)s_idle_cyc * 1000u / ((uint64_t)mhz * (uint64_t)(now - s_win_start)));
                s_idle_cyc = 0;
                {
                    const uint32_t b1 = midi_busy_cycles();
                    if (s_busy1_started) {     /* (the first window only sets the starting count) */
                        s_busy1_pub = (uint32_t)((uint64_t)(b1 - s_busy1_last) * 1000u / ((uint64_t)mhz * (uint64_t)(now - s_win_start)));
                        s_busy1_ok = 1;
                    }
                    s_busy1_last = b1;
                    s_busy1_started = 1;
                }
                s_rate_cycles = s_rate_max_cycles = 0;
                struct jrb_cycles c = jrb_take_cycles(&s_robust);
                s_conf_us = s_conf_cycles / mhz;
                s_conf_cycles = 0;
                s_rb_pub.refresh_us = c.refresh / mhz;
                s_rb_pub.refresh_max_us = c.refresh_max / mhz;
                s_rb_pub.gate_us = c.gate / mhz;
                s_rb_pub.gate_max_us = c.gate_max / mhz;
                s_rb_pub.tick_us = c.tick / mhz;
            }
            s_win_start = now;
            s_win_busy = 0;
            /* the second's input peak in dBFS (full scale 32768; silence and anything below -99 dB: -99) and activation */
            s_peak_db = s_win_peak > 0 ? (int16_t)fmax(-99.0, floor(20.0 * log10((double)s_win_peak / 32768.0) + 0.5)) : -99;
            s_act_pct = (uint8_t)fmin(100.0, fmax(0.0, floor((double)s_win_act * 100.0 + 0.5)));
            s_win_peak = 0;
            s_win_act = 0.0f;
            report('t', (double)s_samples / (double)UAC_RATE);
        }
        if (!s_beat_seen && !nudge && now - last_step < STEP_EVERY_US)
            continue;
        s_beat_seen = 0;
        last_step = now;
        app_cfg_get(&cfg);
        apply_autobar(&cfg);
        s_rate_cfg = cfg.rate;
        if (s_song && cfg.newsong != s_song_on)
            js_reset(s_song);           /* (switched on: the song is learnt from now) */
        s_song_on = cfg.newsong;
        {
            uint8_t want = (uint8_t)(cfg.follower && s_robust_store && s_weak_store);
            uint8_t want_skip = want && cfg.skip <= JSK_PAT ? cfg.skip : 0;
            if (want != s_follower || want_skip != s_skip_mode) {   /* M2m A/B: a new follower or SKIP setting starts on a clean tracker */
                s_follower = want;
                s_skip_mode = want_skip;
                restart_tracker();
            }
            s_graded = (uint8_t)(cfg.graded && s_robust_store && s_weak_store);   /* both followers: the graded value is shown */
        }
        {
            uint32_t c0 = esp_cpu_get_cycle_count();
            follow_conf(&cfg);
            s_conf_cycles += esp_cpu_get_cycle_count() - c0;
        }
        if (cfg.burn_us != s_burn_us) {
            s_burn_us = cfg.burn_us;
            s_burn_until_us = esp_timer_get_time() + (int64_t)BURN_MAX_S * 1000000;
        }
        uint32_t step0 = esp_cpu_get_cycle_count();
        {
            struct step_ctx c;
            make_input(&cfg, &c.in, &mode, &bpm);
            c.bpm = c.in.running ? bpm : 0.0;
            make_bar(&cfg, &c.bar);
            bar_rules(&c);
            c.quantum = cfg.quantum;
            c.nudge = nudge;
            c.nudge_rc = !cfg.beat || !s_audio_live ? BEAT_NUDGE_LINK : mode == JB_STOPPED ? BEAT_NUDGE_NO_BEAT : BEAT_NUDGE_OK;
            if (c.nudge_rc != BEAT_NUDGE_OK)
                c.nudge = 0;            /* refused before it reaches the session */
            if (cfg.beat)
                bridge_with_peer(step_link, &c);
            if (nudge)
                finish_nudge(nudge, c.nudge_rc);
        }
        publish(mode, bpm * jfo_factor(&s_fold), &cfg, cpu_x10);
        s_prof[P_STEP] += esp_cpu_get_cycle_count() - step0;
        __atomic_store_n(&s_bar_done, bar_req, __ATOMIC_RELEASE);   /* the placement asked before this step is made */
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
    __atomic_add_fetch(&s_bar_req, 1u, __ATOMIC_ACQ_REL);
    return exp;
}

void beat_fold_resync(void)
{
    __atomic_store_n(&s_fold_req, (uint8_t)FOLD_REQ_RESET, __ATOMIC_RELEASE);
    __atomic_add_fetch(&s_bar_req, 1u, __ATOMIC_ACQ_REL);   /* (after the request: a step that sees it takes the request) */
}

int beat_bar_settling(void)
{
    return __atomic_load_n(&s_bar_req, __ATOMIC_ACQUIRE) != __atomic_load_n(&s_bar_done, __ATOMIC_ACQUIRE);
}

#define NUDGE_WAIT_MS 200              /* the beat task takes it within a pass (5 ms) unless starved */

int beat_nudge(int halves, int8_t *total, char *view, size_t cap)
{
    uint32_t asked;
    int rc = BEAT_NUDGE_BUSY;
    if (!halves || halves > JBL_NUDGE_MAX || halves < -JBL_NUDGE_MAX)
        return BEAT_NUDGE_LIMIT;
    portENTER_CRITICAL(&s_mux);
    asked = s_nudge.asked;
    if (asked == s_nudge.done) {
        s_nudge.halves = (int8_t)halves;
        s_nudge.asked = ++asked;
        rc = BEAT_NUDGE_OK;
    }
    portEXIT_CRITICAL(&s_mux);
    if (rc != BEAT_NUDGE_OK)
        return rc;                     /* the last one not taken yet */
    for (int i = 0; i < NUDGE_WAIT_MS / POLL_MS; i++) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        portENTER_CRITICAL(&s_mux);
        if (s_nudge.done == asked) {
            rc = s_nudge.rc;
            *total = s_nudge.total;
            if (view && cap)
                strlcpy(view, s_nudge.view, cap);
            portEXIT_CRITICAL(&s_mux);
            return rc;
        }
        portEXIT_CRITICAL(&s_mux);
    }
    return BEAT_NUDGE_BUSY;
}

void beat_state(uint8_t *audio, uint8_t *locked)
{
    portENTER_CRITICAL(&s_mux);
    *audio = (uint8_t)(s_pub.on && s_pub.audio_live);
    *locked = (uint8_t)(s_pub.mode != JB_STOPPED && s_pub.bpm_x100 > 0);
    portEXIT_CRITICAL(&s_mux);
}

void beat_gate(uint8_t *gated, uint8_t *silent, uint8_t *conf_open)
{
    portENTER_CRITICAL(&s_mux);
    *gated = (uint8_t)(s_pub.on && s_pub.audio_live && s_pub.gated);
    *silent = (uint8_t)(s_pub.on && s_pub.audio_live && s_pub.silent);
    *conf_open = s_pub.conf_open;
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
    esp_register_freertos_idle_hook_for_cpu(idle_hook, 0);
    s_b.clock = cycles_now;
    s_jbl.held = 1;                    /* power-on is the user's Stop (core/jank_hold.h jho_init): a session already playing
                                        * (Live, another stick) is not stopped for it, nor started by the tracker */
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
    /* the spectral rate's data: PSRAM only (internal RAM is short); none: the follower runs on the beats alone */
    s_rate_store = heap_caps_malloc(sizeof *s_rate_store, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    jb_rate_init(&s_rate, s_rate_store, JR_STEP, JR_EVERY);
    if (s_rate_store) {
        jb_beat_set_tap(&s_b, rate_tap, NULL);
        ESP_LOGI(TAG, "RATE: %u bytes + %u bytes in PSRAM; frames 1 in %u, an estimate every %u blocks", (unsigned)sizeof s_rate,
                 (unsigned)sizeof *s_rate_store, (unsigned)JR_STEP, (unsigned)JR_EVERY);
    } else
        ESP_LOGW(TAG, "RATE: no PSRAM for its ring, off");
    /* the weak-signal hold's ring and lists: PSRAM only; none: M2i's follow (every beat at once, the strict gate) */
    s_weak_store = heap_caps_malloc(sizeof *s_weak_store, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    jw_init(&s_weak, s_weak_store, NULL, 0, 1);
    jl_init(&s_lost);
    jre_init(&s_resume);
    s_kick = heap_caps_malloc(sizeof *s_kick, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_kick)
        jbk_init(s_kick);
    else
        ESP_LOGW(TAG, "LOST: no PSRAM for the kick band, strong beats only");
    s_song = heap_caps_malloc(sizeof *s_song, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_song) {
        js_init(s_song);
        ESP_LOGI(TAG, "NEW SONG: %u bytes in PSRAM (off until `set newsong on`)", (unsigned)sizeof *s_song);
    } else
        ESP_LOGW(TAG, "NEW SONG: no PSRAM, off");
    if (s_weak_store)
        ESP_LOGI(TAG, "WEAK: %u bytes + %u bytes in PSRAM", (unsigned)sizeof s_weak, (unsigned)sizeof *s_weak_store);
    else
        ESP_LOGW(TAG, "WEAK: no PSRAM for its ring, off");
    /* the robust follower's activation ring: PSRAM only; none: `set follower robust` keeps the tuned one */
    s_robust_store = heap_caps_malloc(sizeof *s_robust_store, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    jrb_init(&s_robust, s_robust_store);
    jsk_init(&s_skip, &s_robust);
    if (s_robust_store)
        ESP_LOGI(TAG, "ROBUST: %u bytes + %u bytes in PSRAM", (unsigned)sizeof s_robust, (unsigned)sizeof *s_robust_store);
    else
        ESP_LOGW(TAG, "ROBUST: no PSRAM for its ring, off");
    ESP_LOGW(TAG, "BEAT-FOLLOW BENCH BUILD: madmom's weights, CC BY-NC-SA 4.0, never ship");
    return xTaskCreatePinnedToCore(beat_task, "beat", BEAT_STACK, NULL, BEAT_PRIO, &s_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
