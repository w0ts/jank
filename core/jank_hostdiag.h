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

#define JHD_DEVS 4u                    /* as many as usb_midi_host.c has slots */
#define JHD_NAME_MAX 23u               /* as JK_USB_NAME_MAX */
#define JHD_VERSION 1u                 /* the record's layout: a saved record of another version is not read */

enum jhd_stack { JHD_STACK_UNKNOWN = 0, JHD_STACK_DEVICE = 1, JHD_STACK_HOST = 2 };

struct jhd_dev {
    uint8_t used;
    uint8_t role;                      /* 0 other, 1 Launchpad X, 2 Digitone (enum jlapp_role) */
    uint8_t ifs;                       /* MIDI streaming interfaces the device has */
    uint8_t itf_index;                 /* the one chosen (0 = the first) */
    uint8_t out_ep, in_ep;             /* endpoint addresses (in 0: none opened) */
    uint8_t in_last_status;            /* the last IN transfer status that was not "completed" */
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
    uint8_t lpx_up, dn2_up;            /* usb_lp.c has attached them */
    uint32_t new_dev, gone_dev, add_calls;
    uint32_t programmer_sent;          /* Programmer mode messages put in the Launchpad's queue */
    uint32_t pads;                     /* Launchpad pad / button events decoded */
    uint32_t dn2_msgs;                 /* packets from the Digitone */
    struct jhd_dev dev[JHD_DEVS];
};

void jhd_init(struct jhd *d);
/* the record as `key=value` lines (the style of `status`), a NUL-terminated string in out (cap bytes, cut to fit); returns its
 * length. Unused device slots print nothing; unprintable characters in a name print as '.' */
size_t jhd_format(const struct jhd *d, char *out, size_t cap);
/* 1: n bytes at p are a record of this layout and version (what was read back from flash) */
int jhd_valid(const void *p, size_t n);
#endif
