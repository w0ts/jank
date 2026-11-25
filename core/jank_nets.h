/* SPDX-License-Identifier: GPL-3.0-only */
/* The saved Wi-Fi networks (owner 2026-11-25: "allow to save up to 5 or 10 networks"; 10 picked): names and passwords,
 * at most JK_NETS_MAX. Adding one more is refused (the owner's choice over dropping the oldest): `forget` makes room.
 * At start-up, and when a connection is lost and does not come back, the unit scans and joins the saved network it hears
 * best; one that fails is skipped for the rest of that round (jk_nets_pick). Kept in NVS apart from the settings blob
 * (firmware/main/store.c, key "nets"), so a firmware without the list still reads its settings. Pure C, host-tested. */
#ifndef JANK_NETS_H
#define JANK_NETS_H

#include <stddef.h>
#include <stdint.h>

#define JK_NETS_MAX 10u
#define JK_NETS_SSID_MAX 32u           /* = JK_SSID_MAX (jank_core.h checks it) */
#define JK_NETS_PASS_MAX 63u           /* = JK_PASS_MAX */
#define JK_NETS_PACK_VERSION 1u
/* the packed form: version, count, then per network: ssid length, ssid, password length, password */
#define JK_NETS_PACK_MAX (2u + JK_NETS_MAX * (2u + JK_NETS_SSID_MAX + JK_NETS_PASS_MAX))

struct jk_saved_net {
    char ssid[JK_NETS_SSID_MAX + 1];
    char pass[JK_NETS_PASS_MAX + 1];   /* empty: an open network */
};

struct jk_nets {
    uint8_t n;
    struct jk_saved_net net[JK_NETS_MAX];
};

/* one network a scan heard */
struct jk_seen {
    char ssid[JK_NETS_SSID_MAX + 1];
    int8_t rssi;
};

void jk_nets_init(struct jk_nets *s);
int jk_nets_find(const struct jk_nets *s, const char *ssid);                    /* its index, -1 not saved */
/* saves a network, or changes the password of a saved one: its index; -1 a bad name or password, -2 full */
int jk_nets_put(struct jk_nets *s, const char *ssid, const char *pass);
int jk_nets_forget(struct jk_nets *s, const char *ssid);                        /* 0 forgotten, -1 not saved */
/* the saved network to join: the one heard best among those not in `tried` (bit i: saved network i failed this round);
 * -1 when none is heard */
int jk_nets_pick(const struct jk_nets *s, const struct jk_seen *seen, size_t n_seen, uint32_t tried);
size_t jk_nets_pack(const struct jk_nets *s, uint8_t *buf, size_t cap);       /* bytes written, 0 when it does not fit */
int jk_nets_unpack(struct jk_nets *s, const uint8_t *buf, size_t n);            /* 0 ok; -1 unreadable (s is then empty) */

#endif
