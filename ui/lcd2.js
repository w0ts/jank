/* The colour face of the jank variant on the Waveshare ESP32-S3-LCD-2 (2.0 inch 240 x 320 ST7789 used in LANDSCAPE, 320 x 240, no
   touch, no RGB LEDs). DESIGN PREVIEW. The lcd2 layout of ui/oled.js has its own logical grid of 160 x 120 pixels (4:3, the same as the
   panel and the lid window): it is painted EXACTLY x2, each source pixel a square block of 2 x 2 on the 320 x 240 panel. No bands, no
   centring, no scaling table. The screen is NEUTRAL: near-white on near-black, menus and edit screens included (the selected row is the
   white inverted bar). Colour is for information only: ui/oled.js fills a second plane, `ink` (one byte per source pixel, semantic ids
   OLED.INK.*, never colours), for the pixels that carry a meaning the white version cannot show; this module maps the ids to the palette
   below and paints. docs/lcd2-colors.md says what each colour means and what the stick reports for it.

   PALETTE (exact RGB; every text colour passes WCAG AA, 4.5, against the background, tested):
     bg      (8, 10, 14)       near black
     neutral (232, 236, 244)   everything without a meaning
     good    (72, 222, 128)    green: joined, strong, high, charged
     warn    (255, 176, 32)    amber: fair, near the line, low; also the bar's first beat (the LED language's bar 1)
     bad     (255, 104, 92)    red: poor, below the line, clipping, empty
     dim     (150, 156, 168)   grey: no signal, nothing heard, not reported
   and, only while the BEAT FLASH setting is SCREEN and a beat flashes (see FLASH below), the two flash fields (FLASH_FIELD):
     beat (214, 255, 226)      white-green: the field of an ordinary beat
     bar  (255, 176, 32)       amber: the field of the bar's first beat (the LED language's bar 1)
   During a flash the field is that colour and every lit pixel is near-black (the background colour), whatever its ink. */
const Lcd2 = (() => {
  const SRC_W = 160, SRC_H = 120, SCALE = 2, W = SRC_W * SCALE, H = SRC_H * SCALE;
  const PALETTE = { bg: [8, 10, 14], neutral: [232, 236, 244], good: [72, 222, 128], warn: [255, 176, 32], bad: [255, 104, 92], dim: [150, 156, 168] };
  // the field colour of a flash (its own table: PALETTE is the one the firmware's palette is tested against, tests/test_lcd_palette.js)
  const FLASH_FIELD = { beat: [214, 255, 226], bar: [255, 176, 32] };
  // ink id (OLED.INK) -> palette key; index = id
  const INK_PALETTE = ['neutral', 'good', 'warn', 'bad', 'dim', 'warn'];
  const inkColour = id => PALETTE[INK_PALETTE[id] || 'neutral'];

  // ---- FLASH: the BEAT FLASH setting SCREEN. A port of jk_led_lit (core/jank_core.c): the same window per beat, for the screen instead of the LED.
  // A beat flashes for FLASH.beatMs (80), the bar's first beat for FLASH.barMs (150), neither longer than a beat / FLASH.capDiv (3), counted
  // from the beat's start; core/jank_core.h's JK_BEAT_FLASH_MS, JK_BAR_FLASH_MS and JK_FLASH_CAP_DIV are the source (tests/test_lcd2.js reads them).
  const FLASH = { beatMs: 80, barMs: 150, capDiv: 3 };
  const UBEAT = 1000000;                                               // micro-beats in a beat, as the C
  // the window of the beat `beatUb` (micro-beats, may be negative: before the session's beat 0) at `mpb` microseconds per beat:
  // { at: micro-beats since the beat's start, flash: its length in micro-beats, bar: it is the first beat of a bar of `quantum` }
  function flashWindow(beatUb, mpb, quantum) {
    let at = beatUb % UBEAT;
    if (at < 0) at += UBEAT;
    const index = ((beatUb - at) / UBEAT) % (quantum || 1);            // floor, then C's remainder (JS keeps the sign too)
    const bar = index === 0;
    return { at, bar, flash: Math.min(Math.floor((bar ? FLASH.barMs : FLASH.beatMs) * 1000 * UBEAT / mpb), Math.floor(UBEAT / FLASH.capDiv)) };
  }
  // jk_led_lit: 1 when the LED is lit. `rest` is the state without a flash (the C: net == JK_NET_WIFI); no session or no tempo: the rest
  function ledLit(rest, joined, beatUb, mpb, quantum) {
    if (!joined || mpb <= 0) return rest ? 1 : 0;
    const w = flashWindow(beatUb, mpb, quantum);
    return w.at < w.flash ? (rest ? 0 : 1) : (rest ? 1 : 0);
  }
  // the flash now: null (none), 'beat' or 'bar', from the beat position (beats, fractional; null: no session / not playing) and the tempo
  function flashAt(beatPos, bpm, quantum) {
    if (beatPos === null || beatPos === undefined || !(bpm > 0)) return null;
    const mpb = Math.round(60000000 / bpm), beatUb = Math.round(beatPos * UBEAT);
    if (!ledLit(false, true, beatUb, mpb, quantum)) return null;
    return flashWindow(beatUb, mpb, quantum).bar ? 'bar' : 'beat';
  }
  // the flash of a frame: only on HOME (screen 'home'), with the setting on SCREEN, never on menus, edits, setup, boot or the splash
  function flashOf(c, screen) {
    if (!c || !c.beatFlash || screen !== 'home' || c.setup || c.boot || c.splash) return null;
    return flashAt(c.beatPos, c.beatBpm, c.quantum);
  }

  // the RGB of every source pixel: unlit = background, lit = the colour of its ink id (neutral when ink is missing or 0). With a flash
  // ('beat' | 'bar') the field is the flash colour and every lit pixel is near-black.
  function pixels(fb, ink, flash) {
    const out = new Uint8Array(SRC_W * SRC_H * 3), field = flash ? FLASH_FIELD[flash] : PALETTE.bg;
    for (let i = 0; i < fb.length; i++) {
      const col = fb[i] ? (flash ? PALETTE.bg : inkColour(ink ? ink[i] : 0)) : field;
      out[3 * i] = col[0]; out[3 * i + 1] = col[1]; out[3 * i + 2] = col[2];
    }
    return out;
  }

  // paints the 320 x 240 canvas: source pixel (x, y) is the block (2x, 2y) .. (2x + 1, 2y + 1); the blocks tile the panel. img is a W x H ImageData.
  // flash: null | 'beat' | 'bar' (see FLASH)
  function paint(ctx2d, img, fb, ink, flash) {
    const px = pixels(fb, ink, flash || null), d = img.data;
    for (let sy = 0; sy < SRC_H; sy++) for (let sx = 0; sx < SRC_W; sx++) {
      const p = 3 * (sy * SRC_W + sx);
      for (let dy = 0; dy < SCALE; dy++) for (let dx = 0; dx < SCALE; dx++) {
        const o = 4 * ((sy * SCALE + dy) * W + sx * SCALE + dx);
        d[o] = px[p]; d[o + 1] = px[p + 1]; d[o + 2] = px[p + 2]; d[o + 3] = 255;
      }
    }
    ctx2d.putImageData(img, 0, 0);
  }

  // WCAG 2.x contrast ratio of two RGB colours
  function luminance(rgb) {
    const lin = v => { const s = v / 255; return s <= 0.03928 ? s / 12.92 : Math.pow((s + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(rgb[0]) + 0.7152 * lin(rgb[1]) + 0.0722 * lin(rgb[2]);
  }
  function contrast(a, b) {
    const la = luminance(a), lb = luminance(b), hi = Math.max(la, lb), lo = Math.min(la, lb);
    return (hi + 0.05) / (lo + 0.05);
  }

  return { W, H, SRC_W, SRC_H, SCALE, PALETTE, FLASH_FIELD, INK_PALETTE, FLASH, inkColour, flashWindow, ledLit, flashAt, flashOf, pixels, paint, contrast };
})();
