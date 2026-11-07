/* SPDX-License-Identifier: GPL-3.0-only */
/* The bridge: Ethernet <-> MODE JAM air frames (jank_core.h). Written from the air format, not from Optimist's code. */
#include "jank_core.h"
#include <string.h>

#define FC_DATA 0x08u
#define FC_TYPE_MASK 0x0Cu
#define FC_VERSION_MASK 0x03u
#define FC_SUB_QOS 0x80u
#define FC_SUB_NODATA 0x40u
#define FC1_TODS 0x01u
#define FC1_FROMDS 0x02u
#define FC1_MOREFRAG 0x04u
#define FC1_RETRY 0x08u
#define FC1_PROTECTED 0x40u
#define FC1_ORDER 0x80u
#define QOS_AMSDU 0x80u

#define ETH_ARP 0x0806u
#define ETH_IPV4 0x0800u
#define IP_UDP 17u

static const uint8_t SNAP[6] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00};
static const uint8_t LINK_GROUP_MAC[6] = {0x01, 0x00, 0x5E, 0x4C, 0x4E, 0x4B};

uint8_t jk_channel(uint32_t group)
{
    static const uint8_t ch[3] = {1, 6, 11};
    if (group < JK_GROUP_MIN || group > JK_GROUP_MAX)
        return 0;
    return ch[(group - 1u) % 3u];
}

int jk_bssid(uint32_t group, uint8_t out[6])
{
    static const uint8_t prefix[5] = {0x02, 0x66, 0x6D, 0x31, 0x6A};
    if (group < JK_GROUP_MIN || group > JK_GROUP_MAX)
        return -1;
    memcpy(out, prefix, 5);
    out[5] = (uint8_t)group;
    return 0;
}

static const uint8_t JANK_PREFIX[3] = {0x02, 0x6A, 0x6B};

void jk_mac_from_efuse(const uint8_t efuse[6], uint8_t out[6])
{
    memcpy(out, JANK_PREFIX, 3);
    memcpy(out + 3, efuse + 3, 3);
}

int jk_mac_is_jank(const uint8_t mac[6]) { return !memcmp(mac, JANK_PREFIX, 3); }

static int mac_is_group(const uint8_t *m) { return m[0] & 1u; }

int jk_init(struct jk_bridge *b, const uint8_t mac[6], uint32_t group, uint32_t filter)
{
    if (!jk_channel(group) || filter > JK_FILTER_ALL || mac_is_group(mac))
        return -1;
    memset(b, 0, sizeof *b);
    memcpy(b->mac, mac, 6);
    b->filter = (uint8_t)filter;
    return jk_set_group(b, group);
}

int jk_set_group(struct jk_bridge *b, uint32_t group)
{
    if (jk_bssid(group, b->bssid))
        return -1;
    b->group = (uint8_t)group;
    b->channel = jk_channel(group);
    memset(b->peer, 0, sizeof b->peer);
    return 0;
}

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }

/* Link's multicast UDP, with the bounds of a frame that may be cut short */
static int is_link_multicast(const uint8_t *eth, size_t len)
{
    const uint8_t *ip = eth + JK_ETH_HDR;
    size_t ihl;
    if (memcmp(eth, LINK_GROUP_MAC, 6) || len < JK_ETH_HDR + 20u || (ip[0] >> 4) != 4u || ip[9] != IP_UDP)
        return 0;
    ihl = (size_t)(ip[0] & 0x0Fu) * 4u;
    return ihl >= 20u && len >= JK_ETH_HDR + ihl + 8u && rd16(ip + ihl + 2u) == JK_LINK_PORT;
}

static int passes_filter(const struct jk_bridge *b, const uint8_t *eth, size_t len)
{
    uint32_t type = rd16(eth + 12);
    if (b->filter == JK_FILTER_ALL || type == ETH_ARP)
        return 1;
    if (type != ETH_IPV4)
        return 0;
    return !mac_is_group(eth) || is_link_multicast(eth, len);
}

/* a source address that is not ours: the computer overrode the one offered */
static int follow_source(struct jk_bridge *b, const uint8_t *src, uint32_t now_ms)
{
    if (!memcmp(src, b->mac, 6))
        return 1;
    if (b->adopted && now_ms - b->t_adopt < JK_ADOPT_HOLD_MS) {
        b->st.drop_foreign++;
        return 0;
    }
    memcpy(b->mac, src, 6);
    b->adopted = 1;
    b->t_adopt = now_ms;
    b->mac_pending = 1;
    b->st.mac_adopted++;
    return 1;
}

static size_t write_air(struct jk_bridge *b, const uint8_t *eth, size_t pl, uint8_t *air)
{
    uint32_t sc = (uint32_t)b->seq << 4;
    b->seq = (uint16_t)((b->seq + 1u) & 0x0FFFu);
    memset(air, 0, JK_DOT11_LEN);
    air[0] = FC_DATA;
    memcpy(air + 4, eth, 6);
    memcpy(air + 10, b->mac, 6);
    memcpy(air + 16, b->bssid, 6);
    air[22] = (uint8_t)sc;
    air[23] = (uint8_t)(sc >> 8);
    memcpy(air + JK_DOT11_LEN, SNAP, 6);
    memcpy(air + JK_DOT11_LEN + 6u, eth + 12, 2);     /* the EtherType, already big-endian */
    memcpy(air + JK_DOT11_LEN + JK_LLC_LEN, eth + JK_ETH_HDR, pl);
    return JK_DOT11_LEN + JK_LLC_LEN + pl;
}

size_t jk_eth_to_air(struct jk_bridge *b, const uint8_t *eth, size_t len, uint32_t now_ms, uint8_t *air, size_t cap)
{
    size_t pl;
    b->st.usb_in++;
    if (len < JK_ETH_HDR || mac_is_group(eth + 6) || rd16(eth + 12) < 0x0600u) {
        b->st.drop_bad++;
        return 0;
    }
    if (!passes_filter(b, eth, len)) {
        b->st.drop_filter++;
        return 0;
    }
    pl = len - JK_ETH_HDR;
    if (pl > JK_PAYLOAD_MAX || cap < JK_DOT11_LEN + JK_LLC_LEN + pl) {
        b->st.drop_big++;
        return 0;
    }
    if (!follow_source(b, eth + 6, now_ms))
        return 0;
    b->st.air_out++;
    return write_air(b, eth, pl, air);
}

int jk_air_is_group(const uint8_t bssid[6], const uint8_t *air, size_t len)
{
    return len >= JK_DOT11_LEN && (air[0] & FC_TYPE_MASK) == FC_DATA && !memcmp(air + 16, bssid, 6);
}

/* 0: no plain open data frame of a sender. Otherwise the offset of the LLC header */
static size_t data_offset(const uint8_t *a, size_t len)
{
    size_t hdr = JK_DOT11_LEN;
    if ((a[0] & FC_VERSION_MASK) || (a[0] & FC_SUB_NODATA))
        return 0;
    if ((a[1] & (FC1_TODS | FC1_FROMDS | FC1_PROTECTED | FC1_MOREFRAG)) || mac_is_group(a + 10) || (a[22] & 0x0Fu))
        return 0;
    if (a[0] & FC_SUB_QOS) {
        hdr += 2u + ((a[1] & FC1_ORDER) ? 4u : 0u);
        if (len < hdr || (a[24] & QOS_AMSDU))
            return 0;
    }
    if (len < hdr + JK_LLC_LEN || memcmp(a + hdr, SNAP, 6) || rd16(a + hdr + 6u) < 0x0600u)
        return 0;
    return hdr;
}

static struct jk_peer *peer_slot(struct jk_bridge *b, const uint8_t *mac, uint32_t now_ms)
{
    struct jk_peer *free_slot = NULL;
    for (uint32_t i = 0; i < JK_PEERS; i++) {
        struct jk_peer *p = &b->peer[i];
        if (p->used && !memcmp(p->mac, mac, 6))
            return p;
        if (!free_slot && (!p->used || now_ms - p->t_heard >= JK_HEARD_MS))
            free_slot = p;
    }
    if (free_slot) {
        memset(free_slot, 0, sizeof *free_slot);
        memcpy(free_slot->mac, mac, 6);
        free_slot->used = 1;
    }
    return free_slot;
}

/* 1: this is a retry of what was passed already */
static int note_peer(struct jk_bridge *b, const uint8_t *a, int8_t rssi, uint32_t now_ms)
{
    struct jk_peer *p = peer_slot(b, a + 10, now_ms);
    uint32_t seq = (uint32_t)a[22] >> 4 | (uint32_t)a[23] << 4;
    if (!p)
        return 0;                     /* a full table still bridges: no retry filter for that unit */
    p->rssi = rssi;
    p->t_heard = now_ms;
    p->frames++;
    if ((a[1] & FC1_RETRY) && p->have_seq && p->seq == seq)
        return 1;
    p->have_seq = 1;
    p->seq = (uint16_t)seq;
    return 0;
}

size_t jk_air_to_eth(struct jk_bridge *b, const uint8_t *a, size_t len, int8_t rssi, uint32_t now_ms, uint8_t *eth, size_t cap)
{
    size_t hdr, pl;
    if (!jk_air_is_group(b->bssid, a, len))
        return 0;
    hdr = data_offset(a, len);
    if (!hdr) {
        b->st.drop_bad++;
        return 0;
    }
    if (!memcmp(a + 10, b->mac, 6)) {
        b->st.drop_own++;
        return 0;
    }
    b->st.air_in++;
    if (note_peer(b, a, rssi, now_ms)) {
        b->st.drop_dup++;
        return 0;
    }
    if (!mac_is_group(a + 4) && memcmp(a + 4, b->mac, 6)) {
        b->st.drop_other++;
        return 0;
    }
    pl = len - hdr - JK_LLC_LEN;
    if (cap < JK_ETH_HDR + pl) {
        b->st.drop_big++;
        return 0;
    }
    memcpy(eth, a + 4, 6);
    memcpy(eth + 6, a + 10, 6);
    memcpy(eth + 12, a + hdr + 6u, 2);
    memcpy(eth + JK_ETH_HDR, a + hdr + JK_LLC_LEN, pl);
    b->st.usb_out++;
    return JK_ETH_HDR + pl;
}

uint32_t jk_peers_heard(const struct jk_bridge *b, uint32_t now_ms)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < JK_PEERS; i++)
        n += b->peer[i].used && now_ms - b->peer[i].t_heard < JK_HEARD_MS;
    return n;
}

int8_t jk_best_rssi(const struct jk_bridge *b, uint32_t now_ms)
{
    int8_t best = 0;
    for (uint32_t i = 0; i < JK_PEERS; i++) {
        const struct jk_peer *p = &b->peer[i];
        if (p->used && now_ms - p->t_heard < JK_HEARD_MS && (!best || p->rssi > best))
            best = p->rssi;
    }
    return best;
}

uint32_t jk_next_group(uint32_t group)
{
    return group >= JK_GROUP_MIN && group < JK_GROUP_MAX ? group + 1u : JK_GROUP_MIN;
}

int jk_led_lit(uint32_t net, int joined, int64_t beat_ub, int32_t mpb, uint32_t quantum)
{
    const int64_t beat = 1000000;
    int rest = net == JK_NET_WIFI;
    int64_t at, index, flash;
    if (!joined || mpb <= 0)
        return rest;
    at = beat_ub % beat;
    if (at < 0)
        at += beat;
    index = (beat_ub - at) / beat;              /* floor, before the session's beat 0 too */
    quantum = quantum ? quantum : 1u;
    index %= (int64_t)quantum;
    flash = (int64_t)(index == 0 ? JK_BAR_FLASH_MS : JK_BEAT_FLASH_MS) * 1000 * beat / mpb;
    if (flash > beat / JK_FLASH_CAP_DIV)
        flash = beat / JK_FLASH_CAP_DIV;
    return at < flash ? !rest : rest;
}
