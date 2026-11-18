# jank on the Waveshare ESP32-S3-LCD-2: the colour screen on its own 160 x 120 grid (design preview, v8)

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

## v8: what changed (owner review of the live screen)

1. **Big tempo digits**: a bitmap font of its own (`ui/bigfont.js`), the rounded 7-segment face the owner chose; one font, no switch.
2. **Status icons** redrawn 11 px high and right-aligned; `GRP n` and the transport icon on the left.
3. **`BPM` label removed**; the tempo is left aligned and the area to its right stays empty.
4. **INPUT is a continuous VU meter** with a colour gradient along the bar (no segments).
5. **Labels in full words**: `CONFIDENCE`, `INPUT`, `BAR` + its state (the 10 character label column, x 2..60).
6. **One bottom row** with only the word `GATED` / `HOLD` / `COAST` when one applies, as before; empty otherwise.

## HOME (AUDIO) on 160 x 120: the row table (`OLED.LCD2` holds the numbers)

| y | height | element | content and colour |
|---|---|---|---|
| 1..11 | 11 | top bar | left: transport icon x 2..10 (play triangle 9 x 11, row widths 2 3 5 6 8 9 8 6 5 3 2; stop = square 9 x 9 at y 2..10; blinks while armed), then at x 14 the fold label (`X2` `X4` `/2` `/4`: an inverted box, text + 4 wide, 11 high, corners cut) or the LINK option icon (IGNORE 11 x 11, FOLLOW and FOLLOW+LEAD 13 x 11), then `GRP n` / the SSID (text y 3, cut to x 67). Right group, fixed slots: link glyph 13 x 11 at x 70 + the peer count (x 85, max 99); signal bars at x 98 (3 bars 4 wide, pitch 5, 4 / 7 / 11 high, a missing bar is a 1 px stub); USB plug 13 x 11 at x 117; battery at x 134 (22 x 11 body, corners cut, nub 2 x 5, fill 18 x 7 = round(18 x pct)), the last lit column is x 157 |
| 15..49 | 35 | TEMPO | big digits, x 2, `120.0` is 121 px wide; the widest (`999.9`) is as wide; the area right of it is empty |
| 52..76 | 25 | beat boxes | four boxes 36 x 25, gap 4, margins 2; lit = solid, unlit = 2 px outline; the first is amber only while lit |
| 79..85 | 7 | `CONFIDENCE` + line + value | label x 2; one 3 px line (rows 81..83) from x 64, its length round(62 x value); notch (rows 80..84) at the stop threshold; the value right-aligned to x 157 |
| 87..93 | 7 | `INPUT` + VU meter + value | label x 2; the VU meter 62 x 5 from x 64 (rows 88..92), notch rows 87..93; the number right-aligned to x 157 |
| 95..101 | 7 | `BAR` + state + outlined bar | `BAR` at x 2, the state word right-aligned to x 59; outlined bar 62 x 5 at x 64 (rows 96..100), fill round(60 x conf); only with AUTO BAR on |
| 111..117 | 7 | the word | `GATED` (red) / `HOLD` (grey) / `COAST` (amber), left aligned at x 2, AUDIO only; empty otherwise. Rows 118 and 119 stay empty |

**LINK layout**: the same top bar and tempo; the beat boxes are 36 x 48 at y 54..101; no audio rows, no word.

### The big digits (`ui/bigfont.js`: the table IS the font)

| item | value |
|---|---|
| glyphs | `0`..`9`, `-` and `.` |
| height | 35 rows (y 15..49) |
| width | `0`..`9` and `-`: 26 columns each. `.`: 5 columns, on the bottom 5 rows (30..34) |
| spacing | 3 dark columns between two glyphs, none before the first or after the last |
| table | one string per glyph, 35 rows of `#` / `.` joined by `|`; `BigFont.GLYPHS[glyph]` is the array of rows; `BigFont.width(text)` and `BigFont.draw(plot, text, x, y)` |
| text | `bpm.toFixed(1)` (`120.0`, `98.4`, `999.9`), or `---` when there is no tempo (three dashes) |
| shape | rounded 7-segment: 5 px strokes on a square skeleton, outer corners cut by 2 px |

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

## What the stick reports for each coloured item

| element | colour rule | reported today |
|---|---|---|
| CONFIDENCE value, line, notch | the rule above | HOME `conf`, `get` threshold and lowconf: yes |
| words `GATED` red, `HOLD` grey, `COAST` amber | outputs stopped for low confidence / holding / running under the threshold | status `gated=1`, HOME hold / coast suffix: yes |
| link glyph + peers | joined (>= 1 Link peer) green; alone: grey in AUDIO mode, red in LINK mode | HOME `link` (Link peers): yes. "Searching" is not reported |
| signal bars | >= -67 dBm green (3 bars), -80..-68 amber (2), < -80 red (1), none or not reported grey stubs | status `rssi` or `wifi_rssi`: yes. Retry / loss shares: no |
| battery | green from 30 %, amber 15..29, red under 15 | HOME `battery` percent: yes. **Charging: not reported** |
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

1. `jo_draw` / `jo_draw_ink`: the 160 x 120 layout with the table `OLED.LCD2` above, the new icons (`LINK_GLYPH`, `USB_PLUG`, `LT_ICONS_L2`, `TRI_W`,
   `SIGNAL_H` in `ui/oled.js`), the battery, `drawTempoLcd2` with `ui/bigfont.js`'s table (12 glyphs x 35 rows of 26 or 5 bits), the CONFIDENCE line, the
   continuous VU meter, the word row.
2. `jank_lcd.c`: the palette gets the 32 ramp colours (`Lcd2.VU_RAMP`) and the ink ids 6..37 (`OLED.VU`); `tests/test_lcd_palette.js` then compares them too.
3. `tests/oled_ref/gen_ink.js` writes the 160 x 120 cases (it hands 128 x 64 buffers today and throws); the C replay `test_oled.c` follows. New cases:
   the tempo texts (`120.0` `146.1` `88.8` `98.4` `---`), the VU at -90 / -60 / -45 / -30 / -12 / 0 dB, CONFIDENCE at 0 / 50 / 100 %, the words, the LINK option icons and the fold label.
4. Firmware wishes (unchanged): a charging flag, Wi-Fi retry / loss shares, a "joining" Link state, `rssi` / `peak_db` on the HOME line.

## Tests in this stage

`tests/test_lcd2.js` (385 checks): the palette (AA) and the ramp (32 exact RGB, each at least 6.5 : 1, neighbours within 16 per channel, the id mapping); sizes and the exact
x2; the row table (order, no lit pixel in a gap, the text of every label and value checked pixel by pixel against the 5 x 7 font parsed from `ui/oled.js`); the big digits
(the table complete, 35 rows, equal widths, the tempo texts drawn exactly as the table says); the four beat boxes on every beat in both layouts; the CONFIDENCE rule and
line lengths at 0 / 50 / 100 %; the VU meter at -90 / -60 / -45 / -30 / -12 / 0 dB (continuous columns, the id of every column, the notch); the BAR row; the icons (play, stop,
fold box, the LINK option icons, link, signal, battery fill and shape); the bottom row (the three words, empty otherwise); LINK layout; no ink in menus, edit, info, setup and
overlays; the OLED layout equal to the golden hashes of before the lcd2 layout (8 screens).

`tests/run.sh` **fails on purpose at its lcd2 ink regeneration step**: `node tests/oled_ref/gen_cases.js --ink` (`gen_ink.js`) hands 128 x 64 buffers to the
lcd2 layout, which refuses them, and `cases_ink.txt` and the C `jo_draw_ink` are still the older design. That is the C parity for the next round.
