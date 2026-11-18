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

/* The grid the primitives draw on: the OLED's 128 x 64, or the lcd2 layout's 160 x 120 for the one call of jo_draw_lcd2 (the emulator's
 * W and H, set per draw). Both are bit-per-pixel in the page layout, byte (y >> 3) * width + x. */
extern int jo_gw, jo_gh;

/* the pixels: v 1 lit, 0 dark; outside the screen: nothing */
void jo_px(uint8_t *fb, int x, int y, int v);
void jo_rect(uint8_t *fb, int x, int y, int w, int h, int v);
void jo_frame(uint8_t *fb, int x, int y, int w, int h, int v);
void jo_xor_col(uint8_t *fb, int x, int y0, int y1);
void jo_xor_px(uint8_t *fb, int x, int y);
static inline int jo_get(const uint8_t *fb, int x, int y) { return (fb[(y >> 3) * jo_gw + x] >> (y & 7)) & 1; }

/* the ink plane of the draw in progress (jo_draw_lcd2): tag marks the pixels that are LIT right now inside a box, so it is called after the
 * element is drawn; untag clears a box (under an overlay). No plane: nothing happens */
void jo_ink_begin(const uint8_t *fb, uint8_t *ink);
void jo_ink_end(void);
void jo_tag(int id, int x, int y, int w, int h);
void jo_untag(int x, int y, int w, int h);
void jo_ink_lit_only(void);               /* the ink only on lit pixels (a tick XOR-ed through a fill unlights some) */

/* the 5 x 7 font, scaled by sc: narrow . : , (3 px), space 4 px, the rest 6 px */
int jo_text_w(const char *s, int sc);
void jo_text(uint8_t *fb, const char *s, int x, int y, int sc, int v);
void jo_text_r(uint8_t *fb, const char *s, int xr, int y, int sc, int v);
void jo_text_c(uint8_t *fb, const char *s, int y, int sc, int v);
/* rows of '#' / '.' as an icon at x, y: lit pixels only */
void jo_icon(uint8_t *fb, const char *const *rows, int n, int x, int y);

/* the menus */
#define JO_MENU_ITEMS 9
#define JO_MENU_ITEMS_LCD2 10
#define JO_VISIBLE 6
#define JO_SETUP 3
enum jo_item_kind { JO_ITEM_SUB, JO_ITEM_EDIT, JO_ITEM_NET, JO_ITEM_INFO };
struct jo_item {
    const char *name;
    uint8_t kind, ref;                    /* the SUB's index, or the EDIT's field */
};
extern const struct jo_item jo_menu[JO_MENU_ITEMS];
extern const struct jo_item jo_menu_lcd2[JO_MENU_ITEMS_LCD2];          /* the lcd2 layout's: 10 items */

struct jo_spec {
    const char *title;
    const char *const *opts;              /* a list of choices (value = index), else a number lo..hi in steps */
    uint8_t nopts;
    int lo, hi, step;
    const char *unit;
    uint8_t live, repick;                 /* applied at every step; YES on the same value applies it again */
    const char *const (*help)[2];         /* per choice, or one pair for a number */
};
const struct jo_spec *jo_spec(enum jo_field f);                       /* the OLED layout's */
const struct jo_spec *jo_spec_l(enum jo_field f, int lcd2);            /* lcd2: its own AUTO BAR and SOURCE, and USB MIDI CLOCK */
int jo_spec_get(enum jo_field f, const struct jo_ctx *c);              /* by c->layout_lcd2 */
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
extern const char *const jo_autobar_short[3];
extern const char *const jo_off_on_names[2];                           /* ON, OFF: USB MIDI CLOCK */

/* shared by the lcd2 layout's screens (jank_oled_lcd2.c) and the OLED's (jank_oled_draw.c) */
extern const char *const *const jo_lt_icons[3];
extern const char *const jo_usb_icon[5];
void jo_jank_glyph(uint8_t *fb, int x, int y);
void jo_fit(const char *s, int w, char *out, size_t cap);
#endif
