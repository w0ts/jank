// node scripts/render_lcd2.js <outdir>
// Renders the lcd2 layout's screens (ui/oled.js with ctx.layout 'lcd2', 320 x 240, one frame pixel per panel pixel) to PNG files with the
// palette of ui/lcd2.js: HOME LINK, HOME AUDIO, MENU, NETWORK, ABOUT and the charging top bar (the plug beside the battery). For looking at a design;
// the firmware and the tests do not use it.
'use strict';
const fs = require('fs'), path = require('path'), zlib = require('zlib');
const root = path.join(__dirname, '..');
const read = f => fs.readFileSync(path.join(root, f), 'utf8');
const { OLED, Lcd2 } = new Function(read('ui/bigfont.js') + read('ui/oled.js') + read('ui/lcd2.js') + '; return { OLED, Lcd2 };')();
const { baseState } = require(path.join(root, 'tests', 'oled_ref', 'state.js'));
const W = 320, H = 240;

const crcT = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc = b => { let c = 0xffffffff; for (const v of b) c = crcT[(c ^ v) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
function chunk(type, data) { const b = Buffer.alloc(12 + data.length); b.writeUInt32BE(data.length, 0); b.write(type, 4); data.copy(b, 8); b.writeUInt32BE(crc(b.subarray(4, 8 + data.length)), 8 + data.length); return b; }
function png(w, h, rgb) {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 3 + 1)] = 0; rgb.copy(raw, y * (w * 3 + 1) + 1, y * w * 3, (y + 1) * w * 3); }
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}
function ctxOf(edit) {
  const s = Object.assign(baseState(), { rssi: -55, airLost: 2, airRetry: 12, rssiMin: -82, rssiMax: -48, peakDb: -12, confKnown: true, silent: false, gated: false, confHold: false, fold: 1, usbMidi: 0, beatN: 0 });
  if (edit) edit(s);
  return Object.assign({}, s, { layout: 'lcd2', setup: false });
}
const SCREENS = {
  'home-link': [ctxOf(s => Object.assign(s, { link: true, lineIn: false, linkBpm: 120, beatN: 1, peers: 2 })), u => u],
  'home-audio': [ctxOf(s => Object.assign(s, { bpm: 124, levelDb: -18, conf: 0.9, beatN: 0, bar: { state: 'locked', conf: 0.8 } })), u => u],
  'menu': [ctxOf(), u => Object.assign(u, { screen: 'menu', cursor: 2 })],
  'network': [ctxOf(), u => Object.assign(u, { screen: 'net', ncur: 0 })],
  'about': [ctxOf(), u => Object.assign(u, { screen: 'info', page: 'about', ascroll: 0 })],
  'charging': [ctxOf(s => Object.assign(s, { usb: 1, fitted: 1, batt: 60, link: true, linkBpm: 120, peers: 2 })), u => u],
};
const outDir = path.resolve(process.argv[2] || path.join(root, 'preview-final'));
fs.mkdirSync(outDir, { recursive: true });
for (const [name, [ctx, mk]] of Object.entries(SCREENS)) {
  const fb = new Uint8Array(W * H), ink = new Uint8Array(W * H);
  OLED.draw(fb, mk(OLED.create()), ctx, 0, ink);
  const rgb = Buffer.alloc(W * H * 3);
  for (let i = 0; i < W * H; i++) { const c = fb[i] ? Lcd2.inkColour(ink[i]) : Lcd2.PALETTE.bg; rgb[3 * i] = c[0]; rgb[3 * i + 1] = c[1]; rgb[3 * i + 2] = c[2]; }
  const f = path.join(outDir, name + '.png');
  fs.writeFileSync(f, png(W, H, rgb));
  console.log(f);
}
