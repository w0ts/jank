/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB port's role and the audio source (jank_usbrole.h). */
#include "jank_usbrole.h"
#include <string.h>

/* what the cable asks for with the setting HOST and a port chip; -1: nothing decides yet (keep what runs) */
static int cable_stack(uint8_t cc, uint8_t vbus)
{
    if (cc == JUR_CC_DEVICE)
        return JUR_STACK_HOST;
    if (cc == JUR_CC_HOST || vbus)      /* a host, a charger, or power from something with no CC role: be its device */
        return JUR_STACK_DEVICE;
    return -1;
}

uint8_t jur_boot_stack(const struct jur_boot *b)
{
    int s;
    if (b->setting != JUR_USB_HOST)
        return JUR_STACK_DEVICE;
    if (!b->has_port)                     /* the XIAO: the device first, unless the last start found no computer */
        return b->hint == 1u + JUR_STACK_HOST ? JUR_STACK_HOST : JUR_STACK_DEVICE;
    s = cable_stack(b->cc, b->vbus);
    return s < 0 ? JUR_STACK_HOST : (uint8_t)s;   /* nothing on the cable: ready for a device */
}

void jur_init(struct jur *r, uint8_t stack, uint32_t prep_ms, uint32_t now_ms)
{
    memset(r, 0, sizeof *r);
    r->stack = stack;
    r->prep_ms = prep_ms;
    r->cc_last = JUR_CC_NONE;
    r->t_start = now_ms;
    r->t_cc = now_ms;
}

/* VBUS sense counts only while jank's own VBUS is off, and not just after it went off */
static uint8_t vbus_seen(const struct jur *r, const struct jur_in *in)
{
    if (r->vbus_on || (r->t_off && in->now_ms - r->t_off < JUR_VBUS_HOLDOFF_MS))
        return 0;
    return in->vbus;
}

static uint8_t want_stack(const struct jur *r, const struct jur_in *in)
{
    int s;
    if (in->setting != JUR_USB_HOST)
        return JUR_STACK_DEVICE;
    if (!in->has_port) {
        if (r->stack == JUR_STACK_HOST || (!in->computer && in->now_ms - r->t_start >= JUR_COMPUTER_WAIT_MS))
            return JUR_STACK_HOST;
        return JUR_STACK_DEVICE;
    }
    s = cable_stack(in->cc, vbus_seen(r, in));
    if (s >= 0)
        return (uint8_t)s;
    /* nothing attached: the host stack, once the cable has been quiet a while (a cable being swapped passes through here) */
    if (r->stack == JUR_STACK_DEVICE && in->now_ms - r->t_cc < JUR_SETTLE_MS)
        return JUR_STACK_DEVICE;
    return JUR_STACK_HOST;
}

void jur_step(struct jur *r, const struct jur_in *in, struct jur_out *out)
{
    uint8_t source;
    memset(out, 0, sizeof *out);
    if (in->cc != r->cc_last) {
        r->cc_last = in->cc;
        r->t_cc = in->now_ms;
    }
    out->want = want_stack(r, in);
    out->port_mode = in->setting == JUR_USB_HOST ? JUR_PORT_DUAL : JUR_PORT_SINK;
    /* a mode write drops the attachment (SLLSEP2D 7.3.2: DISABLE_TERM around it): to the sink at once, to the dual role
     * only with nothing attached (a computer charging jank keeps its link until it is unplugged) */
    out->port_write = (uint8_t)(in->has_port && (out->port_mode == JUR_PORT_SINK || in->cc == JUR_CC_NONE || in->cc == JUR_CC_ACCESSORY));
    /* jank sources VBUS only as the host stack, to a device that the port chip saw attach, never onto outside power */
    source = (uint8_t)(in->has_port && in->setting == JUR_USB_HOST && r->stack == JUR_STACK_HOST && out->want == JUR_STACK_HOST
                       && in->cc == JUR_CC_DEVICE && (r->vbus_on || !vbus_seen(r, in)));
    if (!source) {
        if (r->vbus_on)
            r->t_off = in->now_ms ? in->now_ms : 1u;
        r->vbus_prep = 0;
        r->vbus_on = 0;
    } else {
        if (!r->vbus_prep || in->prep_failed) {
            r->vbus_prep = 1;
            r->prep_ready = 0;         /* the source's time runs from a preparation that worked */
        }
        if (!in->prep_failed && !r->prep_ready) {
            r->prep_ready = 1;
            r->t_prep = in->now_ms;
        }
        if (in->prep_failed) {
            if (r->vbus_on)
                r->t_off = in->now_ms ? in->now_ms : 1u;
            r->vbus_on = 0;
        }
        else if (in->now_ms - r->t_prep >= r->prep_ms)
            r->vbus_on = 1;             /* never before the source is up (mini 2.2: V5H comes from the pump's rail) */
    }
    out->vbus_prep = r->vbus_prep;
    out->vbus_on = r->vbus_on;
    out->restart = (uint8_t)(out->want != r->stack && in->now_ms - r->t_start >= JUR_MIN_RUN_MS);
}

void jur_line_init(struct jur_line *l, int raw, uint32_t now_ms)
{
    l->raw = (uint8_t)(raw != 0);
    l->level = l->raw;
    l->t_raw = now_ms;
}

int jur_line_step(struct jur_line *l, int raw, uint32_t now_ms)
{
    uint8_t v = (uint8_t)(raw != 0);
    if (v != l->raw) {
        l->raw = v;
        l->t_raw = now_ms;
    } else if (v != l->level && now_ms - l->t_raw >= JUR_LINE_DEBOUNCE_MS) {
        l->level = v;
    }
    return l->level;
}

uint8_t jur_source(uint8_t setting, int line_plugged, int usb_open)
{
    if (setting == JUR_SRC_LINE)
        return JUR_AUDIO_LINE;
    if (setting == JUR_SRC_USB)
        return JUR_AUDIO_USB;
    if (line_plugged)
        return JUR_AUDIO_LINE;
    return usb_open && setting != JUR_SRC_LINK ? JUR_AUDIO_USB : JUR_AUDIO_NONE;
}

uint8_t jur_src_toggle(uint8_t setting, int line_plugged, int *changed)
{
    uint8_t next = setting == JUR_SRC_LINK ? JUR_SRC_AUTO : JUR_SRC_LINK;
    *changed = !line_plugged;
    return line_plugged ? setting : next;
}

static const char *const USB_NAMES[] = {"device", "host"};
static const char *const SRC_NAMES[] = {"auto", "line", "usb", "link"};
static const char *const AUDIO_NAMES[] = {"none", "line", "usb"};
static const char *const CC_NAMES[] = {"none", "device", "host", "accessory"};

const char *jur_usb_name(uint8_t setting) { return setting <= JUR_USB_HOST ? USB_NAMES[setting] : "?"; }
const char *jur_src_name(uint8_t setting) { return setting <= JUR_SRC_LINK ? SRC_NAMES[setting] : "?"; }
const char *jur_audio_name(uint8_t audio) { return audio <= JUR_AUDIO_USB ? AUDIO_NAMES[audio] : "?"; }
const char *jur_cc_name(uint8_t cc) { return cc <= JUR_CC_ACCESSORY ? CC_NAMES[cc] : "?"; }

static int parse_name(const char *s, const char *const *names, uint8_t n, uint8_t *out)
{
    for (uint8_t i = 0; i < n; i++)
        if (!strcmp(s, names[i])) {
            *out = i;
            return 0;
        }
    return -1;
}

int jur_usb_parse(const char *s, uint8_t *out) { return parse_name(s, USB_NAMES, 2, out); }
int jur_src_parse(const char *s, uint8_t *out) { return parse_name(s, SRC_NAMES, 4, out); }
