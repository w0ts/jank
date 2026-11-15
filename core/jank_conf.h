/* SPDX-License-Identifier: GPL-3.0-only */
/* LOW CONFIDENCE: how much to trust the published tempo, and the gate that stops the outputs when it is low. A port of
 * beat-listener's live/confidence.py (grid_confidence) and of the gate in live/engine.py (Engine.status).
 *
 * The confidence. Over the last JCF_WINDOW_S seconds (minus the tracker's own JCF_DELAY_S), ModeBpm's grid (jb_mode_grid:
 * a beat and the period) predicts beat slots. A slot is hit when a tracked beat lies within JCF_TOL of a period from it;
 * a tracked beat near no slot is an extra. The confidence is the F-measure of hits (recall: missed beats lower it) against
 * extras (precision: off-grid beats lower it), 0..1. A break with no beats, a tempo the grid no longer fits, or a tracker
 * that wanders all bring it down within a few seconds. No sound: 0 (the Python's silence).
 *
 * The gate (the user's AUDIO > LOW CONFIDENCE = STOP / START, THRESHOLD in percent):
 *   not gated  -> gated   when the confidence is under the threshold
 *   gated      -> running when it is back at the threshold + JCF_HYST_PCT (capped at 100 %)
 * KEEP RUNNING, or a threshold of 0: never gated. Why 5 points of hysteresis (the Python's HYSTERESIS = 0.05): the
 * F-measure moves in steps of one beat in the window (8 s at 90..180 BPM holds 12..24 slots: one beat hit or missed moves
 * it by 2..4 points), so a gate with no band would flap on every beat a track sits near the threshold; 5 points is more
 * than one beat's step at any tempo of the range, yet small enough that a track that is really back (locked tracks measure
 * 0.94-0.97, wrong tempos ~0.4: player.py DEFAULT_MIN_CONFIDENCE) restarts at once.
 *
 * Plain C with doubles, no allocation; times are the tracker's stream seconds. The Python and this agree to rounding
 * (tests/test_conf.c against tests/conf_ref, written by beat-listener's scripts/confidence_golden.py). */
#ifndef JANK_CONF_H
#define JANK_CONF_H
#include <stddef.h>
#include <stdint.h>

#define JCF_WINDOW_S 8.0
#define JCF_DELAY_S 0.1
#define JCF_TOL 0.1                     /* of a period */
#define JCF_HYST_PCT 5                  /* the gate opens again this many points above the threshold */
#define JCF_THRESHOLD_DEFAULT 80        /* percent (player.py DEFAULT_MIN_CONFIDENCE 0.8, the emulator's THRESHOLD) */
/* tracked beats kept: the window needs (8 s + 2 margins) / the tracker's shortest period (180 BPM, 0.33 s) = 26; the rest
 * is room for a tracker that beats faster than its range in a short burst */
#define JCF_KEEP 64
/* SILENCE (engine.py SILENCE_RMS, SILENCE_S): the input under -60 dBFS RMS (1e-3 of full scale) for 0.5 s stops the
 * outputs at once, whatever LOW CONFIDENCE says, and the beats kept go (what comes after silence is new music). The
 * mean square of int16 samples under (1e-3 * 32768)^2 is quiet. */
#define JCF_RATE 44100u
#define JCF_SILENCE_SAMPLES (JCF_RATE / 2u)
#define JCF_QUIET_MS_INT16 1073.741824  /* (1e-3 * 32768)^2 */

struct jcf {
    double t[JCF_KEEP];                 /* the tracker's beat times, ascending, oldest first */
    uint16_t n;
    uint32_t quiet;                     /* samples of quiet in a row (capped) */
    uint8_t silent;                     /* quiet for JCF_SILENCE_SAMPLES: confidence 0, no beats kept */
    uint32_t silences;                  /* silences begun (for `beat`) */
};

/* grid_confidence itself, on any ascending array of beat times: the grid is (t0, period), has_grid 0 for None. 0..1 */
double jcf_grid_confidence(const double *beats, size_t n, int has_grid, double t0, double period, double now,
                           double window_s, double delay_s, double tol);

void jcf_init(struct jcf *c);
/* a block of n samples of the tracker's input with the sum of their squares (int16 units); n samples' time with nothing
 * sent (a stalled stream) is a block of 0. Returns 1 at the silence's onset (the beats kept are gone) */
int jcf_samples(struct jcf *c, uint64_t sumsq, size_t n);
/* a tracked beat (its stream time; kept only while not silent and when later than the last one) */
void jcf_beat(struct jcf *c, double t);
/* the confidence now with the defaults (JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL); silent: 0 */
double jcf_confidence(const struct jcf *c, int has_grid, double t0, double period, double now);
/* 0..1 as percent, rounded (the status, the HOME line) */
uint8_t jcf_pct(double conf);

/* LOW CONFIDENCE: KEEP RUNNING (never gated) or STOP / START (gated under the threshold) */
enum jcf_lowconf { JCF_RUN = 0, JCF_STOP = 1 };
/* the gate's next state from the last one (0 / 1), the confidence 0..1, the setting and the threshold in percent */
uint8_t jcf_gate(uint8_t gated, double conf, uint8_t lowconf, uint8_t threshold_pct);
#endif
