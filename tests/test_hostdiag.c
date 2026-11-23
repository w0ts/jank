/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the host-mode probe record's text (core/jank_hostdiag.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_hostdiag.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_empty_record(void)
{
    struct jhd d;
    char out[512];
    jhd_init(&d);
    CHECK(jhd_format(&d, out, sizeof out) == strlen(out));
    CHECK(strstr(out, "hostdiag_stack=unknown\n"));              /* nothing reported yet */
    CHECK(strstr(out, "hostdiag_events=new:0,gone:0,add:0\n"));
    CHECK(strstr(out, "hostdiag_mem=heap:0,heap_min:0,client_stack_free:0\n"));
    CHECK(!strstr(out, "hostdiag_dev"));                         /* no device lines for unused slots */
}

static void test_memory_line(void)
{
    struct jhd d;
    char out[512];
    jhd_init(&d);
    d.heap_kb = 13;                                              /* internal free heap now, its minimum since boot, the client task's stack margin */
    d.heap_min_kb = 4;
    d.client_stack_free = 1800;
    CHECK(jhd_format(&d, out, sizeof out) > 0);
    CHECK(strstr(out, "hostdiag_mem=heap:13,heap_min:4,client_stack_free:1800\n"));
}

/* why a host start failed (kept apart in flash, so it outlives the restart that follows it) */
static void test_failure_text(void)
{
    char out[128];
    CHECK(jhd_format_failure(JHD_STEP_NONE, 0, out, sizeof out) == strlen(out) && !strcmp(out, "hostdiag_start=ok\n"));
    CHECK(jhd_format_failure(JHD_STEP_CLIENT_TASK, 0x101, out, sizeof out) > 0 && !strcmp(out, "hostdiag_start=failed:client_task,err:257\n"));
    CHECK(jhd_format_failure(JHD_STEP_RADIO, -1, out, sizeof out) > 0 && !strcmp(out, "hostdiag_start=failed:radio,err:-1\n"));
    CHECK(jhd_format_failure(JHD_STEP_HOSTLIB, 0x103, out, sizeof out) > 0 && strstr(out, "failed:hostlib,err:259"));
    CHECK(jhd_format_failure(JHD_STEP_CLIENT, 1, out, sizeof out) > 0 && strstr(out, "failed:client,"));
    CHECK(jhd_format_failure(JHD_STEP_MIDI_TASK, 1, out, sizeof out) > 0 && strstr(out, "failed:midi_task,"));
    CHECK(jhd_format_failure(JHD_STEP_ROLE, 1, out, sizeof out) > 0 && strstr(out, "failed:role,"));
    /* a host run that did not last (a panic, a watchdog, a brownout, seen at the next start): err is esp_reset_reason()'s code */
    CHECK(jhd_format_failure(JHD_STEP_CRASH, 4, out, sizeof out) > 0 && !strcmp(out, "hostdiag_start=failed:crash,err:4\n"));
    CHECK(jhd_format_failure(JHD_STEP_CRASH, 9, out, sizeof out) > 0 && strstr(out, "failed:crash,err:9"));
    CHECK(jhd_format_failure(JHD_STEP_NONE + 99, 5, out, sizeof out) > 0 && strstr(out, "failed:?,err:5"));   /* a step this build does not know */
    CHECK(jhd_format_failure(200, 5, out, sizeof out) > 0 && strstr(out, "failed:?,err:5"));      /* a step this build does not know */
    CHECK(jhd_format_failure(JHD_STEP_RADIO, 1, out, 12) < 12 && out[strlen(out)] == 0);          /* cut to fit, still a string */
    CHECK(jhd_format_failure(JHD_STEP_RADIO, 1, out, 0) == 0);
}

static void test_full_record(void)
{
    struct jhd d;
    char out[1024];
    jhd_init(&d);
    d.stack = JHD_STACK_HOST;
    d.lib_up = 1;
    d.new_dev = 3;
    d.gone_dev = 1;
    d.add_calls = 3;
    d.lpx_up = 1;
    d.programmer_sent = 1;
    d.pads = 7;
    d.dn2_msgs = 9;
    d.dev[1].used = 1;
    strcpy(d.dev[1].name, "Launchpad X");
    d.dev[1].vid = 0x1235;
    d.dev[1].pid = 0x0103;
    d.dev[1].role = 1;
    d.dev[1].ifs = 2;
    d.dev[1].itf_index = 1;
    d.dev[1].out_ep = 0x02;
    d.dev[1].in_ep = 0x82;
    d.dev[1].claim_err = 0;
    d.dev[1].in_packets = 42;
    d.dev[1].in_errors = 2;
    d.dev[1].in_last_status = 4;
    d.dev[1].out_done = 100;
    d.dev[1].out_fail = 1;
    d.dev[1].in_last[0] = 0x09;
    d.dev[1].in_last[1] = 0x90;
    d.dev[1].in_last[2] = 0x0B;
    d.dev[1].in_last[3] = 0x7F;
    CHECK(jhd_format(&d, out, sizeof out) == strlen(out));
    CHECK(strstr(out, "hostdiag_stack=host\n") && strstr(out, "hostdiag_lib=up\n"));
    CHECK(strstr(out, "hostdiag_events=new:3,gone:1,add:3\n"));
    CHECK(strstr(out, "hostdiag_lp=lpx:1,dn2:0,programmer:1,pads:7,dn2msgs:9\n"));
    CHECK(strstr(out, "hostdiag_dev1=Launchpad X,1235:0103,role:lpx,ifs:2,use:1,out:0x02,in:0x82,claim:0,in_pkts:42,in_err:2,in_status:4,out_done:100,out_fail:1,last:09 90 0B 7F\n"));
    CHECK(!strstr(out, "hostdiag_dev0"));
}

static void test_roles_and_claim_error(void)
{
    struct jhd d;
    char out[1024];
    jhd_init(&d);
    d.dev[0].used = 1;
    strcpy(d.dev[0].name, "Digitone");
    d.dev[0].role = 2;
    d.dev[0].claim_err = -259;                                   /* an ESP error code: negative numbers print as they are */
    d.dev[2].used = 1;
    d.dev[2].role = 0;
    CHECK(jhd_format(&d, out, sizeof out) > 0);
    CHECK(strstr(out, "role:dn2") && strstr(out, "claim:-259"));
    CHECK(strstr(out, "hostdiag_dev2=?,0000:0000,role:other"));  /* a device with no name: ? */
}

static void test_truncation_and_hostile_names(void)
{
    struct jhd d;
    char out[40], big[2048];
    size_t n;
    jhd_init(&d);
    d.dev[0].used = 1;
    memset(d.dev[0].name, 'A', JHD_NAME_MAX);                    /* a full name, then a NUL */
    d.dev[0].name[JHD_NAME_MAX] = 0;
    n = jhd_format(&d, out, sizeof out);
    CHECK(n < sizeof out && out[n] == 0 && n == strlen(out));    /* a small buffer: cut, still a string */
    CHECK(jhd_format(&d, out, 0) == 0);                          /* no room at all: nothing written, no crash */
    memset(d.dev[0].name, 'B', sizeof d.dev[0].name);            /* no NUL at all: never read past the field */
    n = jhd_format(&d, big, sizeof big);
    CHECK(n == strlen(big) && n < sizeof big);
    d.dev[0].name[0] = '\n';                                     /* control characters would break the line layer: shown as '.' */
    d.dev[0].name[1] = 0;
    jhd_format(&d, big, sizeof big);
    CHECK(strstr(big, "hostdiag_dev0=.,"));
}

static void test_valid_blob(void)
{
    struct jhd d, back;
    jhd_init(&d);
    d.new_dev = 5;
    CHECK(jhd_valid(&d, sizeof d));
    memcpy(&back, &d, sizeof d);
    CHECK(jhd_valid(&back, sizeof back) && back.new_dev == 5);
    CHECK(!jhd_valid(&d, sizeof d - 1));                         /* a blob of another size is not ours */
    back.version ^= 1;
    CHECK(!jhd_valid(&back, sizeof back));                       /* nor one of another version */
}

int main(void)
{
    test_empty_record();
    test_memory_line();
    test_failure_text();
    test_full_record();
    test_roles_and_claim_error();
    test_truncation_and_hostile_names();
    test_valid_blob();
    printf("test_hostdiag: %d checks, %d failed\n", checks, fails);
    return fails;
}
