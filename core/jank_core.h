/* SPDX-License-Identifier: GPL-3.0-only */
/* jank's portable core: no ESP-IDF in here, so all of it runs in host tests.
 *
 * It turns Ethernet frames from the computer into MODE JANK air frames and back, keeps the
 * table of units heard, and answers the control line protocol. Air format: see README.md.
 * The bridge is written from the format, not from Optimist's code. */
#ifndef JANK_CORE_H
#define JANK_CORE_H
#include <stddef.h>
#include <stdint.h>

#define JK_VERSION "0.1.0"

#define JK_GROUP_MIN 1u
#define JK_GROUP_MAX 9u

#define JK_DOT11_LEN 24u              /* non-QoS data header, what MODE JANK sends */
#define JK_LLC_LEN 8u                 /* LLC/SNAP: AA AA 03 00 00 00 + EtherType */
#define JK_AIR_MAX 1500u              /* esp_wifi_80211_tx's limit on a whole frame, in the FCS-less length */
#define JK_ETH_HDR 14u
#define JK_PAYLOAD_MAX (JK_AIR_MAX - JK_DOT11_LEN - JK_LLC_LEN)   /* 1468 */
#define JK_ETH_MAX (JK_ETH_HDR + JK_PAYLOAD_MAX)

#define JK_PEERS 8u                   /* units kept; a bigger group still bridges, it is only not counted */
#define JK_HEARD_MS 3000u             /* a unit not heard this long is gone (the same 3 s MODE JANK uses) */
#define JK_ADOPT_HOLD_MS 10000u       /* after adopting a computer's MAC, no other is adopted for this long */
#define JK_NAME_MAX 23u

#define JK_LINK_PORT 20808u           /* Ableton Link's multicast: 224.76.78.75:20808 */

enum jk_filter {
    JK_FILTER_LINK = 0,               /* to the air: ARP, IPv4 unicast, Link's multicast. Air time is 8 us a byte */
    JK_FILTER_ALL = 1                 /* to the air: everything the computer sends */
};

struct jk_stats {
    uint32_t usb_in;                  /* Ethernet frames the computer sent */
    uint32_t air_out;                 /* of those, made into air frames */
    uint32_t air_in;                  /* frames of our group heard, any destination */
    uint32_t usb_out;                 /* of those, passed to the computer */
    uint32_t drop_filter;             /* computer's chatter kept off the air (JK_FILTER_LINK) */
    uint32_t drop_big;                /* over JK_PAYLOAD_MAX */
    uint32_t drop_bad;                /* malformed: too short, group source address, not plain open data, no LLC/SNAP */
    uint32_t drop_dup;                /* a retry of a frame already passed on */
    uint32_t drop_own;                /* our own address as sender */
    uint32_t drop_other;              /* a unit's unicast to another unit */
    uint32_t drop_foreign;            /* another source MAC while holding JK_ADOPT_HOLD_MS */
    uint32_t mac_adopted;             /* the computer used another MAC than the one offered and it was adopted */
};

struct jk_peer {
    uint8_t mac[6];
    uint8_t used;
    uint8_t have_seq;
    int8_t rssi;                      /* dBm of the last frame */
    uint16_t seq;                     /* last sequence number passed on */
    uint32_t t_heard;                 /* ms */
    uint32_t frames;
};

struct jk_bridge {
    uint8_t mac[6];                   /* the computer's adapter address, and the stick's on the air */
    uint8_t bssid[6];
    uint8_t group, channel, filter;
    uint8_t mac_pending;              /* mac changed: the radio has to follow */
    uint8_t adopted;
    uint16_t seq;
    uint32_t t_adopt;
    struct jk_peer peer[JK_PEERS];
    struct jk_stats st;
};

uint8_t jk_channel(uint32_t group);                         /* 1, 6, 11 repeating; 0 for a bad group */
int jk_bssid(uint32_t group, uint8_t out[6]);               /* 02:66:6D:31:6A:GROUP; 0 ok, -1 bad group */
/* A jank on the air: addr2 starts 02:6A:6B ("jk", locally administered), then the chip's last three bytes. It is the only
 * thing that tells a jank from an FM-1: their frames are the same. A computer that overrode the MAC loses it. */
void jk_mac_from_efuse(const uint8_t efuse[6], uint8_t out[6]);
int jk_mac_is_jank(const uint8_t mac[6]);

int jk_init(struct jk_bridge *b, const uint8_t mac[6], uint32_t group, uint32_t filter);
int jk_set_group(struct jk_bridge *b, uint32_t group);      /* forgets the units heard: another channel */

/* computer -> air. Returns the air frame's length, 0 when dropped. air holds JK_AIR_MAX. */
size_t jk_eth_to_air(struct jk_bridge *b, const uint8_t *eth, size_t len, uint32_t now_ms, uint8_t *air, size_t cap);

/* cheap test for the radio's receive callback: does this look like our group's data frame? (no state changed) */
int jk_air_is_group(const uint8_t bssid[6], const uint8_t *air, size_t len);

/* air -> computer: len without the FCS. Returns the Ethernet frame's length, 0 when not for the computer. */
size_t jk_air_to_eth(struct jk_bridge *b, const uint8_t *air, size_t len, int8_t rssi, uint32_t now_ms, uint8_t *eth, size_t cap);

uint32_t jk_peers_heard(const struct jk_bridge *b, uint32_t now_ms);
int8_t jk_best_rssi(const struct jk_bridge *b, uint32_t now_ms);     /* 0: none heard */

/* settings */
enum jk_transport {
    JK_TRANSPORT_LINK = 0,            /* MIDI start/stop follows Link's Start Stop Sync */
    JK_TRANSPORT_AUTO = 1             /* start at the next bar once a session is joined, stop when it is gone */
};
enum jk_net {
    JK_NET_JANK = 0,                   /* MODE JANK's air, in GROUP */
    JK_NET_WIFI = 1                   /* a normal network: ssid and pass; read at start-up */
};
#define JK_SSID_MAX 32u
#define JK_PASS_MIN 8u
#define JK_PASS_MAX 63u
#define JK_QUANTUM_MAX 16u
#define JK_OFFSET_MAX_10US 10000       /* +-100 ms */
struct jk_cfg {
    uint8_t group, filter;
    uint8_t peer;                     /* 1: jank is a Link peer itself */
    uint8_t transport;                /* enum jk_transport */
    uint8_t net;                      /* enum jk_net */
    uint8_t quantum;                  /* beats in a bar for MIDI start, 1..JK_QUANTUM_MAX */
    int16_t offset_10us;              /* MIDI clock against the session in 10 us steps, +-100 ms: the gear's own lag (was whole ms) */
    char name[JK_NAME_MAX + 1];
    char ssid[JK_SSID_MAX + 1];
    char pass[JK_PASS_MAX + 1];       /* never sent back: get says only whether there is one */
};
void jk_cfg_defaults(struct jk_cfg *c);
int jk_cfg_set_name(struct jk_cfg *c, const char *name);     /* 0 ok, -1 empty, too long or not printable ASCII */

/* the control line protocol (SysEx on the USB MIDI port: jank_sysex.h). Each answer is lines of text ending in "ok" or "err <why>". */
struct jk_snapshot {
    uint8_t usb_up;
    uint32_t uptime_s;
    uint32_t tx_fail;                 /* the radio refused a frame */
    uint32_t queue_full;              /* frames dropped because the bridge's queue was full */
    uint32_t scan_lost;               /* frames for the air dropped while a group scan was on another channel */
    uint32_t lat_air_us, lat_air_max_us;   /* air -> USB handed over, in the stick: last, highest */
    uint32_t lat_usb_us, lat_usb_max_us;   /* USB -> radio handed over, in the stick: last, highest */
    uint8_t link_on;                  /* the Link peer is in the session's traffic */
    uint8_t playing;                  /* the session plays (Start Stop Sync) */
    uint32_t link_ip;                 /* the peer's own address, host order; 0: none yet */
    uint32_t link_peers;              /* Link peers in the session, the stick not counted */
    int32_t mpb;                      /* us a beat; 0: no session */
    int64_t beat_ub;                  /* session beat now, micro-beats */
    uint8_t wifi_up;                  /* associated with the access point */
    uint32_t wifi_ip;                 /* the address DHCP gave, host order; 0: none yet */
    int8_t wifi_rssi;
    uint8_t wifi_channel;
    uint32_t midi_ticks;              /* MIDI clocks sent */
    uint32_t midi_resync;             /* the clock had to jump to the session */
    struct jk_bridge b;
};

enum jk_ctl_action {
    JK_CTL_NONE = 0,
    JK_CTL_CFG_CHANGED,               /* the caller stores cfg and applies it */
    JK_CTL_REBOOT,
    JK_CTL_BOOTLOADER,                /* restart into the ROM's download mode, for flashing with no button */
    JK_CTL_SCAN,                      /* out is empty: the caller scans and answers with js_table_report */
    JK_CTL_TEMPO,                     /* out is empty: the caller sets the session's tempo (jk_ctl_tempo), answers ok or err */
    JK_CTL_PLAY,                      /* out is empty: the caller starts the session, answers ok or err */
    JK_CTL_STOP                       /* out is empty: the caller stops the session, answers ok or err */
};
enum jk_ctl_action jk_ctl_line(const char *line, struct jk_cfg *cfg, const struct jk_snapshot *snap, uint32_t now_ms, char *out, size_t cap);
#define JK_BPM_MIN_X100 2000u          /* the BPM the tempo command takes, hundredths: Link's own clamp, 20..999 */
#define JK_BPM_MAX_X100 99900u
/* "tempo <bpm>", BPM 20..999 with up to two decimals -> 0 and us a beat; -1 not one */
int jk_ctl_tempo(const char *line, int32_t *mpb);

/* on the session's beat: MODE JANK rests off and flashes on, the Wi-Fi network the other way round. No session: the rest.
 * beat_ub in micro-beats, mpb us a beat. -> 1: lit */
#define JK_BEAT_FLASH_MS 80u
#define JK_BAR_FLASH_MS 150u           /* the bar's first beat, by quantum, on Link's beat count as MIDI start uses */
#define JK_FLASH_CAP_DIV 3             /* no flash is longer than a third of a beat: they stay apart at fast tempos */
int jk_led_lit(uint32_t net, int joined, int64_t beat_ub, int32_t mpb, uint32_t quantum);

/* the GROUP after this one, 9 wraps to 1 */
uint32_t jk_next_group(uint32_t group);
#endif
