/* SPDX-License-Identifier: GPL-3.0-only */
/* Inside the OLED UI (jank_oled.h): the drawing primitives, JavaScript's rounding and number text, the menus' tables. */
#ifndef JANK_OLED_INT_H
#define JANK_OLED_INT_H
#include "jank_oled.h"

/* JavaScript's Math.round: the nearest integer, halves up (toward +infinity) */
double jo_round(double x);
static inline int jo_iround(double x) { return (int)jo_round(x); }
static inline double jo_clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int jo_clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
/* JavaScript's Number.prototype.toFixed(digits) for |x| < 1e21: the exact value, halves away from zero */
void jo_fixed(double x, int digits, char *out, size_t cap);

/* the pixels: v 1 lit, 0 dark; outside the screen: nothing */
void jo_px(uint8_t *fb, int x, int y, int v);
void jo_rect(uint8_t *fb, int x, int y, int w, int h, int v);
void jo_frame(uint8_t *fb, int x, int y, int w, int h, int v);
void jo_xor_col(uint8_t *fb, int x, int y0, int y1);
void jo_xor_px(uint8_t *fb, int x, int y);
static inline int jo_get(const uint8_t *fb, int x, int y) { return (fb[(y >> 3) * JO_W + x] >> (y & 7)) & 1; }

/* the 5 x 7 font, scaled by sc: narrow . : , (3 px), space 4 px, the rest 6 px */
int jo_text_w(const char *s, int sc);
void jo_text(uint8_t *fb, const char *s, int x, int y, int sc, int v);
void jo_text_r(uint8_t *fb, const char *s, int xr, int y, int sc, int v);
void jo_text_c(uint8_t *fb, const char *s, int y, int sc, int v);
/* rows of '#' / '.' as an icon at x, y: lit pixels only */
void jo_icon(uint8_t *fb, const char *const *rows, int n, int x, int y);

/* the menus */
#define JO_MENU_ITEMS 9
#define JO_VISIBLE 6
#define JO_SETUP 3
enum jo_item_kind { JO_ITEM_SUB, JO_ITEM_EDIT, JO_ITEM_NET, JO_ITEM_INFO };
struct jo_item {
    const char *name;
    uint8_t kind, ref;                    /* the SUB's index, or the EDIT's field */
};
extern const struct jo_item jo_menu[JO_MENU_ITEMS];

struct jo_spec {
    const char *title;
    const char *const *opts;              /* a list of choices (value = index), else a number lo..hi in steps */
    uint8_t nopts;
    int lo, hi, step;
    const char *unit;
    uint8_t live, repick;                 /* applied at every step; YES on the same value applies it again */
    const char *const (*help)[2];         /* per choice, or one pair for a number */
};
const struct jo_spec *jo_spec(enum jo_field f);
int jo_spec_get(enum jo_field f, const struct jo_ctx *c);
void jo_edit_text(const struct jo_spec *sp, int v, char *out, size_t cap);
void jo_help_of(const struct jo_spec *sp, int v, const char **h1, const char **h2);

struct jo_sub {
    const char *title;
    uint8_t n;
    uint8_t rows[4];                      /* enum jo_field */
};
extern const struct jo_sub jo_subs[3];

/* the NETWORK screen's rows for the network's mode */
enum jo_net_act { JO_ACT_NONE = 0, JO_ACT_MODE, JO_ACT_GROUP, JO_ACT_SSID, JO_ACT_PASS, JO_ACT_APPASS, JO_ACT_APNAME, JO_ACT_STATUS };
struct jo_net_row {
    const char *name;
    char val[JO_TEXT_MAX + 8];
    uint8_t act;
    const char *help[2];
};
int jo_net_rows(const struct jo_ctx *c, struct jo_net_row rows[4]);

extern const char *const jo_rates_short[5];
extern const char *const jo_net_short[4];
extern const char *const jo_lt_short[3];
extern const char *const jo_usb_names[2];
#endif
