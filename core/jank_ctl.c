/* SPDX-License-Identifier: GPL-3.0-only */
/* The control line protocol (jank_core.h): get, set, status. */
#include "jank_core.h"
#include "jank_home.h"
#include "jank_conf.h"
#include "jank_fold.h"
#include "jank_hold.h"
#include "jank_usbrole.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void jk_cfg_defaults(struct jk_cfg *c)
{
    memset(c, 0, sizeof *c);
    c->group = 1;
    c->filter = JK_FILTER_LINK;
    c->peer = 1;
    c->transport = JK_TRANSPORT_LINK;
    c->quantum = 4;
    c->beat = 1;
    c->beat_latency_ms = JK_BEAT_LATENCY_DEFAULT_MS;
    c->autobar = JK_AUTOBAR_START;
    c->usb = JUR_USB_DEVICE;
    c->usb_set = 0;
    c->source = JUR_SRC_AUTO;
    c->usbmidi = 1;                    /* on: the clock went over USB-MIDI before there was a setting (see jk_cfg) */
    c->lowconf = JCF_STOP;             /* the emulator's default: STOP / START at 80 % (player.py DEFAULT_MIN_CONFIDENCE) */
    c->threshold = JCF_THRESHOLD_DEFAULT;
    c->pause_db = JK_PAUSE_DB_DEFAULT;    /* (the user, 2026-11-19: "40 sounds about right") */
    c->rate = 1;                       /* the spectral rate on (M2i) */
    c->graded = 1;                     /* the graded confidence is the one shown (core/beat/jb_graded.h), on both followers */
    jla_init(&c->lat);                 /* (lat_key: the computer, which beat_latency_ms above is) */
    strcpy(c->name, "Jank");
}

int jk_cfg_set_name(struct jk_cfg *c, const char *name)
{
    size_t n = strlen(name);
    if (!n || n > JK_NAME_MAX)
        return -1;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)name[i] < 0x20u || (unsigned char)name[i] > 0x7Eu)
            return -1;
    memcpy(c->name, name, n + 1u);
    return 0;
}

int jk_cfg_stored_differs(const struct jk_cfg *a, const struct jk_cfg *b)
{
    return memcmp(a, b, JK_CFG_STORED) != 0 || a->usb != b->usb || a->usb_set != b->usb_set || a->source != b->source || a->autobar != b->autobar
           || a->usbmidi != b->usbmidi || a->lowconf != b->lowconf || a->threshold != b->threshold || a->rate != b->rate || a->pause_db != b->pause_db
           || !jla_same(&a->lat, &b->lat);
}

int jk_lat_select(struct jk_cfg *c, const struct jla_key *k)
{
    int16_t ms = jla_get(&c->lat, k);
    if (jla_same_key(&c->lat_key, k) && c->beat_latency_ms == ms)
        return 0;
    c->lat_key = *k;
    c->beat_latency_ms = ms;
    return 1;
}

void jk_lat_set(struct jk_cfg *c, int ms)
{
    jla_set(&c->lat, &c->lat_key, ms);
    c->beat_latency_ms = jla_get(&c->lat, &c->lat_key);
}

void jk_lat_trim(struct jk_cfg *c, int ms)
{
    jla_set_trim(&c->lat, &c->lat_key, ms);
    c->beat_latency_ms = jla_get(&c->lat, &c->lat_key);
}

enum jk_save jk_save_on_change(uint8_t *pending, int changed, int playing, int restart_follows)
{
    if (!changed && !*pending)
        return JK_SAVE_NONE;
    if (playing && !restart_follows) {
        *pending = 1;
        return JK_SAVE_LATER;
    }
    *pending = 0;
    return JK_SAVE_NOW;
}

int jk_save_due(uint8_t *pending, int playing)
{
    if (!*pending || playing)
        return 0;
    *pending = 0;
    return 1;
}

struct writer {
    char *p;
    size_t cap, n;
};

static void put(struct writer *w, const char *fmt, ...)
{
    va_list ap;
    int r;
    if (w->n + 1u >= w->cap)
        return;
    va_start(ap, fmt);
    r = vsnprintf(w->p + w->n, w->cap - w->n, fmt, ap);
    va_end(ap);
    if (r > 0)
        w->n = (size_t)r < w->cap - w->n ? w->n + (size_t)r : w->cap - 1u;
}

static void put_mac(struct writer *w, const char *key, const uint8_t *m, const char *tail)
{
    put(w, "%s=%02x:%02x:%02x:%02x:%02x:%02x%s", key, m[0], m[1], m[2], m[3], m[4], m[5], tail);
}

static const char *on_off(uint32_t v) { return v ? "on" : "off"; }

/* beat_latency= (the value used: the auto base + the AUDIO SHIFT), lat_auto= and lat_trim= (the two layers, M2h) */
static void put_lat(const struct jk_cfg *c, struct writer *w)
{
    put(w, "beat_latency=%d\nlat_auto=%d\nlat_trim=%d\n", c->beat_latency_ms, jla_base(&c->lat, &c->lat_key), jla_trim(&c->lat, &c->lat_key));
}

static const char *const SKIP_NAME[] = {"off", "act", "pat"};      /* by enum jsk_mode (core/beat/jb_skip.h) */

static const char *const AUTOBAR_NAMES[] = {"off", "start", "always"};   /* by enum jk_autobar */
static const char *autobar_name(uint32_t v) { return v <= JK_AUTOBAR_ALWAYS ? AUTOBAR_NAMES[v] : AUTOBAR_NAMES[JK_AUTOBAR_START]; }
static int parse_name3(const char *s, const char *const names[3], uint8_t *out)
{
    for (uint8_t i = 0; i < 3; i++)
        if (!strcmp(s, names[i])) {
            *out = i;
            return 0;
        }
    return -1;
}
static const char *lowconf_name(uint32_t v) { return v == JCF_RUN ? "run" : "stop"; }   /* KEEP RUNNING, STOP / START */

static void put_ip(struct writer *w, const char *key, uint32_t ip)
{
    put(w, "%s=%u.%u.%u.%u\n", key, (unsigned)(ip >> 24), (unsigned)(ip >> 16 & 255u), (unsigned)(ip >> 8 & 255u), (unsigned)(ip & 255u));
}

static const char *const SLEEP_NAME[4] = {"never", "30s", "1min", "5min"};   /* the menu's SCREEN OFF (ui/oled.js SLEEP_NAMES) */

static void cmd_get(const struct jk_cfg *c, const struct jk_snapshot *s, struct writer *w)
{
    put(w, "group=%u\nchannel=%u\nname=%s\nfilter=%s\n", c->group, jk_channel(c->group), c->name, c->filter == JK_FILTER_ALL ? "all" : "link");
    put(w, "peer=%s\ntransport=%s\nquantum=%u\noffset=%s%d.%02d\n", on_off(c->peer), c->transport == JK_TRANSPORT_AUTO ? "auto" : "link", c->quantum, c->offset_10us < 0 ? "-" : "", abs(c->offset_10us) / 100, abs(c->offset_10us) % 100);
    char src[16];
    put(w, "lead=%s\nsleep=%s\nbright=%u\n", on_off(s->lead), SLEEP_NAME[s->sleep & 3u], (unsigned)s->bright);
    put(w, "beat=%s\n", on_off(c->beat));
    put_lat(c, w);
    put(w, "beat_latency_src=%s\nautobar=%s\n", jla_name(&c->lat_key, src), autobar_name(c->autobar));
    put(w, "usbmidi=%s\n", on_off(c->usbmidi));
    put(w, "lowconf=%s\nthreshold=%u\npause_db=%d\nrate=%s\nfollower=%s\nskip=%s\ngraded=%s\nnewsong=%s\n", lowconf_name(c->lowconf), c->threshold,
        c->pause_db, on_off(c->rate), c->follower ? "robust" : "tuned", SKIP_NAME[c->skip < 3u ? c->skip : 0u], on_off(c->graded), on_off(c->newsong));
    put(w, "usb=%s\nsource=%s\nline_gain=%d\n", jur_usb_name(c->usb), jur_src_name(c->source), c->line_gain_db);
    put(w, "net=%s\nssid=%s\npass=%s\n", c->net == JK_NET_WIFI ? "wifi" : "jank", c->ssid, c->pass[0] ? "set" : "none");
    put_mac(w, "mac", s->b.mac, "\n");
    put(w, "board=%s\nversion=%s\nok\n", s->board, JK_VERSION);
}

static void put_bridge_stats(const struct jk_stats *t, struct writer *w)
{
    put(w, "usb_in=%lu\nair_out=%lu\nair_in=%lu\nusb_out=%lu\n", (unsigned long)t->usb_in, (unsigned long)t->air_out, (unsigned long)t->air_in, (unsigned long)t->usb_out);
    put(w, "drop_filter=%lu\ndrop_big=%lu\ndrop_bad=%lu\ndrop_dup=%lu\n", (unsigned long)t->drop_filter, (unsigned long)t->drop_big, (unsigned long)t->drop_bad, (unsigned long)t->drop_dup);
    put(w, "drop_own=%lu\ndrop_other=%lu\ndrop_foreign=%lu\nmac_adopted=%lu\n", (unsigned long)t->drop_own, (unsigned long)t->drop_other, (unsigned long)t->drop_foreign, (unsigned long)t->mac_adopted);
}

/* session micro-beats as beats with three decimals, truncated: "beat=-1.500" */
static void put_beats(struct writer *w, const char *key, int64_t ub)
{
    uint64_t mag = ub < 0 ? (uint64_t)-ub : (uint64_t)ub;
    put(w, "%s=%s%llu.%03llu\n", key, ub < 0 ? "-" : "", (unsigned long long)(mag / 1000000u), (unsigned long long)(mag / 1000u % 1000u));
}

static void put_link(const struct jk_snapshot *s, struct writer *w)
{
    uint32_t ip = s->link_ip;
    uint32_t bpm_x100 = s->mpb > 0 ? (uint32_t)((6000000000ull + (uint64_t)s->mpb / 2u) / (uint32_t)s->mpb) : 0u;
    put(w, "link=%u\nlink_ip=%u.%u.%u.%u\nlink_peers=%lu\n", s->link_on, (unsigned)(ip >> 24), (unsigned)(ip >> 16 & 255u), (unsigned)(ip >> 8 & 255u), (unsigned)(ip & 255u), (unsigned long)s->link_peers);
    put(w, "bpm=%lu.%02lu\n", (unsigned long)(bpm_x100 / 100u), (unsigned long)(bpm_x100 % 100u));
    put_beats(w, "beat", s->beat_ub);
    put(w, "playing=%u\nsettled=%u\narmed=%u\n", s->playing, s->settled, s->armed);
    if (s->armed)
        put_beats(w, "start", s->start_ub);       /* the bar line the transport starts on */
    put(w, "midi_ticks=%lu\nmidi_resync=%lu\n", (unsigned long)s->midi_ticks, (unsigned long)s->midi_resync);
}

/* LOW CONFIDENCE: the grid-fit confidence (conf), the network's activation (act), the gate and the silence, the settings */
static void put_conf(const struct jk_cfg *c, const struct jk_beat *t, struct writer *w)
{
    put(w, "conf=%u\nconf_hold=%u\ncoast=%u\nact=%u\ngated=%u\nsilent=%u\nthreshold=%u\nlowconf=%s\n", t->conf_pct, t->conf_hold, t->coast, t->act_pct,
        t->gated, t->silent, c->threshold, lowconf_name(c->lowconf));
}

static const char *const FOLLOW_NAME[] = {"stopped", "lock", "glide", "?"};

/* the advanced beat follow (M2i): the follower's state (lock: ModeBpm's grid, glide: the Kalman follower), its drift and
 * the last rate ratio it took (status: these; its 1920 bytes are nearly full). full (beat): also the spectral rate's
 * setting, whether it runs, its steps and their fate, its CPU; the weak-signal hold and the fast jump (M2j) */
static void put_follow(const struct jk_cfg *c, const struct jk_beat *t, struct writer *w, int full)
{
    int32_t r = t->rate_x10000;
    uint32_t mag = r < 0 ? 0u - (uint32_t)r : (uint32_t)r;
    put(w, "follow=%s\ndrift_ppm=%ld\nrate_ratio=%s%lu.%04lu\n", FOLLOW_NAME[t->follow & 3u], (long)t->drift_ppm, r < 0 ? "-" : "",
        (unsigned long)(mag / 10000u), (unsigned long)(mag % 10000u));
    if (!full)
        return;
    put(w, "manoeuvres=%lu\nrate=%s\nrate_active=%u\n", (unsigned long)t->manoeuvres, on_off(c->rate && t->rate_on), t->rate_active);
    put(w, "rate_events=%lu\nrate_taken=%lu\nrate_refused=%lu\nrate_ignored=%lu\nrate_us=%lu\nrate_max_us=%lu\n", (unsigned long)t->rate_events,
        (unsigned long)t->rate_taken, (unsigned long)t->rate_refused, (unsigned long)t->rate_ignored, (unsigned long)t->rate_us, (unsigned long)t->rate_max_us);
    put(w, "weak=%s\nweak_holds=%lu\ncoasts=%lu\nweak_beats=%lu\nfast_jumps=%lu\n", on_off(t->weak_on), (unsigned long)t->weak_holds,
        (unsigned long)t->coasts, (unsigned long)t->weak_beats, (unsigned long)t->fast_jumps);
    put(w, "prof_us=");
    for (unsigned i = 0; i < 14u; i++)
        put(w, i ? ",%lu" : "%lu", (unsigned long)t->prof_us[i]);
    put(w, "\nprof_max_us=");
    for (unsigned i = 0; i < 14u; i++)
        put(w, i ? ",%lu" : "%lu", (unsigned long)t->prof_max_us[i]);
    put(w, "\nhop_win_max_us=%lu\nhop_over10=%lu\nuptime_s=%lu\ngap_log=", (unsigned long)t->hop_win_max_us, (unsigned long)t->hop_over10, (unsigned long)t->uptime_s);
    for (unsigned i = 0; i < 6u; i++)
        if (t->gap_len_ms[i])
            put(w, "%lu:%lu ", (unsigned long)t->gap_at_ms[i], (unsigned long)t->gap_len_ms[i]);
    put(w, "\ncore0_idle=%lu.%lu\nburn_us=%u\n", (unsigned long)(t->core0_idle_x10 / 10u), (unsigned long)(t->core0_idle_x10 % 10u), t->burn_us);
    put(w, "follower=%s\nrobust=%s\nconf_us=%lu\n", c->follower ? "robust" : "tuned", t->follower ? "on" : "off", (unsigned long)t->conf_us);
    put(w, "skip=%s\ngraded=%s\nsk_syncs=%u\nsk_act=%u\nsk_gated=%u\n", SKIP_NAME[t->skip < 3u ? t->skip : 0u], on_off(t->graded),
        t->skip_resyncs, t->skip_act_resyncs, t->skip_gated);
    put(w, "robust_refresh_us=%lu\nrobust_refresh_max_us=%lu\nrobust_gate_us=%lu\nrobust_gate_max_us=%lu\nrobust_tick_us=%lu\n",
        (unsigned long)t->robust_refresh_us, (unsigned long)t->robust_refresh_max_us, (unsigned long)t->robust_gate_us,
        (unsigned long)t->robust_gate_max_us, (unsigned long)t->robust_tick_us);
    put(w, "robust_asked=%lu\nrobust_refused_memory=%lu\nrobust_refused_ratio=%lu\nrobust_coast_ticks=%lu\nrobust_prior_ticks=%lu\n",
        (unsigned long)t->robust_asked, (unsigned long)t->robust_refused_memory, (unsigned long)t->robust_refused_ratio,
        (unsigned long)t->robust_coast_ticks, (unsigned long)t->robust_prior_ticks);
}

/* the tracker's source: open, sound (AUDIO mode), AUDIO mode's Stop, the outputs; the input meter and the confidence */
static void put_audio(const struct jk_cfg *c, const struct jk_snapshot *s, struct writer *w)
{
    const struct jk_beat *t = &s->beat;
    put(w, "audio=%u\naudio_live=%u\nhold=%u\noutputs=%s\nfold=%s\nfold_manual=%u\n", t->audio, t->audio_live, s->hold, jho_icon_name(s->outputs),
        jfo_name(t->fold_exp), t->fold_manual);
    put(w, "rms_db=%d\nrms_min_db=%d\nrms_max_db=%d\npeak_db=%d\n", t->level_db, t->level_min_db, t->level_max_db, t->peak_db);
    put_conf(c, t, w);
    put(w, "conf_gate=%u\n", t->conf_graded ? t->conf_gate_pct : t->conf_pct);   /* the gate's own value (conf is the graded one while graded=on) */
    put_follow(c, t, w, 0);
}

static const char *const BAR_NAME[] = {"off", "listening", "locked", "manual"};

/* the downbeat estimator (AUTO BAR): its state, the 1's count position (-1 none) and its lead */
static void put_bar(const struct jk_beat *t, struct writer *w)
{
    int32_t lead = t->bar_lead_x100;
    uint32_t mag = lead < 0 ? (uint32_t)-lead : (uint32_t)lead;
    put(w, "bar=%s\nbar_phi=%d\nbar_lead=%s%lu.%02lu\n", BAR_NAME[t->bar & 3u], t->bar_phi, lead < 0 ? "-" : "", (unsigned long)(mag / 100u), (unsigned long)(mag % 100u));
}

/* the USB port: what runs, what the CC chip sees, the devices hosted and the audio the tracker hears */
static void put_usb(const struct jk_usb *u, struct writer *w)
{
    put(w, "usb_role=%s\nusb_cc=%s\n", jur_usb_name(u->stack), u->has_port ? jur_cc_name(u->cc) : "nochip");
    if (u->has_port)
        put(w, "usb_cc_dir=%u\nusb_vbus_out=%u\nusb_vbus=%u\n", u->cc_dir, u->vbus_on, u->vbus);
    put(w, "line_plugged=%u\naudio_source=%s\n", u->line, jur_audio_name(u->audio));
    for (uint32_t i = 0; i < u->ndev && i < JK_USB_DEVS; i++)
        put(w, "usb_dev=%s,%s%s\n", u->dev[i].name[0] ? u->dev[i].name : "?", u->dev[i].midi ? "midi" : "", u->dev[i].audio ? (u->dev[i].midi ? "+audio" : "audio") : "");
    if (u->audio_rate)
        put(w, "usb_audio=%lu/%u/%u\nusb_audio_packets=%lu\nusb_audio_errors=%lu\n", (unsigned long)u->audio_rate, u->audio_channels, u->audio_bits, (unsigned long)u->audio_packets, (unsigned long)u->audio_errors);
    if (u->stack == JUR_STACK_HOST)
        put(w, "usb_midi_dropped=%lu\n", (unsigned long)u->midi_dropped);
}

const char *jk_half_beats(int halves, char *out)
{
    unsigned mag = (unsigned)(halves < 0 ? -halves : halves) % 100u;
    snprintf(out, 8, "%s%u%s", halves < 0 ? "-" : "", mag / 2u, mag % 2u ? ".5" : "");
    return out;
}

/* PHASE and AUDIO SHIFT: the user's phase (NUDGE BEAT's total), the latency's two layers and the source they are of; the
 * USB-MIDI clock setting */
static void put_nudge(const struct jk_cfg *c, const struct jk_beat *t, struct writer *w)
{
    char b[8], src[16];
    put(w, "nudge_beats=%s\n", jk_half_beats(t->nudge_halves, b));
    put_lat(c, w);
    put(w, "beat_latency_src=%s\nusbmidi=%s\n", jla_name(&c->lat_key, src), on_off(c->usbmidi));
}

/* the peers' air in the last completed 10 s window: -1 for a percentage not known, 0 for no level */
static void put_air(const struct jk_bridge *b, uint32_t now_ms, struct writer *w)
{
    struct jk_airsum a;
    jk_win_summary(&b->win, now_ms, &a);
    put(w, "air_lost_pct=%d\nair_retry_pct=%d\nrssi_min=%d\nrssi_max=%d\n", a.lost_pct, a.retry_pct, a.rssi_min, a.rssi_max);
}

int jk_cpu_pct(uint32_t core0_idle_x10)
{
    int v = 100 - (int)((core0_idle_x10 + 5u) / 10u);
    return core0_idle_x10 ? v < 0 ? 0 : v > 100 ? 100 : v : JK_SYS_UNKNOWN;
}

int jk_cpu1_pct(uint32_t core1_busy_x10, uint8_t ok)
{
    int v = (int)((core1_busy_x10 + 5u) / 10u);
    return ok ? v > 100 ? 100 : v : JK_SYS_UNKNOWN;
}

/* `about`: the ABOUT screen's numbers, one key each, the screen's own unknowns (-1; temp_c -999; reset empty). A second reply, not
 * part of `status`: with these the longest status would pass JK_CTL_OUT_MAX (tests/test_ctl.c) */
static void cmd_about(const struct jk_snapshot *s, struct writer *w)
{
    struct jk_sys x = s->sys;
    if (!x.known)
        x = (struct jk_sys){0, JK_TEMP_UNKNOWN, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "", ""};
    put(w, "uptime_s=%d\nreset=%s\npanic=%s\ntemp_c=%d\ncpu_pct=%d\ncpu1_pct=%d\n", (int)x.uptime_s, x.reset, x.panic ? x.panic : "", (int)x.temp_c, jk_cpu_pct(s->beat.core0_idle_x10), jk_cpu1_pct(s->beat.core1_busy_x10, s->beat.core1_ok));
    put(w, "heap_kb=%d\nheap_min_kb=%d\npsram_kb=%d\nflash_kb=%d\napp_used_kb=%d\napp_part_kb=%d\n", (int)x.heap_kb, (int)x.heap_min_kb, (int)x.psram_kb, (int)x.flash_kb, (int)x.app_used_kb, (int)x.app_kb);
    put(w, "clock_late_us=%d\nui_loop_us=%d\nbeat_loop_us=%d\nok\n", (int)x.clk_late_us, (int)x.ui_loop_us, (int)x.beat_loop_us);
}

static void cmd_status(const struct jk_cfg *c, const struct jk_snapshot *s, uint32_t now_ms, struct writer *w)
{
    const struct jk_bridge *b = &s->b;
    put(w, "usb=%u\nuptime=%lu\ngroup=%u\nchannel=%u\n", s->usb_up, (unsigned long)s->uptime_s, b->group, b->channel);
    put(w, "peers=%lu\nrssi=%d\n", (unsigned long)jk_peers_heard(b, now_ms), jk_best_rssi(b, now_ms));
    put_air(b, now_ms, w);
    put(w, "wifi=%u\n", s->wifi_up);
    put_ip(w, "wifi_ip", s->wifi_ip);
    put(w, "wifi_rssi=%d\nwifi_channel=%u\n", s->wifi_rssi, s->wifi_channel);
    put_bridge_stats(&b->st, w);
    put(w, "tx_fail=%lu\nqueue_full=%lu\nscan_lost=%lu\n", (unsigned long)s->tx_fail, (unsigned long)s->queue_full, (unsigned long)s->scan_lost);
    put(w, "lat_air_us=%lu\nlat_air_max_us=%lu\nlat_usb_us=%lu\nlat_usb_max_us=%lu\n", (unsigned long)s->lat_air_us, (unsigned long)s->lat_air_max_us, (unsigned long)s->lat_usb_us, (unsigned long)s->lat_usb_max_us);
    put_link(s, w);
    put_audio(c, s, w);
    put_bar(&s->beat, w);
    put_nudge(c, &s->beat, w);
    put_usb(&s->usb, w);
    for (uint32_t i = 0; i < JK_PEERS; i++) {
        const struct jk_peer *p = &b->peer[i];
        if (!p->used || now_ms - p->t_heard >= JK_HEARD_MS)
            continue;
        put_mac(w, "peer", p->mac, "");
        put(w, ",%d,%lu,%lu\n", p->rssi, (unsigned long)(now_ms - p->t_heard), (unsigned long)p->frames);
    }
    put(w, "ok\n");
}

/* a whole-string decimal in [lo, hi] */
static int parse_int(const char *s, long lo, long hi, long *out)
{
    char *end;
    long v;
    if (!*s)
        return -1;
    v = strtol(s, &end, 10);
    if (*end || v < lo || v > hi)
        return -1;
    *out = v;
    return 0;
}

/* "-1.27", "12", "0.5" -> hundredths (-127, 1200, 50): up to three digits, up to two decimals; -1 otherwise */
static int parse_hundredths(const char *s, long *out)
{
    long whole = 0, frac = 0;
    int neg = *s == '-', digits = 0, fd = 0;
    s += neg;
    for (; *s >= '0' && *s <= '9' && digits < 4; s++, digits++)
        whole = whole * 10 + (*s - '0');
    if (!digits || digits > 3)
        return -1;
    if (*s == '.') {
        for (s++; *s >= '0' && *s <= '9' && fd < 3; s++, fd++)
            frac = frac * 10 + (*s - '0');
        if (!fd || fd > 2)
            return -1;
        if (fd == 1)
            frac *= 10;
    }
    if (*s)
        return -1;
    *out = (neg ? -1 : 1) * (whole * 100 + frac);
    return 0;
}

static int parse_on_off(const char *s, uint8_t *out)
{
    if (!strcmp(s, "on") || !strcmp(s, "off")) {
        *out = s[1] == 'n';
        return 0;
    }
    return -1;
}

static int parse_pair(const char *s, const char *zero, const char *one, uint8_t *out)
{
    if (!strcmp(s, zero) || !strcmp(s, one)) {
        *out = !strcmp(s, one);
        return 0;
    }
    return -1;
}

static int set_number(struct jk_cfg *c, const char *key, const char *val)
{
    long v;
    if (!strcmp(key, "group") && !parse_int(val, JK_GROUP_MIN, JK_GROUP_MAX, &v))
        c->group = (uint8_t)v;
    else if (!strcmp(key, "burn") && !parse_int(val, 0, 9000, &v))
        c->burn_us = (uint16_t)v;          /* BENCH: extra tracker work a frame, us */
    else if (!strcmp(key, "quantum") && !parse_int(val, 1, JK_QUANTUM_MAX, &v))
        c->quantum = (uint8_t)v;
    else if (!strcmp(key, "offset") && !parse_hundredths(val, &v) && v >= -JK_OFFSET_MAX_10US && v <= JK_OFFSET_MAX_10US)
        c->offset_10us = (int16_t)v;
    else if (!strcmp(key, "threshold") && !parse_int(val, 0, 100, &v))
        c->threshold = (uint8_t)v;         /* LOW CONFIDENCE's THRESHOLD, percent (0: never gated) */
    else if (!strcmp(key, "pause_db") && !parse_int(val, JK_PAUSE_DB_MIN, JK_PAUSE_DB_MAX, &v))
        c->pause_db = (int8_t)v;           /* PAUSE LEVEL, dBFS (core/beat/jb_lost.h) */
    else if (!strcmp(key, "beat_latency") && !parse_int(val, JK_BEAT_LATENCY_MIN_MS, JK_BEAT_LATENCY_MAX_MS, &v))
        jk_lat_set(c, (int)v);             /* the auto base of the source heard now (the player's Auto latency) */
    else if (!strcmp(key, "lat_trim") && !parse_int(val, JK_BEAT_LATENCY_MIN_MS - JK_BEAT_LATENCY_MAX_MS, JK_BEAT_LATENCY_MAX_MS - JK_BEAT_LATENCY_MIN_MS, &v))
        jk_lat_trim(c, (int)v);            /* its AUDIO SHIFT (Calibrate's) */
    else if (!strcmp(key, "beat_latency_usb") && !parse_int(val, JK_BEAT_LATENCY_MIN_MS, JK_BEAT_LATENCY_MAX_MS, &v)) {
        static const struct jla_key computer = {JLA_COMPUTER, 0, 0};   /* the computer's, whatever is heard now */
        jla_set(&c->lat, &computer, (int)v);
        c->beat_latency_ms = jla_get(&c->lat, &c->lat_key);
    }
    else if (!strcmp(key, "line_gain") && !parse_int(val, JK_LINE_GAIN_MIN_DB, JK_LINE_GAIN_MAX_DB, &v))
        c->line_gain_db = (int8_t)v;
    else
        return -1;
    return 0;
}

static int printable(const char *s)
{
    for (; *s; s++)
        if ((unsigned char)*s < 0x20u || (unsigned char)*s > 0x7Eu)
            return 0;
    return 1;
}

static int set_text(char *to, const char *val, size_t min, size_t max)
{
    size_t n = strlen(val);
    if (n < min || n > max || !printable(val))
        return -1;
    memcpy(to, val, n + 1u);
    return 0;
}

static int set_value(struct jk_cfg *c, const char *key, const char *val)
{
    if (!strcmp(key, "name"))
        return jk_cfg_set_name(c, val);
    if (!strcmp(key, "ssid"))
        return set_text(c->ssid, val, 1, JK_SSID_MAX);
    if (!strcmp(key, "pass"))
        return set_text(c->pass, val, JK_PASS_MIN, JK_PASS_MAX);
    if (!strcmp(key, "net"))
        return parse_pair(val, "jank", "wifi", &c->net);
    if (!strcmp(key, "filter"))
        return parse_pair(val, "link", "all", &c->filter);
    if (!strcmp(key, "transport"))
        return parse_pair(val, "link", "auto", &c->transport);
    if (!strcmp(key, "peer"))
        return parse_on_off(val, &c->peer);
    if (!strcmp(key, "beat"))
        return parse_on_off(val, &c->beat);
    if (!strcmp(key, "usbmidi"))
        return parse_on_off(val, &c->usbmidi);
    if (!strcmp(key, "follower"))          /* M2m A/B: tuned (M2j) or robust (all-d-f60); not stored */
        return parse_pair(val, "tuned", "robust", &c->follower);
    if (!strcmp(key, "skip")) {            /* SKIP on the robust follower: off, ACT or PAT (core/beat/jb_skip.h); not stored */
        for (uint8_t i = 0; i < 3u; i++)
            if (!strcmp(val, SKIP_NAME[i])) {
                c->skip = i;
                return 0;
            }
        return -1;
    }
    if (!strcmp(key, "graded"))            /* the shown confidence graded (core/beat/jb_graded.h), both followers: display only; not stored, on at every start */
        return parse_on_off(val, &c->graded);
    if (!strcmp(key, "newsong"))           /* LOST's new-song check (core/beat/jb_song.h), the tuned follower: not stored, off at every start */
        return parse_on_off(val, &c->newsong);
    if (!strcmp(key, "rate"))              /* the advanced beat follow's spectral rate (core/beat/jb_rate.h) */
        return parse_on_off(val, &c->rate);
    if (!strcmp(key, "lowconf"))           /* LOW CONFIDENCE: KEEP RUNNING or STOP / START (jank_conf.h) */
        return parse_pair(val, "run", "stop", &c->lowconf);
    if (!strcmp(key, "autobar")) {         /* on (= start), start, always, even when so already: picked again (MANUAL ends) */
        uint8_t v;
        if (!strcmp(val, "on"))
            v = JK_AUTOBAR_START;
        else if (parse_name3(val, AUTOBAR_NAMES, &v))
            return -1;
        c->autobar = v;
        if (v != JK_AUTOBAR_OFF)
            c->autobar_picks++;
        return 0;
    }
    if (!strcmp(key, "usb")) {
        int r = jur_usb_parse(val, &c->usb);
        if (!r)
            c->usb_set = 1;            /* said by the user: from now on the setting is taken as it is (jur_effective_setting) */
        return r;
    }
    if (!strcmp(key, "source"))
        return jur_src_parse(val, &c->source);
    return set_number(c, key, val);
}

static enum jk_ctl_action cmd_set(char *args, struct jk_cfg *cfg, struct writer *w)
{
    struct jk_cfg next = *cfg;
    char *val = strchr(args, ' ');
    if (val)
        *val++ = 0;
    if (!val && !strcmp(args, "pass"))     /* an open network */
        next.pass[0] = 0;
    else if (!val || set_value(&next, args, val)) {
        put(w, "err bad setting\n");
        return JK_CTL_NONE;
    }
    *cfg = next;
    put(w, "ok\n");
    return JK_CTL_CFG_CHANGED;
}

static const char *const MODE_NAME[] = {"stopped", "steady", "speeding_up", "slowing_down"};

/* the beat tracker's numbers; beat_weights: none (no weights in the beatw partition: no beat follow), ok, damaged */
static void cmd_beat(const struct jk_cfg *c, const struct jk_beat *t, struct writer *w)
{
    uint32_t bpm = t->bpm_x100 > 0 ? (uint32_t)t->bpm_x100 : 0u;
    static const char *const WEIGHTS[] = {"none", "ok", "damaged"};
    put(w, "beat=%s\nbeat_weights=%s\naudio=%u\naudio_live=%u\n", on_off(t->on), WEIGHTS[t->weights < 3u ? t->weights : 0u],
        t->audio, t->audio_live);
    put(w, "rms_db=%d\nrms_min_db=%d\nrms_max_db=%d\npeak_db=%d\n", t->level_db, t->level_min_db, t->level_max_db, t->peak_db);
    put_conf(c, t, w);
    put(w, "mode=%s\nbpm=%lu.%02lu\n", MODE_NAME[t->mode & 3u], (unsigned long)(bpm / 100u), (unsigned long)(bpm % 100u));
    put_follow(c, t, w, 1);
    put(w, "heap_free=%lu\nheap_min=%lu\nheap_largest=%lu\nstack_free=%lu\n", (unsigned long)t->heap_free, (unsigned long)t->heap_min, (unsigned long)t->heap_largest, (unsigned long)t->stack_free);
    put(w, "psram_free=%lu\npsram_min=%lu\n", (unsigned long)t->psram_free, (unsigned long)t->psram_min);
    put(w, "cpu_pct=%lu.%lu\nhop_avg_us=%lu\nhop_max_us=%lu\nhop_avg_cycles=%lu\nhops=%lu\n", (unsigned long)(t->cpu_pct_x10 / 10u), (unsigned long)(t->cpu_pct_x10 % 10u), (unsigned long)t->hop_avg_us, (unsigned long)t->hop_max_us, (unsigned long)t->hop_avg_cycles, (unsigned long)t->hops);
    put(w, "packets=%lu\ngaps=%lu\noverflows=%lu\nresets=%lu\n", (unsigned long)t->packets, (unsigned long)t->gaps, (unsigned long)t->overflows, (unsigned long)t->resets);
    put(w, "tempo_sets=%lu\nphase_forces=%lu\nstarts=%lu\nstops=%lu\n", (unsigned long)t->tempo_sets, (unsigned long)t->phase_forces, (unsigned long)t->starts, (unsigned long)t->stops);
    put(w, "err_us=%ld\noffset_us=%ld\nlatency_us=%ld\n", (long)t->err_us, (long)t->offset_us, (long)t->latency_us);
    put_bar(t, w);
    put(w, "bar_moves=%lu\nok\n", (unsigned long)t->bar_moves);
}

/* the line without the spaces and CR around it, in buf */
static char *trim(const char *line, char *buf, size_t cap)
{
    size_t n = strlen(line);
    while (n && (line[n - 1u] == ' ' || line[n - 1u] == '\r' || line[n - 1u] == '\n'))
        n--;
    while (n && *line == ' ') {
        line++;
        n--;
    }
    if (n >= cap)
        return NULL;
    memcpy(buf, line, n);
    buf[n] = 0;
    return buf;
}

/* hundredths of a BPM from "120", "128.5", "99.25" -> 0; -1 not a number like that */
static int parse_bpm_x100(const char *s, uint32_t *out)
{
    uint32_t v = 0, frac = 0, digits = 0, fd = 0;
    for (; *s >= '0' && *s <= '9'; s++, digits++)
        if (digits < 4u)
            v = v * 10u + (uint32_t)(*s - '0');
    if (!digits || digits > 3u)
        return -1;
    if (*s == '.')
        for (s++; *s >= '0' && *s <= '9'; s++, fd++)
            if (fd < 2u)
                frac = frac * 10u + (uint32_t)(*s - '0');
    if (*s || fd > 2u)
        return -1;
    *out = v * 100u + (fd == 1u ? frac * 10u : frac);
    return 0;
}

int jk_ctl_tempo(const char *line, int32_t *mpb)
{
    char buf[32], *s = trim(line, buf, sizeof buf);
    uint32_t x100;
    if (!s || strncmp(s, "tempo ", 6) || parse_bpm_x100(s + 6, &x100) || x100 < JK_BPM_MIN_X100 || x100 > JK_BPM_MAX_X100)
        return -1;
    *mpb = (int32_t)((6000000000ull + x100 / 2u) / x100);
    return 0;
}

int jk_ctl_lead(const char *line, int *on)
{
    char buf[32], *s = trim(line, buf, sizeof buf);
    if (!s || strncmp(s, "set lead ", 9))
        return -1;
    s += 9;
    if (!strcmp(s, "on"))
        *on = 1;
    else if (!strcmp(s, "off"))
        *on = 0;
    else
        return -1;
    return 0;
}

int jk_ctl_display(const char *line, int *bright, int *v)
{
    char buf[32], *s = trim(line, buf, sizeof buf);
    if (!s)
        return -1;
    if (!strncmp(s, "set sleep ", 10)) {
        for (int i = 0; i < 4; i++)
            if (!strcmp(s + 10, SLEEP_NAME[i])) {
                *bright = 0;
                *v = i;
                return 0;
            }
        return -1;
    }
    if (!strncmp(s, "set bright ", 11)) {
        char *e = s + 11;
        long n = 0;
        if (!*e || strlen(e) > 3)
            return -1;
        for (; *e; e++) {
            if (*e < '0' || *e > '9')
                return -1;
            n = n * 10 + (*e - '0');
        }
        if (n < 10 || n > 100)
            return -1;
        *bright = 1;
        *v = (int)n;
        return 0;
    }
    return -1;
}

int jk_ctl_fold(const char *line, int *dir)
{
    char buf[32], *s = trim(line, buf, sizeof buf);
    if (!s || strncmp(s, "fold ", 5))
        return -1;
    s += 5;
    if (!strcmp(s, "x2"))
        *dir = 1;
    else if (!strcmp(s, "/2"))
        *dir = -1;
    else if (!strcmp(s, "1"))
        *dir = 0;
    else
        return -1;
    return 0;
}

int jk_ctl_nudge(const char *line, int *halves)
{
    char buf[32], *s = trim(line, buf, sizeof buf);
    long v;
    if (!s || strncmp(s, "nudge beat ", 11))
        return -1;
    s += 11;
    if (*s == '+' && s[1] != '-')
        s++;
    if (parse_hundredths(s, &v) || v % 50 || !v || v < -300 || v > 300)
        return -1;
    *halves = (int)(v / 50);
    return 0;
}

int jk_nudge_ms(struct jk_cfg *c, int ms)
{
    int16_t before = jla_trim(&c->lat, &c->lat_key);
    jk_lat_trim(c, before + ms);       /* the current source's AUDIO SHIFT; the auto base untouched */
    return jla_trim(&c->lat, &c->lat_key) != before;
}

/* "nudge ms <n>": AUDIO mode only (LINK mode: the session's timing is not the tracker's) */
static enum jk_ctl_action cmd_nudge_ms(const char *val, struct jk_cfg *cfg, const struct jk_snapshot *snap, struct writer *w)
{
    long v;
    int moved;
    if (parse_int(val, -JK_NUDGE_MS_STEP_MAX, JK_NUDGE_MS_STEP_MAX, &v) || !v) {
        put(w, "err nudge ms is -%d to %d, not 0\n", JK_NUDGE_MS_STEP_MAX, JK_NUDGE_MS_STEP_MAX);
        return JK_CTL_NONE;
    }
    if (!snap->beat.on || !snap->beat.audio_live) {
        put(w, "err link mode\n");
        return JK_CTL_NONE;
    }
    moved = jk_nudge_ms(cfg, (int)v);
    put_lat(cfg, w);
    put(w, "ok\n");
    return moved ? JK_CTL_CFG_CHANGED : JK_CTL_NONE;
}

enum jk_ctl_action jk_ctl_line(const char *line, struct jk_cfg *cfg, const struct jk_snapshot *snap, uint32_t now_ms, char *out, size_t cap)
{
    struct writer w = {out, cap, 0};
    char buf[80], *s = trim(line, buf, sizeof buf);
    out[0] = 0;
    if (!s) {
        put(&w, "err line too long\n");
        return JK_CTL_NONE;
    }
    if (!*s)
        return JK_CTL_NONE;
    if (!strcmp(s, "get"))
        cmd_get(cfg, snap, &w);
    else if (!strcmp(s, "status"))
        cmd_status(cfg, snap, now_ms, &w);
    else if (!strcmp(s, "about"))
        cmd_about(snap, &w);
    else if (!strcmp(s, "beat"))
        cmd_beat(cfg, &snap->beat, &w);
    else if (!strncmp(s, "set lead", 8) && (!s[8] || s[8] == ' ')) {
        int on;
        if (jk_ctl_lead(s, &on)) {
            put(&w, "err bad setting\n");
            return JK_CTL_NONE;
        }
        put(&w, "ok\n");
        return JK_CTL_LEAD;
    }
    else if ((!strncmp(s, "set sleep", 9) && (!s[9] || s[9] == ' ')) || (!strncmp(s, "set bright", 10) && (!s[10] || s[10] == ' '))) {
        int br, v;
        if (jk_ctl_display(s, &br, &v)) {
            put(&w, "err bad setting\n");
            return JK_CTL_NONE;
        }
        put(&w, "ok\n");
        return JK_CTL_DISPLAY;
    }
    else if (!strncmp(s, "set ", 4))
        return cmd_set(s + 4, cfg, &w);
    else if (!strcmp(s, "scan"))
        return JK_CTL_SCAN;
    else if (!strcmp(s, "play") || !strcmp(s, "stop"))
        return s[1] == 'l' ? JK_CTL_PLAY : JK_CTL_STOP;
    else if (!strncmp(s, "tempo", 5) && (!s[5] || s[5] == ' ')) {
        int32_t mpb;
        if (!jk_ctl_tempo(s, &mpb))
            return JK_CTL_TEMPO;
        put(&w, "err tempo is 20 to 999 BPM\n");
    }
    else if (!strncmp(s, "fold", 4) && (!s[4] || s[4] == ' ')) {
        int dir;
        if (!jk_ctl_fold(s, &dir))
            return JK_CTL_FOLD;
        put(&w, "err fold x2, /2 or 1\n");
    }
    else if (!strncmp(s, "nudge ms ", 9))
        return cmd_nudge_ms(s + 9, cfg, snap, &w);
    else if (!strncmp(s, "nudge", 5) && (!s[5] || s[5] == ' ')) {
        int halves;
        if (!jk_ctl_nudge(s, &halves))
            return JK_CTL_NUDGE;
        put(&w, "err nudge beat +-0.5 to 3 in halves, or nudge ms +-1 to %d\n", JK_NUDGE_MS_STEP_MAX);
    }
    else if (!strcmp(s, "home")) {
        char h[JHM_LINE_MAX + 1];
        if (jhm_home_line(cfg, snap, now_ms, h, sizeof h))
            put(&w, "home=%s\nok\n", h);
        else
            put(&w, "err home line too long\n");
    } else if (!strcmp(s, "help"))
        put(&w, "get\nstatus\nabout\nhome\nbeat\nscan\nset group|name|filter|peer|transport|lead|sleep|bright|quantum|offset|beat|beat_latency|beat_latency_usb|lat_trim|autobar|usbmidi|lowconf|threshold|pause_db|rate|follower|skip|graded|newsong|burn|usb|source|line_gain|net|ssid|pass <value>\ntempo <bpm>\nplay\nstop\nfold x2|/2|1\nnudge beat +-0.5..3\nnudge ms +-1..10\nreboot\nbootloader\nok\n");
    else if (!strcmp(s, "reboot") || !strcmp(s, "bootloader")) {
        put(&w, "ok\n");
        return s[0] == 'r' ? JK_CTL_REBOOT : JK_CTL_BOOTLOADER;
    } else
        put(&w, "err unknown command\n");
    return JK_CTL_NONE;
}

/* one line of the dump (" 27     3    Level  Used: TG0_T0_LEVEL"), in use -> "c<cpu> <int> L<level> <status>\n" */
static int intr_line(const char *l, size_t n, int cpu, struct writer *w)
{
    char st[96];
    const char *s = NULL;
    int num = -1, level = -1;
    static const char *const keep[] = {"Used:", "Shared:", "Reserved (run-time)"};
    for (size_t k = 0; k < sizeof keep / sizeof keep[0] && !s; k++) {
        const char *p = strstr(l, keep[k]);
        if (p && p < l + n)
            s = p;
    }
    if (!s || sscanf(l, "%d %d", &num, &level) < 1)
        return 0;
    snprintf(st, sizeof st, "%.*s", (int)(l + n - s), s);
    for (size_t k = strlen(st); k && st[k - 1] == ' '; k--)
        st[k - 1] = 0;
    if (level >= 0)
        put(w, "c%d %d L%d %s\n", cpu, num, level, st);
    else
        put(w, "c%d %d %s\n", cpu, num, st);
    return 1;
}

size_t jk_intr_summary(const char *dump, char *out, size_t cap)
{
    struct writer w = {out, cap, 0};
    int cpu = -1;
    if (!cap)
        return 0;
    out[0] = 0;
    while (*dump) {
        const char *e = strchr(dump, '\n');
        size_t n = e ? (size_t)(e - dump) : strlen(dump);
        if (!strncmp(dump, "CPU ", 4))
            cpu = atoi(dump + 4);
        else if (cpu >= 0)
            intr_line(dump, n, cpu, &w);
        dump += n + (e ? 1u : 0u);
    }
    put(&w, "ok\n");
    return w.n >= 3u && !strcmp(out + w.n - 3u, "ok\n") ? w.n : 0u;
}
