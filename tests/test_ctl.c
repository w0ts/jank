/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the control line protocol, the settings, the LED and the button.
 * Exit status: the number of failed checks. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "jank_core.h"
#include "jank_hold.h"
#include "jank_usbrole.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const uint8_t HOST[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
static struct jk_cfg cfg;
static struct jk_snapshot snap;
static char out[1024];

static void setup(void)
{
    jk_cfg_defaults(&cfg);
    memset(&snap, 0, sizeof snap);
    CHECK(jk_init(&snap.b, HOST, cfg.group, cfg.filter) == 0);
    snap.usb_up = 1;
    snap.uptime_s = 12;
}

static enum jk_ctl_action run(const char *line)
{
    return jk_ctl_line(line, &cfg, &snap, 1000, out, sizeof out);
}

static int ends(const char *s, const char *tail)
{
    size_t a = strlen(s), b = strlen(tail);
    return a >= b && !strcmp(s + a - b, tail);
}

static void test_defaults(void)
{
    setup();
    CHECK(cfg.group == 1 && cfg.filter == JK_FILTER_LINK && !strcmp(cfg.name, "jank"));
}

static void test_get(void)
{
    setup();
    CHECK(run("get") == JK_CTL_NONE);
    CHECK(strstr(out, "group=1\n") && strstr(out, "channel=1\n"));
    CHECK(strstr(out, "name=jank\n") && strstr(out, "filter=link\n"));
    CHECK(strstr(out, "mac=02:11:22:33:44:55\n"));
    CHECK(strstr(out, "version=" JK_VERSION "\n"));
    CHECK(ends(out, "ok\n"));
}

static void test_set(void)
{
    setup();
    CHECK(run("set group 4") == JK_CTL_CFG_CHANGED && cfg.group == 4 && ends(out, "ok\n"));
    CHECK(run("set group 0") == JK_CTL_NONE && cfg.group == 4 && strstr(out, "err ") == out);
    CHECK(run("set group 10") == JK_CTL_NONE && cfg.group == 4);
    CHECK(run("set group x") == JK_CTL_NONE && cfg.group == 4);
    CHECK(run("set group 4x") == JK_CTL_NONE && cfg.group == 4);
    CHECK(run("set group") == JK_CTL_NONE);
    CHECK(run("set name front of house") == JK_CTL_CFG_CHANGED && !strcmp(cfg.name, "front of house"));
    CHECK(run("set name ") == JK_CTL_NONE && !strcmp(cfg.name, "front of house"));
    CHECK(run("set name 012345678901234567890123") == JK_CTL_NONE);     /* 24 characters */
    CHECK(run("set name 01234567890123456789012") == JK_CTL_CFG_CHANGED);   /* 23 */
    CHECK(run("set name caf\xC3\xA9") == JK_CTL_NONE);                   /* printable ASCII only */
    CHECK(run("set filter all") == JK_CTL_CFG_CHANGED && cfg.filter == JK_FILTER_ALL);
    CHECK(run("set filter link") == JK_CTL_CFG_CHANGED && cfg.filter == JK_FILTER_LINK);
    CHECK(run("set filter none") == JK_CTL_NONE && cfg.filter == JK_FILTER_LINK);
    CHECK(run("set colour red") == JK_CTL_NONE && strstr(out, "err ") == out);
}

static void test_set_music(void)
{
    setup();
    CHECK(cfg.peer == 1 && cfg.transport == JK_TRANSPORT_LINK && cfg.quantum == 4 && cfg.offset_10us == 0);
    CHECK(run("set peer off") == JK_CTL_CFG_CHANGED && cfg.peer == 0);
    CHECK(run("set peer on") == JK_CTL_CFG_CHANGED && cfg.peer == 1);
    CHECK(run("set peer maybe") == JK_CTL_NONE);
    CHECK(run("set midi off") == JK_CTL_NONE);
    CHECK(run("set transport auto") == JK_CTL_CFG_CHANGED && cfg.transport == JK_TRANSPORT_AUTO);
    CHECK(run("set transport banana") == JK_CTL_NONE);
    CHECK(run("set quantum 8") == JK_CTL_CFG_CHANGED && cfg.quantum == 8);
    CHECK(run("set quantum 0") == JK_CTL_NONE && cfg.quantum == 8);
    CHECK(run("set quantum 17") == JK_CTL_NONE);
    CHECK(run("set offset -12") == JK_CTL_CFG_CHANGED && cfg.offset_10us == -1200);
    CHECK(run("set offset 101") == JK_CTL_NONE && cfg.offset_10us == -1200);
    CHECK(run("set offset 1.27") == JK_CTL_CFG_CHANGED && cfg.offset_10us == 127);   /* ms, two decimals: 10 us steps */
    CHECK(run("set offset -0.5") == JK_CTL_CFG_CHANGED && cfg.offset_10us == -50);
    CHECK(run("set offset 100.00") == JK_CTL_CFG_CHANGED && cfg.offset_10us == 10000);
    CHECK(run("set offset -100.01") == JK_CTL_NONE && cfg.offset_10us == 10000);
    CHECK(run("set offset 1.275") == JK_CTL_NONE && run("set offset 1.") == JK_CTL_NONE && run("set offset -") == JK_CTL_NONE);
    CHECK(run("set offset 1.27") == JK_CTL_CFG_CHANGED && run("get") == JK_CTL_NONE && strstr(out, "offset=1.27\n"));
    CHECK(run("set offset -0.05") == JK_CTL_CFG_CHANGED && run("get") == JK_CTL_NONE && strstr(out, "offset=-0.05\n"));
    CHECK(run("set offset 0") == JK_CTL_CFG_CHANGED && run("get") == JK_CTL_NONE && strstr(out, "offset=0.00\n"));
    CHECK(run("set offset -101") == JK_CTL_NONE);
    CHECK(run("get") == JK_CTL_NONE);
    CHECK(strstr(out, "peer=on\n") && !strstr(out, "midi=") && strstr(out, "transport=auto\n"));
    CHECK(strstr(out, "quantum=8\n") && strstr(out, "offset=0.00\n"));
}

static void test_network_settings(void)
{
    char long_ssid[50], long_pass[80];
    setup();
    CHECK(run("get") == JK_CTL_NONE);
    CHECK(strstr(out, "net=jank\n") && strstr(out, "ssid=\n") && strstr(out, "pass=none\n"));
    CHECK(run("set ssid My Home Net") == JK_CTL_CFG_CHANGED && !strcmp(cfg.ssid, "My Home Net"));
    CHECK(run("set pass correct horse battery") == JK_CTL_CFG_CHANGED && !strcmp(cfg.pass, "correct horse battery"));
    CHECK(run("set net wifi") == JK_CTL_CFG_CHANGED && cfg.net == JK_NET_WIFI);
    CHECK(run("get") == JK_CTL_NONE);
    CHECK(strstr(out, "net=wifi\n") && strstr(out, "ssid=My Home Net\n") && strstr(out, "pass=set\n"));
    CHECK(!strstr(out, "horse"));                         /* the password never comes back */
    CHECK(run("status") == JK_CTL_NONE && !strstr(out, "horse"));
    CHECK(run("set pass short") == JK_CTL_NONE && !strcmp(cfg.pass, "correct horse battery"));
    memset(long_pass, 'x', sizeof long_pass);
    strcpy(long_pass, "set pass ");
    memset(long_pass + 9, 'x', 63);
    long_pass[72] = 0;
    CHECK(run(long_pass) == JK_CTL_CFG_CHANGED && strlen(cfg.pass) == 63);
    strcpy(long_ssid, "set ssid ");
    memset(long_ssid + 9, 'y', 33);
    long_ssid[42] = 0;
    CHECK(run(long_ssid) == JK_CTL_NONE);
    CHECK(run("set net cable") == JK_CTL_NONE && cfg.net == JK_NET_WIFI);
    CHECK(run("set pass") == JK_CTL_CFG_CHANGED && cfg.pass[0] == 0);
    CHECK(run("set net jank") == JK_CTL_CFG_CHANGED && cfg.net == JK_NET_JANK);
}

static void test_status_wifi(void)
{
    setup();
    snap.wifi_up = 1;
    snap.wifi_ip = 0xC0A80164u;
    snap.wifi_rssi = -52;
    snap.wifi_channel = 6;
    CHECK(run("status") == JK_CTL_NONE);
    CHECK(strstr(out, "wifi=1\n") && strstr(out, "wifi_ip=192.168.1.100\n"));
    CHECK(strstr(out, "wifi_rssi=-52\n") && strstr(out, "wifi_channel=6\n"));
}

static void test_status_link(void)
{
    setup();
    snap.link_on = 1;
    snap.link_ip = 0xA9FE0102u;
    snap.link_peers = 2;
    snap.mpb = 500000;
    snap.beat_ub = 12500000;
    snap.playing = 1;
    snap.midi_ticks = 300;
    CHECK(run("status") == JK_CTL_NONE);
    CHECK(strstr(out, "link=1\n") && strstr(out, "link_ip=169.254.1.2\n") && strstr(out, "link_peers=2\n"));
    CHECK(strstr(out, "bpm=120.00\n") && strstr(out, "beat=12.500\n") && strstr(out, "playing=1\n"));
    CHECK(strstr(out, "midi_ticks=300\n"));
    CHECK(strstr(out, "settled=0\narmed=0\n") && !strstr(out, "start="));
    snap.settled = 1;
    snap.armed = 1;
    snap.start_ub = 16000000;
    CHECK(run("status") == JK_CTL_NONE);
    CHECK(strstr(out, "settled=1\narmed=1\nstart=16.000\n"));       /* the bar line the transport starts on */
    snap.armed = 0;
    snap.beat_ub = -1500000;
    snap.mpb = 400000;
    CHECK(run("status") == JK_CTL_NONE);
    CHECK(strstr(out, "bpm=150.00\n") && strstr(out, "beat=-1.500\n"));
    snap.mpb = 0;
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "bpm=0.00\n"));
}

static void test_status(void)
{
    setup();
    snap.b.st.air_in = 7;
    snap.b.st.usb_in = 3;
    snap.tx_fail = 2;
    snap.lat_air_max_us = 900;
    memcpy(snap.b.peer[0].mac, "\x8C\xAA\xBB\xCC\xDD\xEE", 6);
    snap.b.peer[0].used = 1;
    snap.b.peer[0].rssi = -44;
    snap.b.peer[0].t_heard = 900;
    snap.b.peer[0].frames = 5;
    CHECK(run("status") == JK_CTL_NONE);
    CHECK(strstr(out, "usb=1\n") && strstr(out, "uptime=12\n"));
    CHECK(strstr(out, "air_in=7\n") && strstr(out, "usb_in=3\n") && strstr(out, "tx_fail=2\n"));
    CHECK(strstr(out, "peers=1\n") && strstr(out, "rssi=-44\n"));
    CHECK(strstr(out, "lat_air_max_us=900\n"));
    CHECK(strstr(out, "peer=8c:aa:bb:cc:dd:ee,-44,100,5\n"));
    CHECK(ends(out, "ok\n"));
    /* a unit gone is not listed */
    snap.b.peer[0].t_heard = 0;
    CHECK(jk_ctl_line("status", &cfg, &snap, 10000, out, sizeof out) == JK_CTL_NONE);
    CHECK(!strstr(out, "peer=") && strstr(out, "peers=0\n"));
}

static void test_other_lines(void)
{
    setup();
    CHECK(run("") == JK_CTL_NONE && out[0] == 0);       /* a blank line: nothing at all */
    CHECK(run("   ") == JK_CTL_NONE && out[0] == 0);
    CHECK(run("fly") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("reboot") == JK_CTL_REBOOT && ends(out, "ok\n"));
    CHECK(run("bootloader") == JK_CTL_BOOTLOADER && ends(out, "ok\n"));
    CHECK(run("help") == JK_CTL_NONE && ends(out, "ok\n"));
    CHECK(run("  get  ") == JK_CTL_NONE && ends(out, "ok\n"));   /* spaces around, a CR from a terminal */
    CHECK(run("get\r") == JK_CTL_NONE && ends(out, "ok\n"));
}

static void test_session_commands(void)
{
    int32_t mpb = 0;
    setup();
    CHECK(run("tempo 120") == JK_CTL_TEMPO && out[0] == 0);       /* the caller answers: ok, or err no session */
    CHECK(jk_ctl_tempo("tempo 120", &mpb) == 0 && mpb == 500000);
    CHECK(jk_ctl_tempo("  tempo 128.5 ", &mpb) == 0 && mpb == 466926);
    CHECK(jk_ctl_tempo("tempo 20", &mpb) == 0 && mpb == 3000000);
    CHECK(jk_ctl_tempo("tempo 999", &mpb) == 0 && mpb == 60060);
    CHECK(run("tempo 19.99") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("tempo 1000") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("tempo 120.123") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("tempo fast") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("tempo") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(jk_ctl_tempo("tempo -5", &mpb) == -1);
    CHECK(run("play") == JK_CTL_PLAY && out[0] == 0);
    CHECK(run("stop") == JK_CTL_STOP && out[0] == 0);
    CHECK(run("play now") == JK_CTL_NONE && strstr(out, "err ") == out);
}

static void test_scan_command(void)
{
    setup();
    CHECK(run("scan") == JK_CTL_SCAN && out[0] == 0);    /* the caller scans and answers */
    CHECK(run("scan now") == JK_CTL_NONE && strstr(out, "err ") == out);
}

static void test_short_buffer(void)
{
    char small[32];
    setup();
    memset(small, 'x', sizeof small);
    CHECK(jk_ctl_line("status", &cfg, &snap, 1000, small, 16) == JK_CTL_NONE);
    CHECK(memchr(small, 0, 16) != NULL);                /* terminated inside */
    CHECK(small[20] == 'x');                            /* nothing past it */
}

static void test_cfg_name(void)
{
    setup();
    CHECK(jk_cfg_set_name(&cfg, "") == -1);
    CHECK(jk_cfg_set_name(&cfg, "a\tb") == -1);
    CHECK(jk_cfg_set_name(&cfg, "ok name") == 0 && !strcmp(cfg.name, "ok name"));
}

static void test_led(void)
{
    enum { BEAT = 1000000 };           /* micro-beats */
    int32_t mpb = 500000;              /* 120 BPM: a flash of JK_BEAT_FLASH_MS is 0.16 beat */
    int64_t flash = (int64_t)JK_BEAT_FLASH_MS * 1000 * BEAT / mpb;
    /* JANK: off, lit on each beat */
    CHECK(jk_led_lit(JK_NET_JANK, 1, 0, mpb, 4) == 1);
    CHECK(jk_led_lit(JK_NET_JANK, 1, BEAT + flash - 1, mpb, 4) == 1);
    CHECK(jk_led_lit(JK_NET_JANK, 1, BEAT + flash, mpb, 4) == 0);
    CHECK(jk_led_lit(JK_NET_JANK, 1, 7 * (int64_t)BEAT + 10, mpb, 4) == 1);
    CHECK(jk_led_lit(JK_NET_JANK, 1, -BEAT + 10, mpb, 4) == 1);           /* before the session's beat 0 too */
    CHECK(jk_led_bar_one(0, 4) && jk_led_bar_one(4 * (int64_t)BEAT + 10, 4) && !jk_led_bar_one(BEAT + 10, 4));
    CHECK(jk_led_bar_one(-4 * (int64_t)BEAT + 10, 4) && !jk_led_bar_one(-BEAT + 10, 4) && jk_led_bar_one(-3 * (int64_t)BEAT, 3));
    CHECK(jk_led_lit(JK_NET_JANK, 1, -10, mpb, 4) == 0);
    /* station: the other way round, lit and off on each beat */
    CHECK(jk_led_lit(JK_NET_WIFI, 1, 0, mpb, 4) == 0);
    CHECK(jk_led_lit(JK_NET_WIFI, 1, BEAT / 2, mpb, 4) == 1);
    /* no session: the mode's rest */
    CHECK(jk_led_lit(JK_NET_JANK, 0, 0, mpb, 4) == 0 && jk_led_lit(JK_NET_WIFI, 0, 0, mpb, 4) == 1);
    CHECK(jk_led_lit(JK_NET_JANK, 1, 0, 0, 4) == 0);
    /* the bar's first beat flashes longer: beat 0, 4, 8 of a 4-beat bar, and -4 before the session's 0 */
    int64_t bar = (int64_t)JK_BAR_FLASH_MS * 1000 * BEAT / mpb;
    CHECK(JK_BAR_FLASH_MS > JK_BEAT_FLASH_MS);
    CHECK(jk_led_lit(JK_NET_JANK, 1, bar - 1, mpb, 4) == 1 && jk_led_lit(JK_NET_JANK, 1, bar, mpb, 4) == 0);
    CHECK(jk_led_lit(JK_NET_JANK, 1, 8 * (int64_t)BEAT + bar - 1, mpb, 4) == 1);
    CHECK(jk_led_lit(JK_NET_JANK, 1, -4 * (int64_t)BEAT + bar - 1, mpb, 4) == 1);
    CHECK(jk_led_lit(JK_NET_JANK, 1, 1 * (int64_t)BEAT + bar - 1, mpb, 4) == 0);    /* beat 2: the short flash */
    CHECK(jk_led_lit(JK_NET_JANK, 1, -3 * (int64_t)BEAT + bar - 1, mpb, 4) == 0);
    CHECK(jk_led_lit(JK_NET_JANK, 1, 3 * (int64_t)BEAT + bar - 1, mpb, 3) == 1);    /* a 3-beat bar */
    CHECK(jk_led_lit(JK_NET_WIFI, 1, bar - 1, mpb, 4) == 0);                        /* the network's: off longer */
    /* fast: 300 BPM, a beat is 200 ms, the downbeat's 150 ms is cut to a third of it */
    CHECK(jk_led_lit(JK_NET_JANK, 1, BEAT / 3 - 1, 200000, 4) == 1 && jk_led_lit(JK_NET_JANK, 1, BEAT / 3, 200000, 4) == 0);
    /* a slow session's flash is still JK_BEAT_FLASH_MS long */
    CHECK(jk_led_lit(JK_NET_JANK, 1, (int64_t)JK_BEAT_FLASH_MS * 1000 * BEAT / 2000000 - 1, 2000000, 4) == 1);
}

static void test_button(void)
{
    CHECK(jk_next_group(1) == 2 && jk_next_group(8) == 9 && jk_next_group(9) == 1);
    CHECK(jk_next_group(0) == 1 && jk_next_group(200) == 1);
}

static void test_beat(void)
{
    setup();
    CHECK(cfg.beat == 1 && cfg.beat_latency_ms == JK_BEAT_LATENCY_DEFAULT_MS);
    CHECK(run("set beat_latency 40") == JK_CTL_CFG_CHANGED && cfg.beat_latency_ms == 40);
    CHECK(run("set beat_latency 301") == JK_CTL_NONE && strstr(out, "err") && cfg.beat_latency_ms == 40);
    CHECK(run("set beat_latency -1") == JK_CTL_CFG_CHANGED && cfg.beat_latency_ms == -1);      /* the stick may hear it after the speakers */
    CHECK(run("set beat_latency -301") == JK_CTL_NONE && strstr(out, "err") && cfg.beat_latency_ms == -1);
    CHECK(run("set beat_latency 40") == JK_CTL_CFG_CHANGED && cfg.beat_latency_ms == 40);
    CHECK(run("set beat off") == JK_CTL_CFG_CHANGED && cfg.beat == 0);
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "beat=off\nbeat_latency=40\n"));
    snap.beat = (struct jk_beat){.on = 1, .audio = 1, .mode = 1, .bpm_x100 = 12345, .heap_min = 90000, .cpu_pct_x10 = 253, .packets = 77};
    CHECK(run("beat") == JK_CTL_NONE);
    CHECK(strstr(out, "mode=steady\n") && strstr(out, "bpm=123.45\n") && strstr(out, "heap_min=90000\n"));
    CHECK(strstr(out, "cpu_pct=25.3\n") && strstr(out, "packets=77\n") && strstr(out, "madmom BENCH ONLY") && ends(out, "ok\n"));
    /* the source open and sounding (AUDIO mode), the meter, the confidence: in beat and status alike */
    snap.beat = (struct jk_beat){.on = 1, .audio = 1, .audio_live = 1, .level_db = -21, .level_min_db = -30, .level_max_db = -14,
                                 .peak_db = -6, .conf_pct = 73};
    snap.hold = 1;
    snap.outputs = JHO_STOPPED;
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "audio=1\naudio_live=1\nrms_db=-21\nrms_min_db=-30\nrms_max_db=-14\npeak_db=-6\nconf=73\n"));
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "audio=1\naudio_live=1\nhold=1\noutputs=stopped\nrms_db=-21\nrms_min_db=-30\n"));
    CHECK(strstr(out, "rms_max_db=-14\npeak_db=-6\nconf=73\n") && ends(out, "ok\n"));
    snap.beat = (struct jk_beat){.on = 1, .audio = 1, .level_db = -99, .level_min_db = -99, .level_max_db = -99, .peak_db = -99};
    snap.hold = 0;
    snap.outputs = JHO_ARMED;                               /* a stream of zeros: open, no sound, LINK mode */
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "audio=1\naudio_live=0\nhold=0\noutputs=armed\nrms_db=-99\n"));
}

/* AUTO BAR: the setting (on by default; `on` again counts as picked again) and the estimator in status and beat */
static void test_autobar(void)
{
    setup();
    CHECK(cfg.autobar == 1 && cfg.autobar_picks == 0);
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "autobar=on\n"));
    CHECK(run("set autobar off") == JK_CTL_CFG_CHANGED && cfg.autobar == 0 && cfg.autobar_picks == 0);
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "autobar=off\n"));
    CHECK(run("set autobar on") == JK_CTL_CFG_CHANGED && cfg.autobar == 1 && cfg.autobar_picks == 1);
    CHECK(run("set autobar on") == JK_CTL_CFG_CHANGED && cfg.autobar_picks == 2);      /* picked again */
    CHECK(run("set autobar yes") == JK_CTL_NONE && strstr(out, "err") && cfg.autobar == 1);
    snap.beat = (struct jk_beat){.bar = 2, .bar_phi = 3, .bar_lead_x100 = 241, .bar_moves = 4};
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "bar=locked\nbar_phi=3\nbar_lead=2.41\n") && ends(out, "ok\n"));
    snap.beat = (struct jk_beat){.bar = 1, .bar_phi = -1, .bar_lead_x100 = -5};
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "bar=listening\nbar_phi=-1\nbar_lead=-0.05\n"));
    snap.beat.bar = 3;
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "bar=manual\n"));
    snap.beat = (struct jk_beat){.bar = 0, .bar_phi = -1, .bar_moves = 7};
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "bar=off\n") && strstr(out, "bar_moves=7\n") && ends(out, "ok\n"));
}

/* settings saves: never a flash write while playing, unless a restart follows; what counts as a stored change */
static void test_save(void)
{
    struct jk_cfg a, b;
    uint8_t pending = 0;
    jk_cfg_defaults(&a);
    b = a;
    CHECK(!jk_cfg_stored_differs(&a, &b));
    b.autobar = 0;                                      /* a bench setting: not stored */
    b.beat_latency_ms = 10;
    CHECK(!jk_cfg_stored_differs(&a, &b));
    b.ble = JK_BLE_AUTO;                                /* the release blob's pad byte: stored */
    CHECK(jk_cfg_stored_differs(&a, &b));
    b = a;
    b.source = JUR_SRC_USB;                             /* stored apart ("usb1") */
    CHECK(jk_cfg_stored_differs(&a, &b));
    b = a;
    b.group = 3;
    CHECK(jk_cfg_stored_differs(&a, &b));
    /* stopped: at once */
    CHECK(jk_save_on_change(&pending, 1, 0, 0) == JK_SAVE_NOW && !pending);
    CHECK(jk_save_on_change(&pending, 0, 0, 0) == JK_SAVE_NONE && !pending);
    /* playing: later, and not due until stopped */
    CHECK(jk_save_on_change(&pending, 1, 1, 0) == JK_SAVE_LATER && pending);
    CHECK(!jk_save_due(&pending, 1) && pending);
    CHECK(jk_save_on_change(&pending, 0, 1, 0) == JK_SAVE_LATER && pending);    /* an unstored change: still pending */
    CHECK(jk_save_due(&pending, 0) && !pending);
    CHECK(!jk_save_due(&pending, 0));                                           /* once */
    /* playing, but a restart follows (USB DEVICE / HOST): now, before the restart */
    CHECK(jk_save_on_change(&pending, 1, 1, 1) == JK_SAVE_NOW && !pending);
    /* a pending save and a change while stopped: one save covers both */
    CHECK(jk_save_on_change(&pending, 1, 1, 0) == JK_SAVE_LATER);
    CHECK(jk_save_on_change(&pending, 0, 0, 0) == JK_SAVE_NOW && !pending);
}

/* `diag intr`: esp_intr_dump's text (intr_alloc.c's format) cut to what is in use */
static void test_intr_summary(void)
{
    static const char dump[] = "CPU 0 interrupt status:\n"
                               " Int  Level  Type   Status\n"
                               "  0     1    Level  Reserved\n"
                               "  2     1    Level  Used: USB\n"
                               "  3     1    Level  Shared: SYSTIMER_TARGET0 TG1_T0_LEVEL \n"
                               "  4     1    Level  Free\n"
                               "  6     1    Level  CPU-internal\n"
                               " 17     1    Level  Reserved (run-time)\n"
                               "CPU 1 interrupt status:\n"
                               " Int  Level  Type   Status\n"
                               "  0     1    Level  Reserved\n"
                               " 27     3    Level  Used: TG0_T0_LEVEL\n"
                               " 31     5    Level  Free (not general-use)\n"
                               "Interrupts available for general use: 15\n"
                               "Shared interrupts: 1";
    char out[256];
    size_t n = jk_intr_summary(dump, out, sizeof out);
    CHECK(n == strlen(out));
    CHECK(!strcmp(out, "c0 2 L1 Used: USB\nc0 3 L1 Shared: SYSTIMER_TARGET0 TG1_T0_LEVEL\nc0 17 L1 Reserved (run-time)\n"
                       "c1 27 L3 Used: TG0_T0_LEVEL\nok\n"));
    CHECK(jk_intr_summary(dump, out, 40) == 0);                 /* does not fit: no half answer passed as whole */
    CHECK(jk_intr_summary("", out, sizeof out) == 3 && !strcmp(out, "ok\n"));
}

/* USB DEVICE / HOST and AUDIO > SOURCE; the port's lines in status */
static void test_usb(void)
{
    setup();
    CHECK(cfg.usb == JUR_USB_DEVICE && cfg.source == JUR_SRC_AUTO);
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "usb=device\n") && strstr(out, "source=auto\n"));
    CHECK(run("set usb host") == JK_CTL_CFG_CHANGED && cfg.usb == JUR_USB_HOST && ends(out, "ok\n"));
    CHECK(run("set usb otg") == JK_CTL_NONE && cfg.usb == JUR_USB_HOST && strstr(out, "err ") == out);
    CHECK(run("set usb device") == JK_CTL_CFG_CHANGED && cfg.usb == JUR_USB_DEVICE);
    CHECK(run("set source line") == JK_CTL_CFG_CHANGED && cfg.source == JUR_SRC_LINE);
    CHECK(run("set source usb") == JK_CTL_CFG_CHANGED && cfg.source == JUR_SRC_USB);
    CHECK(run("set source mic") == JK_CTL_NONE && cfg.source == JUR_SRC_USB);
    {
        struct jk_cfg before = cfg;                     /* LINK: a change, so stored by the deferred-save rule */
        CHECK(run("set source link") == JK_CTL_CFG_CHANGED && cfg.source == JUR_SRC_LINK && jk_cfg_stored_differs(&before, &cfg));
        CHECK(run("get") == JK_CTL_NONE && strstr(out, "source=link\n"));
        CHECK(run("set source usb") == JK_CTL_CFG_CHANGED);
    }
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "usb=device\n") && strstr(out, "source=usb\n"));
    /* the XIAO as a device: no CC chip */
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "usb_role=device\nusb_cc=nochip\n") && !strstr(out, "usb_vbus_out"));
    CHECK(strstr(out, "audio_source=none\n") && !strstr(out, "usb_dev=") && !strstr(out, "usb_audio=") && ends(out, "ok\n"));
    /* the board as a host with a synth on a hub: its MIDI and its audio */
    snap.usb.stack = JUR_STACK_HOST;
    snap.usb.has_port = 1;
    snap.usb.cc = JUR_CC_DEVICE;
    snap.usb.cc_dir = 2;
    snap.usb.vbus_on = 1;
    snap.usb.audio = JUR_AUDIO_USB;
    snap.usb.ndev = 2;
    strcpy(snap.usb.dev[0].name, "OP-Z");
    snap.usb.dev[0].midi = 1;
    snap.usb.dev[0].audio = 1;
    snap.usb.dev[1].midi = 1;
    snap.usb.audio_rate = 48000;
    snap.usb.audio_channels = 2;
    snap.usb.audio_bits = 24;
    snap.usb.midi_dropped = 3;
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "usb_role=host\nusb_cc=device\nusb_cc_dir=2\nusb_vbus_out=1\nusb_vbus=0\n"));
    CHECK(strstr(out, "audio_source=usb\n") && strstr(out, "usb_dev=OP-Z,midi+audio\n") && strstr(out, "usb_dev=?,midi\n"));
    CHECK(strstr(out, "usb_audio=48000/2/24\n") && strstr(out, "usb_midi_dropped=3\n") && ends(out, "ok\n"));
}

/* the longest status (a host with four devices, eight peers, every number at its longest) fits the firmware's answer
 * buffer (JK_CTL_OUT_MAX, ctl.c) with its closing ok */
static void test_status_size(void)
{
    static char big[4096];
    setup();
    memset(&snap.b.st, 0xFF, sizeof snap.b.st);
    snap.usb_up = 1;
    snap.uptime_s = snap.tx_fail = snap.queue_full = snap.scan_lost = 4294967295u;
    snap.lat_air_us = snap.lat_air_max_us = snap.lat_usb_us = snap.lat_usb_max_us = 4294967295u;
    snap.wifi_up = 1;
    snap.wifi_ip = snap.link_ip = 0xFFFFFFFFu;
    snap.wifi_rssi = -128;
    snap.wifi_channel = 255;
    snap.link_peers = 4294967295u;
    snap.mpb = 1;
    snap.beat_ub = snap.start_ub = INT64_MIN + 1;
    snap.playing = snap.settled = 1;
    snap.link_on = 1;
    snap.armed = 1;
    snap.usb.stack = JUR_STACK_HOST;
    snap.usb.has_port = 1;
    snap.usb.ndev = JK_USB_DEVS;
    snap.usb.audio_rate = 48000;
    for (uint32_t i = 0; i < JK_USB_DEVS; i++) {
        memset(snap.usb.dev[i].name, 'n', JK_USB_NAME_MAX);
        snap.usb.dev[i].midi = snap.usb.dev[i].audio = 1;
    }
    for (uint32_t i = 0; i < JK_PEERS; i++) {
        snap.b.peer[i].used = 1;
        snap.b.peer[i].rssi = -100;
        snap.b.peer[i].t_heard = 999;
        snap.b.peer[i].frames = 4000000000u;
    }
    /* the merged build's other status lines at their longest: AUTO BAR and Bluetooth */
    snap.beat.bar = 3;
    snap.beat.bar_phi = -128;
    snap.beat.bar_lead_x100 = -2147483647;
    snap.ble_built = 1;
    snap.ble_state = 2;
    snap.ble_left_s = 4294967295u;
    snap.usb.audio_packets = snap.usb.audio_errors = snap.usb.midi_dropped = 4294967295u;
    snap.midi_ticks = snap.midi_resync = 4294967295u;
    snap.beat.audio = snap.beat.audio_live = snap.hold = 1;           /* the audio lines at their longest */
    snap.outputs = JHO_RUNNING;
    snap.beat.level_db = snap.beat.level_min_db = snap.beat.level_max_db = snap.beat.peak_db = -99;
    snap.beat.conf_pct = 100;
    CHECK(jk_ctl_line("status", &cfg, &snap, 1000, big, sizeof big) == JK_CTL_NONE);
    printf("longest status: %zu bytes\n", strlen(big));
    CHECK(strstr(big, "bar=manual\n") && strstr(big, "ble_state=connected\n") && strstr(big, "usb_role=host\n"));
    CHECK(strlen(big) < JK_CTL_OUT_MAX && ends(big, "ok\n"));
}

int main(void)
{
    test_defaults();
    test_beat();
    test_autobar();
    test_get();
    test_set();
    test_set_music();
    test_network_settings();
    test_status_wifi();
    test_status_link();
    test_status();
    test_other_lines();
    test_session_commands();
    test_scan_command();
    test_short_buffer();
    test_cfg_name();
    test_led();
    test_button();
    test_usb();
    test_status_size();
    test_save();
    test_intr_summary();
    printf("ctl: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
