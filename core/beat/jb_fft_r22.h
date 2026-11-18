/* SPDX-License-Identifier: GPL-3.0-only */
/* The front end's FFT kernel for 1024 complex points (interleaved re, im), in place: esp-dsp's dsps_fft2r_fc32 butterflies, two
 * stages per pass. Input in natural order, output in bit-reversed order (the caller reverses it: dsps_bit_rev2r_fc32 on the
 * firmware); w is esp-dsp's twiddle table, 1024 floats: w[2r], w[2r + 1] = cos, sin of 2 pi j / 1024 with r = j's 9 bits reversed
 * (dsps_fft2r_init_fc32 makes it; jb_fft_r22_table makes the same without esp-dsp). */
#ifndef JB_FFT_R22_H
#define JB_FFT_R22_H

void jb_fft_r22_table(float *w);
void jb_fft_r22(float *z, const float *w);

#endif
