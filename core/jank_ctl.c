/* SPDX-License-Identifier: GPL-3.0-only */
/* The control line protocol (jank_core.h): get, set, status. */
#include "jank_core.h"
#include "jank_phone.h"
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
    c->autobar = 1;
    c->ble = JK_BLE_PAIR;
    c->usb = JUR_USB_DEVICE;
    c->source = JUR_SRC_AUTO;
    strcpy(c->name, "jank");
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
    return memcmp(a, b, JK_CFG_STORED) != 0 || a->usb != b->usb || a->source != b->source;
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

static const char *const BLE_NAMES[] = {"pair", "off", "auto"};   /* by enum jk_ble */

static const char *ble_name(uint32_t v) { return v <= JK_BLE_AUTO ? BLE_NAMES[v] : BLE_NAMES[JK_BLE_PAIR]; }

static void put_ip(struct writer *w, const char *key, uint32_t ip)
{
    put(w, "%s=%u.%u.%u.%u\n", key, (unsigned)(ip >> 24), (unsigned)(ip >> 16 & 255u), (unsigned)(ip >> 8 & 255u), (unsigned)(ip & 255u));
}

static void cmd_get(const struct jk_cfg *c, const struct jk_snapshot *s, struct writer *w)
{
    put(w, "group=%u\nchannel=%u\nname=%s\nfilter=%s\n", c->group, jk_channel(c->group), c->name, c->filter == JK_FILTER_ALL ? "all" : "link");
    put(w, "peer=%s\ntransport=%s\nquantum=%u\noffset=%s%d.%02d\n", on_off(c->peer), c->transport == JK_TRANSPORT_AUTO ? "auto" : "link", c->quantum, c->offset_10us < 0 ? "-" : "", abs(c->offset_10us) / 100, abs(c->offset_10us) % 100);
    put(w, "beat=%s\nbeat_latency=%d\nautobar=%s\n", on_off(c->beat), c->beat_latency_ms, on_off(c->autobar));
    put(w, "ble=%s\n", ble_name(c->ble));
    put(w, "usb=%s\nsource=%s\nline_gain=%d\n", jur_usb_name(c->usb), jur_src_name(c->source), c->line_gain_db);
    put(w, "net=%s\nssid=%s\npass=%s\n", c->net == JK_NET_WIFI ? "wifi" : "jank", c->ssid, c->pass[0] ? "set" : "none");
    put_mac(w, "mac", s->b.mac, "\n");
    put(w, "version=%s\nok\n", JK_VERSION);
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

static void cmd_status(const struct jk_snapshot *s, uint32_t now_ms, struct writer *w)
{
    const struct jk_bridge *b = &s->b;
    put(w, "usb=%u\nuptime=%lu\ngroup=%u\nchannel=%u\n", s->usb_up, (unsigned long)s->uptime_s, b->group, b->channel);
    put(w, "peers=%lu\nrssi=%d\n", (unsigned long)jk_peers_heard(b, now_ms), jk_best_rssi(b, now_ms));
    put(w, "wifi=%u\n", s->wifi_up);
    put_ip(w, "wifi_ip", s->wifi_ip);
    put(w, "wifi_rssi=%d\nwifi_channel=%u\n", s->wifi_rssi, s->wifi_channel);
    put_bridge_stats(&b->st, w);
    put(w, "tx_fail=%lu\nqueue_full=%lu\nscan_lost=%lu\n", (unsigned long)s->tx_fail, (unsigned long)s->queue_full, (unsigned long)s->scan_lost);
    put(w, "lat_air_us=%lu\nlat_air_max_us=%lu\nlat_usb_us=%lu\nlat_usb_max_us=%lu\n", (unsigned long)s->lat_air_us, (unsigned long)s->lat_air_max_us, (unsigned long)s->lat_usb_us, (unsigned long)s->lat_usb_max_us);
    put_link(s, w);
    put_bar(&s->beat, w);
    if (s->ble_built) {
        static const char *const st[] = {"off", "window", "connected"};
        put(w, "ble_state=%s\nble_left=%lu\n", s->ble_state <= 2u ? st[s->ble_state] : "off", (unsigned long)s->ble_left_s);
    }
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
    else if (!strcmp(key, "quantum") && !parse_int(val, 1, JK_QUANTUM_MAX, &v))
        c->quantum = (uint8_t)v;
    else if (!strcmp(key, "offset") && !parse_hundredths(val, &v) && v >= -JK_OFFSET_MAX_10US && v <= JK_OFFSET_MAX_10US)
        c->offset_10us = (int16_t)v;
    else if (!strcmp(key, "beat_latency") && !parse_int(val, JK_BEAT_LATENCY_MIN_MS, JK_BEAT_LATENCY_MAX_MS, &v))
        c->beat_latency_ms = (int16_t)v;
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
    if (!strcmp(key, "autobar")) {         /* on, even when on already: picked again (MANUAL ends) */
        if (parse_on_off(val, &c->autobar))
            return -1;
        if (c->autobar)
            c->autobar_picks++;
        return 0;
    }
    if (!strcmp(key, "ble")) {
        for (uint8_t i = 0; i <= JK_BLE_AUTO; i++)
            if (!strcmp(val, BLE_NAMES[i])) {
                c->ble = i;
                return 0;
            }
        return -1;
    }
    if (!strcmp(key, "usb"))
        return jur_usb_parse(val, &c->usb);
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

/* the beat tracker's numbers (the bench build) */
static void cmd_beat(const struct jk_beat *t, struct writer *w)
{
    uint32_t bpm = t->bpm_x100 > 0 ? (uint32_t)t->bpm_x100 : 0u;
    put(w, "beat=%s\nbeat_weights=madmom BENCH ONLY, CC BY-NC-SA 4.0, never ship\naudio=%u\n", on_off(t->on), t->audio);
    put(w, "mode=%s\nbpm=%lu.%02lu\n", MODE_NAME[t->mode & 3u], (unsigned long)(bpm / 100u), (unsigned long)(bpm % 100u));
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
        cmd_status(snap, now_ms, &w);
    else if (!strcmp(s, "beat"))
        cmd_beat(&snap->beat, &w);
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
    else if (!strcmp(s, "home")) {
        char h[64];
        if (jph_home_line(cfg, snap, now_ms, h, sizeof h))
            put(&w, "home=%s\nok\n", h);
        else
            put(&w, "err home line too long\n");
    } else if (!strcmp(s, "ble pair") || !strcmp(s, "ble close"))
        return s[4] == 'p' ? JK_CTL_BLE_PAIR : JK_CTL_BLE_CLOSE;
    else if (!strcmp(s, "help"))
        put(&w, "get\nstatus\nhome\nbeat\nscan\nset group|name|filter|peer|transport|quantum|offset|beat|beat_latency|autobar|ble|usb|source|line_gain|net|ssid|pass <value>\ntempo <bpm>\nplay\nstop\nble pair|close\nreboot\nbootloader\nok\n");
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
