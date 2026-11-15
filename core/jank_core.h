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
enum jk_ble {                         /* Bluetooth for a phone (jank_phone.h, docs/ble.md) */
    JK_BLE_PAIR = 0,                  /* on only for a pair window asked for on the device, and while the phone stays */
    JK_BLE_OFF = 1,                   /* never */
    JK_BLE_AUTO = 2                   /* as pair, plus a window at every start */
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
#define JK_BEAT_LATENCY_MIN_MS (-300)   /* negative: the stick hears the audio after the speakers do (player --xiao) */
#define JK_BEAT_LATENCY_MAX_MS 300
#define JK_BEAT_LATENCY_DEFAULT_MS 25
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
    uint8_t ble;                      /* enum jk_ble. In the byte the compiler padded the release's struct with (its size stays
                                         130): the release stored 0 there, which is the default, JK_BLE_PAIR */
    /* The beat-follow bench build's settings come after everything that is stored (store.c keeps only what is above
     * this line), so they are back at their defaults at every start. */
    uint8_t beat;                     /* 1: the beat tracker steers the Link session */
    int16_t beat_latency_ms;          /* the music is heard this long after its samples reach the stick (JK_BEAT_LATENCY_MIN_MS..MAX) */
    uint8_t autobar;                  /* 1: the downbeat estimator moves the session's bar onto the music's 1 (AUTO BAR) */
    uint8_t autobar_picks;            /* counts `set autobar on`: picked again, it leaves MANUAL even when already on */
    /* USB host mode (core/jank_usbrole.h): stored apart from the blob above (store.c, key "usb1"), so the blob stays 130 */
    uint8_t usb;                      /* enum jur_usb: USB DEVICE / HOST */
    uint8_t source;                   /* enum jur_src: AUDIO > SOURCE AUTO / LINE / USB */
};
void jk_cfg_defaults(struct jk_cfg *c);
int jk_cfg_set_name(struct jk_cfg *c, const char *name);     /* 0 ok, -1 empty, too long or not printable ASCII */
/* what store.c keeps: the release's blob (everything up to the bench settings) and, apart, usb and source */
#define JK_CFG_STORED offsetof(struct jk_cfg, beat)
int jk_cfg_stored_differs(const struct jk_cfg *a, const struct jk_cfg *b);   /* 1: a save would write something */

/* Saving settings writes flash, and a flash write stops everything but the clock's interrupt for the erase (tens of ms,
 * worst ~400 ms: the timing audit). So a change while the session plays (or the MIDI clock plays) is saved once stopped,
 * unless a restart follows the change (a restart stops the clock anyway, and the setting must be there after it). */
enum jk_save { JK_SAVE_NONE, JK_SAVE_NOW, JK_SAVE_LATER };
/* a change of the settings: *pending is the unsaved flag. JK_SAVE_NOW: save before the change is published */
enum jk_save jk_save_on_change(uint8_t *pending, int changed, int playing, int restart_follows);
/* called often (and before a restart with playing 0): 1 when the deferred save is due now (*pending cleared) */
int jk_save_due(uint8_t *pending, int playing);
/* `diag intr`: ESP-IDF's esp_intr_dump text cut to the interrupts in use, one short line each ("c1 27 L3 Used: TG0_T0"),
 * then ok; the free and reserved lines left out so it fits an answer. Returns the bytes written (0: out too small) */
size_t jk_intr_summary(const char *dump, char *out, size_t cap);

/* drive and MIDI updates: refused while playing ("stop first"): flash erases would stall USB, Wi-Fi and the tracker */
#define JK_STOP_FIRST "the session or the MIDI clock is playing: stop first"

#define JK_CTL_OUT_MAX 1792u         /* the firmware's answer buffer (ctl.c): the longest status (1510) fits with its ok; under usb.c's 2048 */
/* the control line protocol (SysEx on the USB MIDI port: jank_sysex.h). Each answer is lines of text ending in "ok" or "err <why>". */
/* the beat tracker's numbers, for the `beat` command (BENCH BUILD, see README) */
struct jk_beat {
    uint8_t on;                       /* the setting */
    uint8_t audio;                    /* the tracker's source is open (the computer's stream, a hosted device's) */
    uint8_t audio_live;               /* ... and it has sound: AUDIO mode (core/jank_live.h); 0: LINK mode */
    uint8_t mode;                     /* 0 stopped, 1 steady, 2 speeding up, 3 slowing down */
    int32_t bpm_x100;                 /* the tracker's published tempo, 0: none */
    uint32_t hops;                    /* 10 ms frames computed */
    uint32_t hop_avg_us, hop_max_us;  /* wall time of the tracker's work for one frame, preemption included */
    uint32_t hop_avg_cycles;          /* the same in CPU cycles (240 MHz in this build) */
    uint32_t cpu_pct_x10;             /* the tracker task's share of core 0 over the last second, preemption included */
    uint32_t packets, gaps, overflows, resets;   /* USB audio packets taken, 1 ms gaps between them, ring overflows, restarts */
    uint32_t heap_free, heap_min, heap_largest;  /* internal heap: now, lowest ever, largest free block */
    uint32_t psram_free, psram_min;   /* the PSRAM heap (XIAO): now, lowest ever; 0 0 when there is none */
    uint32_t stack_free;              /* bytes never used of the tracker task's stack */
    int16_t level_db;                 /* the input meter: RMS over 0.3 s, a value every 250 ms, dBFS -99 .. 0 (the line in's
                                         rms_db; the emulator's level_fast_db) */
    int16_t level_min_db, level_max_db;   /* the lowest and highest of those values over the last 3 s */
    int16_t peak_db;                  /* the input's peak over the last second, dBFS, -99 .. 0 (silence: -99) */
    uint8_t conf_pct;                 /* the tracker's confidence: the network's beat activation peak over the last second,
                                         percent (the device has no grid confidence like the player's) */
    uint32_t tempo_sets, phase_forces, starts, stops;   /* what the tracker did to the Link session */
    int32_t err_us;                   /* the session's beat grid against the tracked beat at the last step, before a correction */
    int32_t offset_us;                /* the clock the audio's samples are mapped to local time with: the offset's value */
    int32_t latency_us;               /* the setting, in us */
    uint8_t bar;                      /* the downbeat estimator: 0 off, 1 listening, 2 locked, 3 manual (jb_down.h) */
    int8_t bar_phi;                   /* locked: the count position of the 1; else -1 */
    int32_t bar_lead_x100;            /* its lead, score units x 100 */
    uint32_t bar_moves;               /* whole-beat moves of the session's bar */
};

/* the USB port, for `status` (firmware/main/usbrole.c fills it) */
#define JK_USB_DEVS 4u
#define JK_USB_NAME_MAX 23u
struct jk_usb_dev {
    char name[JK_USB_NAME_MAX + 1];   /* the product string, printable ASCII */
    uint8_t midi, audio;              /* its MIDI OUT gets the clock; its audio is what the tracker hears */
};
struct jk_usb {
    uint8_t stack;                    /* enum jur_stack: what runs (0 device, 1 host) */
    uint8_t has_port;                   /* a port chip is there and answered */
    uint8_t cc, cc_dir;               /* enum jur_cc; 1 CC1, 2 CC2, 0 unknown */
    uint8_t vbus_on, vbus, line;      /* jank's 5 V on VBUS, VBUS from outside, a plug in the line in */
    uint8_t audio;                    /* enum jur_audio: what the tracker listens to now */
    uint8_t ndev;
    struct jk_usb_dev dev[JK_USB_DEVS];
    uint32_t audio_rate;              /* the hosted stream's format; 0: none */
    uint8_t audio_channels, audio_bits;
    uint32_t audio_packets, audio_errors, midi_dropped;
};

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
    uint8_t settled;                  /* joined long enough to have the session's state (jank_peer.h jp_settled) */
    uint8_t armed;                    /* a start not reached yet: its bar line is start_ub */
    int64_t start_ub;
    uint8_t hold;                     /* AUDIO mode's Stop (core/jank_hold.h) */
    uint8_t outputs;                  /* enum jho_outputs: the clock outputs as the transport icon shows them */
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
    struct jk_beat beat;
    int8_t batt_pct;                  /* -1: no battery gauge */
    uint8_t ble_built;                /* the build has Bluetooth for a phone */
    uint8_t ble_state;                /* enum jph_state (jank_phone.h): off, window, connected */
    uint32_t ble_left_s;              /* the pair window's seconds left */
    struct jk_usb usb;
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
    JK_CTL_STOP,                      /* out is empty: the caller stops the session, answers ok or err */
    JK_CTL_BLE_PAIR,                  /* out is empty: the caller opens a pair window (jph_pair), answers ok or err */
    JK_CTL_BLE_CLOSE                  /* out is empty: the caller ends the window and the phone's link, answers ok */
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
/* the session's beat at beat_ub is its bar's first (by quantum): with AUTO BAR locked, that flash shows the music's 1 */
int jk_led_bar_one(int64_t beat_ub, uint32_t quantum);
#define JK_LED_ONE_RGB 40, 18, 0       /* the locked 1's flash: amber (the other flashes keep the network's colour) */

/* the GROUP after this one, 9 wraps to 1 */
uint32_t jk_next_group(uint32_t group);
#endif
