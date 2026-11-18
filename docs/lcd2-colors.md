# jank on the Waveshare ESP32-S3-LCD-2: the colour screen on its own 160 x 120 grid (design preview, v7)

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
`Stick.screen(model, now, layout, look)` makes the buffers itself and returns `{ fb, ink, w, h }`; `Stick.framebuffer(model, now)` is the OLED's
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

## v7: what changed (owner review of the live screen)

1. **Big tempo digits**: a bitmap font of its own (`ui/bigfont.js`), 3 candidate styles, default B; `?digits=a|b|c` on the page.
2. **Status icons** redrawn 11 px high and right-aligned; `GRP n` and the transport icon on the left.
3. **`BPM` label removed**; the tempo is left aligned and the area to its right stays empty.
4. **INPUT is a VU meter** with a colour gradient along the bar, a -50 dBFS notch and a peak-hold tick; smooth or segmented (`?vu=smooth|seg`).
5. **Labels in full words**: `CONFIDENCE`, `INPUT`, `BAR` + its state (the 10 character label column, x 2..60).
6. **Two stats rows at the bottom**: PEERS, signal (dot + dBm), NET, BATT, and `STOP < 70` / `MIN 70`.

Screens: `lcd2v7-states.png`, `lcd2v7-digits.png`, `lcd2v7-vu.png`, `lcd2v7-home.png`.

## HOME (AUDIO) on 160 x 120: the row table (`OLED.LCD2` holds the numbers)

| y | height | element | content and colour |
|---|---|---|---|
| 1..11 | 11 | top bar | left: transport icon x 2..10 (play triangle 9 x 11, row widths 2 3 5 6 8 9 8 6 5 3 2; stop = square 9 x 9 at y 2..10; blinks while armed), then at x 14 the fold label (`X2` `X4` `/2` `/4`: an inverted box, text + 4 wide, 11 high, corners cut) or the LINK option icon (IGNORE 11 x 11, FOLLOW and FOLLOW+LEAD 13 x 11), then `GRP n` / the SSID (text y 3, cut to x 67). Right group, fixed slots: link glyph 13 x 11 at x 70 + the peer count (x 85, max 99); signal bars at x 98 (3 bars 4 wide, pitch 5, 4 / 7 / 11 high, a missing bar is a 1 px stub); USB plug 13 x 11 at x 117; battery at x 134 (22 x 11 body, corners cut, nub 2 x 5, fill 18 x 7 = round(18 x pct)), the last lit column is x 157 |
| 15..49 | 35 | TEMPO | big digits, x 2, `120.0` is 122 px wide (style B); the widest (`999.9`) is 122..123; the area right of it is empty |
| 52..76 | 25 | beat boxes | four boxes 36 x 25 (they were 26: the stats need two lines), gap 4, margins 2; lit = solid, unlit = 2 px outline; the first is amber only while lit |
| 79..85 | 7 | `CONFIDENCE` + line + value | label x 2; one 3 px line (rows 81..83) from x 64, its length round(62 x value); notch (rows 80..84) at the stop threshold; the value right-aligned to x 157 |
| 87..93 | 7 | `INPUT` + VU meter + value | label x 2; the VU meter 62 x 5 from x 64 (rows 88..92), notch rows 87..93; the number right-aligned to x 157 |
| 95..101 | 7 | `BAR` + state + outlined bar | `BAR` at x 2, the state word right-aligned to x 59; outlined bar 62 x 5 at x 64 (rows 96..100), fill round(60 x conf); only with AUTO BAR on |
| 104..110 | 7 | stats line 1 | left `PEERS n` or the word `GATED` / `HOLD` / `COAST`; signal dot 5 x 5 at x 52 (rows 105..109) and the signal text at x 60; right-aligned to x 157 `STOP < 70` or `MIN 70` |
| 112..118 | 7 | stats line 2 | left `NET JANK G1` / `NET <Wi-Fi name>` (the whole text cut at 16 characters), right-aligned to x 157 `BATT 64%` |

Rows have gaps of 1 to 3 px (the three audio rows have a pitch of 8: a gap of 1). Row 119 stays dark. The test checks the order, that no lit pixel
lies in a gap, and that no two texts of a line touch (the widest signal text `-100dBm` ends before the widest threshold text `STOP < 100`).

**LINK layout**: the same top bar, tempo and stats rows (no `STOP` / `MIN` text, no words); the beat boxes are 36 x 48 at y 54..101; no audio rows.

### The big digits (`ui/bigfont.js`: the tables ARE the font)

| item | value |
|---|---|
| glyphs | `0`..`9`, `-` and `.` |
| height | 35 rows (y 15..49) |
| width | `0`..`9` and `-`: 26 columns each. `.`: A 5, B 6, C 7 columns, on the bottom rows (A rows 30..34, B 29..34, C 28..34) |
| spacing | 3 dark columns between two glyphs, none before the first or after the last; nothing else (no kerning) |
| table | one string per glyph, 35 rows of `#` / `.` joined by `|`; `BigFont.STYLES[style][glyph]` is the array of rows; `BigFont.width(style, text)` and `BigFont.draw(plot, style, text, x, y)` |
| text | `bpm.toFixed(1)` (`120.0`, `98.4`, `999.9`), or `---` when there is no tempo (three dashes) |
| select | `ctx.digits` = `'a'`, `'b'` or `'c'`; anything else is the default `BigFont.DEFAULT_STYLE` (`b`) |

| style | look | stroke | widths of `120.0` |
|---|---|---|---|
| A | rounded 7-segment: square skeleton, outer corners cut by 2 px, the `1` is only the right bar | 5 px | 121 px |
| B (default) | humanist rounded stroke: round caps, ovals and bowls | 4.6 px | 122 px |
| C | bold geometric: circular bowls, flat cuts at the cell edges | 5.8 px | 123 px |

All three were made from skeletons (lines, arcs, curves) rasterised at pixel centres: a pixel is lit when its centre is within half the stroke of the
skeleton, so the stroke is even everywhere and curves have no 1 px steps beyond what 1 bit allows. Pick with `lcd2v7-digits.png`.

### The VU meter

| item | rule |
|---|---|
| scale | -90..0 dBFS over 62 px (as the old line, narrower: the value moved right of the bar to make room for the full-word labels) |
| smooth | `n = round(62 x (dB + 90) / 90)` lit columns from x 64, 5 rows high (y + 1..y + 5) |
| segments | 21 segments, 2 px on and 1 px off (segment k covers columns 3k and 3k + 1, so the last one ends at column 61); `round(21 x (dB + 90) / 90)` lit |
| colour | column i (segment: its first column) has the ink id `6 + floor(16 x i / 62)` (`OLED.VU = { ink0: 6, steps: 16 }`); the id is the position on the dBFS scale, so the bar's end tells how hot the signal is |
| notch | -50 dBFS, 7 rows (y..y + 6), XOR-ed through the bar at column `round(61 x 40 / 90) = 27` (segments: the nearest gap column, 26); neutral |
| peak tick | the last second's peak (`ctx.peakDb`): 1 px wide, 5 rows, in the ramp colour of its column `round(61 x (peak + 90) / 90)`; drawn only when at least one dark column separates it from the bar's end (else it would only lengthen the bar) and a peak is reported |
| number | right-aligned to x 157; neutral, red when the peak reaches -1 dBFS (or, unreported, the RMS maximum -3), amber under -50 dBFS (too quiet to track) |
| default | segments (`VU_SEGMENTS_DEFAULT`; `ctx.vuSegments` and `?vu=smooth|seg`) |

**Ramp** (exact RGB; they colour bars, not text: only visibility is needed, every step is at least 6.6 : 1 on the background):

| step | ids | RGB | hex | contrast on bg | dBFS range |
|---|---|---|---|---|---|
| 0..7 | 6..13 | 72, 222, 128 | #48de80 | 11.35 | -90 .. -45 |
| 8 | 14 | 107, 225, 107 | #6be16b | 11.89 | -45 .. -39 |
| 9 | 15 | 141, 227, 85 | #8de355 | 12.50 | -39 .. -34 |
| 10 | 16 | 176, 230, 64 | #b0e640 | 13.43 | -34 .. -28 |
| 11 | 17 | 202, 212, 53 | #cad435 | 12.24 | -28 .. -22 |
| 12 | 18 | 229, 194, 43 | #e5c22b | 11.40 | -22 .. -17 |
| 13 | 19 | 255, 176, 32 | #ffb020 | 10.83 | -17 .. -11 |
| 14 | 20 | 255, 136, 60 | #ff883c | 8.34 | -11 .. -6 |
| 15 | 21 | 255, 96, 88 | #ff6058 | 6.67 | -6 .. 0 |

The ramp is linear in RGB between green (step 7), lime (step 10), amber (step 13) and red (step 15); the numbers on the table are what the firmware stores.

### The CONFIDENCE line (as before, narrower)

One line 3 px high (rows y + 2..y + 4), no outline, no track; its **length is the value on 62 px: line = round(62 x value)** from x 64; the notch (1 px
wide, 5 tall, rows y + 1..y + 5) is XOR-ed at `64 + round(61 x threshold)`: a gap where the line passes, a stub above and below where it does not.
Value, line and notch carry the same ink id.

| value | rule |
|---|---|
| confidence | red below the stick's threshold; amber from it up to +10 points; green above; grey when the stick reports no value (`--`, no line) or there is silence |

The BAR row keeps the outlined bar (now 62 x 5, a 3 px fill inside it: round(60 x confidence), neutral, no tick). The value moved from the left of the bar to the
right of it because `CONFIDENCE` (59 px) fills the label column: `CONFIDENCE`, bar, `90%`.

### The stats rows

| item | text and colour | reported by the stick |
|---|---|---|
| `PEERS n` | the Link peers (HOME line `link`, shown up to 99), the link glyph's colour rule (joined green; alone grey in AUDIO mode, red in LINK mode); `PEERS -` in grey before any HOME line | yes |
| `GATED` / `HOLD` / `COAST` | take the place of `PEERS n` in AUDIO mode, red / grey / amber as before | `gated`, `hold`, coast: yes |
| signal | a 5 x 5 dot (corners cut) in the signal bars' colour and the text `-55dBm`; `NONE` when nothing is heard (0), `-` when not reported (grey dot) | `rssi` (best jank peer) / `wifi_rssi` (Wi-Fi client): yes |
| `STOP < 70` / `MIN 70` | the stick's threshold; STOP when LOW CONFIDENCE stops the outputs below it, MIN when it only marks; neutral; AUDIO only | `threshold`, `lowconf`: yes |
| `NET ...` | `JANK G1` (the jank group) or the Wi-Fi network name; `-` when the name is empty; the whole `NET ...` text is cut at 16 characters | `group` (read back with `get`; shows 1 until `get` has answered), `ssid`: yes |
| `BATT 64%` | the battery colour rule; `BATT -` when no cell is reported | `battery`: yes |

The old Wi-Fi quality line (rate, retries, losses: `ctx.wifiQ`) is gone from this layout: the stick does not report it. Not reported and never invented:
charging, retries, joining.

## What the stick reports for each coloured item

| element | colour rule | reported today |
|---|---|---|
| CONFIDENCE value, line, notch | the rule above | HOME `conf`, `get` threshold and lowconf: yes |
| words `GATED` red, `HOLD` grey, `COAST` amber | outputs stopped for low confidence / holding / running under the threshold | status `gated=1`, HOME hold / coast suffix: yes |
| link glyph, PEERS | joined (>= 1 Link peer) green; alone: grey in AUDIO mode, red in LINK mode | HOME `link` (Link peers): yes. "Searching" is not reported |
| signal bars, dot | >= -67 dBm green (3 bars), -80..-68 amber (2), < -80 red (1), none or not reported grey stubs | status `rssi` or `wifi_rssi`: yes. Retry / loss shares: no |
| battery, BATT | green from 30 %, amber 15..29, red under 15 | HOME `battery` percent: yes. **Charging: not reported** |
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

## Recommendation

- Digits: style **B** (default): the even, round stroke reads as a clean display face at 2 x 2 pixel blocks; A is the most "instrument" but its `1` leaves a hole at
  the left and the 5 px strokes crowd the `8`; C is the most legible from a distance but its counters in `6 8 9` get small.
- VU meter: **segments** (default): a lit segment count reads as a meter at a glance, the gradient steps line up with the segments, and the notch fits in a gap
  column; the smooth ramp is nicer as a gradient but looks like the old line made thicker.
- The BAR row is a candidate to cut if the owner never turns AUTO BAR on (the space then stays empty).

## What the C side will need later (not written)

1. `jo_draw` / `jo_draw_ink`: the 160 x 120 layout with the table `OLED.LCD2` above, the new icons (`LINK_GLYPH`, `USB_PLUG`, `SIGNAL_DOT`,
   `LT_ICONS_L2`, `TRI_W`, `SIGNAL_H` in `ui/oled.js`), the battery, `drawTempoLcd2` with `ui/bigfont.js`'s tables (3 styles x 12 glyphs x 35 rows
   of 26 / 5..7 bits; the C default is style B, the selector can stay a constant), the lines, the VU meter (smooth and segmented: pick the
   recommended one and port only it, or both with a constant), the stats rows. The 5 x 7 font got one new glyph, `<` (`'...#.|..#..|.#...|#....|.#...|..#..|...#.'`).
2. `jank_lcd.c`: the palette gets the 16 ramp colours (`Lcd2.VU_RAMP`), the ink ids 6..21 (`OLED.VU`); `tests/test_lcd_palette.js` then compares them too.
3. `tests/oled_ref/gen_ink.js` writes the 160 x 120 cases (it hands 128 x 64 buffers today and throws); the C replay `test_oled.c` follows. New cases:
   the five tempo texts (`120.0` `146.1` `88.8` `98.4` `---`), VU at -90 / -60 / -45 / -30 / -12 / 0 dB with and without a peak (both modes), CONFIDENCE at
   0 / 50 / 100 %, every stats text (known and unknown values, long Wi-Fi name, words), the LINK option icons and the fold label.
4. Ctx fields: `rssi`, `peakDb`, `confKnown`, `peers` (null = unknown, shows `-`), `layout`, `bar`, `net.group` / `net.ssid`, and the dev switches `digits` and `vuSegments`.
5. Firmware wishes (unchanged): a charging flag, Wi-Fi retry / loss shares, a "joining" Link state, `rssi` / `peak_db` on the HOME line.

## Tests in this stage

`tests/test_lcd2.js` (444 checks): the palette (AA) and the ramp (16 exact RGB, each at least 6.5 : 1, the id mapping); sizes and the exact x2; the row table (order,
no lit pixel in a gap, the text of every label, value and stats item checked pixel by pixel against the 5 x 7 font parsed from `ui/oled.js`); the big
digits (the three tables complete, 35 rows, equal widths, the tempo texts drawn exactly as the tables say, the style switch, every width fits 156 px);
the four beat boxes on every beat in both layouts; the CONFIDENCE rule and line lengths at 0 / 50 / 100 %; the VU meter in both modes at -90 / -60 / -45 /
-30 / -12 / 0 dB (fill lengths, the id of every column, the notch, the peak tick and when it is hidden); the BAR row; the icons (play, stop, fold box, the LINK option
icons, link, signal, battery fill and shape); the stats rows (words, PEERS in the three colours, unknown values, signal text and dot colours, STOP / MIN, NET and BATT,
a long Wi-Fi name); LINK layout; no ink in menus, edit, info, setup and overlays; the OLED layout equal to the golden hashes of before the lcd2 layout (8 screens).

`tests/run.sh` **fails on purpose at its lcd2 ink regeneration step**: `node tests/oled_ref/gen_cases.js --ink` (`gen_ink.js`) hands 128 x 64 buffers to the
lcd2 layout, which refuses them, and `cases_ink.txt` and the C `jo_draw_ink` are still the older design. That is the C parity for the next round. Everything else
was run without that step and passes (see the report).
