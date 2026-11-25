/* SPDX-License-Identifier: GPL-3.0-only */
/* The radio. In MODE JANK: promiscuous receive and esp_wifi_80211_tx, as a station that never associates.
 * On a normal network: an ordinary station whose Ethernet frames go straight to the bridge, with no IP stack on top
 * (ESP-IDF's sta2eth example does the same). Its MAC is the computer's, so the access point sees one device.
 *
 * JANK's raw frames:
 * That state matters. ieee80211_raw_frame_sanity_check (libnet80211.a, IDF 5.5.6, read in its disassembly) refuses
 * Protected and QoS frames always, but compares addr2 and the ToDS bits only when a BSS exists. Unassociated, a plain
 * data frame with ToDS = FromDS = 0 goes out. addr2 is free too, but it is kept equal to the station MAC, so that the
 * MAC's filter acknowledges unicast to that address. */
#include <string.h>
#include "app.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_private/wifi.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#define FCS_LEN 4u
#define RATE WIFI_PHY_RATE_1M_L      /* what the FM-1 sends in MODE JANK */
static const char *TAG = "radio";
static radio_rx_fn s_rx;
static radio_eth_fn s_eth;
static volatile uint8_t s_up;

static void on_frame(void *buf, wifi_promiscuous_pkt_type_t type)
{
    const wifi_promiscuous_pkt_t *p = buf;
    size_t len = p->rx_ctrl.sig_len;
    if (type == WIFI_PKT_DATA && len > FCS_LEN)
        s_rx(p->payload, len - FCS_LEN, (int8_t)p->rx_ctrl.rssi);
}

static esp_err_t bring_up(const uint8_t mac[6], uint32_t channel)
{
    static const wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA};
    esp_err_t e = esp_wifi_set_mac(WIFI_IF_STA, mac);
    if (e == ESP_OK)
        e = esp_wifi_start();
    if (e == ESP_OK)
        e = esp_wifi_set_ps(WIFI_PS_NONE);
    if (e == ESP_OK)
        e = esp_wifi_config_80211_tx_rate(WIFI_IF_STA, RATE);
    if (e == ESP_OK)
        e = esp_wifi_set_promiscuous_filter(&filter);
    if (e == ESP_OK)
        e = esp_wifi_set_promiscuous_rx_cb(on_frame);
    if (e == ESP_OK)
        e = esp_wifi_set_promiscuous(true);
    if (e == ESP_OK)
        e = radio_channel(channel);
    return e;
}


esp_err_t radio_start(const uint8_t mac[6], uint32_t channel, radio_rx_fn rx)
{
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t e;
    s_rx = rx;
    e = esp_netif_init();
    if (e == ESP_OK)
        e = esp_event_loop_create_default();
    if (e == ESP_OK)
        e = esp_wifi_init(&init);
    if (e == ESP_OK)
        e = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (e == ESP_OK)
        e = esp_wifi_set_mode(WIFI_MODE_STA);
    return e == ESP_OK ? bring_up(mac, channel) : e;
}

static esp_err_t on_eth(void *buf, uint16_t len, void *eb)
{
    s_eth(buf, len);
    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

/* Joining a saved network. A scan (ours: s_scanning; a `scan` line's or the screen's is not) lists what is heard, strongest
 * first; jk_nets_pick takes the saved one heard best that has not failed this round (s_tried). A failure marks it and scans
 * again; none left: a new round after RETRY_US. A connection that is lost tries the same network once before that.
 * Everything runs in the event task: the retry timer only posts JANK_RADIO_RETRY to it. */
#define SCAN_MAX 40u
#define RETRY_US (5ull * 1000000ull)
ESP_EVENT_DEFINE_BASE(JANK_RADIO_EVENT);
#define JANK_RADIO_RETRY 0
static wifi_ap_record_t *s_aps;        /* SCAN_MAX each, and the list copy: PSRAM (radio_start_network), internal RAM is short in host mode */
static struct jk_seen *s_seen;
static struct jk_nets *s_saved;        /* app_nets_get's copy */
static uint32_t s_tried;
static int s_cur = -1;
static uint8_t s_scanning;             /* our scan runs */
static uint8_t s_connecting;           /* a connect to s_saved.net[s_cur] runs */
static volatile uint8_t s_restarting;  /* radio_network_set_mac stopped the radio: its DISCONNECTED is not a failure */
static char s_ssid[JK_NETS_SSID_MAX + 1];
static esp_timer_handle_t s_retry;

static void retry_later(void)
{
    esp_timer_stop(s_retry);           /* (not running: an error that does not matter) */
    if (esp_timer_start_once(s_retry, RETRY_US) != ESP_OK)
        ESP_LOGE(TAG, "no retry timer");
}

static void scan_saved(void)
{
    static const wifi_scan_config_t all = {0};
    esp_timer_stop(s_retry);
    if (s_scanning || s_connecting || s_up)
        return;
    app_nets_get(s_saved);
    if (!s_saved->n) {                  /* none saved: look again later (one may be added) */
        retry_later();
        return;
    }
    if (esp_wifi_scan_start(&all, false) == ESP_OK)
        s_scanning = 1;
    else                               /* another scan runs, or the radio stopped (STA_START follows) */
        retry_later();
}

static void on_retry(void *arg)
{
    (void)arg;
    esp_event_post(JANK_RADIO_EVENT, JANK_RADIO_RETRY, NULL, 0, 0);   /* (queue full: the next failure or STA_START scans) */
}

static void join(int i)
{
    wifi_config_t c = {0};
    const struct jk_saved_net *t = &s_saved->net[i];
    esp_timer_stop(s_retry);
    strlcpy((char *)c.sta.ssid, t->ssid, sizeof c.sta.ssid);
    strlcpy((char *)c.sta.password, t->pass, sizeof c.sta.password);
    c.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    c.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    c.sta.threshold.authmode = t->pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    s_cur = i;
    strlcpy(s_ssid, t->ssid, sizeof s_ssid);
    if (esp_wifi_set_config(WIFI_IF_STA, &c) == ESP_OK && esp_wifi_connect() == ESP_OK) {
        s_connecting = 1;
        return;
    }
    s_tried |= 1u << i;
    retry_later();
}

static void join_heard(const wifi_event_sta_scan_done_t *done)
{
    uint16_t n = SCAN_MAX;
    int i;
    s_scanning = 0;
    if (!done || done->status != 0 || esp_wifi_scan_get_ap_records(&n, s_aps) != ESP_OK)
        n = 0;                         /* a scan cut short (the radio stopped): treated as nothing heard */
    esp_wifi_clear_ap_list();          /* the records past SCAN_MAX (the weakest) */
    for (uint16_t k = 0; k < n; k++) {
        strlcpy(s_seen[k].ssid, (const char *)s_aps[k].ssid, sizeof s_seen[k].ssid);
        s_seen[k].rssi = s_aps[k].rssi;
    }
    i = jk_nets_pick(s_saved, s_seen, n, s_tried);
    if (i < 0) {                       /* none heard, or all failed: a new round later */
        s_tried = 0;
        retry_later();
        return;
    }
    join(i);
}

static void on_disconnected(void)
{
    int was_up = s_up, was_connecting = s_connecting;
    s_up = 0;
    s_connecting = 0;
    esp_wifi_internal_reg_rxcb(WIFI_IF_STA, NULL);
    if (s_restarting)                  /* the MAC changes: STA_START follows and scans */
        return;
    if (was_up && esp_wifi_connect() == ESP_OK) {   /* lost: the same network once more */
        s_connecting = 1;
        return;
    }
    if (was_connecting && s_cur >= 0)
        s_tried |= 1u << s_cur;         /* that one failed this round */
    scan_saved();
}

static void on_station(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == JANK_RADIO_EVENT) {
        if (!s_restarting)
            scan_saved();               /* (it does nothing while connecting, scanning or up) */
    } else if (id == WIFI_EVENT_STA_START) {
        s_restarting = 0;
        s_scanning = s_connecting = 0;
        s_tried = 0;
        scan_saved();
    } else if (id == WIFI_EVENT_SCAN_DONE) {
        if (s_restarting)
            s_scanning = 0;             /* cut by the stop: STA_START scans again */
        else if (s_scanning)
            join_heard(data);
    } else if (id == WIFI_EVENT_STA_CONNECTED) {
        s_tried = 0;
        s_connecting = 0;
        esp_wifi_internal_reg_rxcb(WIFI_IF_STA, on_eth);
        s_up = 1;
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        on_disconnected();
    }
}

esp_err_t radio_start_network(const uint8_t mac[6], radio_eth_fn rx)
{
    static const esp_timer_create_args_t retry = {.callback = on_retry, .name = "wifi_retry"};
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t e;
    s_eth = rx;
    s_aps = heap_caps_malloc(SCAN_MAX * sizeof *s_aps, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_seen = heap_caps_malloc(SCAN_MAX * sizeof *s_seen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_saved = app_nets_alloc();
    if (!s_aps || !s_seen || !s_saved)
        return ESP_ERR_NO_MEM;
    e = esp_timer_create(&retry, &s_retry);
    if (e == ESP_OK)
        e = esp_netif_init();
    if (e == ESP_OK)
        e = esp_event_loop_create_default();
    if (e == ESP_OK)
        e = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_station, NULL);
    if (e == ESP_OK)
        e = esp_event_handler_register(JANK_RADIO_EVENT, JANK_RADIO_RETRY, on_station, NULL);
    if (e == ESP_OK)
        e = esp_wifi_init(&init);
    if (e == ESP_OK)
        e = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (e == ESP_OK)
        e = esp_wifi_set_mode(WIFI_MODE_STA);
    if (e == ESP_OK)
        e = esp_wifi_set_mac(WIFI_IF_STA, mac);
    if (e == ESP_OK)
        e = esp_wifi_start();
    if (e == ESP_OK)
        e = esp_wifi_set_ps(WIFI_PS_NONE);   /* power save would hold multicast back to the DTIM */
    return e;
}

esp_err_t radio_network_tx(const uint8_t *eth, size_t len)
{
    return s_up ? esp_wifi_internal_tx(WIFI_IF_STA, (void *)eth, (uint16_t)len) : ESP_ERR_WIFI_NOT_CONNECT;
}

int radio_network_up(void) { return s_up; }

void radio_network_ssid(char *out, size_t cap) { strlcpy(out, s_up ? s_ssid : "", cap); }

void radio_network_info(int8_t *rssi, uint8_t *channel)
{
    wifi_ap_record_t ap;
    *rssi = 0;
    *channel = 0;
    if (s_up && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        *rssi = ap.rssi;
        *channel = ap.primary;
    }
}

esp_err_t radio_network_set_mac(const uint8_t mac[6])
{
    esp_err_t e;
    s_restarting = 1;                  /* until STA_START: the stop's DISCONNECTED and a cut scan start nothing */
    e = esp_wifi_stop();
    if (e == ESP_OK)
        e = esp_wifi_set_mac(WIFI_IF_STA, mac);
    if (e == ESP_OK)
        e = esp_wifi_start();
    if (e != ESP_OK)
        s_restarting = 0;              /* no STA_START will come: the retry timer and the next events go on as before */
    return e;
}

esp_err_t radio_channel(uint32_t channel) { return esp_wifi_set_channel((uint8_t)channel, WIFI_SECOND_CHAN_NONE); }

esp_err_t radio_set_mac(const uint8_t mac[6], uint32_t channel)
{
    esp_err_t e = esp_wifi_set_promiscuous(false);
    if (e == ESP_OK)
        e = esp_wifi_stop();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "stop: %s", esp_err_to_name(e));
        return e;
    }
    return bring_up(mac, channel);
}

esp_err_t radio_tx(const uint8_t *frame, size_t len)
{
    return esp_wifi_80211_tx(WIFI_IF_STA, frame, (int)len, false);      /* our sequence numbers */
}
