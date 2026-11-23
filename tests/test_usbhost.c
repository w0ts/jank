/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the mini 3's USB host gate (core/jank_usbhost.c) and of how it sits under the role core
 * (core/jank_usbrole.c): HOST_EN, the role. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_usbhost.h"
#include "jank_usbrole.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define STEP 20u                       /* the role task's period */

/* the gate alone: 5 V on since t = 0, the role core always asks for the signal */
struct rig {
    struct juh h;
    struct juh_in in;
    struct juh_out o;
};

static void rig_init(struct rig *r, int setting_host, int vbus_raw)
{
    memset(r, 0, sizeof *r);
    juh_init(&r->h, vbus_raw, 0);
    r->in.setting_host = (uint8_t)setting_host;
    r->in.vbus_raw = (uint8_t)vbus_raw;
    r->in.want_en = 1;
    r->in.v5_known = 1;
    r->in.v5_since_ms = 0;
}

static void run(struct rig *r, uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += STEP) {
        r->in.now_ms += STEP;
        juh_step(&r->h, &r->in, &r->o);
    }
}

static void test_idle_low(void)
{
    struct rig r;
    rig_init(&r, 1, 0);
    juh_step(&r.h, &r.in, &r.o);
    CHECK(!r.o.host_en);                                    /* the first step: still inside the debounce */
    run(&r, JUH_DEBOUNCE_MS - STEP);
    CHECK(!r.o.host_en);
    run(&r, 2 * STEP);
    CHECK(r.o.host_en && r.o.mode_host && !r.o.vbus);       /* after the debounce: host, HOST_EN high */
}

static void test_setting_device_never(void)
{
    struct rig r;
    rig_init(&r, 0, 0);
    for (int i = 0; i < 200; i++) {
        run(&r, 100);
        CHECK(!r.o.host_en && !r.o.mode_host);
    }
}

static void test_power_up_with_vbus(void)
{
    struct rig r;
    rig_init(&r, 1, 1);
    run(&r, 5000);
    CHECK(!r.o.host_en && !r.o.mode_host && r.o.vbus);      /* a computer there at power-up: device */
}

static void test_computer_while_hosting(void)
{
    struct rig r;
    rig_init(&r, 1, 0);
    run(&r, 1000);
    CHECK(r.o.host_en);
    r.in.vbus_raw = 1;                                      /* plugged in */
    run(&r, STEP);                                          /* ONE step */
    CHECK(!r.o.host_en && !r.o.mode_host && r.o.vbus);      /* HOST_EN down at once, the role to device */
    run(&r, 10000);
    CHECK(!r.o.host_en);
}

static void test_unplug_back_to_host(void)
{
    struct rig r;
    rig_init(&r, 1, 1);
    run(&r, 2000);
    r.in.vbus_raw = 0;
    run(&r, JUH_DEBOUNCE_MS - STEP);
    CHECK(!r.o.host_en && r.o.vbus);                        /* not yet: the debounce */
    run(&r, 2 * STEP);
    CHECK(r.o.host_en && !r.o.vbus && r.o.mode_host);
    r.in.setting_host = 0;                                  /* the menu says DEVICE: down at once */
    run(&r, STEP);
    CHECK(!r.o.host_en && !r.o.mode_host);
    r.in.setting_host = 1;                                  /* and back: HOST_EN follows, VBUS being low */
    run(&r, STEP);
    CHECK(r.o.host_en);
    r.in.setting_host = 0;
    r.in.vbus_raw = 0;
    run(&r, 3000);
    CHECK(!r.o.host_en);
}

static void test_unplug_with_setting_device(void)
{
    struct rig r;
    rig_init(&r, 0, 1);
    run(&r, 1000);
    r.in.vbus_raw = 0;
    run(&r, 5000);
    CHECK(!r.o.host_en && !r.o.mode_host);
}

/* a bouncing connector: VBUS flaps with a gap under the debounce -> HOST_EN never rises; flapping after a rise drops it at
 * once and it rises again only after a whole quiet debounce */
static void test_flapping(void)
{
    struct rig r;
    int rises = 0, prev = 0;
    rig_init(&r, 1, 1);
    for (int i = 0; i < 400; i++) {                         /* high 40 ms, low 460 ms (just under the debounce), over and over */
        r.in.vbus_raw = (uint8_t)((i % 25) < 2);
        run(&r, STEP);
        CHECK(!r.o.host_en || !r.in.vbus_raw);              /* never high while the raw read is high */
        rises += r.o.host_en && !prev;
        prev = r.o.host_en;
    }
    CHECK(rises == 0);
    /* a gap of exactly the debounce: one rise per gap at most */
    rises = 0;
    prev = 0;
    for (int i = 0; i < 400; i++) {
        r.in.vbus_raw = (uint8_t)((i % 30) < 1);            /* high 20 ms, low 580 ms */
        run(&r, STEP);
        CHECK(!r.o.host_en || !r.in.vbus_raw);
        rises += r.o.host_en && !prev;
        prev = r.o.host_en;
    }
    CHECK(rises > 0 && rises <= 400 / 30 + 1);              /* at most one rise per period */
    /* once up, every high read pulls it down in that step */
    r.in.vbus_raw = 0;
    run(&r, 2000);
    CHECK(r.o.host_en);
    r.in.vbus_raw = 1;
    run(&r, STEP);
    CHECK(!r.o.host_en);
    r.in.vbus_raw = 0;
    run(&r, STEP);
    CHECK(!r.o.host_en);                                    /* a single low read does not bring it back */
}

static void test_v5(void)
{
    struct rig r;
    rig_init(&r, 1, 0);
    r.in.v5_known = 0;
    run(&r, 5000);
    CHECK(!r.o.host_en);                                    /* no 5 V rail known: never */
    r.in.v5_known = 1;
    r.in.v5_since_ms = r.in.now_ms;                         /* on just now */
    run(&r, STEP);
    CHECK(!r.o.host_en);
    run(&r, JUH_V5_SETTLE_MS);
    CHECK(r.o.host_en);
    r.in.want_en = 0;                                       /* the role core does not ask */
    run(&r, STEP);
    CHECK(!r.o.host_en);
}

static void test_wraparound(void)
{
    struct rig r;
    rig_init(&r, 1, 0);
    r.in.now_ms = 0xFFFFFF00u;
    juh_init(&r.h, 0, r.in.now_ms);
    r.in.v5_since_ms = 0xFFFFFE00u;
    run(&r, 1000);                                          /* across the 32-bit wrap */
    CHECK(r.o.host_en);
}

/* the gate under the role core with a port that reports cc = host when VBUS sense is up, else a device (usbport_mini3.c) */
struct sys {
    struct juh h;
    struct jur r;
    struct juh_in gi;
    struct juh_out go;
    struct jur_in ri;
    struct jur_out ro;
    uint8_t want_en;
    int restarts;
};

static void sys_step(struct sys *s, uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += STEP) {
        s->gi.now_ms += STEP;
        s->gi.want_en = s->want_en;
        juh_step(&s->h, &s->gi, &s->go);                    /* the port's read(): sample, gate, drive HOST_EN */
        s->ri.now_ms = s->gi.now_ms;
        s->ri.cc = s->go.vbus ? JUR_CC_HOST : JUR_CC_DEVICE;
        s->ri.vbus = s->gi.vbus_raw;
        jur_step(&s->r, &s->ri, &s->ro);                    /* the role core */
        s->want_en = s->ro.vbus_on;                         /* vbus_on(): the role core's ask */
        s->gi.want_en = s->want_en;
        juh_step(&s->h, &s->gi, &s->go);                    /* vbus_on() re-evaluates at once */
        if (s->ro.restart)
            s->restarts++;
    }
}

static void sys_init(struct sys *s, int host, int vbus_raw, uint8_t stack)
{
    memset(s, 0, sizeof *s);
    juh_init(&s->h, vbus_raw, 0);
    jur_init(&s->r, stack, JUH_V5_SETTLE_MS, 0);
    s->gi.setting_host = (uint8_t)host;
    s->gi.vbus_raw = (uint8_t)vbus_raw;
    s->gi.v5_known = 1;
    s->ri.setting = host ? JUR_USB_HOST : JUR_USB_DEVICE;
    s->ri.has_port = 1;
}

static void test_system(void)
{
    struct sys s;
    struct jur_boot b = {JUR_USB_HOST, 1, JUR_CC_DEVICE, 0, 0};
    CHECK(jur_boot_stack(&b) == JUR_STACK_HOST);            /* VBUS low at the start, setting HOST: the host stack */
    b.cc = JUR_CC_HOST;
    b.vbus = 1;
    CHECK(jur_boot_stack(&b) == JUR_STACK_DEVICE);          /* VBUS high at the start: the device stack */

    sys_init(&s, 1, 0, JUR_STACK_HOST);
    sys_step(&s, 2000);
    CHECK(s.go.host_en && s.restarts == 0);                 /* nothing on the LCD-2's USB-C, HOST: HOST_EN high */
    s.gi.vbus_raw = 1;                                      /* a computer */
    sys_step(&s, STEP);
    CHECK(!s.go.host_en);                                   /* in the same step */
    sys_step(&s, 5000);
    CHECK(!s.go.host_en && s.restarts > 0);                 /* and the role core restarts into the device stack */

    sys_init(&s, 1, 1, JUR_STACK_DEVICE);                   /* power-up with a computer there */
    sys_step(&s, 20000);
    CHECK(!s.go.host_en && s.restarts == 0);
    s.gi.vbus_raw = 0;                                      /* unplugged: the host stack comes after the debounce */
    sys_step(&s, JUH_DEBOUNCE_MS - STEP);
    CHECK(!s.go.host_en && s.restarts == 0);
    sys_step(&s, 3000);
    CHECK(s.restarts > 0 && !s.go.host_en);                 /* a restart into host; HOST_EN waits for the host stack */

    sys_init(&s, 0, 0, JUR_STACK_DEVICE);                   /* the setting DEVICE */
    sys_step(&s, 20000);
    CHECK(!s.go.host_en && s.restarts == 0);
}

/* the setting the role core is given: with no port chip (the XIAO) and no choice ever made, HOST (the device first, the host stack when no
 * computer configures it in time); a choice, or a port chip (its HOST also drives 5 V onto the cable), is the setting as it is */
static void test_effective_setting(void)
{
    CHECK(jur_effective_setting(JUR_USB_DEVICE, 0, 0) == JUR_USB_HOST);      /* the XIAO's default */
    CHECK(jur_effective_setting(JUR_USB_HOST, 0, 0) == JUR_USB_HOST);
    CHECK(jur_effective_setting(JUR_USB_DEVICE, 1, 0) == JUR_USB_DEVICE);    /* an explicit DEVICE: never host */
    CHECK(jur_effective_setting(JUR_USB_HOST, 1, 0) == JUR_USB_HOST);
    CHECK(jur_effective_setting(JUR_USB_DEVICE, 0, 1) == JUR_USB_DEVICE);    /* a port chip: DEVICE until HOST is chosen */
    CHECK(jur_effective_setting(JUR_USB_HOST, 0, 1) == JUR_USB_HOST);
    CHECK(jur_effective_setting(JUR_USB_DEVICE, 1, 1) == JUR_USB_DEVICE);
    CHECK(jur_effective_setting(JUR_USB_HOST, 1, 1) == JUR_USB_HOST);
    CHECK(jur_effective_setting(7, 0, 0) == JUR_USB_HOST && jur_effective_setting(7, 1, 0) == JUR_USB_DEVICE);   /* a stray value: as DEVICE */
}

/* the whole boot on a XIAO with nothing chosen: a start with no computer ends as host after the wait, one with a computer stays a device */
static void test_xiao_default_end_to_end(void)
{
    struct jur r;
    struct jur_in in;
    struct jur_out o;
    struct jur_boot b = {0};
    b.setting = jur_effective_setting(JUR_USB_DEVICE, 0, 0);
    CHECK(jur_boot_stack(&b) == JUR_STACK_DEVICE);                           /* the device first, always */
    jur_init(&r, JUR_STACK_DEVICE, 0, 0);
    memset(&in, 0, sizeof in);
    in.setting = jur_effective_setting(JUR_USB_DEVICE, 0, 0);
    in.has_port = 0;
    in.now_ms = JUR_COMPUTER_WAIT_MS - 1u;
    jur_step(&r, &in, &o);
    CHECK(o.want == JUR_STACK_DEVICE && !o.restart);                         /* still waiting for a computer */
    in.now_ms = JUR_COMPUTER_WAIT_MS + 1u;
    jur_step(&r, &in, &o);
    CHECK(o.want == JUR_STACK_HOST && o.restart);                            /* none came: restart as host */
    in.computer = 1;                                                         /* but a computer that did configure it keeps it a device */
    jur_step(&r, &in, &o);
    CHECK(o.want == JUR_STACK_DEVICE && !o.restart);
}

int main(void)
{
    test_effective_setting();
    test_xiao_default_end_to_end();
    test_idle_low();
    test_setting_device_never();
    test_power_up_with_vbus();
    test_computer_while_hosting();
    test_unplug_back_to_host();
    test_unplug_with_setting_device();
    test_flapping();
    test_v5();
    test_wraparound();
    test_system();
    printf("usbhost: %d checks, %d failed\n", checks, fails);
    return fails;
}
