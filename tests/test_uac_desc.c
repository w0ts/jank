/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB speaker's descriptor (firmware/main/usb_audio.h): walks it descriptor by descriptor and checks the lengths and
 * the fields a host reads (UAC 2.0: one mono 16-bit 44.1 kHz isochronous OUT, adaptive, no feedback). Exit status: the
 * number of failed checks. */
#include <stdio.h>
#include <stdint.h>
#include "usb_audio.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

enum { ITF = 5, STR = 7, EP = 0x05 };
static const uint8_t D[] = {UAC_DESCRIPTOR(ITF, STR, EP)};

static unsigned le16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

int main(void)
{
    const uint8_t *at[16];
    size_t n = 0, off = 0, ac_total = 0;
    CHECK(sizeof D == UAC_DESC_LEN);
    while (off < sizeof D && n < 16) {             /* every descriptor's bLength chains to the next, ending exactly */
        CHECK(D[off] >= 2);
        if (D[off] < 2)
            break;
        at[n++] = D + off;
        off += D[off];
    }
    CHECK(off == sizeof D);
    CHECK(n == 12);
    if (n != 12)
        return fails ? fails : 1;
    const uint8_t *iad = at[0], *ac = at[1], *hdr = at[2], *clk = at[3], *it = at[4], *ot = at[5];
    const uint8_t *as0 = at[6], *as1 = at[7], *gen = at[8], *fmt = at[9], *ep = at[10];
    /* interface association: 2 interfaces from ITF, audio function, protocol 2.0 */
    CHECK(iad[0] == 8 && iad[1] == 0x0B && iad[2] == ITF && iad[3] == 2 && iad[4] == 0x01 && iad[6] == 0x20 && iad[7] == STR);
    CHECK(ac[0] == 9 && ac[1] == 0x04 && ac[2] == ITF && ac[3] == 0 && ac[4] == 0 && ac[5] == 0x01 && ac[6] == 0x01 && ac[7] == 0x20);
    /* the class-specific audio control part: header's wTotalLength covers header, clock, input and output terminal */
    for (size_t i = 2; i <= 5; ++i)
        ac_total += at[i][0];
    CHECK(hdr[1] == 0x24 && hdr[2] == 0x01 && le16(hdr + 3) == 0x0200 && le16(hdr + 6) == ac_total && ac_total == UAC_AC_TOTAL);
    CHECK(clk[0] == 8 && clk[1] == 0x24 && clk[2] == 0x0A && clk[3] == UAC_CLOCK_ID);
    CHECK(it[0] == 17 && it[2] == 0x02 && it[3] == UAC_IT_ID && le16(it + 4) == 0x0101 && it[7] == UAC_CLOCK_ID);
    CHECK(it[8] == 1 && le32(it + 9) == 0);                  /* mono, no spatial position */
    CHECK(ot[0] == 12 && ot[2] == 0x03 && ot[3] == UAC_OT_ID && le16(ot + 4) == 0x0301 && ot[7] == UAC_IT_ID && ot[8] == UAC_CLOCK_ID);
    /* streaming: alt 0 empty, alt 1 with one endpoint */
    CHECK(as0[0] == 9 && as0[2] == ITF + 1 && as0[3] == 0 && as0[4] == 0 && as0[5] == 0x01 && as0[6] == 0x02);
    CHECK(as1[0] == 9 && as1[2] == ITF + 1 && as1[3] == 1 && as1[4] == 1 && as1[5] == 0x01 && as1[6] == 0x02);
    CHECK(gen[0] == 16 && gen[2] == 0x01 && gen[3] == UAC_IT_ID && gen[5] == 0x01 && le32(gen + 6) == 1u);   /* PCM */
    CHECK(gen[10] == 1 && le32(gen + 11) == 0);               /* the same one channel as the terminal */
    CHECK(fmt[0] == 6 && fmt[2] == 0x02 && fmt[3] == 0x01 && fmt[4] == 2 && fmt[5] == 16);
    /* isochronous OUT, adaptive, data; one 1 ms packet of up to 46 samples */
    CHECK(ep[0] == 7 && ep[1] == 0x05 && ep[2] == EP && ep[3] == 0x09 && le16(ep + 4) == UAC_EP_SIZE && ep[6] == 1);
    CHECK(UAC_EP_SIZE == 92u && UAC_EP_SIZE >= 2u * (UAC_RATE / 1000u + 1u));
    const uint8_t *cs_ep = at[11];                           /* its class-specific part closes the function */
    CHECK(cs_ep[0] == 8 && cs_ep[1] == 0x25 && cs_ep[2] == 0x01);
    printf("uac descriptor: %zu bytes, %zu descriptors; %d checks, %d failed\n", sizeof D, n, checks, fails);
    return fails;
}
