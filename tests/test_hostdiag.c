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
    char out[2048];
    jhd_init(&d);
    CHECK(jhd_format(&d, out, sizeof out) == strlen(out));
    CHECK(strstr(out, "hostdiag_stack=unknown\n"));              /* nothing reported yet */
    CHECK(strstr(out, "hostdiag_filter=hub:0,skipped:0,last:0000:0000,learned:-\n"));      /* nothing learned: a dash */
    CHECK(strstr(out, "hostdiag_events=new:0,gone:0,add:0\n"));
    CHECK(strstr(out, "hostdiag_mem=heap:0,heap_min:0,largest:0,client_stack_free:0,fail_dma:0/0/0\n"));
    CHECK(!strstr(out, "hostdiag_dev"));                         /* no device lines for unused slots */
}

static void test_memory_line(void)
{
    struct jhd d;
    char out[2048];
    jhd_init(&d);
    d.heap_kb = 13;                                              /* internal free heap now, its minimum since boot, the client task's stack margin */
    d.heap_min_kb = 4;
    d.client_stack_free = 1800;
    d.heap_big_b = 7168;                                         /* the largest single block: what a stack or a DMA buffer needs in one piece */
    CHECK(jhd_format(&d, out, sizeof out) > 0);
    CHECK(strstr(out, "hostdiag_mem=heap:13,heap_min:4,largest:7168,client_stack_free:1800,fail_dma:0/0/0\n"));
    d.fail_dma_free = 9675;                                      /* at a failed claim: DMA heap free, its largest block, an aligned block could be had */
    d.fail_dma_big = 2944;
    d.fail_aligned = 1;
    CHECK(jhd_format(&d, out, sizeof out) > 0 && strstr(out, ",client_stack_free:1800,fail_dma:9675/2944/1\n"));
}

/* why a host start failed (kept apart in flash, so it outlives the restart that follows it) */
/* the internal heap at each stage of a host start, kept in RTC memory so that it survives the crash it explains */
static void test_trace_text(void)
{
    struct jhd_mark m[4];
    char out[200];
    memset(m, 0, sizeof m);
    CHECK(jhd_format_trace(m, 0, out, sizeof out) == 0 && out[0] == 0);                         /* nothing recorded: nothing printed */
    m[0].step = JHD_STEP_HOSTLIB;
    m[0].free_b = 21000;
    m[0].largest_b = 9000;
    m[1].step = JHD_STEP_RADIO;
    m[1].free_b = 11500;
    m[1].largest_b = 5800;
    m[2].step = JHD_STEP_BRIDGE;
    m[2].free_b = 5900;
    m[2].largest_b = 5800;
    CHECK(jhd_format_trace(m, 3, out, sizeof out) == strlen(out));
    CHECK(!strcmp(out, "hostdiag_trace=hostlib:21000/9000,radio:11500/5800,bridge:5900/5800\n"));
    m[3].step = JHD_STEP_BEAT;
    CHECK(jhd_format_trace(m, 4, out, sizeof out) > 0 && strstr(out, ",beat:0/0\n"));
    m[3].step = 200;
    CHECK(jhd_format_trace(m, 4, out, sizeof out) > 0 && strstr(out, ",?:0/0\n"));               /* a step this build does not know */
    CHECK(jhd_format_trace(m, 4, out, 30) < 30 && out[strlen(out)] == 0);                        /* cut to fit, still a string */
    CHECK(jhd_format_trace(m, 4, out, 0) == 0);
    {
        struct jhd_mark big[JHD_TRACE_MAX + 5];
        char wide[600];
        memset(big, 0, sizeof big);
        for (unsigned i = 0; i < JHD_TRACE_MAX + 5u; i++)
            big[i].step = JHD_STEP_HOSTLIB;
        CHECK(jhd_format_trace(big, JHD_TRACE_MAX + 5u, wide, sizeof wide) < sizeof wide);        /* more marks than the trace holds: the first JHD_TRACE_MAX */
        CHECK(strstr(wide, "hostlib:0/0") != NULL);
    }
}

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
    CHECK(jhd_format_failure(JHD_STEP_BRIDGE, 257, out, sizeof out) > 0 && !strcmp(out, "hostdiag_start=failed:bridge,err:257\n"));
    CHECK(jhd_format_failure(JHD_STEP_BEAT, 257, out, sizeof out) > 0 && !strcmp(out, "hostdiag_start=failed:beat,err:257\n"));
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
    char out[2048];
    jhd_init(&d);
    d.stack = JHD_STACK_HOST;
    d.lib_up = 1;
    d.lib_devs = 2;
    d.hprt = 0x00021401u;              /* connected, not enabled, line state J (1), powered, full speed (1) */
    d.filter.hub_seen = 1;             /* a hub came, and two devices behind it were left alone (the last one's IDs) */
    d.filter.skipped = 2;
    d.filter.skip_vid = 0x25A4;
    d.filter.skip_pid = 0x9311;
    jhf_set_add(&d.filter.deny, 0x14CD, 0x1212);
    jhf_set_add(&d.filter.deny, 0x25A4, 0x9311);
    d.new_dev = 3;
    d.gone_dev = 1;
    d.add_calls = 3;
    d.lpx_up = 1;
    d.programmer_sent = 2;
    d.programmer_acked = 1;
    d.setup_sent = 2;
    d.lights_sent = 243;
    d.lp_mode = 1;                     /* found in Live mode once, put back in Programmer mode */
    d.lp_reentered = 1;
    d.pads = 7;
    d.dn2_msgs = 9;
    d.live_state = 2;
    d.live_play = 1;
    d.live_asked = 12;
    d.live_replies = 11;
    d.live_plays = 5;
    d.live_selects = 3;
    d.live_pushes = 4;
    d.live_notes = 6;
    d.live_dropped = 1;
    d.live_reads = 8;
    d.live_msgs = 10;
    d.live_msgs_sent = 7;
    d.live_msg_ok = 1;
    d.live_scale_reads = 55;
    d.live_scale_sets = 4;
    d.dn2_scale = 6;
    d.dn2_root = 9;
    d.dn2_root_known = 1;
    d.scale_ok = 1;
    d.scale_set_ok = 0;
    d.live_mixer_reads = 70;
    d.live_mixer_sets = 5;
    d.live_edits = 12;
    d.live_pagereads = 340;
    d.live_step_busy = 2;
    d.live_step_refused = 1;
    d.live_ignored = 3;
    d.live_ign_len = 41;
    d.dn2_proto = 3;
    d.dn2_push = 1;
    d.dn2_lease = 9;
    d.dn2_made = 100;
    d.dn2_sent = 96;
    d.dn2_lost = 4;
    d.dn2_dropped = 2;
    d.dn2_pushmsgs = 30;
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
    d.dev[1].out_mps = 64;
    d.dev[1].in_mps = 512;
    d.dev[1].neps = 2;
    d.dev[1].clamped = 2;
    d.dev[1].claim_where = 3;
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
    CHECK(strstr(out, "hostdiag_lib=up\nhostdiag_libdevs=2\n"));   /* the library's own device count (a hub counts), next to its state */
    CHECK(strstr(out, "hostdiag_port=hprt:0x00021401,conn:1,ena:0,lnsts:1,pwr:1,spd:1\n"));   /* the root port's register, decoded */
    CHECK(strstr(out, "hostdiag_filter=hub:1,skipped:2,last:25A4:9311,learned:14CD:1212+25A4:9311\n"));   /* what the enumeration rule left alone, and its list */
    CHECK(strstr(out, "hostdiag_events=new:3,gone:1,add:3\n"));
    CHECK(strstr(out, "hostdiag_lp=lpx:1,dn2:0,programmer:2,acked:1,setup:2,mode:live,reentered:1,lights:243,pads:7,dn2msgs:9\n"));
    CHECK(strstr(out, "hostdiag_live=state:live,play:1,asked:12,replies:11,plays:5,selects:3,pushes:4,notes:6,dropped:1,reads:8,msgs:10,msgs_sent:7,msg_ok:1\n"));
    CHECK(strstr(out, "hostdiag_scale=dn2:6,root:9,reads:55,sets:4,read_ok:1,set_ok:0\n"));
    d.dn2_root_known = 0;                                         /* no root known (protocol 4): -1 */
    jhd_format(&d, out, sizeof out);
    CHECK(strstr(out, "hostdiag_scale=dn2:6,root:-1,"));
    d.dn2_root_known = 1;
    CHECK(strstr(out, "hostdiag_steps=edits:12,reads:340,busy:2,refused:1,mixreads:70,mixsets:5\n"));
    CHECK(strstr(out, "hostdiag_dn2live=proto:3,push:1,lease:9,made:100,sent:96,lost:4,dropped:2,pushmsgs:30,ignored:3,last_ignored_len:41\n"));
    d.live_state = 9;                  /* a state number from a newer build prints as ?, not past the list */
    jhd_format(&d, out, sizeof out);
    CHECK(strstr(out, "hostdiag_live=state:?,"));
    d.live_state = 2;
    CHECK(strstr(out, "hostdiag_dev1=Launchpad X,1235:0103,role:lpx,ifs:2,use:1,out:0x02,in:0x82,mps:64/512,eps:2,clamp:2,claim:0,where:3,in_pkts:42,in_err:2,in_status:4,out_done:100,out_fail:1,last:09 90 0B 7F\n"));
    CHECK(!strstr(out, "hostdiag_dev0"));
}

/* the Session page's line: PATTERNS reads, PATTERN requests, not refused (reads / cue), the playing pattern; short, `diag` has 2040 bytes in all */
static void test_the_pattern_line(void)
{
    struct jhd d;
    char out[2048];
    const char *l;
    jhd_init(&d);
    CHECK(jhd_format(&d, out, sizeof out) > 0 && strstr(out, "hostdiag_pat=reads:0,cues:0,ok:0/0,playing:-1\n"));   /* nothing read yet: -1 */
    d.live_pat_reads = 120;
    d.live_pat_cues = 4;
    d.pat_reads_ok = 1;
    d.pat_cue_ok = 0;                  /* a Digitone without PATTERN 60 */
    d.dn2_pattern = 37;
    CHECK(jhd_format(&d, out, sizeof out) > 0 && strstr(out, "hostdiag_pat=reads:120,cues:4,ok:1/0,playing:37\n"));
    CHECK(strstr(out, "hostdiag_steps=") < strstr(out, "hostdiag_pat=") && strstr(out, "hostdiag_pat=") < strstr(out, "hostdiag_dn2live="));
    CHECK(strstr(out, "hostdiag_rt=start:0,stop:0,due:0,clock:0,gated_start:0,gated_stop:0\n") && strstr(out, "hostdiag_rt=") < strstr(out, "hostdiag_pat="));   /* the realtime bytes to the hosted devices */
    d.live_pat_reads = 0xFFFFFFFFu;    /* at its longest */
    d.live_pat_cues = 0xFFFFFFFFu;
    d.dn2_pattern = 127;
    jhd_format(&d, out, sizeof out);
    l = strstr(out, "hostdiag_pat=");
    CHECK(l && strchr(l, '\n') && (size_t)(strchr(l, '\n') - l) + 1u <= 66u);
    d.dn2_pattern = 200;               /* not a pattern: -1 */
    jhd_format(&d, out, sizeof out);
    CHECK(strstr(out, ",playing:-1\n") != NULL);
}

/* stage 8's line: TRANSPORT sent / not refused / the Digitone's state, peers, the Digitone's keys followed, CHAIN sent / not refused */
static void test_the_transport_line(void)
{
    struct jhd d;
    char out[2048];
    const char *l;
    jhd_init(&d);
    CHECK(jhd_format(&d, out, sizeof out) > 0 && strstr(out, "hostdiag_tr=sent:0,ok:0,dn2:-1,peers:0,follows:0,chains:0,chain_ok:0\n"));   /* nothing read: -1 */
    d.tr_sent = 12;
    d.tr_ok = 1;
    d.dn2_tr = 2;                      /* paused */
    d.tr_peers = 1;
    d.tr_follows = 3;
    d.chain_sent = 5;
    d.chain_ok = 1;
    CHECK(jhd_format(&d, out, sizeof out) > 0 && strstr(out, "hostdiag_pat=") < strstr(out, "hostdiag_tr=sent:12,ok:1,dn2:2,peers:1,follows:3,chains:5,chain_ok:1\n"));
    d.tr_sent = d.tr_follows = d.chain_sent = 0xFFFFFFFFu;   /* at its longest */
    d.dn2_tr = 9;                      /* not a state: -1 */
    jhd_format(&d, out, sizeof out);
    l = strstr(out, "hostdiag_tr=");
    CHECK(l && strstr(l, ",dn2:-1,") && (size_t)(strchr(l, '\n') - l) + 1u <= 105u);
}

static void test_roles_and_claim_error(void)
{
    struct jhd d;
    char out[2048];
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
    test_trace_text();
    test_failure_text();
    test_full_record();
    test_the_pattern_line();
    test_the_transport_line();
    test_roles_and_claim_error();
    test_truncation_and_hostile_names();
    test_valid_blob();
    printf("test_hostdiag: %d checks, %d failed\n", checks, fails);
    return fails;
}
