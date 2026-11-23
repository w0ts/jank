/* SPDX-License-Identifier: GPL-3.0-only */
/* The host-mode probe record (jank_hostdiag.h). */
#include "jank_hostdiag.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *const STACK[] = {"unknown", "device", "host"};
static const char *const ROLE[] = {"other", "lpx", "dn2"};

void jhd_init(struct jhd *d)
{
    memset(d, 0, sizeof *d);
    d->version = JHD_VERSION;
    d->stack = JHD_STACK_UNKNOWN;
}

int jhd_valid(const void *p, size_t n) { return p && n == sizeof(struct jhd) && ((const uint8_t *)p)[0] == JHD_VERSION; }

/* appends what fits and keeps out a NUL-terminated string */
static void put(char *out, size_t cap, size_t *n, const char *fmt, ...)
{
    va_list ap;
    int r;
    if (*n + 1u >= cap)
        return;
    va_start(ap, fmt);
    r = vsnprintf(out + *n, cap - *n, fmt, ap);
    va_end(ap);
    if (r > 0)
        *n += (size_t)r < cap - *n ? (size_t)r : cap - *n - 1u;
}

/* the name, never past its field, with unprintable characters as '.'; "?" when empty */
static void clean_name(const struct jhd_dev *v, char out[JHD_NAME_MAX + 2])
{
    size_t i = 0;
    for (; i < sizeof v->name && v->name[i]; i++)
        out[i] = (v->name[i] >= 0x20 && v->name[i] < 0x7F) ? v->name[i] : '.';
    if (!i)
        out[i++] = '?';
    out[i] = 0;
}

size_t jhd_format(const struct jhd *d, char *out, size_t cap)
{
    size_t n = 0;
    if (!cap)
        return 0;
    out[0] = 0;
    put(out, cap, &n, "hostdiag_stack=%s\n", STACK[d->stack <= JHD_STACK_HOST ? d->stack : 0]);
    put(out, cap, &n, "hostdiag_lib=%s\n", d->lib_up ? "up" : "down");
    put(out, cap, &n, "hostdiag_events=new:%lu,gone:%lu,add:%lu\n", (unsigned long)d->new_dev, (unsigned long)d->gone_dev, (unsigned long)d->add_calls);
    put(out, cap, &n, "hostdiag_lp=lpx:%u,dn2:%u,programmer:%lu,pads:%lu,dn2msgs:%lu\n", d->lpx_up, d->dn2_up, (unsigned long)d->programmer_sent,
        (unsigned long)d->pads, (unsigned long)d->dn2_msgs);
    for (unsigned i = 0; i < JHD_DEVS; i++) {
        const struct jhd_dev *v = &d->dev[i];
        char name[JHD_NAME_MAX + 2];
        if (!v->used)
            continue;
        clean_name(v, name);
        put(out, cap, &n, "hostdiag_dev%u=%s,%04X:%04X,role:%s,ifs:%u,use:%u,out:0x%02X,in:0x%02X,claim:%ld,", i, name, v->vid, v->pid,
            ROLE[v->role < 3u ? v->role : 0], v->ifs, v->itf_index, v->out_ep, v->in_ep, (long)v->claim_err);
        put(out, cap, &n, "in_pkts:%lu,in_err:%lu,in_status:%u,out_done:%lu,out_fail:%lu,last:%02X %02X %02X %02X\n", (unsigned long)v->in_packets,
            (unsigned long)v->in_errors, v->in_last_status, (unsigned long)v->out_done, (unsigned long)v->out_fail, v->in_last[0], v->in_last[1],
            v->in_last[2], v->in_last[3]);
    }
    return n;
}
