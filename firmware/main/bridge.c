/* SPDX-License-Identifier: GPL-3.0-only */
/* The bridge task: one owner for the core's state, frames from the radio and from USB in one queue of fixed slots.
 * A full queue drops the frame: Link's next one is worth more than an old one. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "board.h"
#include "jank_hold.h"
#include "uiscreen.h"
#include "usb_host_mode.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "jank_net.h"
#include "jank_peer.h"

#define SLOTS 20u
#define SLOT_BYTES (JK_ETH_MAX + 32u)
#define IDLE_WAIT_MS 2u               /* the peer's and the clock's polling */
#define PUBLISH_US 5000               /* a new clock map this often */
#define HOP_HOLD_US 10000000          /* after a new GROUP, keep the transport this long while looking for the session */
#define TASK_PRIO 20                  /* above the TinyUSB task, below Wi-Fi's */

enum { FROM_AIR, FROM_USB, FROM_NETWORK };

struct slot {
    int64_t at;
    uint16_t len;
    uint8_t from;
    int8_t rssi;
    uint8_t data[SLOT_BYTES];
};

static const char *TAG = "bridge";
/* the frame slots: tasks only touch them (the Wi-Fi, TinyUSB and bridge tasks, never an interrupt), and NCM and Wi-Fi copy
 * a frame out before sending it. Allocated by bridge_start: in PSRAM with BOARD_BRIDGE_SLOTS_PSRAM (the mini 3), else internal */
static struct slot *s_slot;            /* SLOTS of them, from the heap (bridge_start): PSRAM on boards short of internal RAM */
static QueueHandle_t s_free, s_work;
static SemaphoreHandle_t s_lock;
static struct jk_bridge s_b;
static struct jank_peer s_peer;
static struct jank_net s_net;
static uint8_t s_on_network;          /* cfg.net, from start-up on */
static uint8_t s_net_was_up;
static struct jk_cfg s_cfg;
static uint8_t s_bssid[6];
static uint8_t s_own[6];
static volatile uint8_t s_away;
static struct js_scan s_scan;
static uint8_t s_air[JK_AIR_MAX + 8];
static uint32_t s_tx_fail, s_queue_full, s_scan_lost;
static uint32_t s_lat_air, s_lat_air_max, s_lat_usb, s_lat_usb_max;
static uint8_t s_want;                /* what the MIDI clock outputs were told (midi_want) */
static uint8_t s_base;                /* what the transport setting wants of them, before AUDIO mode's hold */
static struct jho s_hold;             /* AUDIO mode's Stop / Start (core/jank_hold.h) */
static struct jho_in s_hin;           /* its inputs at the last step */
static uint8_t s_lead = 1;            /* LINK TRANSPORT FOLLOW+LEAD (1) or FOLLOW (0), with the transport setting LINK */
static uint8_t s_playing;             /* bridge_is_playing: written by the bridge task only (follow_transport) */
static int64_t s_hop_until;
static struct js_table s_table;
static volatile uint8_t s_table_on;

uint32_t bridge_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static struct slot *take_slot(int *index)
{
    uint8_t i;
    if (xQueueReceive(s_free, &i, 0) != pdTRUE) {
        s_queue_full++;
        return NULL;
    }
    *index = i;
    return &s_slot[i];
}

static void give_slot(int i)
{
    uint8_t v = (uint8_t)i;
    xQueueSend(s_free, &v, 0);
}

/* the Wi-Fi task */
static void on_air(const uint8_t *f, size_t len, int8_t rssi)
{
    uint32_t g = js_group_of(f, len, s_own);
    int i;
    struct slot *s;
    uint8_t v;
    if (!g)
        return;
    js_note(&s_scan, g, bridge_ms());
    if (s_table_on)
        js_table_note(&s_table, g, f + 10, rssi);
    if (s_away || len > SLOT_BYTES || !jk_air_is_group(s_bssid, f, len))
        return;
    s = take_slot(&i);
    if (!s)
        return;
    s->at = midi_now_us();
    s->len = (uint16_t)len;
    s->from = FROM_AIR;
    s->rssi = rssi;
    memcpy(s->data, f, len);
    v = (uint8_t)i;
    xQueueSend(s_work, &v, 0);
}

/* the Wi-Fi task, on a normal network */
static void on_network(const uint8_t *eth, size_t len)
{
    int i;
    struct slot *s;
    uint8_t v;
    if (len > SLOT_BYTES)
        return;
    s = take_slot(&i);
    if (!s)
        return;
    s->at = midi_now_us();
    s->len = (uint16_t)len;
    s->from = FROM_NETWORK;
    memcpy(s->data, eth, len);
    v = (uint8_t)i;
    xQueueSend(s_work, &v, 0);
}

/* the TinyUSB task */
static void on_usb(const uint8_t *eth, size_t len)
{
    int i;
    struct slot *s;
    uint8_t v;
    if (len > SLOT_BYTES)
        return;
    s = take_slot(&i);
    if (!s)
        return;
    s->at = midi_now_us();
    s->len = (uint16_t)len;
    s->from = FROM_USB;
    memcpy(s->data, eth, len);
    v = (uint8_t)i;
    xQueueSend(s_work, &v, 0);
}

static void on_usb_tx_done(void *token) { give_slot((int)(intptr_t)token); }

static void to_computer(const uint8_t *eth, size_t len, int64_t at)
{
    int i;
    struct slot *s;
    uint32_t us;
    if (!usb_up())
        return;
    s = take_slot(&i);
    if (!s)
        return;
    memcpy(s->data, eth, len);
    if (usb_send(s->data, len, (void *)(intptr_t)i) != ESP_OK) {
        give_slot(i);
        return;
    }
    us = (uint32_t)(midi_now_us() - at);
    s_lat_air = us;
    s_lat_air_max = us > s_lat_air_max ? us : s_lat_air_max;
}

static void to_air(const uint8_t *eth, size_t len, int64_t at)
{
    uint32_t us;
    size_t n;
    if (s_on_network) {
        if (radio_network_tx(eth, len) != ESP_OK)
            s_tx_fail++;
        return;
    }
    n = jk_eth_to_air(&s_b, eth, len, bridge_ms(), s_air, sizeof s_air);
    if (!n)
        return;
    if (s_away) {
        s_scan_lost++;
        return;
    }
    if (radio_tx(s_air, n) != ESP_OK) {
        s_tx_fail++;
        return;
    }
    us = (uint32_t)(midi_now_us() - at);
    s_lat_usb = us;
    s_lat_usb_max = us > s_lat_usb_max ? us : s_lat_usb_max;
}

static void peer_out(void *ctx, uint32_t where, const uint8_t *eth, size_t len)
{
    int64_t now = midi_now_us();
    (void)ctx;
    if (where == JP_TO_AIR)
        to_air(eth, len, now);
    else
        to_computer(eth, len, now);
}

/* a computer that overrode its MAC: on MODE JANK's air jk_eth_to_air sees it, here nothing else would */
static void note_computer(const uint8_t *eth, size_t len)
{
    if (len >= 12u && !(eth[6] & 1u) && memcmp(eth + 6, s_b.mac, 6) && memcmp(eth + 6, s_peer.usb_mac, 6)) {
        memcpy(s_b.mac, eth + 6, 6);
        s_b.mac_pending = 1;
    }
}

static void net_out(void *ctx, const uint8_t *eth, size_t len)
{
    (void)ctx;
    to_air(eth, len, midi_now_us());
}

static void handle(struct slot *s, int i)
{
    static uint8_t eth[SLOT_BYTES];
    if (s->from == FROM_NETWORK) {
        uint8_t arp[64];                 /* an ARP reply is 42 bytes */
        size_t arp_len;
        int pass, w = wsvc_input(s_net.mac, jn_ip(&s_net), jn_mask(&s_net), s->data, s->len, arp, sizeof arp, &arp_len);   /* the Wi-Fi diagnostics (ARP, a UDP request) */
        if (w != WSVC_NOT_OURS) {
            if (w == WSVC_SEND_ARP)
                net_out(NULL, arp, arp_len);
            give_slot(i);
            return;
        }
        pass = !jn_input(&s_net, s->data, s->len, bridge_ms()) && jp_input(&s_peer, s->data, s->len, JP_TO_AIR, s->at);
        if (pass && jn_to_computer(s->data, s->len))  /* an address for the computer, never a way out (jank_net.h) */
            to_computer(s->data, s->len, s->at);
        give_slot(i);
        return;
    }
    if (s->from == FROM_AIR) {
        size_t n = jk_air_to_eth(&s_b, s->data, s->len, s->rssi, bridge_ms(), eth, sizeof eth);
        if (n && jp_input(&s_peer, eth, n, JP_TO_AIR, s->at))
            to_computer(eth, n, s->at);
    } else {
        if (s_on_network)
            note_computer(s->data, s->len);
        if (jp_input(&s_peer, s->data, s->len, JP_TO_USB, s->at))
            to_air(s->data, s->len, s->at);
    }
    give_slot(i);
}

static void follow_mac(void)
{
    if (!s_b.mac_pending)
        return;
    s_b.mac_pending = 0;
    memcpy(s_own, s_b.mac, 6);
    jp_set_air_mac(&s_peer, s_b.mac);
    jn_set_mac(&s_net, s_b.mac);
    esp_err_t e = s_on_network ? radio_network_set_mac(s_b.mac) : radio_set_mac(s_b.mac, s_b.channel);
    if (e != ESP_OK)
        ESP_LOGE(TAG, "radio to the computer's MAC: %s", esp_err_to_name(e));
}

static uint8_t lt_mode(void) { return s_cfg.transport == JK_TRANSPORT_AUTO ? JHO_IGNORE : s_lead ? JHO_LEAD : JHO_FOLLOW; }

/* what the MIDI transport should be (jank_clock.h jc_base): the session's word while in one (joined, or AUDIO mode's
 * session of its own); held through a change of GROUP; then AUDIO mode's hold (jank_hold.h): Stop silences the outputs at
 * once, Start lets them go once the tracker locks; LOW CONFIDENCE's gate (jank_conf.h): closed, the outputs stop; open
 * again, they start on the next bar line; and LINK mode's own Stop, which holds until its Start */
static void follow_transport(int64_t now)
{
    int want, automatic = s_cfg.transport == JK_TRANSPORT_AUTO;
    int session = jp_in_session(&s_peer);
    uint8_t audio, locked, gated, silent, conf_open;
    struct jc_tr t;
    beat_state(&audio, &locked);
    beat_gate(&gated, &silent, &conf_open);
    t = (struct jc_tr){.audio = audio, .automatic = (uint8_t)automatic, .session = (uint8_t)session,
                       .settled = (uint8_t)jp_settled(&s_peer, now), .playing = (uint8_t)jp_playing(&s_peer),
                       .gone = (uint8_t)(automatic && !s_peer.on && now >= s_hop_until), .peers = jp_peers(&s_peer)};
    if (session)
        s_hop_until = 0;
    s_base = (uint8_t)jc_base(s_base, &t);  /* (in a session not settled yet: the last wish held, nothing started on a guess) */
    s_hin = (struct jho_in){.audio = audio, .lt = lt_mode(), .base = s_base, .locked = locked,
                            .clock_running = (uint8_t)(midi_is_playing() && !midi_start_pending()), .gated = gated,
                            .silent = silent, .conf_open = conf_open, .bar_settling = (uint8_t)beat_bar_settling()};
    want = jho_outputs(&s_hold, &s_hin);
    if (want != s_want) {
        s_want = (uint8_t)want;
        midi_want(want);
    }
    /* playing, for the flash rules (bridge_is_playing): the session plays (joined, or AUDIO mode's own), or (transport
     * LINK) the MIDI clock has a start out or armed. Not the MIDI clock under transport AUTO, where it plays whenever a
     * peer is there (Live open, stopped): no stop would ever allow an update or a save. Not a session gone: nothing could
     * stop it. */
    __atomic_store_n(&s_playing, (uint8_t)(session && (jp_playing(&s_peer) || (!automatic && midi_is_playing()))), __ATOMIC_RELAXED);
}

static void publish(int64_t now)
{
    static int64_t next;
    struct jc_map m;
    if (now < next || !jp_settled(&s_peer, now) || s_away)
        return;                       /* no settled session: the timer keeps the last tempo and phase (none: no clock) */
    next = now + PUBLISH_US;
    jc_map_make(&m, now, jp_beat_at(&s_peer, now), jp_mpb(&s_peer), (int32_t)s_cfg.offset_10us * 10);
    midi_publish(&m);
}

/* the access point came or went, DHCP's address moved */
static void follow_network(int64_t now)
{
    int up = radio_network_up();
    if (!s_on_network)
        return;
    if (up != s_net_was_up) {
        s_net_was_up = (uint8_t)up;
        jn_link(&s_net, up, bridge_ms());
    }
    jn_poll(&s_net, bridge_ms());
    jp_set_network(&s_peer, jn_ip(&s_net), jn_mask(&s_net), now);
    for (int i = 0; i < 3; i++) {        /* the Wi-Fi diagnostics' answers, a few datagrams a pass (one buffer: only this task sends them) */
        static uint8_t f[JW_FRAME_MAX];
        size_t n = wsvc_next_frame(s_net.mac, jn_ip(&s_net), f, sizeof f);
        if (!n)
            break;
        net_out(NULL, f, n);
    }
}

static void bridge_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint8_t i, audio, locked;
        int got = xQueueReceive(s_work, &i, pdMS_TO_TICKS(IDLE_WAIT_MS)) == pdTRUE;
        int64_t now = midi_now_us();
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (got)
            handle(&s_slot[i], i);
        beat_state(&audio, &locked);
        /* M2l: a session of its own while alone, in either mode (jank_peer.h), as a lone Link app: it never ends with the
         * sound (the user, 2026-11-16: "how am I supposed to start it here?") */
        if (jp_solo(&s_peer, 1, now))
            s_base = 0;
        jp_poll(&s_peer, now);
        follow_transport(now);
        publish(now);
        follow_mac();
        follow_network(now);
        xSemaphoreGive(s_lock);
    }
}

void bridge_apply(const struct jk_cfg *cfg)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int64_t now = midi_now_us();
    int moved = cfg->group != s_b.group && !s_on_network;   /* a network's channel is the access point's */
    s_cfg = *cfg;
    s_b.filter = cfg->filter;
    jp_allow(&s_peer, cfg->peer, now);
    midi_set_quantum(cfg->quantum);
    if (moved) {
        jp_leave(&s_peer, now);       /* (a bye-bye to the old group, on the channel it is still on) */
        s_hop_until = now + HOP_HOLD_US;
        if (!jk_set_group(&s_b, cfg->group)) {
            memcpy(s_bssid, s_b.bssid, 6);
            esp_err_t e = radio_channel(s_b.channel);
            if (e != ESP_OK)
                ESP_LOGE(TAG, "channel %u: %s", s_b.channel, esp_err_to_name(e));
        }
    }
    xSemaphoreGive(s_lock);
}

/* a start not reached yet, and its bar line: one waiting to be sent (play, the tracker), one sent for a bar still ahead,
 * or the session playing while the MIDI start waits for the next bar line (a peer's start "from now", as Live and the
 * FM-1 send them). The lock held */
static int pending_start(int64_t beat_ub, int64_t *bar_ub)
{
    if (jp_start_armed(&s_peer, bar_ub))
        return 1;
    if (!jp_playing(&s_peer))
        return 0;
    if (lk_play_beat(&s_peer.k) > beat_ub) {
        *bar_ub = lk_play_beat(&s_peer.k);
        return 1;
    }
    if (s_cfg.transport != JK_TRANSPORT_AUTO && midi_start_pending()) {
        *bar_ub = jst_bar_at_or_after(beat_ub, s_cfg.quantum);
        return 1;
    }
    return 0;
}

void bridge_snapshot(struct jk_snapshot *s)
{
    int8_t rssi;
    uint8_t channel, gate_off;
    memset(s, 0, sizeof *s);
    /* the Wi-Fi driver's own call outside the bridge's lock: the HOME push and the control line take this snapshot, and
     * the bridge's task (frames, Link) must never wait behind a Wi-Fi API call (the timing audit's fix 3) */
    radio_network_info(&rssi, &channel);
    radio_network_ssid(s->wifi_ssid, sizeof s->wifi_ssid);
    app_nets_get(&s->nets);            /* (the settings' lock and a 1 KB copy: outside the bridge's lock too) */
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int64_t now = midi_now_us();
    s->b = s_b;
    s->usb_up = (uint8_t)usb_up();
    s->tx_fail = s_tx_fail;
    s->queue_full = s_queue_full;
    s->scan_lost = s_scan_lost;
    s->lat_air_us = s_lat_air;
    s->lat_air_max_us = s_lat_air_max;
    s->lat_usb_us = s_lat_usb;
    s->lat_usb_max_us = s_lat_usb_max;
    s->wifi_up = (uint8_t)radio_network_up();
    s->wifi_ip = jn_ip(&s_net);
    s->wifi_rssi = rssi;
    s->wifi_channel = channel;
    s->link_on = s_peer.on;
    s->link_ip = s_peer.ip;
    s->link_peers = jp_peers(&s_peer);
    if (jp_in_session(&s_peer)) {
        s->mpb = jp_mpb(&s_peer);
        s->beat_ub = jp_beat_at(&s_peer, now);
        s->playing = (uint8_t)jp_playing(&s_peer);
        s->settled = (uint8_t)jp_settled(&s_peer, now);
        s->armed = (uint8_t)pending_start(s->beat_ub, &s->start_ub);
    }
    s->hold = s_hold.hold;
    gate_off = s_hold.gate_off;
    if (s_hin.audio)
        s->outputs = jho_icon(&s_hold, &s_hin);
    else                               /* LINK mode: the clocks as they are, a start waiting for its bar line armed (not
                                        * after the stick's own Stop: its clocks wait for its Start, not for a bar line) */
        s->outputs = s_hin.clock_running ? JHO_RUNNING : !s_hold.hold && (s->armed || midi_start_pending()) ? JHO_ARMED : JHO_STOPPED;
    xSemaphoreGive(s_lock);
    midi_stats(&s->midi_ticks, &s->midi_resync);
    beat_stats(&s->beat);
    if (gate_off)
        s->beat.gated = 0;             /* gated= as it acts: a Start disarmed it (core/jank_hold.h gate_off, M2g) */
    s->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    s->batt_pct = (int8_t)linein_batt_pct();   /* the cell from the LINE IN's ADC pattern (linein.c); -1: none */
    snprintf(s->board, sizeof s->board, "%s", BOARD_NAME);
}


void bridge_away(int away) { s_away = away != 0; }

void bridge_note_scan_groups(struct js_scan *out) { *out = s_scan; }

void bridge_table_start(void)
{
    js_table_clear(&s_table);
    s_table_on = 1;
}

void bridge_table_stop(struct js_table *out)
{
    s_table_on = 0;
    *out = s_table;
}

int bridge_beat(int64_t *beat_ub, int32_t *mpb)
{
    int joined;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    joined = jp_in_session(&s_peer);   /* (AUDIO mode's session of its own too: the LED shows the tracker's beat) */
    if (joined) {
        *mpb = jp_mpb(&s_peer);
        *beat_ub = jp_beat_at(&s_peer, midi_now_us());
    }
    xSemaphoreGive(s_lock);
    return joined;
}

int bridge_tempo(int32_t mpb)
{
    int r;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    r = jp_set_tempo(&s_peer, mpb, midi_now_us());
    xSemaphoreGive(s_lock);
    return r;
}

/* Read without the bridge's lock: TinyUSB's task asks this for every sector of a drive copy, and the bridge task can sit
 * on its lock waiting for TinyUSB's queue (to_computer), which only TinyUSB's task empties. */
int bridge_is_playing(void) { return __atomic_load_n(&s_playing, __ATOMIC_RELAXED); }

int bridge_play(int play)
{
    int r = 0;
    uint8_t audio, locked;
    beat_state(&audio, &locked);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (audio) {
        jho_set(&s_hold, !play);      /* AUDIO mode: Stop / Start of the outputs; the tracker keeps the session (jank_hold.h) */
        if (play && s_hold.arming)
            beat_fold_resync();       /* Start: a resync, the fold back to 1 (the emulator's setHold) */
        follow_transport(midi_now_us());   /* the clocks stop now, not at the bridge's next step */
    } else {
        /* LINK mode: the stick's own clocks (a Stop holds until its Start, jank_hold.h jho_link_stop); with transport LINK
         * (FOLLOW+LEAD) the session too. AUTO (IGNORE): the session is not the stick's to start or stop */
        if (s_cfg.transport != JK_TRANSPORT_AUTO || !jp_peers(&s_peer))   /* M2l: alone, the session is the stick's */
            r = jp_set_playing(&s_peer, play, s_cfg.quantum, midi_now_us());
        if (play)
            jho_link_play(&s_hold);
        else
            jho_link_stop(&s_hold);
        follow_transport(midi_now_us());
    }
    xSemaphoreGive(s_lock);
    uis_started_set(play);            /* every user press (HOME's YES, `play` / `stop`, the click) ends here: stored */
    return r;
}

void bridge_set_lead(int lead)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_lead = (uint8_t)(lead != 0);
    xSemaphoreGive(s_lock);
}

int bridge_session_held(void) { return jho_session_held(&s_hold, &s_hin); }

int bridge_user_stopped(void)
{
    int held;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    held = s_hold.hold;
    xSemaphoreGive(s_lock);
    return held;
}

void bridge_with_peer(void (*fn)(struct jank_peer *p, int64_t now, void *ctx), void *ctx)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    fn(&s_peer, midi_now_us(), ctx);
    xSemaphoreGive(s_lock);
}

/* the Link clock filter's state, for `diag` */
void bridge_diag(char *out, size_t cap)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snprintf(out, cap, "link_settled=%u\nlink_drift_ppb=%ld\nlink_last_err_us=%lld\nlink_far=%u\nlink_partner_rtt_us=%u\n", s_peer.k.settled,
             (long)lk_drift_ppb(&s_peer.k), (long long)s_peer.k.last_err, s_peer.k.far, s_peer.k.last_rtt);
    xSemaphoreGive(s_lock);
}

uint32_t bridge_peers_heard(void)
{
    uint32_t n;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    n = s_on_network ? jp_peers(&s_peer) : jk_peers_heard(&s_b, bridge_ms());
    xSemaphoreGive(s_lock);
    return n;
}

esp_err_t bridge_start(const struct jk_cfg *cfg, const uint8_t mac[6], const uint8_t peer_mac[6])
{
    esp_err_t e;
    s_cfg = *cfg;
    if (!s_slot)
        s_slot = heap_caps_calloc(SLOTS, sizeof *s_slot,
                                  (BOARD_BRIDGE_SLOTS_PSRAM ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL) | MALLOC_CAP_8BIT);
    if (!s_slot)
        return ESP_ERR_NO_MEM;
    s_lock = xSemaphoreCreateMutex();
    s_free = xQueueCreate(SLOTS, 1);
    s_work = xQueueCreate(SLOTS, 1);
    if (!s_lock || !s_free || !s_work || jk_init(&s_b, mac, cfg->group, cfg->filter))
        return ESP_ERR_NO_MEM;
    for (int i = 0; i < (int)SLOTS; i++)
        give_slot(i);
    memcpy(s_bssid, s_b.bssid, 6);
    memcpy(s_own, mac, 6);
    js_clear(&s_scan);
    jho_init(&s_hold);                 /* power-on: the user's layer stopped until the user's Start (M2h) ... */
    if (uis_started_get())
        jho_set(&s_hold, 0);           /* ... unless the user's last press was Start (stored): as that Start, nothing sent to a Link session */
    jp_init(&s_peer, peer_mac, mac, esp_random(), peer_out, NULL);
    s_on_network = cfg->net == JK_NET_WIFI;
    jn_init(&s_net, mac, esp_random(), cfg->name, LK_MCAST_IP, net_out, NULL);
    if (s_on_network)
        jp_set_network(&s_peer, 0, 0, 0);
    e = midi_start();
    if (e != ESP_OK)
        return e;
    jp_allow(&s_peer, cfg->peer, midi_now_us());
    midi_set_quantum(cfg->quantum);
    e = usb_start(mac, on_usb, on_usb_tx_done, ctl_serial_rx);
    if (e != ESP_OK)
        return e;
    e = s_on_network ? radio_start_network(mac, on_network) : radio_start(mac, s_b.channel, on_air);
    if (e != ESP_OK) {
        if (usb_is_host())               /* the radio after a host start that used the memory: a restart as a device, with the reason kept */
            usbrole_host_failed(JHD_STEP_RADIO, e);
        return e;
    }
    if (s_on_network)                    /* read-only diagnostics over the network (core/jank_wsvc.h); not fatal: without it the rest runs */
        (void)wsvc_start();
    hostdiag_mark(JHD_STEP_BRIDGE);      /* the heap before the bridge task's 6 KB stack (host mode only) */
    if (xTaskCreatePinnedToCore(bridge_task, "bridge", 6144, NULL, TASK_PRIO, NULL, 0) != pdPASS) {
        if (usb_is_host())               /* host start used the memory: a restart as a device, with the reason and the heap trace kept */
            usbrole_host_failed(JHD_STEP_BRIDGE, ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }
    hostdiag_mark(JHD_STEP_BEAT);
    e = beat_start();
    if (e != ESP_OK) {
        if (usb_is_host())
            usbrole_host_failed(JHD_STEP_BEAT, e);
        return e;
    }
    /* the LINE IN after the tracker (its ring is ready); not fatal: without it the rest runs on USB audio */
    if (linein_start() != ESP_OK)
        ESP_LOGE(TAG, "LINE IN task: no memory");
    usb_host_boot_done();                /* the start-up is through: a host start's hang guard (usb.c) stands down */
    return ESP_OK;
}
