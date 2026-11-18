# jank on the Waveshare ESP32-S3-LCD-2: the colour screen on its own 160 x 120 grid (design preview, v9)

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

## v9: what changed (owner review of v8)

1. **The digit after the dot** is 2/3 of the size (23 x 17 instead of 35 x 26), bottom-aligned on the big digits' baseline.
2. **The tempo is centred** on the 160 columns.
3. **No signal bars in the top bar**; the signal quality is a read-only `SIGNAL` row on the NETWORK screen.
4. **The last line is a state word** (always filled in AUDIO mode) and the rows are re-spaced: bigger beat boxes, evenly spaced rows, no dead rows.

(v8: one font, no BPM label, the continuous gradient INPUT meter, full-word labels, no stats line. v7: the big digits, the 11 px icons.)

## HOME (AUDIO) on 160 x 120: the row table (`OLED.LCD2` holds the numbers)

| y | height | element | content and colour |
|---|---|---|---|
| 1..11 | 11 | top bar | left: transport icon x 2..10 (play triangle 9 x 11, row widths 2 3 5 6 8 9 8 6 5 3 2; stop = square 9 x 9 at y 2..10; blinks while armed), then at x 14 the fold label (`X2` `X4` `/2` `/4`: an inverted box, text + 4 wide, 11 high, corners cut) or the LINK option icon (IGNORE 11 x 11, FOLLOW and FOLLOW+LEAD 13 x 11), then `GRP n` / the SSID (text y 3, cut to x 67). Right group, fixed slots, packed against the right edge: link glyph 13 x 11 at x 85 + the peer count (x 100, max 99); USB plug 13 x 11 at x 117; battery at x 134 (22 x 11 body, corners cut, nub 2 x 5, fill 18 x 7 = round(18 x pct)), the last lit column is x 157 |
| 15..49 | 35 | TEMPO | big digits, centred (below); the digit after the dot is 23 rows (y 27..49) |
| 52..82 | 31 | beat boxes | four boxes 36 x 31, gap 4, margins 2; lit = solid, unlit = 2 px outline; the first is amber only while lit |
| 85..91 | 7 | `CONFIDENCE` + line + value | label x 2; one 3 px line (rows 87..89) from x 64, its length round(62 x value); notch (rows 86..90) at the stop threshold; the value right-aligned to x 157 |
| 94..100 | 7 | `INPUT` + VU meter + value | label x 2; the VU meter 62 x 5 from x 64 (rows 95..99), notch rows 94..100; the number right-aligned to x 157 |
| 103..109 | 7 | `BAR` + state + outlined bar | `BAR` at x 2, the state word right-aligned to x 59; outlined bar 62 x 5 at x 64 (rows 104..108), fill round(60 x conf); only with AUTO BAR on |
| 112..118 | 7 | state word | see below, left aligned at x 2; AUDIO only |

Gaps: 2 rows between the tempo and the boxes (50, 51), and 2 rows between each of the boxes, CONFIDENCE, INPUT, BAR and the state line (83..84, 92..93, 101..102, 110..111).
Only row 119 is empty. (v8: boxes 25 high, rows 79 / 87 / 95, word at 111, rows 118 and 119 empty.)

**LINK layout**: the same top bar and tempo; the beat boxes are 36 x 65 at y 54..118 (the space the audio rows and the state line leave free); no audio rows, no state line.
The state words below would apply to it too (the transport icon already says running or stopped); not added.

### The state word (AUDIO only)

Exactly one word, the first that applies (ctx fields `gated`, `confHold`, `confKnown`, `conf`, `th`, `running`, `silent`, all derived already):

| word | when | colour |
|---|---|---|
| `GATED` | the stick says the outputs stopped for low confidence (`gated`) | red |
| `HOLD` | the confidence holds (`confHold`) | grey |
| `COAST` | running, the confidence known and under the threshold, not holding | amber |
| `STOPPED` | not running (`running` false; the armed, blinking icon counts as not running yet) | grey |
| `SILENT` | running, and nothing is heard (`silent`: level under -60 dBFS or the stick says so) | grey |
| `PLAYING` | otherwise | neutral |

The old header's `STEADY`, `FASTER` and `SLOWER` are not derived by the stick's ctx (`mode` is always `steady`), so they are not shown.

### The big digits (`ui/bigfont.js`: the table IS the font)

| item | value |
|---|---|
| glyphs | `0`..`9`, `-` and `.` |
| height | 35 rows (y 15..49) |
| width | `0`..`9` and `-`: 26 columns each. `.`: 5 columns, on the bottom 5 rows (30..34) |
| small digit | the digit after the `.`: `BigFont.SMALL[digit]`, 23 rows x 17 columns (35 x 2/3 = 23.3, 26 x 2/3 = 17.3), its bottom on the big digits' bottom (rows 12..34 of the 35) |
| spacing | 3 dark columns between two glyphs, none before the first or after the last |
| table | one string per glyph, 35 rows (small: 23) of `#` / `.` joined by `|`; `BigFont.GLYPHS[glyph]` and `BigFont.SMALL[digit]` are the arrays of rows; `BigFont.width(text)` and `BigFont.draw(plot, text, x, y)` |
| text | `bpm.toFixed(1)` (`120.0`, `98.4`, `999.9`): big digits, the point, then the small digit; or `---` when there is no tempo (three big dashes, unchanged) |
| centring | the whole text (glyph widths + 3 px gaps: `120.0` is 3 x 26 + 5 + 17 + 4 x 3 = 112, `98.4` 83, `---` 84) at `left = floor((160 - width) / 2)`: 24, 38, 38; an odd width leaves the spare column on the right (`88.8`: 38 free columns left, 39 right); AUDIO and LINK alike |
| shape | rounded 7-segment: 5 px strokes on a square skeleton, outer corners cut by 2 px |

**The small digits** are made with the same construction as the big face, on a 17 x 23 cell: the seven segments (a b c d e f g) are lines on a square skeleton
(x 1.5 and 15.5, y 1.5, 11.5 and 21.5); a pixel is lit when its centre is within 1.5 (a 3 px stroke: 5 x 2/3 = 3.3, rounded down to keep the counters open)
of a line of the digit, in the metric `max(|dx|, |dy|, 0.9 (|dx| + |dy|))`, which cuts every outer corner by 1 px (2 x 2/3). The digit-to-segment map is the big face's
(`1` = b c, `7` = a b c, ...). They are a pure table in `ui/bigfont.js`, not computed at run time.

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
| state word | `GATED` red, `HOLD` / `STOPPED` / `SILENT` grey, `COAST` amber, `PLAYING` neutral | status `gated=1`, HOME hold / coast suffix: yes |
| link glyph + peers | joined (>= 1 Link peer) green; alone: grey in AUDIO mode, red in LINK mode | HOME `link` (Link peers): yes. "Searching" is not reported |
| SIGNAL row (NETWORK screen) | >= -67 dBm green GOOD, -80..-68 amber FAIR, < -80 red POOR, none grey NONE, not reported `-` | status `rssi` or `wifi_rssi`: yes. Retry / loss shares: no |
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

1. `jo_draw` / `jo_draw_ink`: the 160 x 120 layout with the table `OLED.LCD2` above, the icons (`LINK_GLYPH`, `USB_PLUG`, `LT_ICONS_L2`, `TRI_W` in `ui/oled.js`; no signal
   bars any more), the battery, the tempo with `ui/bigfont.js` (12 big glyphs x 35 rows of 26 or 5 bits, 10 small digits x 23 rows of 17 bits, the rule "a digit right after
   the `.` is the small one", `left = floor((160 - width) / 2)`), the CONFIDENCE line, the continuous VU meter, the state word (`stateWordLcd2`: the order and the colours above), and
   the NETWORK screen's `SIGNAL` row (`drawSignalRowLcd2`: word and dBm, quality thresholds as before).
2. `jank_lcd.c`: the palette gets the 32 ramp colours (`Lcd2.VU_RAMP`) and the ink ids 6..37 (`OLED.VU`); `tests/test_lcd_palette.js` then compares them too.
3. `tests/oled_ref/gen_ink.js` writes the 160 x 120 cases (it hands 128 x 64 buffers today and throws); the C replay `test_oled.c` follows. New cases: the tempo texts
   (`120.0` `146.1` `88.8` `98.4` `---`), the VU at -90 / -60 / -45 / -30 / -12 / 0 dB, CONFIDENCE at 0 / 50 / 100 %, the six state words, the NETWORK screen with signal -55 / -68 / -81 / 0 / unknown.
4. Firmware wishes (unchanged): a charging flag, Wi-Fi retry / loss shares, a "joining" Link state, `rssi` / `peak_db` on the HOME line.

## Tests in this stage

`tests/test_lcd2.js` (423 checks): the palette (AA) and the ramp (32 exact RGB, each at least 6.5 : 1, neighbours within 16 per channel, the id mapping); sizes and the exact
x2; the row table (order, no lit pixel in a gap, the text of every label and value checked pixel by pixel against the 5 x 7 font parsed from `ui/oled.js`); the big and the small
digits (both tables complete, sizes, the tempo texts `120.0` `98.4` `88.8` `146.1` `---` `999.9` drawn exactly as the tables say, centred, in AUDIO and LINK, the small digit on
the baseline); the four beat boxes on every beat in both layouts; the CONFIDENCE rule and line lengths at 0 / 50 / 100 %; the VU meter (continuous columns, the id of every column, the notch);
the BAR row; the icons and the top bar without signal bars; the six state words and their order; the NETWORK screen's SIGNAL row for known and unknown values; LINK layout; no ink in
menus, edit, info, setup and overlays; the OLED layout equal to the golden hashes of before the lcd2 layout (8 screens).

`tests/run.sh` **fails on purpose at its lcd2 ink regeneration step**: `node tests/oled_ref/gen_cases.js --ink` (`gen_ink.js`) hands 128 x 64 buffers to the
lcd2 layout, which refuses them, and `cases_ink.txt` and the C `jo_draw_ink` are still the older design. That is the C parity for the next round.
