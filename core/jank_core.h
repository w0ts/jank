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
#include "jank_lat.h"

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
#define JK_WIN_MS 10000u              /* the window of the air statistics */
#define JK_WIN_MIN 10u                /* fewer frames than this in a window: the percentages are unknown */
#define JK_SEQ_JUMP 2000u             /* a sequence gap above this is a restart or a long silence: the count starts again */
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

/* What the peers' frames say about the air, over a tumbling window (jk_win_*): all peers together */
struct jk_win {
    uint32_t heard;                   /* frames counted (a retry of a frame already counted is not) */
    uint32_t dup;                     /* retries of a frame already counted */
    uint32_t missed;                  /* sequence numbers skipped by the senders: frames that never arrived */
    uint32_t retry;                   /* frames with the 802.11 Retry bit set, the dup ones too */
    int8_t rssi_min, rssi_max;        /* dBm over the frames counted; 0: none */
};

struct jk_airwin {
    uint8_t on;                       /* start is set */
    uint32_t start;                   /* ms */
    struct jk_win cur, done;          /* the window running, and the last one completed */
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
    struct jk_airwin win;
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

/* Air statistics from the peers' frames (MODE JANK only). The sequence number is the sender's 12-bit counter, so the gaps
 * between a peer's frames are the frames that never arrived. Our own sends are not measured: group frames are not acked. */
struct jk_airsum {
    int lost_pct;                     /* % of the peers' frames missing; -1: unknown */
    int retry_pct;                    /* % of the peers' frames that arrived with the Retry bit set; -1: unknown */
    int8_t rssi_min, rssi_max;        /* the weakest and the strongest frame of the last window, dBm; 0: none */
};
unsigned jk_seq_missed(uint32_t last, uint32_t seq);        /* frames skipped between two sequence numbers; 0 after a jump */
void jk_win_roll(struct jk_airwin *w, uint32_t now_ms);     /* completes the window when its 10 s are over */
void jk_win_heard(struct jk_airwin *w, uint32_t now_ms, unsigned missed, int retry, int8_t rssi);
void jk_win_dup(struct jk_airwin *w, uint32_t now_ms);
void jk_win_summary(const struct jk_airwin *w, uint32_t now_ms, struct jk_airsum *out);   /* of the last completed window */

/* settings */
/* AUTO BAR (core/jank_beatlink.h jbl_bar_mode): OFF; AT START (the default) the 1 is placed only at a (re)start, never
 * while playing; ALWAYS also mid-play. `set autobar on` is AT START, as before */
enum jk_autobar { JK_AUTOBAR_OFF = 0, JK_AUTOBAR_START = 1, JK_AUTOBAR_ALWAYS = 2 };
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
#define JK_BEAT_LATENCY_MIN_MS (-300)   /* negative: the stick hears the audio after the speakers do (player --xiao) */
#define JK_BEAT_LATENCY_MAX_MS 300
#define JK_BEAT_LATENCY_DEFAULT_MS 25
#define JK_LINE_GAIN_MIN_DB (-12)        /* LINE IN gain (core/jank_line.h JL_GAIN_*) */
#define JK_LINE_GAIN_MAX_DB 12
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
    uint8_t stored_pad;               /* the byte the compiler padded the release's struct with (its size stays 130). A release stored
                                         the Bluetooth setting here, a setting that no longer exists: store.c reads it as 0, whatever
                                         an old blob holds, and writes 0 */
    /* The beat-follow bench build's settings come after everything that is stored (store.c keeps only what is above
     * this line), so they are back at their defaults at every start. */
    uint8_t beat;                     /* 1: the beat tracker steers the Link session */
    int16_t beat_latency_ms;          /* the music is heard this long after its samples reach the stick (JK_BEAT_LATENCY_MIN_MS..MAX):
                                         the current source's value from `lat` below (stored there, since M2d) */
    uint16_t burn_us;                 /* BENCH: extra work the tracker does per frame, us (0..9000), to find where the core overloads.
                                         Not stored: 0 at every start */
    uint8_t autobar_picks;            /* counts `set autobar start|always|on`: picked again, it leaves MANUAL even when on */
    uint8_t follower;                 /* M2m A/B: 0 (default) the tuned follower (tuned-l16, M2j), 1 the robust one (all-d-f60,
                                         core/beat/jb_robust.h). Not stored: 0 again at every start */
    uint8_t skip;                     /* SKIP (core/beat/jb_skip.h, on the robust follower): 0 off, 1 ACT, 2 PAT. Not stored */
    uint8_t graded;                   /* the shown confidence is the graded one (core/beat/jb_graded.h, both followers): display only;
                                         0 off, 1 on (the default). Not stored: 1 again at every start. The gate never uses it */
    /* Stored apart from the blob above (store.c, key "usb1"), so the blob stays 130: USB host mode (core/jank_usbrole.h)
     * and AUTO BAR */
    uint8_t autobar;                  /* enum jk_autobar: the downbeat estimator puts the session's bar on the music's 1 */
    uint8_t usb;                      /* enum jur_usb: USB DEVICE / HOST */
    uint8_t source;                   /* enum jur_src: AUDIO > SOURCE AUTO / LINE / USB */
    /* Stored in the same small blob since M2d: */
    uint8_t usbmidi;                  /* 1: the MIDI clock also goes out over USB-MIDI (to the computer, or to hosted gear) */
    /* ... and since M2e: LOW CONFIDENCE (core/jank_conf.h) */
    uint8_t lowconf;                  /* enum jcf_lowconf: KEEP RUNNING (0) or STOP / START (1, the default, as the emulator) */
    uint8_t threshold;                /* percent 0..100: STOP / START gates the outputs under it (0: never) */
    /* ... and since M2i: the advanced beat follow's spectral rate (core/beat/jb_rate.h) */
    uint8_t rate;                     /* 1 (default): a pitch move read from the spectrum steers the follower; 0: the beats alone
                                         (CPU: the rate costs ~0.4 % of core 0 while it runs) */
    /* Stored in their own blob (store.c, key "lat1") since M2d: beat_latency per audio source (core/jank_lat.h) */
    struct jla lat;
    struct jla_key lat_key;           /* the source beat_latency_ms belongs to now (not stored: usbrole.c follows it) */
    int8_t line_gain_db;              /* LINE IN gain, JK_LINE_GAIN_MIN_DB..MAX (a bench setting: not stored, 0 at every start) */
};
void jk_cfg_defaults(struct jk_cfg *c);
int jk_cfg_set_name(struct jk_cfg *c, const char *name);     /* 0 ok, -1 empty, too long or not printable ASCII */
/* what store.c keeps: the release's blob (everything up to the bench settings) and, apart, usb and source */
#define JK_CFG_STORED offsetof(struct jk_cfg, beat)
/* the audio source the tracker hears now (usbrole.c): beat_latency_ms becomes that source's value (jank_lat.h). 1 changed */
int jk_lat_select(struct jk_cfg *c, const struct jla_key *k);
/* the current source's auto base (`set beat_latency`, the player's Auto latency), stored; its AUDIO SHIFT kept on top */
void jk_lat_set(struct jk_cfg *c, int ms);
/* the current source's AUDIO SHIFT (`set lat_trim`, Calibrate's, NUDGE MS; 0: the auto base alone), stored */
void jk_lat_trim(struct jk_cfg *c, int ms);
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

#define JK_CTL_OUT_MAX 2040u         /* the firmware's answer buffer (ctl.c): the longest status (1871) and `beat` with every counter at its largest (1963, M2m's profile) fit with the ok, tests/test_ctl.c; under usb.c's 2048 */
/* the control line protocol (SysEx on the USB MIDI port: jank_sysex.h). Each answer is lines of text ending in "ok" or "err <why>". */
/* the beat tracker's numbers, for the `beat` command (BENCH BUILD, see README) */
struct jk_beat {
    uint8_t on;                       /* the setting */
    uint8_t audio;                    /* the tracker's source is open (the computer's stream, a hosted device's) */
    uint8_t audio_live;               /* ... and it has sound: AUDIO mode (core/jank_live.h); 0: LINK mode */
    int8_t fold_exp;                  /* AUDIO mode's fold, 2^fold_exp (core/jank_fold.h) */
    uint8_t fold_manual;              /* the user chose it, not AUTO RANGE */
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
    uint8_t conf_pct;                 /* the confidence SHOWN, percent: the graded value (core/beat/jb_graded.h) when conf_graded, else how
                                         well the last 8 s of tracked beats fit the published grid (core/jank_conf.h, the player's
                                         live/confidence.py); 0 in silence */
    uint8_t conf_gate_pct;            /* the gate's own confidence, percent (what STOP / START, COAST, the LED and the notch judge); only
                                         read with conf_graded */
    uint8_t conf_graded;              /* conf_pct is the graded value, not the gate's */
    uint8_t act_pct;                  /* the network's beat activation peak over the last second, percent (M2d's conf=) */
    uint8_t conf_hold;                /* M2k: the confidence holds (no evidence): conf_pct is the shown value, easing toward
                                         the threshold (jank_conf.h jcf_show_step); the gate keeps the held one */
    uint8_t gated;                    /* LOW CONFIDENCE's gate: the confidence under the threshold (STOP / START) */
    uint8_t silent;                   /* 0.5 s under -60 dBFS: the outputs stopped too (jank_conf.h SILENCE) */
    uint8_t conf_open;                /* the confidence at the threshold + 5 (where the gate opens): a Start's disarm ends */
    uint32_t tempo_sets, phase_forces, starts, stops;   /* what the tracker did to the Link session */
    int32_t err_us;                   /* the session's beat grid against the tracked beat at the last step, before a correction */
    int32_t offset_us;                /* the clock the audio's samples are mapped to local time with: the offset's value */
    int32_t latency_us;               /* the setting, in us */
    uint8_t bar;                      /* the downbeat estimator: 0 off, 1 listening, 2 locked, 3 manual (jb_down.h) */
    int8_t bar_phi;                   /* locked: the count position of the 1; else -1 */
    int32_t bar_lead_x100;            /* its lead, score units x 100 */
    uint32_t bar_moves;               /* whole-beat moves of the session's bar */
    int8_t nudge_halves;              /* NUDGE BEAT: the user's nudges since the last reset, half beats (core/jank_beatlink.h) */
    /* the advanced beat follow (M2i, core/beat/jb_follow.h, jb_rate.h); conf_pct above is against the published lines */
    uint8_t follow;                   /* enum jb_follow_state: 0 stopped, 1 lock (ModeBpm's grid), 2 glide (the Kalman follower) */
    int32_t drift_ppm;                /* the glide's period change a beat, parts per million (+: slowing down) */
    uint32_t manoeuvres;              /* the glide's tempo moves taken from the beats */
    uint8_t rate_on;                  /* the spectral rate: the setting, and its RAM there (PSRAM) */
    uint8_t rate_active;              /* ... running (only while the glide runs) */
    int32_t rate_x10000;              /* the last ratio the follower took, x 10000 (10000: none yet) */
    uint32_t rate_events, rate_taken, rate_refused, rate_ignored;   /* steps proposed; taken, refused by the next beat,
                                                                       not judged (no trusted glide) */
    uint32_t rate_us;                 /* the rate's work over the last second, us (1000 = 0.1 % of core 0) */
    uint32_t rate_max_us;             /* its longest frame (a block's match) over the last second */
    /* the tuned follow (M2j, core/beat/jb_weak.h, beat-listener studies/tempo-changes tuned-l16) */
    uint8_t weak_on;                  /* the weak-signal hold runs (its RAM is PSRAM: without, M2i's follow) */
    uint32_t weak_holds;              /* steps the weak confidence kept the outputs running with the strict one under the
                                         threshold */
    uint32_t coasts;                  /* lines with no tracked beat the grid was corrected on from the activation */
    uint32_t weak_beats;              /* weak tracked beats taken as weak observations */
    uint32_t fast_jumps;              /* new grids (a section change) the follower jumped to at once */
    /* the robust follower (M2m, core/beat/jb_robust.h) and the cost counters for the A/B */
    uint8_t follower;                 /* the one running: 0 tuned, 1 robust (the setting, and its PSRAM there) */
    uint8_t skip, graded;             /* the ones running (follower robust only): SKIP 0 off, 1 ACT, 2 PAT; the graded confidence shown */
    uint8_t coast;                    /* the outputs run under the threshold (not a HOLD): the confidence is the robust follower's coast */
    uint16_t skip_resyncs, skip_act_resyncs, skip_gated;   /* since the tracker started (saturating): phase re-syncs, those from the activation, confidence steps with SKIP on and the gate closed */
    uint32_t conf_us;                 /* the confidence step's work (follow_conf) over the last second, us (10000 = 1 % of core 0);
                                         it is not in cpu_pct, which is the tracker's work (frames, beats, the gate's questions) */
    uint32_t robust_refresh_us, robust_refresh_max_us;    /* the agreement refresh (every 0.5 s): its total over the second, its longest */
    uint32_t robust_gate_us, robust_gate_max_us;          /* the lock/jump gate's questions (ratio check): total, longest */
    uint32_t robust_tick_us;          /* the robust tick without its refresh */
    /* the profile (M2m): the tracker task's cycles per stage over the last second, us; order: pull, meter, fe (without the
     * rate's tap), rate (the tap), net, dbn, down, act, feed, coast, rstep, conf, step, burn. idle: core 0's idle task's share
     * over the second, x10 percent (Wi-Fi, Link, USB and the tracker are the rest) */
    uint32_t prof_us[14];
    uint32_t prof_max_us[14];         /* the longest single call of each stage in the last second (fe, net, dbn: one frame), us */
    uint32_t hop_win_max_us, hop_over10;   /* the longest tracker pass (wall time, preemption included) in the last second, and how many
                                              passes took over 10 ms (a frame's worth of audio) */
    uint32_t uptime_s;
    uint32_t gap_at_ms[6], gap_len_ms[6];  /* the last audio gaps: when (ms since boot) and how long (ms) */
    uint32_t core0_idle_x10;
    uint32_t core1_busy_x10;          /* core 1: the MIDI clock interrupt's share over the last second, x10 percent (midi.c); core1_ok 0: not measured yet */
    uint8_t core1_ok;
    uint16_t burn_us;
    uint32_t robust_asked, robust_refused_memory, robust_refused_ratio, robust_coast_ticks, robust_prior_ticks;
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

/* the ABOUT screen's system numbers (firmware/main/uiscreen.c's sampler: the one source, the screen and the `about` reply read the same
 * values), with the unknowns the screen uses: -1 (JK_SYS_UNKNOWN), temp_c JK_TEMP_UNKNOWN, reset "" */
#define JK_SYS_UNKNOWN (-1)
#define JK_TEMP_UNKNOWN (-999)
struct jk_sys {
    uint8_t known;                    /* 0: the sampler has not filled this (every number is then unknown) */
    int32_t temp_c, heap_kb, psram_kb, heap_min_kb, uptime_s, flash_kb, app_used_kb, app_kb;
    int32_t clk_late_us, ui_loop_us, beat_loop_us;
    char reset[12];                   /* why the chip last reset, upper case ("POWERON", "PWR GLITCH"); "": unknown */
    const char *panic;                /* the last panic since power was lost, "<task>@0x4200abcd" (jk_panic_brief); NULL or "": none */
};
/* core 0's CPU load %, from its idle task's share in x10 percent (jk_beat core0_idle_x10; 0: not measured -> -1) */
int jk_cpu_pct(uint32_t core0_idle_x10);
/* core 1's load %, from the MIDI clock interrupt's share in x10 percent (jk_beat core1_busy_x10; ok 0: not measured -> -1) */
int jk_cpu1_pct(uint32_t core1_busy_x10, uint8_t ok);

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
    char board[12];                   /* the board the firmware is built for ("xiao", "lcd2"): `get` says it so the page picks the screen */
    struct jk_usb usb;
    uint8_t sleep, bright;            /* the menu's SCREEN OFF (0 NEVER, 1 30 S, 2 1 MIN, 3 5 MIN) and BRIGHTNESS (10..100 %), for `get` sleep= bright= */
    uint8_t lead;                     /* LINK TRANSPORT FOLLOW+LEAD (1) or FOLLOW (0): the menu's own preference (uiscreen.c uis_lead_get), for `get` lead= */
    struct jk_sys sys;                /* for `about` (ctl.c fills it from the ui task's sampler) */
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
    JK_CTL_FOLD,                      /* out is empty: the caller folds (jk_ctl_fold), answers fold=<f> ok or err link mode */
    JK_CTL_LEAD,                      /* `set lead on|off`, out is "ok\n": the caller stores the menu's FOLLOW+LEAD preference (jk_ctl_lead), cfg is untouched */
    JK_CTL_DISPLAY,                   /* `set sleep <v>` / `set bright <n>`, out is "ok\n": the caller stores the menu's SCREEN OFF / BRIGHTNESS preference (jk_ctl_display), cfg is untouched */
    JK_CTL_NUDGE                      /* out is empty: the caller nudges the beat (jk_ctl_nudge), answers nudge_beats=<b> ok or err */
};
enum jk_ctl_action jk_ctl_line(const char *line, struct jk_cfg *cfg, const struct jk_snapshot *snap, uint32_t now_ms, char *out, size_t cap);
#define JK_BPM_MIN_X100 2000u          /* the BPM the tempo command takes, hundredths: Link's own clamp, 20..999 */
#define JK_BPM_MAX_X100 99900u
/* "tempo <bpm>", BPM 20..999 with up to two decimals -> 0 and us a beat; -1 not one */
int jk_ctl_tempo(const char *line, int32_t *mpb);
/* "fold x2" / "fold /2" / "fold 1" (AUDIO mode's x2 / /2, core/jank_fold.h) -> 0 and dir +1 / -1 / 0; -1 not one */
int jk_ctl_fold(const char *line, int *dir);
/* "set lead on" / "set lead off" (LINK TRANSPORT FOLLOW+LEAD / FOLLOW) -> 0 and 1 / 0; -1 not one */
int jk_ctl_lead(const char *line, int *on);
/* "set sleep never|30s|1min|5min" -> 0, *bright 0 and *v 0..3; "set bright 10..100" -> 0, *bright 1 and *v the percent; -1 not one */
int jk_ctl_display(const char *line, int *bright, int *v);
/* NUDGE BEAT and NUDGE MS (AUDIO mode: the device's NO / YES held + UP/DOWN). "nudge beat <b>", b in half beats, signed,
 * 0.5 to 3 (+0.5, -0.5, 1, -1.5) -> 0 and the half beats (+-1..+-6); -1 not one. The caller nudges (core/jank_beatlink.h
 * jbl_nudge) and answers `nudge_beats=<total>`, the estimator's view before it, ok; or err link mode, no beat, at the limit */
int jk_ctl_nudge(const char *line, int *halves);
/* "nudge ms <n>": the AUDIO SHIFT (M2h), the current audio source's shift += n (1..JK_NUDGE_MS_STEP_MAX either way) on top
 * of its auto base, which it never changes; the sum kept within JK_BEAT_LATENCY_MIN_MS..MAX (jank_lat.h; stored, saved once
 * stopped). jk_ctl_line answers beat_latency=<sum> lat_auto=<base> lat_trim=<shift> ok (a change: JK_CTL_CFG_CHANGED), or
 * err link mode. jk_nudge_ms is the same for the device's YES + UP/DOWN: 1 when it moved */
#define JK_NUDGE_MS_STEP_MAX 10
int jk_nudge_ms(struct jk_cfg *c, int ms);
/* half beats as beats: 1 -> "0.5", -3 -> "-1.5", 4 -> "2", 0 -> "0" (out holds 8) */
const char *jk_half_beats(int halves, char *out);

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
