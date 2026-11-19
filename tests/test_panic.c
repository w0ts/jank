/* SPDX-License-Identifier: GPL-3.0-only */
/* The last panic's record: pack, validate, the `diag` text. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "jank_core.h"
#include "jank_panic.h"

_Static_assert(sizeof(jk_panic_t) == 92, "the record stays well under 128 bytes of RTC memory");

static void fill(jk_panic_t *r)
{
    memset(r, 0xA5, sizeof *r);                      /* stale bytes must not survive the pack */
    jk_panic_pack(r, 0x4200abcdu, 28, 0x00000010u, 0x4201beefu, 0x3fc9a120u, 1, 123456, "ui", "Cache disabled but cached memory region accessed");
}

int main(void)
{
    jk_panic_t r, z;
    char out[JK_CTL_OUT_MAX], small[40];
    size_t n;
    fill(&r);
    assert(jk_panic_valid(&r));
    assert(r.pc == 0x4200abcdu && r.cause == 28 && r.core == 1 && r.sp == 0x3fc9a120u && r.up_ms == 123456);
    assert(!strcmp(r.task, "ui"));
    assert(strlen(r.msg) == JK_PANIC_MSG - 1 && !memcmp(r.msg, "Cache disabled but cached memory region", JK_PANIC_MSG - 1));   /* cut to 39 + 0 */

    /* garbage and a record cut short are refused */
    memset(&z, 0, sizeof z);
    assert(!jk_panic_valid(&z));
    memset(&z, 0xA5, sizeof z);
    assert(!jk_panic_valid(&z));
    for (size_t i = 0; i < sizeof r; i++) {          /* one flipped bit anywhere */
        jk_panic_t c = r;
        ((unsigned char *)&c)[i] ^= 0x10;
        assert(!jk_panic_valid(&c));
    }
    z = r;
    z.magic = 0;                                     /* the cleared record (what the boot leaves in RTC memory) */
    assert(!jk_panic_valid(&z));
    z = r;
    z.msg[JK_PANIC_MSG - 1] = 'x';                   /* an unterminated text cannot be valid even with a fixed sum */
    z.sum = r.sum;
    assert(!jk_panic_valid(&z));

    /* strings: NULL, empty, exactly full, too long, 16-char task */
    jk_panic_pack(&z, 1, 2, 3, 4, 5, 0, 6, NULL, NULL);
    assert(jk_panic_valid(&z) && !z.task[0] && !z.msg[0]);
    jk_panic_pack(&z, 1, 2, 3, 4, 5, 0, 6, "0123456789abcdefXYZ", "");
    assert(jk_panic_valid(&z) && !strcmp(z.task, "0123456789abcde"));

    /* the diag text */
    n = jk_panic_text(&r, out, sizeof out);
    assert(n == strlen(out) && out[n - 1] == '\n');
    assert(strstr(out, "panic_pc=0x4200abcd\n") && strstr(out, "panic_cause=28\n") && strstr(out, "panic_vaddr=0x00000010\n"));
    assert(strstr(out, "panic_a0=0x4201beef\n") && strstr(out, "panic_sp=0x3fc9a120\n") && strstr(out, "panic_core=1\n"));
    assert(strstr(out, "panic_task=ui\n") && strstr(out, "panic_up_ms=123456\n") && strstr(out, "panic_msg=Cache disabled but cached memory region\n"));
    assert(n < 300);                                 /* diag stays short, far under JK_CTL_OUT_MAX */
    assert(jk_panic_text(NULL, out, sizeof out) == 0 && !out[0]);   /* no panic: nothing at all */
    assert(jk_panic_text(&r, small, sizeof small) == 0 && !small[0]);   /* no room: no half line */
    jk_panic_pack(&z, 1, 2, 3, 4, 5, 0, 6, "a\nb=c", "x\ty\n");   /* control characters cannot forge a line */
    n = jk_panic_text(&z, out, sizeof out);
    assert(strstr(out, "panic_task=a?b=c\n") && strstr(out, "panic_msg=x?y?\n"));
    printf("panic ok\n");
    return 0;
}
