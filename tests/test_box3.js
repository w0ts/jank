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

eq([BOX3.case.w, BOX3.case.h, BOX3.case.r], [100, 79, 3], 'the case is 100 x 79, corner radius 3');
const s = BOX3.screen, win = Box3.windowRect();
near(s.w / s.h, 4 / 3, 'the active area is 4:3 like the 320 x 240 canvas');
eq([s.x, s.x + s.w, s.y, s.y + s.h].map((v) => Math.round(v * 10) / 10), [9.7, 50.5, 24.2, 54.8], 'the active area: x 9.7..50.5, y 24.2..54.8');
eq([win.x, win.x + win.w, win.y, win.y + win.h].map((v) => Math.round(v * 10) / 10), [8.7, 51.5, 23.2, 55.8], 'the lid window is the active area plus 1 mm each side');
const a = Box3.canvasArea(null), half = Box3.canvasArea(128 / 64);
eq([a.x, a.y, a.w, a.h], [s.x, s.y, s.w, s.h], 'the colour canvas fills the active area exactly');
near(half.w / half.h, 2, 'the black and white preview keeps 2:1');
ok(half.x >= s.x && half.y >= s.y && half.x + half.w <= s.x + s.w + 1e-9 && half.y + half.h <= s.y + s.h + 1e-9, '... inside the active area');
ok(win.x > 0 && win.y > 0 && win.x + win.w < BOX3.case.w && win.y + win.h < BOX3.case.h, 'the window is inside the case');

const keys = ['up', 'down', 'no', 'yes'], r = BOX3.keys.d / 2;
eq(keys.map((k) => BOX3.keys[k]), [[67, 30.5], [67, 48.5], [58, 39.5], [76, 39.5]], 'the four keys');
for (const k of keys) {
  const [x, y] = BOX3.keys[k];
  ok(x - r >= 0 && x + r <= BOX3.case.w && y - r >= 0 && y + r <= BOX3.case.h, k + ' is inside the case');
  ok(x - r > win.x + win.w || y - r > win.y + win.h || y + r < win.y, k + ' does not overlap the screen window');
}
eq(BOX3.keys.d, 6, 'the keys are 6 mm');
eq(BOX3.ports.map((p) => p.id + ':' + p.wall + ':' + p.at), ['midi1:top:33.3', 'midi2:top:46.1', 'din1:top:63.5', 'din2:top:85.6', 'clock:right:29.5', 'reset:right:49.5', 'linein:bottom:73.5', 'usbHost:left:9.6', 'usbOwn:left:39.5', 'usbIn:left:68.5'], 'ten ports, none else');
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
