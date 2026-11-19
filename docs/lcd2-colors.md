# jank on the Waveshare ESP32-S3-LCD-2: the colour screen at its own 320 x 240 pixels (v14)

The variant uses a 2.0 inch 240 x 320 ST7789 colour LCD **in landscape** (320 x 240), no touch, no RGB LEDs. Until v13 the lcd2 layout had its
own 160 x 120 grid painted as 2 x 2 blocks. **From v14 (owner, 2026-11-18) it is drawn at the panel's true 320 x 240 pixels**: one frame pixel is
one panel pixel, no scaling anywhere. The small text is the old 5 x 7 face doubled and rounded (so every position of the old grid simply doubles),
the tempo digits and the icons are redrawn at 1 px detail. The lid window (43.4 x 33.2 mm) and the active area (40.8 x 30.6 mm) are 4:3 too, so a
pixel is a 0.1275 mm square. The OLED boards keep 128 x 64 and are bit-identical (`tests/oled_ref/cases.txt` is untouched).

All coordinates below are on the **320 x 240 grid**.

Colour is information only: the screen is neutral (near-white on near-black, menus and edit screens included) and a colour appears only
on an element that says something the white version cannot.

## How a caller learns the size

`OLED.size(layout)` returns `{ w, h }`: `OLED.size()` / `'oled'` = 128 x 64, `'lcd2'` = 320 x 240 (`OLED.SIZES`). `OLED.W` and `OLED.H` stay
the OLED's. `OLED.draw(fb, ui, ctx, now, ink)` keeps its signature: with `ctx.layout === 'lcd2'` `fb` (and `ink`) must be 320 * 240 bytes
(else it throws), any other layout 128 * 64. The grid is a module variable set for the duration of one `draw()` and restored after.
`Stick.screen(model, now, layout)` makes the buffers itself and returns `{ fb, ink, w, h }`; `Stick.framebuffer(model, now)` is the OLED's
128 x 64 as before. `Lcd2.paint(ctx2d, img, fb, ink)` paints a 320 x 240 frame 1:1 (`Lcd2.SRC_W` 320, `Lcd2.SCALE` 1).

## The ink plane (unchanged idea)

`ink` is a `Uint8Array(320 * 240)`; 0 = neutral. Ids (`OLED.INK`, meanings not colours): `GOOD` 1, `WARN` 2, `BAD` 3, `DIM` 4, `ONE` 5 (the
bar's first beat while lit). The ids 6..37 (`OLED.VU = { ink0: 6, steps: 32 }`, not members of `OLED.INK`) are the VU meter's ramp; see below. Only lit pixels carry ink (`draw()` clears ink under unlit pixels, e.g. where a notch is
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

## v14: what changed (owner, 2026-11-18)

1. **True 320 x 240 drawing.** The small text is "font A, smooth 10 x 14": every pixel of the 5 x 7 face doubled, the diagonal steps rounded with Scale2x (`BigFont.scale2x`),
   2 px strokes, a fixed 12 px advance (the glyph sits 1 px into its cell), 14 px rows. A narrow `.` `:` `,` advances 6, a space 8. `textW(s)` = the advances minus 1 per scale step.
   Text at "scale k" (the BOOT / splash / edit / overlay / SAVED texts) is this face magnified k times.
2. **Big tempo digits regenerated at 1 px detail** (style A, rounded): NORMAL 52 x 70, LARGE 70 x 94 (below).
3. **Frames, bars, boxes, icons at 1 px detail**: frame lines 1 px, the unlit beat boxes 2 px (half of the 4 px the old doubling would give), VU / CONFIDENCE / BAR outlines 1 px; the icons
   (play / stop, LINK TRANSPORT, battery, plug) are the old small bitmaps (the plug with Scale2x, the LINK TRANSPORT icons IGNORE and FOLLOW in plain 2 x 2 blocks, BOTH drawn natively as two separate triangles) or drawn directly; the link glyph is the Optimist's chain.
4. **The plug moved out of the battery**: a separate 18 x 14 plug glyph immediately left of the battery, only while charging. The battery stays a plain battery with its fill.
5. **The link glyph is the chain** of the Optimist's `hs_chain` (19 x 14, two rounded links with 2 px strokes, the second down and right of the first), same ink as before.

(v13: the AUDIO boxes spaced like LINK's, tight centred info rows. v12: the LINK tempo margins. v11: paddings. v10: INPUT above CONFIDENCE, the centred state word, the LARGE LINK digits.)

## The margins around the tempo (measured on lit pixels)

Above = the first lit row of the digits minus the last lit row of the top bar (y 23) minus 1; below = the first lit row of the squares minus the last lit row of the digits minus 1. The test draws
frames and measures: for every numeric text (`120.0`, `98.4`, `88.8`, `146.1`, `133.3`, `999.9`) AUDIO has 8 above (digits y 32..101) and 8 below (squares at y 110), and LINK has 14 above and 14 below (digits y 38..131,
squares at y 146). Every digit uses the first and the last row of its cell, so the digit cell extent is the lit extent. `---` lights only the dash rows in the middle of the cell: AUDIO 37 above and 38 below, LINK 53 above and
54 below.

## The top bar (y 2..23, both modes; `OLED.LCD2` holds the numbers)

Every item is a fixed slot, nothing moves when a value changes; icons are 22 rows high (y 2..23), text at y 6.

| group | item | x range | notes |
|---|---|---|---|
| left | transport icon | 4..21 | play triangle 18 x 22 (running; blinks while armed), stop square 18 x 18 (y 4..21) when the user stopped (`ctx.held`), pause = two bars 6 x 18 at x 4..9 and x 16..21 (y 4..21) when the user started but the outputs do not run (no bpm yet, low-confidence gate, silence, LOST) |
| left | LINK TRANSPORT icon | 28..49 (IGNORE, 22 wide) / 28..53 (FOLLOW, 26 wide) / 28..57 (BOTH, 30 wide) | **both modes** (`ctx.ltMode`): IGNORE = crossed circle, FOLLOW = arrow in, BOTH = two outward triangles drawn natively at 320 x 240 (each up to 11 x 22, row width max(1, round(12 * (1 - abs(y - 10.5) / 11))), left one ends at x 11 and right one starts at x 18, so 6 px (x 12..17) stay open on every row; 4 px free before the fold label at x 62). IGNORE and FOLLOW are the old 11 x 11 / 13 x 11 bitmaps in plain 2 x 2 blocks |
| left | fold label | 62..92 | AUDIO mode only, when the fold is not 1 (`X2` `X4` `/2` `/4`: an inverted 31 x 22 box with its corners cut); sits next to the LINK icon |
| right | `GRP n` (or the Wi-Fi / own network name) | right-aligned to x 187 (`GRP 1` is x 128..186; cut to 7 characters, 90 px) | in AUDIO and LINK mode, directly before the chain |
| right | chain + peer count | chain 195..213 (19 x 14, y 6..19), count from x 220 (up to 2 digits; slot 195..243) | colour: joined green, alone grey (AUDIO) / red (LINK) |
| right | plug | 246..263 (18 x 14, y 6..19) | **only while charging**; neutral (no ink of its own) |
| right | battery | 268..315 (44 x 22 body with a 1 px outline and its corners cut, nub 4 x 10 at x 312..315, fill 38 x 16 = round(38 x pct) inside) | neutral while healthy; amber under 30 % (`BATT_WARN_PCT`), red under 15 % (`BATT_BAD_PCT`) |

Gaps: 8 px between GRP n and the chain (x 187..194), 2 between the chain slot and the plug (x 244..245), 4 between the plug and the battery (x 264..267). Free width without a fold label: x 54..127 between the LINK
icon and `GRP 1`; with one x 93..127. Right edge: the nub ends at x 315 (4 px margin). The plug slot is always reserved, so nothing moves when charging starts or stops.

**Charging** is not reported by the jank; the lcd2 layout uses exactly the inference of the BATTERY info screen (`CHARGING`): `usb` and a cell fitted and under 100 %
(`charging(c)` in `ui/oled.js`); at 100 % with USB the info screen says FULL and no plug is drawn. `Stick.ctx` sets `usb` to true always, so on the page this reads as charging whenever the
battery is under 100 %; on a unit running from the cell it would be wrong until the firmware reports USB power. With no cell fitted nothing is drawn.

## HOME (AUDIO) on 320 x 240: the row table

The four beat boxes are 72 wide with 8 px gaps, spanning x 4..315, at the same x positions as LINK's boxes (x 4, 84, 164, 244); they are 56 high (`OLED.LCD2.audioBox = { w: 72, h: 56, gap: 8 }`). The info rows
(INPUT, CONFIDENCE, BAR when shown) are 2 rows apart (`rowGap`) and form one block, centred vertically between the bottom of the boxes (y 165) and the state line (y 226): the empty rows above and below the block
differ by at most 1, the odd one goes below. The BAR row exists only with AUTO BAR on (a state of `locked`, `listening` or `manual`). The state line keeps its reserved row (y 226..239) in both.

| y | height | element (with the BAR row) | empty rows before it |
|---|---|---|---|
| 2..23 | 22 | top bar | |
| 32..101 | 70 | TEMPO (NORMAL digits, centred; the digit after the dot 46 rows, y 56..101) | 8 (y 24..31) |
| 110..165 | 56 | four beat boxes 72 x 56, gap 8, x 4..315 (lit = solid, unlit = 2 px outline; the first amber only while lit) | 8 (y 102..109) |
| 173..186 | 14 | `INPUT` + VU meter (rows 175..184, notch rows 173..186) + number | 7 (y 166..172) |
| 189..202 | 14 | `CONFIDENCE` + line (rows 193..198, notch rows 191..200) + value | 2 |
| 205..218 | 14 | `BAR` + state word + outlined bar (frame rows 207..216) | 2 |
| 226..239 | 14 | state word, centred, AUDIO only (reserved, empty while running) | 7 (y 219..225) |

| y | height | element (without the BAR row) | empty rows before it |
|---|---|---|---|
| 32..101 | 70 | TEMPO | 8 |
| 110..165 | 56 | four beat boxes (the same place) | 8 |
| 181..194 | 14 | `INPUT` | 15 (y 166..180) |
| 197..210 | 14 | `CONFIDENCE` | 2 |
| 226..239 | 14 | state word | 15 (y 211..225) |

**Why 72 x 56 and not 72 x 72.** True 72 x 72 squares do not fit with the BAR row. From the tempo's end (y 101) to row 239 there are 138 rows. The squares (72), the block of three info rows (3 x 14 + 4 = 46), the
state line (14), the 8 empty rows between the tempo and the squares, and at least 7 empty rows under the squares and 7 above the state line need
8 + 72 + 7 + 46 + 7 + 14 = 154 rows: **16 rows missing**. Without the BAR row (block of 30) it would fit. The largest height that fits with the BAR row is 56, so the default is **A: boxes 72 wide x 56 high, not square**.
The alternative **B** keeps true squares: the largest side is 56 as well, spread over the full width with equal gaps (4 x 56 + 3 x 28 = 308, x 6..313). The boxes are the same size and place with and without
the BAR row. The state line is not given back to the stack while it is empty: anything that grew into it would have to jump back when a word appears. (The owner has not chosen A or B.)

**LINK layout**: the top bar above; the tempo in the LARGE digits (94 rows, y 38..131), centred; four SQUARE beat boxes 72 x 72 with 8 px gaps, a row of 312 px centred (x 4..315), at y 146..217 (22 empty
rows under them); the first amber only while lit; no audio rows, no state line. The tempo has 14 empty rows above (y 24..37) and 14 below (y 132..145).

### The state word (AUDIO only)

At most one word, centred (`left = floor((320 - width) / 2)`), the first that applies (ctx fields `gated`, `confHold`, `confKnown`, `conf`, `th`, `running`, `silent`):

| word | when | colour |
|---|---|---|
| `GATED` | the jank says the outputs stopped for low confidence (`gated`) | red |
| `HOLD` | the confidence holds (`confHold`) | grey |
| `COAST` | running, the confidence known and under the threshold, not holding | amber |
| `STOPPED` | not running (`running` false; the armed, blinking icon counts as not running yet) | grey |
| `SILENT` | running, and nothing is heard (`silent`: level under -60 dBFS or the jank says so) | grey |
| (nothing) | otherwise: running with sound and confidence | |

### The big digits (`ui/bigfont.js`)

Two fonts, one face (rounded 7-segment). `BigFont.NORMAL` is for AUDIO, `BigFont.LARGE` for LINK; each has `GLYPHS`, `SMALL`, `ROWS`, `SMALL_ROWS`, `GAP`, `width(text)` and
`draw(plot, text, x, y)`. The tables are built when the file loads (the construction below); `scripts/gen_bigfont.js` writes the same rows as const tables for the firmware.

| | NORMAL (AUDIO) | LARGE (LINK) |
|---|---|---|
| big digit `0`..`9`, `-` | 52 x 70, 10 px strokes, rounded corners | 70 x 94, 14 px strokes, rounded corners |
| point | 10 x 10, a rounded square, bottom rows 60..69 | 14 x 14, bottom rows 80..93 |
| small digit (after the point, about 2/3 size, bottom-aligned) | 34 x 46, 6 px strokes | 46 x 62, 10 px strokes |
| gap between glyphs | 6 | 8 |
| `120.0` / `98.4` / `---` | 224 / 166 / 168 px | 302 / 224 / 226 px |
| rows | y 32..101 | y 38..131 |

The text is `bpm.toFixed(1)` (`120.0`, `98.4`, `999.9`): big digits, the point, then the small digit; or `---` (three big dashes) when there is no tempo. The whole text is centred:
`left = floor((320 - width) / 2)` (48 and 77 for NORMAL, 9 and 48 for LARGE).

**Derivation**: each digit is made from the seven segments a-g (1 = b c, 7 = a b c, ...) as lines on a square skeleton one stroke-radius inside the
cell (x R and W - R, y R, (H - 1) / 2 and H - R). A pixel is lit when its centre is within R (half the stroke) of a line of the digit, in the metric `max(|dx|, |dy|, K (|dx| + |dy|))`;
the last term cuts the corners at the end of a line (the pixels with |dx| + |dy| > R / K). The v14 tables are the old ones at twice the size and twice R: the generator reproduced the old 160 x 120 LARGE tables exactly.

| table | cell W x H | R | K |
|---|---|---|---|
| NORMAL big | 52 x 70 | 5 | 0.9 |
| NORMAL small | 34 x 46 | 3 | 0.9 |
| LARGE big | 70 x 94 | 7 | 0.9 |
| LARGE small | 46 x 62 | 5 | 0.9 |

The LARGE `120.0` is 302 columns, inside the 312 between the 4 px margins. The dot is a square of the same metric (R = half its side) on the bottom rows.

### The VU meter

| item | rule |
|---|---|
| scale | -90..0 dBFS over 124 px |
| bar | `n = round(124 x (dB + 90) / 90)` lit columns from x 128, 10 rows high (y + 2..y + 11), continuous: no gaps, no segments |
| colour | column i has the ink id `6 + floor(32 x i / 124)` (`OLED.VU = { ink0: 6, steps: 32 }`); the id is the position on the dBFS scale, so the bar's end tells how hot the signal is |
| notch | -50 dBFS, 14 rows (y..y + 13), 2 px wide, XOR-ed through the bar at column `round(122 x 40 / 90) = 54` (and 55); neutral |
| number | right-aligned to x 315; neutral, red when the peak reaches -1 dBFS (or, unreported, the RMS maximum -3), amber under -50 dBFS (too quiet to track) |

**Ramp**: 32 ink ids (6..37), exact RGB, `Lcd2.VU_RAMP`. They colour a bar, not text: only visibility is needed (every step is at least 6.6 : 1 on the background, #ff6058 the lowest at 6.67),
and neighbouring steps differ by at most 16 per channel (so a bar shows no banding), except the last three, the amber to red leg, where the green channel falls 27 per step.
The colour follows how a level feels (step s starts at -90 + 2.8125 s dBFS): green through step 16 (-42 dBFS), lime ("chill") at step 23 (-25 dBFS), amber ("hot", a party level)
at step 28 (-11 dBFS), red only at the last step 31 (-3 dBFS). Linear in RGB between the anchors: steps 0..16 green (72, 222, 128), step 23 lime (176, 230, 64),
step 28 amber (255, 176, 32), step 31 red (255, 96, 88), each channel rounded:

| step | RGB | step | RGB | step | RGB | step | RGB |
|---|---|---|---|---|---|---|---|
| 0..16 | 72, 222, 128 | 17 | 87, 223, 119 | 18 | 102, 224, 110 | 19 | 117, 225, 101 |
| 20 | 131, 227, 91 | 21 | 146, 228, 82 | 22 | 161, 229, 73 | 23 | 176, 230, 64 |
| 24 | 192, 219, 58 | 25 | 208, 208, 51 | 26 | 223, 198, 45 | 27 | 239, 187, 38 |
| 28 | 255, 176, 32 | 29 | 255, 149, 51 | 30 | 255, 123, 69 | 31 | 255, 96, 88 |

### The CONFIDENCE line

One line 6 px high (rows y + 4..y + 9), no outline, no track; its **length is the value on 124 px: line = round(124 x value)** from x 128; the notch (2 px
wide, 10 tall, rows y + 2..y + 11) is XOR-ed at `128 + round(122 x threshold)`: a gap where the line passes, a stub above and below where it does not.
Value, line and notch carry the same ink id.

**With the graded confidence shown** (`confGraded`, the jank's `graded=on`, the default): the length and the value are the graded number, which has no
threshold, so the notch is **not drawn** (the line is the whole of the row). The ink (red / amber / green) and the state word's COAST keep judging the
jank's gate value (`confGate`, `status` `conf_gate`), so a good-looking line does not hide a gate that is about to stop the outputs. With `graded=off`
(or an older jank) everything is as above. The same on the mono OLED's `CF` bar (no tick).

| value | rule |
|---|---|
| confidence | red below the jank's threshold; amber from it up to +10 points; green above; grey when the jank reports no value (`--`, no line) or there is silence |

The BAR row keeps the outlined bar (124 x 10, a 1 px outline, the fill inside it: round(122 x confidence), neutral, no tick). The value is at the right end of the row
because `CONFIDENCE` (119 px, x 4..122) fills the label column: `CONFIDENCE`, bar, `90%`.

### The NETWORK screen's SIGNAL row

A read-only row under the screen's rows (TYPE, GROUP, PEERS in JANK mode: y 80; TYPE and 3 more in the Wi-Fi modes: y 98; that is 26 + 18 x the number of rows), label x 8, never inverted, never selectable, no new menu item:
`SIGNAL` on the left; on the right the dBm and a word, right-aligned to x 311: `-55dBm GOOD`. The dBm is the best jank peer's (`rssi`), or the router's in Wi-Fi mode (`wifi_rssi`).
The word is coloured by the rule the bars had: `GOOD` green (>= -67 dBm), `FAIR` amber (-80..-68), `POOR` red (< -80), `NONE` grey (nothing heard, 0: no dBm shown); `-`
alone when the jank does not report it. The `NETWORK` screen's existing `STATUS` row is a connection state, not a signal, so it was not used. The word is the
only ink on the SIGNAL row. The OLED layout's NETWORK screen is unchanged.

### The NETWORK screen's air rows (MODE JANK only)

Three more read-only rows after SIGNAL (y 98, 116, 134; the selected row's help stays at y 202 and 220), label x 8, value right-aligned to x 311. In Wi-Fi client and own-network
mode the screen is unchanged (SIGNAL only: there is no peer traffic to measure). The numbers are those of the last completed 10 s window of the peers' frames (`docs/firmware.md`, "Air statistics").

| row | value | ink |
|---|---|---|
| `LOST` | `3%`; `-` while not known | `<` 3 % green, 3..9 % amber, from 10 % red |
| `LEVEL` | `-78/-52 dBm`: the weakest and the strongest frame of the window; `-` when none | none (neutral) |
| `RX RETRY` | `2%`; `-` while not known | from 30 % red, else neutral |

`RX RETRY` is the peers' retries seen here, not this device's own (a group frame is never acknowledged, so the device cannot count its own).

## What the jank reports for each coloured item

| element | colour rule | reported today |
|---|---|---|
| CONFIDENCE value, line, notch | the rule above | HOME `conf`, `get` threshold and lowconf: yes |
| state word | `GATED` red, `HOLD` / `STOPPED` / `SILENT` grey, `COAST` amber, empty while running | status `gated=1`, HOME hold / coast suffix: yes |
| chain + peers | joined (>= 1 Link peer) green; alone: grey in AUDIO mode, red in LINK mode | HOME `link` (Link peers): yes. "Searching" is not reported |
| SIGNAL row (NETWORK screen) | >= -67 dBm green GOOD, -80..-68 amber FAIR, < -80 red POOR, none grey NONE, not reported `-` | status `rssi` or `wifi_rssi`: yes. Retry / loss shares: no (see the air rows) |
| LOST / LEVEL / RX RETRY rows (NETWORK screen, MODE JANK) | LOST green / amber from 3 % / red from 10 %; RX RETRY red from 30 %; LEVEL neutral; not known `-` | status `air_lost_pct`, `air_retry_pct` (-1: not known), `rssi_min`, `rssi_max` (0: none): yes |
| battery | neutral from 30 %, amber 15..29, red under 15 | HOME `battery` percent: yes |
| plug (left of the battery, while charging) | neutral | **Charging: not reported**, inferred as the BATTERY screen does (`usb`, cell fitted, under 100 %) |
| INPUT number | rule above | HOME `level`, status `peak_db`: yes |
| VU bar | the ramp (position on the dBFS scale) | the same |
| first beat | amber while it is the lit box | Link bar phase: yes |
| BAR row | neutral | status `bar`, `bar_lead`: yes, when AUTO BAR is on |

## The other screens on the 320 x 240 grid (neutral, the selected row inverted, no ink)

| screen | geometry |
|---|---|
| title bar | 22 rows, full width, text at x 6, y 4 |
| MENU | 11 rows of 18 px from y 26 (the 11 items of the lcd2 menu all fit: no scrolling; row k at y 26 + 18k, last at y 206; the OLED menu has 10), text at x 8, value right-aligned at x 311, selected row inverted over the full width (y - 2, 18 rows) |
| sub menu (AUDIO, OFFSET, DISPLAY) | rows of 24 px from y 36 (the selected bar 22 rows) |
| edit | the value at font scale 2 in an inverted box centred on 320 (16 px padding; scale 1 if it would not fit), the two help lines at y 140 and 164. THRESHOLD is the exception: no box and no help lines, the CONFIDENCE line (label x 4, the 6 px line on 124 px with the notch at the threshold being edited, no number on the row) at y 60, then NOW (the live confidence, `-` when unknown, inked like the HOME row judged against the edited threshold) at y 84 and THRESHOLD (the edited value, neutral) at y 108, labels at x 4, values right aligned to x 316 |
| info (BATTERY) | text rows of 24 px from y 36, the level bar 304 px wide x 14 at y 184 (1 px outline) |
| info (ABOUT) | VERSION, REPO, UPTIME, RESET, TEMP, CPU (core 0 / core 1 in percent, `57/3%`, `-` for a core not known), MEMORY (+ PSRAM), MIN HEAP, FLASH, CLOCK LATE, UI LOOP, BEAT LOOP: rows of 22 px from y 36 (the last of 9 ends at y 226), 9 shown of 12, each a label at x 8 and its value right aligned to x 312 (the list rows' margin); UP / DOWN move one page (9 rows: the pages start at row 0 and 9, so the second shows rows 9 to 12 and the rest of the screen under them stays empty, no row on two pages). The OLED shows 5 at 9 px from y 13, label at x 4, value right aligned to x 124, one page is 5 rows (pages start at row 0, 5, 10); `-` when not known; no bar, no ink |
| NETWORK / scan list | rows of 18 px like the menu (scan: 11 rows, the selected one kept near the middle, signal bars of 6 px at x 288, 296, 304, heights 4 / 8 / 12), the selected row's help at y 202 and 220 |
| text editor | 25 character cells of 12 px from x 10, the help lines at y 92, 116, 140 |
| confirm | two lines centred at y 68 and 92, `YES      NO` at scale 2 at y 168 |
| SETUP (QR) | the QR box on the left at module size 8, 6 or 4 (the largest that fits 238 rows and leaves 119 px for the text: 25 modules at 6 px = 162 px box); text right of it |
| overlays | one box 260 x 120 at (30, 60), centred, with a 1 px dark line inside (4 px in); the value at the largest scale of 4, 3, 2, 1 that fits; the NO / YES hint uses it too; SAVED: 180 x 68 at (70, 84), text scale 2 |
| BOOT / splash | `BOOT` at scale 5; `JANK` at scale 5 and `MINI 2.2` at scale 3 |

The cursor logic in `OLED.press` still scrolls as if 6 menu rows were visible (`ui.top`); the lcd2 draw ignores `ui.top` and keeps the
cursor in view by itself (all 11 menu items fit; the scan list centres on the selection), so no state is shared between the layouts.

## The C side (built, never run on the board)

- `core/jank_oled_lcd2.c` (`jo_draw_lcd2`) is a port of the lcd2 section of `ui/oled.js` with the same constants and integer arithmetic. The small text and the icons are Scale2x of the 5 x 7 face and of small bitmaps,
  made in `core/jank_oled_gfx.c` at draw time (`jo_fine`, `jo_icon2`): no table. `core/jank_bigfont.[ch]` draw the tempo from `core/jank_bigfont_tables.c` (about 22 KB of const bytes, a row of `ceil(width / 8)`
  bytes), which `scripts/gen_bigfont.js` writes from `ui/bigfont.js` (`tests/run.sh` checks the file is current).
- The frame is 320 x 240 bits (9.6 KB, page layout: byte `(y >> 3) * 320 + x`); the ink plane is one byte per pixel (320 x 240 = 76800 bytes); ids 0..37 (`JO_INK_VU0` 6, `JO_VU_STEPS` 32). `core/jank_lcd.h` holds the palette
  and the 32 ramp colours; `tests/test_lcd_palette.js` compares them with `ui/lcd2.js`.
- Parity: `tests/oled_ref/cases_ink.txt` (`node tests/oled_ref/gen_cases.js --ink`, made by `ui/oled.js` itself) holds the frame and the ink plane of every case;
  `tests/test_oled.c` reproduces both pixel for pixel and the OLED's 412 frames stay as they were.
- Firmware wishes (unchanged): a charging flag (the C uses the same inference as the BATTERY screen), Wi-Fi retry / loss shares, a "joining" Link state,
  `rssi` / `peak_db` on the HOME line.

## Tests in this stage

`tests/test_lcd2.js` (536 checks): the palette (AA) and the ramp (32 exact RGB, each at least 6.5 : 1, neighbours within 16 per channel, the id mapping); sizes and the 1:1 paint; the row table (order, no lit pixel in a gap, the text of every label and value checked pixel by pixel against the 5 x 7 font parsed from `ui/oled.js` and doubled with `BigFont.scale2x`); the big and the small
digits (both tables complete, sizes, the tempo texts `120.0` `98.4` `88.8` `146.1` `---` `999.9` drawn exactly as the tables say, centred, in AUDIO and LINK, the small digit on
the baseline); the four beat boxes on every beat in both layouts; the CONFIDENCE rule and line lengths at 0 / 50 / 100 %; the VU meter (continuous columns, the id of every column, the notch);
the BAR row; the icons, the chain and the top bar; the plug beside the battery (only while charging, the battery unchanged); the five state words (centred) and their order, no PLAYING; the NETWORK screen's SIGNAL row for known and unknown values; LINK layout; no ink in
menus, edit, info, setup and overlays; the OLED layout equal to the golden hashes of before the lcd2 layout (8 screens).
