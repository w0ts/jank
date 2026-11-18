/* The colour face of the jank variant on the Waveshare ESP32-S3-LCD-2 (2.0 inch 240 x 320 ST7789 used in LANDSCAPE, 320 x 240, no
   touch, no RGB LEDs). DESIGN PREVIEW. The lcd2 layout of ui/oled.js is drawn at the panel's own 320 x 240 pixels (4:3, the same as the
   lid window): one frame pixel is one panel pixel. No bands, no centring, no scaling. The screen is NEUTRAL: near-white on near-black, menus and edit screens included (the selected row is the
   white inverted bar). Colour is for information only: ui/oled.js fills a second plane, `ink` (one byte per pixel, semantic ids
   OLED.INK.*, never colours), for the pixels that carry a meaning the white version cannot show; this module maps the ids to the palette
   below and paints. docs/lcd2-colors.md says what each colour means and what the stick reports for it.

   PALETTE (exact RGB; every text colour passes WCAG AA, 4.5, against the background, tested):
     bg      (8, 10, 14)       near black
     neutral (232, 236, 244)   everything without a meaning
     good    (72, 222, 128)    green: joined, strong, high, charged
     warn    (255, 176, 32)    amber: fair, near the line, low; also the bar's first beat (the LED language's bar 1)
     bad     (255, 104, 92)    red: poor, below the line, clipping, empty
     dim     (150, 156, 168)   grey: no signal, nothing heard, not reported
   VU RAMP: 32 more ink ids (OLED.VU: ids 6..37), the input meter's continuous gradient from green (quiet) through lime and amber to red (hot). They
   colour bars only (the numbers beside them use the ids above), so they need to be visible, not AA text: every step is at least 6.6 : 1
   on the background, and neighbouring steps differ by at most 16 per channel (no visible banding). */
const Lcd2 = (() => {
  const SRC_W = 320, SRC_H = 240, SCALE = 1, W = SRC_W * SCALE, H = SRC_H * SCALE;
  const PALETTE = { bg: [8, 10, 14], neutral: [232, 236, 244], good: [72, 222, 128], warn: [255, 176, 32], bad: [255, 104, 92], dim: [150, 156, 168] };
  // ink id (OLED.INK) -> palette key; index = id
  const INK_PALETTE = ['neutral', 'good', 'warn', 'bad', 'dim', 'warn'];
  const VU_RAMP = [[72, 222, 128], [72, 222, 128], [72, 222, 128], [72, 222, 128], [72, 222, 128], [72, 222, 128], [72, 222, 128], [72, 222, 128],
                   [72, 222, 128], [72, 222, 128], [72, 222, 128], [82, 223, 122], [93, 224, 115], [103, 224, 109], [114, 225, 102], [124, 226, 96],
                   [134, 227, 90], [145, 228, 83], [155, 228, 77], [166, 229, 70], [176, 230, 64], [189, 221, 59], [202, 212, 53], [216, 203, 48],
                   [229, 194, 43], [242, 185, 37], [255, 176, 32], [255, 160, 43], [255, 144, 54], [255, 128, 66], [255, 112, 77], [255, 96, 88]];
  const inkColour = id => (id >= OLED.VU.ink0 && id < OLED.VU.ink0 + VU_RAMP.length ? VU_RAMP[id - OLED.VU.ink0] : PALETTE[INK_PALETTE[id] || 'neutral']);

  // the RGB of every pixel: unlit = background, lit = the colour of its ink id (neutral when ink is missing or 0)
  function pixels(fb, ink) {
    const out = new Uint8Array(SRC_W * SRC_H * 3);
    for (let i = 0; i < fb.length; i++) {
      const col = fb[i] ? inkColour(ink ? ink[i] : 0) : PALETTE.bg;
      out[3 * i] = col[0]; out[3 * i + 1] = col[1]; out[3 * i + 2] = col[2];
    }
    return out;
  }

  // paints the 320 x 240 canvas, one frame pixel per panel pixel (no scaling). img is a W x H ImageData.
  function paint(ctx2d, img, fb, ink) {
    const px = pixels(fb, ink), d = img.data;
    for (let i = 0; i < SRC_W * SRC_H; i++) {
      d[4 * i] = px[3 * i]; d[4 * i + 1] = px[3 * i + 1]; d[4 * i + 2] = px[3 * i + 2]; d[4 * i + 3] = 255;
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

  return { W, H, SRC_W, SRC_H, SCALE, PALETTE, INK_PALETTE, VU_RAMP, inkColour, pixels, paint, contrast };
})();
