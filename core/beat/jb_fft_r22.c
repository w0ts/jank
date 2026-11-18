/* SPDX-License-Identifier: GPL-3.0-only */
/* The front end's 1024-point complex FFT in two-stage passes (jb_fft_r22.h). */
#include "jb_fft_r22.h"
#include <math.h>

#define N 1024

void jb_fft_r22_table(float *w)
{
    for (int j = 0; j < N / 2; ++j) {                             /* e^(2 pi i j / N), j in bit-reversed order */
        int r = 0;
        for (int b = 0; b < 9; ++b)
            r |= ((j >> b) & 1) << (8 - b);
        w[2 * r] = (float)cos(2.0 * 3.14159265358979323846 * j / N);
        w[2 * r + 1] = (float)sin(2.0 * 3.14159265358979323846 * j / N);
    }
}

/* esp-dsp's dsps_fft2r_fc32 does, for the half sizes h = N/2 .. 1, for each block j of 2h points: x[m] = x[ia] - t, x[ia] += t with
 * t = x[m] (c - i s), (c, s) = w[2j], w[2j + 1]. Two stages in one pass (half sizes h and h/2, the second stage's blocks 2j and
 * 2j + 1 inside the first's block j): the same butterflies in the same order, so the same results, with four points in registers
 * between loads and stores and four independent butterflies per iteration for the FPU's latency to overlap (the one-butterfly
 * loop is latency bound: about 24 cycles a butterfly on the S3, 5120 of them). */
void jb_fft_r22(float *z, const float *w)
{
    for (int h = N / 2; h >= 2; h >>= 2) {                        /* first stage's half size: 512, 128, 32, 8, 2 */
        const int q = h >> 1;                                     /* the second stage's half size */
        for (int j = 0; j < N / (2 * h); ++j) {
            const float ca = w[2 * j], sa = w[2 * j + 1];         /* the first stage's twiddle, block j */
            const float cb = w[4 * j], sb = w[4 * j + 1];         /* the second stage's, block 2j (for points 0, 1) */
            const float cc = w[4 * j + 2], sc = w[4 * j + 3];     /* and block 2j + 1 (points 2, 3) */
            float *p0 = z + 4 * j * h;
            for (int i = 0; i < q; ++i, p0 += 2) {
                float *p1 = p0 + 2 * q, *p2 = p0 + 2 * h, *p3 = p2 + 2 * q;
                const float x0r = p0[0], x0i = p0[1], x1r = p1[0], x1i = p1[1];
                const float x2r = p2[0], x2i = p2[1], x3r = p3[0], x3i = p3[1];
                const float t2r = ca * x2r + sa * x2i, t2i = ca * x2i - sa * x2r;    /* the first stage: (0, 2) and (1, 3) */
                const float t3r = ca * x3r + sa * x3i, t3i = ca * x3i - sa * x3r;
                const float a0r = x0r + t2r, a0i = x0i + t2i, a2r = x0r - t2r, a2i = x0i - t2i;
                const float a1r = x1r + t3r, a1i = x1i + t3i, a3r = x1r - t3r, a3i = x1i - t3i;
                const float u1r = cb * a1r + sb * a1i, u1i = cb * a1i - sb * a1r;    /* the second: (0, 1) and (2, 3) */
                const float u3r = cc * a3r + sc * a3i, u3i = cc * a3i - sc * a3r;
                p0[0] = a0r + u1r;
                p0[1] = a0i + u1i;
                p1[0] = a0r - u1r;
                p1[1] = a0i - u1i;
                p2[0] = a2r + u3r;
                p2[1] = a2i + u3i;
                p3[0] = a2r - u3r;
                p3[1] = a2i - u3i;
            }
        }
    }
}
