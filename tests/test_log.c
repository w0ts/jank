/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the serial test log. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_log.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

int main(void)
{
    static const uint8_t MAC[6] = {0x02, 1, 2, 3, 4, 5};
    uint32_t heard[JK_GROUP_MAX + 1u] = {0};
    struct jk_snapshot s;
    char out[320];                           /* the same as firmware/main/serial.c */
    size_t n;
    memset(&s, 0, sizeof s);
    CHECK(jk_init(&s.b, MAC, 3, JK_FILTER_LINK) == 0);
    s.uptime_s = 12;
    s.tx_fail = 2;
    s.b.st.air_out = 34;
    s.link_on = 1;
    s.link_peers = 1;
    s.mpb = 500000;
    s.b.peer[0].used = 1;
    s.b.peer[0].rssi = -44;
    s.b.peer[0].t_heard = 900;
    heard[3] = 120;
    heard[9] = 7;
    n = jk_log_line(out, sizeof out, &s, heard, 5, 1, 1000);
    CHECK(n == strlen(out) && n > 0);
    CHECK(!strcmp(out, "# up=12 group=3 ch=11 heard=0,0,120,0,0,0,0,0,7 peers=1 rssi=-44 sent=34 tx_fail=2 uni_ok=5 uni_fail=1 link=1 link_peers=1 bpm=120.00\n"));
    /* no session */
    s.mpb = 0;
    CHECK(jk_log_line(out, sizeof out, &s, heard, 0, 0, 100000) > 0);
    CHECK(strstr(out, "peers=0 rssi=0 ") && strstr(out, "bpm=0.00\n"));
    /* the largest numbers fit the firmware's buffer, a short one is refused, not cut */
    memset(heard, 0xFF, sizeof heard);
    s.uptime_s = s.tx_fail = s.link_peers = s.b.st.air_out = 0xFFFFFFFFu;
    CHECK(jk_log_line(out, sizeof out, &s, heard, 0xFFFFFFFFu, 0xFFFFFFFFu, 0) > 0);
    CHECK(jk_log_line(out, 20, &s, heard, 0, 0, 0) == 0);
    printf("log: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
