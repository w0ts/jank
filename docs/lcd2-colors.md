# jank on the Waveshare ESP32-S3-LCD-2: the colour screen on its own 160 x 120 grid (design preview, v4)

The variant uses a 2.0 inch 240 x 320 ST7789 colour LCD **in landscape** (320 x 240), no touch, no RGB LEDs. The owner said the screen
was not used fully (the 128 x 64 UI doubled is 256 x 128 with black bands and small text) and asked for more visual feedback, small
progress bars. So the lcd2 layout has **its own logical grid of 160 x 120 pixels (4:3)**, the same 5 x 7 font and primitives at integer
scales, painted **exactly x2** on 320 x 240: no bands, no centring, every pixel square. The lid window (43.4 x 33.2 mm) and the active
area (40.8 x 30.6 mm) are 4:3 too, so a source pixel is a 0.255 mm square. The OLED boards keep 128 x 64 and are bit-identical.

Colour is information only: the screen is neutral (near-white on near-black, menus and edit screens included) and a colour appears only
on an element that says something the white version cannot. Screens: `lcd2v4-states.png` (12 situations), `lcd2v4-home.png` (the control
page), `lcd2v4-menus.png` (menus, sub menu, edit, info, overlay).

## How a caller learns the size

`OLED.size(layout)` returns `{ w, h }`: `OLED.size()` / `'oled'` = 128 x 64, `'lcd2'` = 160 x 120 (`OLED.SIZES`). `OLED.W` and `OLED.H` stay
the OLED's. `OLED.draw(fb, ui, ctx, now, ink)` keeps its signature: with `ctx.layout === 'lcd2'` `fb` (and `ink`) must be 160 * 120 bytes
(else it throws), any other layout 128 * 64. The grid is a module variable set for the duration of one `draw()` and restored after.
`Stick.screen(model, now, layout)` makes the buffers itself and returns `{ fb, ink, w, h }`; `Stick.framebuffer(model, now)` is the OLED's
128 x 64 as before. `Lcd2.paint(ctx2d, img, fb, ink)` paints a 160 x 120 frame as 320 x 240.

## The ink plane (unchanged idea)

`ink` is a `Uint8Array(160 * 120)`; 0 = neutral. Ids (`OLED.INK`, meanings not colours): `GOOD` 1, `WARN` 2, `BAD` 3, `DIM` 4, `ONE` 5 (the
bar's first beat while lit). **No id was added.** Only lit pixels carry ink (`draw()` clears ink under unlit pixels, e.g. where a tick is
XOR-ed through a fill). Menus, edit, info, network, text, confirm, setup screens and overlays: no ink at all.

## The palette (exact RGB, WCAG AA against the background, tested; unchanged)

| name | RGB | hex | contrast on bg | means |
|---|---|---|---|---|
| bg | 8, 10, 14 | #080a0e | | unlit |
| neutral | 232, 236, 244 | #e8ecf4 | 16.7 | everything without a meaning |
| good | 72, 222, 128 | #48de80 | 11.4 | joined, strong, above the line, charged |
| warn | 255, 176, 32 | #ffb020 | 10.8 | fair, near the line, low, too quiet; also the bar's first beat |
| bad | 255, 104, 92 | #ff685c | 7.0 | poor, below the line, clipping, empty |
| dim | 150, 156, 168 | #969ca8 | 7.2 | nothing there / not reported |

## HOME (AUDIO) on 160 x 120: the row table (`OLED.LCD2` holds the numbers)

| y | height | element | content and colour |
|---|---|---|---|
| 0..12 | 13 | top bar | x 2 transport icon (triangle 9 high, square 7, blinks while armed); x 14 fold label or LINK option icon; `GRP n` / SSID right-aligned to x 66; x 70 link glyph + Link peer count; x 98 signal bars (3 bars 3 px wide, 3 / 6 / 9 high); x 119 USB plug; x 134 battery icon 21 x 9 with nub |
| 15..49 | 35 | TEMPO | font scale 5 (digits 25 x 35, `120.0` is 130 px wide, x 2..131; the widest, 999.9, also fits); `BPM` at x 138, y 43..49 |
| 52..77 | 26 | beat boxes | four boxes 36 x 26 (they were 36 x 18), gap 4, margins 2, from x 2 to x 157: the beat indicator now that there are no LEDs; the lit one is a solid fill, the others 2 px outlines; the first is amber only while it is the lit one |
| 81..87 | 7 | `CF  92%` + line | label x 2; value right-aligned to x 60; one thin line from x 64 (3 px high, rows 83..85, its length the value on 94 px), a notch at the stop threshold (rows 82..86) |
| 91..97 | 7 | `IN -18dB` + line | the same columns and line; notch at the tracker's -50 dBFS sound level |
| 101..107 | 7 | `BAR LOCKED` + outlined bar | AUTO BAR's state word and its confidence, in v4's outlined bar (94 x 5, 3 px fill); only with AUTO BAR on (the row stays empty otherwise: the rows never move) |
| 111..117 | 7 | status line | left: `GATED` / `HOLD` / `COAST`, else the Wi-Fi line; right-aligned: `STOP 70` or `MIN 70` |

Every element has its own row, with gaps of 2 to 5 px between rows (the meter rows moved from a 12 px to a 10 px pitch to give the boxes their height); the test checks that no lit pixel lies in a gap, that the rows are in
increasing order, and that the status word and the threshold text on row 111 do not touch (the v3 overlap of GATED / COAST over STOP 70 is
gone because the words moved to the left of the same row).

**LINK layout**: the same top bar, tempo and status line (no STOP / MIN text, no words); the beat boxes are 36 x 52 (they were 36 x 36) at y 54..105, the same 2 px outlines and solid lit fill; no audio rows. Boxes are drawn on HOME only; no other screen shows them.

### The CF and IN meters: one thin line

A row is: label (neutral), the value (coloured), and **one line**: 3 px high (rows y + 2..y + 4 of the 7 px row), no outline, no separate
track. Its **length is the value on 94 px: line = round(94 x value)** from x 64; the unfilled remainder is plain background. **I chose no
dotted baseline**: with the value written beside it and the notch marking the one line that matters, the end of the line reads without
one (the screenshots show it). A **notch**, 1 px wide and 5 px tall (rows y + 1..y + 5), is XOR-ed through the line at
`barX + round(93 x tickFraction)`: a gap in the line where it passes the notch, a 5 px stub (one above, one below the line's rows) where it does not.
Value, line and notch carry the same ink id.

| line | fraction | notch | colour of value, line and notch |
|---|---|---|---|
| CF | confidence 0..1 (0 / 50 / 100 % = 0 / 47 / 94 px) | the stick's stop threshold `th` (70 % = x 129) | red below `th`; amber from `th` up to `th` + 10 points; green above; grey when the stick reports no value (`--`, no line) or there is silence |
| IN | (dBFS + 90) / 90 (-90 / -45 / 0 dB = 0 / 47 / 94 px) | -50 dBFS (x 105): the tracker's sound level | neutral; red when the last second's peak reaches -1 dBFS (or, if `peak_db` is not reported, the RMS maximum reaches -3 dB); amber under -50 dBFS (too quiet to track) |

The BAR row keeps v4's outlined bar (94 x 5, a 3 px fill inside it, so the fill is round(92 x confidence), neutral, no tick): AUTO BAR's
confidence is the only meter left with a frame. It sits under two frameless lines and is a little heavier, but it appears only with AUTO
BAR on and I found no clash worth changing; if the owner wants it uniform, it is one function (`outlinedRow` to `progressRow`).

**Position inside the bar**: not drawn as a bar (my choice, easy to add). The face's beat position is an extrapolation (`Stick.ctx` runs the
HOME line's phase forward with `phaseAt`, then floors it to the whole beat `beatN`), so a position bar would show the same estimate as the
four boxes, only smoother, and `ctx` does not expose the fractional position today (it would need one more field, e.g. `barPos`). The BAR
row shows what the stick reports about the bar that the boxes do not: AUTO BAR's state and confidence.

## What the stick reports for each coloured item (unchanged)

| element | colour rule | reported today |
|---|---|---|
| CF value, bar, tick | the rule above | HOME `conf`, `get` threshold and lowconf: yes |
| words `GATED` red, `HOLD` grey, `COAST` amber | outputs stopped for low confidence / holding / running under the threshold | status `gated=1`, HOME hold / coast suffix: yes |
| link glyph + count | joined (>= 1 Link peer) green; alone: grey in AUDIO mode, red in LINK mode | HOME `link` (Link peers): yes. "Searching" is not reported |
| signal bars | >= -67 dBm green (3 bars), -80..-68 amber (2), < -80 red (1), none or not reported grey stubs | status `rssi` (best jank peer, 0 = none) or `wifi_rssi`: yes. Retry / loss shares: no |
| battery | green from 30 %, amber 15..29, red under 15 | HOME `battery` percent: yes. **Charging: not reported** (no mark drawn) |
| IN | rule above | HOME `level`, status `peak_db`: yes |
| first beat | amber while it is the lit box | Link bar phase: yes |
| BAR row | neutral | status `bar`, `bar_lead` (via `Stick.ctx` `bar`): yes, when AUTO BAR is on |

## The other screens on the bigger grid (neutral, the selected row inverted, no ink)

| screen | geometry |
|---|---|
| title bar | 11 rows, full width, text at y 2 |
| MENU | 10 rows of 9 px from y 13 (the 10 items of the lcd2 menu all fit: no scrolling; row k at y 13 + 9k, last at y 94; the OLED menu has 9), value right-aligned at x 156, selected row inverted over the full width |
| sub menu (AUDIO, OFFSET, DISPLAY) | rows of 12 px from y 16 |
| edit | the value at font scale 2 in an inverted box centred on 160 (scale 1 if it would not fit), the two help lines at y 70 and 82 |
| info (BATTERY) | text rows of 12 px from y 18, the level bar 152 px wide at y 92 |
| NETWORK / scan list | rows of 9 px like the menu (scan: 11 rows, the selected one kept near the middle, signal bars at x 144), the selected row's help at y 101 and 110 |
| text editor | 25 character cells of 6 px from x 5, the help lines at y 46, 58, 70 |
| confirm | two lines centred at y 34 and 46, `YES      NO` at scale 2 at y 84 |
| SETUP (QR) | the QR box on the left at module size 4, 3 or 2 (the largest that fits 118 rows and leaves 66 px for the text: 25 modules at 3 px = 81 px box); text right of it |
| overlays | one box 130 x 60 at (15, 30), centred; the value at the largest scale of 4, 3, 2, 1 that fits; the NO / YES hint uses it too; SAVED: 90 x 34 at (35, 42), text scale 2 |
| BOOT / splash | `BOOT` at scale 5; `JANK` at scale 5 and `MINI 2.2` at scale 3 |

The cursor logic in `OLED.press` still scrolls as if 6 menu rows were visible (`ui.top`); the lcd2 draw ignores `ui.top` and keeps the
cursor in view by itself (all 10 menu items fit; the scan list centres on the selection), so no state is shared between the layouts.

## Recommendation

Keep: the tempo as the biggest element, the CF and IN lines with their notches, the beat boxes with the amber first one, the top bar. The BAR
row is a candidate to cut if the owner never turns AUTO BAR on (the space then stays empty). `MIN 70` / `STOP 70` is useful but could be
cut to the colour alone.

## What the C side will need later (not written)

1. `jo_draw` / `jo_draw_ink` get a layout size: 160 x 120 frame buffer and ink plane (19200 bytes each; the OLED path keeps 1024 / 8192), the
   `LCD2` row table above, and the lcd2 screens (lists, edit, info, setup, overlays) as `ui/oled.js` draws them. `jank_lcd.c` paints the 160 x
   120 frame x2 with the palette.
2. `tests/oled_ref/gen_ink.js` writes planes of 128 x 64 (`W = OLED.W`) and the C `test_oled.c` replays them: both must move to the 160 x 120
   grid (the generator throws today, see below), and new cases cover the lines at 0 / 50 / 100 %, the notch, the words, the lists and the overlays.
3. Ctx fields as before: `rssi`, `peakDb`, `confKnown`, `peers`, `layout`, plus `bar` for the BAR row.
4. Firmware wishes (unchanged): a charging flag, Wi-Fi retry / loss shares, a "joining" Link state, `rssi` / `peak_db` on the HOME line.

## Tests in this stage

`tests/test_lcd2.js` (193 checks): palette AA; sizes and the throw on a wrong buffer; the exact x2 (a full frame covers all 320 x 240, an empty one
none, six single source pixels each become exactly their 2 x 2 block in their ink colour); the four boxes on every beat in both layouts (exact rectangles, solid lit box, 2 px outline and dark inside for the rest, even 4 px gaps and dark margins, amber only for a lit first box, gaps to the rows above and below); the row table (rows increasing, no lit pixel in
a gap, tempo 35 rows, the widest tempo fits); ink only in the allowed boxes; the CF rule at 60, 69, 70, 75, 80, 81, 87, 100 for value, line
and notch; line lengths at 0 / 50 / 100 % (exactly 3 rows, no outline, no track) and the notch position (also following the stick's threshold); IN clip / quiet edges and lengths; the BAR
row (still outlined); link, signal, battery; the status words and the threshold text; the LINK layout; no ink in menus, sub menus, edit, info, net, scan,
text, confirm, setup and overlays; overlays and SAVED centred; the OLED layout equal to golden hashes made by the OLED module from before the
lcd2 layout (8 screens) and unaffected after an lcd2 draw.

`tests/run.sh` **fails on purpose at its first lcd2 step**: it regenerates the ink references with `node tests/oled_ref/gen_cases.js --ink`
(`gen_ink.js`), which hands 128 x 64 buffers to the lcd2 layout, and `OLED.draw` now refuses them (`needs a 160 x 120 frame buffer`). That
is the C parity for the lcd2 ink cases (`cases_ink.txt` and the C `jo_draw_ink`, both still the 128 x 64 design); it stops the script before
`test_oled.c` and everything after it. Everything after that step was run without it and passes (see the report).
