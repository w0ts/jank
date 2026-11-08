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
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_private/wifi.h"
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

static void on_station(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)data;
    if (id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (id == WIFI_EVENT_STA_CONNECTED) {
        esp_wifi_internal_reg_rxcb(WIFI_IF_STA, on_eth);
        s_up = 1;
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        s_up = 0;
        esp_wifi_internal_reg_rxcb(WIFI_IF_STA, NULL);
        esp_wifi_connect();             /* an error here means it stopped, for a new MAC, and STA_START follows */
    }
}

esp_err_t radio_start_network(const uint8_t mac[6], const char *ssid, const char *pass, radio_eth_fn rx)
{
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t c = {0};
    esp_err_t e;
    s_eth = rx;
    strlcpy((char *)c.sta.ssid, ssid, sizeof c.sta.ssid);
    strlcpy((char *)c.sta.password, pass, sizeof c.sta.password);
    c.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    c.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    c.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    e = esp_netif_init();
    if (e == ESP_OK)
        e = esp_event_loop_create_default();
    if (e == ESP_OK)
        e = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_station, NULL);
    if (e == ESP_OK)
        e = esp_wifi_init(&init);
    if (e == ESP_OK)
        e = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (e == ESP_OK)
        e = esp_wifi_set_mode(WIFI_MODE_STA);
    if (e == ESP_OK)
        e = esp_wifi_set_mac(WIFI_IF_STA, mac);
    if (e == ESP_OK)
        e = esp_wifi_set_config(WIFI_IF_STA, &c);
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
    esp_err_t e = esp_wifi_stop();
    if (e == ESP_OK)
        e = esp_wifi_set_mac(WIFI_IF_STA, mac);
    return e == ESP_OK ? esp_wifi_start() : e;
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
