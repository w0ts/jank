/* SPDX-License-Identifier: GPL-3.0-only */
/* Remembers the last panic across the restart, for `diag` (core/jank_panic.h). The chip has no console and no core dump
 * partition, so this is all that is left of a crash.
 *
 * The hook: the linker wraps esp_panic_handler (CMakeLists.txt, -Wl,--wrap=esp_panic_handler). ESP-IDF's port code calls it
 * once the registers are saved into an exception frame and the flash cache is back on; __wrap_ stores a short record into
 * RTC_NOINIT memory (survives the software reset, not power loss) and then calls the real handler, which prints, reboots
 * and behaves as before. The hook is IRAM (jank_panic.c's two pack/valid functions are too: linker.lf), makes no call
 * into flash code, no lock, no malloc and no printf: only reads of the frame, the task name and the clock. */
#include <string.h>
#include "app.h"
#include "esp_attr.h"
#include "esp_memory_utils.h"
#include "esp_private/panic_internal.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_panic.h"
#include "xtensa_context.h"

static RTC_NOINIT_ATTR jk_panic_t s_rtc;
static jk_panic_t s_last;
static int s_have;

extern void __real_esp_panic_handler(panic_info_t *info);

void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t *info)
{
    const XtExcFrame *f = (const XtExcFrame *)info->frame;
    const char *msg = g_panic_abort ? g_panic_abort_details : info->reason;   /* an abort() says why only in the details */
    if (msg && !esp_ptr_in_drom(msg) && !esp_ptr_in_dram(msg) && !esp_ptr_in_iram(msg))
        msg = NULL;                                                            /* a wild pointer is not read */
    if (f)
        jk_panic_pack(&s_rtc, f->pc, f->exccause, f->excvaddr, f->a0, f->a1, (uint32_t)info->core,
                      (uint32_t)(esp_timer_get_time() / 1000), pcTaskGetName(NULL), msg);
    __real_esp_panic_handler(info);
}

void panic_info_take(void)
{
    if (jk_panic_valid(&s_rtc)) {
        s_last = s_rtc;
        s_have = 1;
    }
    memset(&s_rtc, 0, sizeof s_rtc);                 /* each panic is handed over once; garbage is dropped too */
}

size_t panic_info_text(char *out, size_t cap)
{
    return jk_panic_text(s_have ? &s_last : NULL, out, cap);
}
