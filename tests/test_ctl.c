/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the control line protocol, the settings, the LED and the button.
 * Exit status: the number of failed checks. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "jank_core.h"
#include "jank_conf.h"
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
static char out[JK_CTL_OUT_MAX];

static void setup(void)
{
    jk_cfg_defaults(&cfg);
    memset(&snap, 0, sizeof snap);
    CHECK(jk_init(&snap.b, HOST, cfg.group, cfg.filter) == 0);
    snap.usb_up = 1;
    snap.uptime_s = 12;
    strcpy(snap.board, "lcd2");
    snap.lead = 1;                        /* the menu's default: FOLLOW+LEAD */
    snap.bright = 100;                    /* the menu's defaults: BRIGHTNESS 100 %, SCREEN OFF NEVER (snap.sleep 0) */
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

/* ten saved networks, every name at its longest (the longest `get`) */
static void full_nets(struct jk_nets *s)
{
    char ssid[JK_SSID_MAX + 1];
    jk_nets_init(s);
    for (int i = 0; i < (int)JK_NETS_MAX; i++) {
        memset(ssid, 'a' + i, JK_SSID_MAX);
        ssid[JK_SSID_MAX] = 0;
        CHECK(jk_nets_put(s, ssid, "password1") == i);
    }
}

static void test_defaults(void)
{
    setup();
    CHECK(cfg.group == 1 && cfg.filter == JK_FILTER_LINK && !strcmp(cfg.name, "Jank"));
}

static void test_get(void)
{
    setup();
    CHECK(run("get") == JK_CTL_NONE);
    CHECK(strstr(out, "group=1\n") && strstr(out, "channel=1\n"));
    CHECK(strstr(out, "name=Jank\n") && strstr(out, "filter=link\n"));
    CHECK(strstr(out, "mac=02:11:22:33:44:55\n"));
    CHECK(strstr(out, "board=lcd2\nversion=" JK_VERSION "\nok\n"));
    strcpy(snap.board, "xiao");
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "board=xiao\n"));
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
    CHECK(strstr(out, "peer=on\n") && !strstr(out, "\nmidi=") && strstr(out, "transport=auto\n"));   /* (usbmidi= is M2d's) */
    CHECK(strstr(out, "quantum=8\n") && strstr(out, "offset=0.00\n"));
}

/* LINK TRANSPORT's third choice: `lead` is the menu's own preference (the firmware keeps it in uiscreen.c): get reads snap.lead, `set lead` is an
 * action that leaves cfg alone and carries the value (jk_ctl_lead) */
static void test_lead(void)
{
    struct jk_cfg before;
    int on = -1;
    static char big[4096];
    setup();
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "transport=link\nquantum=4\noffset=0.00\nlead=on\nsleep=never\nbright=100\nbeat=on\n"));
    snap.lead = 0;
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nlead=off\n"));
    before = cfg;
    CHECK(run("set lead on") == JK_CTL_LEAD && !strcmp(out, "ok\n") && !jk_ctl_lead("set lead on", &on) && on == 1);
    CHECK(run("set lead off") == JK_CTL_LEAD && !strcmp(out, "ok\n") && !jk_ctl_lead("  set lead off \r\n", &on) && on == 0);
    CHECK(!memcmp(&cfg, &before, sizeof cfg));
    CHECK(run("set lead") == JK_CTL_NONE && !strcmp(out, "err bad setting\n"));
    CHECK(run("set lead maybe") == JK_CTL_NONE && !strcmp(out, "err bad setting\n"));
    CHECK(run("set lead 1") == JK_CTL_NONE && run("set lead ON") == JK_CTL_NONE && run("set leadx on") == JK_CTL_NONE);
    CHECK(jk_ctl_lead("set lead maybe", &on) && jk_ctl_lead("set transport link", &on) && jk_ctl_lead("get", &on));
    CHECK(run("help") == JK_CTL_NONE && strstr(out, "|transport|lead|sleep|"));
    snap.lead = 1;
    memset(snap.board, 'x', 11);
    cfg.name[0] = 0;
    memset(cfg.name, 'n', JK_NAME_MAX);
    memset(cfg.ssid, 's', JK_SSID_MAX);
    full_nets(&snap.nets);
    CHECK(jk_ctl_line("get", &cfg, &snap, 1000, big, sizeof big) == JK_CTL_NONE);
    printf("longest get: %zu bytes (limit %u)\n", strlen(big), (unsigned)JK_CTL_OUT_MAX);
    CHECK(strlen(big) < JK_CTL_OUT_MAX && ends(big, "ok\n"));
}

/* SCREEN OFF and BRIGHTNESS are the menu's own preferences too: get reads snap.sleep / snap.bright, `set sleep` / `set bright` are an action carrying the value */
static void test_display(void)
{
    static const char *const NAMES[4] = {"never", "30s", "1min", "5min"};
    struct jk_cfg before;
    char line[32];
    int br = -1, v = -1;
    static char big[4096];
    setup();
    for (int i = 0; i < 4; i++) {
        snap.sleep = (uint8_t)i;
        snprintf(line, sizeof line, "set sleep %s", NAMES[i]);
        CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nsleep=") && !strncmp(strstr(out, "sleep=") + 6, NAMES[i], strlen(NAMES[i])));
        br = -1;
        CHECK(run(line) == JK_CTL_DISPLAY && !strcmp(out, "ok\n") && !jk_ctl_display(line, &br, &v) && br == 0 && v == i);
    }
    snap.bright = 40;
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nbright=40\n"));
    before = cfg;
    CHECK(run("set bright 10") == JK_CTL_DISPLAY && !strcmp(out, "ok\n") && !jk_ctl_display("set bright 10", &br, &v) && br == 1 && v == 10);
    CHECK(run("set bright 100") == JK_CTL_DISPLAY && !jk_ctl_display("  set bright 70 \r\n", &br, &v) && br == 1 && v == 70);
    CHECK(!memcmp(&cfg, &before, sizeof cfg));
    CHECK(run("set sleep") == JK_CTL_NONE && !strcmp(out, "err bad setting\n"));
    CHECK(run("set sleep 30") == JK_CTL_NONE && run("set sleep NEVER") == JK_CTL_NONE && run("set sleep 2") == JK_CTL_NONE && run("set sleepx never") == JK_CTL_NONE);
    CHECK(run("set bright") == JK_CTL_NONE && !strcmp(out, "err bad setting\n"));
    CHECK(run("set bright 9") == JK_CTL_NONE && run("set bright 101") == JK_CTL_NONE && run("set bright 0") == JK_CTL_NONE && run("set bright -10") == JK_CTL_NONE);
    CHECK(run("set bright 5x") == JK_CTL_NONE && run("set bright 1000") == JK_CTL_NONE && run("set bright ") == JK_CTL_NONE && run("set brightx 50") == JK_CTL_NONE);
    CHECK(jk_ctl_display("set bright 9", &br, &v) && jk_ctl_display("set sleep x", &br, &v) && jk_ctl_display("set lead on", &br, &v) && jk_ctl_display("get", &br, &v));
    CHECK(run("help") == JK_CTL_NONE && strstr(out, "|lead|sleep|bright|quantum|"));
    snap.sleep = 3;
    snap.bright = 100;
    memset(snap.board, 'x', 11);
    memset(cfg.name, 'n', JK_NAME_MAX);
    memset(cfg.ssid, 's', JK_SSID_MAX);
    full_nets(&snap.nets);
    CHECK(jk_ctl_line("get", &cfg, &snap, 1000, big, sizeof big) == JK_CTL_NONE);
    printf("longest get: %zu bytes (limit %u)\n", strlen(big), (unsigned)JK_CTL_OUT_MAX);
    CHECK(strlen(big) < JK_CTL_OUT_MAX && ends(big, "ok\n"));
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

/* owner 2026-11-25: up to 10 saved networks; an 11th is refused, `forget` makes room. The firmware saves the network of
 * `set ssid` / `set pass` into the list when it stores the settings (jk_nets_take_cfg, done here by hand) */
static void test_saved_networks(void)
{
    char name[JK_SSID_MAX + 1];
    setup();
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nnets=0\n") && !strstr(out, "net_1="));
    CHECK(run("set ssid Home") == JK_CTL_CFG_CHANGED && run("set pass password1") == JK_CTL_CFG_CHANGED);
    CHECK(jk_nets_take_cfg(&snap.nets, &cfg) == 1 && snap.nets.n == 1 && jk_nets_take_cfg(&snap.nets, &cfg) == 0);
    CHECK(run("set ssid Office Guests") == JK_CTL_CFG_CHANGED && !strcmp(cfg.ssid, "Office Guests") && cfg.pass[0] == 0);   /* new: no password yet */
    CHECK(run("set pass Made-up100%Pass:)") == JK_CTL_CFG_CHANGED && jk_nets_take_cfg(&snap.nets, &cfg) == 1 && snap.nets.n == 2);
    CHECK(!strcmp(snap.nets.net[1].pass, "Made-up100%Pass:)"));
    CHECK(run("set ssid Home") == JK_CTL_CFG_CHANGED && !strcmp(cfg.pass, "password1"));   /* a saved one comes back with its password */
    CHECK(jk_nets_take_cfg(&snap.nets, &cfg) == 0 && snap.nets.n == 2);
    CHECK(run("set pass password2") == JK_CTL_CFG_CHANGED && jk_nets_take_cfg(&snap.nets, &cfg) == 1 && !strcmp(snap.nets.net[0].pass, "password2"));
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nnets=2\nnet_1=Home\nnet_2=Office Guests\n"));
    CHECK(!strstr(out, "password") && !strstr(out, "Made-up"));          /* the passwords never come back */
    for (int i = 2; i < (int)JK_NETS_MAX; i++) {
        snprintf(name, sizeof name, "net %d", i);
        CHECK(jk_nets_put(&snap.nets, name, "password1") == i);
    }
    CHECK(run("set ssid eleventh") == JK_CTL_NONE && !strcmp(out, "err networks full: forget one\n") && !strcmp(cfg.ssid, "Home"));
    CHECK(run("set ssid net 5") == JK_CTL_CFG_CHANGED && !strcmp(cfg.ssid, "net 5"));   /* a saved one is still taken */
    CHECK(run("forget nowhere") == JK_CTL_NONE && !strcmp(out, "err not saved\n"));
    CHECK(run("forget net 5") == JK_CTL_FORGET && !strcmp(out, "ok\n") && !jk_ctl_forget(" forget net 5 \r\n", name) && !strcmp(name, "net 5"));
    CHECK(jk_nets_forget(&snap.nets, name) == 0 && snap.nets.n == 9);
    CHECK(jk_cfg_after_forget(&cfg, &snap.nets) == 1 && !strcmp(cfg.ssid, "Home") && !strcmp(cfg.pass, "password2"));   /* the first saved one */
    CHECK(jk_cfg_after_forget(&cfg, &snap.nets) == 0);
    CHECK(run("set ssid eleventh") == JK_CTL_CFG_CHANGED);               /* there is room again */
    CHECK(jk_ctl_forget("forget", name) == -1 && jk_ctl_forget("forget ", name) == -1 && jk_ctl_forget("get", name) == -1);
    CHECK(run("forget") == JK_CTL_NONE && !strcmp(out, "err unknown command\n"));
    jk_nets_init(&snap.nets);
    CHECK(jk_cfg_after_forget(&cfg, &snap.nets) == 1 && cfg.ssid[0] == 0 && cfg.pass[0] == 0);   /* none left */
    CHECK(run("help") == JK_CTL_NONE && strstr(out, "\nforget <ssid>\n"));
    strcpy(snap.wifi_ssid, "Office Guests");
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "\nwifi_ssid=Office Guests\n"));
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
    CHECK(strstr(out, "air_lost_pct=-1\nair_retry_pct=-1\nrssi_min=0\nrssi_max=0\n"));   /* no window completed yet */
    CHECK(strstr(out, "peer=8c:aa:bb:cc:dd:ee,-44,100,5\n"));
    CHECK(ends(out, "ok\n"));
    /* the last completed window of the peers' frames: 3 of 40 lost, 6 of 40 retried */
    snap.b.win.on = 1;
    snap.b.win.done = (struct jk_win){.heard = 37, .missed = 3, .dup = 3, .retry = 6, .rssi_min = -78, .rssi_max = -52};
    CHECK(run("status") == JK_CTL_NONE);
    CHECK(strstr(out, "peers=1\nrssi=-44\nair_lost_pct=8\nair_retry_pct=15\nrssi_min=-78\nrssi_max=-52\nwifi=0\n"));
    CHECK(strstr(out, "peer=8c:aa:bb:cc:dd:ee,-44,100,5\n"));        /* the peer line keeps its fields */
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
    /* the settings are wiped only by these two words together (a force option: a flash install never touches them) */
    CHECK(run("wipe settings") == JK_CTL_WIPE && ends(out, "ok\n"));
    CHECK(run("wipe") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("wipe settings now") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("wipe all") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("WIPE SETTINGS") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("settings") == JK_CTL_NONE && strstr(out, "err ") == out);
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
    {
        int dir = 9;                                    /* AUDIO mode's fold: the caller answers fold=<f> ok, or err link mode */
        CHECK(run("fold x2") == JK_CTL_FOLD && out[0] == 0);
        CHECK(!jk_ctl_fold("fold x2", &dir) && dir == 1 && !jk_ctl_fold(" fold /2 ", &dir) && dir == -1);
        CHECK(!jk_ctl_fold("fold 1", &dir) && dir == 0);
        CHECK(jk_ctl_fold("fold x3", &dir) == -1 && jk_ctl_fold("fold", &dir) == -1 && jk_ctl_fold("folder x2", &dir) == -1);
        CHECK(run("fold x3") == JK_CTL_NONE && strstr(out, "err fold"));
        CHECK(run("fold") == JK_CTL_NONE && strstr(out, "err fold"));
    }
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

/* the retired BEAT FLASH setting: no key to read, none to set */
static void test_beat_flash_gone(void)
{
    setup();
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "graded=on\nnewsong=off\nusb=") && !strstr(out, "beat_flash"));
    CHECK(run("set beat_flash 1") == JK_CTL_NONE && strstr(out, "err ") == out);
    CHECK(run("help") == JK_CTL_NONE && !strstr(out, "beat_flash"));
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
    /* LINE IN gain: -12..+12 dB, a bench setting (not stored: no save, jk_cfg_stored_differs) */
    CHECK(cfg.line_gain_db == 0 && run("get") == JK_CTL_NONE && strstr(out, "source=auto\nline_gain=0\n"));
    {
        struct jk_cfg before = cfg;
        CHECK(run("set line_gain -12") == JK_CTL_CFG_CHANGED && cfg.line_gain_db == -12 && !jk_cfg_stored_differs(&before, &cfg));
    }
    CHECK(run("set line_gain 13") == JK_CTL_NONE && strstr(out, "err") && cfg.line_gain_db == -12);
    CHECK(run("set line_gain 6") == JK_CTL_CFG_CHANGED && cfg.line_gain_db == 6 && run("get") == JK_CTL_NONE && strstr(out, "line_gain=6\n"));
    snap.beat = (struct jk_beat){.on = 1, .audio = 1, .mode = 1, .bpm_x100 = 12345, .heap_min = 90000, .cpu_pct_x10 = 253, .packets = 77};
    CHECK(run("beat") == JK_CTL_NONE);
    CHECK(strstr(out, "mode=steady\n") && strstr(out, "bpm=123.45\n") && strstr(out, "heap_min=90000\n"));
    CHECK(strstr(out, "cpu_pct=25.3\n") && strstr(out, "packets=77\n") && strstr(out, "beat_weights=none\n") && ends(out, "ok\n"));
    snap.beat.weights = JK_WEIGHTS_OK;               /* the weights blob bound (core/beat/beat_weights.h) */
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "beat_weights=ok\n"));
    snap.beat.weights = JK_WEIGHTS_DAMAGED;
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "beat_weights=damaged\n"));
    /* the source open and sounding (AUDIO mode), the meter, the confidence: in beat and status alike */
    snap.beat = (struct jk_beat){.on = 1, .audio = 1, .audio_live = 1, .level_db = -21, .level_min_db = -30, .level_max_db = -14,
                                 .peak_db = -6, .conf_pct = 73};
    snap.hold = 1;
    snap.outputs = JHO_STOPPED;
    snap.beat.fold_exp = -1;
    snap.beat.fold_manual = 1;
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "outputs=stopped\nfold=0.5\nfold_manual=1\n"));
    snap.beat.fold_exp = 0;
    snap.beat.fold_manual = 0;
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "audio=1\naudio_live=1\nrms_db=-21\nrms_min_db=-30\nrms_max_db=-14\npeak_db=-6\nconf=73\n"));
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "audio=1\naudio_live=1\nhold=1\noutputs=stopped\nfold=1\nfold_manual=0\nrms_db=-21\nrms_min_db=-30\n"));
    CHECK(strstr(out, "rms_max_db=-14\npeak_db=-6\nconf=73\n") && ends(out, "ok\n"));
    snap.beat = (struct jk_beat){.on = 1, .audio = 1, .level_db = -99, .level_min_db = -99, .level_max_db = -99, .peak_db = -99};
    snap.hold = 0;
    snap.outputs = JHO_ARMED;                               /* a stream of zeros: open, no sound, LINK mode */
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "audio=1\naudio_live=0\nhold=0\noutputs=armed\nfold=1\n"));
}

/* AUTO BAR: the setting (on by default; `on` again counts as picked again) and the estimator in status and beat */
static void test_autobar(void)
{
    setup();
    CHECK(cfg.autobar == JK_AUTOBAR_START && cfg.autobar_picks == 0);              /* AT START by default */
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "autobar=start\n"));
    CHECK(run("set autobar off") == JK_CTL_CFG_CHANGED && cfg.autobar == JK_AUTOBAR_OFF && cfg.autobar_picks == 0);
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "autobar=off\n"));
    CHECK(run("set autobar on") == JK_CTL_CFG_CHANGED && cfg.autobar == JK_AUTOBAR_START && cfg.autobar_picks == 1);   /* on = start */
    CHECK(run("set autobar start") == JK_CTL_CFG_CHANGED && cfg.autobar_picks == 2);   /* picked again */
    CHECK(run("set autobar always") == JK_CTL_CFG_CHANGED && cfg.autobar == JK_AUTOBAR_ALWAYS && cfg.autobar_picks == 3);
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "autobar=always\n"));
    CHECK(run("set autobar yes") == JK_CTL_NONE && strstr(out, "err") && cfg.autobar == JK_AUTOBAR_ALWAYS);
    snap.beat = (struct jk_beat){.bar = 2, .bar_phi = 3, .bar_lead_x100 = 241, .bar_moves = 4};
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "bar=locked\nbar_phi=3\nbar_lead=2.41\n") && ends(out, "ok\n"));
    snap.beat = (struct jk_beat){.bar = 1, .bar_phi = -1, .bar_lead_x100 = -5};
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "bar=listening\nbar_phi=-1\nbar_lead=-0.05\n"));
    snap.beat.bar = 3;
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "bar=manual\n"));
    snap.beat = (struct jk_beat){.bar = 0, .bar_phi = -1, .bar_moves = 7};
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "bar=off\n") && strstr(out, "bar_moves=7\n") && ends(out, "ok\n"));
}

/* NUDGE BEAT (the caller applies it) and NUDGE MS (beat_latency, AUDIO mode only); status says both */
static void test_nudge(void)
{
    int h = 0;
    char b[8];
    setup();
    CHECK(run("nudge beat +0.5") == JK_CTL_NUDGE && out[0] == 0);
    CHECK(!jk_ctl_nudge("nudge beat +0.5", &h) && h == 1 && !jk_ctl_nudge(" nudge beat -0.5 ", &h) && h == -1);
    CHECK(!jk_ctl_nudge("nudge beat 1", &h) && h == 2 && !jk_ctl_nudge("nudge beat -1.5", &h) && h == -3);
    CHECK(!jk_ctl_nudge("nudge beat 3", &h) && h == 6 && !jk_ctl_nudge("nudge beat -3.0", &h) && h == -6);
    CHECK(jk_ctl_nudge("nudge beat 0", &h) == -1 && jk_ctl_nudge("nudge beat 0.25", &h) == -1);
    CHECK(jk_ctl_nudge("nudge beat 3.5", &h) == -1 && jk_ctl_nudge("nudge beat +-1", &h) == -1 && jk_ctl_nudge("nudge beat", &h) == -1);
    CHECK(jk_ctl_nudge("nudge beatx 1", &h) == -1 && jk_ctl_nudge("nudge beat up", &h) == -1);
    CHECK(run("nudge beat 4") == JK_CTL_NONE && strstr(out, "err nudge") == out);
    CHECK(run("nudge") == JK_CTL_NONE && strstr(out, "err nudge") == out);
    CHECK(run("nudger") == JK_CTL_NONE && strstr(out, "err unknown") == out);
    CHECK(!strcmp(jk_half_beats(1, b), "0.5") && !strcmp(jk_half_beats(-3, b), "-1.5") && !strcmp(jk_half_beats(4, b), "2"));
    CHECK(!strcmp(jk_half_beats(0, b), "0") && !strcmp(jk_half_beats(-1, b), "-0.5") && !strcmp(jk_half_beats(-6, b), "-3"));
    /* NUDGE MS: LINK mode refused; AUDIO mode moves the AUDIO SHIFT (lat_trim) by n, answers the sum and both layers */
    CHECK(run("nudge ms 1") == JK_CTL_NONE && !strcmp(out, "err link mode\n") && cfg.beat_latency_ms == JK_BEAT_LATENCY_DEFAULT_MS);
    snap.beat.on = snap.beat.audio = snap.beat.audio_live = 1;
    CHECK(run("nudge ms +1") == JK_CTL_CFG_CHANGED && cfg.beat_latency_ms == JK_BEAT_LATENCY_DEFAULT_MS + 1);
    CHECK(!strcmp(out, "beat_latency=26\nlat_auto=25\nlat_trim=1\nok\n"));
    CHECK(run("nudge ms -1") == JK_CTL_CFG_CHANGED && !strcmp(out, "beat_latency=25\nlat_auto=25\nlat_trim=0\nok\n"));
    CHECK(run("nudge ms -10") == JK_CTL_CFG_CHANGED && cfg.beat_latency_ms == 15);
    CHECK(run("nudge ms 0") == JK_CTL_NONE && strstr(out, "err") && run("nudge ms 11") == JK_CTL_NONE && strstr(out, "err"));
    CHECK(run("nudge ms x") == JK_CTL_NONE && strstr(out, "err") && cfg.beat_latency_ms == 15);
    CHECK(run("set beat_latency 300") == JK_CTL_CFG_CHANGED && cfg.beat_latency_ms == 290);   /* the auto base: the shift kept */
    CHECK(run("set lat_trim 0") == JK_CTL_CFG_CHANGED && cfg.beat_latency_ms == JK_BEAT_LATENCY_MAX_MS);
    CHECK(run("nudge ms 1") == JK_CTL_NONE && !strcmp(out, "beat_latency=300\nlat_auto=300\nlat_trim=0\nok\n"));   /* at the top */
    CHECK(run("set beat_latency 10") == JK_CTL_CFG_CHANGED && run("set lat_trim 5") == JK_CTL_CFG_CHANGED && cfg.beat_latency_ms == 15);
    CHECK(run("set lat_trim 601") == JK_CTL_NONE && strstr(out, "err") && cfg.beat_latency_ms == 15);
    snap.beat.nudge_halves = -3;
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "nudge_beats=-1.5\nbeat_latency=15\nlat_auto=10\nlat_trim=5\nbeat_latency_src=usb\nusbmidi=on\n"));
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "beat_latency=15\nlat_auto=10\nlat_trim=5\nbeat_latency_src=usb\n"));
}

/* settings saves: never a flash write while playing, unless a restart follows; what counts as a stored change */
static void test_save(void)
{
    struct jk_cfg a, b;
    uint8_t pending = 0;
    jk_cfg_defaults(&a);
    b = a;
    CHECK(!jk_cfg_stored_differs(&a, &b));
    b.beat_latency_ms = 10;                             /* a bench setting: not stored */
    CHECK(!jk_cfg_stored_differs(&a, &b));
    b.autobar = JK_AUTOBAR_ALWAYS;                      /* stored apart ("usb1"), as SOURCE */
    CHECK(jk_cfg_stored_differs(&a, &b));
    b = a;
    b.source = JUR_SRC_USB;                             /* stored apart ("usb1") */
    CHECK(jk_cfg_stored_differs(&a, &b));
    b = a;
    b.group = 3;
    CHECK(jk_cfg_stored_differs(&a, &b));
    b = a;
    b.lowconf = JCF_RUN;                                /* LOW CONFIDENCE and THRESHOLD: stored apart ("usb1") */
    CHECK(jk_cfg_stored_differs(&a, &b));
    b = a;
    b.threshold = 50;
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

/* usb_set: whether the USB role was ever chosen. Untouched, a board with no port chip (the XIAO) acts as HOST (jur_effective_setting):
 * nothing on a unit with no menu could set it, and the control page refuses to */
static void test_usb_set(void)
{
    struct jk_cfg a, b;
    setup();
    CHECK(cfg.usb == JUR_USB_DEVICE && cfg.usb_set == 0);                 /* the default: no choice made */
    CHECK(run("set usb otg") == JK_CTL_NONE && cfg.usb_set == 0);         /* a refused value is no choice */
    CHECK(run("set usb host") == JK_CTL_CFG_CHANGED && cfg.usb == JUR_USB_HOST && cfg.usb_set == 1);
    setup();
    CHECK(run("set usb device") == JK_CTL_CFG_CHANGED && cfg.usb == JUR_USB_DEVICE && cfg.usb_set == 1);   /* an explicit DEVICE is a choice too: it stays */
    run("set usb device");
    CHECK(cfg.usb == JUR_USB_DEVICE && cfg.usb_set == 1);                  /* said again: the choice stays */
    setup();
    CHECK(run("set quantum 3") == JK_CTL_CFG_CHANGED && cfg.usb_set == 0); /* another setting is no choice of the USB role */
    jk_cfg_defaults(&a);
    b = a;
    CHECK(!jk_cfg_stored_differs(&a, &b));
    b.usb_set = 1;
    CHECK(jk_cfg_stored_differs(&a, &b));                                  /* a choice made is something to save */
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
    memset(snap.wifi_ssid, 'w', JK_SSID_MAX);
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
    /* the merged build's other status lines at their longest: AUTO BAR */
    snap.beat.bar = 3;
    snap.beat.bar_phi = -128;
    snap.beat.bar_lead_x100 = -2147483647;
    snap.usb.audio_packets = snap.usb.audio_errors = snap.usb.midi_dropped = 4294967295u;
    snap.midi_ticks = snap.midi_resync = 4294967295u;
    snap.beat.audio = snap.beat.audio_live = snap.hold = 1;           /* the audio lines at their longest */
    snap.outputs = JHO_RUNNING;
    snap.beat.level_db = snap.beat.level_min_db = snap.beat.level_max_db = snap.beat.peak_db = -99;
    snap.beat.conf_pct = snap.beat.act_pct = 100;
    snap.beat.gated = snap.beat.silent = 1;
    cfg.threshold = 100;
    cfg.lowconf = JCF_STOP;                                           /* "stop" */
    snap.beat.fold_exp = -2;                                          /* "0.25" */
    snap.beat.fold_manual = 1;
    snap.beat.nudge_halves = -5;                                      /* "-2.5" */
    snap.beat.follow = 0;                                             /* "stopped" (jb_follow.h JF_STOPPED) */
    snap.beat.drift_ppm = -2147483647 - 1;                            /* the follow lines at their longest */
    snap.beat.rate_x10000 = -2147483647 - 1;
    cfg.lat_key = (struct jla_key){JLA_HOSTED, 0xFFFF, 0xFFFF};       /* "host:ffff:ffff" */
    jla_set(&cfg.lat, &cfg.lat_key, JK_BEAT_LATENCY_MAX_MS);          /* (a shift of -600 needs a base of 300 first) */
    jla_set_trim(&cfg.lat, &cfg.lat_key, -600);                       /* lat_trim=-600 */
    jla_set(&cfg.lat, &cfg.lat_key, JK_BEAT_LATENCY_MIN_MS);          /* lat_auto=-300, beat_latency=-300 */
    cfg.beat_latency_ms = JK_BEAT_LATENCY_MIN_MS;
    CHECK(jk_ctl_line("status", &cfg, &snap, 1000, big, sizeof big) == JK_CTL_NONE);
    printf("longest status: %zu bytes\n", strlen(big));
    CHECK(strstr(big, "bar=manual\n") && strstr(big, "usb_role=host\n"));
    CHECK(strlen(big) < JK_CTL_OUT_MAX && ends(big, "ok\n"));
    /* `beat` with every counter at its largest (M2m added the robust follower's: it must still fit the answer buffer) */
    memset(&snap.beat, 0xFF, sizeof snap.beat);
    snap.beat.mode = snap.beat.follow = 0;
    CHECK(jk_ctl_line("beat", &cfg, &snap, 1000, big, sizeof big) == JK_CTL_NONE);
    printf("longest beat: %zu bytes\n", strlen(big));
    CHECK(strlen(big) < JK_CTL_OUT_MAX && ends(big, "ok\n"));
}

/* LOW CONFIDENCE: STOP / START at 80 % by default; set lowconf run|stop, set threshold 0..100; get and status say them,
 * status and beat the grid-fit confidence (conf=), the network's activation (act=), the gate and the silence */
static void test_lowconf(void)
{
    struct jk_cfg before;
    setup();
    CHECK(cfg.lowconf == JCF_STOP && cfg.threshold == 80);
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nlowconf=stop\nthreshold=80\npause_db=-40\n"));
    /* PAUSE LEVEL (core/beat/jb_lost.h): set pause_db -70..-20 dBFS, -40 by default, stored */
    before = cfg;
    CHECK(cfg.pause_db == JK_PAUSE_DB_DEFAULT && run("set pause_db -35") == JK_CTL_CFG_CHANGED && cfg.pause_db == -35);
    CHECK(jk_cfg_stored_differs(&before, &cfg));
    CHECK(run("set pause_db -71") == JK_CTL_NONE && run("set pause_db -19") == JK_CTL_NONE && run("set pause_db x") == JK_CTL_NONE);
    CHECK(run("set pause_db -70") == JK_CTL_CFG_CHANGED && run("set pause_db -20") == JK_CTL_CFG_CHANGED && cfg.pause_db == -20);
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nthreshold=80\npause_db=-20\nrate="));
    run("set pause_db -40");
    before = cfg;
    CHECK(run("set lowconf run") == JK_CTL_CFG_CHANGED && cfg.lowconf == JCF_RUN && jk_cfg_stored_differs(&before, &cfg));
    CHECK(run("set lowconf stop") == JK_CTL_CFG_CHANGED && cfg.lowconf == JCF_STOP);
    CHECK(run("set lowconf keep") == JK_CTL_NONE && !strcmp(out, "err bad setting\n") && cfg.lowconf == JCF_STOP);
    CHECK(run("set threshold 65") == JK_CTL_CFG_CHANGED && cfg.threshold == 65);
    CHECK(run("set threshold 0") == JK_CTL_CFG_CHANGED && cfg.threshold == 0);
    CHECK(run("set threshold 100") == JK_CTL_CFG_CHANGED && cfg.threshold == 100);
    CHECK(run("set threshold 101") == JK_CTL_NONE && cfg.threshold == 100);
    CHECK(run("set threshold -5") == JK_CTL_NONE && run("set threshold 7.5") == JK_CTL_NONE && run("set threshold") == JK_CTL_NONE);
    CHECK(cfg.threshold == 100);
    run("set threshold 70");
    run("set lowconf run");
    snap.beat = (struct jk_beat){.on = 1, .audio = 1, .audio_live = 1, .conf_pct = 62, .act_pct = 91, .gated = 1};
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "\nconf=62\nconf_hold=0\ncoast=0\nact=91\ngated=1\nsilent=0\nthreshold=70\nlowconf=run\n"));
    snap.beat.conf_hold = 1;                                          /* M2k: the confidence holds (no evidence) */
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "\nconf=62\nconf_hold=1\n"));
    snap.beat.coast = 1;                                              /* the robust follower's coast */
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "\nconf=62\nconf_hold=1\ncoast=1\nact=91\n"));
    snap.beat.coast = 0;
    snap.beat.conf_hold = 0;
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "\nlowconf=run\nconf_gate=62\n"));   /* not graded: the gate's value is the shown one */
    snap.beat.conf_graded = 1;                                        /* the graded value is shown: the gate's own is conf_gate */
    snap.beat.conf_gate_pct = 91;
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "\nconf=62\n") && strstr(out, "\nlowconf=run\nconf_gate=91\n"));
    snap.beat.conf_graded = 0;
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nlowconf=run\nthreshold=70\n"));
    snap.beat.silent = 1;
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "\nconf=62\nconf_hold=0\ncoast=0\nact=91\ngated=1\nsilent=1\nthreshold=70\nlowconf=run\nmode="));
    CHECK(run("help") == JK_CTL_NONE && strstr(out, "|usbmidi|lowconf|threshold|"));
}

/* ADVANCED FOLLOW (M2i): set rate on|off (on by default, stored); get says it; status the follower's state, drift and the
 * last ratio taken; beat all of the rate's numbers */
static void test_follow(void)
{
    struct jk_cfg before;
    setup();
    CHECK(cfg.rate == 1);
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nthreshold=80\npause_db=-40\nrate=on\n"));
    before = cfg;
    CHECK(run("set rate off") == JK_CTL_CFG_CHANGED && cfg.rate == 0 && jk_cfg_stored_differs(&before, &cfg));
    CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nrate=off\n"));
    /* M2m A/B: set follower tuned|robust, not stored (back to tuned at every start) */
    CHECK(cfg.follower == 0 && run("get") == JK_CTL_NONE && strstr(out, "\nfollower=tuned\n"));
    {
        struct jk_cfg was = cfg;
        CHECK(run("set follower robust") == JK_CTL_CFG_CHANGED && cfg.follower == 1 && !jk_cfg_stored_differs(&was, &cfg));
        CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nfollower=robust\n"));
        CHECK(run("set follower fast") == JK_CTL_NONE && !strcmp(out, "err bad setting\n") && cfg.follower == 1);
        CHECK(run("set follower tuned") == JK_CTL_CFG_CHANGED && cfg.follower == 0);
    }
    /* SKIP (robust follower) and the graded confidence (both followers, on by default): set skip off|act|pat, set graded on|off; not stored */
    CHECK(cfg.skip == 0 && cfg.graded == 1 && run("get") == JK_CTL_NONE && strstr(out, "\nskip=off\ngraded=on\n"));
    {
        struct jk_cfg was = cfg;
        CHECK(run("set skip pat") == JK_CTL_CFG_CHANGED && cfg.skip == 2 && !jk_cfg_stored_differs(&was, &cfg));
        CHECK(run("get") == JK_CTL_NONE && strstr(out, "\nskip=pat\n"));
        CHECK(run("set skip act") == JK_CTL_CFG_CHANGED && cfg.skip == 1);
        CHECK(run("set skip fast") == JK_CTL_NONE && !strcmp(out, "err bad setting\n") && cfg.skip == 1);
        CHECK(run("set skip off") == JK_CTL_CFG_CHANGED && cfg.skip == 0);
        CHECK(run("set graded off") == JK_CTL_CFG_CHANGED && cfg.graded == 0 && !jk_cfg_stored_differs(&was, &cfg));
        CHECK(run("get") == JK_CTL_NONE && strstr(out, "\ngraded=off\n"));
        CHECK(run("set graded maybe") == JK_CTL_NONE && !strcmp(out, "err bad setting\n") && cfg.graded == 0);
        CHECK(run("set graded on") == JK_CTL_CFG_CHANGED && cfg.graded == 1);
        /* NEW SONG (core/beat/jb_song.h): set newsong on|off, off by default, not stored */
        CHECK(cfg.newsong == 0 && run("set newsong on") == JK_CTL_CFG_CHANGED && cfg.newsong == 1 && !jk_cfg_stored_differs(&was, &cfg));
        CHECK(run("get") == JK_CTL_NONE && strstr(out, "\ngraded=on\nnewsong=on\n"));
        CHECK(run("set newsong maybe") == JK_CTL_NONE && !strcmp(out, "err bad setting\n") && cfg.newsong == 1);
        CHECK(run("set newsong off") == JK_CTL_CFG_CHANGED && cfg.newsong == 0);
    }
    CHECK(run("help") == JK_CTL_NONE && strstr(out, "|follower|skip|graded|newsong|burn|"));
    CHECK(run("set rate maybe") == JK_CTL_NONE && !strcmp(out, "err bad setting\n") && cfg.rate == 0);
    CHECK(run("set rate on") == JK_CTL_CFG_CHANGED && cfg.rate == 1);
    snap.beat = (struct jk_beat){.on = 1, .audio = 1, .audio_live = 1, .conf_pct = 97, .follow = 2 /* glide */, .drift_ppm = -35,
                                 .rate_x10000 = 10203, .manoeuvres = 2, .rate_on = 1, .rate_active = 1, .rate_events = 7,
                                 .rate_taken = 3, .rate_refused = 4, .rate_ignored = 1, .rate_us = 3120, .rate_max_us = 95};
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "\nfollow=glide\ndrift_ppm=-35\nrate_ratio=1.0203\n"));
    CHECK(!strstr(out, "rate_events="));
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "\nfollow=glide\ndrift_ppm=-35\nrate_ratio=1.0203\nmanoeuvres=2\nrate=on\nrate_active=1\n"));
    CHECK(strstr(out, "\nrate_events=7\nrate_taken=3\nrate_refused=4\nrate_ignored=1\nrate_us=3120\nrate_max_us=95\n"));
    CHECK(strstr(out, "\nrate_max_us=95\nweak=off\nweak_holds=0\ncoasts=0\nweak_beats=0\nfast_jumps=0\n"));
    snap.beat.weak_on = 1, snap.beat.weak_holds = 41, snap.beat.coasts = 17, snap.beat.weak_beats = 230, snap.beat.fast_jumps = 2;
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "\nweak=on\nweak_holds=41\ncoasts=17\nweak_beats=230\nfast_jumps=2\n"));
    CHECK(ends(out, "ok\n"));
    CHECK(run("status") == JK_CTL_NONE && !strstr(out, "weak_holds="));     /* (status is nearly full: beat only) */
    snap.beat.rate_on = 0;                                            /* no PSRAM for its ring: off whatever the setting */
    CHECK(run("beat") == JK_CTL_NONE && strstr(out, "\nrate=off\n"));
    snap.beat.follow = 1;                                             /* lock */
    snap.beat.rate_x10000 = 9700;
    CHECK(run("status") == JK_CTL_NONE && strstr(out, "\nfollow=lock\n") && strstr(out, "\nrate_ratio=0.9700\n"));
    CHECK(run("help") == JK_CTL_NONE && strstr(out, "|threshold|pause_db|rate|follower|skip|graded|newsong|burn|usb|") && !strstr(out, "ble"));
}

/* `about`: the ABOUT screen's numbers as a second reply (not in `status`, which has no room for them: test_status_size). Unknown until the
 * sampler has filled them, then the sampler's values; cpu_pct from the beat's idle share as the screen computes it */
static void test_about(void)
{
    static char big[4096];
    setup();
    CHECK(run("about") == JK_CTL_NONE);
    CHECK(!strcmp(out, "uptime_s=-1\nreset=\npanic=\ntemp_c=-999\ncpu_pct=-1\ncpu1_pct=-1\nheap_kb=-1\nheap_min_kb=-1\npsram_kb=-1\nflash_kb=-1\napp_used_kb=-1\n"
                       "app_part_kb=-1\nclock_late_us=-1\nui_loop_us=-1\nbeat_loop_us=-1\nok\n"));
    snap.sys = (struct jk_sys){1, 41, 123, 7936, 98, 3725, 8192, 1260, 3072, 12, 4210, 8800, "POWERON", "loopTask@0x4200abcd"};
    snap.beat.core0_idle_x10 = 630;
    snap.beat.core1_busy_x10 = 27;
    snap.beat.core1_ok = 1;
    CHECK(run("about") == JK_CTL_NONE);
    CHECK(!strcmp(out, "uptime_s=3725\nreset=POWERON\npanic=loopTask@0x4200abcd\ntemp_c=41\ncpu_pct=37\ncpu1_pct=3\nheap_kb=123\nheap_min_kb=98\npsram_kb=7936\nflash_kb=8192\napp_used_kb=1260\n"
                       "app_part_kb=3072\nclock_late_us=12\nui_loop_us=4210\nbeat_loop_us=8800\nok\n"));
    CHECK(jk_cpu_pct(0) == -1 && jk_cpu_pct(1000) == 0 && jk_cpu_pct(1004) == 0 && jk_cpu_pct(985) == 1 && jk_cpu_pct(5) == 99 && jk_cpu_pct(4000) == 0);
    CHECK(jk_cpu1_pct(0, 0) == -1 && jk_cpu1_pct(0, 1) == 0 && jk_cpu1_pct(27, 1) == 3 && jk_cpu1_pct(24, 1) == 2 && jk_cpu1_pct(1000, 1) == 100 && jk_cpu1_pct(4000, 1) == 100);
    snap.sys = (struct jk_sys){1, -999, 2147483647, 2147483647, 2147483647, 2147483647, 2147483647, 2147483647, 2147483647, 2147483647, 2147483647, 2147483647, "PWR GLITCH", "0123456789abcde@0x4200abcd"};
    CHECK(jk_ctl_line("about", &cfg, &snap, 1000, big, sizeof big) == JK_CTL_NONE);
    printf("longest about: %zu bytes (status limit %u)\n", strlen(big), (unsigned)JK_CTL_OUT_MAX);
    CHECK(strlen(big) < JK_CTL_OUT_MAX && ends(big, "ok\n") && strstr(big, "panic=0123456789abcde@0x4200abcd\n"));
    CHECK(run("help") == JK_CTL_NONE && strstr(out, "status\nabout\n"));
}

int main(void)
{
    test_defaults();
    test_saved_networks();
    test_lowconf();
    test_follow();
    test_beat();
    test_autobar();
    test_nudge();
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
    test_beat_flash_gone();
    test_button();
    test_usb();
    test_usb_set();
    test_status_size();
    test_about();
    test_lead();
    test_display();
    test_save();
    test_intr_summary();
    printf("ctl: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
