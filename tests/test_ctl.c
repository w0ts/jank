/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the control line protocol, the settings, the LED and the button.
 * Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_core.h"

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
    CHECK(cfg.peer == 1 && cfg.transport == JK_TRANSPORT_LINK && cfg.quantum == 4 && cfg.offset_ms == 0);
    CHECK(run("set peer off") == JK_CTL_CFG_CHANGED && cfg.peer == 0);
    CHECK(run("set peer on") == JK_CTL_CFG_CHANGED && cfg.peer == 1);
    CHECK(run("set peer maybe") == JK_CTL_NONE);
    CHECK(run("set midi off") == JK_CTL_NONE);
    CHECK(run("set transport auto") == JK_CTL_CFG_CHANGED && cfg.transport == JK_TRANSPORT_AUTO);
    CHECK(run("set transport banana") == JK_CTL_NONE);
    CHECK(run("set quantum 8") == JK_CTL_CFG_CHANGED && cfg.quantum == 8);
    CHECK(run("set quantum 0") == JK_CTL_NONE && cfg.quantum == 8);
    CHECK(run("set quantum 17") == JK_CTL_NONE);
    CHECK(run("set offset -12") == JK_CTL_CFG_CHANGED && cfg.offset_ms == -12);
    CHECK(run("set offset 101") == JK_CTL_NONE && cfg.offset_ms == -12);
    CHECK(run("set offset -101") == JK_CTL_NONE);
    CHECK(run("get") == JK_CTL_NONE);
    CHECK(strstr(out, "peer=on\n") && !strstr(out, "midi=") && strstr(out, "transport=auto\n"));
    CHECK(strstr(out, "quantum=8\n") && strstr(out, "offset=-12\n"));
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

int main(void)
{
    test_defaults();
    test_get();
    test_set();
    test_set_music();
    test_network_settings();
    test_status_wifi();
    test_status_link();
    test_status();
    test_other_lines();
    test_scan_command();
    test_short_buffer();
    test_cfg_name();
    test_led();
    test_button();
    printf("ctl: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
