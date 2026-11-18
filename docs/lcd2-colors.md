# jank on the Waveshare ESP32-S3-LCD-2: the colour screen on its own 160 x 120 grid (design preview, v11)

The variant uses a 2.0 inch 240 x 320 ST7789 colour LCD **in landscape** (320 x 240), no touch, no RGB LEDs. The owner said the screen
was not used fully (the 128 x 64 UI doubled is 256 x 128 with black bands and small text) and asked for more visual feedback, small
progress bars. So the lcd2 layout has **its own logical grid of 160 x 120 pixels (4:3)**, the same 5 x 7 font and primitives at integer
scales, painted **exactly x2** on 320 x 240: no bands, no centring, every pixel square. The lid window (43.4 x 33.2 mm) and the active
area (40.8 x 30.6 mm) are 4:3 too, so a source pixel is a 0.255 mm square. The OLED boards keep 128 x 64 and are bit-identical.

Colour is information only: the screen is neutral (near-white on near-black, menus and edit screens included) and a colour appears only
on an element that says something the white version cannot. 

## How a caller learns the size

`OLED.size(layout)` returns `{ w, h }`: `OLED.size()` / `'oled'` = 128 x 64, `'lcd2'` = 160 x 120 (`OLED.SIZES`). `OLED.W` and `OLED.H` stay
the OLED's. `OLED.draw(fb, ui, ctx, now, ink)` keeps its signature: with `ctx.layout === 'lcd2'` `fb` (and `ink`) must be 160 * 120 bytes
(else it throws), any other layout 128 * 64. The grid is a module variable set for the duration of one `draw()` and restored after.
`Stick.screen(model, now, layout)` makes the buffers itself and returns `{ fb, ink, w, h }`; `Stick.framebuffer(model, now)` is the OLED's
128 x 64 as before. `Lcd2.paint(ctx2d, img, fb, ink)` paints a 160 x 120 frame as 320 x 240.

## The ink plane (unchanged idea)

`ink` is a `Uint8Array(160 * 120)`; 0 = neutral. Ids (`OLED.INK`, meanings not colours): `GOOD` 1, `WARN` 2, `BAD` 3, `DIM` 4, `ONE` 5 (the
bar's first beat while lit). The ids 6..21 (`OLED.VU = { ink0: 6, steps: 16 }`, not members of `OLED.INK`) are the VU meter's ramp; see below. Only lit pixels carry ink (`draw()` clears ink under unlit pixels, e.g. where a tick is
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

## v11: what changed (owner review of v10)

1. **AUDIO beat boxes are squares** (27 x 27, equal gaps, centred), the same size with and without the BAR row.
2. **Padding**: 4 empty rows between the tempo and the squares and between the squares and the text below; even gaps between the text rows.
3. **LINK**: 4 empty rows between the top bar icons and the tempo (the same rule applies in AUDIO: the tempo moved from y 15 to y 16); the squares are re-fitted under it.

(v10: INPUT above CONFIDENCE, even rows, the centred state word without PLAYING, the LARGE LINK digits, the battery with the plug inside, GRP n next to the link glyph, the LINK icon in both
modes. v9: the small digit after the dot, the centred tempo, the signal on the NETWORK screen, the state line.)

## The top bar (y 1..11, both modes; `OLED.LCD2` holds the numbers)

Every item is a fixed slot, nothing moves when a value changes; items are 11 rows high (text at y 3).

| group | item | x range | notes |
|---|---|---|---|
| left | transport icon | 2..10 | play triangle 9 x 11 (blinks while armed) or stop square 9 x 9 |
| left | LINK TRANSPORT icon | 14..24 (IGNORE, 11 wide) / 14..26 (FOLLOW, BOTH, 13 wide) | **both modes** (`ctx.ltMode`): IGNORE = crossed circle, FOLLOW = arrow in, BOTH = double arrow |
| left | fold label | 31..45 | AUDIO mode only, when the fold is not 1 (`X2` `X4` `/2` `/4`: an inverted 15 x 11 box); sits next to the LINK icon |
| right | `GRP n` (or the Wi-Fi / own network name) | right-aligned to x 98 (`GRP 1` is x 70..98; cut to 8 characters) | in AUDIO and LINK mode, directly before the link glyph |
| right | link glyph + peer count | glyph 103..115, count from x 118 (up to 2 digits, slot 103..129) | colour: joined green, alone grey (AUDIO) / red (LINK) |
| right | battery | 134..157 (22 x 11 body with corners cut, nub 2 x 5, fill 18 x 7 = round(18 x pct)) | neutral while healthy; amber under 30 % (`BATT_WARN_PCT`), red under 15 % (`BATT_BAD_PCT`); while charging the 9 x 7 plug (`PLUG_IN_BATTERY`) is XOR-ed into its middle (dark over the fill) |

Gaps: 4 px between GRP n and the link glyph (x 99..102) and between the link slot and the battery (x 130..133). Free width: x 46..69 (24 px) between the left group (with the fold label) and
`GRP n`; without a fold label (or in LINK mode) x 27..69 (43 px, 45 after the IGNORE icon). Right edge: the nub ends at x 157.

**Charging** is not reported by the stick; the lcd2 layout uses exactly the inference of the BATTERY info screen (`CHARGING`): `usb` and a cell fitted and under 100 %
(`charging(c)` in `ui/oled.js`); at 100 % with USB the info screen says FULL and no plug is drawn. `Stick.ctx` sets `usb` to true always, so on the page this reads as charging whenever the
battery is under 100 %; on a unit running from the cell it would be wrong until the firmware reports USB power. With no cell fitted nothing is drawn (there is no plug icon of its own any more).

## HOME (AUDIO) on 160 x 120: the row table

Padding: the icons end at y 11 and the tempo starts at y 16 (4 empty rows, the same rule in LINK); 4 empty rows between the tempo and the squares; 4 between the squares and the first
text row (INPUT) when the BAR row is shown. The four beat boxes are SQUARES 27 x 27 (width == height) with 4 px gaps, a row of 120 px centred (x 20..139), at the same place whether the
BAR row is shown or not. The state line keeps its reserved row at the bottom (y 113..119).

The BAR row exists only with AUTO BAR on (a state of `locked`, `listening` or `manual`): then the stack is the first table, else the second.

| y | height | element (with the BAR row) | empty rows before it |
|---|---|---|---|
| 1..11 | 11 | top bar | |
| 16..50 | 35 | TEMPO (NORMAL digits, centred; the digit after the dot 23 rows, y 28..50) | 4 (y 12..15) |
| 55..81 | 27 | four beat squares 27 x 27, gap 4, x 20..139 (lit = solid, unlit = 2 px outline; the first amber only while lit) | 4 (y 51..54) |
| 86..92 | 7 | `INPUT` + VU meter (rows 87..91, notch rows 86..92) + number | 4 (y 82..85) |
| 95..101 | 7 | `CONFIDENCE` + line (rows 97..99, notch rows 96..100) + value | 2 |
| 104..110 | 7 | `BAR` + state word + outlined bar (rows 105..109) | 2 |
| 113..119 | 7 | state word, centred, AUDIO only (reserved, empty while running) | 2 |

| y | height | element (without the BAR row) | empty rows before it |
|---|---|---|---|
| 16..50 | 35 | TEMPO | 4 |
| 55..81 | 27 | four beat squares (the same place) | 4 |
| 87..93 | 7 | `INPUT` | 5 (y 82..86) |
| 100..106 | 7 | `CONFIDENCE` | 6 |
| 113..119 | 7 | state word | 6 |

**Why 27**: the width would allow 36 (with 4 px gaps), the height does not: from the tempo's end (y 50) to row 119 there are 69 rows for the squares, INPUT, CONFIDENCE, BAR, the state line and the
paddings (4 + 4 above and below the squares, gaps of 2 between text rows): 69 - 4 - 4 - 3 x 7 - 3 x 2 - 7 = 27. (With 3 px paddings it would be 29.) The squares are the same size and place with and without the BAR row; the
9 rows the missing BAR row frees go to the gaps below the squares (5) and between the text rows (6, 6), evenly, so nothing is left empty inside the stack. The state line is not given back to
the stack while it is empty: anything that grew into it would have to jump back when a word appears.

**LINK layout**: the top bar above; the tempo in the LARGE digits (47 rows, y 16..62), centred (4 empty rows under the icons); four SQUARE beat boxes 36 x 36 with 4 px gaps, a row of 156 px
centred (x 2..157), at y 73..108, centred vertically in the space under the tempo (10 empty rows above, 11 below, both over the 4 px minimum); the first amber only while lit; no audio rows, no
state line. The boxes are as large as the width allows with 4 px gaps (4 x 36 + 3 x 4 = 156).

### The state word (AUDIO only)

At most one word, centred (`left = floor((160 - width) / 2)`), the first that applies (ctx fields `gated`, `confHold`, `confKnown`, `conf`, `th`, `running`, `silent`):

| word | when | colour |
|---|---|---|
| `GATED` | the stick says the outputs stopped for low confidence (`gated`) | red |
| `HOLD` | the confidence holds (`confHold`) | grey |
| `COAST` | running, the confidence known and under the threshold, not holding | amber |
| `STOPPED` | not running (`running` false; the armed, blinking icon counts as not running yet) | grey |
| `SILENT` | running, and nothing is heard (`silent`: level under -60 dBFS or the stick says so) | grey |
| (nothing) | otherwise: running with sound and confidence | |

### The big digits (`ui/bigfont.js`: the tables ARE the fonts)

Two fonts, one face (rounded 7-segment). `BigFont.NORMAL` is for AUDIO, `BigFont.LARGE` for LINK; each has `GLYPHS`, `SMALL`, `ROWS`, `SMALL_ROWS`, `GAP`, `width(text)` and
`draw(plot, text, x, y)`.

| | NORMAL (AUDIO) | LARGE (LINK) |
|---|---|---|
| big digit `0`..`9`, `-` | 26 x 35, 5 px strokes, corners cut 2 px | 35 x 47, 7 px strokes, corners cut 3 px |
| point | 5 x 5, bottom rows 30..34 | 7 x 7, bottom rows 40..46 |
| small digit (after the point, 2/3 size, bottom-aligned) | 17 x 23, 3 px strokes, 1 px cuts | 23 x 31, 5 px strokes, 2 px cuts |
| gap between glyphs | 3 | 4 |
| `120.0` / `98.4` / `---` | 112 / 83 / 84 px | 151 / 112 / 113 px |
| rows | y 16..50 | y 16..62 |

The text is `bpm.toFixed(1)` (`120.0`, `98.4`, `999.9`): big digits, the point, then the small digit; or `---` (three big dashes) when there is no tempo. The whole text is centred:
`left = floor((160 - width) / 2)` (24 and 38 for NORMAL, 4 and 24 for LARGE; an odd width leaves the spare column on the right).

**Derivation** (all four tables, pure data in the file): each digit is made from the seven segments a-g (1 = b c, 7 = a b c, ...) as lines on a square skeleton one stroke-radius inside the
cell (x R and W - R, y R, (H - 1) / 2 and H - R). A pixel is lit when its centre is within R (half the stroke) of a line of the digit, in the metric `max(|dx|, |dy|, K (|dx| + |dy|))`;
the last term cuts the corners at the end of a line (the pixels with |dx| + |dy| > R / K).

| table | cell W x H | R | K |
|---|---|---|---|
| NORMAL big | 26 x 35 | 2.5 | 0.9 |
| NORMAL small | 17 x 23 | 1.5 | 0.9 |
| LARGE big | 35 x 47 | 3.5 | 0.9 |
| LARGE small | 23 x 31 | 2.5 | 0.9 |

The LARGE size was picked as the largest odd height (odd so that a 7 px stroke is centred and the counters are equal) whose `120.0` with the small digit fits the 156 columns between the margins:
47 x 35 digits (35 x 1.34 of 26 x 35), small digit 2/3 of it (47 x 2/3 = 31.3, 35 x 2/3 = 23.3), gap 4: 3 x 35 + 7 + 23 + 4 x 4 = 151. The dots are set by hand (an octagon: 5 x 5 with 1 px cuts; 7 x 7 with 2 px cuts).

### The VU meter

| item | rule |
|---|---|
| scale | -90..0 dBFS over 62 px |
| bar | `n = round(62 x (dB + 90) / 90)` lit columns from x 64, 5 rows high (y + 1..y + 5), continuous: no gaps, no segments |
| colour | column i has the ink id `6 + floor(32 x i / 62)` (`OLED.VU = { ink0: 6, steps: 32 }`); the id is the position on the dBFS scale, so the bar's end tells how hot the signal is |
| notch | -50 dBFS, 7 rows (y..y + 6), XOR-ed through the bar at column `round(61 x 40 / 90) = 27`; neutral |
| number | right-aligned to x 157; neutral, red when the peak reaches -1 dBFS (or, unreported, the RMS maximum -3), amber under -50 dBFS (too quiet to track) |

**Ramp**: 32 ink ids (6..37), exact RGB, `Lcd2.VU_RAMP`. They colour a bar, not text: only visibility is needed (every step is at least 6.6 : 1 on the background, #ff6058 the lowest at 6.67),
and neighbouring steps differ by at most 16 per channel, so a bar at x2 shows no banding. Linear in RGB between the anchors: steps 0..10 green (72, 222, 128), step 20 lime
(176, 230, 64), step 26 amber (255, 176, 32), step 31 red (255, 96, 88), each channel rounded:

| step | RGB | step | RGB | step | RGB | step | RGB |
|---|---|---|---|---|---|---|---|
| 0..10 | 72, 222, 128 | 11 | 82, 223, 122 | 12 | 93, 224, 115 | 13 | 103, 224, 109 |
| 14 | 114, 225, 102 | 15 | 124, 226, 96 | 16 | 134, 227, 90 | 17 | 145, 228, 83 |
| 18 | 155, 228, 77 | 19 | 166, 229, 70 | 20 | 176, 230, 64 | 21 | 189, 221, 59 |
| 22 | 202, 212, 53 | 23 | 216, 203, 48 | 24 | 229, 194, 43 | 25 | 242, 185, 37 |
| 26 | 255, 176, 32 | 27 | 255, 160, 43 | 28 | 255, 144, 54 | 29 | 255, 128, 66 |
| 30 | 255, 112, 77 | 31 | 255, 96, 88 | | | | |

### The CONFIDENCE line (as before, narrower)

One line 3 px high (rows y + 2..y + 4), no outline, no track; its **length is the value on 62 px: line = round(62 x value)** from x 64; the notch (1 px
wide, 5 tall, rows y + 1..y + 5) is XOR-ed at `64 + round(61 x threshold)`: a gap where the line passes, a stub above and below where it does not.
Value, line and notch carry the same ink id.

| value | rule |
|---|---|
| confidence | red below the stick's threshold; amber from it up to +10 points; green above; grey when the stick reports no value (`--`, no line) or there is silence |

The BAR row keeps the outlined bar (now 62 x 5, a 3 px fill inside it: round(60 x confidence), neutral, no tick). The value moved from the left of the bar to the
right of it because `CONFIDENCE` (59 px) fills the label column: `CONFIDENCE`, bar, `90%`.

### The NETWORK screen's SIGNAL row

A read-only row under the screen's rows (TYPE, GROUP, PEERS in JANK mode: y 40; TYPE and 3 more in the Wi-Fi modes: y 49; that is 13 + 9 x the number of rows), label x 4, never inverted, never selectable, no new menu item:
`SIGNAL` on the left; on the right the dBm and a word, right-aligned to x 155: `-55dBm GOOD`. The dBm is the best jank peer's (`rssi`), or the router's in Wi-Fi mode (`wifi_rssi`).
The word is coloured by the rule the bars had: `GOOD` green (>= -67 dBm), `FAIR` amber (-80..-68), `POOR` red (< -80), `NONE` grey (nothing heard, 0: no dBm shown); `-`
alone when the stick does not report it. The `NETWORK` screen's existing `STATUS` row is a connection state, not a signal, so it was not used. The word is the
only ink on the screen. The OLED layout's NETWORK screen is unchanged.

## What the stick reports for each coloured item

| element | colour rule | reported today |
|---|---|---|
| CONFIDENCE value, line, notch | the rule above | HOME `conf`, `get` threshold and lowconf: yes |
| state word | `GATED` red, `HOLD` / `STOPPED` / `SILENT` grey, `COAST` amber, empty while running | status `gated=1`, HOME hold / coast suffix: yes |
| link glyph + peers | joined (>= 1 Link peer) green; alone: grey in AUDIO mode, red in LINK mode | HOME `link` (Link peers): yes. "Searching" is not reported |
| SIGNAL row (NETWORK screen) | >= -67 dBm green GOOD, -80..-68 amber FAIR, < -80 red POOR, none grey NONE, not reported `-` | status `rssi` or `wifi_rssi`: yes. Retry / loss shares: no |
| battery (+ plug inside while charging) | neutral from 30 %, amber 15..29, red under 15 | HOME `battery` percent: yes. **Charging: not reported**, inferred as the BATTERY screen does (`usb`, cell fitted, under 100 %) |
| INPUT number | rule above | HOME `level`, status `peak_db`: yes |
| VU bar | the ramp (position on the dBFS scale) | the same |
| first beat | amber while it is the lit box | Link bar phase: yes |
| BAR row | neutral | status `bar`, `bar_lead`: yes, when AUTO BAR is on |

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

## What the C side will need later (not written)

1. `jo_draw` / `jo_draw_ink`: the 160 x 120 layout with the table `OLED.LCD2` above, the icons (`LINK_GLYPH`, `LT_ICONS_L2`, `TRI_W`, `PLUG_IN_BATTERY` in `ui/oled.js`; no signal
   bars any more), the battery with the plug inside (`charging`), the top bar slots above, the tempo with `ui/bigfont.js` (two fonts: NORMAL 12 glyphs x 35 rows of 26 or 5 bits and 10 small digits x 23 rows of 17 bits, LARGE 12 x 47 rows of 35 or 7 bits and 10 x 31 rows of 23 bits; the rule "a digit right after
   the `.` is the small one", `left = floor((160 - width) / 2)`), the CONFIDENCE line, the continuous VU meter, the two AUDIO stacks (with and without the BAR row), the LINK square boxes, the state word (`stateWordLcd2`: the order and the colours above; centred; nothing while running), and
   the NETWORK screen's `SIGNAL` row (`drawSignalRowLcd2`: word and dBm, quality thresholds as before).
2. `jank_lcd.c`: the palette gets the 32 ramp colours (`Lcd2.VU_RAMP`) and the ink ids 6..37 (`OLED.VU`); `tests/test_lcd_palette.js` then compares them too.
3. `tests/oled_ref/gen_ink.js` writes the 160 x 120 cases (it hands 128 x 64 buffers today and throws); the C replay `test_oled.c` follows. New cases: the tempo texts
   (`120.0` `146.1` `88.8` `98.4` `---`), the VU at -90 / -60 / -45 / -30 / -12 / 0 dB, CONFIDENCE at 0 / 50 / 100 %, the five state words, the battery with and without the plug and at the colour edges, the LINK tempo texts and boxes, the NETWORK screen with signal -55 / -68 / -81 / 0 / unknown.
4. Firmware wishes (unchanged): a charging flag, Wi-Fi retry / loss shares, a "joining" Link state, `rssi` / `peak_db` on the HOME line.

## Tests in this stage

`tests/test_lcd2.js` (494 checks): the palette (AA) and the ramp (32 exact RGB, each at least 6.5 : 1, neighbours within 16 per channel, the id mapping); sizes and the exact
x2; the row table (order, no lit pixel in a gap, the text of every label and value checked pixel by pixel against the 5 x 7 font parsed from `ui/oled.js`); the big and the small
digits (both tables complete, sizes, the tempo texts `120.0` `98.4` `88.8` `146.1` `---` `999.9` drawn exactly as the tables say, centred, in AUDIO and LINK, the small digit on
the baseline); the four beat boxes on every beat in both layouts; the CONFIDENCE rule and line lengths at 0 / 50 / 100 %; the VU meter (continuous columns, the id of every column, the notch);
the BAR row; the icons and the top bar without signal bars; the five state words (centred) and their order, no PLAYING; the NETWORK screen's SIGNAL row for known and unknown values; LINK layout; no ink in
menus, edit, info, setup and overlays; the OLED layout equal to the golden hashes of before the lcd2 layout (8 screens).

`tests/run.sh` **fails on purpose at its lcd2 ink regeneration step**: `node tests/oled_ref/gen_cases.js --ink` (`gen_ink.js`) hands 128 x 64 buffers to the
lcd2 layout, which refuses them, and `cases_ink.txt` and the C `jo_draw_ink` are still the older design. That is the C parity for the next round.
