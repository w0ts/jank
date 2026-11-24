/* SPDX-License-Identifier: GPL-3.0-only */
/* Why the unit restarted (app.h restart_why_set): every place that restarts the unit says so in RTC memory first, and the next boot reports it (`diag`: last_restart=).
 * Three Wi-Fi updates rolled back within seconds of starting (2026-11-24) and nothing could say which restart did it. */
#include <string.h>
#include "app.h"
#include "esp_attr.h"

#define MAGIC 0x52535457u              /* "RSTW" */
#define WHY_MAX 24

static RTC_NOINIT_ATTR uint32_t s_magic;
static RTC_NOINIT_ATTR char s_why[WHY_MAX];
static char s_last[WHY_MAX] = "none";  /* what the last boot found */

void restart_why_set(const char *why)
{
    strncpy(s_why, why, WHY_MAX - 1);
    s_why[WHY_MAX - 1] = 0;
    s_magic = MAGIC;
}

void restart_why_default(const char *why)
{
    if (s_magic != MAGIC)
        restart_why_set(why);
}

void restart_why_boot(void)
{
    if (s_magic == MAGIC) {
        s_why[WHY_MAX - 1] = 0;
        memcpy(s_last, s_why, WHY_MAX);
    } else {
        strcpy(s_last, "none");        /* a power-on, a panic or a watchdog: nobody said anything */
    }
    s_magic = 0;
}

const char *restart_why_last(void) { return s_last; }
