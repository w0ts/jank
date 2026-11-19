/* The device drawing of the control page: jank-mini-3 seen from the top (the lid), as the printed box looks (hardware/jank-mini-3/case/BOX.md,
   box.scad). Every number lives in the BOX3 table below, in mm, case coordinates (origin top left, x right, y down); Box3.build(svg) draws the
   shell of ui/box3.svg.html from it and nothing else holds a position. The case origin lies 2.4 mm (fit 0.4 + wall 2.0) outside the board's
   corner, so a board coordinate of box.scad is b(x) here; the tables below say b(...) wherever the number comes from box.scad. Positions may
   still move by +-2 mm: change them here. The face labels are the LABELS and ARROWS tables, the D-pad cap's symbols are drawn polygons.
   Builds with the DOM only. */
const BOX3 = (() => {
  const b = (v) => Math.round((v + 2.4) * 100) / 100;          // box.scad board mm -> case mm
  const W = 95.5, H = 82.0;                                       // the board (box.scad W, H)
  const J = { j1: 31.95, j4: 44.95, j2: 6.3, j3: 19.1, j5: W - 33.0, j6: W - 10.9, j13: H / 2 - 10.0, j14: H / 2 + 10.0, j7: 30.0, lcd2Usb: 41.0 };
  const rows = [b(2.4), b(5.2)];                                  // the top edge's rows: triangles, names (box.scad LAB_ROWS)
  return {
    case: { w: 100.3, h: 86.8, r: 3 },
    // the Waveshare LCD-2 in landscape: the active area, and the lid window 0.5 mm bigger on each side; the 320 x 240 canvas fills the active area
    screen: { x: b(5.4), y: b(25.7), w: 40.8, h: 30.6, windowMargin: 0.5 },
    // the D-pad: one cross-shaped cap, flush with the lid; its four arms are the four buttons, the switches sit under the arm ends
    cross: { cx: b(64.75), cy: b(41.0), span: 24, arm: 8 },
    switches: { up: [b(64.75), b(33.0)], down: [b(64.75), b(49.0)], no: [b(56.75), b(41.0)], yes: [b(72.75), b(41.0)] },
    // the symbols on the cap (box.scad sym_*): centred symR out on the arms, a solid triangle up and down, an X on NO, a tick on YES
    symbols: { symR: 8.7, tri: { w: 5.0, h: 4.0 }, x: { size: 4.6, bold: 1.7 }, tick: { bold: 1.7, points: [[-2.0, 0.1], [-0.6, -1.3], [1.7, 1.5]], centre: [0.12, 0.26] } },
    // the face labels as printed: [x, y, text, turn in degrees (counter-clockwise)] (box.scad LABELS); the font is in ui/box3.css
    labelSize: 2.2, labelSpacing: 0.12,
    labels: [
      [b(J.j2), rows[1], '1'], [b((J.j2 + J.j3) / 2), rows[1], 'MIDI'], [b(J.j3), rows[1], '2'],
      [b(J.j1), rows[1], 'ESP'], [b(J.j4), rows[1], 'DEVICE'],
      [b(J.j5), rows[1], '1'], [b((J.j5 + J.j6) / 2), rows[1], 'MIDI'], [b(J.j6), rows[1], '2'],
      [b(J.j7), b(H - 5.0), 'LINE IN'], [b(W - 7.0), b(J.j13), 'CLK'], [b(W - 7.0), b(J.j14), 'RST'],
      [b(0.4), b(J.lcd2Usb + 5.6), 'ESP', 90],
    ],
    // the triangles that point at the sockets: [x, y, direction in degrees: 0 up, -90 right, 180 down, 90 left] (box.scad LAB_ARROWS); size w x h
    arrows: [
      ...[J.j2, J.j3, J.j1, J.j4, J.j5, J.j6].map((x) => [b(x), rows[0], 0]),
      [b(W - 2.2), b(J.j13), -90], [b(W - 2.2), b(J.j14), -90], [b(J.j7), b(H - 1.8), 180], [b(0.4), b(J.lcd2Usb), 90],
    ],
    arrowSize: { w: 2.6, h: 2.1 },
    // each port: the wall it is on and its centre along that wall (x for top and bottom, y for left and right), mm
    ports: [
      { id: 'midi1', wall: 'top', at: b(J.j2), kind: 'trs' }, { id: 'midi2', wall: 'top', at: b(J.j3), kind: 'trs' },
      { id: 'din1', wall: 'top', at: b(J.j5), kind: 'din' }, { id: 'din2', wall: 'top', at: b(J.j6), kind: 'din' },
      { id: 'clock', wall: 'right', at: b(J.j13), kind: 'trs' }, { id: 'reset', wall: 'right', at: b(J.j14), kind: 'trs' },
      { id: 'linein', wall: 'bottom', at: b(J.j7), kind: 'trs' },
      { id: 'usbIn', wall: 'top', at: b(J.j1), kind: 'usbc' }, { id: 'usbHost', wall: 'top', at: b(J.j4), kind: 'usbc' }, { id: 'usbOwn', wall: 'left', at: b(J.lcd2Usb), kind: 'usbc' },
    ],
    // connector sizes: length along the wall, depth out of the wall (3.5 mm jack ~6, 5-pin DIN ~15, USB-C 9 x 3.2)
    kinds: { trs: { len: 6, depth: 3.2 }, din: { len: 15, depth: 3.2 }, usbc: { len: 9, depth: 3.2 } },
    margin: 1.5,
  };
})();

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
  // a button's click area: the arm's end square (arm x arm mm) centred on its switch; the cross' centre square is no button
  function armRect(name) {
    const a = BOX3.cross.arm, [x, y] = BOX3.switches[name];
    return { x: x - a / 2, y: y - a / 2, w: a, h: a };
  }
  // the cap's outline: a plus, twelve corners
  function crossPoints() {
    const { cx, cy, span, arm } = BOX3.cross, h = span / 2, a = arm / 2;
    return [[cx - a, cy - h], [cx + a, cy - h], [cx + a, cy - a], [cx + h, cy - a], [cx + h, cy + a], [cx + a, cy + a], [cx + a, cy + h], [cx - a, cy + h], [cx - a, cy + a], [cx - h, cy + a], [cx - h, cy - a], [cx - a, cy - a]];
  }
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

  // a symbol's points as an SVG points string, about the centre (x, y)
  const pts = (list, x, y) => list.map(([px, py]) => mm(x + px) + ',' + mm(y + py)).join(' ');
  // the cap's symbol on an arm, centred symR out from the cap's centre along its arm, as box.scad draws them (y down here)
  function symbolFor(svg, name, x, y) {
    const S = BOX3.symbols, cls = { class: 'sym' };
    if (name === 'up' || name === 'down') {
      const s = name === 'up' ? 1 : -1, hw = S.tri.w / 2, hh = S.tri.h / 2;
      return [make(svg, 'polygon', { ...cls, id: 'sym-' + name, points: pts([[-hw, s * hh], [hw, s * hh], [0, -s * hh]], x, y) })];
    }
    if (name === 'no') {
      // two bars of width bold, cut flush by the size x size square: two hexagons
      const h = S.x.size / 2, d = S.x.bold / 2 * Math.SQRT2;
      return [[[-h, -h], [-h + d, -h], [h, h - d], [h, h], [h - d, h], [-h, -h + d]], [[-h, h], [-h, h - d], [h - d, -h], [h, -h], [h, -h + d], [-h + d, h]]]
        .map((poly, i) => make(svg, 'polygon', { ...cls, id: 'sym-no' + i, points: pts(poly, x, y) }));
    }
    const T = S.tick, [cx, cy] = T.centre;
    return [make(svg, 'polyline', { class: 'sym tick', id: 'sym-yes', 'stroke-width': T.bold, points: pts(T.points.map(([px, py]) => [px - cx, -(py - cy)]), x, y) })];
  }
  // the printed texts and the pointing triangles of the face
  function labelEls(svg) {
    const z = BOX3.labelSize, { w, h } = BOX3.arrowSize;
    const texts = BOX3.labels.map(([x, y, t, turn = 0]) => {
      const e = make(svg, 'text', { x: mm(x), y: mm(y + 0.36 * z), 'font-size': z, 'letter-spacing': BOX3.labelSpacing, class: 'label', 'data-label': t });
      if (turn) e.setAttribute('transform', 'rotate(' + -turn + ' ' + mm(x) + ' ' + mm(y) + ')');
      e.textContent = t;
      return e;
    });
    const tris = BOX3.arrows.map(([x, y, dir]) => make(svg, 'polygon', {
      class: 'label-arrow', transform: 'translate(' + mm(x) + ' ' + mm(y) + ') rotate(' + -dir + ')', points: [[-w / 2, h / 2], [w / 2, h / 2], [0, -h / 2]].map((q) => q.join(',')).join(' '),
    }));
    return [...texts, ...tris];
  }
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
    svg.querySelector('#labels').replaceChildren(...labelEls(svg));
    const cap = make(svg, 'path', { id: 'cap', d: 'M' + crossPoints().map(([x, y]) => mm(x) + ' ' + mm(y)).join('L') + 'Z', class: 'cap' });
    svg.querySelector('#dpad').replaceChildren(cap, ...['up', 'down', 'no', 'yes'].map((name) => {
      const [x, y] = BOX3.switches[name], r = armRect(name), g = make(svg, 'g', { id: 'arm-' + name, 'data-b': name });
      const dir = { up: [0, -1], down: [0, 1], no: [-1, 0], yes: [1, 0] }[name], C = BOX3.cross, R = BOX3.symbols.symR;
      g.append(...symbolFor(svg, name, C.cx + dir[0] * R, C.cy + dir[1] * R));
      g.prepend(make(svg, 'rect', { x: mm(r.x), y: mm(r.y), width: r.w, height: r.h, rx: 1.5, class: 'armarea' }));
      return g;
    }));
  }
  return { build, placeScreen, windowRect, portRect, viewBox, canvasArea, armRect, crossPoints };
})();
