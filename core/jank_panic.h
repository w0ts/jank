/* SPDX-License-Identifier: GPL-3.0-only */
/* The last panic, kept across the restart (RTC memory, firmware/main/panic_info.c) and told on the control line's `diag`.
 * The pure half: the record's layout, its checksum and the `diag` text. 92 bytes. */
#ifndef JANK_PANIC_H
#define JANK_PANIC_H
#include <stddef.h>
#include <stdint.h>

#define JK_PANIC_MAGIC 0x4A4B5041u        /* "JKPA" */
#define JK_PANIC_TASK 16u                 /* task name bytes (FreeRTOS configMAX_TASK_NAME_LEN) */
#define JK_PANIC_MSG 40u                  /* panic text bytes, the last one always 0 */

typedef struct {
    uint32_t magic;                       /* JK_PANIC_MAGIC while the record holds a panic */
    uint32_t sum;                         /* 32-bit sum of all the other words, plus a constant */
    uint32_t pc;                          /* the faulting address */
    uint32_t cause;                       /* EXCCAUSE */
    uint32_t vaddr;                       /* EXCVADDR */
    uint32_t a0;                          /* return address register */
    uint32_t sp;                          /* a1 */
    uint32_t up_ms;                       /* uptime at the panic */
    uint32_t core;
    char task[JK_PANIC_TASK];             /* the task that ran, 0 padded; the last byte always 0 */
    char msg[JK_PANIC_MSG];               /* the panic text cut to fit; 0 padded; the last byte always 0 */
} jk_panic_t;

/* Fill r (all of it: no stale bytes). task and msg may be NULL; they are cut to fit. No library calls, no flash: the
 * panic hook (IRAM) runs these. */
void jk_panic_pack(jk_panic_t *r, uint32_t pc, uint32_t cause, uint32_t vaddr, uint32_t a0, uint32_t sp, uint32_t core,
                   uint32_t up_ms, const char *task, const char *msg);
/* 1 when r holds a record that was packed whole (magic and checksum right), else 0 (garbage after power loss). */
int jk_panic_valid(const jk_panic_t *r);
/* The `diag` lines for r ("panic_pc=0x4200abcd\n" ...), nothing for NULL. Task and text are cut to printable ASCII. Returns
 * the length; the text always ends in a whole line. */
size_t jk_panic_text(const jk_panic_t *r, char *out, size_t cap);

/* The task name as printable ASCII (anything else becomes '?'), at most JK_PANIC_TASK - 1 characters. */
void jk_panic_task(const jk_panic_t *r, char out[JK_PANIC_TASK]);
/* The short form for the `about` line: "<task>@0x4200abcd" (the pc). Returns the length; 0 when it does not fit. */
size_t jk_panic_brief(const jk_panic_t *r, char *out, size_t cap);
/* PANIC.TXT on the drive: plain text with CR LF line ends (when, task, EXCCAUSE and its name, pc / a0 / sp / vaddr, the message,
 * the firmware version, the addr2line line). Returns the length; 0 for NULL or when it does not fit (JK_PANIC_FILE_MAX is enough). */
#define JK_PANIC_FILE_MAX 768u
size_t jk_panic_file(const jk_panic_t *r, const char *version, char *out, size_t cap);

#endif
