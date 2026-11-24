/* SPDX-License-Identifier: GPL-3.0-only */
/* The full-speed bulk packet size clamp (jank_fsclamp.h). */
#include "jank_fsclamp.h"

#define DESC_ENDPOINT 5u
#define EP_DESC_LEN 7u                 /* bLength of a standard endpoint descriptor: the size field is at 4 and 5 */
#define XFER_BULK 2u

unsigned jfc_clamp_fs_bulk(uint8_t *cfg, size_t len)
{
    size_t off = 0;
    unsigned changed = 0;
    if (!cfg)
        return 0;
    while (off + 2u <= len) {
        size_t n = cfg[off];
        if (n < 2u || off + n > len)
            break;                     /* not a descriptor: stop, change nothing past here */
        if (cfg[off + 1] == DESC_ENDPOINT && n >= EP_DESC_LEN && (cfg[off + 3] & 3u) == XFER_BULK) {
            unsigned mps = (unsigned)(cfg[off + 4] | (cfg[off + 5] << 8));
            if ((mps & 0x7FFu) > JFC_FS_BULK_MAX) {
                cfg[off + 4] = (uint8_t)JFC_FS_BULK_MAX;
                cfg[off + 5] = 0;
                changed++;
            }
        }
        off += n;
    }
    return changed;
}
