/* SPDX-License-Identifier: GPL-3.0-only */
/* The bridge task: one owner for the core's state, frames from the radio and from USB in one queue of fixed slots.
 * A full queue drops the frame: Link's next one is worth more than an old one. */
#include <string.h>
#include "app.h"
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
static struct slot s_slot[SLOTS];
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
static uint8_t s_want;
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
        int pass = !jn_input(&s_net, s->data, s->len, bridge_ms()) && jp_input(&s_peer, s->data, s->len, JP_TO_AIR, s->at);
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

/* what the MIDI transport should be: the session's word while joined; held through a change of GROUP */
static void follow_transport(int64_t now)
{
    int want = s_want;
    int automatic = s_cfg.transport == JK_TRANSPORT_AUTO;
    if (jp_joined(&s_peer)) {
        want = jc_should_play(automatic, jp_playing(&s_peer), jp_peers(&s_peer));
        s_hop_until = 0;
    } else if (automatic && !s_peer.on && now >= s_hop_until) {
        want = 0;
    }
    if (want != s_want) {
        s_want = (uint8_t)want;
        midi_want(want);
    }
}

static void publish(int64_t now)
{
    static int64_t next;
    struct jc_map m;
    if (now < next || !jp_joined(&s_peer) || s_away)
        return;                       /* no session: the timer keeps the last tempo and phase */
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
}

static void bridge_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint8_t i;
        int got = xQueueReceive(s_work, &i, pdMS_TO_TICKS(IDLE_WAIT_MS)) == pdTRUE;
        int64_t now = midi_now_us();
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (got)
            handle(&s_slot[i], i);
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

void bridge_snapshot(struct jk_snapshot *s)
{
    memset(s, 0, sizeof *s);
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
    radio_network_info(&s->wifi_rssi, &s->wifi_channel);
    s->link_on = s_peer.on;
    s->link_ip = s_peer.ip;
    s->link_peers = jp_peers(&s_peer);
    if (jp_joined(&s_peer)) {
        s->mpb = jp_mpb(&s_peer);
        s->beat_ub = jp_beat_at(&s_peer, now);
        s->playing = (uint8_t)jp_playing(&s_peer);
    }
    xSemaphoreGive(s_lock);
    midi_stats(&s->midi_ticks, &s->midi_resync);
    s->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
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
    joined = jp_joined(&s_peer);
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

int bridge_play(int play)
{
    int r;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    r = jp_set_playing(&s_peer, play, midi_now_us());
    xSemaphoreGive(s_lock);
    return r;
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
    e = s_on_network ? radio_start_network(mac, cfg->ssid, cfg->pass, on_network) : radio_start(mac, s_b.channel, on_air);
    if (e != ESP_OK)
        return e;
    return xTaskCreatePinnedToCore(bridge_task, "bridge", 6144, NULL, TASK_PRIO, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
