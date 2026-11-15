/* SPDX-License-Identifier: GPL-3.0-only */
/* A hosted USB audio device's descriptors: where its audio comes out (an isochronous IN streaming interface, PCM Type I),
 * in which formats, and the class requests that set its sample rate. UAC 1.0 and UAC 2.0, both at full speed (the S3's
 * host is full speed only). Portable and host-tested; firmware/main/usb_uac_host.c does the transfers.
 *
 * Why not espressif/usb_host_uac: its README (1.5.0) says "UAC driver allows access to UAC 1.0 devices" only, and a
 * class-compliant UAC 2.0 device (a 2.0 device runs at full speed too) would be left out.
 *
 * The descriptors, from the USB Device Class Definition for Audio Devices 1.0 (audio10.pdf) and 2.0 (Audio20.pdf), and
 * Frmts10 / Frmts20 (Type I formats):
 * - interface: class 1 (audio), subclass 1 control, 2 streaming; protocol 0x20 = 2.0 (Audio20 A.6), 0x00 = 1.0;
 * - class-specific interface 0x24, endpoint 0x25; AS_GENERAL 0x01, FORMAT_TYPE 0x02 (Type I = 1);
 *   1.0: FORMAT_TYPE I = bNrChannels, bSubframeSize, bBitResolution, bSamFreqType, then the rates (3 bytes each; 0: a
 *   continuous min / max); the CS endpoint's bmAttributes bit 0 = a sampling frequency control on the endpoint;
 *   2.0: AS_GENERAL carries bNrChannels, FORMAT_TYPE I bSubslotSize + bBitResolution, and the rates come from the clock
 *   source (AC 0x0A) linked through the terminal (bTerminalLink -> the output terminal's bCSourceID), by a RANGE request;
 * - the data endpoint: isochronous (bmAttributes 1:0 = 01), IN, usage data (5:4 = 00). Valid bits are MSB-aligned in the
 *   subframe (Frmts 2.3.1.x), which jp_to_mono16 relies on.
 * Requests: 1.0 SET_CUR SAMPLING_FREQ_CONTROL to the endpoint (bmRequestType 0x22, bRequest 0x01, wValue 0x0100, wIndex the
 * endpoint, 3 bytes); 2.0 CUR / RANGE CS_SAM_FREQ_CONTROL to the clock (0x21 / 0xA1, bRequest 0x01 / 0x02, wValue 0x0100,
 * wIndex clock id << 8 | the control interface, 4 bytes / wNumSubRanges + {MIN, MAX, RES} x 4 bytes). */
#ifndef JANK_UACDESC_H
#define JANK_UACDESC_H
#include <stddef.h>
#include <stdint.h>

#define JU_ALTS_MAX 8u
#define JU_RATES_MAX 8u
#define JU_MPS_MAX 600u                    /* the S3 host's largest IN packet with CONFIG_USB_HOST_HW_BUFFER_BIAS_IN */
#define JU_RANGE_LEN (2u + 12u * JU_RATES_MAX)

struct ju_alt {
    uint8_t itf, alt;                      /* the streaming interface and its alternate setting */
    uint8_t ep;                            /* the data endpoint's address (IN) */
    uint16_t mps;
    uint8_t channels, subframe, bits;
    uint8_t clock;                         /* 2.0: the clock source's id; 0 none found */
    uint8_t freq_ctrl;                     /* 1.0: the endpoint takes SET_CUR sampling frequency */
    uint8_t continuous;                    /* rates[0] .. rates[1] is a range (1.0 bSamFreqType 0) */
    uint8_t nrates;
    uint32_t rates[JU_RATES_MAX];
};

struct ju_dev {
    uint8_t uac2;                          /* 1: Audio 2.0 */
    uint8_t ac_itf;                        /* the audio control interface (2.0 clock requests go to it) */
    uint8_t n;
    struct ju_alt alt[JU_ALTS_MAX];
};

/* every PCM Type I alternate setting with an isochronous IN data endpoint; returns how many (0: no audio out of it) */
uint8_t ju_parse(const uint8_t *cfg, size_t len, struct ju_dev *d);
/* 2.0: a RANGE answer for a clock's sampling frequency, into each alternate setting on that clock; 44.1 and 48 kHz (and
 * their doubles) are taken out of ranges, discrete values as they are. Returns the rates kept. */
uint8_t ju_clock_rates(struct ju_dev *d, uint8_t clock, const uint8_t *range, size_t len);

struct ju_pick {
    uint8_t index;                         /* into d->alt */
    uint32_t rate;
};
/* the setting to stream from: 44.1 kHz before 48 kHz, then fewer bytes a second (16 before 24 bits, mono before stereo),
 * at most 8 channels, a packet that fits JU_MPS_MAX and holds a millisecond. 0 ok, -1 none fits. */
int ju_choose(const struct ju_dev *d, struct ju_pick *p);

/* the 8-byte setup packets */
void ju_req_set_interface(uint8_t setup[8], uint8_t itf, uint8_t alt);
void ju_req_uac1_rate(uint8_t setup[8], uint8_t data[3], uint8_t ep, uint32_t hz);
void ju_req_uac2_rate(uint8_t setup[8], uint8_t data[4], uint8_t clock, uint8_t ac_itf, uint32_t hz);
void ju_req_uac2_range(uint8_t setup[8], uint8_t clock, uint8_t ac_itf);

/* a string descriptor (UTF-16LE) as printable ASCII, others as '?'; returns the length written (cap includes the 0) */
size_t ju_string_ascii(const uint8_t *desc, size_t len, char *out, size_t cap);
#endif
