/* SPDX-License-Identifier: GPL-3.0-only */
/* The saved Wi-Fi networks (core/jank_nets.h): saving, the 10 limit, forgetting, which one to join, the stored form. */
#include <stdio.h>
#include <string.h>
#include "jank_nets.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void name_of(int i, char *ssid) { snprintf(ssid, JK_NETS_SSID_MAX + 1, "net %d", i); }

static struct jk_seen heard(const char *ssid, int rssi)
{
    struct jk_seen s;
    memset(&s, 0, sizeof s);
    strcpy(s.ssid, ssid);
    s.rssi = (int8_t)rssi;
    return s;
}

static void test_put_and_find(void)
{
    struct jk_nets s;
    jk_nets_init(&s);
    CHECK(s.n == 0 && jk_nets_find(&s, "Home") == -1);
    CHECK(jk_nets_put(&s, "Home", "password1") == 0 && s.n == 1);
    CHECK(jk_nets_put(&s, "Office Guests", "Made-up100%Pass:)") == 1 && s.n == 2);   /* spaces and punctuation are kept */
    CHECK(jk_nets_find(&s, "Office Guests") == 1 && !strcmp(s.net[1].pass, "Made-up100%Pass:)"));
    CHECK(jk_nets_put(&s, "Home", "password2") == 0 && s.n == 2 && !strcmp(s.net[0].pass, "password2"));   /* a new password, same place */
    CHECK(jk_nets_put(&s, "cafe", "") == 2 && s.n == 3);                    /* an open network */
    CHECK(jk_nets_find(&s, "Mots") == -1);                                   /* names are exact */
}

static void test_bad_entries_are_refused(void)
{
    struct jk_nets s;
    char long_name[40];
    jk_nets_init(&s);
    CHECK(jk_nets_put(&s, "", "password1") == -1);
    CHECK(jk_nets_put(&s, "short pass", "1234567") == -1);                   /* a password is 8 to 63 characters */
    memset(long_name, 'n', 33);
    long_name[33] = 0;
    CHECK(jk_nets_put(&s, long_name, "password1") == -1);                    /* a name is at most 32 */
    long_name[32] = 0;
    CHECK(jk_nets_put(&s, long_name, "password1") == 0);
    CHECK(jk_nets_put(&s, "tab\tname", "password1") == -1 && s.n == 1);      /* printable only */
}

static void test_ten_at_most(void)
{
    struct jk_nets s;
    char ssid[JK_NETS_SSID_MAX + 1];
    jk_nets_init(&s);
    CHECK(JK_NETS_MAX == 10);
    for (int i = 0; i < 10; i++) {
        name_of(i, ssid);
        CHECK(jk_nets_put(&s, ssid, "password1") == i);
    }
    CHECK(jk_nets_put(&s, "eleventh", "password1") == -2 && s.n == 10);     /* refused: forget one first (owner) */
    CHECK(jk_nets_put(&s, "net 3", "password9") == 3);                       /* a saved one still takes a new password */
    CHECK(jk_nets_forget(&s, "net 3") == 0 && s.n == 9);
    CHECK(jk_nets_put(&s, "eleventh", "password1") == 9);
}

static void test_forget(void)
{
    struct jk_nets s;
    jk_nets_init(&s);
    jk_nets_put(&s, "a", "password1");
    jk_nets_put(&s, "b", "password2");
    jk_nets_put(&s, "c", "password3");
    CHECK(jk_nets_forget(&s, "x") == -1 && s.n == 3);
    CHECK(jk_nets_forget(&s, "b") == 0 && s.n == 2);
    CHECK(!strcmp(s.net[0].ssid, "a") && !strcmp(s.net[1].ssid, "c") && !strcmp(s.net[1].pass, "password3"));   /* the others keep their order */
    CHECK(s.net[2].ssid[0] == 0 && s.net[2].pass[0] == 0);                   /* nothing of b is left behind */
    CHECK(jk_nets_forget(&s, "c") == 0 && jk_nets_forget(&s, "a") == 0 && s.n == 0);
}

/* the network to join: the saved one heard best; one that failed this round is skipped */
static void test_pick(void)
{
    struct jk_nets s;
    struct jk_seen seen[4];
    jk_nets_init(&s);
    jk_nets_put(&s, "Home", "password1");
    jk_nets_put(&s, "Office Guests", "password2");
    seen[0] = heard("neighbour", -30);                                       /* not saved: never */
    seen[1] = heard("Home", -70);
    seen[2] = heard("Office Guests", -55);
    seen[3] = heard("Home", -60);                                            /* the same network from a second access point */
    CHECK(jk_nets_pick(&s, seen, 4, 0) == 1);
    CHECK(jk_nets_pick(&s, seen, 4, 1u << 1) == 0);                          /* Office Guests failed: Home */
    CHECK(jk_nets_pick(&s, seen, 4, (1u << 0) | (1u << 1)) == -1);           /* both failed */
    CHECK(jk_nets_pick(&s, seen, 1, 0) == -1);                               /* none saved in range */
    CHECK(jk_nets_pick(&s, seen, 0, 0) == -1);
    seen[2] = heard("Office Guests", -60);
    CHECK(jk_nets_pick(&s, seen + 1, 3, 0) == 1);                            /* a tie at -60: the first heard (Office Guests) */
}

static void test_pack_round_trip(void)
{
    struct jk_nets s, r;
    uint8_t buf[JK_NETS_PACK_MAX];
    char ssid[JK_NETS_SSID_MAX + 1], pass[JK_NETS_PASS_MAX + 1];
    size_t n;
    jk_nets_init(&s);
    CHECK(jk_nets_pack(&s, buf, sizeof buf) == 2 && jk_nets_unpack(&r, buf, 2) == 0 && r.n == 0);
    memset(pass, 'p', JK_NETS_PASS_MAX);
    pass[JK_NETS_PASS_MAX] = 0;
    for (int i = 0; i < 10; i++) {                                           /* the largest list: every name and password at its longest */
        memset(ssid, 'a' + i, JK_NETS_SSID_MAX);
        ssid[JK_NETS_SSID_MAX] = 0;
        CHECK(jk_nets_put(&s, ssid, pass) == i);
    }
    n = jk_nets_pack(&s, buf, sizeof buf);
    CHECK(n == JK_NETS_PACK_MAX);
    CHECK(jk_nets_pack(&s, buf, n - 1u) == 0);                               /* it does not fit: nothing */
    CHECK(jk_nets_unpack(&r, buf, n) == 0 && !memcmp(&r, &s, sizeof s));
    jk_nets_init(&s);
    jk_nets_put(&s, "Home", "password1");
    jk_nets_put(&s, "cafe", "");
    n = jk_nets_pack(&s, buf, sizeof buf);
    CHECK(jk_nets_unpack(&r, buf, n) == 0 && r.n == 2 && !strcmp(r.net[1].ssid, "cafe") && r.net[1].pass[0] == 0);
}

static void test_unreadable_is_empty(void)
{
    struct jk_nets s, r;
    uint8_t buf[JK_NETS_PACK_MAX];
    size_t n;
    jk_nets_init(&s);
    jk_nets_put(&s, "Home", "password1");
    jk_nets_put(&s, "cafe", "password2");
    n = jk_nets_pack(&s, buf, sizeof buf);
    CHECK(jk_nets_unpack(&r, buf, n - 1u) == -1 && r.n == 0);                 /* cut short */
    buf[n] = 0;
    CHECK(jk_nets_unpack(&r, buf, n + 1u) == -1 && r.n == 0);                 /* a byte too many */
    buf[0] = 2;
    CHECK(jk_nets_unpack(&r, buf, n) == -1);                                 /* another version */
    buf[0] = JK_NETS_PACK_VERSION;
    buf[1] = 11;
    CHECK(jk_nets_unpack(&r, buf, n) == -1);                                 /* more than 10 */
    buf[1] = 2;
    buf[2] = 40;
    CHECK(jk_nets_unpack(&r, buf, n) == -1);                                 /* a name longer than 32 */
    jk_nets_init(&s);
    jk_nets_put(&s, "Home", "password1");
    n = jk_nets_pack(&s, buf, sizeof buf);
    memcpy(buf + n, buf + 2, n - 2u);                                        /* the same network twice */
    buf[1] = 2;
    CHECK(jk_nets_unpack(&r, buf, 2u * n - 2u) == -1);
    buf[1] = 1;
    CHECK(jk_nets_unpack(&r, buf, n) == 0 && r.n == 1);                      /* (the first copy alone reads) */
    buf[2 + 1 + 4] = 3;                                                      /* a password of 3 characters (the rest left over) */
    CHECK(jk_nets_unpack(&r, buf, n) == -1);
    CHECK(jk_nets_unpack(&r, buf, 1) == -1 && jk_nets_unpack(&r, buf, 0) == -1);
}

int main(void)
{
    test_put_and_find();
    test_bad_entries_are_refused();
    test_ten_at_most();
    test_forget();
    test_pick();
    test_pack_round_trip();
    test_unreadable_is_empty();
    printf("test_nets: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
