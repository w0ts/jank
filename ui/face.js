/* The device face shared by the simulator (beat/live/player.html) and the control page (page/jank.html): drawing the
   128 x 64 framebuffer of ui/oled.js on the OLED canvas, placing the screen module in the box drawing (ui/box.svg.html),
   and the beat LEDs' holes and glow. Everything takes the box's <svg id="dev"> element; no global state. */
const Face = (() => {
  const OLED_ON = [226, 236, 255], OLED_OFF = [4, 6, 10];
  // each module: active area (aw x ah) and the lid window (ww x wh), both centred on (cx, cy); board mm, y down
  const SCREENS = {
    '1.54': { aw: 35.05, ah: 17.52, cx: 38.0, cy: 30.3, ww: 37.8, wh: 19.7 },
    '2.42': { aw: 55.01, ah: 27.49, cx: 36.0, cy: 19.9, ww: 57.5, wh: 29.5 },
    // the colour variant (ui/lcd2.js): the Waveshare ESP32-S3-LCD-2, a 2.0 inch 240 x 320 ST7789 in landscape (320 x 240): its active area is
    // 30.6 x 40.8 mm (the 2.0 inch diagonal of a 3:4 panel: sqrt(40.8^2 + 30.6^2) = 51.0 mm = 2.008 in), the lid window 1.3 mm wider each side.
    // No RGB LEDs on this variant (leds: false); the D-pad is where it always is, below the window.
    lcd2: { aw: 40.8, ah: 30.6, cx: 36.0, cy: 21.5, ww: 43.4, wh: 33.2, leds: false },
  };
  // the 4 beat LEDs (board mm): a small hole in the lid over each, and the glow it throws on the lid
  const LED_X = [3.0, 10.5, 18.0, 25.5], LED_Y = 54.6, LED_HOLE_R = 1.2, LED_GLOW_R = 3.2, LED_DARK = '#1c1e20';

  const litColour = brightPct => OLED_ON.map((v, j) => Math.round(OLED_OFF[j] + (v - OLED_OFF[j]) * brightPct / 100));

  function paintFramebuffer(ctx2d, img, fb, brightPct) {
    const d = img.data, on = litColour(brightPct);
    for (let i = 0; i < fb.length; i++) {
      const col = fb[i] ? on : OLED_OFF;
      d[4 * i] = col[0]; d[4 * i + 1] = col[1]; d[4 * i + 2] = col[2]; d[4 * i + 3] = 255;
    }
    ctx2d.putImageData(img, 0, 0);
  }

  function setAttrs(el, attrs) { for (const key in attrs) el.setAttribute(key, attrs[key]); }

  function placeScreen(svg, name) {
    const m = SCREENS[name];
    setAttrs(svg.querySelector('#oledBezel'), { x: m.cx - m.ww / 2, y: m.cy - m.wh / 2, width: m.ww, height: m.wh });
    setAttrs(svg.querySelector('#oledFO'), { x: m.cx - m.aw / 2, y: m.cy - m.ah / 2, width: m.aw, height: m.ah });
  }

  function makeLeds(svg) {
    const mk = (tag, attrs) => { const e = document.createElementNS(svg.namespaceURI, tag); setAttrs(e, attrs); return e; };
    LED_X.forEach((x, i) => {
      svg.querySelector('#ledGlows').append(mk('circle', { id: 'ledGlow' + i, cx: x, cy: LED_Y, r: LED_GLOW_R, filter: 'url(#seam)', opacity: 0, fill: '#000' }));
      svg.querySelector('#ledHoles').append(mk('circle', { id: 'led' + i, cx: x, cy: LED_Y, r: LED_HOLE_R, fill: LED_DARK, stroke: '#34373b', 'stroke-width': '.3' }));
    });
  }

  // css: the colour LED i shows ('rgb(r,g,b)'), or null for dark
  function paintLed(svg, i, css) {
    setAttrs(svg.querySelector('#led' + i), { fill: css || LED_DARK });
    setAttrs(svg.querySelector('#ledGlow' + i), { fill: css || '#000', opacity: css ? '0.55' : '0' });
  }

  // the LED dots of the box drawing are shown for the screens that have LEDs only
  function showLeds(svg, name) {
    const on = SCREENS[name].leds !== false;
    for (const id of ['#ledView', '#seamGlow']) { const el = svg.querySelector(id); if (on) el.removeAttribute('hidden'); else el.setAttribute('hidden', ''); }
  }

  return { SCREENS, LED_X, paintFramebuffer, placeScreen, showLeds, makeLeds, paintLed };
})();
