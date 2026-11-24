/* SPDX-License-Identifier: GPL-3.0-only */
/* The button, the LED and the group scan.
 * Click: stopped by the user's layer (power-on, a Stop: core/jank_hold.h): the user's Start (bridge_play, as `play`), the
 * only Start a unit with no screen and no computer has; started: resync (jc_resync: stop now, start on the next bar line). Double click: to the next GROUP heard on the air.
 * Held 1.5 s and let go: MODE JANK <-> the Wi-Fi network, a restart.
 * Held 10 s: download mode, for flashing. The LED: the session's beat, inverted on the Wi-Fi network; with AUTO BAR
 * locked the bar's 1 flashes amber.
 *
 * Boards with the D-pad (board.h BOARD_BTN_*: the mini 2.2, the XIAO's UI bench): UP, DOWN, YES and NO (the button above)
 * drive the screens and menus of uiscreen.c / core/jank_oled.h, polled here every 5 ms and debounced (JB_DEBOUNCE_MS),
 * no interrupt. With a screen, NO is the menus' NO and 10 s still means download mode; the click, double click and long hold are the menus' business.
 * Without a screen, NO keeps all of the above, and UP / DOWN / YES act as on HOME. The RGB LEDs speak the LED language
 * (core/jank_led.h); the XIAO's one plain LED keeps the beat as before. */
#include <string.h>
#include "app.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "jank_led.h"
#if BOARD_ANY_RGB_LED
#include "led_strip.h"
#endif
#include "oled.h"
#include "uiscreen.h"

#define POLL_MS 5u
/* the screen's state is kept (a snapshot 10 times a second) for a screen, a D-pad or the RGB LEDs' language: not on the
 * plain XIAO, which stays as it was */
#define UI_STATE (uis_has_screen() || BOARD_LED_RGB_GPIO >= 0 || BOARD_BTN_UP_GPIO >= 0 || BOARD_BTN_DOWN_GPIO >= 0 || BOARD_BTN_YES_GPIO >= 0)
#define LED_EVERY_MS 20u                 /* the RGB LEDs are refreshed every frame: a dip in a Wi-Fi burst is repaired */

void app_bootloader(void);
void app_reboot(void);

static const char *TAG = "ui";
#if BOARD_ANY_RGB_LED
static led_strip_handle_t s_strip;       /* the RGB LEDs (BOARD_LED_RGB_GPIO >= 0) */
#endif
static SemaphoreHandle_t s_scanning;     /* one sweep at a time: the button's or the page's */
static enum jl_event s_event;
static uint32_t s_event_at, s_event_group;

static struct uis_led s_led;           /* the state the screen shows, for the LED language */

/* LED1 shows it; LED2 too on USB power only (on the cell one LED at a time: the board README's 24 mA) */
static void led_show(uint8_t r, uint8_t g, uint8_t b)
{
#if BOARD_ANY_RGB_LED
    if (BOARD_LED_RGB_GPIO >= 0 && s_strip) {
        led_strip_clear(s_strip);
        if (r || g || b) {
            led_strip_set_pixel(s_strip, 0, r, g, b);
            if (BOARD_LED_COUNT > 1 && s_led.on_usb)
                led_strip_set_pixel(s_strip, BOARD_LED_COUNT - 1, r, g, b);
        }
        led_strip_refresh(s_strip);
        return;
    }
#endif
    if (BOARD_LED_MONO_GPIO >= 0) {
        gpio_set_level(BOARD_LED_MONO_GPIO, !(r || g || b));   /* active low */
    }                                    /* else no LED on a pin (the lcd2): the screen is the language */
}

static void led_event(enum jl_event e, uint32_t group)
{
    s_event = e;
    s_event_at = bridge_ms();
    s_event_group = group;
}

/* During a host-mode flash window (flip) a lit LED goes dark and a dark one lights: the flash shows against the MODE WIFI LED that rests
 * lit as much as against one that rests dark */
static void led_show_host(int flip, uint8_t r, uint8_t g, uint8_t b)
{
    if (!flip)
        led_show(r, g, b);
    else if (r || g || b)
        led_show(0, 0, 0);
    else
        led_show(JLG_HOST_RGB);
}

static void led_step(uint32_t now)
{
    static uint32_t last;
    uint8_t c[3];
    struct jk_cfg cfg;
    int64_t beat = 0;
    int32_t mpb = 0;
    int joined, lit, flip;
    if (now - last < LED_EVERY_MS)
        return;
    last = now;
    if (jl_pattern(s_event, s_event_group, now - s_event_at, c)) {
        led_show(c[0], c[1], c[2]);
        return;
    }
    s_event = JL_NONE;
    app_cfg_get(&cfg);
    joined = bridge_beat(&beat, &mpb);
    if (BOARD_LED_RGB_GPIO >= 0) {
        struct jlg_state s = {now, s_led.batt_pct, s_led.on_usb, (uint8_t)joined, s_led.running, s_led.armed, s_led.low_conf,
                              s_led.bar_locked, beat, mpb, cfg.quantum, (uint8_t)usb_is_host()};
        if (!s_led.valid)
            s.batt_pct = -1;
        jlg_color(&s, c);
        led_show(c[0], c[1], c[2]);
        return;
    }
    flip = jlg_host_overlay(0, now, usb_is_host());   /* USB host mode: two short flashes every 2.4 s, the opposite of what the LED would show */
    lit = jk_led_lit(cfg.net, joined, beat, mpb, cfg.quantum);
    if (joined && lit != (cfg.net == JK_NET_WIFI) && beat_bar_state() == 2 && jk_led_bar_one(beat, cfg.quantum))
        led_show_host(flip, JK_LED_ONE_RGB);   /* AUTO BAR locked: the bar's 1 flashes amber (the music's 1) */
    else if (!lit)
        led_show_host(flip, 0, 0, 0);
    else if (cfg.net == JK_NET_WIFI)
        led_show_host(flip, 0, 0, 40);         /* (an RGB LED: blue on a network, green in MODE JANK) */
    else
        led_show_host(flip, 0, 30, 0);
}

/* MODE JANK <-> the Wi-Fi network: the radio is set up at start-up, so a restart. No network saved: red pulses */
static void switch_network(void)
{
    struct jk_cfg cfg;
    app_cfg_get(&cfg);
    if (cfg.net == JK_NET_JANK && !cfg.ssid[0]) {
        led_event(JL_NO_GROUP, cfg.group);
        return;
    }
    cfg.net = cfg.net == JK_NET_WIFI ? JK_NET_JANK : JK_NET_WIFI;
    app_cfg_set(&cfg);
    app_reboot();
}

static void move_to(uint32_t group)
{
    struct jk_cfg cfg;
    app_cfg_get(&cfg);
    cfg.group = (uint8_t)group;
    app_cfg_set(&cfg);
}

/* Off the home channel for two dwells: this is all of the time the stick is away. The MIDI clock's timer is not touched */
static void sweep(uint32_t home_group)
{
    uint8_t plan[2];
    uint32_t n = js_plan(home_group, plan);
    esp_err_t e;
    bridge_away(1);
    for (uint32_t i = 0; i < n; i++) {
        e = radio_channel(plan[i]);
        if (e != ESP_OK)
            ESP_LOGE(TAG, "scan channel %u: %s", plan[i], esp_err_to_name(e));
        vTaskDelay(pdMS_TO_TICKS(JS_DWELL_MS));
    }
    e = radio_channel(jk_channel(home_group));
    if (e != ESP_OK)
        ESP_LOGE(TAG, "back home: %s", esp_err_to_name(e));
    bridge_away(0);
}

/* The button. The home channel needs no dwell: the radio listens there all the time, so what it heard in the last
 * JS_HEARD_MS counts. */
static void scan_groups(void)
{
    struct jk_cfg cfg;
    struct js_scan seen;
    uint32_t next;
    app_cfg_get(&cfg);
    if (cfg.net == JK_NET_WIFI)
        return;                          /* groups are MODE JANK's */
    xSemaphoreTake(s_scanning, portMAX_DELAY);
    led_event(JL_SCANNING, cfg.group);
    led_step(bridge_ms() + LED_EVERY_MS);
    sweep(cfg.group);
    bridge_note_scan_groups(&seen);
    next = js_pick(&seen, cfg.group, bridge_ms());
    if (next) {
        move_to(next);
        led_event(JL_JOINED, next);
    } else {
        led_event(JL_NO_GROUP, cfg.group);
    }
    xSemaphoreGive(s_scanning);
}

/* The automatic scan: alone at home for JS_ALONE_MS, a sweep every JS_AUTO_EVERY_MS until a group is heard (core/jank_ui.c
 * js_auto_step). Quiet on the LED but for the sweep's magenta: no red "none found" every five seconds. Skipped while the
 * button's or the page's sweep holds the radio. */
#define AUTO_CHECK_MS 250u
#define CLOCK_PEERS_MS 1000u           /* host mode: how often the clock to the Digitone is allowed or stopped by the peers count (usb.c) */
#define BEAT_FEED_MS 50u               /* host mode: how often the session's beat is sampled for the Launchpad's playhead (usb_lp.c extrapolates between) */
static struct js_auto s_auto;

static void auto_scan(uint32_t now)
{
    struct jk_cfg cfg;
    struct js_scan seen;
    uint32_t next;
    app_cfg_get(&cfg);
    if (cfg.net != JK_NET_JANK || !js_auto_step(&s_auto, bridge_peers_heard(), now))
        return;
    if (xSemaphoreTake(s_scanning, 0) != pdTRUE)
        return;
    if (BOARD_LED_RGB_GPIO >= 0) {       /* (a mono LED cannot show magenta: it would light for the whole sweep, a long flash every 6 s) */
        led_event(JL_SCANNING, cfg.group);
        led_step(bridge_ms() + LED_EVERY_MS);
    }
    sweep(cfg.group);
    bridge_note_scan_groups(&seen);
    next = js_pick(&seen, cfg.group, bridge_ms());
    if (next) {
        ESP_LOGI(TAG, "alone on group %u: joining group %lu", cfg.group, (unsigned long)next);
        move_to(next);
        led_event(JL_JOINED, next);
    } else {
        led_event(JL_NONE, cfg.group);
    }
    js_auto_swept(&s_auto, bridge_ms());
    xSemaphoreGive(s_scanning);
}

/* The control page. The table counts from the start of a sweep, so the home channel gets a dwell of its own, at home */
esp_err_t ui_scan_table(struct js_table *out)
{
    struct jk_cfg cfg;
    app_cfg_get(&cfg);
    if (!s_scanning || cfg.net == JK_NET_WIFI)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_scanning, portMAX_DELAY);
    led_event(JL_SCANNING, cfg.group);
    bridge_table_start();
    vTaskDelay(pdMS_TO_TICKS(JS_DWELL_MS));
    sweep(cfg.group);
    bridge_table_stop(out);
    led_event(JL_NONE, cfg.group);
    xSemaphoreGive(s_scanning);
    return ESP_OK;
}

static void on_button(enum jb_event e)
{
    if (uis_has_screen()) {              /* the click, double click and long hold are the menus' (GROUP, NETWORK) */
        if (e != JB_DOWNLOAD)
            return;
        uis_boot_screen();
        vTaskDelay(pdMS_TO_TICKS(100));   /* the frame goes out first */
        app_bootloader();
        return;
    }
    switch (e) {
    case JB_CLICK:
        if (bridge_user_stopped())
            bridge_play(1);              /* M2h: power-on is stopped; this is the screenless unit's Start */
        else
            midi_resync();
        break;
    case JB_DOUBLE:
        scan_groups();
        break;
    case JB_LONG:
        switch_network();
        break;
    case JB_DOWNLOAD:
        app_bootloader();
        break;
    default:
        break;
    }
}

/* the D-pad: a level holds JB_DEBOUNCE_MS before it counts */
struct pad {
    int gpio;
    uint8_t raw, level;
    uint32_t t_raw;
};
static struct pad s_pad[JO_BTNS];       /* the pins from the board table (ui_start) */

static int pressed(int gpio) { return gpio >= 0 && !gpio_get_level(gpio); }   /* to GND, pulled up */

/* NO: the board's button (BOOT), or the bench's second one */
static int no_pressed(void)
{
    int level = gpio_get_level(BOARD_BUTTON_GPIO);
    return (BOARD_BUTTON_ACTIVE_LOW ? !level : level) || pressed(BOARD_BTN_NO2_GPIO);
}

static void pads_step(uint32_t now, int no)
{
    for (int i = 0; i < JO_BTNS; i++) {
        struct pad *p = &s_pad[i];
        uint8_t raw = (uint8_t)(i == JO_NO ? no : pressed(p->gpio));
        if (i == JO_NO && !uis_has_screen())
            continue;                    /* NO is the one-button UI's without a screen */
        if (p->gpio < 0 && i != JO_NO)
            continue;
        if (raw != p->raw) {
            p->raw = raw;
            p->t_raw = now;
        }
        if (p->raw != p->level && now - p->t_raw >= JB_DEBOUNCE_MS) {
            p->level = p->raw;
            uis_button((enum jo_btn)i, p->level, now);
        }
    }
}

static void ui_task(void *arg)
{
    struct jb_button b;
    uint32_t last_auto, last_clock_check, last_beat_feed;
    int lp_started = 0;                  /* the Launchpad's start / stop button has the transport started (usb_midi_clock_force) */
    (void)arg;
    jb_init(&b);
    js_auto_init(&s_auto, bridge_ms());
    last_auto = bridge_ms();
    last_clock_check = bridge_ms();
    last_beat_feed = bridge_ms();
    uis_start(bridge_ms());
    for (;;) {
        uint32_t now = bridge_ms();
        int no = no_pressed();
        pads_step(now, no);
        on_button(jb_step(&b, no, now));
        if (UI_STATE)
            uis_step(now, &s_led);
        uis_sys_step(now);               /* (the ABOUT numbers for the control line, also where there is no screen) */
        led_step(now);
        if ((uint32_t)(now - last_auto) >= AUTO_CHECK_MS) {
            last_auto = now;
            auto_scan(now);
        }
        if ((uint32_t)(now - last_clock_check) >= CLOCK_PEERS_MS) {   /* host mode: the clock to the Digitone only while there are peers (usb.c) */
            last_clock_check = now;
            usb_midi_clock_peers(usb_is_host() ? bridge_peers_heard() > 0u : 1);
            if (lp_started && !bridge_is_playing()) {   /* the transport was stopped by some other means: the Launchpad's override ends too */
                usb_midi_clock_force(0);
                lp_started = 0;
            }
        }
        if (usb_is_host() && (uint32_t)(now - last_beat_feed) >= BEAT_FEED_MS) {   /* the step modes' playhead: the session's beat, sampled here (the lock), used by usb_lp.c */
            int64_t beat_ub = 0;
            int32_t mpb = 0;
            int known;
            last_beat_feed = now;
            known = bridge_beat(&beat_ub, &mpb);
            usb_lp_beat_feed(known, beat_ub, mpb);
        }
        if (usb_is_host() && usb_lp_transport_request()) {   /* the Launchpad's start / stop scene button (usb_lp.c asks; this task does the bridge's work) */
            int start = !bridge_is_playing();
            if (start)
                usb_midi_clock_force(1);       /* an explicit start reaches the Digitone even with no peers (usb.c); before the Start byte is made */
            bridge_play(start);
            if (!start)
                usb_midi_clock_force(0);       /* after the Stop byte */
            lp_started = start;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

static esp_err_t led_up(void)
{
#if BOARD_ANY_RGB_LED
    if (BOARD_LED_RGB_GPIO >= 0) {
        led_strip_config_t strip = {.strip_gpio_num = BOARD_LED_RGB_GPIO, .max_leds = (uint32_t)BOARD_LED_COUNT, .led_model = LED_MODEL_WS2812, .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB};
        led_strip_rmt_config_t rmt = {.clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 10000000};
        return led_strip_new_rmt_device(&strip, &rmt, &s_strip);
    }
#endif
    if (BOARD_LED_MONO_GPIO >= 0) {
        esp_err_t e = gpio_set_direction(BOARD_LED_MONO_GPIO, GPIO_MODE_OUTPUT);
        return e == ESP_OK ? gpio_set_level(BOARD_LED_MONO_GPIO, 1) : e;
    }
    return ESP_OK;
}

esp_err_t ui_screen_early(void)
{
    uis_init();
    uis_open_screen();                   /* (none: the LED language) */
    return ESP_OK;
}

esp_err_t ui_start(void)
{
    esp_err_t e = ESP_OK;
    const int PADS[] = {BOARD_BTN_UP_GPIO, BOARD_BTN_DOWN_GPIO, BOARD_BTN_YES_GPIO, BOARD_BTN_NO2_GPIO};
    s_pad[JO_UP].gpio = BOARD_BTN_UP_GPIO;
    s_pad[JO_DOWN].gpio = BOARD_BTN_DOWN_GPIO;
    s_pad[JO_NO].gpio = BOARD_BUTTON_GPIO;
    s_pad[JO_YES].gpio = BOARD_BTN_YES_GPIO;
    gpio_config_t c = {.pin_bit_mask = 1ull << BOARD_BUTTON_GPIO, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    for (size_t i = 0; i < sizeof PADS / sizeof PADS[0]; i++)
        if (PADS[i] >= 0)
            c.pin_bit_mask |= 1ull << PADS[i];
    e = gpio_config(&c);
    if (e == ESP_OK)
        e = oled_start();
    s_scanning = xSemaphoreCreateMutex();
    if (!s_scanning)
        return ESP_ERR_NO_MEM;
    if (e == ESP_OK)
        e = led_up();
    if (e != ESP_OK)
        return e;
    return xTaskCreatePinnedToCore(ui_task, "ui", 6144, NULL, 5, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
