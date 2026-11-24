/* SPDX-License-Identifier: GPL-3.0-only */
/* What a Launchpad press asks of the Digitone (and of the transport), as data: the firmware turns each act into a live-control request (usb_lp.c). Shared by the
 * Note mode (jank_lpapp.c) and the step modes (jank_lpstep.c). */
#ifndef JANK_LPACT_H
#define JANK_LPACT_H
#include <stdint.h>

enum jlapp_act_kind {
    JLAPP_ACT_NONE,
    JLAPP_ACT_NOTE_ON,                 /* track, note, vel */
    JLAPP_ACT_NOTE_OFF,                /* track, note */
    JLAPP_ACT_TRACK_ON,                /* a track pad: the track's own default note */
    JLAPP_ACT_TRACK_OFF,
    JLAPP_ACT_SELECT,                  /* the Digitone's active track becomes `track` */
    JLAPP_ACT_STEP,                    /* a step edit: track, step (0..127), op (enum jdl_step_op: 0 clear, 1 trig, 2 note), note, vel (255: the track's), len (255) */
    JLAPP_ACT_TRANSPORT                /* start / stop the clock (Jank is the transport master) */
};

struct jlapp_act {
    uint8_t kind, track, note, vel;
    uint8_t step, op, len;
};

/* a note the Digitone's sequencer played (its push), kept for the lights: both Note mode and the step modes flash what plays */
struct jlapp_recent {
    uint32_t at_ms;
    uint8_t track, note, used;
};
#endif
