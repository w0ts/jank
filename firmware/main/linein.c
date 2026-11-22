/* SPDX-License-Identifier: GPL-3.0-only */
/* The LINE IN into the beat tracker, and the cell's level (board.h BOARD_HAS_LINEIN; the design: worktrees/timing-scratch
 * TIMING.md section 5, docs/linein.md).
 *
 * ADC1 continuous mode (ESP-IDF adc_continuous.rst): the pattern [LINE IN, cell] at 83,333 conversions a second, or
 * [LINE IN] alone at 41,666 (the XIAO bench): the LINE IN is sampled every 24 us exactly either way (core/jank_line.h). GDMA
 * frames of 200 line samples (4.8 ms). The ADC and the gptimer that is the local clock (midi.c) both run from APB, so sample n
 * is at t0 + 24 n us with no drift; t0 comes from the frame interrupts' timer stamps (jl_fit).
 *
 * - The DMA interrupt (on_frame): IRAM, cache-safe (CONFIG_ADC_CONTINUOUS_ISR_IRAM_SAFE, adc_continuous.rst "IRAM Safe"),
 *   so it keeps running while flash is written. It stamps the frame with the local clock, splits it into the line samples
 *   and the cell's sum (jl_parse, in IRAM by linker.lf) into a ring of SLOTS frames, and wakes the task. The interrupt
 *   belongs to the core that allocates it (the GDMA channel's, in adc_continuous_new_handle): the task below, on core 0.
 *   Core 1 stays the clock interrupt's alone.
 * - The task (core 0, LINE_PRIO, over the tracker): the fit, DC / gain / level (jl_cond), the cell's average and percent;
 *   and, while the LINE IN is the tracker's source (AUDIO > SOURCE, usbrole.c), the cubic resampler to 44.1 kHz (jp_cr) and
 *   uac_line_feed with each block's sample-stamped time. beat.c, beat_latency and AUTO BAR are unchanged: the same ring
 *   and offset as USB audio.
 * - Lost frames (the ring full: the task starved, or a flash stall while stopped) are fed as silence, so the times stay
 *   right; frames the DMA overwrote before the interrupt ran show as a step of the fit (counted).
 * - The cell: oneshot at the start (before continuous mode: the unit is locked while it runs, TIMING.md trap 6), then its
 *   in-pattern average each second, moved by the difference the first second showed (the sampling capacitor shared with
 *   LINE IN pulls it a few mV low: the board README).
 * Nothing here writes flash. No blanking of the board's own CLK / MIDI edges yet: TIMING.md says measure first (test 5). */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "board.h"
#include "jank_usbrole.h"
#include "usb_audio.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_line.h"
#include "jank_pcm.h"

#define LINE_PRIO 4                    /* over the tracker (3): it never waits on it; the ring holds SLOTS x 4.8 ms */
#define LINE_STACK 4096
#define SLOTS 16u                      /* frames: 77 ms, a power of two */
#define SKIP_FRAMES 2u                 /* the first frames after the start: the ADC settling [est], fed as silence */
#define CELL_BLOCK 41600u              /* cell samples a block: ~1 s */
#define BOOT_READS 32
#define CELL_OFFSET_MAX_MV 200         /* a boot-vs-pattern difference beyond this is not the capacitor's: not applied */
#define OUT_MAX (JL_FRAME * 1323u / 1250u + 2u)
#define WINDOW_US 1000000
#define CELL_DIV BOARD_CELL_DIV           /* (1 / 0 on a board without a cell: never used, s_cell_ch stays -1) */
#define CELL_EMPTY_MV BOARD_CELL_EMPTY_MV

static const char *TAG = "linein";

struct slot {
    int64_t stamp;                     /* the local clock at the frame's interrupt (midi_now_us_isr) */
    uint32_t seq;                      /* frames since the start, counted in the interrupt */
    struct jl_parse p;
    uint16_t line[JL_FRAME];
};

/* the interrupt's and the task's (single producer, single consumer, both on core 0) */
static struct slot s_ring[SLOTS];
static volatile uint32_t s_wr, s_rd;
static volatile uint32_t s_isr_seq, s_isr_dropped, s_isr_cycles;
static uint8_t s_line_ch;
static int8_t s_cell_ch = -1;
static TaskHandle_t s_task;

/* the task's own */
static adc_continuous_handle_t s_adc;
static adc_cali_handle_t s_cali;
static struct jl_fit s_fit;
static struct jl_cond s_cond;
static struct jl_cell s_cell;
static struct jp_cr s_rs;
static uint8_t s_feeding;              /* the resampler runs: the LINE IN is the source */
static int64_t s_rs_origin;            /* the line sample that is the resampler's input 0 */
static uint64_t s_rs_out;              /* outputs since */
static uint32_t s_next_seq;
static int32_t s_cell_off_mv;
static uint8_t s_cell_off_set;

/* for `line` and the battery, under s_mux */
struct line_pub {
    const char *state;                 /* "running", "starting" or what failed */
    esp_err_t err;
    uint32_t conv_hz, interval;
    uint8_t channels, cal, feeding;
    uint32_t frames, lost, isr_dropped, short_frames, other;
    uint32_t lat_avg_us, lat_max_us, lat_max_ever_us, steps;
    int32_t drift_ppb;
    int32_t dc_code;
    struct jl_level level;
    uint32_t clips_total;
    int8_t gain_db;
    int32_t cell_mv, cell_boot_mv, cell_pattern_mv;
    int8_t batt_pct;
    uint32_t cpu_x100;                 /* percent of core 0 x 100, interrupt and task, last second */
    uint32_t isr_avg_cycles, task_avg_cycles;
};
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static struct line_pub s_pub = {.state = "starting", .batt_pct = -1};

/* the DMA interrupt, once per frame: stamp, split, wake */
static bool IRAM_ATTR on_frame(adc_continuous_handle_t h, const adc_continuous_evt_data_t *e, void *arg)
{
    uint32_t c0 = esp_cpu_get_cycle_count();
    int64_t now = midi_now_us_isr();
    uint32_t seq = s_isr_seq++;
    BaseType_t woken = pdFALSE;
    (void)h;
    (void)arg;
    if (s_wr - s_rd >= SLOTS) {
        s_isr_dropped++;               /* the task is behind: this frame becomes silence there (its seq is missing) */
    } else {
        struct slot *s = &s_ring[s_wr & (SLOTS - 1u)];
        s->stamp = now;
        s->seq = seq;
        jl_parse((const uint32_t *)e->conv_frame_buffer, e->size / SOC_ADC_DIGI_RESULT_BYTES, s_line_ch, s_cell_ch, s->line, &s->p);
        s_wr = s_wr + 1u;
    }
    if (s_task)
        vTaskNotifyGiveFromISR(s_task, &woken);
    s_isr_cycles += esp_cpu_get_cycle_count() - c0;
    return woken == pdTRUE;
}

static esp_err_t channel_of(int gpio, uint8_t *ch)
{
    adc_unit_t unit;
    adc_channel_t c;
    esp_err_t e = adc_continuous_io_to_channel(gpio, &unit, &c);
    if (e == ESP_OK && unit != ADC_UNIT_1)
        e = ESP_ERR_INVALID_ARG;       /* ADC2 conflicts with Wi-Fi: ADC1 only */
    if (e == ESP_OK)
        *ch = (uint8_t)c;
    return e;
}

static int32_t to_mv(int raw)
{
    int mv = 0;
    if (s_cali && adc_cali_raw_to_voltage(s_cali, raw, &mv) == ESP_OK)
        return mv;
    return (int32_t)raw * 3100 / 4095;  /* no eFuse calibration: the 12 dB range's nominal ~3.1 V [est] */
}

/* the cell by oneshot, before continuous mode takes the unit: the reference for the in-pattern average */
static void cell_boot(void)
{
    adc_oneshot_unit_handle_t u;
    adc_oneshot_unit_init_cfg_t ucfg = {.unit_id = ADC_UNIT_1};
    adc_oneshot_chan_cfg_t ccfg = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
    int32_t sum = 0, n = 0;
    if (adc_oneshot_new_unit(&ucfg, &u) != ESP_OK)
        return;
    if (adc_oneshot_config_channel(u, (adc_channel_t)s_cell_ch, &ccfg) == ESP_OK)
        for (int i = 0; i < BOOT_READS; i++) {
            int raw;
            if (adc_oneshot_read(u, (adc_channel_t)s_cell_ch, &raw) == ESP_OK) {
                sum += raw;
                n++;
            }
        }
    adc_oneshot_del_unit(u);
    if (n)
        s_pub.cell_boot_mv = to_mv((int)((sum + n / 2) / n)) * CELL_DIV;
}

static esp_err_t adc_up(void)
{
    adc_digi_pattern_config_t pat[2];
    adc_continuous_handle_cfg_t hcfg;
    adc_continuous_config_t cfg = {0};
    adc_continuous_evt_cbs_t cbs = {.on_conv_done = on_frame};
    unsigned n = 1;
    esp_err_t e = channel_of(BOARD_LINEIN_GPIO, &s_line_ch);
    if (e == ESP_OK && BOARD_CELL_GPIO >= 0) {
        uint8_t c = 0;
        e = channel_of(BOARD_CELL_GPIO, &c);
        s_cell_ch = (int8_t)c;
        n = 2;
    }
    if (e != ESP_OK)
        return e;
    if (s_cell_ch >= 0) {
        adc_cali_curve_fitting_config_t cc = {.unit_id = ADC_UNIT_1, .chan = (adc_channel_t)s_cell_ch, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
        if (adc_cali_create_scheme_curve_fitting(&cc, &s_cali) != ESP_OK)
            s_cali = NULL;             /* no eFuse values: the nominal scale */
        s_pub.cal = (uint8_t)(s_cali != NULL);
        cell_boot();
    }
    /* the pattern: 12 dB for both (mini 2.2: LINE IN swings 0.05 .. 3.29 V at the pin; mini 3: about 1.47 V +- 1.47 V, board README) */
    pat[0] = (adc_digi_pattern_config_t){.atten = ADC_ATTEN_DB_12, .channel = s_line_ch, .unit = ADC_UNIT_1, .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH};
    if (n == 2u)
        pat[1] = (adc_digi_pattern_config_t){.atten = ADC_ATTEN_DB_12, .channel = (uint8_t)s_cell_ch, .unit = ADC_UNIT_1, .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH};
    hcfg = (adc_continuous_handle_cfg_t){
        .conv_frame_size = JL_FRAME * n * SOC_ADC_DIGI_RESULT_BYTES,
        /* the driver's own pool, never read: the frames are taken in on_frame. One frame; once full its sends fail cheaply */
        .max_store_buf_size = JL_FRAME * n * SOC_ADC_DIGI_RESULT_BYTES,
    };
    cfg.pattern_num = n;
    cfg.adc_pattern = pat;
    cfg.sample_freq_hz = jl_conv_hz(n);
    cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
    s_pub.channels = (uint8_t)n;
    s_pub.conv_hz = cfg.sample_freq_hz;
    s_pub.interval = jl_interval(80000000u, cfg.sample_freq_hz);   /* APB, ADC_DIGI_CLK_SRC_DEFAULT on the S3 */
    e = adc_continuous_new_handle(&hcfg, &s_adc);   /* here, on core 0: the GDMA interrupt is allocated on this core */
    if (e == ESP_OK)
        e = adc_continuous_config(s_adc, &cfg);
    if (e == ESP_OK)
        e = adc_continuous_register_event_callbacks(s_adc, &cbs, NULL);
    if (e == ESP_OK)
        e = adc_continuous_start(s_adc);
    return e;
}

/* the cell's second: millivolts, the boot reading's offset (taken once), the percent */
static void cell_second(void)
{
    int32_t mv = to_mv((int)((s_cell.avg_x16 + 8u) / 16u)) * CELL_DIV;
    int32_t corr;
    int pct;
    if (!s_cell_off_set) {
        int32_t d = s_pub.cell_boot_mv - mv;
        s_cell_off_mv = s_pub.cell_boot_mv > JL_NO_CELL_MV && d > -CELL_OFFSET_MAX_MV && d < CELL_OFFSET_MAX_MV ? d : 0;
        s_cell_off_set = 1;
    }
    corr = mv + s_cell_off_mv;
    pct = jl_batt_pct(corr, CELL_EMPTY_MV);
    portENTER_CRITICAL(&s_mux);
    s_pub.cell_pattern_mv = mv;
    s_pub.cell_mv = corr;
    s_pub.batt_pct = (int8_t)pct;
    portEXIT_CRITICAL(&s_mux);
}

/* the 44.1 kHz stream for the tracker, while the LINE IN is its source */
static void feed(const int16_t *x, int64_t first)
{
    static int16_t out[OUT_MAX];
    size_t n;
    if (!s_feeding) {
        jp_cr_reset(&s_rs);
        s_rs_origin = first;
        s_rs_out = 0;
        s_feeding = 1;
    }
    n = jp_cr_run(&s_rs, x, JL_FRAME, out, OUT_MAX);
    s_rs_out += n;
    /* the time just after the last output: output m stands for line position m x 1250 / 1323 from the origin, i.e.
     * origin + m x 10000 / 441 us, as usb_audio.c counts it (floor) */
    uac_line_feed(out, (uint32_t)n, jl_fit_time(&s_fit, s_rs_origin) + (int64_t)(s_rs_out * 10000u / 441u));
}

/* one frame (or a lost one: raw NULL) */
static void frame(const struct slot *s, int routed)
{
    static int16_t x[JL_FRAME];
    int64_t first = (int64_t)(s ? s->seq : s_next_seq) * JL_FRAME;
    if (s && s->seq >= SKIP_FRAMES) {
        if (s->stamp >= 0)
            jl_fit_add(&s_fit, first + JL_FRAME, s->stamp);
        jl_cond_run(&s_cond, s->line, JL_FRAME, x);
        if (s->p.cell_n && jl_cell_add(&s_cell, s->p.cell_sum, s->p.cell_n, CELL_BLOCK))
            cell_second();
    } else {
        memset(x, 0, sizeof x);
    }
    if (!routed)
        s_feeding = 0;
    else if (jl_fit_ready(&s_fit))
        feed(x, first);
}

static void publish(uint32_t frames, uint32_t lost, uint32_t short_frames, uint32_t other, uint64_t task_cycles, int64_t window_us)
{
    struct jl_level lv;
    uint32_t isr_c = s_isr_cycles;
    static uint32_t s_isr_c_last, s_frames_last;
    uint32_t dframes = frames - s_frames_last, disr = isr_c - s_isr_c_last;
    jl_cond_window(&s_cond, &lv);
    portENTER_CRITICAL(&s_mux);
    s_pub.frames = frames;
    s_pub.lost = lost;
    s_pub.isr_dropped = s_isr_dropped;
    s_pub.short_frames = short_frames;
    s_pub.other = other;
    s_pub.lat_avg_us = s_fit.lat_avg_us;
    s_pub.lat_max_us = s_fit.lat_max_us;
    s_pub.lat_max_ever_us = s_fit.lat_max_ever_us;
    s_pub.steps = s_fit.steps;
    s_pub.drift_ppb = s_fit.drift_ppb;
    s_pub.dc_code = jl_cond_dc(&s_cond);
    s_pub.level = lv;
    s_pub.clips_total = s_cond.clips_total;
    s_pub.gain_db = s_cond.gain_db;
    s_pub.feeding = s_feeding;
    s_pub.cpu_x100 = window_us > 0 ? (uint32_t)((task_cycles + disr) * 10000u / ((uint64_t)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * (uint64_t)window_us)) : 0u;
    s_pub.isr_avg_cycles = dframes ? disr / dframes : 0u;
    s_pub.task_avg_cycles = dframes ? (uint32_t)(task_cycles / dframes) : 0u;
    portEXIT_CRITICAL(&s_mux);
    s_isr_c_last = isr_c;
    s_frames_last = frames;
}

static void line_task(void *arg)
{
    uint32_t frames = 0, lost = 0, short_frames = 0, other = 0;
    uint64_t task_cycles = 0;
    int64_t win_start;
    int8_t gain = 0;
    esp_err_t e;
    (void)arg;
    jl_fit_init(&s_fit);
    jl_cond_init(&s_cond);
    jl_cell_init(&s_cell);
    jp_cr_init(&s_rs, JL_RATE_NUM, JL_RATE_DEN, UAC_RATE);
    e = adc_up();
    portENTER_CRITICAL(&s_mux);
    s_pub.err = e;
    s_pub.state = e == ESP_OK ? "running" : "failed";
    portEXIT_CRITICAL(&s_mux);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "ADC: %s", esp_err_to_name(e));
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "ADC1 ch %u%s at %lu Hz (interval %lu), %u-sample frames", s_line_ch, s_cell_ch >= 0 ? " + cell" : "",
             (unsigned long)s_pub.conv_hz, (unsigned long)s_pub.interval, (unsigned)JL_FRAME);
    uac_line_running(1);
    win_start = esp_timer_get_time();
    for (;;) {
        int64_t now;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        while (s_rd != s_wr) {
            const struct slot *s = &s_ring[s_rd & (SLOTS - 1u)];
            int routed = uac_source_now() == JUR_AUDIO_LINE;
            uint32_t c0 = esp_cpu_get_cycle_count();
            /* frames the interrupt could not keep: silence in their place, so that the sample count stays the clock */
            while (s_next_seq < s->seq) {
                frame(NULL, routed);
                s_next_seq++;
                lost++;
            }
            if (s->seq == s_next_seq) {
                frame(s, routed);
                s_next_seq++;
                frames++;
                if (s->p.line_n < JL_FRAME)
                    short_frames++;
                other += s->p.other;
            }
            task_cycles += esp_cpu_get_cycle_count() - c0;
            s_rd = s_rd + 1u;
        }
        now = esp_timer_get_time();
        if (now - win_start >= WINDOW_US) {
            struct jk_cfg cfg;
            app_cfg_get(&cfg);
            if (cfg.line_gain_db != gain) {
                gain = cfg.line_gain_db;
                jl_cond_gain(&s_cond, gain);
            }
            publish(frames, lost, short_frames, other, task_cycles, now - win_start);
            task_cycles = 0;
            win_start = now;
        }
    }
}

static esp_err_t la_start(void)
{
    /* core 0: the task allocates the ADC's interrupt, which belongs to the core that allocates it */
    return xTaskCreatePinnedToCore(line_task, "linein", LINE_STACK, NULL, LINE_PRIO, &s_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

static int la_batt_pct(void)
{
    int p;
    portENTER_CRITICAL(&s_mux);
    p = s_pub.batt_pct;
    portEXIT_CRITICAL(&s_mux);
    return p;
}


static void la_report(char *out, size_t cap)
{
    struct line_pub p;
    portENTER_CRITICAL(&s_mux);
    p = s_pub;
    portEXIT_CRITICAL(&s_mux);
    snprintf(out, cap,
             "line_in=%s\nerr=%s\nfeeds_tracker=%s\nadc_hz=%lu\ninterval=%lu\nchannels=%u\nframes=%lu\nlost=%lu\nisr_dropped=%lu\nshort=%lu\nother=%lu\n"
             "lat_avg_us=%lu\nlat_max_us=%lu\nlat_max_ever_us=%lu\nfit_steps=%lu\ndrift_ppb=%ld\n"
             "dc_code=%ld\npeak_db=%d\nrms_db=%d\nclips=%lu\nclips_total=%lu\ngain_db=%d\n"
             "cell_mv=%ld\ncell_boot_mv=%ld\ncell_pattern_mv=%ld\nbatt_pct=%d\ncal=%s\n"
             "cpu_pct=%lu.%02lu\nisr_cycles=%lu\ntask_cycles=%lu\nok\n",
             p.state, esp_err_to_name(p.err), p.feeding ? "yes" : "no", (unsigned long)p.conv_hz, (unsigned long)p.interval,
             p.channels, (unsigned long)p.frames, (unsigned long)p.lost, (unsigned long)p.isr_dropped, (unsigned long)p.short_frames,
             (unsigned long)p.other, (unsigned long)p.lat_avg_us, (unsigned long)p.lat_max_us, (unsigned long)p.lat_max_ever_us,
             (unsigned long)p.steps, (long)p.drift_ppb, (long)p.dc_code, p.level.peak_db, p.level.rms_db, (unsigned long)p.level.clips,
             (unsigned long)p.clips_total, p.gain_db, (long)p.cell_mv, (long)p.cell_boot_mv, (long)p.cell_pattern_mv, p.batt_pct,
             p.cal ? "efuse" : "nominal", (unsigned long)(p.cpu_x100 / 100u), (unsigned long)(p.cpu_x100 % 100u),
             (unsigned long)p.isr_avg_cycles, (unsigned long)p.task_avg_cycles);
}

/* no LINE IN, but a cell pin: a oneshot read every few seconds */

#define CELL_STEP_MS 5000u
#define CELL_SAMPLES 16

static adc_oneshot_unit_handle_t s_ob_unit;
static adc_cali_handle_t s_ob_cali;
static adc_channel_t s_ob_ch;
static int s_ob_up;                       /* 0 not tried, 1 ready, -1 failed: the percent stays -1 */
static uint32_t s_ob_last_ms;
static volatile int8_t s_ob_pct = -1;     /* one byte, written by the ui task only */

static int ob_up(void)
{
    adc_unit_t unit;
    adc_oneshot_unit_init_cfg_t ucfg = {.unit_id = ADC_UNIT_1};
    adc_oneshot_chan_cfg_t ccfg = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
    adc_cali_curve_fitting_config_t cc = {.unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
    if (adc_oneshot_io_to_channel(BOARD_CELL_GPIO, &unit, &s_ob_ch) != ESP_OK || unit != ADC_UNIT_1)
        return -1;
    if (adc_oneshot_new_unit(&ucfg, &s_ob_unit) != ESP_OK)
        return -1;
    if (adc_oneshot_config_channel(s_ob_unit, s_ob_ch, &ccfg) != ESP_OK)
        return -1;
    cc.chan = s_ob_ch;
    if (adc_cali_create_scheme_curve_fitting(&cc, &s_ob_cali) != ESP_OK)
        s_ob_cali = NULL;                 /* no eFuse values: the nominal scale, as in the line-in build */
    return 1;
}

static void ob_step(uint32_t now_ms)
{
    int32_t sum = 0, n = 0, mv;
    if (s_ob_up < 0 || (s_ob_up > 0 && (uint32_t)(now_ms - s_ob_last_ms) < CELL_STEP_MS))
        return;
    s_ob_last_ms = now_ms;
    if (s_ob_up == 0) {
        s_ob_up = ob_up();
        if (s_ob_up < 0) {
            ESP_LOGE("cell", "ADC1 oneshot for GPIO%d failed: no battery percent", BOARD_CELL_GPIO);
            return;
        }
    }
    for (int i = 0; i < CELL_SAMPLES; i++) {
        int raw;
        if (adc_oneshot_read(s_ob_unit, s_ob_ch, &raw) == ESP_OK) {
            sum += raw;
            n++;
        }
    }
    if (!n)
        return;                        /* keep the last value */
    sum = (sum + n / 2) / n;
    mv = 0;
    if (!s_ob_cali || adc_cali_raw_to_voltage(s_ob_cali, (int)sum, (int *)&mv) != ESP_OK)
        mv = sum * 3100 / 4095;        /* the 12 dB range's nominal ~3.1 V [est] */
    s_ob_pct = (int8_t)jl_cell_pin_pct(mv, BOARD_CELL_DIV, BOARD_CELL_EMPTY_MV);
}



/* the board's variant (board.h): LINE IN with the cell in its ADC pattern, the cell alone (oneshot), or neither */
esp_err_t linein_start(void)
{
    return BOARD_HAS_LINEIN ? la_start() : ESP_OK;
}

int linein_batt_pct(void)
{
    return BOARD_HAS_LINEIN ? la_batt_pct() : BOARD_CELL_GPIO >= 0 ? s_ob_pct : -1;
}

void linein_cell_step(uint32_t now_ms)
{
    if (!BOARD_HAS_LINEIN && BOARD_CELL_GPIO >= 0)
        ob_step(now_ms);                 /* (with a LINE IN the cell is in the ADC pattern) */
}

void linein_report(char *out, size_t cap)
{
    if (BOARD_HAS_LINEIN)
        la_report(out, cap);
    else if (BOARD_CELL_GPIO >= 0)
        snprintf(out, cap, "line_in=none\nbatt_pct=%d\nok\n", s_ob_pct);
    else
        snprintf(out, cap, "line_in=none\nok\n");
}
