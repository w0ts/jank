// SPDX-License-Identifier: MIT
// The device drawing's geometry (ui/box3.js): read from its one table, no browser.
'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const src = fs.readFileSync(path.join(__dirname, '..', 'ui', 'box3.js'), 'utf8');
const { BOX3, Box3 } = new Function(src + '; return { BOX3, Box3 };')();
let checks = 0;
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };
const ok = (c, what) => { checks++; assert.ok(c, what); };
const near = (a, b, what) => { checks++; assert.ok(Math.abs(a - b) < 1e-9, what + ': ' + a + ' vs ' + b); };

eq([BOX3.case.w, BOX3.case.h, BOX3.case.r], [100.3, 86.8, 3], 'the case is the box outline 100.3 x 86.8, corner radius 3');
const s = BOX3.screen, win = Box3.windowRect();
near(s.w / s.h, 4 / 3, 'the active area is 4:3 like the 320 x 240 canvas');
eq([s.x, s.x + s.w, s.y, s.y + s.h].map((v) => Math.round(v * 10) / 10), [7.8, 48.6, 28.1, 58.7], 'the active area: x 7.8..48.6, y 28.1..58.7 (box.scad: board x 5.4..46.2, y 25.7..56.3, plus 2.4)');
eq([win.x, win.x + win.w, win.y, win.y + win.h].map((v) => Math.round(v * 10) / 10), [7.3, 49.1, 27.6, 59.2], 'the lid window is the active area plus 0.5 mm each side');
const a = Box3.canvasArea(null), half = Box3.canvasArea(128 / 64);
eq([a.x, a.y, a.w, a.h], [s.x, s.y, s.w, s.h], 'the colour canvas fills the active area exactly');
near(half.w / half.h, 2, 'the black and white preview keeps 2:1');
ok(half.x >= s.x && half.y >= s.y && half.x + half.w <= s.x + s.w + 1e-9 && half.y + half.h <= s.y + s.h + 1e-9, '... inside the active area');
ok(win.x > 0 && win.y > 0 && win.x + win.w < BOX3.case.w && win.y + win.h < BOX3.case.h, 'the window is inside the case');

// the printed box (box.scad): the labels as printed, the cap's symbols, the colours
eq(BOX3.labels.map((l) => l[2]), ['1', 'MIDI', '2', 'ESP', 'DEVICE', '1', 'MIDI', '2', 'LINE IN', 'CLK', 'RST', 'ESP'], 'the printed texts: 1 MIDI 2, ESP, DEVICE, 1 MIDI 2, LINE IN, CLK, RST, ESP (left wall)');
eq(BOX3.labels.map((l) => l[0] + ',' + l[1] + ',' + (l[3] || 0)).slice(0, 5), ['8.7,7.6,0', '15.1,7.6,0', '21.5,7.6,0', '34.35,7.6,0', '47.35,7.6,0'], 'the first row of names: 1 MIDI 2 over the TRS jacks, ESP and DEVICE over the USB-C');
eq(BOX3.labels[11].slice(0, 2).concat(BOX3.labels[11][3]), [2.8, 49, 90], 'the left wall ESP runs up the edge (turned 90)');
eq(BOX3.arrows.map((a) => a[2]), [0, 0, 0, 0, 0, 0, -90, -90, 180, 90], 'ten pointing triangles: up on the top edge, right at CLK and RST, down at LINE IN, left at the left ESP');
eq([BOX3.arrowSize.w, BOX3.arrowSize.h, BOX3.labelSize], [2.6, 2.1, 2.2], 'triangle 2.6 x 2.1, text size 2.2');
const css = fs.readFileSync(path.join(__dirname, '..', 'ui', 'box3.css'), 'utf8');
ok(/\.lid \{ fill:#ffffff;/.test(css), 'the body is white');
ok(/\.label \{ fill:#A06CF0;/.test(css) && /\.sym \{ fill:#A06CF0;/.test(css) && /\.label-arrow \{ fill:#A06CF0;/.test(css), 'labels and symbols are purple #A06CF0');
ok(/\.cap \{ fill:#5f5f5f;/.test(css), 'the D-pad cap is grey');
ok(/Avenir Next Condensed[^;]*sans-serif; font-weight:800/.test(css), 'the labels use the condensed heavy stack');
const keys = ['up', 'down', 'no', 'yes'], X = BOX3.cross, hh = X.span / 2, aa = X.arm / 2;
eq([X.cx, X.cy, X.span, X.arm], [67.15, 43.4, 24, 8], 'the D-pad: a cross centred at (67.15, 43.4), 24 mm across, arms 8 mm wide');
eq(keys.map((k) => BOX3.switches[k]), [[67.15, 35.4], [67.15, 51.4], [59.15, 43.4], [75.15, 43.4]], 'the four switch points');
const pts = Box3.crossPoints();
eq(pts.length, 12, 'the cap is a plus of twelve corners');
eq([Math.min(...pts.map((p) => p[0])), Math.max(...pts.map((p) => p[0])), Math.min(...pts.map((p) => p[1])), Math.max(...pts.map((p) => p[1]))].map((v) => Math.round(v * 100) / 100), [55.15, 79.15, 31.4, 55.4], 'the cross spans x 55.15..79.15, y 31.4..55.4');
const E = 1e-9, inCross = (x, y) => (Math.abs(x - X.cx) <= aa + E && Math.abs(y - X.cy) <= hh + E) || (Math.abs(y - X.cy) <= aa + E && Math.abs(x - X.cx) <= hh + E);
const rects = {};
for (const k of keys) {
  const [x, y] = BOX3.switches[k], q = rects[k] = Box3.armRect(k);
  eq([q.w, q.h, q.x + q.w / 2, q.y + q.h / 2], [8, 8, x, y], k + ': the click area is the 8 x 8 end square centred on its switch');
  ok([[q.x, q.y], [q.x + q.w, q.y], [q.x, q.y + q.h], [q.x + q.w, q.y + q.h]].every(([px, py]) => inCross(px, py)), k + ': its click area is inside the cross');
  ok(q.x >= 0 && q.x + q.w <= BOX3.case.w && q.y >= 0 && q.y + q.h <= BOX3.case.h, k + ' is inside the case');
  ok(q.x > win.x + win.w || q.y > win.y + win.h || q.y + q.h < win.y, k + ' is clear of the screen window');
}
for (let i = 0; i < 4; i++) for (let j = i + 1; j < 4; j++) {
  const p = rects[keys[i]], q = rects[keys[j]];
  ok(p.x + p.w <= q.x + 1e-9 || q.x + q.w <= p.x + 1e-9 || p.y + p.h <= q.y + 1e-9 || q.y + q.h <= p.y + 1e-9, keys[i] + ' and ' + keys[j] + ' click areas do not overlap');
}
const mid = { x: X.cx - aa, y: X.cy - aa, w: X.arm, h: X.arm };
ok(keys.every((k) => { const q = rects[k]; return q.x + q.w <= mid.x + 1e-9 || mid.x + mid.w <= q.x + 1e-9 || q.y + q.h <= mid.y + 1e-9 || mid.y + mid.h <= q.y + 1e-9; }), 'the centre square is no button');
eq(BOX3.ports.map((p) => p.id + ':' + p.wall + ':' + p.at), ['midi1:top:8.7', 'midi2:top:21.5', 'din1:top:64.9', 'din2:top:87', 'clock:right:33.4', 'reset:right:53.4', 'linein:bottom:32.4', 'usbIn:top:34.35', 'usbHost:top:47.35', 'usbOwn:left:43.4'], 'ten ports, none else');
const c = BOX3.case;
for (const p of BOX3.ports) {
  const rect = Box3.portRect(p), horizontal = p.wall === 'top' || p.wall === 'bottom';
  const along = horizontal ? [rect.x, rect.x + rect.w, c.w] : [rect.y, rect.y + rect.h, c.h];
  ok(along[0] >= 0 && along[1] <= along[2], p.id + ' is within its wall');
  const touching = { top: rect.y + rect.h, bottom: rect.y, left: rect.x + rect.w, right: rect.x }[p.wall];
  near(touching, { top: 0, bottom: c.h, left: 0, right: c.w }[p.wall], p.id + ' sits on its wall');
}
eq([BOX3.kinds.din.len, BOX3.kinds.usbc.len, BOX3.kinds.usbc.depth, BOX3.kinds.trs.len], [15, 9, 3.2, 6], 'connector sizes');
const vb = Box3.viewBox();
for (const p of BOX3.ports) { const q = Box3.portRect(p); ok(q.x > vb.x && q.y > vb.y && q.x + q.w < vb.x + vb.w && q.y + q.h < vb.y + vb.h, p.id + ' is inside the view box'); }
ok(vb.x < 0 && vb.y < 0 && vb.x + vb.w > c.w && vb.y + vb.h > c.h, 'the view box holds the whole case');
// built into a minimal DOM: the labels, the cap's four symbols and the four buttons
{
  const mk = (tag) => { const e = { tag, attrs: {}, kids: [], children: [], namespaceURI: 'x', setAttribute(k, v) { this.attrs[k] = String(v); }, append(...k) { this.kids.push(...k); }, prepend(...k) { this.kids.unshift(...k); }, replaceChildren(...k) { this.kids = k; }, set textContent(v) { this.text = v; } }; return e; };
  const els = { '#lid': mk('rect'), '#lidshade': mk('rect'), '#ports': mk('g'), '#labels': mk('g'), '#dpad': mk('g') };
  const svg = mk('svg'); svg.querySelector = (q) => els[q];
  globalThis.document = { createElementNS: (ns, tag) => mk(tag) };
  Box3.build(svg);
  eq(els['#labels'].kids.filter((e) => e.tag === 'text').map((e) => e.text), BOX3.labels.map((l) => l[2]), 'the labels are drawn as text');
  eq(els['#labels'].kids.filter((e) => e.tag === 'polygon').length, 10, 'and ten triangles');
  const arms = els['#dpad'].kids.filter((e) => e.tag === 'g');
  eq(arms.map((g) => g.attrs['data-b']), ['up', 'down', 'no', 'yes'], 'the four buttons are there');
  eq(arms.map((g) => g.kids.filter((e) => /armarea/.test(e.attrs.class)).length), [1, 1, 1, 1], 'each has its click area');
  eq(arms.map((g) => g.kids.filter((e) => /sym/.test(e.attrs.class || '')).map((e) => e.tag).join()), ['polygon', 'polygon', 'polygon,polygon', 'polyline'], 'symbols: triangle up, triangle down, an X (two bars), a tick');
  ok(arms.every((g) => !g.kids.some((e) => e.tag === 'text')), 'no text on the cap');
}
console.log('box3: ' + checks + ' checks passed');
