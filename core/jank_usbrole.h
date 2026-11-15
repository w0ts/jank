/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB port's role (device or host) and the beat tracker's audio source: small state machines, host-tested.
 *
 * The setting USB: DEVICE / HOST says what the user wants; what runs follows the cable:
 * - DEVICE: always the device stack (a computer: network, MIDI, drive, the speaker), the CC chip a sink (UFP).
 * - HOST, on a board with the CC chip (jank mini 2.2, battery order: TI TUSB320LI): the chip a dual-role port that tries to
 *   be the source (DRP + Try.SRC, SLLSEP2D 7.3, Table 10). A device attached (Attached.SRC: jank is the DFP) -> the host
 *   stack, then the charge pump, then HOST_EN (the 5 V "signal" on VBUS). A host or a charger (Attached.SNK) -> HOST_EN low
 *   at once and the device stack: 5 V never goes onto a charger. Nothing attached -> the host stack, ready for a device.
 * - HOST, on a board without it (the XIAO): the device stack first; no computer within JUR_COMPUTER_WAIT_MS -> the host
 *   stack (no 5 V of jank's own: the gear powers itself, through an OTG adapter).
 * The stack is chosen at a start (jur_boot_stack); changing it means a restart into the other stack (jur_out.restart),
 * which the firmware does with the wanted stack kept in RTC memory across it.
 *
 * The audio source (AUDIO > SOURCE): AUTO = the line in when a plug is in, else USB audio when a stream is open; LINE, USB:
 * that one only. USB audio is the computer's stream (device stack) or the hosted device's (host stack). */
#ifndef JANK_USBROLE_H
#define JANK_USBROLE_H
#include <stdint.h>

enum jur_usb { JUR_USB_DEVICE = 0, JUR_USB_HOST = 1 };                 /* the setting */
enum jur_src { JUR_SRC_AUTO = 0, JUR_SRC_LINE = 1, JUR_SRC_USB = 2 };   /* the setting */
enum jur_audio { JUR_AUDIO_NONE = 0, JUR_AUDIO_LINE = 1, JUR_AUDIO_USB = 2 };   /* what the tracker hears now */
enum jur_stack { JUR_STACK_DEVICE = 0, JUR_STACK_HOST = 1 };
/* what the CC chip says is on the cable */
enum jur_cc {
    JUR_CC_NONE = 0,                    /* not attached */
    JUR_CC_DEVICE = 1,                  /* a device (a sink, Rd): jank is the source / host (TUSB320 Attached.SRC) */
    JUR_CC_HOST = 2,                    /* a host or a charger (a source, Rp): jank is the sink (TUSB320 Attached.SNK) */
    JUR_CC_ACCESSORY = 3                /* an audio or debug accessory: neither */
};
enum jur_ccmode { JUR_CCMODE_UFP = 0, JUR_CCMODE_DRP_TRYSRC = 1 };

#define JUR_COMPUTER_WAIT_MS 3000u     /* no CC chip, HOST: a computer that has not configured the device by then is none */
#define JUR_SETTLE_MS 1500u            /* nothing on the cable this long before the device stack restarts as host */
#define JUR_MIN_RUN_MS 3000u           /* a stack runs at least this long before a restart into the other: no restart loop */
#define JUR_PUMP_MS 10u                /* the charge pump's rail is up this long after its start (board README) */
#define JUR_LINE_DEBOUNCE_MS 50u       /* the line in's plug detect holds a level this long before it counts */

struct jur_boot {
    uint8_t setting;                    /* enum jur_usb */
    uint8_t has_cc;                     /* the board has the CC chip and it answered */
    uint8_t cc;                         /* enum jur_cc, read at the start */
    uint8_t vbus;                       /* the board's VBUS is powered from outside (VBUS_SENSE) */
    uint8_t hint;                       /* 0 none, 1 + enum jur_stack: what the last restart asked for */
};
/* the stack to start: the hint when there is one and the setting still allows it, else from the cable */
uint8_t jur_boot_stack(const struct jur_boot *b);

struct jur_in {
    uint8_t setting;                    /* enum jur_usb */
    uint8_t has_cc;
    uint8_t cc;                         /* enum jur_cc */
    uint8_t vbus;                       /* VBUS_SENSE high: only meaningful while HOST_EN is low (Q9 is off with it high) */
    uint8_t computer;                   /* a computer configured the device stack since the start */
    uint32_t now_ms;
};

struct jur_out {
    uint8_t cc_mode;                    /* enum jur_ccmode: what the CC chip should be set to ... */
    uint8_t cc_write;                   /* ... and 1 when it may be written now (a mode write drops the attachment) */
    uint8_t pump;                       /* 1: the charge pump should run (for HOST_EN) */
    uint8_t host_en;                    /* 1: the 5 V signal on VBUS */
    uint8_t want;                       /* enum jur_stack: the stack the cable and the setting ask for */
    uint8_t restart;                    /* 1: restart now into `want` (HOST_EN is already low in this output) */
};

struct jur {
    uint8_t stack;                      /* the stack running: fixed until a restart */
    uint8_t host_en, pump;
    uint8_t cc_last;
    uint32_t t_start, t_cc, t_pump;
};

void jur_init(struct jur *r, uint8_t stack, uint32_t now_ms);
void jur_step(struct jur *r, const struct jur_in *in, struct jur_out *out);

/* the line in's plug detect, debounced */
struct jur_line {
    uint8_t raw, level;
    uint32_t t_raw;
};
void jur_line_init(struct jur_line *l, int raw, uint32_t now_ms);
int jur_line_step(struct jur_line *l, int raw, uint32_t now_ms);    /* -> the debounced level */

/* what the tracker listens to (enum jur_audio) */
uint8_t jur_source(uint8_t setting, int line_plugged, int usb_open);

const char *jur_usb_name(uint8_t setting);      /* "device" "host" */
const char *jur_src_name(uint8_t setting);      /* "auto" "line" "usb" */
const char *jur_audio_name(uint8_t audio);      /* "none" "line" "usb" */
const char *jur_cc_name(uint8_t cc);            /* "none" "device" "host" "accessory" */
int jur_usb_parse(const char *s, uint8_t *out);  /* 0 ok, -1 not one */
int jur_src_parse(const char *s, uint8_t *out);
#endif
