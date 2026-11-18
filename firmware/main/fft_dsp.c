/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat front end's FFT on the ESP32-S3 (JB_FFT_FAST, core/beat/jb_fe.h): esp-dsp's assembly kernel for 1024 complex points
 * (dsps_fft2r_fc32_aes3) and its table-driven bit reversal, the same transform as core/beat/jb_fe.c's jb_fft_scalar. The
 * twiddle table (1024 floats, 4 KB) is our own static, in internal RAM and 16-byte aligned, handed to esp-dsp's init. */
#include <stdint.h>
#include "dsps_fft2r.h"
#include "esp_err.h"
#include "jb_fe.h"

#define FFT_N 1024                      /* the front end's complex points (jb_fe.c N2) */

static float s_w[FFT_N] __attribute__((aligned(16)));

void jb_fft_fast_init(void)
{
    ESP_ERROR_CHECK(dsps_fft2r_init_fc32(s_w, FFT_N));
}

void jb_fft_fast(float *z)
{
    if ((uintptr_t)z & 15u) {            /* the kernel needs 16-byte alignment: jb_fe's buffer has it; anything else gets the scalar one */
        jb_fft_scalar(z);
        return;
    }
    dsps_fft2r_fc32_aes3(z, FFT_N);
    dsps_bit_rev2r_fc32(z, FFT_N);
}
