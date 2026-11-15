/* SPDX-License-Identifier: GPL-3.0-only */
/* The TI TUSB320LI USB Type-C CC logic over I2C (datasheet SLLSEP2D, May 2017), host-tested through jt_io. All register
 * facts below are the datasheet's section 7.5 (Tables 7 .. 11) unless said otherwise.
 *
 * - Address: 0x47 with ADDR low, 0x67 with ADDR high (pin table, ADDR). jank mini 2.2 ties it low: 0x47.
 * - 0x00 .. 0x07 DEVICE_ID: "TUSB320" backwards, 0x07 .. 0x00 = 00 54 55 53 42 33 32 30 (Table 7).
 * - 0x09: ATTACHED_STATE bits 7:6 (00 none, 01 Attached.SRC = DFP, 10 Attached.SNK = UFP, 11 accessory), CABLE_DIR bit 5
 *   (0 CC1, 1 CC2), INTERRUPT_STATUS bit 4 (write 1 to clear; "SW must make sure the INTERRUPT_STATUS has been cleared to
 *   zero", Table 9), DRP_DUTY_CYCLE bits 2:1 and DISABLE_UFP_ACCESSORY bit 0 (read-write).
 * - 0x0A: DEBOUNCE 7:6, MODE_SELECT 5:4 (00 by the PORT pin, 01 UFP, 10 DFP, 11 DRP), I2C_SOFT_RESET 3, SOURCE_PREF 2:1
 *   (00 standard DRP, 01 Try.SNK, 11 Try.SRC), DISABLE_TERM 0 (Table 10).
 * - A mode change (7.3.2): set DISABLE_TERM, change MODE_SELECT, wait 5 ms, clear DISABLE_TERM. It drops an attachment.
 * - INT_N is open drain, pulled low while a read-update register changed and INTERRUPT_STATUS is set (7.2.4).
 * - Unpowered (a dead cell) or shut down, the CC pins present Rd 5.1 k +-20 %: a sink, so a charger still charges (7.3.3,
 *   Table 4). After its power-up it is Unattached.SNK and follows the PORT pin (7.3.1); jank mini 2.2 ties PORT low (UFP),
 *   so until the firmware writes MODE_SELECT it is a sink, the safe default. A restart of the ESP32 alone does not reset
 *   it: the mode written before the restart stays (only power, EN_N or I2C_SOFT_RESET resample PORT, 7.3.1). */
#ifndef JANK_TUSB320_H
#define JANK_TUSB320_H
#include <stddef.h>
#include <stdint.h>

#define JT_ADDR 0x47u
#define JT_REG_ID 0x00u
#define JT_ID_LEN 8u
#define JT_REG_CSR8 0x08u
#define JT_REG_STATUS 0x09u
#define JT_REG_MODE 0x0Au

#define JT_ATTACHED_SHIFT 6
#define JT_ATTACHED_MASK 0xC0u
#define JT_CABLE_DIR 0x20u
#define JT_INTERRUPT 0x10u
#define JT_STATUS_RW 0x07u               /* DRP_DUTY_CYCLE and DISABLE_UFP_ACCESSORY: kept as read when INTERRUPT is cleared */
#define JT_MODE_SELECT_SHIFT 4
#define JT_MODE_SELECT_MASK 0x30u
#define JT_SOFT_RESET 0x08u
#define JT_SOURCE_PREF_SHIFT 1
#define JT_SOURCE_PREF_MASK 0x06u
#define JT_DISABLE_TERM 0x01u
#define JT_MODE_SETTLE_MS 5u              /* 7.3.2's wait with the termination off */
#define JT_CLEAR_TRIES 3                  /* INTERRUPT_STATUS writes before giving up */

enum jt_attached { JT_NONE = 0, JT_ATTACHED_SRC = 1, JT_ATTACHED_SNK = 2, JT_ACCESSORY = 3 };
enum jt_mode { JT_MODE_PORT = 0, JT_MODE_UFP = 1, JT_MODE_DFP = 2, JT_MODE_DRP = 3 };
enum jt_pref { JT_PREF_STANDARD = 0, JT_PREF_TRY_SNK = 1, JT_PREF_TRY_SRC = 3 };

/* the bus, given by the caller: 0 on success */
struct jt_io {
    void *ctx;
    int (*read)(void *ctx, uint8_t reg, uint8_t *buf, size_t n);
    int (*write)(void *ctx, uint8_t reg, uint8_t value);
    void (*delay_ms)(void *ctx, uint32_t ms);
};

struct jt_status {
    uint8_t attached;                     /* enum jt_attached */
    uint8_t cable_dir;                    /* 1 CC1, 2 CC2 */
    uint8_t interrupt;                    /* INTERRUPT_STATUS was set (and has been cleared) */
    uint8_t current;                      /* CURRENT_MODE_DETECT (0x08 bits 5:4): 0 default, 1 medium, 2 accessory 500 mA, 3 high */
    uint8_t accessory;                    /* ACCESSORY_CONNECTED (0x08 bits 3:1) */
};

/* 0: a TUSB320 answered with its ID; -1 no answer; -2 another ID */
int jt_probe(const struct jt_io *io);
/* reads 0x08 and 0x09 into st and clears INTERRUPT_STATUS when set; 0 ok, -1 bus error, -2 the flag would not clear */
int jt_read(const struct jt_io *io, struct jt_status *st);
/* the 0x0A value for a mode and a source preference, other fields as in `current` (DEBOUNCE kept, SOFT_RESET and
 * DISABLE_TERM clear) */
uint8_t jt_mode_value(uint8_t current, enum jt_mode mode, enum jt_pref pref);
/* sets the mode with 7.3.2's sequence, only when MODE_SELECT or SOURCE_PREF differ from what the chip holds;
 * 1 written, 0 already so, -1 bus error */
int jt_set_mode(const struct jt_io *io, enum jt_mode mode, enum jt_pref pref);
#endif
