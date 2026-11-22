// SPDX-License-Identifier: MIT
// Updating a running Jank over USB MIDI from the browser (the Install page), as scripts/ota.py does from a computer: the
// control line's SysEx framing (F0 7D 'J' 'K' <7-bit text> F7, core/jank_sysex.h) and the transfer of core/jank_ota.h:
//   ota begin <size> <crc32 hex> [beatw]  ->  ok 0 <chunk>
//   ota d <offset> <crc32 hex> <base64>   ->  ok <next offset>   (err gap / crc <offset>: go on from there)
//   ota end                               ->  ok, then the device restarts (an app on trial; the weights at once)
// No DOM, no Web MIDI here: the caller passes send(text) and a line queue (MidiOta.Lines), so node can test it.
const MidiOta = (() => {
  'use strict';
  const HEAD = [0xf0, 0x7d, 0x4a, 0x4b];
  const CHUNK = 120, TRIES = 6, REPLY_MS = 2000;
  const ANSWER = /^(ok( \d+){1,2}|err (busy|size|arg|state|gap|crc|header|short|idle|playing)( \d+)?)$/;
  const crcTable = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
  /** zlib's CRC-32 of bytes, as 8 lowercase hex digits */
  const crcHex = (b) => { let c = 0xffffffff; for (let i = 0; i < b.length; i++) c = crcTable[(c ^ b[i]) & 255] ^ (c >>> 8); return ((c ^ 0xffffffff) >>> 0).toString(16).padStart(8, '0'); };
  const b64 = (b) => { let s = ''; for (let i = 0; i < b.length; i++) s += String.fromCharCode(b[i]); return btoa(s); };

  /** one control line as a SysEx message */
  function frame(text) {
    const t = text.endsWith('\n') ? text : text + '\n';
    const out = HEAD.slice();
    for (let i = 0; i < t.length; i++) {
      const c = t.charCodeAt(i);
      if (c >= 0x80) throw new Error('control lines are 7-bit text');
      out.push(c);
    }
    out.push(0xf7);
    return out;
  }

  /** the device's SysEx frames back into lines (a frame carries part of a line) */
  class Lines {
    constructor() { this.buf = ''; this.lines = []; this.waiters = []; }
    feed(msg) {
      if (msg.length < 5 || msg[0] !== 0xf0 || msg[1] !== 0x7d || msg[2] !== 0x4a || msg[3] !== 0x4b || msg[msg.length - 1] !== 0xf7) return;
      for (let i = 4; i < msg.length - 1; i++) this.buf += String.fromCharCode(msg[i]);
      let k;
      while ((k = this.buf.indexOf('\n')) >= 0) {
        const line = this.buf.slice(0, k).trim();
        this.buf = this.buf.slice(k + 1);
        const w = this.waiters.shift();
        if (w) w(line); else this.lines.push(line);
      }
    }
    drop() { this.lines = []; }
    /** the next line, or null after ms */
    next(ms) {
      if (this.lines.length) return Promise.resolve(this.lines.shift());
      return new Promise((res) => {
        const w = (l) => { clearTimeout(t); res(l); };
        const t = setTimeout(() => { this.waiters = this.waiters.filter((x) => x !== w); res(null); }, ms);
        this.waiters.push(w);
      });
    }
  }

  /** sends a line and returns its answer ("ok ..." / "err ..."), resending on silence; other lines are skipped */
  async function ask(send, lines, text, replyMs = REPLY_MS) {
    for (let i = 0; i < TRIES; i++) {
      lines.drop();
      send(text);
      const end = Date.now() + replyMs;
      for (;;) {
        const left = end - Date.now();
        if (left <= 0) break;
        const line = await lines.next(left);
        if (line !== null && ANSWER.test(line)) return line;
        if (line === null) break;
      }
    }
    throw new Error('no answer to "' + text.split(' ').slice(0, 2).join(' ') + '"');
  }

  /** the whole transfer; target 'app' (an ESP32-S3 app image) or 'beatw' (the weights blob). progress(done, total) */
  async function upload(send, lines, data, target, progress = () => {}) {
    if (target === 'app' && (data.length < 24 || data[0] !== 0xe9 || (data[12] | (data[13] << 8)) !== 9)) throw new Error('not an ESP32-S3 app image');
    if (target === 'beatw' && !(data[0] === 0x4a && data[1] === 0x42 && data[2] === 0x57 && data[3] === 0x31)) throw new Error('not a weights blob');
    const begin = 'ota begin ' + data.length + ' ' + crcHex(data) + (target === 'beatw' ? ' beatw' : '');
    let a = await ask(send, lines, begin, target === 'beatw' ? 8000 : REPLY_MS);   // (the weights' erase takes about a second)
    if (a.startsWith('err busy')) { await ask(send, lines, 'ota abort'); a = await ask(send, lines, begin, 8000); }
    if (a.startsWith('err playing')) throw new Error('the device is playing: stop the session first');
    if (a.startsWith('err state') && target === 'beatw') throw new Error('this device has no room for the weights (an older partition table): flash it once as a new device, with the box ticked');
    const p = a.split(' ');
    if (p[0] !== 'ok' || p.length < 3) throw new Error('the device refused: ' + a);
    const chunk = Math.min(CHUNK, +p[2]);
    let pos = 0, bad = 0;
    while (pos < data.length) {
      const c = data.subarray(pos, pos + chunk);
      const r = (await ask(send, lines, 'ota d ' + pos + ' ' + crcHex(c) + ' ' + b64(c))).split(' ');
      if (r[0] === 'ok') { pos = +r[1]; bad = 0; progress(pos, data.length); }
      else if ((r[1] === 'gap' || r[1] === 'crc') && bad < TRIES) { pos = +r[2]; bad++; }
      else throw new Error('the device stopped the transfer: ' + r.join(' '));
    }
    const e = await ask(send, lines, 'ota end', 8000);
    if (!e.startsWith('ok')) throw new Error('the device refused the image: ' + e);
  }

  return { frame, Lines, ask, upload, crcHex };
})();
if (typeof module !== 'undefined') module.exports = MidiOta;
