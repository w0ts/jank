/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the TUSB320LI driver (core/jank_tusb320.c) against a model of the chip's registers as SLLSEP2D section
 * 7.5 describes them. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_tusb320.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

struct chip {
    uint8_t reg[256];
    int dead;                          /* no answer on the bus */
    int sticky;                        /* INTERRUPT_STATUS needs this many more clears */
    uint32_t delayed_ms;
    int writes;
    uint8_t log[16];                   /* values written to 0x0A, in order */
    int nlog;
    uint32_t delay_at_log[16];         /* ms delayed before each 0x0A write */
};

static int rd(void *ctx, uint8_t reg, uint8_t *buf, size_t n)
{
    struct chip *c = ctx;
    if (c->dead)
        return -1;
    for (size_t i = 0; i < n; i++)
        buf[i] = c->reg[(uint8_t)(reg + i)];
    return 0;
}

static int wr(void *ctx, uint8_t reg, uint8_t v)
{
    struct chip *c = ctx;
    if (c->dead)
        return -1;
    c->writes++;
    if (reg == JT_REG_STATUS) {
        /* RU bits (7:5) are the chip's; INTERRUPT_STATUS is cleared by a 1; the RW bits 2:0 take the value */
        uint8_t keep = c->reg[reg] & 0xE0u;
        uint8_t intr = c->reg[reg] & JT_INTERRUPT;
        if (v & JT_INTERRUPT) {
            if (c->sticky > 0)
                c->sticky--;
            else
                intr = 0;
        }
        c->reg[reg] = (uint8_t)(keep | intr | (v & JT_STATUS_RW));
    } else {
        if (reg == JT_REG_MODE && c->nlog < 16) {
            c->delay_at_log[c->nlog] = c->delayed_ms;
            c->log[c->nlog++] = v;
        }
        c->reg[reg] = v;
    }
    return 0;
}

static void dl(void *ctx, uint32_t ms) { ((struct chip *)ctx)->delayed_ms += ms; }

static void power_up(struct chip *c, struct jt_io *io)
{
    static const uint8_t id[8] = {'0', '2', '3', 'B', 'S', 'U', 'T', 0};
    memset(c, 0, sizeof *c);
    memcpy(c->reg, id, sizeof id);
    c->reg[JT_REG_STATUS] = JT_CABLE_DIR;   /* reset values: not attached, CC2 (Table 9) */
    c->reg[JT_REG_MODE] = 0x00;             /* by the PORT pin, standard DRP preference, termination on (Table 10) */
    io->ctx = c;
    io->read = rd;
    io->write = wr;
    io->delay_ms = dl;
}

static void test_probe(void)
{
    struct chip c;
    struct jt_io io;
    power_up(&c, &io);
    CHECK(jt_probe(&io) == 0);
    c.reg[6] = 'X';
    CHECK(jt_probe(&io) == -2);
    c.dead = 1;
    CHECK(jt_probe(&io) == -1);
}

static void test_read(void)
{
    struct chip c;
    struct jt_io io;
    struct jt_status st;
    power_up(&c, &io);
    CHECK(jt_read(&io, &st) == 0);
    CHECK(st.attached == JT_NONE && st.cable_dir == 2 && !st.interrupt && c.writes == 0);
    /* a device attached on CC1, the interrupt raised; DRP duty 50 % set earlier must survive the clear */
    c.reg[JT_REG_STATUS] = (uint8_t)((JT_ATTACHED_SRC << JT_ATTACHED_SHIFT) | JT_INTERRUPT | 0x04u);
    c.reg[JT_REG_CSR8] = 0x30;          /* CURRENT_MODE_DETECT high */
    CHECK(jt_read(&io, &st) == 0);
    CHECK(st.attached == JT_ATTACHED_SRC && st.cable_dir == 1 && st.interrupt && st.current == 3 && st.accessory == 0);
    CHECK(!(c.reg[JT_REG_STATUS] & JT_INTERRUPT));
    CHECK((c.reg[JT_REG_STATUS] & JT_STATUS_RW) == 0x04u);
    CHECK((c.reg[JT_REG_STATUS] >> JT_ATTACHED_SHIFT) == JT_ATTACHED_SRC);
    /* a charger: Attached.SNK; the flag needs a second write (Table 9's note) */
    c.reg[JT_REG_STATUS] = (uint8_t)((JT_ATTACHED_SNK << JT_ATTACHED_SHIFT) | JT_CABLE_DIR | JT_INTERRUPT);
    c.sticky = 1;
    CHECK(jt_read(&io, &st) == 0 && st.attached == JT_ATTACHED_SNK && st.cable_dir == 2);
    CHECK(!(c.reg[JT_REG_STATUS] & JT_INTERRUPT));
    /* a flag that never clears is reported */
    c.reg[JT_REG_STATUS] |= JT_INTERRUPT;
    c.sticky = 100;
    CHECK(jt_read(&io, &st) == -2);
    c.dead = 1;
    CHECK(jt_read(&io, &st) == -1);
}

static void test_mode(void)
{
    struct chip c;
    struct jt_io io;
    power_up(&c, &io);
    c.reg[JT_REG_MODE] = 0x40;          /* DEBOUNCE 118 ms, set by someone: kept */
    CHECK(jt_mode_value(0x40, JT_MODE_DRP, JT_PREF_TRY_SRC) == 0x76);
    CHECK(jt_mode_value(0xFF, JT_MODE_UFP, JT_PREF_STANDARD) == 0xD0);
    CHECK(jt_set_mode(&io, JT_MODE_DRP, JT_PREF_TRY_SRC) == 1);
    CHECK(c.reg[JT_REG_MODE] == 0x76);
    /* 7.3.2: DISABLE_TERM on, the mode with it on, 5 ms, then off */
    CHECK(c.nlog == 3);
    CHECK(c.log[0] == 0x41 && c.delay_at_log[0] == 0);
    CHECK(c.log[1] == 0x77 && c.delay_at_log[1] == 0);
    CHECK(c.log[2] == 0x76 && c.delay_at_log[2] >= JT_MODE_SETTLE_MS);
    /* the same again: nothing written (a restart of the ESP32 keeps an attachment) */
    c.nlog = 0;
    CHECK(jt_set_mode(&io, JT_MODE_DRP, JT_PREF_TRY_SRC) == 0 && c.nlog == 0);
    /* to the sink */
    CHECK(jt_set_mode(&io, JT_MODE_UFP, JT_PREF_STANDARD) == 1 && c.reg[JT_REG_MODE] == 0x50);
    /* termination left disabled (an interrupted sequence): rewritten */
    c.reg[JT_REG_MODE] = 0x51;
    c.nlog = 0;
    CHECK(jt_set_mode(&io, JT_MODE_UFP, JT_PREF_STANDARD) == 1 && c.reg[JT_REG_MODE] == 0x50);
    c.dead = 1;
    CHECK(jt_set_mode(&io, JT_MODE_DRP, JT_PREF_TRY_SRC) == -1);
}

int main(void)
{
    test_probe();
    test_read();
    test_mode();
    printf("tusb320: %d checks, %d failed\n", checks, fails);
    return fails;
}
