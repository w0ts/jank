/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the USB role and the audio source (core/jank_usbrole.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_usbrole.h"

#define JUR_TEST_PREP_MS 10u           /* mini 2.2's pump (its README: ~10 ms); any HAL's value works the same */

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

/* steps every 10 ms for ms with the same inputs; returns the last output, and whether a restart or HOST_EN was ever seen */
static struct jur_out run(struct jur *r, struct jur_in *in, uint32_t ms, int *restarted, int *sourced)
{
    struct jur_out o;
    memset(&o, 0, sizeof o);
    for (uint32_t t = 0; t <= ms; t += 10u) {
        jur_step(r, in, &o);
        if (restarted && o.restart)
            *restarted = 1;
        if (sourced && o.vbus_on)
            *sourced = 1;
        in->now_ms += 10u;
    }
    return o;
}

static void test_boot(void)
{
    struct jur_boot b = {JUR_USB_DEVICE, 1, JUR_CC_DEVICE, 0, 0};
    CHECK(jur_boot_stack(&b) == JUR_STACK_DEVICE);              /* DEVICE: always */
    b.hint = 1u + JUR_STACK_HOST;
    CHECK(jur_boot_stack(&b) == JUR_STACK_DEVICE);              /* a stale hint does not beat the setting */
    b.setting = JUR_USB_HOST;
    b.hint = 0;
    CHECK(jur_boot_stack(&b) == JUR_STACK_HOST);                /* a device on the cable */
    b.cc = JUR_CC_HOST;
    CHECK(jur_boot_stack(&b) == JUR_STACK_DEVICE);              /* a computer or a charger */
    b.cc = JUR_CC_NONE;
    CHECK(jur_boot_stack(&b) == JUR_STACK_HOST);                /* nothing: ready for a device */
    b.vbus = 1;
    CHECK(jur_boot_stack(&b) == JUR_STACK_DEVICE);              /* outside power with no CC role */
    b.has_port = 0;
    b.vbus = 0;
    CHECK(jur_boot_stack(&b) == JUR_STACK_DEVICE);              /* the XIAO: the device first ... */
    b.hint = 1u + JUR_STACK_HOST;
    CHECK(jur_boot_stack(&b) == JUR_STACK_HOST);                /* ... host after a start that found no computer */
    b.hint = 1u + JUR_STACK_DEVICE;
    CHECK(jur_boot_stack(&b) == JUR_STACK_DEVICE);
}

/* the setting DEVICE: never 5 V, never host, the CC chip a sink and writable at once */
static void test_device_setting(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_DEVICE, 1, JUR_CC_DEVICE, 0, 0, 1000, 0};
    int restarted = 0, sourced = 0;
    struct jur_out o;
    jur_init(&r, JUR_STACK_DEVICE, JUR_TEST_PREP_MS, 1000);
    o = run(&r, &in, 10000, &restarted, &sourced);
    CHECK(!restarted && !sourced && !o.vbus_prep && o.want == JUR_STACK_DEVICE);
    CHECK(o.port_mode == JUR_PORT_SINK && o.port_write);
}

/* HOST with the CC chip: a device attaches -> pump, then HOST_EN 10 ms later; it leaves -> both off at once */
static void test_host_device_attach(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 1, JUR_CC_NONE, 0, 0, 1000, 0};
    struct jur_out o;
    int restarted = 0;
    jur_init(&r, JUR_STACK_HOST, JUR_TEST_PREP_MS, 1000);
    o = run(&r, &in, 5000, &restarted, NULL);
    CHECK(!restarted && !o.vbus_on && !o.vbus_prep && o.want == JUR_STACK_HOST);
    CHECK(o.port_mode == JUR_PORT_DUAL && o.port_write);    /* nothing attached: the dual role may be written */
    in.cc = JUR_CC_DEVICE;
    jur_step(&r, &in, &o);
    CHECK(o.vbus_prep && !o.vbus_on);                                /* the pump first ... */
    CHECK(!o.port_write);                                         /* attached: the dual role is not rewritten */
    in.now_ms += JUR_TEST_PREP_MS - 1u;
    jur_step(&r, &in, &o);
    CHECK(o.vbus_prep && !o.vbus_on);
    in.now_ms += 1u;
    jur_step(&r, &in, &o);
    CHECK(o.vbus_prep && o.vbus_on);                                 /* ... HOST_EN once its rail is up */
    in.vbus = 1;                                                /* (Q9 off: VBUS_SENSE means nothing now) */
    jur_step(&r, &in, &o);
    CHECK(o.vbus_on);
    in.vbus = 0;
    in.cc = JUR_CC_NONE;
    jur_step(&r, &in, &o);
    CHECK(!o.vbus_on && !o.vbus_prep && !o.restart);
}

/* HOST, host stack running, a charger or a computer plugged: HOST_EN low in the same step, then a restart as device */
static void test_host_then_charger(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 1, JUR_CC_DEVICE, 0, 0, 1000, 0};
    struct jur_out o;
    jur_init(&r, JUR_STACK_HOST, JUR_TEST_PREP_MS, 1000);
    o = run(&r, &in, 4000, NULL, NULL);
    CHECK(o.vbus_on);
    in.cc = JUR_CC_HOST;
    jur_step(&r, &in, &o);
    CHECK(!o.vbus_on && !o.vbus_prep);
    CHECK(o.want == JUR_STACK_DEVICE && o.restart);
}

/* the restart waits until the stack ran JUR_MIN_RUN_MS: a flapping cable cannot loop restarts */
static void test_min_run(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 1, JUR_CC_HOST, 0, 0, 500, 0};
    struct jur_out o;
    jur_init(&r, JUR_STACK_HOST, JUR_TEST_PREP_MS, 500);
    jur_step(&r, &in, &o);
    CHECK(o.want == JUR_STACK_DEVICE && !o.restart && !o.vbus_on);
    in.now_ms = 500u + JUR_MIN_RUN_MS - 1u;
    jur_step(&r, &in, &o);
    CHECK(!o.restart);
    in.now_ms++;
    jur_step(&r, &in, &o);
    CHECK(o.restart);
}

/* HOST, device stack (a computer was on the cable), the computer unplugged: host after JUR_SETTLE_MS of nothing */
static void test_computer_unplugged(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 1, JUR_CC_HOST, 1, 1, 1000, 0};
    struct jur_out o;
    int restarted = 0;
    jur_init(&r, JUR_STACK_DEVICE, JUR_TEST_PREP_MS, 1000);
    o = run(&r, &in, 10000, &restarted, NULL);
    CHECK(!restarted && o.want == JUR_STACK_DEVICE && !o.vbus_on);
    CHECK(!o.port_write);                                         /* the computer keeps its link: no mode write */
    in.cc = JUR_CC_NONE;
    in.vbus = 0;
    jur_step(&r, &in, &o);
    CHECK(o.port_write && !o.restart && o.want == JUR_STACK_DEVICE);
    in.now_ms += JUR_SETTLE_MS - 10u;
    jur_step(&r, &in, &o);
    CHECK(!o.restart);
    in.now_ms += 10u;
    jur_step(&r, &in, &o);
    CHECK(o.restart && o.want == JUR_STACK_HOST && !o.vbus_on);   /* the restart itself sources nothing */
}

/* a device that powers VBUS itself (back-feeding through Q9): HOST_EN never turns on */
static void test_no_source_onto_power(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 1, JUR_CC_DEVICE, 1, 0, 1000, 0};
    int sourced = 0;
    jur_init(&r, JUR_STACK_HOST, JUR_TEST_PREP_MS, 1000);
    run(&r, &in, 5000, NULL, &sourced);
    CHECK(!sourced);
    /* the device stack running and a device attached: no 5 V before the restart into the host stack */
    jur_init(&r, JUR_STACK_DEVICE, JUR_TEST_PREP_MS, 1000);
    in.vbus = 0;
    in.now_ms = 1000;
    sourced = 0;
    run(&r, &in, 2000, NULL, &sourced);
    CHECK(!sourced);
}

/* the XIAO (no CC chip): HOST means the device first, then the host when no computer came; DEVICE never restarts */
static void test_no_cc(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 0, JUR_CC_NONE, 0, 0, 200, 0};
    struct jur_out o;
    int restarted = 0, sourced = 0;
    jur_init(&r, JUR_STACK_DEVICE, JUR_TEST_PREP_MS, 200);
    in.now_ms = 200u + JUR_COMPUTER_WAIT_MS - 10u;
    jur_step(&r, &in, &o);
    CHECK(!o.restart && o.want == JUR_STACK_DEVICE && !o.port_write);
    in.now_ms += 10u;
    jur_step(&r, &in, &o);
    CHECK(o.restart && o.want == JUR_STACK_HOST && !o.vbus_on && !o.vbus_prep);
    /* a computer configured it in time: stays a device */
    jur_init(&r, JUR_STACK_DEVICE, JUR_TEST_PREP_MS, 0);
    in.now_ms = 0;
    in.computer = 1;
    o = run(&r, &in, 20000, &restarted, &sourced);
    CHECK(!restarted && !sourced && o.want == JUR_STACK_DEVICE);
    /* started as host: stays host (no computer can show up on a host port) */
    jur_init(&r, JUR_STACK_HOST, JUR_TEST_PREP_MS, 0);
    in.now_ms = 0;
    in.computer = 0;
    o = run(&r, &in, 20000, &restarted, &sourced);
    CHECK(!restarted && !sourced && o.want == JUR_STACK_HOST);
    /* the setting back to DEVICE: a restart as device */
    in.setting = JUR_USB_DEVICE;
    jur_step(&r, &in, &o);
    CHECK(o.restart && o.want == JUR_STACK_DEVICE);
}

/* the setting changed to DEVICE while a device is attached and sourced: HOST_EN low and the sink mode written at once */
static void test_setting_to_device(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 1, JUR_CC_DEVICE, 0, 0, 1000, 0};
    struct jur_out o;
    jur_init(&r, JUR_STACK_HOST, JUR_TEST_PREP_MS, 1000);
    o = run(&r, &in, 4000, NULL, NULL);
    CHECK(o.vbus_on);
    in.setting = JUR_USB_DEVICE;
    jur_step(&r, &in, &o);
    CHECK(!o.vbus_on && !o.vbus_prep && o.port_mode == JUR_PORT_SINK && o.port_write && o.restart && o.want == JUR_STACK_DEVICE);
}

static void test_line(void)
{
    struct jur_line l;
    jur_line_init(&l, 0, 0);
    CHECK(jur_line_step(&l, 1, 10) == 0);
    CHECK(jur_line_step(&l, 1, 10u + JUR_LINE_DEBOUNCE_MS - 1u) == 0);
    CHECK(jur_line_step(&l, 1, 10u + JUR_LINE_DEBOUNCE_MS) == 1);
    CHECK(jur_line_step(&l, 0, 100) == 1);                      /* a short drop is a bounce */
    CHECK(jur_line_step(&l, 1, 120) == 1);
    CHECK(jur_line_step(&l, 1, 400) == 1);
    CHECK(jur_line_step(&l, 0, 500) == 1);
    CHECK(jur_line_step(&l, 0, 550) == 0);
}

static void test_source(void)
{
    CHECK(jur_source(JUR_SRC_AUTO, 1, 1) == JUR_AUDIO_LINE);
    CHECK(jur_source(JUR_SRC_AUTO, 0, 1) == JUR_AUDIO_USB);
    CHECK(jur_source(JUR_SRC_AUTO, 0, 0) == JUR_AUDIO_NONE);
    CHECK(jur_source(JUR_SRC_LINE, 0, 1) == JUR_AUDIO_LINE);
    CHECK(jur_source(JUR_SRC_USB, 1, 0) == JUR_AUDIO_USB);
    CHECK(jur_source(JUR_SRC_LINK, 0, 1) == JUR_AUDIO_NONE);    /* LINK: USB audio ignored ... */
    CHECK(jur_source(JUR_SRC_LINK, 1, 1) == JUR_AUDIO_LINE);    /* ... the line in's plug still wins */
    CHECK(jur_source(JUR_SRC_LINK, 1, 0) == JUR_AUDIO_LINE);
}

/* whose beat_latency is in use (core/jank_lat.h) */
static void test_lat_key(void)
{
    struct jla_key k = {9, 9, 9};
    CHECK(jur_lat_key(JUR_AUDIO_LINE, JUR_STACK_HOST, 1, 5, 6, &k) == 1 && k.kind == JLA_LINE);
    CHECK(jur_lat_key(JUR_AUDIO_USB, JUR_STACK_DEVICE, 0, 0, 0, &k) == 1 && k.kind == JLA_COMPUTER);
    CHECK(jur_lat_key(JUR_AUDIO_USB, JUR_STACK_HOST, 1, 0x1935, 0x000c, &k) == 1 && k.kind == JLA_HOSTED && k.vid == 0x1935 && k.pid == 0x000c);
    k.kind = JLA_LINE;
    CHECK(jur_lat_key(JUR_AUDIO_USB, JUR_STACK_HOST, 0, 0, 0, &k) == 0 && k.kind == JLA_LINE);   /* no device yet: as it was */
    CHECK(jur_lat_key(JUR_AUDIO_NONE, JUR_STACK_DEVICE, 0, 0, 0, &k) == 0 && k.kind == JLA_LINE); /* LINK: as it was */
}

/* a screenless unit's single NO: USB AUDIO (AUTO) <-> LINK; a plug in the line in: nothing changes */
static void test_source_toggle(void)
{
    int changed = 9;
    uint8_t s = jur_src_toggle(JUR_SRC_AUTO, 0, &changed);
    CHECK(s == JUR_SRC_LINK && changed == 1);
    s = jur_src_toggle(s, 0, &changed);
    CHECK(s == JUR_SRC_AUTO && changed == 1);
    CHECK(jur_src_toggle(JUR_SRC_AUTO, 1, &changed) == JUR_SRC_AUTO && changed == 0);
    CHECK(jur_src_toggle(JUR_SRC_LINK, 1, &changed) == JUR_SRC_LINK && changed == 0);
    CHECK(jur_src_toggle(JUR_SRC_USB, 0, &changed) == JUR_SRC_LINK && changed == 1);   /* from the page's USB or LINE */
    CHECK(jur_src_toggle(JUR_SRC_LINE, 0, &changed) == JUR_SRC_LINK);
    /* unplugged again: the line was only ever an override, the selection is what NO left */
    CHECK(jur_source(JUR_SRC_AUTO, 1, 1) == JUR_AUDIO_LINE && jur_source(JUR_SRC_AUTO, 0, 1) == JUR_AUDIO_USB);
}

static void test_names(void)
{
    uint8_t v = 9;
    CHECK(!jur_usb_parse("host", &v) && v == JUR_USB_HOST);
    CHECK(!jur_usb_parse("device", &v) && v == JUR_USB_DEVICE);
    CHECK(jur_usb_parse("otg", &v) && v == JUR_USB_DEVICE);
    CHECK(!jur_src_parse("usb", &v) && v == JUR_SRC_USB);
    CHECK(!jur_src_parse("auto", &v) && v == JUR_SRC_AUTO);
    CHECK(!jur_src_parse("link", &v) && v == JUR_SRC_LINK && !strcmp(jur_src_name(JUR_SRC_LINK), "link"));
    CHECK(jur_src_parse("adc", &v) && !strcmp(jur_src_name(4), "?"));
    CHECK(!strcmp(jur_usb_name(JUR_USB_HOST), "host") && !strcmp(jur_src_name(JUR_SRC_LINE), "line"));
    CHECK(!strcmp(jur_audio_name(JUR_AUDIO_NONE), "none") && !strcmp(jur_cc_name(JUR_CC_HOST), "host"));
    CHECK(!strcmp(jur_cc_name(9), "?"));
}

/* another port HAL (a power-bank IC whose boost is ready at once): prep_ms 0, VBUS on in the step the device attaches */
static void test_other_hal(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 1, JUR_CC_DEVICE, 0, 0, 1000, 0};
    struct jur_out o;
    jur_init(&r, JUR_STACK_HOST, 0, 1000);
    jur_step(&r, &in, &o);
    CHECK(o.vbus_prep && o.vbus_on);
    in.cc = JUR_CC_HOST;
    jur_step(&r, &in, &o);
    CHECK(!o.vbus_prep && !o.vbus_on);
}

/* after jank's own VBUS goes off, the charge left on the connector is not "outside power": no restart for it */
static void test_vbus_holdoff(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 1, JUR_CC_DEVICE, 0, 0, 1000, 0};
    struct jur_out o;
    int restarted = 0;
    jur_init(&r, JUR_STACK_HOST, JUR_TEST_PREP_MS, 1000);
    o = run(&r, &in, 4000, NULL, NULL);
    CHECK(o.vbus_on);
    in.cc = JUR_CC_NONE;
    in.vbus = 1;                                                /* the caps seen through Q9 */
    o = run(&r, &in, JUR_VBUS_HOLDOFF_MS - 30u, &restarted, NULL);
    CHECK(!restarted && o.want == JUR_STACK_HOST && !o.vbus_on);
    in.vbus = 0;
    o = run(&r, &in, 2000, &restarted, NULL);
    CHECK(!restarted && o.want == JUR_STACK_HOST);
    /* real outside power after the hold-off: a restart as device */
    in.vbus = 1;
    o = run(&r, &in, 100, &restarted, NULL);
    CHECK(restarted && o.want == JUR_STACK_DEVICE);
}

/* the HAL failed to prepare its source: VBUS stays off, and the source's time starts again when it works */
static void test_prep_failed(void)
{
    struct jur r;
    struct jur_in in = {JUR_USB_HOST, 1, JUR_CC_DEVICE, 0, 0, 1000, 1};
    struct jur_out o;
    int sourced = 0;
    jur_init(&r, JUR_STACK_HOST, JUR_TEST_PREP_MS, 1000);
    o = run(&r, &in, 500, NULL, &sourced);
    CHECK(!sourced && o.vbus_prep);
    in.prep_failed = 0;
    jur_step(&r, &in, &o);
    CHECK(!o.vbus_on);                                          /* not at once: the rail's time first */
    in.now_ms += JUR_TEST_PREP_MS;
    jur_step(&r, &in, &o);
    CHECK(o.vbus_on);
}

int main(void)
{
    test_lat_key();
    test_boot();
    test_device_setting();
    test_host_device_attach();
    test_host_then_charger();
    test_min_run();
    test_computer_unplugged();
    test_no_source_onto_power();
    test_no_cc();
    test_setting_to_device();
    test_line();
    test_source();
    test_source_toggle();
    test_names();
    test_other_hal();
    test_vbus_holdoff();
    test_prep_failed();
    printf("usbrole: %d checks, %d failed\n", checks, fails);
    return fails;
}
