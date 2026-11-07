/* SPDX-License-Identifier: GPL-3.0-only */
/* The radio: promiscuous receive and esp_wifi_80211_tx, as a station that never associates.
 * That state matters. ieee80211_raw_frame_sanity_check (libnet80211.a, IDF 5.5.6, read in its disassembly) refuses
 * Protected and QoS frames always, but compares addr2 and the ToDS bits only when a BSS exists. Unassociated, a plain
 * data frame with ToDS = FromDS = 0 goes out. addr2 is free too, but it is kept equal to the station MAC, so that the
 * MAC's filter acknowledges unicast to that address. */
#include <string.h>
#include "app.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#define FCS_LEN 4u
#define RATE WIFI_PHY_RATE_1M_L      /* what the FM-1 sends in MODE JAM */
static const char *TAG = "radio";
static radio_rx_fn s_rx;

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

#if !CONFIG_JANK_USB_NET
/* what the driver says of each unicast sent: test 2's evidence */
static volatile uint32_t s_unicast_ok, s_unicast_fail;

static void on_sent(const esp_80211_tx_info_t *t)
{
    if (t->des_addr[0] & 1u)
        return;
    if (t->tx_status == WIFI_SEND_SUCCESS)
        s_unicast_ok++;
    else
        s_unicast_fail++;
}

void radio_unicast_results(uint32_t *ok, uint32_t *fail)
{
    *ok = s_unicast_ok;
    *fail = s_unicast_fail;
}
#endif

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
#if !CONFIG_JANK_USB_NET
    if (e == ESP_OK)
        e = esp_wifi_register_80211_tx_cb(on_sent);
#endif
    return e == ESP_OK ? bring_up(mac, channel) : e;
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
