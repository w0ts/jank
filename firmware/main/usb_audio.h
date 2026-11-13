/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB audio speaker (BEAT-FOLLOW BENCH BUILD): a UAC2 output called "jank", 44.1 kHz, 16-bit, MONO, isochronous OUT
 * on endpoint 5, adaptive, with no feedback endpoint (the IN endpoints are all used: usb.c). What the computer plays to it
 * goes into a ring buffer for the beat tracker (beat.c); it is never played. The player sends the mid, (L + R) / 2.
 *
 * Descriptor: an interface association of the audio control interface (clock source 4, input terminal 1 = USB streaming,
 * output terminal 3 = speaker; no feature unit, so the computer has no volume for it) and one streaming interface (alt 0
 * with no endpoint, alt 1 with the data endpoint). One channel, no spatial position (bmChannelConfig 0). Packets: 44
 * samples a millisecond and 45 every tenth, 90 bytes at most; the endpoint takes 92 (46 samples) for a host that runs a
 * little fast. Data FIFO budget on the S3's 256 words (tinyusb dcd_dwc2.c calc_device_grxfsiz): 14 words EPInfo (DMA),
 * RX FIFO 14 + 2 x (23 + 1) + 2 x 7 = 76 words for 92-byte packets, TX FIFOs EP0 16 + NCM 16 + 16 + MIDI 16 + drive 16 =
 * 80: 170 of 256, 86 words spare. */
#ifndef USB_AUDIO_H
#define USB_AUDIO_H
#include <stddef.h>
#include <stdint.h>

#define UAC_RATE 44100u
#define UAC_EP_SIZE 92u                 /* 46 samples of 2 bytes, mono (third_party/esp_tinyusb-uac2.patch says the same) */
#define UAC_CLOCK_ID 4
#define UAC_IT_ID 1
#define UAC_OT_ID 3
#define UAC_AC_TOTAL (9 + 8 + 17 + 12)  /* class-specific audio control descriptors: header, clock, input, output terminal */
#define UAC_DESC_LEN (8 + 9 + UAC_AC_TOTAL + 9 + 9 + 16 + 6 + 7 + 8)

#define UAC_LE16(v) (uint8_t)((v) & 0xFFu), (uint8_t)(((v) >> 8) & 0xFFu)

/* itf: the audio control interface's number (the streaming one follows); str: the name's string index */
#define UAC_DESCRIPTOR(itf, str, ep)                                                                                       \
    /* interface association: the audio function, protocol 2.0 */                                                         \
    8, 0x0B, (itf), 2, 0x01, 0x00, 0x20, (str),                                                                           \
    /* audio control interface */                                                                                         \
    9, 0x04, (itf), 0, 0, 0x01, 0x01, 0x20, (str),                                                                        \
    /* header: UAC 2.00, desktop speaker, total length of the entities below, no controls */                              \
    9, 0x24, 0x01, 0x00, 0x02, 0x01, UAC_LE16(UAC_AC_TOTAL), 0x00,                                                        \
    /* clock source: internal fixed, frequency and validity read-only */                                                  \
    8, 0x24, 0x0A, UAC_CLOCK_ID, 0x01, 0x05, 0, 0,                                                                        \
    /* input terminal: USB streaming, 1 channel with no spatial position, clock source 4 */                               \
    17, 0x24, 0x02, UAC_IT_ID, 0x01, 0x01, 0, UAC_CLOCK_ID, 1, 0x00, 0x00, 0x00, 0x00, 0, 0x00, 0x00, 0,                  \
    /* output terminal: speaker, fed by the input terminal */                                                             \
    12, 0x24, 0x03, UAC_OT_ID, 0x01, 0x03, 0, UAC_IT_ID, UAC_CLOCK_ID, 0x00, 0x00, (str),                                 \
    /* streaming interface, alt 0: no endpoint */                                                                         \
    9, 0x04, (itf) + 1, 0, 0, 0x01, 0x02, 0x20, (str),                                                                    \
    /* alt 1: one endpoint */                                                                                             \
    9, 0x04, (itf) + 1, 1, 1, 0x01, 0x02, 0x20, (str),                                                                    \
    /* AS general: to terminal 1, PCM type I, 1 channel */                                                                \
    16, 0x24, 0x01, UAC_IT_ID, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 1, 0x00, 0x00, 0x00, 0x00, 0,                          \
    /* format type I: 2 bytes a sample, 16 bits */                                                                        \
    6, 0x24, 0x02, 0x01, 2, 16,                                                                                           \
    /* the endpoint: isochronous, adaptive, data; one packet a frame */                                                   \
    7, 0x05, (ep), 0x09, UAC_LE16(UAC_EP_SIZE), 1,                                                                        \
    /* its audio class part: no controls, no lock delay */                                                                \
    8, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00

/* ---- the receive path, for the beat tracker ---- */

struct uac_stats {
    uint8_t open;                       /* the computer is sending (alt setting 1) */
    uint32_t packets;                   /* with samples in them */
    uint32_t gaps;                      /* 1 ms frames with no packet while streaming (a packet more than 1.5 ms after the last) */
    uint32_t overflows;                 /* the ring was full: the tracker restarts */
    uint32_t resets;                    /* the stream opened or the tracker restarted */
};

/* The ring and the mapping are the tracker task's: it calls these from one task. */
uint32_t uac_epoch(void);               /* changes when the stream opens or closes, or the ring overflowed */
/* Starts again from the newest sample: the ring is emptied, sample 0 is the next one that arrives. */
void uac_restart(void);
/* Mono samples taken from the ring (at most max); returns how many. */
size_t uac_pull(int16_t *dst, size_t max);
/* Local time (the Link host clock, us) of stream sample s, from the offset between the sample count and the packets'
 * arrival; 0 when none yet. */
int uac_sample_time(int64_t s, int64_t *us);
void uac_stats(struct uac_stats *out);
int32_t uac_offset_us(void);
#endif
