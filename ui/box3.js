/* The device drawing of the control page: jank-mini-3 seen from the top (the lid). Every number lives in the BOX3 table below, in mm, case
   coordinates (origin top left, x right, y down); Box3.build(svg) draws the shell of ui/box3.svg.html from it and nothing else holds a
   position. Positions may still move by +-2 mm: change them here. No text on the device but the keys' own marks. Builds with the DOM only. */
const BOX3 = {
  case: { w: 100, h: 79, r: 3 },
  // the Waveshare LCD-2 in landscape: the active area, and the lid window one mm bigger on each side; the 320 x 240 canvas fills the active area
  screen: { x: 9.7, y: 24.2, w: 40.8, h: 30.6, windowMargin: 1 },
  keys: { d: 6, up: [67.0, 30.5], down: [67.0, 48.5], no: [58.0, 39.5], yes: [76.0, 39.5] },
  keyMarks: { up: '▲', down: '▼', no: 'NO', yes: 'YES' },
  // each port: the wall it is on and its centre along that wall (x for top and bottom, y for left and right), mm
  ports: [
    { id: 'midi1', wall: 'top', at: 33.3, kind: 'trs' }, { id: 'midi2', wall: 'top', at: 46.1, kind: 'trs' },
    { id: 'din1', wall: 'top', at: 63.5, kind: 'din' }, { id: 'din2', wall: 'top', at: 85.6, kind: 'din' },
    { id: 'clock', wall: 'right', at: 29.5, kind: 'trs' }, { id: 'reset', wall: 'right', at: 49.5, kind: 'trs' },
    { id: 'linein', wall: 'bottom', at: 73.5, kind: 'trs' },
    { id: 'usbHost', wall: 'left', at: 9.6, kind: 'usbc' }, { id: 'usbOwn', wall: 'left', at: 39.5, kind: 'usbc' }, { id: 'usbIn', wall: 'left', at: 68.5, kind: 'usbc' },
  ],
  // connector sizes: length along the wall, depth out of the wall (3.5 mm jack ~6, 5-pin DIN ~15, USB-C 9 x 3.2)
  kinds: { trs: { len: 6, depth: 3.2 }, din: { len: 15, depth: 3.2 }, usbc: { len: 9, depth: 3.2 } },
  margin: 1.5,
};

const Box3 = (() => {
  const windowRect = (s = BOX3.screen) => ({ x: s.x - s.windowMargin, y: s.y - s.windowMargin, w: s.w + 2 * s.windowMargin, h: s.h + 2 * s.windowMargin });
  // a port's opening as a rectangle outside its wall (the stub the drawing shows)
  function portRect(p) {
    const k = BOX3.kinds[p.kind], c = BOX3.case, horizontal = p.wall === 'top' || p.wall === 'bottom';
    const w = horizontal ? k.len : k.depth, h = horizontal ? k.depth : k.len;
    const x = p.wall === 'left' ? -k.depth : p.wall === 'right' ? c.w : p.at - w / 2;
    const y = p.wall === 'top' ? -k.depth : p.wall === 'bottom' ? c.h : p.at - h / 2;
    return { x, y, w, h };
  }
  const keyCentre = (name) => BOX3.keys[name];
  // the view box: the case and the port stubs, with a small margin
  function viewBox() {
    const rects = BOX3.ports.map(portRect);
    const x0 = Math.min(0, ...rects.map((r) => r.x)) - BOX3.margin, y0 = Math.min(0, ...rects.map((r) => r.y)) - BOX3.margin;
    const x1 = Math.max(BOX3.case.w, ...rects.map((r) => r.x + r.w)) + BOX3.margin, y1 = Math.max(BOX3.case.h, ...rects.map((r) => r.y + r.h)) + BOX3.margin;
    return { x: x0, y: y0, w: x1 - x0, h: y1 - y0 };
  }
  // the screen's canvas area: the whole active area, or (the black and white preview) its largest centred part with the canvas' proportions
  function canvasArea(aspect) {
    const s = BOX3.screen;
    if (!aspect || Math.abs(aspect - s.w / s.h) < 1e-9) return { x: s.x, y: s.y, w: s.w, h: s.h };
    const h = Math.min(s.h, s.w / aspect), w = h * aspect;
    return { x: s.x + (s.w - w) / 2, y: s.y + (s.h - h) / 2, w, h };
  }
  const setAttrs = (el, attrs) => { for (const key in attrs) el.setAttribute(key, String(attrs[key])); };
  const make = (svg, tag, attrs) => { const e = document.createElementNS(svg.namespaceURI, tag); setAttrs(e, attrs); return e; };
  const mm = (v) => Math.round(v * 100) / 100;

  function placeScreen(svg, aspect) {
    const win = windowRect(), area = canvasArea(aspect);
    setAttrs(svg.querySelector('#oledBezel'), { x: mm(win.x), y: mm(win.y), width: mm(win.w), height: mm(win.h) });
    setAttrs(svg.querySelector('#oledFO'), { x: mm(area.x), y: mm(area.y), width: mm(area.w), height: mm(area.h) });
  }
  function build(svg) {
    const c = BOX3.case, v = viewBox();
    setAttrs(svg, { viewBox: [v.x, v.y, v.w, v.h].map(mm).join(' ') });
    setAttrs(svg.querySelector('#lid'), { x: 0, y: 0, width: c.w, height: c.h, rx: c.r });
    setAttrs(svg.querySelector('#lidshade'), { x: 1, y: 1, width: c.w - 2, height: c.h - 2, rx: Math.max(0, c.r - 1) });
    svg.querySelector('#ports').replaceChildren(...BOX3.ports.map((p) => {
      const r = portRect(p);
      return make(svg, 'rect', { id: 'port-' + p.id, x: mm(r.x), y: mm(r.y), width: r.w, height: r.h, rx: 1, class: 'port' });
    }));
    svg.querySelector('#dpad').replaceChildren(...['up', 'down', 'no', 'yes'].map((name) => {
      const [x, y] = keyCentre(name), g = make(svg, 'g', { id: 'arm-' + name, 'data-b': name });
      const mark = make(svg, 'text', { x, y: y + (name === 'no' || name === 'yes' ? 0.9 : 1.1), class: name === 'no' || name === 'yes' ? 'glyph s' : 'glyph' });
      mark.textContent = BOX3.keyMarks[name];
      g.append(make(svg, 'circle', { cx: x, cy: y, r: BOX3.keys.d / 2, class: 'btnarm' }), mark);
      return g;
    }));
  }
  return { build, placeScreen, windowRect, portRect, viewBox, canvasArea };
})();
