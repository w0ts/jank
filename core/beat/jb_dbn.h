/* SPDX-License-Identifier: GPL-3.0-only */
/* madmom's online beat DBN (DBNBeatTrackingProcessor.process_online), tempo range narrowed: the forward algorithm over
 * a state space of one position per frame of the beat interval, for every interval in the range; per frame the most
 * probable state is a beat when it lies in the first 1/16 of a beat. Transitions: within a beat the position steps on;
 * at the end the interval may change, with exp(-100 |interval ratio - 1|); observation lambda 16. A beat is reported
 * only when it is at least 60 / max_bpm after the last one. Float32 throughout (madmom: float64). 90..180 BPM at 100
 * fps is 35 intervals (33..67 frames) and 1750 states: 14 KB of state. */
#ifndef JB_DBN_H
#define JB_DBN_H
#include <stdint.h>

#define JB_DBN_MAX_INTERVALS 40
#define JB_DBN_MAX_STATES 2048

struct jb_dbn {
    float fwd[JB_DBN_MAX_STATES], next[JB_DBN_MAX_STATES];
    float trans[JB_DBN_MAX_INTERVALS][JB_DBN_MAX_INTERVALS];     /* from the last state of interval j to the first of i */
    uint16_t interval[JB_DBN_MAX_INTERVALS];                     /* frames */
    uint16_t first[JB_DBN_MAX_INTERVALS];                        /* first state of each interval */
    uint16_t zone[JB_DBN_MAX_INTERVALS];                         /* leading states of an interval that count as a beat */
    uint16_t n_intervals, n_states;
    uint8_t is_beat[JB_DBN_MAX_STATES];
    uint32_t counter;                                            /* frames seen */
    double last_beat, min_gap, fps;                              /* seconds */
};

/* 0 ok, -1 range or size not supported */
int jb_dbn_init(struct jb_dbn *d, double min_bpm, double max_bpm, double fps);
void jb_dbn_reset(struct jb_dbn *d);
/* One frame's activation. Returns 1 when it makes a beat, at *t seconds. */
int jb_dbn_step(struct jb_dbn *d, float act, double *t);

#endif
