/* SPDX-License-Identifier: GPL-3.0-only */
/* The two-stage FFT kernel (core/beat/jb_fft_r22.c) against a plain double-precision DFT and against the front end's scalar FFT,
 * on the 1024-point frames the front end uses. The kernel leaves bit-reversed order: reversed here the way esp-dsp's
 * dsps_bit_rev2r_fc32 does it on the firmware. Exit status: the number of failed checks.
 *
 * test_fft_r22 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "jb_fe.h"
#include "jb_fft_r22.h"

#define N 1024
#define PI_D 3.14159265358979323846

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void bit_reverse(float *d)
{
    for (int i = 1, j = 0; i < N - 1; ++i) {
        int k = N >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
        if (i < j) {
            float r = d[2 * j], m = d[2 * j + 1];
            d[2 * j] = d[2 * i];
            d[2 * j + 1] = d[2 * i + 1];
            d[2 * i] = r;
            d[2 * i + 1] = m;
        }
    }
}

int main(void)
{
    static struct jb_fe fe;
    static float w[N], z[2 * N], a[2 * N];
    double worst_scalar = 0.0, worst_dft = 0.0, peak = 0.0;
    jb_fe_init(&fe);                                             /* (makes the scalar FFT's tables) */
    jb_fft_r22_table(w);
    srand(5);
    for (int t = 0; t < 20; ++t) {
        for (int i = 0; i < 2 * N; ++i)
            z[i] = a[i] = ((float)rand() / (float)RAND_MAX - 0.5f) * (float)(t + 1);
        jb_fft_scalar(a);
        jb_fft_r22(z, w);
        bit_reverse(z);
        for (int i = 0; i < 2 * N; ++i) {
            worst_scalar = fmax(worst_scalar, fabs((double)z[i] - a[i]));
            peak = fmax(peak, fabs(a[i]));
        }
    }
    CHECK(worst_scalar < 1e-6 * peak);
    for (int i = 0; i < N; ++i) {                                /* a tone and a click against the double DFT */
        z[2 * i] = 0.5f * sinf(2.0f * 3.14159265f * 37.0f * (float)i / N) + (i == 100 ? 1.0f : 0.0f);
        z[2 * i + 1] = 0.25f * cosf(2.0f * 3.14159265f * 5.0f * (float)i / N);
    }
    for (int i = 0; i < 2 * N; ++i)
        a[i] = z[i];
    jb_fft_r22(z, w);
    bit_reverse(z);
    for (int k = 0; k < N; k += 7) {
        double re = 0, im = 0;
        for (int n = 0; n < N; ++n) {
            const double ang = -2.0 * PI_D * k * n / N;
            re += a[2 * n] * cos(ang) - a[2 * n + 1] * sin(ang);
            im += a[2 * n] * sin(ang) + a[2 * n + 1] * cos(ang);
        }
        worst_dft = fmax(worst_dft, fmax(fabs(re - z[2 * k]), fabs(im - z[2 * k + 1])));
    }
    CHECK(worst_dft < 1e-3);
    printf("fft r22: max difference to the scalar FFT %.3g (peak %.0f), to a double DFT %.3g\n", worst_scalar, peak, worst_dft);
    printf("%d checks, %d failed\n", checks, fails);
    return fails;
}
