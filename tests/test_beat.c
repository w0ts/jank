/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the beat tracker (core/beat/). Exit status: the number of failed checks.
 *
 *   - the front end against a plain double-precision DFT of the same frames
 *   - the DBN's state space and a synthetic activation
 *   - the estimator (jb_mode) against the Python's traces in tests/beat_ref/NAME.mode.txt, beat by beat
 *   - when BEAT_WAV_DIR is set (mono 44.1 kHz float32 or 16-bit WAVs, made by beat-listener/scripts/export_beat_c.py
 *     prep): the whole tracker on each tests/beat_ref/NAME.beats.txt's WAV, against madmom's activation and beats
 *
 * test_beat [REF_DIR]   (default: the directory of this file, beat_ref) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "jb_beat.h"
#include "jb_mode.h"

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

/* ---- the front end ---- */
static void test_front_end(void)
{
    static struct jb_fe fe;
    static float x[JB_FRAME * 6];
    float feat[BEAT_INPUTS], prev_log[BEAT_BANDS] = {0};
    int ready, frame = 0;
    double worst = 0.0;
    srand(7);
    for (size_t i = 0; i < sizeof x / sizeof *x; ++i)
        x[i] = 0.3f * sinf(0.05f * (float)i) + 0.2f * ((float)rand() / 2147483648.0f - 0.5f);
    jb_fe_init(&fe);
    const size_t total = sizeof x / sizeof *x;
    for (size_t off = 0; off < total;) {
        size_t took = jb_fe_push(&fe, x + off, total - off > 333 ? 333 : total - off, feat, &ready);
        off += took;
        if (!ready)
            continue;
        const long start = (long)frame * JB_HOP - JB_FRAME / 2;
        CHECK(jb_fe_last_frame(&fe) == frame);
        double mag[BEAT_BINS];
        for (int k = 0; k < BEAT_BINS; ++k) {                       /* the plain DFT of the windowed frame */
            double re = 0, im = 0;
            if (k > 800) {
                mag[k] = 0;
                continue;
            }
            for (int n = 0; n < JB_FRAME; ++n) {
                long idx = start + n;
                double s = idx < 0 || idx >= (long)total ? 0.0 : x[idx];
                double w = 0.5 - 0.5 * cos(2.0 * PI_D * n / (JB_FRAME - 1));
                re += s * w * cos(2.0 * PI_D * k * n / JB_FRAME);
                im -= s * w * sin(2.0 * PI_D * k * n / JB_FRAME);
            }
            mag[k] = sqrt(re * re + im * im);
        }
        const float *val = beat_fb_val;
        for (int j = 0; j < BEAT_BANDS; ++j) {
            double s = 0;
            for (int i = 0; i < beat_fb_len[j]; ++i)
                s += val[i] * mag[beat_fb_start[j] + i];
            val += beat_fb_len[j];
            double lg = log10(1.0 + s), d = frame ? lg - prev_log[j] : 0.0;
            worst = fmax(worst, fabs(lg - feat[j]));
            worst = fmax(worst, fabs((d > 0 ? d : 0) - feat[BEAT_BANDS + j]));
            prev_log[j] = (float)lg;
        }
        ++frame;
        if (frame >= 3)
            break;
    }
    CHECK(frame == 3);
    CHECK(worst < 1e-3);
    printf("front end: worst feature error against a double DFT %.2g over %d frames\n", worst, frame);
}

/* ---- the DBN ---- */
static void test_dbn(void)
{
    static struct jb_dbn d;
    CHECK(jb_dbn_init(&d, 90, 180, 100) == 0);
    CHECK(d.n_intervals == 35 && d.n_states == 1750 && d.interval[0] == 33 && d.interval[34] == 67);
    for (int j = 0; j < d.n_intervals; ++j) {
        double s = 0;
        for (int i = 0; i < d.n_intervals; ++i)
            s += d.trans[i][j];                                       /* [to][from] since M2m: a column is a row of the matrix */
        CHECK(fabs(s - 1.0) < 1e-5);
    }
    CHECK(jb_dbn_init(&d, 20, 240, 100) == -1);                       /* too many states for the fixed tables */
    jb_dbn_init(&d, 90, 180, 100);
    int beats = 0;
    double t, first = -1, last = -1;
    for (int f = 0; f < 3000; ++f) {                                  /* pulses every 50 frames: 120 BPM */
        float act = f % 50 == 0 ? 0.95f : 0.02f;
        if (jb_dbn_step(&d, act, &t)) {
            ++beats;
            if (first < 0)
                first = t;
            last = t;
        }
    }
    printf("dbn: pulses every 0.5 s -> %d beats, first %.2f s, last %.2f s\n", beats, first, last);
    CHECK(beats >= 55 && beats <= 61);
    CHECK(last > 29.0);
}

/* ---- the estimator ---- */
static void synthetic_modes(void)
{
    struct jb_mode m;
    double bpm = 0, t = 0;
    jb_mode_init(&m);
    for (int i = 0; i < 80; ++i) {                                    /* 128 BPM with 8 ms of jitter */
        t += 60.0 / 128.0;
        jb_mode_update(&m, t + 0.008 * sin(i * 1.7));
    }
    CHECK(jb_mode_bpm_at(&m, t, &bpm) && fabs(bpm - 128.0) < 0.2 && jb_mode_at(&m, t) == JB_STEADY);
    for (int i = 0; i < 60; ++i) {                                    /* a jump to 140 */
        t += 60.0 / 140.0;
        jb_mode_update(&m, t);
    }
    CHECK(jb_mode_bpm_at(&m, t, &bpm) && fabs(bpm - 140.0) < 0.5);
    CHECK(jb_mode_at(&m, t + 31.0) == JB_STOPPED);                    /* no beats for 30 s */
    CHECK(!jb_mode_bpm_at(&m, t + 31.0, &bpm));
    jb_mode_init(&m);
    t = 0;
    for (int i = 0; i < 100; ++i) {                                   /* a ramp from 120 to 130 BPM over 100 beats */
        t += 60.0 / (120.0 + 10.0 * i / 100.0);
        jb_mode_update(&m, t);
    }
    CHECK(jb_mode_bpm_at(&m, t, &bpm) && fabs(bpm - 130.0) < 1.5);
    printf("mode: steady 128 / jump 140 / stop / ramp 120-130 -> %.2f BPM, mode %d\n", bpm, jb_mode_at(&m, t));
}

static int read_beats(const char *path, double *v, int max)
{
    FILE *f = fopen(path, "r");
    int n = 0;
    if (!f)
        return -1;
    while (n < max && fscanf(f, "%lf", &v[n]) == 1)
        ++n;
    fclose(f);
    return n;
}

static void test_mode_trace(const char *dir, const char *name)
{
    char path[512], mode[32];
    double t, bpm, beats[512];
    snprintf(path, sizeof path, "%s/%s.mode.txt", dir, name);
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("mode %s: no trace\n", name);
        return;
    }
    struct jb_mode m;
    jb_mode_init(&m);
    m.lock_beats = 8;                  /* the traces are ModeBpm with LOCK_BEATS 8 (the device has 16 since M2j) */
    int n = 0, bad_mode = 0;
    double worst = 0;
    (void)beats;
    while (fscanf(f, "%lf %31s %lf", &t, mode, &bpm) == 3) {
        jb_mode_update(&m, t);
        static const char *const NAMES[] = {"stopped", "steady", "speeding_up", "slowing_down"};
        bad_mode += strcmp(NAMES[m.mode], mode) != 0;
        double got = 0;
        int have = jb_mode_bpm_at(&m, t, &got);
        if (have && bpm > 0)
            worst = fmax(worst, fabs(got - bpm));
        else if (have != (bpm > 0))
            ++bad_mode;
        ++n;
    }
    fclose(f);
    printf("mode %s: %d beats, %d mode mismatches, worst BPM difference to the Python %.2g\n", name, n, bad_mode, worst);
    CHECK(n > 0 && bad_mode == 0 && worst < 1e-3);
}

/* ---- the whole tracker on audio ---- */
static float *read_wav(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    unsigned char h[12], ck[8];
    int fmt = 0, bits = 0, ch = 0;
    long rate = 0;
    float *out = NULL;
    if (!f)
        return NULL;
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
        fclose(f);
        return NULL;
    }
    while (fread(ck, 1, 8, f) == 8) {
        unsigned long len = ck[4] | ck[5] << 8 | ck[6] << 16 | (unsigned long)ck[7] << 24;
        if (!memcmp(ck, "fmt ", 4)) {
            unsigned char b[16];
            if (len < 16 || fread(b, 1, 16, f) != 16)
                break;
            fmt = b[0] | b[1] << 8;
            ch = b[2] | b[3] << 8;
            rate = b[4] | b[5] << 8 | b[6] << 16 | (long)b[7] << 24;
            bits = b[14] | b[15] << 8;
            fseek(f, (long)(len - 16 + (len & 1)), SEEK_CUR);
        } else if (!memcmp(ck, "data", 4)) {
            size_t bytes = len, per = (size_t)bits / 8;
            if (ch != 1 || rate != 44100 || !((fmt == 1 && bits == 16) || (fmt == 3 && bits == 32)))
                break;
            unsigned char *raw = malloc(bytes);
            out = malloc(bytes / per * sizeof(float));
            if (!raw || !out || fread(raw, 1, bytes, f) != bytes) {
                free(raw);
                free(out);
                out = NULL;
                break;
            }
            *n = bytes / per;
            for (size_t i = 0; i < *n; ++i) {
                if (fmt == 3)
                    memcpy(&out[i], raw + 4 * i, 4);
                else
                    out[i] = (float)(short)(raw[2 * i] | raw[2 * i + 1] << 8) / 32768.0f;
            }
            free(raw);
            break;
        } else {
            fseek(f, (long)(len + (len & 1)), SEEK_CUR);
        }
    }
    fclose(f);
    return out;
}

struct run {
    float *act;
    double *beats;
    int n_frames, n_beats;
};

static void on_frame(void *ctx, const struct jb_frame *fr)
{
    struct run *r = ctx;
    r->act[r->n_frames++] = fr->act;
    if (fr->beat)
        r->beats[r->n_beats++] = fr->time;
}

/* ---- the int16 network against the float one, on the timing bench's audio (no WAV needed) ---- */
static uint32_t s_rng;
static float noise(void)
{
    s_rng = s_rng * 1664525u + 1013904223u;
    return (float)(int32_t)s_rng * (1.0f / 2147483648.0f);
}

/* 124 BPM: a kick on every beat, a hat on the off-beats, a little noise (timing-scratch bench/main/bench.c's synth) */
static void synth(float *x, size_t n)
{
    const double period = 44100.0 * 60.0 / 124.0;
    for (size_t s = 0; s < n; ++s) {
        double ph = fmod((double)s, period);
        double ph2 = fmod((double)s + period / 2.0, period);
        float kick = ph < 6000.0 ? (float)(0.8 * exp(-ph / 1500.0) * sin(2.0 * PI_D * 55.0 * ph / 44100.0)) : 0.0f;
        float hat = ph2 < 1500.0 ? (float)(0.2 * exp(-ph2 / 300.0)) * noise() : 0.0f;
        x[s] = kick + hat + 0.02f * noise();
    }
}

static void test_q16(void)
{
    enum { SECONDS = 30, N = SECONDS * JB_RATE, MAX_FRAMES = SECONDS * 100 + 16 };
    static float x[N], act_f[MAX_FRAMES], act_q[MAX_FRAMES];
    static int16_t x16[N];
    static double beats_f[512], beats_q[512];
    static struct jb_beat tf, tq;
    struct run rf = {act_f, beats_f, 0, 0}, rq = {act_q, beats_q, 0, 0};
    s_rng = 12345u;
    synth(x, N);
    for (size_t i = 0; i < N; ++i)                                    /* what the USB speaker hands the tracker */
        x16[i] = (int16_t)lrintf(fmaxf(-1.0f, fminf(1.0f, x[i])) * 32767.0f);
    CHECK(jb_beat_init(&tf) == 0);
    CHECK(jb_beat_init_q16(&tq) == 0);
    for (size_t off = 0; off < N; off += 441) {                      /* 441: the firmware's pull, one hop at a time */
        size_t m = N - off < 441 ? N - off : 441;
        jb_beat_push_s16(&tf, x16 + off, m, on_frame, &rf);
        jb_beat_push_s16(&tq, x16 + off, m, on_frame, &rq);
    }
    double worst = 0.0;
    int same = 0;
    for (int i = 0; i < rf.n_frames && i < rq.n_frames; ++i)
        worst = fmax(worst, fabs((double)act_f[i] - (double)act_q[i]));
    for (int i = 0; i < rf.n_beats && i < rq.n_beats; ++i)
        same += fabs(beats_f[i] - beats_q[i]) < 0.005;
    double bpm = rq.n_beats > 9 ? 60.0 * (rq.n_beats - 9 - 1) / (beats_q[rq.n_beats - 1] - beats_q[9]) : 0.0;
    printf("int16 network: %zu B of RAM; %d frames; activation error max %.2g; beats float %d, int16 %d, same frame %d; "
           "%.2f BPM from the int16 beats (synth 124)\n", jb_lstm_q16_ram(), rq.n_frames, worst, rf.n_beats, rq.n_beats, same, bpm);
    CHECK(rf.n_frames == rq.n_frames);
    CHECK(worst < 2e-3);
    CHECK(rf.n_beats == rq.n_beats && same == rq.n_beats);
    CHECK(rq.n_beats >= 50);                                         /* 62 beats in 30 s, less the first second or two */
    CHECK(fabs(bpm - 124.0) < 1.0);
}

static void test_audio(const char *dir, const char *wav_dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.44k.wav", wav_dir, name);
    size_t n = 0;
    float *x = read_wav(path, &n);
    if (!x) {
        printf("audio %s: cannot read %s (mono 44.1 kHz PCM16 or float32)\n", name, path);
        CHECK(0);
        return;
    }
    static struct jb_beat tracker;
    struct run r = {malloc((n / JB_HOP + 8) * sizeof(float)), malloc(4096 * sizeof(double)), 0, 0};
    CHECK(jb_beat_init(&tracker) == 0);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (size_t off = 0; off < n; off += 480)                          /* the way a USB sound card delivers: 480 samples */
        jb_beat_push(&tracker, x + off, n - off < 480 ? n - off : 480, on_frame, &r);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    const double cpu = (double)(t1.tv_sec - t0.tv_sec) + 1e-9 * (double)(t1.tv_nsec - t0.tv_nsec);

    static float ref_act[20000];
    static double ref_beats[2048];
    int na = 0, nb;
    snprintf(path, sizeof path, "%s/%s.act.txt", dir, name);
    FILE *f = fopen(path, "r");
    double v;
    while (f && na < 20000 && fscanf(f, "%lf", &v) == 1)
        ref_act[na++] = (float)v;
    if (f)
        fclose(f);
    snprintf(path, sizeof path, "%s/%s.beats.txt", dir, name);
    nb = read_beats(path, ref_beats, 2048);
    int frames = r.n_frames < na ? r.n_frames : na;
    double worst = 0, sum = 0;
    for (int i = 0; i < frames; ++i) {
        double d = fabs(r.act[i] - ref_act[i]);
        worst = fmax(worst, d);
        sum += d;
    }
    int same = 0, within1 = 0, missing = 0, extra = 0;
    for (int i = 0; i < nb; ++i) {
        double best = 1e9;
        for (int j = 0; j < r.n_beats; ++j)
            best = fmin(best, fabs(r.beats[j] - ref_beats[i]));
        if (best < 0.005)
            ++same;
        else if (best < 0.015)
            ++within1;
        else
            ++missing;
    }
    for (int j = 0; j < r.n_beats; ++j) {
        double best = 1e9;
        for (int i = 0; i < nb; ++i)
            best = fmin(best, fabs(r.beats[j] - ref_beats[i]));
        extra += best >= 0.015;
    }
    printf("audio %s: %d frames (python %d), activation error max %.2g mean %.2g; beats: python %d, C %d, same %d, "
           "within 1 frame %d, missing %d, extra %d; %.3f s of CPU for %.1f s of audio (%.2f%% of real time)\n",
           name, r.n_frames, na, worst, sum / (frames ? frames : 1), nb, r.n_beats, same, within1, missing, extra, cpu,
           (double)n / JB_RATE, 100.0 * cpu / ((double)n / JB_RATE));
    CHECK(r.n_frames >= na - 3 && r.n_frames <= na + 3);
    CHECK(worst < 1e-2);
    CHECK(missing == 0 && extra == 0);
    free(x);
    free(r.act);
    free(r.beats);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "tests/beat_ref";
    const char *wavs = getenv("BEAT_WAV_DIR");
    static const char *const NAMES[] = {"disco.00005", "hiphop.00010", "pop.00020"};
    printf("sizeof struct jb_beat %zu, struct jb_mode %zu\n", sizeof(struct jb_beat), sizeof(struct jb_mode));
    test_front_end();
    test_dbn();
    synthetic_modes();
    test_q16();
    for (size_t i = 0; i < sizeof NAMES / sizeof *NAMES; ++i) {
        test_mode_trace(dir, NAMES[i]);
        if (wavs)
            test_audio(dir, wavs, NAMES[i]);
    }
    if (!wavs)
        printf("audio: BEAT_WAV_DIR not set, the tracker was NOT run on audio\n");
    printf("%d checks, %d failed\n", checks, fails);
    return fails;
}
