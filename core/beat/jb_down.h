/* SPDX-License-Identifier: GPL-3.0-only */
/* Which beat is the 1: the downbeat estimator, a port of beat-listener's live/downbeat.py (the second downbeat study's
 * rule), checked against it by tests/test_down.c on golden vectors (tests/down_ref/, to 1e-4).
 *
 * It counts Link's own beats: the tracker's grid (jb_mode_grid after each tracked beat, a beat at tb then every period),
 * one count per grid beat played, a jump of the grid moving the count by its gap rounded. At each counted beat it takes
 * 22 accent features from the tracker's 81 log bands (jb_fe's frame) and keeps 22 x 4 accumulators (feature, bar
 * position = count mod 4) with an exponential memory of 16 bars. The score of "the 1 is at position r" is
 * sum over f, p of down_w[f * 4 + p] * rel_f((r + p) mod 4), rel = (acc - mean of the 4) / (|mean| + 1e-6).
 *
 * Causal: beat j's features need the 4 beats after it; at each beat the decision comes first, then beat j = i - 4
 * enters (its features weigh in 5 beats later).
 *
 * Windows: peaks (max, and e150's mean) over the frames k - 5 .. k + 10 of the beat's frame k; each grid beat opens a
 * segment (its frames up to the next grid beat's): level novelty = segment j against j - 1, bar novelty = segments
 * j .. j + 3 against j - 4 .. j - 1 (only segments that join without a gap count as "before"), group means of segment j.
 * The segments run on every grid beat: also ModeBpm's provisional grid before it is steady ("shadow" beats, not
 * counted) and across resets.
 *
 * States: OFF (AUTO BAR off), LISTENING, LOCKED(phi), MANUAL (a user nudge: no move until a reset or AUTO BAR picked
 * again). The first move when the best phase leads the second by DOWN_COMMIT; a later one when another phase leads the
 * current one by DOWN_SWITCH for DOWN_HOLD beats in a row. A reset clears the count, the accumulators and the state.
 *
 * Portable C, float32 (the ESP32-S3 has a single-precision FPU; times are doubles, a few a second), no allocation.
 * RAM: sizeof(struct jb_down) (about 2 KB) plus the caller's segment ring, JD_SEGS x 81 floats (2.9 KB, PSRAM is fine). */
#ifndef JB_DOWN_H
#define JB_DOWN_H
#include <stdint.h>
#include "down_model.h"

#define JD_BANDS 81
#define JD_NF DOWN_FEATURES
#define JD_FPS 100
#define JD_PRE 5                       /* the peak window, frames before and after the beat's frame */
#define JD_POST 10
#define JD_RING (JD_PRE + JD_POST + 1)
#define JD_CURVES 10                   /* e150, e100, flux150, flux_all, flux_g0..5 */
#define JD_PEAKS 11                    /* e150, e100, e150_mean, flux150, flux_all, flux_g0..5 */
#define JD_SEGS 9                      /* segments j-4 .. j+4 around the beat j = i - 4 */
#define JD_BEATS 8                     /* counted beat records */
#define JD_DELAY 4

enum jd_state { JD_OFF = 0, JD_LISTENING = 1, JD_LOCKED = 2, JD_MANUAL = 3 };

typedef float jd_seg_row[JD_BANDS];

struct jb_down {
    jd_seg_row *seg;                   /* JD_SEGS rows of band sums: the caller's memory */
    /* the frame stream */
    float prev[JD_BANDS];
    uint8_t have_prev;
    float ring[JD_RING][JD_CURVES];
    int64_t first_frame;               /* -1: none yet */
    /* the grid beat stream and its segments (they survive resets) */
    uint8_t has_grid, running, open, has_last;
    double tb, period, next, b_last;
    int64_t k, next_frame;
    int64_t b_last_frame, index;       /* the frame of the last grid beat; the last frame taken */
    uint32_t sn;                       /* segments opened; segment m at m % JD_SEGS */
    int32_t seg_n[JD_SEGS];
    int64_t seg_start[JD_SEGS];
    /* counting, since the last reset */
    uint32_t n;
    int32_t c_last;
    int32_t rec_count[JD_BEATS];
    double rec_time[JD_BEATS];
    uint32_t rec_seg[JD_BEATS];
    int64_t rec_frame[JD_BEATS];
    uint8_t rec_peak_ok[JD_BEATS];
    float rec_peak[JD_BEATS][JD_PEAKS];
    float acc[JD_NF][4];
    uint32_t acc_n[4];
    /* the decision */
    uint8_t auto_on, state, has_scores;
    int8_t phi, cand;                  /* -1: none */
    int32_t run;
    float scores[4], lead;
    uint32_t seq, moves;
    int32_t dec_count;
    double dec_time;
    /* the beat that entered last (tests, the golden check) */
    uint32_t entered;
    int32_t x_count;
    double x_time;
    float x[JD_NF];
};

struct jd_view {
    uint8_t state;                     /* enum jd_state */
    int8_t phi;                        /* LOCKED: the count position (count mod 4) of the 1; else -1 */
    uint8_t ready;                     /* the scores exist (every bar position has a beat) */
    float lead;                        /* LOCKED: the 1's score over the best other; else the best over the second */
    float conf;                        /* 0..1: the lead against the commit level (DOWN_COMMIT) */
    uint32_t seq, moves;               /* decisions taken (one a counted beat), moves asked for */
    int32_t count;                     /* the beat of the last decision: its count, stream time */
    double t_beat;
    float scores[4];
};

void jd_init(struct jb_down *d, jd_seg_row *seg);
/* a reset event: the count, the accumulators and the state (MANUAL too); the frame and segment memory stays */
void jd_reset(struct jb_down *d);
/* AUTO BAR: off -> OFF; on while OFF or MANUAL -> LISTENING (picked again) */
void jd_set_auto(struct jb_down *d, int on);
/* the user nudged the beat: MANUAL (AUTO BAR on) */
void jd_nudge(struct jb_down *d);
/* after a tracked beat at t: the grid, a beat at tb and every period; running: the tracker's mode is not stopped (its
 * beats are counted), else shadow beats. -1: a bad period */
int jd_grid(struct jb_down *d, double t, double tb, double period, int running);
/* no grid: nothing played, the open segment closes; a reset when beats were counted */
void jd_stop(struct jb_down *d);
int jd_counting(const struct jb_down *d);
int jd_playing(const struct jb_down *d);
/* one frame of 81 log bands, frame `index` at index / JD_FPS s. 1: a counted beat (a decision) at this frame */
int jd_frame(struct jb_down *d, int64_t index, const float *bands);
void jd_view(const struct jb_down *d, struct jd_view *v);
#endif
