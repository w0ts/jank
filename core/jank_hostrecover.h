/* SPDX-License-Identifier: GPL-3.0-only */
/* The software replug (owner 2026-11-25: "can we put a fix so that we dont have to unplug replug ? it should recover dn2 and lp automatically"). After a Wi-Fi update the
 * Launchpad's or the Digitone's claim has failed with ESP_ERR_NO_MEM (257) at the very first step (the interface claim), every retry failing too, while a replug always
 * fixed it (root cause not known). So when a controller's claims fail JHR_AFTER_FAILS times in a row, the root port is powered off and, JHR_OFF_MS later, on again
 * (usb_host_lib_set_root_port_power): the hub and every device behind it leave and are enumerated again, as a replug does on the USB side (their power is the hub's,
 * which jank cannot switch). At most JHR_MAX times a start, JHR_GAP_MS apart, so a device that never claims cannot make it cycle for ever. Only failed claims start
 * it: a device that is claimed but says nothing is left alone (a Digitone without live control is silent too). Pure; host-tested (tests/test_hostrecover.c). */
#ifndef JANK_HOSTRECOVER_H
#define JANK_HOSTRECOVER_H
#include <stdint.h>

#define JHR_AFTER_FAILS 3u             /* failed claims in a row (the retries run every 2 s: about 6 s) */
#define JHR_OFF_MS 1000u               /* the port stays off this long */
#define JHR_MAX 3u                     /* cycles in a start */
#define JHR_GAP_MS 60000u              /* between two cycles */

enum jhr_act { JHR_NONE, JHR_POWER_OFF, JHR_POWER_ON };

struct jhr {
    uint8_t count;                     /* cycles done since the start */
    uint8_t off;                       /* the port is off, since off_ms */
    uint32_t off_ms, last_ms;
};

void jhr_init(struct jhr *s);
/* fails: the controllers' (a Launchpad's, the Digitone's) claims that failed in a row since the last success or cycle; what to do with the root port now */
enum jhr_act jhr_step(struct jhr *s, unsigned fails, uint32_t now_ms);
/* how long until jhr_step has something to do on its own (the port to power on again); UINT32_MAX: nothing */
uint32_t jhr_wait_ms(const struct jhr *s, uint32_t now_ms);

/* After an UPDATE's restart the root port starts unpowered (owner 2026-11-25, fix A): the cycle above did not help (the Launchpad did not even enumerate again),
 * and the claims failed only after Wi-Fi updates, while the update client was still talking to the unit in its first seconds; a restart of another kind with
 * the same devices claimed both at once. So the devices are enumerated once the update is confirmed and the Wi-Fi requests have stopped: at least
 * JHR_HOLD_MIN_MS after the start, JHR_HOLD_QUIET_MS after the last request, never later than JHR_HOLD_MAX_MS. Cause not proven: diag measures it (fail_dma). */
#define JHR_HOLD_MIN_MS 15000u
#define JHR_HOLD_QUIET_MS 5000u
#define JHR_HOLD_MAX_MS 45000u
/* 1: the port may be powered now. since_request_ms: since the last Wi-Fi request (UINT32_MAX: none yet); confirmed: the new image is no longer on probation */
int jhr_hold_over(uint32_t since_start_ms, int confirmed, uint32_t since_request_ms);
#endif
