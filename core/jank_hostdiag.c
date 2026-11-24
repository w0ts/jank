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

static const char *step_name(uint8_t step)
{
    static const char *const NAME[] = {"none", "hostlib", "client", "client_task", "midi_task", "role", "radio", "bridge", "beat", "crash"};
    return step < sizeof NAME / sizeof NAME[0] ? NAME[step] : "?";
}

size_t jhd_format_failure(uint8_t step, int32_t err, char *out, size_t cap)
{
    size_t n = 0;
    if (!cap)
        return 0;
    out[0] = 0;
    if (step == JHD_STEP_NONE)
        put(out, cap, &n, "hostdiag_start=ok\n");
    else
        put(out, cap, &n, "hostdiag_start=failed:%s,err:%ld\n", step_name(step), (long)err);
    return n;
}

size_t jhd_format_trace(const struct jhd_mark *m, unsigned n, char *out, size_t cap)
{
    size_t len = 0;
    if (!cap)
        return 0;
    out[0] = 0;
    if (!n)
        return 0;
    if (n > JHD_TRACE_MAX)
        n = JHD_TRACE_MAX;
    put(out, cap, &len, "hostdiag_trace=");
    for (unsigned i = 0; i < n; i++)
        put(out, cap, &len, "%s%s:%lu/%lu", i ? "," : "", step_name(m[i].step), (unsigned long)m[i].free_b, (unsigned long)m[i].largest_b);
    put(out, cap, &len, "\n");
    return len;
}

size_t jhd_format(const struct jhd *d, char *out, size_t cap)
{
    size_t n = 0;
    if (!cap)
        return 0;
    out[0] = 0;
    put(out, cap, &n, "hostdiag_stack=%s\n", STACK[d->stack <= JHD_STACK_HOST ? d->stack : 0]);
    put(out, cap, &n, "hostdiag_lib=%s\n", d->lib_up ? "up" : "down");
    put(out, cap, &n, "hostdiag_libdevs=%u\n", d->lib_devs);
    /* HPRT (the DWC2 host port register): bit 0 device connected, bit 2 port enabled, bits 10-11 line state (0 SE0, 1 J, 2 K), bit 12 powered,
     * bits 17-18 speed (0 high, 1 full, 2 low). The line state says whether anything pulls D+ / D- at all, with no library in between. */
    put(out, cap, &n, "hostdiag_port=hprt:0x%08lx,conn:%lu,ena:%lu,lnsts:%lu,pwr:%lu,spd:%lu\n", (unsigned long)d->hprt, (unsigned long)(d->hprt & 1u),
        (unsigned long)((d->hprt >> 2) & 1u), (unsigned long)((d->hprt >> 10) & 3u), (unsigned long)((d->hprt >> 12) & 1u),
        (unsigned long)((d->hprt >> 17) & 3u));
    /* the enumeration rule (jank_hostfilter.h): a hub came, devices left alone (and the last one's IDs), the learned list ('-' when empty) */
    put(out, cap, &n, "hostdiag_filter=hub:%u,skipped:%lu,last:%04X:%04X,learned:", d->filter.hub_seen, (unsigned long)d->filter.skipped,
        d->filter.skip_vid, d->filter.skip_pid);
    for (unsigned i = 0; i < d->filter.deny.n && i < JHF_MAX; i++)
        put(out, cap, &n, "%s%04X:%04X", i ? "+" : "", d->filter.deny.id[i].vid, d->filter.deny.id[i].pid);
    put(out, cap, &n, "%s\n", d->filter.deny.n ? "" : "-");
    put(out, cap, &n, "hostdiag_events=new:%lu,gone:%lu,add:%lu\n", (unsigned long)d->new_dev, (unsigned long)d->gone_dev, (unsigned long)d->add_calls);
    put(out, cap, &n, "hostdiag_lp=lpx:%u,dn2:%u,programmer:%lu,acked:%lu,setup:%lu,mode:%s,reentered:%u,lights:%lu,pads:%lu,dn2msgs:%lu\n", d->lpx_up,
        d->dn2_up, (unsigned long)d->programmer_sent, (unsigned long)d->programmer_acked, (unsigned long)d->setup_sent,
        d->lp_mode == 2u ? "programmer" : d->lp_mode == 1u ? "live" : "unknown", d->lp_reentered, (unsigned long)d->lights_sent, (unsigned long)d->pads,
        (unsigned long)d->dn2_msgs);
    {
        static const char *const live[] = {"down", "asking", "live", "remote_off", "no_live"};
        put(out, cap, &n, "hostdiag_live=state:%s,play:%u,asked:%lu,replies:%lu,plays:%lu,selects:%lu,pushes:%lu,notes:%lu,dropped:%lu,reads:%lu,msgs:%lu\n",
            d->live_state < sizeof live / sizeof live[0] ? live[d->live_state] : "?", d->live_play, (unsigned long)d->live_asked, (unsigned long)d->live_replies,
            (unsigned long)d->live_plays, (unsigned long)d->live_selects, (unsigned long)d->live_pushes, (unsigned long)d->live_notes,
            (unsigned long)d->live_dropped, (unsigned long)d->live_reads, (unsigned long)d->live_msgs);
    }
    put(out, cap, &n, "hostdiag_steps=edits:%lu,reads:%lu,busy:%lu,refused:%lu\n", (unsigned long)d->live_edits, (unsigned long)d->live_pagereads,
        (unsigned long)d->live_step_busy, (unsigned long)d->live_step_refused);
    put(out, cap, &n, "hostdiag_dn2live=proto:%u,push:%u,lease:%u,made:%lu,sent:%lu,lost:%lu,dropped:%lu,pushmsgs:%lu,ignored:%lu,last_ignored_len:%u\n",
        d->dn2_proto, d->dn2_push, d->dn2_lease, (unsigned long)d->dn2_made, (unsigned long)d->dn2_sent, (unsigned long)d->dn2_lost,
        (unsigned long)d->dn2_dropped, (unsigned long)d->dn2_pushmsgs, (unsigned long)d->live_ignored, d->live_ign_len);
    put(out, cap, &n, "hostdiag_mem=heap:%u,heap_min:%u,largest:%lu,client_stack_free:%u\n", d->heap_kb, d->heap_min_kb, (unsigned long)d->heap_big_b,
        d->client_stack_free);
    for (unsigned i = 0; i < JHD_DEVS; i++) {
        const struct jhd_dev *v = &d->dev[i];
        char name[JHD_NAME_MAX + 2];
        if (!v->used)
            continue;
        clean_name(v, name);
        put(out, cap, &n, "hostdiag_dev%u=%s,%04X:%04X,role:%s,ifs:%u,use:%u,out:0x%02X,in:0x%02X,mps:%u/%u,eps:%u,clamp:%u,claim:%ld,where:%u,", i, name, v->vid,
            v->pid, ROLE[v->role < 3u ? v->role : 0], v->ifs, v->itf_index, v->out_ep, v->in_ep, v->out_mps, v->in_mps, v->neps, v->clamped,
            (long)v->claim_err, v->claim_where);
        put(out, cap, &n, "in_pkts:%lu,in_err:%lu,in_status:%u,out_done:%lu,out_fail:%lu,last:%02X %02X %02X %02X\n", (unsigned long)v->in_packets,
            (unsigned long)v->in_errors, v->in_last_status, (unsigned long)v->out_done, (unsigned long)v->out_fail, v->in_last[0], v->in_last[1],
            v->in_last[2], v->in_last[3]);
    }
    return n;
}
