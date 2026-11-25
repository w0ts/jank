/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_nets.h"

#include <string.h>

static int printable(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 0x20u || (unsigned char)s[i] > 0x7Eu)
            return 0;
    return 1;
}

/* a name of 1..32 printable characters, a password empty (open) or 8..63 */
static int valid(const char *ssid, const char *pass)
{
    size_t ns = strlen(ssid), np = strlen(pass);
    return ns >= 1u && ns <= JK_NETS_SSID_MAX && printable(ssid, ns) && (np == 0u || (np >= 8u && np <= JK_NETS_PASS_MAX)) &&
           printable(pass, np);
}

void jk_nets_init(struct jk_nets *s) { memset(s, 0, sizeof *s); }

int jk_nets_find(const struct jk_nets *s, const char *ssid)
{
    for (uint8_t i = 0; i < s->n && i < JK_NETS_MAX; i++)
        if (!strcmp(s->net[i].ssid, ssid))
            return i;
    return -1;
}

int jk_nets_put(struct jk_nets *s, const char *ssid, const char *pass)
{
    int i;
    if (!valid(ssid, pass))
        return -1;
    i = jk_nets_find(s, ssid);
    if (i < 0) {
        if (s->n >= JK_NETS_MAX)
            return -2;
        i = s->n++;
        strcpy(s->net[i].ssid, ssid);
    }
    strcpy(s->net[i].pass, pass);
    return i;
}

int jk_nets_forget(struct jk_nets *s, const char *ssid)
{
    int i = jk_nets_find(s, ssid);
    if (i < 0)
        return -1;
    memmove(&s->net[i], &s->net[i + 1], (size_t)(s->n - 1u - (unsigned)i) * sizeof s->net[0]);
    s->n--;
    memset(&s->net[s->n], 0, sizeof s->net[0]);
    return 0;
}

int jk_nets_pick(const struct jk_nets *s, const struct jk_seen *seen, size_t n_seen, uint32_t tried)
{
    int best = -1;
    int best_rssi = -1000;
    for (size_t k = 0; k < n_seen; k++) {
        int i = jk_nets_find(s, seen[k].ssid);
        if (i < 0 || (tried & (1u << i)) || seen[k].rssi <= best_rssi)
            continue;
        best = i;
        best_rssi = seen[k].rssi;
    }
    return best;
}

size_t jk_nets_pack(const struct jk_nets *s, uint8_t *buf, size_t cap)
{
    size_t at = 2;
    if (cap < 2u || s->n > JK_NETS_MAX)
        return 0;
    buf[0] = JK_NETS_PACK_VERSION;
    buf[1] = s->n;
    for (uint8_t i = 0; i < s->n; i++) {
        size_t ns = strlen(s->net[i].ssid), np = strlen(s->net[i].pass);
        if (at + 2u + ns + np > cap)
            return 0;
        buf[at++] = (uint8_t)ns;
        memcpy(buf + at, s->net[i].ssid, ns);
        at += ns;
        buf[at++] = (uint8_t)np;
        memcpy(buf + at, s->net[i].pass, np);
        at += np;
    }
    return at;
}

/* one length-prefixed text of at most max bytes into to (terminated); -1 when it runs past the end or is too long */
static int take_text(const uint8_t *buf, size_t n, size_t *at, char *to, size_t max)
{
    size_t len;
    if (*at >= n)
        return -1;
    len = buf[(*at)++];
    if (len > max || *at + len > n)
        return -1;
    memcpy(to, buf + *at, len);
    to[len] = 0;
    *at += len;
    return 0;
}

int jk_nets_unpack(struct jk_nets *s, const uint8_t *buf, size_t n)
{
    struct jk_nets r;
    size_t at = 2;
    jk_nets_init(s);
    if (n < 2u || buf[0] != JK_NETS_PACK_VERSION || buf[1] > JK_NETS_MAX)
        return -1;
    jk_nets_init(&r);
    for (uint8_t i = 0; i < buf[1]; i++) {
        struct jk_saved_net t;
        if (take_text(buf, n, &at, t.ssid, JK_NETS_SSID_MAX) || take_text(buf, n, &at, t.pass, JK_NETS_PASS_MAX) ||
            jk_nets_put(&r, t.ssid, t.pass) != (int)i)
            return -1;                  /* unreadable, or a name twice */
    }
    if (at != n)
        return -1;
    *s = r;
    return 0;
}
