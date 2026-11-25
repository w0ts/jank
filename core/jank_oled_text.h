/* SPDX-License-Identifier: GPL-3.0-only */
/* The device's own UI as lines of text, for a Digitone II's screen (Wishes live control, JANK PAGE 62; owner 2026-11-25: text in the
 * DN2's own font, not a bitmap; driven by the DN2's keys; all of jank's menu items).
 *
 * jot_render turns the state jo_draw draws (jank_oled.h: the screen, the menus' cursors, the overlays) into one page of up to 7 lines:
 * the same content in the same order as the OLED's screen, line 0 the title where the screen has a title bar, the OLED's inverted row
 * as `sel`, the character editor's cursor as a column of that row. The menus are the ones jo_press walks (c->layout_lcd2 picks them, as
 * there). Pure: no ESP-IDF, host-tested (tests/test_oled_text.c).
 *
 * The DN2's small font is proportional (capitals and digits 4 px with the gap, a space only 2): any 21 characters fit its width, up to 31
 * when most are capitals or digits. So the text carries no padding: where the OLED centres a line or puts a value on the right, the page
 * says so beside the text (centred, right), and the text alone reads left-aligned with two spaces before the value. Lines keep to 21
 * characters where the OLED's own text fits; every character is 0x20..0x7e (anything else becomes '?'). */
#ifndef JANK_OLED_TEXT_H
#define JANK_OLED_TEXT_H
#include <stdint.h>
#include "jank_oled.h"

#define JOT_LINES 7                       /* lines on the DN2's page */
#define JOT_LINE_MAX 31                   /* the longest line it takes */
#define JOT_FIT 21                        /* any 21 characters always fit */
#define JOT_NONE 255u                     /* sel / cursor: none */
#define JOT_F_CLOSE 0x01u                 /* flags: the page closes (the host's CLOSE) */
#define JOT_F_TITLE 0x02u                 /* flags: line 0 is a title */

struct jot_page {
    uint8_t flags;                        /* JOT_F_* */
    uint8_t sel;                          /* the inverted line, JOT_NONE none */
    uint8_t cursor;                       /* the TEXT editor's cursor: a column of line `sel` (only that cell inverted), JOT_NONE none */
    uint8_t n;                            /* lines sent, 0..JOT_LINES (trailing empty lines are not) */
    uint8_t centred;                      /* bit i: line i is centred on the OLED */
    uint8_t right[JOT_LINES];             /* where line i's right-aligned part starts (after two spaces), 0 none */
    char line[JOT_LINES][JOT_LINE_MAX + 1];
};

/* the page for this state: the screen, its overlays and the SAVED box, as jo_draw would show them at now */
void jot_render(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c, uint32_t now);
/* 1 when two pages would draw the same (a page is sent only when it changes) */
int jot_equal(const struct jot_page *a, const struct jot_page *b);
#endif
