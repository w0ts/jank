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

eq([BOX3.case.w, BOX3.case.h, BOX3.case.r], [100, 81, 3], 'the case is 100 x 81, corner radius 3');
const s = BOX3.screen, win = Box3.windowRect();
near(s.w / s.h, 4 / 3, 'the active area is 4:3 like the 320 x 240 canvas');
eq([s.x, s.x + s.w, s.y, s.y + s.h].map((v) => Math.round(v * 10) / 10), [9.7, 50.5, 25.2, 55.8], 'the active area: x 9.7..50.5, y 25.2..55.8');
eq([win.x, win.x + win.w, win.y, win.y + win.h].map((v) => Math.round(v * 10) / 10), [8.7, 51.5, 24.2, 56.8], 'the lid window is the active area plus 1 mm each side');
const a = Box3.canvasArea(null), half = Box3.canvasArea(128 / 64);
eq([a.x, a.y, a.w, a.h], [s.x, s.y, s.w, s.h], 'the colour canvas fills the active area exactly');
near(half.w / half.h, 2, 'the black and white preview keeps 2:1');
ok(half.x >= s.x && half.y >= s.y && half.x + half.w <= s.x + s.w + 1e-9 && half.y + half.h <= s.y + s.h + 1e-9, '... inside the active area');
ok(win.x > 0 && win.y > 0 && win.x + win.w < BOX3.case.w && win.y + win.h < BOX3.case.h, 'the window is inside the case');

const keys = ['up', 'down', 'no', 'yes'], X = BOX3.cross, hh = X.span / 2, aa = X.arm / 2;
eq([X.cx, X.cy, X.span, X.arm], [67, 40.5, 24, 8], 'the D-pad: a cross centred at (67, 40.5), 24 mm across, arms 8 mm wide');
eq(keys.map((k) => BOX3.switches[k]), [[67, 32.5], [67, 48.5], [59, 40.5], [75, 40.5]], 'the four switch points');
const pts = Box3.crossPoints();
eq(pts.length, 12, 'the cap is a plus of twelve corners');
eq([Math.min(...pts.map((p) => p[0])), Math.max(...pts.map((p) => p[0])), Math.min(...pts.map((p) => p[1])), Math.max(...pts.map((p) => p[1]))], [55, 79, 28.5, 52.5], 'the cross spans x 55..79, y 28.5..52.5');
const inCross = (x, y) => (Math.abs(x - X.cx) <= aa && Math.abs(y - X.cy) <= hh) || (Math.abs(y - X.cy) <= aa && Math.abs(x - X.cx) <= hh);
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
eq(BOX3.ports.map((p) => p.id + ':' + p.wall + ':' + p.at), ['midi1:top:33.3', 'midi2:top:46.1', 'din1:top:63.5', 'din2:top:85.6', 'clock:right:30.5', 'reset:right:50.5', 'linein:bottom:33.5', 'usbIn:bottom:9.5', 'usbHost:bottom:21', 'usbOwn:left:40.5'], 'ten ports, none else');
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
console.log('box3: ' + checks + ' checks passed');
