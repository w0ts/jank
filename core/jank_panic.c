/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_panic.h"
#include <stdio.h>

#define SUM_SEED 0x5A17C0DEu

static uint32_t sum_of(const jk_panic_t *r)
{
    const uint32_t *w = (const uint32_t *)(const void *)r;
    uint32_t s = SUM_SEED;
    for (size_t i = 2; i < sizeof *r / 4; i++)      /* words 0 (magic) and 1 (sum) are not summed */
        s += w[i] * 2654435761u + (uint32_t)i;
    return s;
}

static void copy_cut(char *dst, size_t n, const char *src)
{
    size_t i = 0;
    if (src)
        for (; i + 1 < n && src[i]; i++)
            dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;
}

void jk_panic_pack(jk_panic_t *r, uint32_t pc, uint32_t cause, uint32_t vaddr, uint32_t a0, uint32_t sp, uint32_t core,
                   uint32_t up_ms, const char *task, const char *msg)
{
    r->pc = pc;
    r->cause = cause;
    r->vaddr = vaddr;
    r->a0 = a0;
    r->sp = sp;
    r->up_ms = up_ms;
    r->core = core;
    copy_cut(r->task, sizeof r->task, task);
    copy_cut(r->msg, sizeof r->msg, msg);
    r->sum = sum_of(r);
    r->magic = JK_PANIC_MAGIC;                      /* last: a record cut short by the reset is never valid */
}

int jk_panic_valid(const jk_panic_t *r)
{
    return r->magic == JK_PANIC_MAGIC && r->sum == sum_of(r) && !r->task[JK_PANIC_TASK - 1] && !r->msg[JK_PANIC_MSG - 1];
}

static void printable(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i + 1 < n && src[i]; i++)
        dst[i] = (src[i] >= 0x20 && src[i] < 0x7F) ? src[i] : '?';
    dst[i] = 0;
}

size_t jk_panic_text(const jk_panic_t *r, char *out, size_t cap)
{
    char task[JK_PANIC_TASK], msg[JK_PANIC_MSG];
    int n;
    if (!cap)
        return 0;
    out[0] = 0;
    if (!r)
        return 0;
    printable(task, r->task, sizeof task);
    printable(msg, r->msg, sizeof msg);
    n = snprintf(out, cap, "panic_pc=0x%08x\npanic_cause=%u\npanic_vaddr=0x%08x\npanic_a0=0x%08x\npanic_sp=0x%08x\npanic_core=%u\n"
                 "panic_task=%s\npanic_up_ms=%u\npanic_msg=%s\n",
                 (unsigned)r->pc, (unsigned)r->cause, (unsigned)r->vaddr, (unsigned)r->a0, (unsigned)r->sp, (unsigned)r->core,
                 task, (unsigned)r->up_ms, msg);
    if (n < 0 || (size_t)n >= cap) {                /* does not fit: nothing rather than half a line */
        out[0] = 0;
        return 0;
    }
    return (size_t)n;
}
