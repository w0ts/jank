/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB port's role (device or host) and the beat tracker's audio source: small state machines, host-tested.
 *
 * Nothing here knows the port chip. The firmware puts a port HAL under it (firmware/main/usbport.h): what is on the cable
 * (enum jur_cc, read from the chip), the port mode to set (sink / dual role preferring source), and "VBUS out": prepare the
 * 5 V source (vbus_prep, ready after prep_ms) and switch it onto VBUS (vbus_on). jank mini 2.2's TUSB320LI + charge pump
 * + HT7550 + HOST_EN is one HAL (usbport_tusb320.c); a power-bank IC with its own role logic and boost would be another.
 *
 * The setting USB: DEVICE / HOST says what the user wants; what runs follows the cable:
 * - DEVICE: always the device stack (a computer: network, MIDI, drive, the speaker), the port a sink.
 * - HOST, with a port chip: the port a dual-role port (preferring the sink against another dual role, below). A device attached (jank the source) ->
 *   the host stack, then the 5 V source prepared, then VBUS on. A host or a charger (jank the sink) -> VBUS off at once and
 *   the device stack: 5 V never goes onto a charger. Nothing attached -> the host stack, ready for a device.
 * - HOST, with no port chip (the XIAO): the device stack first; no computer within JUR_COMPUTER_WAIT_MS -> the host stack
 *   (no 5 V of jank's own: the gear powers itself, through an OTG adapter or a powered hub).
 * The stack is chosen at a start (jur_boot_stack); changing it means a restart into the other stack (jur_out.restart),
 * which the firmware does with the wanted stack kept in RTC memory across it.
 *
 * The audio source (AUDIO > SOURCE): AUTO = the line in when a plug is in, else USB audio when a stream is open; LINE, USB:
 * that one only; LINK = USB audio ignored, the line in still when a plug is in (the plug is mechanical: it wins). USB audio
 * is the computer's stream (device stack) or the hosted device's (host stack). AUDIO mode itself is "the source has
 * sound" (jank_live.h). A unit with no screen toggles AUTO <-> LINK with a single NO (jur_src_toggle). */
#ifndef JANK_USBROLE_H
#define JANK_USBROLE_H
#include <stdint.h>
#include "jank_lat.h"

enum jur_usb { JUR_USB_DEVICE = 0, JUR_USB_HOST = 1 };                 /* the setting */
enum jur_src { JUR_SRC_AUTO = 0, JUR_SRC_LINE = 1, JUR_SRC_USB = 2, JUR_SRC_LINK = 3 };   /* the setting */
enum jur_audio { JUR_AUDIO_NONE = 0, JUR_AUDIO_LINE = 1, JUR_AUDIO_USB = 2 };   /* what the tracker hears now */
enum jur_stack { JUR_STACK_DEVICE = 0, JUR_STACK_HOST = 1 };
/* what the port chip says is on the cable */
enum jur_cc {
    JUR_CC_NONE = 0,                    /* not attached */
    JUR_CC_DEVICE = 1,                  /* a device (a sink, Rd): jank is the source / host (e.g. TUSB320 Attached.SRC) */
    JUR_CC_HOST = 2,                    /* a host or a charger (a source, Rp): jank is the sink (e.g. TUSB320 Attached.SNK) */
    JUR_CC_ACCESSORY = 3                /* an audio or debug accessory: neither */
};
/* sink only; dual role. The dual role prefers the SINK against another dual-role port (TUSB320 Try.SNK): a plain sink (a
 * synth, an interface, an OTG adapter: Rd only) still makes jank the source, but a dual-role laptop, tablet or power bank
 * makes jank its device, so a computer can always reach the control line (Try.SRC would make such a computer jank's
 * "device", with no way back to `set usb device` until the menu exists). */
enum jur_port_mode { JUR_PORT_SINK = 0, JUR_PORT_DUAL = 1 };

#define JUR_COMPUTER_WAIT_MS 3000u     /* no port chip, HOST: a computer that has not configured the device by then is none */
#define JUR_SETTLE_MS 1500u            /* nothing on the cable this long before the device stack restarts as host */
#define JUR_MIN_RUN_MS 3000u           /* a stack runs at least this long before a restart into the other: no restart loop */
#define JUR_VBUS_HOLDOFF_MS 500u       /* VBUS sense ignored this long after jank's own VBUS went off (mini 2.2: C27 / C91 hold
                                          the signal's charge on the connector side, seen once Q9 is back on) */
#define JUR_LINE_DEBOUNCE_MS 50u       /* the line in's plug detect holds a level this long before it counts */

struct jur_boot {
    uint8_t setting;                    /* enum jur_usb */
    uint8_t has_port;                   /* the board has a port chip and it answered */
    uint8_t cc;                         /* enum jur_cc, read at the start */
    uint8_t vbus;                       /* the board's VBUS is powered from outside (VBUS sense) */
    uint8_t hint;                       /* 0 none, 1 + enum jur_stack: what the last restart asked for */
};
/* The setting the role core is given. With no port chip (the XIAO) and no choice ever made (usb_set 0), HOST: the device stack first,
 * the host stack when no computer has configured the device within JUR_COMPUTER_WAIT_MS. A XIAO has no menu and the control page cannot
 * change the role, so without this no one could host with it; a computer on the cable still wins. A choice (`set usb device|host`) or a
 * port chip (its HOST also drives 5 V onto the cable) is taken as it is; a value that is not DEVICE or HOST counts as DEVICE. */
uint8_t jur_effective_setting(uint8_t setting, uint8_t user_set, uint8_t has_port);

/* the stack to start: the hint when there is one and the setting still allows it, else from the cable */
uint8_t jur_boot_stack(const struct jur_boot *b);

struct jur_in {
    uint8_t setting;                    /* enum jur_usb */
    uint8_t has_port;
    uint8_t cc;                         /* enum jur_cc */
    uint8_t vbus;                       /* VBUS powered from outside: read only while jank's own VBUS is off (mini 2.2: Q9 is off with it on) */
    uint8_t computer;                   /* a computer configured the device stack since the start */
    uint32_t now_ms;
    uint8_t prep_failed;                /* the port HAL could not prepare its 5 V source last step: its time starts again */
};

struct jur_out {
    uint8_t port_mode;                  /* enum jur_port_mode: what the port chip should be set to ... */
    uint8_t port_write;                 /* ... and 1 when it may be written now (a mode write drops the attachment) */
    uint8_t vbus_prep;                  /* 1: prepare the 5 V source (mini 2.2: the charge pump) */
    uint8_t vbus_on;                    /* 1: jank's 5 V onto VBUS (mini 2.2: HOST_EN) */
    uint8_t want;                       /* enum jur_stack: the stack the cable and the setting ask for */
    uint8_t restart;                    /* 1: restart now into `want` (vbus_on is already 0 in this output) */
};

struct jur {
    uint8_t stack;                      /* the stack running: fixed until a restart */
    uint8_t vbus_on, vbus_prep, prep_ready;
    uint8_t cc_last;
    uint32_t prep_ms;                   /* the port HAL's: how long its 5 V source takes to come up */
    uint32_t t_start, t_cc, t_prep, t_off;
};

void jur_init(struct jur *r, uint8_t stack, uint32_t prep_ms, uint32_t now_ms);
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
/* whose beat_latency is in use (core/jank_lat.h): the line in, the computer (USB audio, jank a device) or the hosted device
 * streaming (host: has_dev, its VID:PID). 1 and *k set; 0 nothing heard or the device not known yet (the value stays) */
int jur_lat_key(uint8_t audio, uint8_t stack, int has_dev, uint16_t vid, uint16_t pid, struct jla_key *k);
/* a screenless unit's single NO: USB AUDIO (AUTO) <-> LINK; from LINE or USB (set on the page): LINK. A plug in the line in:
 * the setting unchanged (the line wins anyway), *changed 0. Returns the new setting */
uint8_t jur_src_toggle(uint8_t setting, int line_plugged, int *changed);

const char *jur_usb_name(uint8_t setting);      /* "device" "host" */
const char *jur_src_name(uint8_t setting);      /* "auto" "line" "usb" "link" */
const char *jur_audio_name(uint8_t audio);      /* "none" "line" "usb" */
const char *jur_cc_name(uint8_t cc);            /* "none" "device" "host" "accessory" */
int jur_usb_parse(const char *s, uint8_t *out);  /* 0 ok, -1 not one */
int jur_src_parse(const char *s, uint8_t *out);
#endif
