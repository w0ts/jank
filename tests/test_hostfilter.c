/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the host enumeration rule and its sets (core/jank_hostfilter.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_hostfilter.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_set(void)
{
    struct jhf_set s, t;
    memset(&s, 0, sizeof s);
    memset(&t, 0, sizeof t);
    CHECK(!jhf_set_has(&s, 0x14CD, 0x1212));
    CHECK(jhf_set_add(&s, 0x14CD, 0x1212));
    CHECK(jhf_set_has(&s, 0x14CD, 0x1212));
    CHECK(!jhf_set_has(&s, 0x14CD, 0x1213) && !jhf_set_has(&s, 0x24CD, 0x1212));   /* both halves of the ID count */
    CHECK(!jhf_set_add(&s, 0x14CD, 0x1212) && s.n == 1);                          /* no duplicate */
    jhf_set_add(&t, 0x14CD, 0x1212);
    jhf_set_add(&t, 0x25A4, 0x9311);
    CHECK(jhf_set_merge(&s, &t) == 1 && s.n == 2 && jhf_set_has(&s, 0x25A4, 0x9311));   /* only the new one counts */
    CHECK(jhf_set_merge(&s, &t) == 0);                                           /* merging again changes nothing: no restart for it */
}

static void test_set_is_bounded(void)
{
    struct jhf_set s;
    memset(&s, 0, sizeof s);
    for (unsigned i = 0; i < JHF_MAX; i++)
        CHECK(jhf_set_add(&s, 0x1000, (uint16_t)i));
    CHECK(!jhf_set_add(&s, 0x2000, 1) && s.n == JHF_MAX);                         /* a full set adds nothing: the restarts end */
    CHECK(!jhf_set_has(&s, 0x2000, 1));
}

static void test_no_hub_nothing_is_skipped(void)
{
    struct jhf f;
    memset(&f, 0, sizeof f);
    jhf_set_add(&f.deny, 0x14CD, 0x1212);
    CHECK(jhf_proceed(&f, 0, 0x14CD, 0x1212));           /* listed, but on the port itself: no pipes shared with anyone */
    CHECK(f.skipped == 0 && !f.hub_seen);
}

static void test_behind_a_hub_the_listed_are_skipped(void)
{
    struct jhf f;
    memset(&f, 0, sizeof f);
    jhf_set_add(&f.deny, 0x14CD, 0x1212);                /* the dock's card reader */
    jhf_set_add(&f.deny, 0x25A4, 0x9311);                /* its video adapter */
    CHECK(jhf_proceed(&f, JHF_CLASS_HUB, 0x05E3, 0x0610));
    CHECK(f.hub_seen);
    CHECK(!jhf_proceed(&f, 0, 0x14CD, 0x1212));
    CHECK(f.skipped == 1 && f.skip_vid == 0x14CD && f.skip_pid == 0x1212);
    CHECK(!jhf_proceed(&f, 0, 0x25A4, 0x9311));
    CHECK(f.skipped == 2 && f.skip_vid == 0x25A4 && f.skip_pid == 0x9311);
    CHECK(jhf_proceed(&f, 0, 0x1935, 0x0134));           /* the Digitone II: not listed */
    CHECK(jhf_proceed(&f, 0, 0x1235, 0x0108));           /* the Launchpad X */
    CHECK(jhf_proceed(&f, 0, 0x0499, 0x1000));           /* any other device (a MIDI interface): not listed, so it comes as before */
    CHECK(f.skipped == 2);
}

static void test_a_hub_is_always_enumerated(void)
{
    struct jhf f;
    memset(&f, 0, sizeof f);
    jhf_set_add(&f.deny, 0x1111, 0x2222);
    CHECK(jhf_proceed(&f, JHF_CLASS_HUB, 0x1111, 0x2222));   /* even a listed ID: a hub's devices need it */
    CHECK(jhf_proceed(&f, JHF_CLASS_HUB, 0x3333, 0x4444));
    CHECK(f.skipped == 0);
}

static void test_stored_form(void)
{
    struct jhf_set s, back;
    uint8_t b[JHF_PACK_MAX];
    size_t n;
    memset(&s, 0, sizeof s);
    n = jhf_pack(&s, b);
    CHECK(n == 1 && b[0] == 0);                          /* the empty set */
    CHECK(jhf_unpack(&back, b, n) && back.n == 0);
    jhf_set_add(&s, 0x14CD, 0x1212);
    jhf_set_add(&s, 0x25A4, 0x9311);
    n = jhf_pack(&s, b);
    CHECK(n == 9 && b[0] == 2 && b[1] == 0xCD && b[2] == 0x14 && b[3] == 0x12 && b[4] == 0x12);   /* little-endian */
    CHECK(jhf_unpack(&back, b, n) && back.n == 2 && jhf_set_has(&back, 0x14CD, 0x1212) && jhf_set_has(&back, 0x25A4, 0x9311));
    CHECK(!jhf_unpack(&back, b, n - 1) && back.n == 0);  /* a cut record is nothing, not half a list */
    CHECK(!jhf_unpack(&back, b, n + 1) && back.n == 0);
    b[0] = JHF_MAX + 1;
    CHECK(!jhf_unpack(&back, b, n) && back.n == 0);      /* a count past the set */
    CHECK(!jhf_unpack(&back, b, 0) && back.n == 0);
    CHECK(!jhf_unpack(&back, NULL, 5) && back.n == 0);
    for (unsigned i = 0; i < JHF_MAX; i++)
        jhf_set_add(&s, 0x3000, (uint16_t)i);
    n = jhf_pack(&s, b);
    CHECK(n == JHF_PACK_MAX && jhf_unpack(&back, b, n) && back.n == JHF_MAX);             /* a full set round-trips */
}

int main(void)
{
    test_set();
    test_set_is_bounded();
    test_no_hub_nothing_is_skipped();
    test_behind_a_hub_the_listed_are_skipped();
    test_a_hub_is_always_enumerated();
    test_stored_form();
    printf("test_hostfilter: %d checks, %d failed\n", checks, fails);
    return fails;
}
