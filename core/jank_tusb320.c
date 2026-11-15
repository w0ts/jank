/* SPDX-License-Identifier: GPL-3.0-only */
/* The TUSB320LI's registers (jank_tusb320.h, SLLSEP2D section 7.5). */
#include "jank_tusb320.h"

#define SETTING (JT_MODE_SELECT_MASK | JT_SOURCE_PREF_MASK | JT_DISABLE_TERM)   /* a termination left off counts as differing */

static const uint8_t ID[JT_ID_LEN] = {0x30, 0x32, 0x33, 0x42, 0x53, 0x55, 0x54, 0x00};   /* 0x00 .. 0x07 (Table 7) */

int jt_probe(const struct jt_io *io)
{
    uint8_t id[JT_ID_LEN];
    if (io->read(io->ctx, JT_REG_ID, id, sizeof id))
        return -1;
    for (size_t i = 0; i < sizeof id; i++)
        if (id[i] != ID[i])
            return -2;
    return 0;
}

int jt_read(const struct jt_io *io, struct jt_status *st)
{
    uint8_t r[2];
    if (io->read(io->ctx, JT_REG_CSR8, r, sizeof r))
        return -1;
    st->current = (uint8_t)((r[0] >> 4) & 3u);
    st->accessory = (uint8_t)((r[0] >> 1) & 7u);
    st->attached = (uint8_t)((r[1] & JT_ATTACHED_MASK) >> JT_ATTACHED_SHIFT);
    st->cable_dir = (r[1] & JT_CABLE_DIR) ? 2u : 1u;
    st->interrupt = (uint8_t)((r[1] & JT_INTERRUPT) != 0);
    /* write 1 to clear, the read-write bits as they were; read back until it is 0 (Table 9's note) */
    for (int i = 0; i < JT_CLEAR_TRIES && (r[1] & JT_INTERRUPT); i++) {
        if (io->write(io->ctx, JT_REG_STATUS, (uint8_t)((r[1] & JT_STATUS_RW) | JT_INTERRUPT)))
            return -1;
        if (io->read(io->ctx, JT_REG_STATUS, &r[1], 1))
            return -1;
    }
    return (r[1] & JT_INTERRUPT) ? -2 : 0;
}

uint8_t jt_mode_value(uint8_t current, enum jt_mode mode, enum jt_pref pref)
{
    uint8_t v = (uint8_t)(current & ~(JT_MODE_SELECT_MASK | JT_SOURCE_PREF_MASK | JT_SOFT_RESET | JT_DISABLE_TERM));
    return (uint8_t)(v | (((unsigned)mode << JT_MODE_SELECT_SHIFT) & JT_MODE_SELECT_MASK) | (((unsigned)pref << JT_SOURCE_PREF_SHIFT) & JT_SOURCE_PREF_MASK));
}

int jt_set_mode(const struct jt_io *io, enum jt_mode mode, enum jt_pref pref)
{
    uint8_t cur, want;
    if (io->read(io->ctx, JT_REG_MODE, &cur, 1))
        return -1;
    want = jt_mode_value(cur, mode, pref);
    if ((cur & SETTING) == (want & SETTING))
        return 0;
    /* 7.3.2: termination off, the new mode, 5 ms, termination on */
    if (io->write(io->ctx, JT_REG_MODE, (uint8_t)((cur & ~(JT_SOFT_RESET)) | JT_DISABLE_TERM)))
        return -1;
    if (io->write(io->ctx, JT_REG_MODE, (uint8_t)(want | JT_DISABLE_TERM)))
        return -1;
    io->delay_ms(io->ctx, JT_MODE_SETTLE_MS);
    if (io->write(io->ctx, JT_REG_MODE, want))
        return -1;
    return 1;
}
