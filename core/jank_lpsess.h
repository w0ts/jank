/* SPDX-License-Identifier: GPL-3.0-only */
/* The Launchpad's Session page (owner 2026-11-25): the Digitone's 128 patterns, 8 banks (A..H) of 16. Two pad rows per bank, four banks a page, from the top: A01..A08,
 * A09..A16, then B, C, D; up / down switch to banks E..H and back. Lights as Launchpad 95's session skin (SkinMK2.py Session): the playing pattern GREEN (21) PULSING, a
 * cued one GREEN FLASHING, a pattern that holds trigs RED_THIRD (7), an empty pattern (or one not read yet) dark.
 *
 * What it knows comes from the Digitone: PATTERNS (51, jank_dn2live.h) per bank, read once for all 8 banks when the page opens (one request every JLSS_OPEN_GAP_MS), then only
 * the playing pattern's bank every JLSS_POLL_MS while the page is shown, nothing while it is closed (each bank costs the Digitone up to about 0.8 ms in its MIDI task); and
 * PATTERN's (60, provisional) answer or read: the cued pattern. A press cues the pattern (PATTERN how 0): it flashes at once, and stops flashing when the Digitone says another
 * cue or none, or when the pattern plays. Pure; host-tested (tests/test_lpsess.c). */
#ifndef JANK_LPSESS_H
#define JANK_LPSESS_H
#include <stdint.h>
#include "jank_dn2live.h"
#include "jank_lpx.h"

#define JLSS_HALVES 2u                 /* the two pages: banks A..D, E..H */
#define JLSS_BANKS_SHOWN 4u
#define JLSS_OPEN_GAP_MS 60u           /* the opening reads, one bank each (the step modes' quick reads are 60 ms apart too) */
#define JLSS_POLL_MS 500u              /* then the playing pattern's bank, this often */
#define JLSS_COLOR_PLAYING 21u         /* GREEN, pulsing (Session.ClipStarted) */
#define JLSS_COLOR_CUED 21u            /* GREEN, flashing with dark (Session.ClipTriggeredPlay) */
#define JLSS_COLOR_FILLED 7u           /* RED_THIRD: a pattern that holds trigs */
#define JLSS_UNKNOWN 255u

/* what to ask the Digitone now (jlss_due) */
enum jlss_req_kind { JLSS_REQ_NONE, JLSS_REQ_BANK, JLSS_REQ_CUE_READ };
struct jlss_req {
    uint8_t kind;                      /* enum jlss_req_kind */
    uint8_t bank;                      /* JLSS_REQ_BANK: 0..7, or JDL_BANK_PLAYING */
};

struct jlss {
    uint16_t mask[JDL_BANKS];          /* bit k: pattern 16 x bank + k holds a trig */
    uint8_t have;                      /* bit b: bank b has been read */
    uint8_t playing, cued;             /* JLSS_UNKNOWN / JDL_NO_PATTERN: none known */
    uint8_t half;                      /* 0: banks A..D, 1: E..H */
    uint8_t sweep;                     /* the next bank of the opening reads; JDL_BANKS when they are done */
    uint8_t cue_due;                   /* a PATTERN read goes with the next request */
    uint8_t retry_due;                 /* ... and a bank that never answered, after it */
    uint8_t asked;                     /* a request went at last_ms since the page opened */
    uint32_t last_ms;
};

void jlss_init(struct jlss *s);
/* the page opens (or live control came back): every bank is read again, then PATTERN is read once (a build without it says so at once, not at the first press) */
void jlss_open(struct jlss *s);
/* live control went away: nothing known is shown any more */
void jlss_forget(struct jlss *s);
/* The next request due at now_ms (1 and *r filled; the caller sends it and may ask again at once), else 0. `reads_ok`: PATTERNS has not been refused; `cue_ok`: PATTERN has not
 * been refused. The opening reads, one bank per JLSS_OPEN_GAP_MS; then every JLSS_POLL_MS the playing bank, the PATTERN read right after it, and one bank that never answered */
int jlss_due(struct jlss *s, uint32_t now_ms, int reads_ok, int cue_ok, struct jlss_req *r);
/* the Digitone's answers: a bank (PATTERNS), the playing and cued patterns (PATTERN) */
void jlss_on_patterns(struct jlss *s, const struct jdl_patterns *p);
void jlss_on_pattern(struct jlss *s, const struct jdl_pattern *p);
/* the pattern of pad (x, y from the bottom) on the half shown, 0..127 */
uint8_t jlss_pattern_of_pad(const struct jlss *s, uint8_t x, uint8_t y);
/* a press cued `pattern`: it flashes until the Digitone says otherwise */
void jlss_cue(struct jlss *s, uint8_t pattern);
/* up (-1) / down (+1): the half; 1 when it changed, 0 at the limit */
int jlss_half_step(struct jlss *s, int dir);
/* "A01" .. "H16" into out (4 bytes) */
void jlss_name(uint8_t pattern, char out[4]);
/* the 64 pads (led[JLX_PAD0 ..]) */
void jlss_frame(const struct jlss *s, struct jlx_frame *f);
#endif
