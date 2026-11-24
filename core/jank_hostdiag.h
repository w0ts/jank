/* SPDX-License-Identifier: GPL-3.0-only */
/* A unit in USB host mode has no control line and no log, so what it sees is recorded here (firmware/main/hostdiag.c fills it
 * from the host clients, saves it to flash when it changed, and `diag` prints it once the computer is plugged in again): the
 * stack that started, the devices that came, their names and IDs, the interface and endpoints chosen, the claim result, and
 * what the Launchpad and the Digitone sent. Pure: a record, its text and a size / version check. Host-tested
 * (tests/test_hostdiag.c). */
#ifndef JANK_HOSTDIAG_H
#define JANK_HOSTDIAG_H
#include <stddef.h>
#include <stdint.h>
#include "jank_hostfilter.h"

#define JHD_DEVS 4u                    /* as many as usb_midi_host.c has slots */
#define JHD_NAME_MAX 23u               /* as JK_USB_NAME_MAX */
#define JHD_VERSION 17u             /* the record's layout: a saved record of another version is not read (2: the memory numbers; 3: lib_devs; 4: hprt; 5: filter;
                                          17: the scale sync; 16: live_msgs_sent, live_msg_ok; 15: the playhead's source (head_); 14: the step counters; 13: live_ignored and the dn2_ status; 12: live_reads, live_msgs; 11: the live_ fields; 10: setup_sent; 9: lp_mode, lp_reentered; 8: lights_sent; 7: programmer_acked; 6: heap_big_b, and the endpoint fields of a device: a field that fits into padding changes the layout but not the size,
                                          so the size check does not catch it: bump this with every change to struct jhd or jhd_dev) */

/* the steps of a host start, in order, for the one that failed (jhd_format_failure) */
enum jhd_step {
    JHD_STEP_NONE = 0,
    JHD_STEP_HOSTLIB,                  /* the USB Host Library's install */
    JHD_STEP_CLIENT,                   /* the MIDI client's registration */
    JHD_STEP_CLIENT_TASK,              /* the client's task (its stack is internal heap) */
    JHD_STEP_MIDI_TASK,                /* usb.c's MIDI task */
    JHD_STEP_ROLE,                     /* the role task */
    JHD_STEP_RADIO,                    /* the Wi-Fi radio, which starts after the USB stack */
    JHD_STEP_BRIDGE,                   /* the bridge task (a 6 KB stack from the internal heap) */
    JHD_STEP_BEAT,                     /* the beat tracker's start */
    JHD_STEP_CRASH                     /* not a step: the host run ended in a reset (panic, watchdog, brownout), found at the next start */
};

/* The internal heap at each stage of a host start (free bytes, and the largest block a task stack could come from), kept in RTC memory by
 * the firmware so that it survives the crash it explains and `diag` can print it after the restart as a device. */
#define JHD_TRACE_MAX 12u
struct jhd_mark {
    uint8_t step;                      /* enum jhd_step: the stage that is about to run */
    uint32_t free_b, largest_b;
};
/* `hostdiag_trace=<stage>:<free>/<largest>,...` for the first n marks (at most JHD_TRACE_MAX; m must hold n); nothing at all for n == 0 */
size_t jhd_format_trace(const struct jhd_mark *m, unsigned n, char *out, size_t cap);

enum jhd_stack { JHD_STACK_UNKNOWN = 0, JHD_STACK_DEVICE = 1, JHD_STACK_HOST = 2 };

struct jhd_dev {
    uint8_t used;
    uint8_t role;                      /* 0 other, 1 Launchpad X, 2 Digitone (enum jlapp_role) */
    uint8_t ifs;                       /* MIDI streaming interfaces the device has */
    uint8_t itf_index;                 /* the one chosen (0 = the first) */
    uint8_t out_ep, in_ep;             /* endpoint addresses (in 0: none opened) */
    uint8_t in_last_status;            /* the last IN transfer status that was not "completed" */
    uint8_t neps;                      /* endpoints in the chosen interface (the library books every one of them on a claim) */
    uint8_t clamped;                   /* bulk endpoints whose declared size was over what a full-speed link carries, corrected to 64 (core/jank_fsclamp.h) */
    uint8_t claim_where;               /* how far the claim got: 1 the interface, 2 the OUT transfer, 3 the IN transfer (the step that failed when claim_err is not 0) */
    uint16_t out_mps, in_mps;          /* the bulk endpoints' max packet sizes as the descriptor gives them (the controller's FIFO limit is checked against them) */
    uint16_t vid, pid;
    int32_t claim_err;                 /* the claim's esp_err_t, 0 ok; non-zero: the device was left alone */
    uint32_t in_packets, in_errors, out_done, out_fail;
    uint8_t in_last[4];                /* the last 4-byte packet that came in */
    char name[JHD_NAME_MAX + 1];
};

struct jhd {
    uint8_t version;                   /* JHD_VERSION */
    uint8_t stack;                     /* enum jhd_stack: what started */
    uint8_t lib_up;
    uint8_t lib_devs;                  /* devices the USB Host Library has enumerated right now, hubs included (usb_host_lib_info) */
    uint8_t lpx_up, dn2_up;           /* usb_lp.c has attached them */
    uint32_t hprt;                     /* the controller's host port register as it was last read (firmware/main/hostdiag.c) */
    struct jhf filter;                 /* the enumeration rule's state: devices it skipped (core/jank_hostfilter.h) */
    uint32_t new_dev, gone_dev, add_calls;
    uint32_t programmer_sent;          /* Programmer mode messages put in the Launchpad's queue */
    uint32_t programmer_acked;         /* the Launchpad's answers to them (jlx_programmer_ack): fewer than sent means it did not take one */
    uint32_t setup_sent;               /* the LED-feedback and sleep-off messages (jlx_setup) sent after an answer: without them it shows no light */
    uint32_t lights_sent;              /* light messages put in the Launchpad's queue (the whole frame is 81; it is sent again every 2 s) */
    uint8_t lp_mode;                   /* the mode the Launchpad last reported: 0 nothing yet, 1 Live, 2 Programmer (a Live answer makes the unit enter Programmer again) */
    uint8_t lp_reentered;              /* times it was found in Live mode after it had been put in Programmer mode (capped at 255) */
    uint32_t pads;                     /* Launchpad pad / button events decoded */
    uint32_t dn2_msgs;                 /* packets from the Digitone */
    uint8_t live_state;                /* the Digitone's live control (jank_dn2live.h enum jdl_state): 0 down, 1 asking, 2 live, 3 REMOTE CONTROL off, 4 no live control */
    uint8_t live_play;                 /* PLAY has not been refused: 1 until the Digitone answers 2 to one */
    uint32_t live_asked;               /* live-control requests sent (LIVE ON / STATUS, track select, PLAY) */
    uint32_t live_replies;             /* replies to them */
    uint32_t live_plays;               /* PLAY requests sent */
    uint32_t live_selects;             /* track selects sent */
    uint32_t live_pushes;              /* note pushes received */
    uint32_t live_notes;               /* notes in them */
    uint32_t live_dropped;             /* requests that found no room in the Digitone's packet FIFO, and were not sent */
    uint32_t live_reads;               /* active-track reads sent (FOLLOW) */
    uint32_t live_msgs;                /* messages the Launchpad logic made (shown on Jank's screen if it has one; the Digitone's MESSAGE command is not used yet) */
    uint32_t live_scale_reads, live_scale_sets;   /* SCALE polls sent, SCALE SET writes sent (the Launchpad's scale or key change) */
    uint8_t dn2_scale, dn2_root, dn2_root_known, scale_ok, scale_set_ok;   /* the Digitone's scale index and root as last read; SCALE and SCALE SET not refused */
    uint32_t live_edits, live_pagereads;   /* the step modes: STEP edits sent, STEPS page reads sent */
    uint32_t live_step_busy, live_step_refused;   /* the Digitone answered STEP / STEPS with 5 (16 writes waiting) / 2 (refused) */
    uint32_t live_msgs_sent;           /* of those messages, the ones sent to the Digitone's screen (MESSAGE, 54) */
    uint8_t live_msg_ok;               /* the Digitone has not refused MESSAGE: 1 until it does */
    uint32_t live_ignored;             /* SysEx frames from the Digitone that the live-control reader did not take (not a reply, not a push, or damaged) */
    uint16_t live_ign_len;             /* the length of the last one */
    uint8_t dn2_proto, dn2_push, dn2_lease;   /* the Digitone's own LIVE STATUS: protocol (0: none read yet), push on, seconds of lease left */
    uint32_t dn2_made, dn2_sent, dn2_lost, dn2_dropped, dn2_pushmsgs;   /* ... notes made, sent, lost, dropped, and push messages sent (the Digitone's counters) */
    uint32_t heap_big_b;               /* the largest free block of byte-addressable internal RAM, bytes: what a task stack or a DMA buffer needs in one piece */
    uint16_t heap_kb;                  /* internal heap free now (task stacks and USB buffers come from it), KB */
    uint16_t heap_min_kb;              /* its lowest since the start */
    uint16_t client_stack_free;        /* the MIDI client task's stack never used (high-water mark), bytes */
    struct jhd_dev dev[JHD_DEVS];
};

void jhd_init(struct jhd *d);
/* the record as `key=value` lines (the style of `status`), a NUL-terminated string in out (cap bytes, cut to fit); returns its
 * length. Unused device slots print nothing; unprintable characters in a name print as '.' */
size_t jhd_format(const struct jhd *d, char *out, size_t cap);
/* `hostdiag_start=ok` (step 0) or `hostdiag_start=failed:<step name>,err:<esp_err_t>`: why a host start failed, kept in flash apart from the
 * record (the restart that follows a failure must not lose it). Same cut-to-fit rules as jhd_format; returns the length */
size_t jhd_format_failure(uint8_t step, int32_t err, char *out, size_t cap);
/* 1: n bytes at p are a record of this layout and version (what was read back from flash) */
int jhd_valid(const void *p, size_t n);
#endif
