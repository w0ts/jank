# jank on the Waveshare ESP32-S3-LCD-2: colour only where it informs (design preview, v3: numbers, no bars)

The variant uses a 2.0 inch 240 x 320 ST7789 colour LCD **in landscape** (320 x 240), no touch, no RGB LEDs. The UI is the same simple
128 x 64 1-bit UI (`ui/oled.js`), drawn at integer **x2 = 256 x 128, centred at (32, 56)**; the 56 px bands above and below stay black
(the owner chose scaling (a); `ui/lcd2.js` still has the x2.5 geometry for `?scale=2.5`, unused by default).

The first preview (whole-screen accent colour, state words in the bands) was rejected: "not color just for the sake of color". This one
is **neutral** (near-white on near-black, menus and edit screens included, the selected row the white inverted bar). A colour appears
only on an element that says something the white version cannot. Screens: `lcd2v2-home.png` (the control page, `?screen=lcd2`) and
`lcd2v2-states.png` (12 situations).

## v3 (owner, 2026-11-17: "we dont need the sliders/progress bars for confidence etc. or at least not as big. color will help")

On the lcd2 HOME there are **no bars**: the CF bar is the number `CF 87` (v2), the IN bar is now the number `IN -20` (dBFS, rounded, no unit), small
text (scale 1) at x 3, y 33, its digits coloured by the level rule (red clipping, amber too quiet, neutral otherwise). **No meter line at all:** the
number already moves with the level (a value every 250 ms), the colour carries the alarm, and a meter would need its own rules (width, colour,
min / max, a silence tick). Gone with the bar: the threshold tick, the min / max lines, the downbeat estimator's growing line under the four
boxes. Kept: the locked underline under the first box and the M (state marks, not sliders), the battery and signal icons (icons, not bars).
Rows 30..43 are empty on purpose (the tempo digits do not grow: they are scale 3 against the boxes at x 87, and a bigger size would need a new
layout rule). Menus, edit screens and every OLED layout are untouched. Layout, rows: top bar 0..8, tempo 9..29, IN 33..39, CF number 44..57, status 57..63.

## The mechanism: an ink plane

`OLED.draw(fb, ui, ctx, now, ink)` takes an optional second output, `ink`: a `Uint8Array(128 * 64)`. 0 = default neutral. The ids are
meanings, never colours (`OLED.INK`): `GOOD` 1, `WARN` 2, `BAD` 3, `DIM` 4, `ONE` 5 (the bar's first beat, only while it is the lit one). The draw functions tag the
pixels of exactly the informative elements, and only those that are lit (`tag()` in `ui/oled.js`). `ui/lcd2.js` maps id to palette and
paints the 256 x 128 picture. Two switches keep the OLED untouched:

- the ink plane is filled only with `ctx.layout === 'lcd2'` (`Stick.framebuffer(model, now, ink)` sets it when it is given a plane);
- the confidence NUMBER replaces the CF bar only with `ctx.layout === 'lcd2'`; every OLED board keeps its bar.

Without the flag the bytes are the old ones: `tests/test_lcd2.js` holds golden hashes made by the unmodified `ui/oled.js` for 8 screens
(6 HOME states, the menu, a sub menu), and `tests/oled_ref`, `tests/test_oled.c` and the 11 beat node tests pass unchanged.
Overlays (the hint box, the value box, SAVED) clear the ink under their box. Menus, sub menus, edit, scan, setup: no ink at all.

## The palette (exact RGB, WCAG AA against the background, tested)

| name | RGB | hex | contrast on bg | means |
|---|---|---|---|---|
| bg | 8, 10, 14 | #080a0e | | unlit |
| neutral | 232, 236, 244 | #e8ecf4 | 16.7 | everything without a meaning |
| good | 72, 222, 128 | #48de80 | 11.4 | joined, strong, above the line, charged |
| warn | 255, 176, 32 | #ffb020 | 10.8 | fair, near the line, low, too quiet; also the bar's first beat while it is lit |
| bad | 255, 104, 92 | #ff685c | 7.0 | poor, below the line, clipping, empty |
| dim | 150, 156, 168 | #969ca8 | 7.2 | nothing there / not reported |

The test checks each colour on the background and the background on each colour (an inverted label), and that the five are different.

## What is coloured, and what the stick reports for it today

| element | colour rule | what the stick reports today |
|---|---|---|
| **Confidence** number `CF 87` (the digits; the label `CF` stays white) | red below the stop threshold `ctx.th`, amber from the threshold up to 10 points above it, green above; grey with no value or in silence | HOME line field `conf` (percent) and the `threshold` / `lowconf` settings (`get`): reported. No HOME line: `CF --` in grey. Rounding: the shown integer is compared. |
| `STOP 70` / `MIN 70` under the right block | white text: the line the colours refer to | `threshold`, `lowconf`: reported |
| Word right of the number | `GATED` red (outputs stopped for low confidence), `HOLD` grey, `COAST` amber (running under the threshold) | `status gated=1` (needs a fresh status), HOME `h` suffix (hold / coast): reported |
| **Link icon** (the three linked dots) and the peer count `2` | joined (at least 1 Link peer) green; alone: grey in AUDIO mode, red in LINK mode | HOME line `link` field = Link peers: reported. "Searching" is not reported separately: not joined and searching look the same. |
| **Signal bars** (3 bars) | good >= -67 dBm green (3 bars), fair -80..-68 amber (2), poor < -80 red (1), none grey stubs | see below |
| **Battery** | green from 30 %, amber 15..29, red under 15 | HOME `battery` percent: reported. **Charging is not reported** (the OLED's "CHARGING" is inferred from USB present and < 100 %); no charging mark drawn. |
| **Input level** number `IN -20` (the number; the label `IN` stays white) | neutral; red when the last second's peak reaches -1 dBFS (clipping); amber under -50 dBFS (below the tracker's sound level, too quiet to track) | `status peak_db` (last second's peak) reported; if missing, the RMS maximum over -3 dB counts. Level: HOME (the min / max lines are gone with the bar). |
| **Downbeat**: first of the four beat boxes | amber **only while that box is the current beat** (owner decision 2026-11-17: a flash on the downbeat, not a static accent); neutral otherwise, and the other boxes always | Link's bar phase (HOME `bar`, `quantum`): reported. With AUTO BAR off the "1" is Link's bar 1, as the LEDs show it. |

Why red for "alone" only in LINK mode: in AUDIO mode the stick leads its own session, so having no peer is normal and red would cry wolf
(grey says "nothing there"); in LINK mode it has no tempo to follow, which is a fault.

### Signal: the control line does carry a value

`status` has `rssi=` (dBm of the best jank peer, `jk_best_rssi`, **0 = none heard**, in `core/jank_ctl.c cmd_status`) and `wifi_rssi=` (the
Wi-Fi router's, in Wi-Fi client mode). `Stick.ctx` feeds `rssi`: jank mode the peer's, Wi-Fi mode the router's (0 while not joined), `null`
when no fresh status (5 s) is there: grey stubs, never made up. Not reported: the retry and loss shares of Optimist's Wi-Fi page
(`wifiQ`), so the status line's `R3% L0%` text stays unused. The thresholds are the Wi-Fi page's own: -80 dBm is its BAD line. -67 is
a guess for "good": measure it with real sticks before keeping it.

## Proposals, not drawn

- **SILENT** (no sound on the source): the IN bar is then empty and the CF number grey; a word would add little. Left out.
- **Lost lock** is already `GATED` (red) with the red number; `HOLD` and `COAST` are the softer cases.
- **Bar locked** (`bar.state === 'locked'`): a green bar under the first box instead of white: a small green accent that says "the 1 is
  the music's". Not drawn to keep the screen calm.
- **Charging**: needs the firmware to report it (see below).

## Recommendation

Keep: the confidence number (the one change that makes the screen read at a glance), the signal bars, the battery, the link glyph.
Keep the clipping red; the quiet amber is useful but rarely seen. The first-beat amber is a flash on the downbeat only (owner decision: amber while that box is the current beat, neutral otherwise). Cut: nothing else; the bands carry nothing.

## The C side (stage 2, built, never run on the board)

- `core/jank_oled_draw.c`: `jo_draw_ink(fb, ink, ui, ctx, now)` is `jo_draw` plus the ink plane; `ctx->layout_lcd2` switches the layout
  (the CF and IN numbers, the net cluster, the lcd2 status line, no bars). `jo_draw` is `jo_draw_ink` with no plane, and the OLED boards'
  ctx never sets the flag: their bytes are what they were (`tests/test_oled.c`: the 385 frames of `cases.txt`, and 283 non-HOME frames
  drawn again with the layout on come out the same with an empty plane).
- Parity with `ui/oled.js`: `tests/oled_ref/cases_ink.txt` (`node tests/oled_ref/gen_cases.js --ink`, made by `ui/oled.js` itself) holds 88
  cases, 90 frames: the 1-bit frame and the 8192-byte ink plane of each, and `test_oled` reproduces both pixel for pixel. `tests/run.sh`
  makes the file again and wants it to be the one kept.
- `core/jank_lcd.[ch]`: the palette (`JLC_PALETTE`, `JLC_INK_MAP`; `tests/test_lcd_palette.js` compares them with `ui/lcd2.js`), RGB565 once,
  `jlc_row` expands the frame + plane to a 320-pixel row; `firmware/main/lcd.c` sends it (changed pages, pixels or ink).
- Fed in `firmware/main/uiscreen.c` from what the firmware has: `rssi` (`jk_best_rssi`, or the router's in Wi-Fi client mode; 0 = none heard),
  `peak_db` (the tracker's last-second peak, no LEVEL offset), `silent`, `gated`, `conf_hold`, the confidence, the stored threshold and
  STOP / KEEP RUNNING, `peers`, the battery percent. In LINK mode (no audio) the AUDIO layout is not shown, so `conf_unknown` there is moot.

Not done, wishes: a **charging** flag (the device infers CHARGING from USB present and under 100 %), the **Wi-Fi retry / loss** counts in jank
mode (the status line shows `R--` `L--`), a **"Link session found, joining"** state, and `rssi` / `peak_db` on the HOME line: that line is
positional and rate-limited (`jph_home_due`), the page's parser is not mine to change, and a value that moves every frame would defeat its
de-duplication, so the face keeps reading both from `status`.

Known difference, not caused by the colour layout: the C port of the OLED UI is a port of player.html's module (commit 78e5cd4's version),
`ui/oled.js` has moved on (the BLUETOOTH and USB MIDI CLOCK menu items, the fold label, the Bluetooth rune, a different top-bar name for MODE
JANK, the min / max meter lines that avoid the label). The lcd2 HOME is ported from `ui/oled.js`; the menus and edit screens stay the C
port's, so the screens other than HOME are compared for "unchanged, no ink", not against `ui/oled.js`'s frames.

## The screen (unchanged from v1)

Active area 40.8 x 30.6 mm landscape (diagonal 51.0 mm = 2.008 in); the lid window 43.4 x 33.2 mm (1.3 mm of bezel each side, a guess:
measure it) centred at (36.0, 21.5) mm on the board, above the D-pad. No LED dots on this variant.
