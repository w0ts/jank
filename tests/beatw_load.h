/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests: bind the beat network's weights from the blob file BEAT_WEIGHTS names (tests/run.sh makes it from madmom's
 * model with scripts/beat_weights.js). No file or a bad one: the test stops here, saying why. */
#ifndef BEATW_LOAD_H
#define BEATW_LOAD_H
#include <stdio.h>
#include <stdlib.h>
#include "beat_weights.h"

static void beatw_load(void)
{
    const char *path = getenv("BEAT_WEIGHTS");
    if (!path) {
        fprintf(stderr, "BEAT_WEIGHTS is not set: the weights blob (tests/run.sh makes it)\n");
        exit(2);
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "BEAT_WEIGHTS %s: cannot open\n", path);
        exit(2);
    }
    static float buf[(BEAT_WEIGHTS_HEADER + 4u * BEAT_WEIGHTS_COUNT) / sizeof(float) + 1];   /* float-aligned */
    const size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    const enum beat_weights_err e = beat_weights_bind(buf, n);
    if (e != BW_OK) {
        fprintf(stderr, "BEAT_WEIGHTS %s: %s\n", path, beat_weights_why(e));
        exit(2);
    }
}

#endif
